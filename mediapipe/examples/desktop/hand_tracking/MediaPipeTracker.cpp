

#include "MediaPipeTrackerImpl.hpp"

#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"

#include <fstream>
#include <sstream>

MediaPipeTracker::~MediaPipeTracker() { stop(); }

bool MediaPipeTracker::closeInputStream()
{
    if (!impl_->running || impl_->inputClosed)
        return false;

    auto status = impl_->graph.CloseInputStream(impl_->config.inputStream);

    if (!status.ok())
    {
        fprintf(stderr, "ERROR CloseInputStream: %s\n", status.ToString().c_str());
        return false;
    }

    impl_->inputClosed = true;
    return true;
}

bool MediaPipeTracker::initialize(const MediaPipeTrackerConfig &config)
{
    impl_->config = config;

    // Leer grafo
    std::ifstream file(config.graphPath);

    if (!file.is_open())
    {
        fprintf(stderr, "ERROR: no se puede abrir graph: %s\n", config.graphPath.c_str());
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();

    mediapipe::CalculatorGraphConfig graphConfig = mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(ss.str());

    // Inicializar grafo
    auto status = impl_->graph.Initialize(graphConfig);

    if (!status.ok())
    {
        fprintf(stderr, "ERROR graph.Initialize: %s\n", status.ToString().c_str());
        return false;
    }

    // GPU resources
    if (config.useGPU)
    {
        auto gpuResources = mediapipe::GpuResources::Create();

        if (!gpuResources.ok())
        {
            fprintf(stderr, "ERROR GpuResources::Create: %s\n", gpuResources.status().ToString().c_str());
            return false;
        }

        status = impl_->graph.SetGpuResources(std::move(gpuResources).value());
        if (!status.ok())
        {
            fprintf(stderr, "ERROR SetGpuResources: %s\n", status.ToString().c_str());
            return false;
        }

        impl_->gpuHelper.InitializeForTest(impl_->graph.GetGpuResources().get());
    }

    // Observers de la subclase
    if (!registerObservers())
        return false;

    // Arrancar
    status = impl_->graph.StartRun({});
    if (!status.ok())
    {
        fprintf(stderr, "ERROR graph.StartRun: %s\n", status.ToString().c_str());
        return false;
    }

    impl_->running = true;
    impl_->inputClosed = false;

    return true;
}

bool MediaPipeTracker::isRunning() const { return impl_->running; }

bool MediaPipeTracker::processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs)
{
    if (!impl_->running || impl_->inputClosed)
        return false;

    auto inputFrame = absl::make_unique<mediapipe::ImageFrame>(mediapipe::ImageFormat::SRGB, width, height, mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);

    inputFrame->CopyPixelData(mediapipe::ImageFormat::SRGB, width, height, rgbData, mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);

    auto status = impl_->graph.AddPacketToInputStream(impl_->config.inputStream, mediapipe::Adopt(inputFrame.release()).At(mediapipe::Timestamp(timestampUs)));

    if (!status.ok())
    {
        fprintf(stderr, "ERROR ProcessFrame: %s\n", status.ToString().c_str());
        return false;
    }

    return true;
}

void MediaPipeTracker::stop()
{
    if (!impl_->running)
        return;

    if (!impl_->inputClosed)
        impl_->graph.CloseAllInputStreams().IgnoreError();

    impl_->graph.WaitUntilDone().IgnoreError();
    impl_->running = false;
    impl_->inputClosed = true;
}

bool MediaPipeTracker::waitUntilIdle()
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

MediaPipeTracker::MediaPipeTracker() : impl_(std::make_unique<Impl>()) {}
