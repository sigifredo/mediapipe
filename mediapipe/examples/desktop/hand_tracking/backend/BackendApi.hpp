#ifndef BACKENDAPI_HPP
#define BACKENDAPI_HPP

#include "../TrackerTypes.hpp"

#include <cstdint>
#include <functional>
#include <string>

/**
 * @brief Contrato entre la fachada (libhand_tracker) y los backends
 * (libhand_tracker_cpu / libhand_tracker_gpu).
 *
 * La fachada carga un backend con dlopen y lo usa solo a través de esta
 * interfaz virtual, de modo que no necesita enlazar símbolos del backend ni
 * depender de EGL. Fachada y backends deben compilarse con el mismo toolchain.
 */
class IBackend
{
public:
    using BodyCallback = std::function<void(const BodyTrackingResult &)>;
    using HandCallback = std::function<void(const HandTrackingResult &)>;

    virtual ~IBackend() = default;

    /// Cierra el stream de entrada. Falso si no estaba abierto.
    virtual bool closeInputStream() = 0;

    /// Verdadero si el grafo registró un error en ejecución.
    virtual bool hasFailed() const = 0;

    /**
     * @brief Inicia un grafo de cuerpo.
     * @param config Configuración con `graphPath` ya resuelto.
     * @param landmarkStream Stream de landmarks normalizados.
     * @param worldLandmarkStream Stream de landmarks en metros.
     * @param callback Se invoca desde un hilo del grafo con cada resultado.
     */
    virtual bool initializeBody(const MediaPipeTrackerConfig &config, const std::string &landmarkStream, const std::string &worldLandmarkStream, BodyCallback callback) = 0;

    /**
     * @brief Inicia un grafo de manos.
     * @param config Configuración con `graphPath` ya resuelto.
     * @param landmarkStream Stream de landmarks normalizados.
     * @param handednessStream Stream de lateralidad.
     * @param worldLandmarkStream Stream de landmarks en metros (vacío si no se pide).
     * @param callback Se invoca desde un hilo del grafo con cada resultado.
     */
    virtual bool initializeHand(const MediaPipeTrackerConfig &config, const std::string &landmarkStream, const std::string &handednessStream, const std::string &worldLandmarkStream, HandCallback callback) = 0;

    virtual bool isRunning() const = 0;

    /// Envía un frame RGB (3 bytes por píxel, filas contiguas).
    virtual bool processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs) = 0;

    virtual void stop() = 0;
    virtual bool waitUntilIdle() = 0;
};

/// Símbolos que exporta cada biblioteca de backend.
extern "C"
{
    typedef IBackend *(*PlexusCreateBackendFn)();
    typedef void (*PlexusDestroyBackendFn)(IBackend *);
}

#ifdef _WIN32
#define PLEXUS_BACKEND_EXPORT __declspec(dllexport)
#else
#define PLEXUS_BACKEND_EXPORT __attribute__((visibility("default")))
#endif

#define PLEXUS_CREATE_BACKEND_SYMBOL "plexusCreateBackend"
#define PLEXUS_DESTROY_BACKEND_SYMBOL "plexusDestroyBackend"

#endif
