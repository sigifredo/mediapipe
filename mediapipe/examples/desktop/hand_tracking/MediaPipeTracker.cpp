#include "FacadeImpl.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

TrackerBackend MediaPipeTracker::Impl::effectiveBackend(TrackerBackend requested)
{
    const char *env = std::getenv("PLEXUS_TRACKER_BACKEND");

    if (!env || !*env)
        return requested;

    if (!std::strcmp(env, "cpu"))
        return TrackerBackend::Cpu;

    if (!std::strcmp(env, "gpu"))
        return TrackerBackend::Gpu;

    if (!std::strcmp(env, "auto"))
        return TrackerBackend::Auto;

    fprintf(stderr, "[hand_tracker] PLEXUS_TRACKER_BACKEND='%s' no es válido (auto, cpu, gpu): se ignora\n", env);

    return requested;
}

bool MediaPipeTracker::Impl::fallbackToCpu()
{
    fprintf(stderr, "[hand_tracker] el backend gpu falló en ejecución: se reinicia en cpu\n");

    active.reset();
    allowFallback = false;

    std::string error;
    auto cpu = LoadedBackend::load("cpu", error);

    if (!cpu)
    {
        fprintf(stderr, "[hand_tracker] no se pudo cargar cpu: %s\n", error.c_str());
        return false;
    }

    MediaPipeTrackerConfig resolved = config;

    resolved.graphPath = resolveGraphPath(config.graphPath, "cpu");

    if (!initializer(cpu->backend(), resolved))
        return false;

    active = std::move(cpu);
    return true;
}

std::string MediaPipeTracker::Impl::resolveGraphPath(const std::string &graphPath, const std::string &kind)
{
    static const std::string marker = "{backend}";
    std::string resolved = graphPath;

    for (auto pos = resolved.find(marker); pos != std::string::npos; pos = resolved.find(marker, pos + kind.size()))
        resolved.replace(pos, marker.size(), kind);

    return resolved;
}

bool MediaPipeTracker::Impl::start(const MediaPipeTrackerConfig &requested, Initializer init)
{
    config = requested;
    initializer = std::move(init);
    active.reset();
    allowFallback = false;

    const TrackerBackend mode = effectiveBackend(requested.backend);
    const bool hasMarker = requested.graphPath.find("{backend}") != std::string::npos;
    std::vector<std::string> candidates;

    if (mode == TrackerBackend::Cpu)
        candidates = {"cpu"};
    else if (mode == TrackerBackend::Gpu)
        candidates = {"gpu"};
    else
        candidates = hasMarker ? std::vector<std::string>{"gpu", "cpu"} : std::vector<std::string>{"cpu"};

    for (const auto &kind : candidates)
    {
        std::string error;
        auto loaded = LoadedBackend::load(kind, error);

        if (!loaded)
        {
            fprintf(stderr, "[hand_tracker] backend %s no disponible: %s\n", kind.c_str(), error.c_str());
            continue;
        }

        MediaPipeTrackerConfig resolved = requested;

        resolved.graphPath = resolveGraphPath(requested.graphPath, kind);

        if (!initializer(loaded->backend(), resolved))
        {
            fprintf(stderr, "[hand_tracker] backend %s no pudo iniciar el grafo\n", kind.c_str());
            continue;
        }

        active = std::move(loaded);
        allowFallback = mode == TrackerBackend::Auto && kind == "gpu";
        fprintf(stderr, "[hand_tracker] backend activo: %s\n", kind.c_str());

        return true;
    }

    return false;
}

MediaPipeTracker::~MediaPipeTracker() { stop(); }

std::string MediaPipeTracker::backendName() const { return impl_->active ? impl_->active->kind() : std::string(); }

bool MediaPipeTracker::closeInputStream() { return impl_->active && impl_->active->backend().closeInputStream(); }

bool MediaPipeTracker::isRunning() const { return impl_->active && impl_->active->backend().isRunning(); }

bool MediaPipeTracker::processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs)
{
    if (!impl_->active)
        return false;

    if (impl_->allowFallback && impl_->active->backend().hasFailed() && !impl_->fallbackToCpu())
        return false;

    if (impl_->active->backend().processFrame(rgbData, width, height, timestampUs))
        return true;

    if (impl_->allowFallback && impl_->active->backend().hasFailed() && impl_->fallbackToCpu())
        return impl_->active->backend().processFrame(rgbData, width, height, timestampUs);

    return false;
}

void MediaPipeTracker::stop()
{
    if (impl_ && impl_->active)
        impl_->active->backend().stop();
}

bool MediaPipeTracker::waitUntilIdle() { return impl_->active && impl_->active->backend().waitUntilIdle(); }

MediaPipeTracker::MediaPipeTracker() : impl_(std::make_unique<Impl>()) {}
