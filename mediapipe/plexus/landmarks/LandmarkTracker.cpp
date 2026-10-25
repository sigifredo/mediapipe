#include "FacadeImpl.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace plexus::landmarks
{

ComputeDevice LandmarkTracker::Impl::effectiveDevice(ComputeDevice requested)
{
    const char *env = std::getenv("PLEXUS_LANDMARKS_DEVICE");

    if (!env || !*env)
        return requested;

    if (!std::strcmp(env, "cpu"))
        return ComputeDevice::Cpu;

    if (!std::strcmp(env, "gpu"))
        return ComputeDevice::Gpu;

    if (!std::strcmp(env, "auto"))
        return ComputeDevice::Auto;

    fprintf(stderr, "[plexus_landmarks] PLEXUS_LANDMARKS_DEVICE='%s' no es válido (auto, cpu, gpu): se ignora\n", env);

    return requested;
}

bool LandmarkTracker::Impl::fallbackToCpu()
{
    fprintf(stderr, "[plexus_landmarks] el motor gpu falló en ejecución: se reinicia en cpu\n");

    active.reset();
    allowFallback = false;

    std::string error;
    auto cpu = LoadedEngine::load("cpu", error);

    if (!cpu)
    {
        fprintf(stderr, "[plexus_landmarks] no se pudo cargar el motor cpu: %s\n", error.c_str());
        return false;
    }

    TrackerConfig resolved = config;

    resolved.graphPath = resolveGraphPath(config.graphPath, "cpu");

    if (!initializer(cpu->engine(), resolved))
        return false;

    active = std::move(cpu);
    return true;
}

std::string LandmarkTracker::Impl::resolveGraphPath(const std::string &graphPath, const std::string &device)
{
    static const std::string marker = "{device}";
    std::string resolved = graphPath;

    for (auto pos = resolved.find(marker); pos != std::string::npos; pos = resolved.find(marker, pos + device.size()))
        resolved.replace(pos, marker.size(), device);

    return resolved;
}

bool LandmarkTracker::Impl::start(const TrackerConfig &requested, Initializer init)
{
    config = requested;
    initializer = std::move(init);
    active.reset();
    allowFallback = false;

    const ComputeDevice mode = effectiveDevice(requested.device);
    const bool hasMarker = requested.graphPath.find("{device}") != std::string::npos;
    std::vector<std::string> candidates;

    if (mode == ComputeDevice::Cpu)
        candidates = {"cpu"};
    else if (mode == ComputeDevice::Gpu)
        candidates = {"gpu"};
    else
        candidates = hasMarker ? std::vector<std::string>{"gpu", "cpu"} : std::vector<std::string>{"cpu"};

    for (const auto &device : candidates)
    {
        std::string error;
        auto loaded = LoadedEngine::load(device, error);

        if (!loaded)
        {
            fprintf(stderr, "[plexus_landmarks] dispositivo %s no disponible: %s\n", device.c_str(), error.c_str());
            continue;
        }

        TrackerConfig resolved = requested;

        resolved.graphPath = resolveGraphPath(requested.graphPath, device);

        if (!initializer(loaded->engine(), resolved))
        {
            fprintf(stderr, "[plexus_landmarks] dispositivo %s no pudo iniciar el grafo\n", device.c_str());
            continue;
        }

        active = std::move(loaded);
        allowFallback = mode == ComputeDevice::Auto && device == "gpu";
        fprintf(stderr, "[plexus_landmarks] dispositivo activo: %s\n", device.c_str());

        return true;
    }

    return false;
}

LandmarkTracker::~LandmarkTracker() { stop(); }

std::string LandmarkTracker::deviceName() const { return impl_->active ? impl_->active->device() : std::string(); }

bool LandmarkTracker::closeInputStream() { return impl_->active && impl_->active->engine().closeInputStream(); }

bool LandmarkTracker::isRunning() const { return impl_->active && impl_->active->engine().isRunning(); }

bool LandmarkTracker::processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs)
{
    if (!impl_->active)
        return false;

    if (impl_->allowFallback && impl_->active->engine().hasFailed() && !impl_->fallbackToCpu())
        return false;

    if (impl_->active->engine().processFrame(rgbData, width, height, timestampUs))
        return true;

    if (impl_->allowFallback && impl_->active->engine().hasFailed() && impl_->fallbackToCpu())
        return impl_->active->engine().processFrame(rgbData, width, height, timestampUs);

    return false;
}

void LandmarkTracker::stop()
{
    if (impl_ && impl_->active)
        impl_->active->engine().stop();
}

bool LandmarkTracker::waitUntilIdle() { return impl_->active && impl_->active->engine().waitUntilIdle(); }

LandmarkTracker::LandmarkTracker() : impl_(std::make_unique<Impl>()) {}

} // namespace plexus::landmarks
