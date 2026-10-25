# Parches de Plexus sobre MediaPipe

Rama `plexus`: MediaPipe upstream más los cambios listados aquí. Base actual: `v1.1.1`.

Todo parche sobre código de upstream lleva la marca `plexus-patch:` en un comentario. Para listarlos:

```
git grep -n 'plexus-patch:'
```

El código propio (sin equivalente upstream) está en `mediapipe/plexus/` y no genera conflictos: ningún archivo de upstream se modifica para alojarlo.

## Parches sobre archivos de upstream

| Archivo | Cambio | Motivo |
|---|---|---|
| `.bazelrc` | Flags `/Zc:preprocessor`, `/wd5105`, `/std:c11`, `/experimental:c11atomics` para `windows` | Compilar con MSVC (preprocesador conforme, C11 atomics) |
| `mediapipe/framework/api3/calculator_context.h` | Se elimina `int&... DoNotSpecify` en `VisitPacket*OrDie` | Compatibilidad MSVC |
| `mediapipe/framework/port/opencv_video_inc.h` | `&& !defined(_WIN32)` en el include de `optflow` | Compatibilidad Windows (optflow no se compila allí) |
| `third_party/opencv_linux.BUILD` | Descomentadas las rutas OpenCV 4 x86_64 y `highgui` desactivado | OpenCV del sistema (Debian) |
| `third_party/opencv_macos.BUILD`, `WORKSPACE` (`macos_opencv`) | `PREFIX` `opt/opencv@4`, cabeceras en `include/opencv4` y ruta `/opt/homebrew` | OpenCV 4 de Homebrew en Apple Silicon |
| `third_party/opencv_windows.BUILD` | `OPENCV_VERSION` 4120 y `vc16` | OpenCV 4.12.0 en `C:\opencv\build` |

## Biblioteca de landmarks (`mediapipe/plexus/landmarks/`)

Interfaz de Plexus hacia MediaPipe: entrega landmarks de manos (`HandTracker`) y de cuerpo (`BodyTracker`) a partir de frames RGB. Todo el API público vive en el namespace `plexus::landmarks`.

`./build_plexus.sh [cpu|gpu|all]` genera en `dist/` (ignorado por git):

| Archivo | Contenido | Dependencias de sistema |
|---|---|---|
| `libplexus_landmarks.so` | Fachada: `LandmarkTracker`, `HandTracker`, `BodyTracker`. Elige y carga el motor con dlopen | libstdc++ y libc |
| `libplexus_landmarks_cpu.so` | Motor MediaPipe sin GPU | OpenCV 4.10 |
| `libplexus_landmarks_gpu.so` | Motor MediaPipe con OpenGL ES (solo Linux) | OpenCV 4.10, `libEGL.so.1`, `libGLESv2.so.2` |
| `mediapipe/modules/...` | Modelos y recursos que lee el motor | |

El consumidor enlaza solo `libplexus_landmarks.so`. Los motores y `mediapipe/` deben estar en su mismo directorio, o en `PLEXUS_LANDMARKS_LIBDIR` (bibliotecas) y `PLEXUS_LANDMARKS_RESOURCE_DIR` (modelos).

### Estructura y nombres

| Elemento | Rol |
|---|---|
| `Namespace.hpp` | Macros de apertura y cierre del namespace |
| `Config.hpp` | `ComputeDevice` (Auto, Cpu, Gpu) y `TrackerConfig` |
| `HandTypes.hpp`, `BodyTypes.hpp` | Resultados y landmarks de cada dominio |
| `LandmarkTracker` | Base de `HandTracker` y `BodyTracker`. Gestiona dispositivo, motor y fallback |
| `backend/EngineApi.hpp` | `IEngine`: contrato entre fachada y motores |
| `backend/MediaPipeEngine.cpp` | `MediaPipeEngine`: único `IEngine` existente, sobre `CalculatorGraph` |
| `EngineLoader.*` | `LoadedEngine`: carga un motor por dispositivo con dlopen/LoadLibrary |
| `graphs/{hand,body}_{cpu,gpu}.pbtxt` | Grafos de ejemplo |
| `LandmarksSmoke.cpp`, `LandmarksBench.cpp` | Prueba de humo (usa la fachada) y benchmark (usa MediaPipe directo) |

