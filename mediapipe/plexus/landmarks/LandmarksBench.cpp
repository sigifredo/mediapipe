

/**
 * @file LandmarksBench.cpp
 * @brief Benchmark headless del grafo de manos en CPU o GPU.
 *
 * Mide la latencia por frame (AddPacket hasta WaitUntilIdle) sobre un video,
 * sin ventanas. Requiere un build con GPU habilitada (sin
 * MEDIAPIPE_DISABLE_GPU) y ejecutarse desde la raíz del repositorio para que
 * se resuelvan los modelos .tflite.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"

ABSL_FLAG(std::string, device, "cpu", "Dispositivo de inferencia: cpu o gpu.");
ABSL_FLAG(std::string, video, "", "Ruta del video de entrada.");
ABSL_FLAG(int, maxFrames, 300, "Máximo de frames a procesar.");
ABSL_FLAG(int, warmup, 10, "Frames descartados de las estadísticas.");

namespace
{
    using Clock = std::chrono::steady_clock;

    const char *const kCommonHeader = R"pb(
  input_stream: "input_video"
  node {
    calculator: "ConstantSidePacketCalculator"
    output_side_packet: "PACKET:num_hands"
    node_options: {
      [type.googleapis.com/mediapipe.ConstantSidePacketCalculatorOptions]: {
        packet { int_value: 2 }
      }
    }
  }
)pb";

    const char *const kCpuGraph = R"pb(
  node {
    calculator: "HandLandmarkTrackingCpu"
    input_stream: "IMAGE:input_video"
    input_side_packet: "NUM_HANDS:num_hands"
    output_stream: "LANDMARKS:landmarks"
  }
)pb";

    const char *const kGpuGraph = R"pb(
  node {
    calculator: "ImageFrameToGpuBufferCalculator"
    input_stream: "input_video"
    output_stream: "input_video_gpu"
  }
  node {
    calculator: "HandLandmarkTrackingGpu"
    input_stream: "IMAGE:input_video_gpu"
    input_side_packet: "NUM_HANDS:num_hands"
    output_stream: "LANDMARKS:landmarks"
  }
)pb";

    /**
     * @brief Calcula el percentil p (0 a 1) de una serie ya ordenada.
     */
    double Percentile(const std::vector<double> &sorted, double p)
    {
        return sorted[static_cast<size_t>(p * (sorted.size() - 1))];
    }

    absl::Status Run()
    {
        const std::string device = absl::GetFlag(FLAGS_device);

        if (device != "cpu" && device != "gpu")
            return absl::InvalidArgumentError("--device debe ser cpu o gpu");

        cv::VideoCapture capture(absl::GetFlag(FLAGS_video));

        if (!capture.isOpened())
            return absl::NotFoundError("no se puede abrir el video");

        const std::string graphText = std::string(kCommonHeader) + (device == "gpu" ? kGpuGraph : kCpuGraph);
        auto graphConfig = mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(graphText);

        const auto initStart = Clock::now();
        mediapipe::CalculatorGraph graph;

        ABSL_RETURN_IF_ERROR(graph.Initialize(graphConfig));

        int64_t handsSeen = 0;

        ABSL_RETURN_IF_ERROR(graph.ObserveOutputStream("landmarks", [&handsSeen](const mediapipe::Packet &packet)
                                                       {
        handsSeen += packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>().size();
        return absl::OkStatus(); }));
        ABSL_RETURN_IF_ERROR(graph.StartRun({}));

        const double initMs = std::chrono::duration<double, std::milli>(Clock::now() - initStart).count();

        std::vector<double> latenciesMs;
        double firstFrameMs = 0.0;
        int frameIndex = 0;
        cv::Mat bgr;

        while (frameIndex < absl::GetFlag(FLAGS_maxFrames) && capture.read(bgr))
        {
            auto frame = std::make_unique<mediapipe::ImageFrame>(mediapipe::ImageFormat::SRGB, bgr.cols, bgr.rows, mediapipe::ImageFrame::kDefaultAlignmentBoundary);
            cv::Mat view = mediapipe::formats::MatView(frame.get());

            cv::cvtColor(bgr, view, cv::COLOR_BGR2RGB);

            const auto start = Clock::now();

            ABSL_RETURN_IF_ERROR(graph.AddPacketToInputStream("input_video", mediapipe::Adopt(frame.release()).At(mediapipe::Timestamp(frameIndex * 33333LL))));
            ABSL_RETURN_IF_ERROR(graph.WaitUntilIdle());

            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();

            if (frameIndex == 0)
                firstFrameMs = ms;
            else if (frameIndex >= absl::GetFlag(FLAGS_warmup))
                latenciesMs.push_back(ms);

            ++frameIndex;
        }

        ABSL_RETURN_IF_ERROR(graph.CloseInputStream("input_video"));
        ABSL_RETURN_IF_ERROR(graph.WaitUntilDone());

        if (latenciesMs.empty())
            return absl::FailedPreconditionError("muy pocos frames para estadísticas");

        std::sort(latenciesMs.begin(), latenciesMs.end());

        const double mean = std::accumulate(latenciesMs.begin(), latenciesMs.end(), 0.0) / latenciesMs.size();

        printf("device=%s frames=%d\n", device.c_str(), frameIndex);
        printf("init_ms=%.1f primer_frame_ms=%.1f\n", initMs, firstFrameMs);
        printf("latencia_ms media=%.2f p50=%.2f p95=%.2f min=%.2f max=%.2f\n", mean, Percentile(latenciesMs, 0.5), Percentile(latenciesMs, 0.95), latenciesMs.front(), latenciesMs.back());
        printf("fps_sostenidos=%.1f manos_detectadas_total=%lld\n", 1000.0 / mean, static_cast<long long>(handsSeen));

        return absl::OkStatus();
    }

} // namespace

int main(int argc, char **argv)
{
    absl::ParseCommandLine(argc, argv);

    const absl::Status status = Run();

    if (!status.ok())
    {
        fprintf(stderr, "ERROR: %s\n", status.ToString().c_str());
        return 1;
    }

    return 0;
}
