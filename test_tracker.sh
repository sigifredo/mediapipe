#!/usr/bin/env bash
# Pruebas de selección de backend de libhand_tracker. Requiere ./build_plexus.sh all.
# Ejecutar desde la raíz del repositorio (los modelos .tflite se resuelven desde ahí).
set -uo pipefail

cd "$(dirname "$0")"

SRC=mediapipe/examples/desktop/hand_tracking
GRAPHS=$SRC/graphs
OUT=$(mktemp -d)
FAILED=0

g++ -std=c++17 -I"$SRC" -o "$OUT/tracker_smoke" "$SRC/TrackerSmoke.cpp" \
  -Ldist -lhand_tracker -Wl,-rpath,"$PWD/dist" || exit 1

# run <nombre> <esperado: backend=... o FAIL> <comando...>
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

S=$OUT/tracker_smoke
HAND="$GRAPHS/hand_tracking_{backend}.pbtxt"
BODY="$GRAPHS/pose_tracking_{backend}.pbtxt"

run "mano auto con GPU"            "backend=gpu" "$S" hand "$HAND" auto
run "cuerpo auto con GPU"          "backend=gpu" "$S" body "$BODY" auto
run "mano forzada a cpu"           "backend=cpu" "$S" hand "$HAND" cpu
run "cuerpo forzado a cpu"         "backend=cpu" "$S" body "$BODY" cpu
run "env fuerza cpu sobre auto"    "backend=cpu" env PLEXUS_TRACKER_BACKEND=cpu "$S" hand "$HAND" auto
run "env fuerza gpu sobre cpu"     "backend=gpu" env PLEXUS_TRACKER_BACKEND=gpu "$S" hand "$HAND" cpu
run "auto sin marcador usa cpu"    "backend=cpu" "$S" hand "$GRAPHS/hand_tracking_cpu.pbtxt" auto

# Sin biblioteca gpu en el directorio: auto cae a cpu, gpu explícito falla.
mkdir -p "$OUT/solo_cpu"
cp dist/libhand_tracker.so dist/libhand_tracker_cpu.so "$OUT/solo_cpu/"
ln -s "$PWD/dist/mediapipe" "$OUT/solo_cpu/mediapipe"
run "auto sin libhand_tracker_gpu" "backend=cpu" env PLEXUS_TRACKER_LIBDIR="$OUT/solo_cpu" "$S" hand "$HAND" auto
run "gpu explícito sin biblioteca" FAIL           env PLEXUS_TRACKER_LIBDIR="$OUT/solo_cpu" "$S" hand "$HAND" gpu

# libEGL ausente de verdad: copia del backend gpu cuyo DT_NEEDED apunta a una biblioteca inexistente.
if command -v patchelf >/dev/null; then
  mkdir -p "$OUT/sin_egl"
  cp dist/libhand_tracker.so dist/libhand_tracker_cpu.so dist/libhand_tracker_gpu.so "$OUT/sin_egl/"
  ln -s "$PWD/dist/mediapipe" "$OUT/sin_egl/mediapipe"
  patchelf --replace-needed libEGL.so.1 libEGL_inexistente.so.1 "$OUT/sin_egl/libhand_tracker_gpu.so"
  run "auto sin libEGL instalada"  "backend=cpu" env PLEXUS_TRACKER_LIBDIR="$OUT/sin_egl" "$S" hand "$HAND" auto
  run "gpu explícito sin libEGL"   FAIL           env PLEXUS_TRACKER_LIBDIR="$OUT/sin_egl" "$S" hand "$HAND" gpu
else
  echo "omitido: patchelf no está instalado (prueba sin libEGL)"
fi

# EGL inutilizable (sin vendors): GpuResources::Create falla; auto cae a cpu, gpu explícito falla.
# Fallo del backend gpu tras 10 frames: auto debe reiniciar en cpu y aceptar los 30 frames.
run "fallback a mitad (mano)"      "backend=cpu frames_ok=30/30" env PLEXUS_TRACKER_FAULT_AFTER_FRAMES=10 "$S" hand "$HAND" auto
run "fallback a mitad (cuerpo)"    "backend=cpu frames_ok=30/30" env PLEXUS_TRACKER_FAULT_AFTER_FRAMES=10 "$S" body "$BODY" auto
# Con gpu explícito no hay fallback: los frames posteriores al fallo se rechazan.
run "gpu explícito no hace fallback" FAIL env PLEXUS_TRACKER_FAULT_AFTER_FRAMES=10 "$S" hand "$HAND" gpu

run "auto con EGL roto"            "backend=cpu" env __EGL_VENDOR_LIBRARY_FILENAMES=/nonexistent.json "$S" hand "$HAND" auto
run "gpu explícito con EGL roto"   FAIL           env __EGL_VENDOR_LIBRARY_FILENAMES=/nonexistent.json "$S" hand "$HAND" gpu

rm -rf "$OUT"
exit $FAILED
