# Parches de Plexus sobre MediaPipe

Rama `plexus`: MediaPipe upstream más los cambios listados aquí. Base actual: `v1.1.1`.

## Versionado

La versión del fork es `<base de upstream>+plexus.<N>`, por ejemplo `1.1.1+plexus.1`. Es SemVer válido: lo que va tras `+` es metadato de build y no cambia la precedencia (con `-N` sería un pre-release anterior a `1.1.1`).

- La base es `MEDIAPIPE_FULL_VERSION` de `mediapipe/version.bzl`. `N` es la revisión propia: sube en 1 con cada cambio del fork que se vendoriza en Plexus y vuelve a 1 al cambiar de base.
- Se edita a mano en `mediapipe/plexus/VERSION`, en el mismo commit que el cambio. `build_plexus.{sh,ps1}` fallan si la base de ese archivo no coincide con `version.bzl`.
- Cada versión vendorizada lleva el tag `v<versión>` (por ejemplo `v1.1.1+plexus.1`) sobre el commit exacto. `git describe --match 'v*+plexus.*'` dice dónde está el árbol respecto al último tag propio; los tags `vX.Y.Z` de upstream no sirven para eso (hay que traerlos con `git fetch upstream --tags`).
- El build escribe `mediapipe/plexus/landmarks/VersionInfo.generated.hpp` (ignorado por git) con la versión y el commit, y `dist/VERSION` con `version`, `upstream`, `revision` (commit, con `-dirty` si el árbol tenía cambios), `date` y `platform`. Solo se recompila `LandmarksVersion.cpp`.
- La API es `plexus::landmarks::version()`, `revision()` y `describe()` (`<LandmarksVersion.hpp>`). La fachada y el motor llevan la misma, y al cargar el motor la fachada rechaza uno con otro `describe()`: mezclar builds comparte `IEngine` solo en apariencia. El motor la expone con `plexusLandmarksEngineVersion`.
- Cada cambio propio se anota en [`mediapipe/plexus/CHANGELOG.md`](mediapipe/plexus/CHANGELOG.md), en el mismo commit que sube `mediapipe/plexus/VERSION`.
- Plexus guarda `dist/VERSION` como `vendor/mediapipe/lib/<plataforma>/VERSION`. Para saber si lo vendorizado está al día: comparar su `revision` con `git rev-parse --short=9 HEAD` del fork.

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
| `.bazelrc` (`build:windows`) | `--define=protobuf_allow_msvc=true` y `--override_repository` de `rules_swift` a `third_party/windows_swift_stub` | protobuf 6.31 se niega a compilar con MSVC sin el define; el toolchain de `rules_swift` exige `swiftc.exe` en Windows aunque solo se use en iOS |
| `MODULE.bazel` (`archive_override` de protobuf), `third_party/protobuf_msvc_untyped_message.diff` | Parche que usa `std::map` en `UntypedMessage::fields_` solo con `_MSC_VER` | MSVC 14.44 no compila `flat_hash_map<int, variant<..., UntypedMessage>>` con la clase incompleta. En el resto de compiladores el código es el mismo |
| `mediapipe/framework/legacy_calculator_support.h` | `ABSL_CONST_INIT` se omite con MSVC (no clang-cl) | Se expande a `constinit` y no coincide con la definición del `.cc` |
| `mediapipe/framework/port/advanced_proto_inc.h` | `gzip_stream.h` solo si no es `_WIN32` | protobuf en Windows se compila sin zlib; MediaPipe no usa gzip |

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
| `LandmarksVersion.hpp`, `LandmarksVersion.cpp` | `version()`, `revision()` y `describe()`; los valores vienen de `VersionInfo.generated.hpp` (ver Versionado) |
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
- Windows: solo CPU (MediaPipe desactiva la GPU allí). La fachada carga el motor con `LoadLibraryEx` y `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR`, para que OpenCV se resuelva junto al motor y no junto al ejecutable del consumidor.

### Restricciones de build

- Los motores se compilan con `--copt=-fno-gnu-unique`. Sin ello los registros de calculators son globales al proceso y el fallback (cargar CPU tras GPU) aborta por registro duplicado.
- CPU y GPU usan `--platform_suffix` distinto (`-cpu`, `-gpu`) para conservar ambas cachés de Bazel. Los directorios de salida son `bazel-out/k8-opt--cpu` y `k8-opt--gpu`.
- Las bibliotecas de motor nunca se descargan (`dlclose`).

### macOS (Apple Silicon)

- `./build_plexus.sh` detecta Darwin: usa `--config=macos`, genera `libplexus_landmarks.dylib` y `libplexus_landmarks_cpu.dylib` (sin motor GPU, no hay EGL) y corrige su install name con `install_name_tool`. Requiere `brew install opencv@4`.
- El motor exporta solo `plexusLandmarksCreateEngine` y `plexusLandmarksDestroyEngine` (`engine_exports_macos.lds`). Sin eso los símbolos débiles de protobuf y abseil del motor se fusionan con los de Homebrew, que llegan por OpenCV, y el proceso falla con segfault al cargar el motor.
- `./test_landmarks.sh` en macOS (usar `CXX=clang++` si no hay `g++` real) ejecuta solo los escenarios de CPU y de ausencia de la biblioteca GPU.

### Windows (MSVC, release)

- `.\build_plexus.ps1` compila con `-c opt --config=windows` y genera en `dist\`: `plexus_landmarks.dll` y `plexus_landmarks.lib` (fachada), `plexus_landmarks_cpu.dll` (motor), `opencv_world4120.dll` y `mediapipe\modules\...`. El consumidor enlaza `plexus_landmarks.lib` y despliega todo `dist\`.
- Requiere Visual Studio 2022 con C++ (probado con MSVC 14.44), OpenCV 4.12 en `C:\opencv\build` y el Bazel de `.bazelversion` en `%USERPROFILE%\.bazel\bin\bazel-7.7.0.exe`.
- Sin JDK en el sistema, `rules_java` 7.10 genera una etiqueta inexistente (`@rules_java//tools/jdk`) y el análisis falla. El script usa el JDK embebido de Bazel como `JAVA_HOME`.
- Bazel nombra los targets `libplexus_landmarks*.so` aunque sean DLL, y el `.lib` que genera apunta a ese nombre. El script regenera el `.lib` desde los exports de la DLL.
- El motor se compila con `PLEXUS_LANDMARKS_BUILDING_DLL` en Windows: los tipos de resultado son `PLEXUS_LANDMARKS_API` y sin eso el motor los importaría de la fachada.
- `.\test_landmarks.ps1` ejecuta los escenarios de CPU y de ausencia de la biblioteca GPU.
- La primera compilación completa tarda unos 20 minutos.

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
6. Poner `mediapipe/plexus/VERSION` en `X.Y.Z+plexus.1`, abrir su sección en `mediapipe/plexus/CHANGELOG.md` y confirmarlo.
7. `./build_plexus.sh all`, revisar el log (no solo el código de salida) y ejecutar `./test_landmarks.sh`.
8. Probar en Windows y en runtime con el proyecto consumidor antes de mover `plexus` por fast-forward.
9. Etiquetar el commit (`git tag v<versión>`) y vendorizar en Plexus (`docs/build-mediapipe.md`).

## Estado de verificación

Versión `1.1.1+plexus.1`. Compila y arranca en Linux, CPU y GPU, con selección y fallback probados. En macOS (arm64) compila y pasa los escenarios de CPU con frames sintéticos. En Windows (MSVC 14.44, release) compila y pasa los escenarios de CPU con frames sintéticos. Pendiente: probar con cámara en vivo y con una imagen real en Windows.
