#!/usr/bin/env bash
# Reproducible CPU-only validation, without a GPU, display server or audio device.
set -euo pipefail
source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${1:-$source_root/out/cpu-build}"
output_root="${2:-$source_root/out/cpu-validation}"
cmake_bin="${CMAKE:-cmake}"
"$cmake_bin" -S "$source_root" -B "$build_root" -DCMAKE_BUILD_TYPE=Release -DFURY_ENABLE_DX12=OFF
"$cmake_bin" --build "$build_root" --parallel "${FURY_BUILD_WORKERS:-4}"
ctest --test-dir "$build_root" --output-on-failure
mkdir -p "$output_root"
output_root="$(cd "$output_root" && pwd)"
cd "$source_root"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FURY_AUDIO_BACKEND=cpu
export FURY_CPU_THREADS="${FURY_CPU_THREADS:-4}"
lab="$build_root/apps/renderlab/fury_renderlab"
for backend in software cpu-ray; do
  "$lab" --backend "$backend" --width 640 --height 360 --frames 24 --warmup 4 \
    --mode ray --hidden --capture "$output_root/coastal-$backend.ppm" --report "$output_root/coastal-$backend.json"
done
"$lab" --backend cpu-ray --width 640 --height 360 --frames 16 --warmup 2 --spp 4 \
  --mode path --bounces 6 --hidden --capture "$output_root/coastal-path.ppm" --report "$output_root/coastal-path.json"
"$lab" --backend cpu-ray --width 320 --height 180 --frames 8 --warmup 1 \
  --camera-motion --resize-test --deform-test --hidden --report "$output_root/motion-resize.json"
"$build_root/apps/vaultline/vaultline" --cpu-ray --photo --view storefront --no-hud \
  --width 640 --height 360 --spp 4 --frames 8 --capture "$output_root/storefront.ppm"
"$build_root/tests/fury_audio_tests" --write-wav "$output_root/cpu-audio.wav"
python3 "$source_root/scripts/audit-assets.py" --output "$output_root/asset-audit.json"
echo "CPU tests, runtime frames, captures, WAV and asset inventory written to $output_root"