Variables de entorno: `PLEXUS_LANDMARKS_DEVICE`, `PLEXUS_LANDMARKS_LIBDIR`, `PLEXUS_LANDMARKS_RESOURCE_DIR` y, solo para pruebas, `PLEXUS_LANDMARKS_FAULT_AFTER_FRAMES`. Macros: `PLEXUS_LANDMARKS_API`, `PLEXUS_LANDMARKS_BUILDING_DLL`, `PLEXUS_LANDMARKS_ENGINE_EXPORT`. Símbolos de los motores: `plexusLandmarksCreateEngine`, `plexusLandmarksDestroyEngine`.

### Decisiones de diseño

- **Nombre por la salida.** La biblioteca se llama por lo que entrega (landmarks), no por una modalidad (`hand`) ni por la dependencia (`mediapipe`). Admite cara u otros dominios sin renombrar. El API público no menciona MediaPipe porque la fachada no depende de él; solo `MediaPipeEngine` lo nombra.
- **"Dispositivo" y "motor" son conceptos distintos.** El dispositivo (`ComputeDevice`) es dónde se infiere. El motor (`IEngine`) es la implementación que ejecuta los grafos. Hay un motor por dispositivo, cargado desde su propia biblioteca.
- **Estilo de archivo.** Todo `.hpp` y `.cpp` empieza con dos líneas en blanco.
- **Namespace por macros.** `Namespace.hpp` define `PLEXUS_LANDMARKS_NAMESPACE_BEGIN` y `PLEXUS_LANDMARKS_NAMESPACE_END`, que abren y cierran `plexus::landmarks` sin que el IDE indente el contenido. Los includes propios usan `<...>` relativos a `mediapipe/plexus/landmarks/` (`includes = ["."]` en el BUILD) y se agrupan en secciones `// own`, `// mediapipe`, `// std` y `// system`.
- **Un solo prefijo** (`plexus_landmarks` / `PLEXUS_LANDMARKS_*`) en bibliotecas, macros, variables de entorno y símbolos exportados, y namespace propio para evitar colisiones en el espacio global del consumidor.
- **Tipos por dominio.** Los tipos de manos y de cuerpo están en cabeceras separadas. `HandWorldLandmark` es específico de manos (sin visibilidad); `BodyLandmark` sirve para coordenadas normalizadas y de mundo del cuerpo.
- **Ubicación fuera de `examples/`.** `mediapipe/examples/desktop/hand_tracking/` se mantiene idéntico a upstream; el código propio vive en `mediapipe/plexus/`.

### Selección de dispositivo

- `TrackerConfig::device`: `Auto` (por defecto), `Cpu` o `Gpu`. La variable `PLEXUS_LANDMARKS_DEVICE` (`auto`, `cpu`, `gpu`) tiene prioridad.
- `graphPath` puede contener `{device}`, que se sustituye por `cpu` o `gpu` (`graphs/hand_{device}.pbtxt`). Sin marcador, `Auto` usa CPU.
- `Auto` prueba la GPU y cae a CPU si falta el motor o `libEGL`/`libGLESv2`, si no hay contexto GLES, o si el grafo no inicia. Si la GPU falla durante la ejecución, reinicia el grafo en CPU (no llamar a `processFrame` desde el callback).
- `Gpu` explícito no hace fallback: falla si no puede usar la GPU.
- Los grafos de GPU son los estándar de MediaPipe (reciben GpuBuffer). El motor renombra la entrada a `<input>_frame` e inserta `ImageFrameToGpuBufferCalculator`.
- Windows: solo CPU (MediaPipe desactiva la GPU allí). La carga por `LoadLibrary` está escrita y sin probar.

### Restricciones de build

- Los motores se compilan con `--copt=-fno-gnu-unique`. Sin ello los registros de calculators son globales al proceso y el fallback (cargar CPU tras GPU) aborta por registro duplicado.
- CPU y GPU usan `--platform_suffix` distinto (`-cpu`, `-gpu`) para conservar ambas cachés de Bazel. Los directorios de salida son `bazel-out/k8-opt--cpu` y `k8-opt--gpu`.
- Las bibliotecas de motor nunca se descargan (`dlclose`).

