#ifndef PLEXUS_LANDMARKS_LANDMARKTRACKER_HPP
#define PLEXUS_LANDMARKS_LANDMARKTRACKER_HPP

#include "Config.hpp"
#include "Export.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace plexus::landmarks
{

/**
 * @brief Base de HandTracker y BodyTracker.
 *
 * Esta clase vive en la fachada (libplexus_landmarks) y no depende de MediaPipe
 * ni de EGL. La inferencia ocurre en un motor cargado en tiempo de ejecución:
 * libplexus_landmarks_gpu si hay GPU utilizable y se solicita, y
 * libplexus_landmarks_cpu en otro caso. Ambos deben estar junto a la fachada, o
 * en el directorio de PLEXUS_LANDMARKS_LIBDIR.
 *
 * Con ComputeDevice::Auto, si el motor de GPU falla durante la ejecución, se
 * reinicia el grafo en CPU y se continúa. No debe llamarse a processFrame desde
 * el callback de resultados.
 */
class PLEXUS_LANDMARKS_API LandmarkTracker
{
public:
    virtual ~LandmarkTracker();

    LandmarkTracker(const LandmarkTracker &) = delete;
    LandmarkTracker &operator=(const LandmarkTracker &) = delete;

    bool closeInputStream();

    /// Dispositivo activo: "cpu", "gpu" o cadena vacía si no hay ninguno.
    std::string deviceName() const;
    bool isRunning() const;
    bool processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs);
    void stop();
    bool waitUntilIdle();

protected:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    LandmarkTracker();
};

} // namespace plexus::landmarks

#endif
