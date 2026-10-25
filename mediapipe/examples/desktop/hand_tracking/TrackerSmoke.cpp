/**
 * @file TrackerSmoke.cpp
 * @brief Prueba de humo de libhand_tracker: selección de backend y procesamiento de frames.
 *
 * Uso: tracker_smoke <hand|body> <graphPath> <auto|cpu|gpu> [frames]
 * Imprime `backend=<cpu|gpu>` y `frames_ok=<n>`. Sale con 1 si no puede iniciar.
 * No depende de MediaPipe: solo de la fachada. Ver test_tracker.sh.
 */

#include "BodyTracker.hpp"
#include "HandTracker.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{

TrackerBackend ParseBackend(const char *text)
{
    if (!std::strcmp(text, "cpu"))
        return TrackerBackend::Cpu;

    if (!std::strcmp(text, "gpu"))
        return TrackerBackend::Gpu;

    return TrackerBackend::Auto;
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

    const std::string kind = argv[1];
    const int frames = argc > 4 ? std::atoi(argv[4]) : 30;
    MediaPipeTrackerConfig config;

    config.graphPath = argv[2];
    config.backend = ParseBackend(argv[3]);

    std::atomic<int> results{0};
    int accepted = 0;
    std::string backend;

    if (kind == "hand")
    {
        HandTracker tracker;

        tracker.setResultCallback([&](const HandTrackingResult &)
                                  { ++results; });

        if (!tracker.initialize(config, "hand_landmarks", "handedness", "hand_world_landmarks"))
            return 1;

        accepted = Feed(tracker, frames);
        backend = tracker.backendName();
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
        backend = tracker.backendName();
        tracker.stop();
    }

    printf("backend=%s frames_ok=%d/%d results=%d\n", backend.c_str(), accepted, frames, results.load());

    return accepted == frames ? 0 : 1;
}
