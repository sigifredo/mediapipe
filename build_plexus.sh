#!/usr/bin/env bash
# Construye las bibliotecas de landmarks de Plexus y las deja en dist/:
#   libplexus_landmarks.so      fachada (sin dependencia de MediaPipe ni EGL)
#   libplexus_landmarks_cpu.so  motor CPU
#   libplexus_landmarks_gpu.so  motor GPU (OpenGL ES vía EGL; solo Linux)
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
COMMON=(-c opt --copt=-fno-gnu-unique)

build_cpu() {
  bazel build "${COMMON[@]}" --define MEDIAPIPE_DISABLE_GPU=1 --platform_suffix=-cpu \
    //$TARGET_DIR:libplexus_landmarks.so //$TARGET_DIR:libplexus_landmarks_engine.so //$TARGET_DIR:models
  cp -f "bazel-out/k8-opt--cpu/bin/$TARGET_DIR/libplexus_landmarks.so" "$DIST/libplexus_landmarks.so"
  cp -f "bazel-out/k8-opt--cpu/bin/$TARGET_DIR/libplexus_landmarks_engine.so" "$DIST/libplexus_landmarks_cpu.so"
  # Modelos en dist/mediapipe/modules/...: el motor los busca junto a su biblioteca.
  (cd bazel-out/k8-opt--cpu/bin && find mediapipe/modules -name '*.tflite' -exec cp -f --parents -L {} "$ROOT/$DIST/" \;)
  # Recursos de texto que son fuentes del repositorio y no artefactos de Bazel.
  cp -f --parents mediapipe/modules/hand_landmark/handedness.txt "$DIST/"
}

build_gpu() {
  bazel build "${COMMON[@]}" --copt=-DMESA_EGL_NO_X11_HEADERS --copt=-DEGL_NO_X11 --platform_suffix=-gpu \
    //$TARGET_DIR:libplexus_landmarks_engine.so
  cp -f "bazel-out/k8-opt--gpu/bin/$TARGET_DIR/libplexus_landmarks_engine.so" "$DIST/libplexus_landmarks_gpu.so"
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
