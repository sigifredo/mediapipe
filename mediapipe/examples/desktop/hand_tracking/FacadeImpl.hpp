#ifndef FACADEIMPL_HPP
#define FACADEIMPL_HPP

#include "BackendLoader.hpp"
#include "MediaPipeTracker.hpp"

#include <functional>
#include <memory>
#include <string>

/**
 * @brief Estado interno de la fachada, compartido por MediaPipeTracker, HandTracker y BodyTracker.
 */
struct MediaPipeTracker::Impl
{
    using Initializer = std::function<bool(IBackend &, const MediaPipeTrackerConfig &)>;

    std::unique_ptr<LoadedBackend> active;
    bool allowFallback = false;
    MediaPipeTrackerConfig config;
    Initializer initializer;

    /// Vuelve a iniciar el tracker en CPU tras un fallo del backend de GPU.
    bool fallbackToCpu();

    /**
     * @brief Elige un backend, lo carga y lo inicia con `init`.
     *
     * Orden según el backend efectivo (PLEXUS_TRACKER_BACKEND tiene prioridad
     * sobre config.backend): cpu, gpu, o auto (gpu y luego cpu si el grafo usa
     * `{backend}`; solo cpu si no).
     */
    bool start(const MediaPipeTrackerConfig &requested, Initializer init);

    /// Backend efectivo tras aplicar la variable de entorno.
    static TrackerBackend effectiveBackend(TrackerBackend requested);

    /// Sustituye `{backend}` en la ruta del grafo.
    static std::string resolveGraphPath(const std::string &graphPath, const std::string &kind);
};

#endif
