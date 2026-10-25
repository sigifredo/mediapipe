#include "BackendLoader.hpp"

#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{

/// Directorio donde está esta biblioteca (la fachada), o PLEXUS_TRACKER_LIBDIR si se define.
std::string LibraryDirectory()
{
    if (const char *override = std::getenv("PLEXUS_TRACKER_LIBDIR"))
        return override;

#ifdef _WIN32
    HMODULE module = nullptr;
    char path[MAX_PATH] = {0};

    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(&LibraryDirectory), &module) || !GetModuleFileNameA(module, path, MAX_PATH))
        return ".";

    std::string full(path);
    const auto slash = full.find_last_of("\\/");

    return slash == std::string::npos ? "." : full.substr(0, slash);
#else
    Dl_info info;

    if (!dladdr(reinterpret_cast<void *>(&LibraryDirectory), &info) || !info.dli_fname)
        return ".";

    std::string full(info.dli_fname);
    const auto slash = full.find_last_of('/');

    return slash == std::string::npos ? "." : full.substr(0, slash);
#endif
}

std::string LibraryFileName(const std::string &kind)
{
#ifdef _WIN32
    return "hand_tracker_" + kind + ".dll";
#else
    return "libhand_tracker_" + kind + ".so";
#endif
}

} // namespace

LoadedBackend::~LoadedBackend()
{
    if (backend_ && destroy_)
        destroy_(backend_);
}

std::unique_ptr<LoadedBackend> LoadedBackend::load(const std::string &kind, std::string &error)
{
    const std::string path = LibraryDirectory() + "/" + LibraryFileName(kind);

#ifdef _WIN32
    HMODULE lib = LoadLibraryA(path.c_str());

    if (!lib)
    {
        error = "LoadLibrary falló (" + std::to_string(GetLastError()) + "): " + path;
        return nullptr;
    }

    auto create = reinterpret_cast<PlexusCreateBackendFn>(GetProcAddress(lib, PLEXUS_CREATE_BACKEND_SYMBOL));
    auto destroy = reinterpret_cast<PlexusDestroyBackendFn>(GetProcAddress(lib, PLEXUS_DESTROY_BACKEND_SYMBOL));
#else
    // RTLD_NOW: si falta una dependencia del sistema (p. ej. libEGL) se falla aquí y no más tarde.
    void *lib = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);

    if (!lib)
    {
        error = dlerror();
        return nullptr;
    }

    auto create = reinterpret_cast<PlexusCreateBackendFn>(dlsym(lib, PLEXUS_CREATE_BACKEND_SYMBOL));
    auto destroy = reinterpret_cast<PlexusDestroyBackendFn>(dlsym(lib, PLEXUS_DESTROY_BACKEND_SYMBOL));
#endif

    if (!create || !destroy)
    {
        error = "la biblioteca no exporta los símbolos de backend: " + path;
        return nullptr;
    }

    std::unique_ptr<LoadedBackend> loaded(new LoadedBackend());

    loaded->backend_ = create();
    loaded->destroy_ = destroy;
    loaded->kind_ = kind;

    return loaded;
}
