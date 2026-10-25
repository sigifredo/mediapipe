#ifndef BACKENDLOADER_HPP
#define BACKENDLOADER_HPP

#include "backend/BackendApi.hpp"

#include <memory>
#include <string>

/**
 * @brief Backend cargado dinámicamente. Destruye la instancia con la función de su propia biblioteca.
 *
 * La biblioteca no se descarga al destruirse: descargar un runtime con hilos,
 * contextos EGL y registros estáticos de MediaPipe no es seguro.
 */
class LoadedBackend
{
public:
    ~LoadedBackend();

    LoadedBackend(const LoadedBackend &) = delete;
    LoadedBackend &operator=(const LoadedBackend &) = delete;

    IBackend &backend() { return *backend_; }
    const std::string &kind() const { return kind_; }

    /**
     * @brief Carga el backend `kind` ("cpu" o "gpu").
     * @param error Recibe el motivo si falla.
     * @return Instancia, o nullptr si falla (biblioteca ausente, dependencias
     *         del sistema como libEGL ausentes, símbolos faltantes).
     */
    static std::unique_ptr<LoadedBackend> load(const std::string &kind, std::string &error);

private:
    IBackend *backend_ = nullptr;
    PlexusDestroyBackendFn destroy_ = nullptr;
    std::string kind_;

    LoadedBackend() = default;
};

#endif
