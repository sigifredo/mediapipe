# Parches de Plexus sobre MediaPipe

Rama `plexus`: MediaPipe upstream más los cambios listados aquí. Base actual: `v1.1.1`.

Todo parche sobre código de upstream lleva la marca `plexus-patch:` en un comentario. Para listarlos:

```
git grep -n 'plexus-patch:'
```

El código propio (sin equivalente upstream) está en `mediapipe/examples/desktop/hand_tracking/` y no genera conflictos.

## Parches sobre archivos de upstream

| Archivo | Cambio | Motivo |
|---|---|---|
| `.bazelrc` | Flags `/Zc:preprocessor`, `/wd5105`, `/std:c11`, `/experimental:c11atomics` para `windows` | Compilar con MSVC (preprocesador conforme, C11 atomics) |
| `mediapipe/framework/api3/calculator_context.h` | Se elimina `int&... DoNotSpecify` en `VisitPacket*OrDie` | Compatibilidad MSVC |
| `mediapipe/framework/port/opencv_video_inc.h` | `&& !defined(_WIN32)` en el include de `optflow` | Compatibilidad Windows (optflow no se compila allí) |
| `third_party/opencv_linux.BUILD` | Descomentadas las rutas OpenCV 4 x86_64 y `highgui` desactivado | OpenCV del sistema (Debian) |
| `third_party/opencv_windows.BUILD` | `OPENCV_VERSION` 4120 y `vc16` | OpenCV 4.12.0 en `C:\opencv\build` |

Retirados por absorción upstream: `mediapipe/gpu/gpu_service.cc` (`ABSL_CONST_INIT`, presente en v1.1.1).

## Procedimiento de actualización

1. `git fetch origin --tags` y elegir el último tag `vX.Y.Z` (las ramas `release` y `master` no sirven: la primera está abandonada y la segunda es desarrollo).
2. Rama de trabajo: `git checkout -b plexus-vX.Y.Z plexus && git merge --no-commit --no-ff vX.Y.Z`.
3. Resolver conflictos y revisar cada fila de la tabla: si upstream ya incorpora el cambio, retirar el parche y moverlo a la lista de absorbidos.
4. Leer `git diff vANTERIOR vX.Y.Z -- .bazelrc WORKSPACE MODULE.bazel .bazelversion`: es donde upstream mueve el build.
5. Instalar el Bazel de `.bazelversion` en `~/.bazel/bin` si falta.
6. `bazel build -c opt --define MEDIAPIPE_DISABLE_GPU=1 //mediapipe/examples/desktop/hand_tracking:libhand_tracker.so` y revisar el log (no solo el código de salida).
7. Probar en Windows y en runtime con el proyecto consumidor antes de mover `plexus` por fast-forward.

## Historial

| Versión | Fecha | Verificado |
|---|---|---|
| v1.1.1 | 2026-10 | Compila en Linux (CPU). Pendiente: runtime, Windows, GPU |
