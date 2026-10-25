

// own
#include <EngineLoader.hpp>

// std
#include <cstdlib>

// system
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

PLEXUS_LANDMARKS_NAMESPACE_BEGIN

namespace
{
    /// Directorio donde está esta biblioteca (la fachada), o PLEXUS_LANDMARKS_LIBDIR si se define.
    std::string LibraryDirectory()
    {
        if (const char *override = std::getenv("PLEXUS_LANDMARKS_LIBDIR"))
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

    std::string LibraryFileName(const std::string &device)
    {
#ifdef _WIN32
        return "plexus_landmarks_" + device + ".dll";
#else
        return "libplexus_landmarks_" + device + ".so";
#endif
    }

} // namespace

LoadedEngine::~LoadedEngine()
{
    if (engine_ && destroy_)
        destroy_(engine_);
}

std::unique_ptr<LoadedEngine> LoadedEngine::load(const std::string &device, std::string &error)
{
    const std::string path = LibraryDirectory() + "/" + LibraryFileName(device);

#ifdef _WIN32
    HMODULE lib = LoadLibraryA(path.c_str());

    if (!lib)
    {
        error = "LoadLibrary falló (" + std::to_string(GetLastError()) + "): " + path;
        return nullptr;
    }

    auto create = reinterpret_cast<CreateEngineFn>(GetProcAddress(lib, PLEXUS_LANDMARKS_CREATE_ENGINE_SYMBOL));
    auto destroy = reinterpret_cast<DestroyEngineFn>(GetProcAddress(lib, PLEXUS_LANDMARKS_DESTROY_ENGINE_SYMBOL));
#else
    // RTLD_NOW: si falta una dependencia del sistema (p. ej. libEGL) se falla aquí y no más tarde.
    void *lib = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);

    if (!lib)
    {
        error = dlerror();
        return nullptr;
    }

    auto create = reinterpret_cast<CreateEngineFn>(dlsym(lib, PLEXUS_LANDMARKS_CREATE_ENGINE_SYMBOL));
    auto destroy = reinterpret_cast<DestroyEngineFn>(dlsym(lib, PLEXUS_LANDMARKS_DESTROY_ENGINE_SYMBOL));
#endif

    if (!create || !destroy)
    {
        error = "la biblioteca no exporta los símbolos de motor: " + path;
        return nullptr;
    }

    std::unique_ptr<LoadedEngine> loaded(new LoadedEngine());

    loaded->engine_ = create();
    loaded->destroy_ = destroy;
    loaded->device_ = device;

    return loaded;
}

PLEXUS_LANDMARKS_NAMESPACE_END
