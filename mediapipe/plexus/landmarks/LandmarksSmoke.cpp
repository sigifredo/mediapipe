

/**
 * @file LandmarksSmoke.cpp
 * @brief Prueba de humo de libplexus_landmarks: selección de dispositivo y procesamiento de frames.
 *
 * Uso: landmarks_smoke <hand|body> <graphPath> <auto|cpu|gpu> [frames]
 * Imprime `device=<cpu|gpu>`, `frames_ok=<n>` y `close_ok=<0|1>` (resultado de closeInputStream).
 * Sale con 1 si no puede iniciar, si se rechaza algún frame o si falla el cierre.
 * No depende de MediaPipe: solo de la fachada. Ver test_landmarks.sh.
 */

// own
#include <BodyTracker.hpp>
#include <HandTracker.hpp>

// std
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace plexus::landmarks;

namespace
{
    ComputeDevice ParseDevice(const char *text)
    {
        if (!std::strcmp(text, "cpu"))
            return ComputeDevice::Cpu;

        if (!std::strcmp(text, "gpu"))
            return ComputeDevice::Gpu;

        return ComputeDevice::Auto;
    }

    /**
     * @brief Envía `frames` frames sintéticos y devuelve cuántos se aceptaron.
     */
    template <typename Tracker>
    int Feed(Tracker &tracker, int frames)
    {
        const int width = 640, height = 480;
        std::vector<uint8_t> rgb(width * height * 3);
        int accepted = 0;

        for (int i = 0; i < frames; ++i)
        {
            for (size_t p = 0; p < rgb.size(); ++p)
                rgb[p] = static_cast<uint8_t>((p + i * 7) & 0xFF);

            if (tracker.processFrame(rgb.data(), width, height, i * 33333LL))
                ++accepted;

            tracker.waitUntilIdle();
        }

        return accepted;
    }

} // namespace

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "uso: %s <hand|body> <graphPath> <auto|cpu|gpu> [frames]\n", argv[0]);
        return 2;
    }

    const std::string trackerKind = argv[1];
    const int frames = argc > 4 ? std::atoi(argv[4]) : 30;
    TrackerConfig config;

    config.graphPath = argv[2];
    config.device = ParseDevice(argv[3]);

    std::atomic<int> results{0};
    int accepted = 0;
    bool closed = false;
    std::string device;

    if (trackerKind == "hand")
    {
        HandTracker tracker;

        tracker.setResultCallback([&](const HandTrackingResult &)
                                  { ++results; });

        if (!tracker.initialize(config, "hand_landmarks", "handedness", "hand_world_landmarks"))
            return 1;

        accepted = Feed(tracker, frames);
        device = tracker.deviceName();
        closed = tracker.closeInputStream();
        tracker.stop();
    }
    else
    {
        BodyTracker tracker;

        tracker.setResultCallback([&](const BodyTrackingResult &)
                                  { ++results; });

        if (!tracker.initialize(config, "pose_landmarks", "pose_world_landmarks"))
            return 1;

        accepted = Feed(tracker, frames);
        device = tracker.deviceName();
        closed = tracker.closeInputStream();
        tracker.stop();
    }

    printf("device=%s frames_ok=%d/%d results=%d close_ok=%d\n", device.c_str(), accepted, frames, results.load(), closed ? 1 : 0);

    return accepted == frames && closed ? 0 : 1;
}
