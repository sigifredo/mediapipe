#ifndef TRACKERTYPES_HPP
#define TRACKERTYPES_HPP

#include "TrackerExport.hpp"

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Backend de inferencia solicitado.
 *
 * Auto intenta la GPU y cae a CPU si no está disponible. Solo hay GPU en Linux.
 * La variable de entorno PLEXUS_TRACKER_BACKEND (auto, cpu o gpu) tiene
 * prioridad sobre este valor.
 */
enum class TrackerBackend
{
    Auto,
    Cpu,
    Gpu
};

/**
 * @brief Configuración común a todos los trackers.
 */
struct TRACKER_API MediaPipeTrackerConfig
{
    /// Backend solicitado. Ver TrackerBackend.
    TrackerBackend backend = TrackerBackend::Auto;

    /**
     * Ruta del grafo (.pbtxt). Si contiene el texto `{backend}` se sustituye por
     * `cpu` o `gpu` según el backend que quede activo (p. ej.
     * `graphs/pose_{backend}.pbtxt`). Sin el marcador se usa el mismo grafo
     * siempre, y el modo Auto lo trata como grafo de CPU.
     * Un grafo de GPU recibe GpuBuffer en `inputStream`: la biblioteca inserta
     * la conversión desde ImageFrame.
     */
    std::string graphPath;

    /// Nombre del stream de entrada del grafo.
    std::string inputStream = "input_video";
};

struct TRACKER_API HandLandmark
{
    float x, y, z;
    float visibility;
};

struct TRACKER_API WorldLandmark
{
    float x, y, z; // metros, origen en centro geométrico de la mano
};

struct TRACKER_API HandClassification
{
    std::string label; // "Left" | "Right"
    float score;       // 0.0–1.0
};

struct TRACKER_API HandTrackingResult
{
    std::vector<std::vector<HandLandmark>> hands;
    std::vector<std::vector<WorldLandmark>> worldLandmarks; // vacío si no se pidió
    std::vector<HandClassification> handedness;
    int64_t timestampUs;
};

struct TRACKER_API BodyLandmark
{
    float x, y, z;
    float visibility;
    float presence;
};

struct TRACKER_API BodyTrackingResult
{
    std::vector<BodyLandmark> landmarks;      // 33, normalized image coordinates
    std::vector<BodyLandmark> worldLandmarks; // 33, real-world meters (hip-centered)
    int64_t timestampUs;
};

#endif
