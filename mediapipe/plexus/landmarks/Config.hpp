#ifndef PLEXUS_LANDMARKS_CONFIG_HPP
#define PLEXUS_LANDMARKS_CONFIG_HPP

#include "Export.hpp"

#include <string>

namespace plexus::landmarks
{

/**
 * @brief Dispositivo de cómputo solicitado para la inferencia.
 *
 * Auto intenta la GPU y cae a CPU si no está disponible. Solo hay GPU en Linux.
 * La variable de entorno PLEXUS_LANDMARKS_DEVICE (auto, cpu o gpu) tiene
 * prioridad sobre este valor.
 */
enum class ComputeDevice
{
    Auto,
    Cpu,
    Gpu
};

/**
 * @brief Configuración común a HandTracker y BodyTracker.
 */
struct PLEXUS_LANDMARKS_API TrackerConfig
{
    /// Dispositivo solicitado. Ver ComputeDevice.
    ComputeDevice device = ComputeDevice::Auto;

    /**
     * Ruta del grafo (.pbtxt). Si contiene el texto `{device}` se sustituye por
     * `cpu` o `gpu` según el dispositivo que quede activo (p. ej.
     * `graphs/body_{device}.pbtxt`). Sin el marcador se usa el mismo grafo
     * siempre, y el modo Auto lo trata como grafo de CPU.
     * Un grafo de GPU recibe GpuBuffer en `inputStream`: la biblioteca inserta
     * la conversión desde ImageFrame.
     */
    std::string graphPath;

    /// Nombre del stream de entrada del grafo.
    std::string inputStream = "input_video";
};

} // namespace plexus::landmarks

#endif
