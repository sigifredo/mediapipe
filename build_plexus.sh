#!/usr/bin/env bash
# Construye las bibliotecas de tracker de Plexus y las deja en dist/:
#   libhand_tracker.so      fachada (sin dependencia de MediaPipe ni EGL)
#   libhand_tracker_cpu.so  backend CPU
#   libhand_tracker_gpu.so  backend GPU (OpenGL ES vía EGL; solo Linux)
# Uso: ./build_plexus.sh [cpu|gpu|all]   (por defecto all)
set -euo pipefail

cd "$(dirname "$0")"

TARGET_DIR=mediapipe/examples/desktop/hand_tracking
ROOT=$PWD
DIST=dist
WHAT="${1:-all}"

# -fno-gnu-unique: sin esto los singletons de MediaPipe (registros de calculators) son
# globales al proceso aunque se use RTLD_LOCAL, y no pueden coexistir dos backends
# (p. ej. gpu que falla al iniciar y luego cpu).
COMMON=(-c opt --copt=-fno-gnu-unique)

build_cpu() {
  bazel build "${COMMON[@]}" --define MEDIAPIPE_DISABLE_GPU=1 --platform_suffix=-cpu \
    //$TARGET_DIR:libhand_tracker.so //$TARGET_DIR:libhand_tracker_backend.so //$TARGET_DIR:models
  cp -f "bazel-out/k8-opt--cpu/bin/$TARGET_DIR/libhand_tracker.so" "$DIST/libhand_tracker.so"
  cp -f "bazel-out/k8-opt--cpu/bin/$TARGET_DIR/libhand_tracker_backend.so" "$DIST/libhand_tracker_cpu.so"
  # Modelos en dist/mediapipe/modules/...: el backend los busca junto a su biblioteca.
  (cd bazel-out/k8-opt--cpu/bin && find mediapipe/modules -name '*.tflite' -exec cp -f --parents -L {} "$ROOT/$DIST/" \;)
  # Recursos de texto que son fuentes del repositorio y no artefactos de Bazel.
  cp -f --parents mediapipe/modules/hand_landmark/handedness.txt "$DIST/"
}

build_gpu() {
  bazel build "${COMMON[@]}" --copt=-DMESA_EGL_NO_X11_HEADERS --copt=-DEGL_NO_X11 --platform_suffix=-gpu \
    //$TARGET_DIR:libhand_tracker_backend.so
  cp -f "bazel-out/k8-opt--gpu/bin/$TARGET_DIR/libhand_tracker_backend.so" "$DIST/libhand_tracker_gpu.so"
}

mkdir -p "$DIST"

case "$WHAT" in
  cpu) build_cpu ;;
  gpu) build_gpu ;;
  all) build_cpu; build_gpu ;;
  *) echo "uso: $0 [cpu|gpu|all]" >&2; exit 2 ;;
esac

chmod u+w "$DIST"/*.so
ls -la "$DIST"
