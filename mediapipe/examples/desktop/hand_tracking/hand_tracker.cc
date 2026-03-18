

#include "hand_tracker.h"

#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_buffer.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"

#include <fstream>
#include <sstream>

struct HandTracker::Impl
{
    mediapipe::CalculatorGraph graph;
    mediapipe::GlCalculatorHelper gpu_helper;
    ResultCallback callback;
    HandTrackerConfig config;
    bool running = false;
    bool input_closed = false;
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
            if (!impl_->callback)
                return absl::OkStatus();

            const auto &multi_hand = packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();

            HandTrackingResult result;
            result.timestamp_us = packet.Timestamp().Microseconds();

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

                result.hands.push_back(std::move(landmarks));
            }

            impl_->callback(result);
            return absl::OkStatus();
        });

    if (!lm_status.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", config.landmark_stream.c_str(), lm_status.ToString().c_str());
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
