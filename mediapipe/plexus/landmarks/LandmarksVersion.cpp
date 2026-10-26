

// own
#include <LandmarksVersion.hpp>

// build_plexus.{sh,ps1} generan VersionInfo.generated.hpp (ignorado por git) con estas macros. Solo
// cambia este archivo en cada build, así que no se recompila nada más. Sin él la biblioteca se
// identifica como no versionada.
#if __has_include(<VersionInfo.generated.hpp>)
#include <VersionInfo.generated.hpp>
#endif

#ifndef PLEXUS_LANDMARKS_VERSION
#define PLEXUS_LANDMARKS_VERSION "0.0.0+unversioned"
#endif

#ifndef PLEXUS_LANDMARKS_REVISION
#define PLEXUS_LANDMARKS_REVISION "unknown"
#endif

PLEXUS_LANDMARKS_NAMESPACE_BEGIN

const char *version()
{
    return PLEXUS_LANDMARKS_VERSION;
}

const char *revision()
{
    return PLEXUS_LANDMARKS_REVISION;
}

const char *describe()
{
    return PLEXUS_LANDMARKS_VERSION " (" PLEXUS_LANDMARKS_REVISION ")";
}

PLEXUS_LANDMARKS_NAMESPACE_END
