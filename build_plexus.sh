#!/usr/bin/env bash
# Construye las bibliotecas de landmarks de Plexus y las deja en dist/:
#   libplexus_landmarks.so      fachada (sin dependencia de MediaPipe ni EGL)
#   libplexus_landmarks_cpu.so  motor CPU
#   libplexus_landmarks_gpu.so  motor GPU (OpenGL ES vía EGL; solo Linux)
# En macOS las bibliotecas son .dylib y solo se construye el motor CPU.
# Uso: ./build_plexus.sh [cpu|gpu|all]   (por defecto all)
set -euo pipefail

cd "$(dirname "$0")"

TARGET_DIR=mediapipe/plexus/landmarks
ROOT=$PWD
DIST=dist
WHAT="${1:-all}"

# -fno-gnu-unique: sin esto los singletons de MediaPipe (registros de calculators) son
# globales al proceso aunque se use RTLD_LOCAL, y no pueden coexistir dos motores
# (p. ej. gpu que falla al iniciar y luego cpu).
COMMON=(-c opt)

if [[ $(uname -s) == Darwin ]]; then
  EXT=dylib
  CPU_OUT=bazel-out/darwin_$(uname -m)-opt--cpu
  COMMON+=(--config=macos)
else
  EXT=so
  CPU_OUT=bazel-out/k8-opt--cpu
  COMMON+=(--copt=-fno-gnu-unique)
fi

# Equivalente portable de cp --parents (BSD cp no lo tiene).
copy_parents() {
  mkdir -p "$2/$(dirname "$1")"
  cp -f -L "$1" "$2/$1"
}

build_cpu() {
  bazel build "${COMMON[@]}" --define MEDIAPIPE_DISABLE_GPU=1 --platform_suffix=-cpu \
    //$TARGET_DIR:libplexus_landmarks.so //$TARGET_DIR:libplexus_landmarks_engine.so //$TARGET_DIR:models
  cp -f "$CPU_OUT/bin/$TARGET_DIR/libplexus_landmarks.so" "$DIST/libplexus_landmarks.$EXT"
  cp -f "$CPU_OUT/bin/$TARGET_DIR/libplexus_landmarks_engine.so" "$DIST/libplexus_landmarks_cpu.$EXT"
  # Modelos en dist/mediapipe/modules/...: el motor los busca junto a su biblioteca.
  (cd "$CPU_OUT/bin" && find mediapipe/modules -name '*.tflite' | while read -r f; do copy_parents "$f" "$ROOT/$DIST"; done)
  # Recursos de texto que son fuentes del repositorio y no artefactos de Bazel.
  copy_parents mediapipe/modules/hand_landmark/handedness.txt "$DIST"
}

build_gpu() {
  bazel build "${COMMON[@]}" --copt=-DMESA_EGL_NO_X11_HEADERS --copt=-DEGL_NO_X11 --platform_suffix=-gpu \
    //$TARGET_DIR:libplexus_landmarks_engine.so
  cp -f "bazel-out/k8-opt--gpu/bin/$TARGET_DIR/libplexus_landmarks_engine.so" "$DIST/libplexus_landmarks_gpu.so"
}

mkdir -p "$DIST"

case "$WHAT" in
  cpu) build_cpu ;;
  gpu) [[ $(uname -s) != Darwin ]] || { echo "GPU no disponible en macOS" >&2; exit 2; }; build_gpu ;;
  all) build_cpu; [[ $(uname -s) == Darwin ]] || build_gpu ;;
  *) echo "uso: $0 [cpu|gpu|all]" >&2; exit 2 ;;
esac

chmod u+w "$DIST"/*.$EXT

# Bazel deja el install name con extensión .so; dyld lo necesita igual al nombre real del archivo.
if [[ $EXT == dylib ]]; then
  for lib in "$DIST"/*.dylib; do
    install_name_tool -id "@rpath/$(basename "$lib")" "$lib"
    codesign --force --sign - "$lib" 2>/dev/null
  done
fi

ls -la "$DIST"