### macOS (Apple Silicon)

- `./build_plexus.sh` detecta Darwin: usa `--config=macos`, genera `libplexus_landmarks.dylib` y `libplexus_landmarks_cpu.dylib` (sin motor GPU, no hay EGL) y corrige su install name con `install_name_tool`. Requiere `brew install opencv@4`.
- El motor exporta solo `plexusLandmarksCreateEngine` y `plexusLandmarksDestroyEngine` (`engine_exports_macos.lds`). Sin eso los símbolos débiles de protobuf y abseil del motor se fusionan con los de Homebrew, que llegan por OpenCV, y el proceso falla con segfault al cargar el motor.
- `./test_landmarks.sh` en macOS (usar `CXX=clang++` si no hay `g++` real) ejecuta solo los escenarios de CPU y de ausencia de la biblioteca GPU.

### Pruebas

`./test_landmarks.sh` (tras `build_plexus.sh all`) ejecuta 16 escenarios de selección y fallback sobre frames sintéticos. `landmarks_bench` mide latencia por frame:

```
bazel build -c opt --copt=-DMESA_EGL_NO_X11_HEADERS --copt=-DEGL_NO_X11 //mediapipe/plexus/landmarks:landmarks_bench
bazel-bin/mediapipe/plexus/landmarks/landmarks_bench --device=gpu --video=<video>
```

Medición de referencia (GTX 1660 SUPER, detección de palma sobre `test_video.mp4`, sin manos): CPU 20,95 ms por frame, GPU 6,18 ms; primer frame en GPU 1336 ms por compilación de shaders. Sin medir con manos o cuerpo reales.

Limitación de las pruebas automáticas: los frames son sintéticos y no producen landmarks. Los casos `cierre de entrada en gpu/cpu` comprueban que `closeInputStream()` tenga éxito (en GPU el stream de entrada del grafo es `<input>_frame`, no `input_video`).

Verificación manual con una imagen real (una foto con personas enviada 6 veces, resolución 960 px de ancho): CPU y GPU entregan el mismo número de landmarks (2 manos con handedness `Right`; 33 de cuerpo más 33 de mundo) y coordenadas que difieren en milésimas (0,637 frente a 0,638 en la muñeca; 0,460 frente a 0,455 en la nariz). Una sola imagen; no se ha probado con cámara en vivo ni con movimiento. Las imágenes sin detección no emiten ningún callback en ninguno de los dos dispositivos.

Aviso conocido en GPU: `Tensors are designed for single writes` (`tensor.cc`) sale una vez por proceso mientras el detector de palma o de pose corre en frames consecutivos (es decir, sin nada que seguir). Viene de `TensorsToDetectionsCalculator::ProcessGPU`, que reescribe en cada frame dos `Tensor` miembros, todo en el único hilo del contexto GL. Es diseño de upstream y no afecta a los resultados; no se parchea.

## Procedimiento de actualización

1. `git fetch upstream --tags` (`upstream` es google-ai-edge/mediapipe; `origin` es el fork propio) y elegir el último tag `vX.Y.Z` (las ramas `release` y `master` no sirven: la primera está abandonada y la segunda es desarrollo).
2. Rama de trabajo: `git checkout -b plexus-vX.Y.Z plexus && git merge --no-commit --no-ff vX.Y.Z`.
3. Resolver conflictos y revisar cada fila de la tabla: si upstream ya incorpora el cambio, retirar el parche y moverlo a la lista de absorbidos.
4. Leer `git diff vANTERIOR vX.Y.Z -- .bazelrc WORKSPACE MODULE.bazel .bazelversion`: es donde upstream mueve el build.
5. Instalar el Bazel de `.bazelversion` en `~/.bazel/bin` si falta.
6. `./build_plexus.sh all`, revisar el log (no solo el código de salida) y ejecutar `./test_landmarks.sh`.
7. Probar en Windows y en runtime con el proyecto consumidor antes de mover `plexus` por fast-forward.

## Estado de verificación

Base `v1.1.1`. Compila y arranca en Linux, CPU y GPU, con selección y fallback probados. En macOS (arm64) compila y pasa los escenarios de CPU con frames sintéticos. Pendiente: probar con cámara en vivo, Windows.
