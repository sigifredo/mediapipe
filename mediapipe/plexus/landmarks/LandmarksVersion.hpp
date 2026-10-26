

#ifndef PLEXUS_LANDMARKS_LANDMARKSVERSION_HPP
#define PLEXUS_LANDMARKS_LANDMARKSVERSION_HPP

// own
#include <Export.hpp>
#include <Namespace.hpp>

PLEXUS_LANDMARKS_NAMESPACE_BEGIN

/**
 * @brief Versión de la biblioteca: `<versión de MediaPipe>+plexus.<revisión propia>`, por ejemplo `1.1.1+plexus.1`.
 *
 * La parte antes de `+` es la base de upstream (`MEDIAPIPE_FULL_VERSION`); la revisión propia sube con cada
 * cambio del fork que se vendoriza y vuelve a 1 al cambiar de base. Sale de `mediapipe/plexus/VERSION`.
 */
PLEXUS_LANDMARKS_API const char *version();

/// Commit del fork con el que se compiló, con sufijo `-dirty` si había cambios sin confirmar.
PLEXUS_LANDMARKS_API const char *revision();

/// `version()` y `revision()` juntos: `1.1.1+plexus.1 (7ba399584)`. Es lo que se compara entre fachada y motor.
PLEXUS_LANDMARKS_API const char *describe();

PLEXUS_LANDMARKS_NAMESPACE_END

#endif
