

#ifndef PLEXUS_LANDMARKS_ENGINEAPI_HPP
#define PLEXUS_LANDMARKS_ENGINEAPI_HPP

// own
#include <BodyTypes.hpp>
#include <Config.hpp>
#include <HandTypes.hpp>
#include <Namespace.hpp>

// std
#include <cstdint>
#include <functional>
#include <string>

PLEXUS_LANDMARKS_NAMESPACE_BEGIN

/**
 * @brief Contrato entre la fachada (libplexus_landmarks) y los motores
 * (libplexus_landmarks_cpu / libplexus_landmarks_gpu).
 *
 * La fachada carga un motor con dlopen y lo usa solo a través de esta
 * interfaz virtual, de modo que no necesita enlazar símbolos del motor ni
 * depender de EGL. Fachada y motores deben compilarse con el mismo toolchain.
 */
class IEngine
{
public:
    using BodyCallback = std::function<void(const BodyTrackingResult &)>;
    using HandCallback = std::function<void(const HandTrackingResult &)>;

    virtual ~IEngine() = default;

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
    virtual bool initializeBody(const TrackerConfig &config, const std::string &landmarkStream, const std::string &worldLandmarkStream, BodyCallback callback) = 0;

    /**
     * @brief Inicia un grafo de manos.
     * @param config Configuración con `graphPath` ya resuelto.
     * @param landmarkStream Stream de landmarks normalizados.
     * @param handednessStream Stream de lateralidad.
     * @param worldLandmarkStream Stream de landmarks en metros (vacío si no se pide).
     * @param callback Se invoca desde un hilo del grafo con cada resultado.
     */
    virtual bool initializeHand(const TrackerConfig &config, const std::string &landmarkStream, const std::string &handednessStream, const std::string &worldLandmarkStream, HandCallback callback) = 0;

    virtual bool isRunning() const = 0;

    /// Envía un frame RGB (3 bytes por píxel, filas contiguas).
    virtual bool processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs) = 0;

    virtual void stop() = 0;
    virtual bool waitUntilIdle() = 0;
};

PLEXUS_LANDMARKS_NAMESPACE_END

/// Símbolos que exporta cada biblioteca de motor.
extern "C"
{
    typedef plexus::landmarks::IEngine *(*CreateEngineFn)();
    typedef void (*DestroyEngineFn)(plexus::landmarks::IEngine *);
}

#ifdef _WIN32
#define PLEXUS_LANDMARKS_ENGINE_EXPORT __declspec(dllexport)
#else
#define PLEXUS_LANDMARKS_ENGINE_EXPORT __attribute__((visibility("default")))
#endif

#define PLEXUS_LANDMARKS_CREATE_ENGINE_SYMBOL "plexusLandmarksCreateEngine"
#define PLEXUS_LANDMARKS_DESTROY_ENGINE_SYMBOL "plexusLandmarksDestroyEngine"

#endif
