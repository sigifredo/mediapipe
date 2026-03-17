#include "mediapipe/examples/desktop/hand_tracking/hand_tracker.h"

#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_buffer.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"

#include <fstream>
#include <sstream>

static constexpr char kInputStream[] = "input_video";
static constexpr char kOutputStream[] = "output_video";
static constexpr char kLandmarksStream[] = "hand_landmarks";

struct HandTracker::Impl {
    mediapipe::CalculatorGraph graph;
    mediapipe::GlCalculatorHelper gpu_helper;
    ResultCallback callback;
    bool running = false;
};

HandTracker::HandTracker() : impl_(std::make_unique<Impl>()) {}
HandTracker::~HandTracker() { Stop(); }

void HandTracker::SetResultCallback(ResultCallback cb) {
    impl_->callback = std::move(cb);
}

bool HandTracker::Initialize(const std::string& graph_path,
                              const std::string& model_dir,
                              bool use_gpu) {
    std::ifstream file(graph_path);
    if (!file.is_open()) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    std::string graph_str = ss.str();

    mediapipe::CalculatorGraphConfig config = mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(graph_str);



    auto status = impl_->graph.Initialize(config);
    if (!status.ok()) return false;

    if (use_gpu) {
        auto gpu_resources = mediapipe::GpuResources::Create();
        if (!gpu_resources.ok()) return false;
        status = impl_->graph.SetGpuResources(
            std::move(gpu_resources).value());
        if (!status.ok()) return false;
        impl_->gpu_helper.InitializeForTest(
            impl_->graph.GetGpuResources().get());
    }

    auto landmarks_status = impl_->graph.ObserveOutputStream(
        kLandmarksStream,
        [this](const mediapipe::Packet& packet) -> absl::Status {
            if (impl_->callback) {
                const auto& multi_hand =
                    packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();

                HandTrackingResult result;
                result.timestamp_us = packet.Timestamp().Microseconds();

                for (const auto& hand : multi_hand) {
                    std::vector<HandLandmark> landmarks;
                    landmarks.reserve(hand.landmark_size());
                    for (const auto& lm : hand.landmark()) {
                        HandLandmark hl;
                        hl.x = lm.x();
                        hl.y = lm.y();
                        hl.z = lm.z();
                        hl.visibility = lm.has_visibility()
                            ? lm.visibility() : 0.0f;
                        landmarks.push_back(hl);
                    }
                    result.hands.push_back(std::move(landmarks));
                }
                impl_->callback(result);
            }
            return absl::OkStatus();
        });
    if (!landmarks_status.ok()) return false;

    status = impl_->graph.StartRun({});
    if (!status.ok()) return false;
    impl_->running = true;
    return true;
}

bool HandTracker::ProcessFrame(const uint8_t* rgb_data,
                                int width, int height,
                                int64_t timestamp_us) {
    if (!impl_->running) return false;

    auto input_frame = std::make_shared<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, width, height,
        mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);
    input_frame->CopyPixelData(
        mediapipe::ImageFormat::SRGB, width, height,
        rgb_data, mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);

    auto status = impl_->gpu_helper.RunInGlContext(
        [&]() -> absl::Status {
            auto texture = impl_->gpu_helper.CreateSourceTexture(*input_frame);
            auto gpu_buffer = texture.GetFrame<mediapipe::GpuBuffer>();
            glFlush();
            texture.Release();

            return impl_->graph.AddPacketToInputStream(
                kInputStream,
                mediapipe::Adopt(gpu_buffer.release())
                    .At(mediapipe::Timestamp(timestamp_us)));
        });

    return status.ok();
}

void HandTracker::Stop() {
    if (impl_->running) {
        impl_->graph.CloseAllInputStreams().IgnoreError();
        impl_->graph.WaitUntilDone().IgnoreError();
        impl_->running = false;
    }
}
