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

## Bibliotecas de tracker (CPU y GPU)

`./build_plexus.sh [cpu|gpu|all]` genera en `dist/` (ignorado por git):

| Archivo | Contenido | Dependencias de sistema |
|---|---|---|
| `libhand_tracker.so` | Fachada: `HandTracker`, `BodyTracker`. Elige y carga el backend con dlopen | libstdc++ y libc |
| `libhand_tracker_cpu.so` | Backend MediaPipe sin GPU | OpenCV 4.10 |
| `libhand_tracker_gpu.so` | Backend MediaPipe con OpenGL ES (solo Linux) | OpenCV 4.10, `libEGL.so.1`, `libGLESv2.so.2` |
| `mediapipe/modules/...` | Modelos y recursos que lee el backend | |

El consumidor enlaza solo `libhand_tracker.so`. Los backends y `mediapipe/` deben estar en su mismo directorio, o en `PLEXUS_TRACKER_LIBDIR` (bibliotecas) y `PLEXUS_TRACKER_RESOURCE_DIR` (modelos).

### Selección de backend

- `MediaPipeTrackerConfig::backend`: `Auto` (por defecto), `Cpu` o `Gpu`. La variable `PLEXUS_TRACKER_BACKEND` (`auto`, `cpu`, `gpu`) tiene prioridad.
- `graphPath` puede contener `{backend}`, que se sustituye por `cpu` o `gpu` (`graphs/hand_tracking_{backend}.pbtxt`). Sin marcador, `Auto` usa CPU.
- `Auto` prueba la GPU y cae a CPU si falta el backend o `libEGL`/`libGLESv2`, si no hay contexto GLES, o si el grafo no inicia. Si la GPU falla durante la ejecución, reinicia el grafo en CPU (no llamar a `processFrame` desde el callback).
- `Gpu` explícito no hace fallback: falla si no puede usar la GPU.
- Los grafos de GPU son los estándar de MediaPipe (reciben GpuBuffer). El backend renombra la entrada a `<input>_frame` e inserta `ImageFrameToGpuBufferCalculator`. Ejemplos en `mediapipe/examples/desktop/hand_tracking/graphs/`.
- Windows: solo CPU (MediaPipe desactiva la GPU allí). La carga por `LoadLibrary` está escrita y sin probar.

### Restricciones de build

- Los backends se compilan con `--copt=-fno-gnu-unique`. Sin ello los registros de calculators son globales al proceso y el fallback (cargar CPU tras GPU) aborta por registro duplicado.
- CPU y GPU usan `--platform_suffix` distinto (`-cpu`, `-gpu`) para conservar ambas cachés de Bazel. Los directorios de salida son `bazel-out/k8-opt--cpu` y `k8-opt--gpu`.
- Las bibliotecas de backend nunca se descargan (`dlclose`).

### Pruebas

`./test_tracker.sh` (tras `build_plexus.sh all`) ejecuta 16 escenarios de selección y fallback sobre frames sintéticos. `tracker_bench` mide latencia por frame:

```
bazel build -c opt --copt=-DMESA_EGL_NO_X11_HEADERS --copt=-DEGL_NO_X11 //mediapipe/examples/desktop/hand_tracking:tracker_bench
bazel-bin/mediapipe/examples/desktop/hand_tracking/tracker_bench --backend=gpu --video=<video>
```

Medición de referencia (GTX 1660 SUPER, detección de palma sobre `test_video.mp4`, sin manos): CPU 20,95 ms por frame, GPU 6,18 ms; primer frame en GPU 1336 ms por compilación de shaders. Sin medir con manos o cuerpo reales.

Limitación de las pruebas: los frames son sintéticos y los resultados de landmarks son siempre 0. La salida real de landmarks por la ruta GPU no está verificada.

## Procedimiento de actualización

1. `git fetch origin --tags` y elegir el último tag `vX.Y.Z` (las ramas `release` y `master` no sirven: la primera está abandonada y la segunda es desarrollo).
2. Rama de trabajo: `git checkout -b plexus-vX.Y.Z plexus && git merge --no-commit --no-ff vX.Y.Z`.
3. Resolver conflictos y revisar cada fila de la tabla: si upstream ya incorpora el cambio, retirar el parche y moverlo a la lista de absorbidos.
4. Leer `git diff vANTERIOR vX.Y.Z -- .bazelrc WORKSPACE MODULE.bazel .bazelversion`: es donde upstream mueve el build.
5. Instalar el Bazel de `.bazelversion` en `~/.bazel/bin` si falta.
6. `./build_plexus.sh all`, revisar el log (no solo el código de salida) y ejecutar `./test_tracker.sh`.
7. Probar en Windows y en runtime con el proyecto consumidor antes de mover `plexus` por fast-forward.

## Historial

| Versión | Fecha | Verificado |
|---|---|---|
| v1.1.1 | 2026-10 | Compila y arranca en Linux, CPU y GPU, con selección y fallback probados. Pendiente: landmarks reales por GPU, Windows |
