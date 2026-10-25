#!/usr/bin/env bash
# Pruebas de selección de dispositivo de libplexus_landmarks. Requiere ./build_plexus.sh all.
# Ejecutar desde la raíz del repositorio (los modelos .tflite se resuelven desde ahí).
set -uo pipefail

cd "$(dirname "$0")"

if [[ $(uname -s) == Darwin ]]; then EXT=dylib; HAS_GPU=0; else EXT=so; HAS_GPU=1; fi

SRC=mediapipe/plexus/landmarks
GRAPHS=$SRC/graphs
OUT=$(mktemp -d)
FAILED=0

${CXX:-g++} -std=c++17 -I"$SRC" -o "$OUT/landmarks_smoke" "$SRC/LandmarksSmoke.cpp" \
  -Ldist -lplexus_landmarks -Wl,-rpath,"$PWD/dist" || exit 1

# run <nombre> <esperado: device=... o FAIL> <comando...>
run() {
  local name=$1 expect=$2
  shift 2
  local out code
  out=$("$@" 2>"$OUT/stderr"); code=$?
  if [[ $expect == FAIL ]]; then
    [[ $code -ne 0 ]] && echo "ok    $name (falla como se espera)" || { echo "FALLA $name: debía fallar"; FAILED=1; }
  elif [[ $code -eq 0 && $out == *"$expect"* ]]; then
    echo "ok    $name -> $out"
  else
    echo "FALLA $name: salida='$out' código=$code"; tail -3 "$OUT/stderr"; FAILED=1
  fi
}

S=$OUT/landmarks_smoke
HAND="$GRAPHS/hand_{device}.pbtxt"
BODY="$GRAPHS/body_{device}.pbtxt"

(( HAS_GPU )) && run "mano auto con GPU"            "device=gpu" "$S" hand "$HAND" auto
(( HAS_GPU )) && run "cuerpo auto con GPU"          "device=gpu" "$S" body "$BODY" auto
run "mano forzada a cpu"           "device=cpu" "$S" hand "$HAND" cpu
run "cuerpo forzado a cpu"         "device=cpu" "$S" body "$BODY" cpu
run "env fuerza cpu sobre auto"    "device=cpu" env PLEXUS_LANDMARKS_DEVICE=cpu "$S" hand "$HAND" auto
(( HAS_GPU )) && run "env fuerza gpu sobre cpu"     "device=gpu" env PLEXUS_LANDMARKS_DEVICE=gpu "$S" hand "$HAND" cpu
run "auto sin marcador usa cpu"    "device=cpu" "$S" hand "$GRAPHS/hand_cpu.pbtxt" auto

# El stream de entrada del grafo gpu se renombra (<input>_frame): closeInputStream debe cerrar ese, no el original.
(( HAS_GPU )) && run "cierre de entrada en gpu (mano)"   "close_ok=1" "$S" hand "$HAND" gpu
(( HAS_GPU )) && run "cierre de entrada en gpu (cuerpo)" "close_ok=1" "$S" body "$BODY" gpu
run "cierre de entrada en cpu (mano)"   "close_ok=1" "$S" hand "$HAND" cpu

# Sin biblioteca gpu en el directorio: auto cae a cpu, gpu explícito falla.
mkdir -p "$OUT/solo_cpu"
cp dist/libplexus_landmarks.$EXT dist/libplexus_landmarks_cpu.$EXT "$OUT/solo_cpu/"
ln -s "$PWD/dist/mediapipe" "$OUT/solo_cpu/mediapipe"
run "auto sin libplexus_landmarks_gpu" "device=cpu" env PLEXUS_LANDMARKS_LIBDIR="$OUT/solo_cpu" "$S" hand "$HAND" auto
run "gpu explícito sin biblioteca" FAIL           env PLEXUS_LANDMARKS_LIBDIR="$OUT/solo_cpu" "$S" hand "$HAND" gpu

# libEGL ausente de verdad: copia del motor gpu cuyo DT_NEEDED apunta a una biblioteca inexistente.
if (( HAS_GPU )) && command -v patchelf >/dev/null; then
  mkdir -p "$OUT/sin_egl"
  cp dist/libplexus_landmarks.so dist/libplexus_landmarks_cpu.so dist/libplexus_landmarks_gpu.so "$OUT/sin_egl/"
  ln -s "$PWD/dist/mediapipe" "$OUT/sin_egl/mediapipe"
  patchelf --replace-needed libEGL.so.1 libEGL_inexistente.so.1 "$OUT/sin_egl/libplexus_landmarks_gpu.so"
  run "auto sin libEGL instalada"  "device=cpu" env PLEXUS_LANDMARKS_LIBDIR="$OUT/sin_egl" "$S" hand "$HAND" auto
  run "gpu explícito sin libEGL"   FAIL           env PLEXUS_LANDMARKS_LIBDIR="$OUT/sin_egl" "$S" hand "$HAND" gpu
elif (( HAS_GPU )); then
  echo "omitido: patchelf no está instalado (prueba sin libEGL)"
fi

# EGL inutilizable (sin vendors): GpuResources::Create falla; auto cae a cpu, gpu explícito falla.
# Fallo del motor gpu tras 10 frames: auto debe reiniciar en cpu y aceptar los 30 frames.
(( HAS_GPU )) && run "fallback a mitad (mano)"      "device=cpu frames_ok=30/30" env PLEXUS_LANDMARKS_FAULT_AFTER_FRAMES=10 "$S" hand "$HAND" auto
(( HAS_GPU )) && run "fallback a mitad (cuerpo)"    "device=cpu frames_ok=30/30" env PLEXUS_LANDMARKS_FAULT_AFTER_FRAMES=10 "$S" body "$BODY" auto
# Con gpu explícito no hay fallback: los frames posteriores al fallo se rechazan.
(( HAS_GPU )) && run "gpu explícito no hace fallback" FAIL env PLEXUS_LANDMARKS_FAULT_AFTER_FRAMES=10 "$S" hand "$HAND" gpu

(( HAS_GPU )) && run "auto con EGL roto"            "device=cpu" env __EGL_VENDOR_LIBRARY_FILENAMES=/nonexistent.json "$S" hand "$HAND" auto
(( HAS_GPU )) && run "gpu explícito con EGL roto"   FAIL           env __EGL_VENDOR_LIBRARY_FILENAMES=/nonexistent.json "$S" hand "$HAND" gpu

rm -rf "$OUT"
exit $FAILED
