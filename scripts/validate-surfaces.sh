#!/usr/bin/env bash
# Compare the same actual runtime scenes with original detail disabled/enabled.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/out/surface-build}"
out="${2:-$root/out/surface-validation}"
cmake_bin="${CMAKE:-cmake}"
"$cmake_bin" -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DFURY_ENABLE_DX12=OFF
"$cmake_bin" --build "$build" --parallel "${FURY_BUILD_WORKERS:-4}"
ctest --test-dir "$build" --output-on-failure
build="$(cd "$build" && pwd)"
mkdir -p "$out"; out="$(cd "$out" && pwd)"
cd "$root"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FURY_AUDIO_BACKEND=cpu
export FURY_CPU_THREADS="${FURY_CPU_THREADS:-4}"
for detail in 0 1; do
  export FURY_SURFACE_DETAIL="$detail"
  "$build/apps/renderlab/fury_renderlab" --backend cpu-ray --mode ray --surface-gallery \
    --width 960 --height 540 --spp 2 --frames 8 --warmup 1 --hidden \
    --capture "$out/gallery-$detail.ppm" --report "$out/gallery-$detail.json"
  for backend in software cpu-ray; do
    "$build/apps/renderlab/fury_renderlab" --backend "$backend" --mode ray \
      --width 640 --height 360 --spp 1 --frames 35 --warmup 5 --hidden \
      --capture "$out/coastal-$backend-$detail.ppm" --report "$out/coastal-$backend-$detail.json"
  done
  for view in storefront storefront-close bench; do
    FURY_TRACE_MODE=ray "$build/apps/vaultline/vaultline" --cpu-ray --photo --view "$view" \
      --no-hud --width 960 --height 540 --spp 2 --frames 8 --capture "$out/$view-$detail.ppm"
  done
done
"$build/tests/fury_surface_detail_tests" --dump-dir "$out/maps"
echo "Runtime surface comparisons and actual map exports written to $out"
