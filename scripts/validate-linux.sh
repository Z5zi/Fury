#!/usr/bin/env bash
set -euo pipefail
source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${1:-$source_root/out/linux-build}"
cmake -S "$source_root" -B "$build_root" -G Ninja -DCMAKE_BUILD_TYPE=Release -DFURY_ENABLE_DX12=OFF
cmake --build "$build_root" --parallel "${FURY_BUILD_WORKERS:-12}"
ctest --test-dir "$build_root" --output-on-failure
cd "$build_root/apps/vaultline"
timeout 30s xvfb-run -a ./vaultline --smoke
SDL_VIDEODRIVER=dummy timeout 30s ./vaultline --soft --smoke
echo 'Linux build, unit tests, OpenGL smoke and software smoke passed.'
