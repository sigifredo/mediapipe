#ifndef MEDIAPIPETRACKER_HPP
#define MEDIAPIPETRACKER_HPP

#include "TrackerExport.hpp"
#include "TrackerTypes.hpp"

#include <cstdint>
#include <memory>
#include <string>

/**
 * @brief Base de HandTracker y BodyTracker.
 *
 * Esta clase vive en la fachada (libhand_tracker) y no depende de MediaPipe ni
 * de EGL. La inferencia ocurre en un backend cargado en tiempo de ejecución:
 * libhand_tracker_gpu si hay GPU utilizable y se solicita, y
 * libhand_tracker_cpu en otro caso. Ambos deben estar junto a la fachada, o en
 * el directorio de PLEXUS_TRACKER_LIBDIR.
 *
 * Con TrackerBackend::Auto, si el backend de GPU falla durante la ejecución, se
 * reinicia el grafo en CPU y se continúa. No debe llamarse a processFrame desde
 * el callback de resultados.
 */
class TRACKER_API MediaPipeTracker
{
public:
    virtual ~MediaPipeTracker();

    MediaPipeTracker(const MediaPipeTracker &) = delete;
    MediaPipeTracker &operator=(const MediaPipeTracker &) = delete;

    /// Backend activo: "cpu", "gpu" o cadena vacía si no hay ninguno.
    std::string backendName() const;
    bool closeInputStream();
    bool isRunning() const;
    bool processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs);
    void stop();
    bool waitUntilIdle();

protected:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    MediaPipeTracker();
};

#endif
