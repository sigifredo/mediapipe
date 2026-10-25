#ifndef PLEXUS_LANDMARKS_ENGINELOADER_HPP
#define PLEXUS_LANDMARKS_ENGINELOADER_HPP

#include "backend/EngineApi.hpp"

#include <memory>
#include <string>

namespace plexus::landmarks
{

/**
 * @brief Motor cargado dinámicamente. Destruye la instancia con la función de su propia biblioteca.
 *
 * La biblioteca no se descarga al destruirse: descargar un runtime con hilos,
 * contextos EGL y registros estáticos de MediaPipe no es seguro.
 */
class LoadedEngine
{
public:
    ~LoadedEngine();

    LoadedEngine(const LoadedEngine &) = delete;
    LoadedEngine &operator=(const LoadedEngine &) = delete;

    IEngine &engine() { return *engine_; }
    const std::string &device() const { return device_; }

    /**
     * @brief Carga el motor del dispositivo `device` ("cpu" o "gpu").
     * @param error Recibe el motivo si falla.
     * @return Instancia, o nullptr si falla (biblioteca ausente, dependencias
     *         del sistema como libEGL ausentes, símbolos faltantes).
     */
    static std::unique_ptr<LoadedEngine> load(const std::string &device, std::string &error);

private:
    IEngine *engine_ = nullptr;
    DestroyEngineFn destroy_ = nullptr;
    std::string device_;

    LoadedEngine() = default;
};

} // namespace plexus::landmarks

#endif
