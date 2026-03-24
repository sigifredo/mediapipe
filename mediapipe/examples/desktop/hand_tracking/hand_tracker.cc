

#include "hand_tracker.h"

#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/classification.pb.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_buffer.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"

#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

struct PartialResult
{
    std::vector<std::vector<HandLandmark>> hands;
    std::vector<HandClassification> handedness;
    bool has_landmarks = false;
    bool has_handedness = false;

    bool isComplete() const { return has_landmarks && has_handedness; }
};

struct HandTracker::Impl
{
    mediapipe::CalculatorGraph graph;
    mediapipe::GlCalculatorHelper gpu_helper;
    ResultCallback callback;
    HandTrackerConfig config;
    bool running = false;
    bool input_closed = false;

    std::mutex pending_mutex;
    std::map<int64_t, PartialResult> pending;

    void tryEmit(int64_t timestamp_us)
    {
        auto it = pending.find(timestamp_us);

        if (it == pending.end() || !it->second.isComplete())
            return;

        if (callback)
        {
            HandTrackingResult result;
            result.timestamp_us = timestamp_us;
            result.hands = std::move(it->second.hands);
            result.handedness = std::move(it->second.handedness);
            callback(result);
        }

        pending.erase(it);

        // Purgar entradas huérfanas (un stream llegó pero el otro nunca).
        // Si hay más de 8 pendientes, eliminar las más antiguas.
        while (pending.size() > 8)
            pending.erase(pending.begin());
    }
};

HandTracker::HandTracker() : impl_(std::make_unique<Impl>()) {}
HandTracker::~HandTracker() { Stop(); }

void HandTracker::SetResultCallback(ResultCallback cb)
{
    impl_->callback = std::move(cb);
}

bool HandTracker::is_running() const { return impl_->running; }

bool HandTracker::Initialize(const HandTrackerConfig &config)
{
    impl_->config = config;

    // --- Leer grafo ---
    std::ifstream file(config.graph_path);

    if (!file.is_open())
    {
        fprintf(stderr, "ERROR: no se puede abrir graph: %s\n", config.graph_path.c_str());
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();

    mediapipe::CalculatorGraphConfig graph_config = mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(ss.str());

    // --- Inicializar grafo ---
    auto status = impl_->graph.Initialize(graph_config);

    if (!status.ok())
    {
        fprintf(stderr, "ERROR graph.Initialize: %s\n", status.ToString().c_str());
        return false;
    }

    // --- GPU resources (solo si se pide) ---
    if (config.use_gpu)
    {
        auto gpu_resources = mediapipe::GpuResources::Create();

        if (!gpu_resources.ok())
        {
            fprintf(stderr, "ERROR GpuResources::Create: %s\n", gpu_resources.status().ToString().c_str());
            return false;
        }

        status = impl_->graph.SetGpuResources(std::move(gpu_resources).value());
        if (!status.ok())
        {
            fprintf(stderr, "ERROR SetGpuResources: %s\n", status.ToString().c_str());
            return false;
        }

        impl_->gpu_helper.InitializeForTest(impl_->graph.GetGpuResources().get());
    }

    // --- Observer: landmarks ---
    auto lm_status = impl_->graph.ObserveOutputStream(
        config.landmark_stream,
        [this](const mediapipe::Packet &packet) -> absl::Status
        {
            const auto &multi_hand = packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
            int64_t ts = packet.Timestamp().Microseconds();

            std::vector<std::vector<HandLandmark>> hands;
            hands.reserve(multi_hand.size());

            for (const auto &hand : multi_hand)
            {
                std::vector<HandLandmark> landmarks;
                landmarks.reserve(hand.landmark_size());

                for (const auto &lm : hand.landmark())
                {
                    landmarks.push_back({lm.x(),
                                         lm.y(),
                                         lm.z(),
                                         lm.has_visibility() ? lm.visibility() : 0.0f});
                }

                hands.push_back(std::move(landmarks));
            }

            {
                std::lock_guard<std::mutex> lock(impl_->pending_mutex);
                auto &partial = impl_->pending[ts];
                partial.hands = std::move(hands);
                partial.has_landmarks = true;
                impl_->tryEmit(ts);
            }

            return absl::OkStatus();
        });

    if (!lm_status.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", config.landmark_stream.c_str(), lm_status.ToString().c_str());
        return false;
    }

    // --- Observer: handedness ---
    auto hd_status = impl_->graph.ObserveOutputStream(
        config.handedness_stream,
        [this](const mediapipe::Packet &packet) -> absl::Status
        {
            const auto &multi_handedness = packet.Get<std::vector<mediapipe::ClassificationList>>();
            int64_t ts = packet.Timestamp().Microseconds();

            std::vector<HandClassification> handedness;
            handedness.reserve(multi_handedness.size());

            for (const auto &cl_list : multi_handedness)
            {
                if (cl_list.classification_size() > 0)
                {
                    const auto &cl = cl_list.classification(0);
                    handedness.push_back({cl.label(), cl.score()});
                }
                else
                {
                    handedness.push_back({"Unknown", 0.0f});
                }
            }

            {
                std::lock_guard<std::mutex> lock(impl_->pending_mutex);
                auto &partial = impl_->pending[ts];
                partial.handedness = std::move(handedness);
                partial.has_handedness = true;
                impl_->tryEmit(ts);
            }

            return absl::OkStatus();
        });

    if (!hd_status.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", config.handedness_stream.c_str(), hd_status.ToString().c_str());
        return false;
    }

    // --- Arrancar ---

    status = impl_->graph.StartRun({});
    if (!status.ok())
    {
        fprintf(stderr, "ERROR graph.StartRun: %s\n", status.ToString().c_str());
        return false;
    }

    impl_->running = true;
    impl_->input_closed = false;

    return true;
}

bool HandTracker::ProcessFrame(const uint8_t *rgb_data, int width, int height, int64_t timestamp_us)
{
    if (!impl_->running || impl_->input_closed)
        return false;

    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(mediapipe::ImageFormat::SRGB, width, height, mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);

    input_frame->CopyPixelData(mediapipe::ImageFormat::SRGB, width, height, rgb_data, mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);

    auto status = impl_->graph.AddPacketToInputStream(
        impl_->config.input_stream,
        mediapipe::Adopt(input_frame.release())
            .At(mediapipe::Timestamp(timestamp_us)));

    if (!status.ok())
    {
        fprintf(stderr, "ERROR ProcessFrame: %s\n", status.ToString().c_str());
        return false;
    }

    return true;
}

bool HandTracker::WaitUntilIdle()
{
    if (!impl_->running)
        return false;

    auto status = impl_->graph.WaitUntilIdle();

    if (!status.ok())
    {
        fprintf(stderr, "ERROR WaitUntilIdle: %s\n", status.ToString().c_str());
        return false;
    }

    return true;
}

bool HandTracker::CloseInputStream()
{
    if (!impl_->running || impl_->input_closed)
        return false;

    auto status = impl_->graph.CloseInputStream(impl_->config.input_stream);

    if (!status.ok())
    {
        fprintf(stderr, "ERROR CloseInputStream: %s\n", status.ToString().c_str());
        return false;
    }

    impl_->input_closed = true;

    return true;
}

void HandTracker::Stop()
{
    if (!impl_->running)
        return;

    if (!impl_->input_closed)
        impl_->graph.CloseAllInputStreams().IgnoreError();

    impl_->graph.WaitUntilDone().IgnoreError();
    impl_->running = false;
    impl_->input_closed = true;
}
