#ifndef PLEXUS_LANDMARKS_FACADEIMPL_HPP
#define PLEXUS_LANDMARKS_FACADEIMPL_HPP

#include "EngineLoader.hpp"
#include "LandmarkTracker.hpp"

#include <functional>
#include <memory>
#include <string>

namespace plexus::landmarks
{

/**
 * @brief Estado interno de la fachada, compartido por LandmarkTracker, HandTracker y BodyTracker.
 */
struct LandmarkTracker::Impl
{
    using Initializer = std::function<bool(IEngine &, const TrackerConfig &)>;

    std::unique_ptr<LoadedEngine> active;
    bool allowFallback = false;
    TrackerConfig config;
    Initializer initializer;

    /// Vuelve a iniciar el tracker en CPU tras un fallo del motor de GPU.
    bool fallbackToCpu();

    /**
     * @brief Elige un dispositivo, carga su motor y lo inicia con `init`.
     *
     * Orden según el dispositivo efectivo (PLEXUS_LANDMARKS_DEVICE tiene prioridad
     * sobre config.device): cpu, gpu, o auto (gpu y luego cpu si el grafo usa
     * `{device}`; solo cpu si no).
     */
    bool start(const TrackerConfig &requested, Initializer init);

    /// Dispositivo efectivo tras aplicar la variable de entorno.
    static ComputeDevice effectiveDevice(ComputeDevice requested);

    /// Sustituye `{device}` en la ruta del grafo.
    static std::string resolveGraphPath(const std::string &graphPath, const std::string &device);
};

} // namespace plexus::landmarks

#endif
