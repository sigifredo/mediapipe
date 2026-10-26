# Changelog de Plexus sobre MediaPipe

Cambios del fork respecto a MediaPipe upstream. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y la numeración es `<base de upstream>+plexus.<N>`, descrita en [`PLEXUS.md`](../../PLEXUS.md#versionado): `N` sube con cada cambio del fork que se vendoriza en Plexus y vuelve a 1 al cambiar de base. Los cambios de upstream no se registran aquí: están en sus notas de versión.

Cada entrada lleva el tag `v<versión>` en el commit exacto. Al preparar una versión se actualiza `mediapipe/plexus/VERSION` y se mueve `[Sin publicar]` a una sección nueva.

## [Sin publicar]

## [1.1.1+plexus.1] - 2026-10-26

Primera versión numerada. Base: MediaPipe `v1.1.1`.

### Añadido

- **landmarks**: versionado de las bibliotecas. `plexus::landmarks::version()`, `revision()` y `describe()` (`LandmarksVersion.hpp`), el símbolo `plexusLandmarksEngineVersion` de los motores y `dist/VERSION` (versión, base, commit con `-dirty` si el árbol tenía cambios, fecha y plataforma). La fachada rechaza un motor con otro `describe()`. `build_plexus.{sh,ps1}` validan `mediapipe/plexus/VERSION` contra `MEDIAPIPE_FULL_VERSION`.
- **landmarks**: fachada `libplexus_landmarks` (sin dependencia de MediaPipe ni EGL) y motores `libplexus_landmarks_cpu` y `libplexus_landmarks_gpu` cargados con dlopen/LoadLibrary. Selección de dispositivo `Auto`/`Cpu`/`Gpu`, marcador `{device}` en la ruta del grafo, variables de entorno `PLEXUS_LANDMARKS_*` y fallback de GPU a CPU al iniciar y en ejecución.
- **build**: `build_plexus.sh` (Linux y macOS) y `build_plexus.ps1` (Windows), que dejan las bibliotecas, los modelos y `VERSION` en `dist/`.
- **test**: `test_landmarks.{sh,ps1}`, con escenarios de selección y fallback sobre frames sintéticos, la versión de la fachada y el rechazo de un motor de otra versión; `landmarks_bench` para medir latencia.
- **macOS**: motor CPU en Apple Silicon (`.dylib`, install name corregido, firma ad-hoc y exportación restringida de símbolos).
- **Windows**: motor CPU con MSVC (release), con `.lib` regenerado desde los exports de la DLL y JDK embebido de Bazel.
- **tracking**: landmarks de mundo (en metros) en `BodyTracker` y `HandTracker`.
- **build**: `MODULE.bazel.lock` versionado.
- **docs**: `PLEXUS.md` con los parches sobre upstream (marcados con `plexus-patch:`), las decisiones de diseño, el procedimiento de actualización de base y el estado de verificación.

### Cambiado

- **base**: se integran `v1.1.0` y `v1.1.1` de upstream (antes `v0.10.32`) sin conflictos textuales. El parche de `ABSL_CONST_INIT` en `gpu_service.cc` queda absorbido por upstream.
- **landmarks**: la biblioteca `libhand_tracker` pasa a llamarse `plexus_landmarks`, vive en `mediapipe/plexus/landmarks/` (el árbol de upstream queda intacto) y usa el namespace `plexus::landmarks`, abierto con macros.

### Corregido

- **landmarks**: `closeInputStream()` cerraba el stream equivocado en el motor GPU (la entrada del grafo es `<input>_frame`).

## Anteriores a la numeración

Cambios hechos sobre la base `v0.10.32`, antes de pasar a `v1.1.1` y de numerar el fork.

### Añadido

- **hand_tracking**: biblioteca compartida `libhand_tracker` con `HandTracker`, que expone el control explícito del grafo, el diagnóstico de errores y la clasificación de lateralidad (`handedness`).
- **hand_tracking**: `BodyTracker` para seguimiento corporal y dependencia `pose_tracking_cpu_deps`.
- **build**: compilación en Windows y OpenCV 4 de `x86_64` en Linux.

### Cambiado

- **hand_tracking**: ejecución en CPU; `HandTracker` sigue la convención PascalCase y `Impl` se separa en su propia cabecera.

### Eliminado

- **build**: la dependencia de `opencv_highgui`, que enlaza Qt5 y chocaba con Qt6 en el mismo proceso.
