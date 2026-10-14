#!/usr/bin/env bash
# Full real-world regression and paired renderer-owned district captures.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/out/world-build}"
out="${2:-$root/out/world-validation}"
cmake_bin="${CMAKE:-cmake}"
if [[ "${FURY_SKIP_BUILD:-0}" != 1 ]]; then
  "$cmake_bin" -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DFURY_ENABLE_DX12=OFF
  # Keep validation usable on modest machines; explicit values may only lower this.
  workers="${FURY_BUILD_WORKERS:-2}"
  if [[ "$workers" != 1 && "$workers" != 2 ]]; then
    echo 'FURY_BUILD_WORKERS must be 1 or 2 for bounded world validation' >&2
    exit 2
  fi
  "$cmake_bin" --build "$build" --parallel "$workers"
fi
build="$(cd "$build" && pwd)"
mkdir -p "$out"; out="$(cd "$out" && pwd)"
exe="$build/apps/vaultline/vaultline"
[[ -x "$exe" ]] || { echo "Missing Vaultline executable: $exe" >&2; exit 2; }
if [[ "${FURY_SKIP_CTEST:-0}" != 1 ]]; then
  ctest --test-dir "$build" --output-on-failure --parallel 1
fi
options=(--vaultline "$exe" --source-root "$root" --output-dir "$out" --captures
  --width "${FURY_WORLD_WIDTH:-640}" --height "${FURY_WORLD_HEIGHT:-360}"
  --frames "${FURY_WORLD_FRAMES:-3}" --spp "${FURY_WORLD_SPP:-1}"
  --timeout "${FURY_WORLD_TIMEOUT:-90}")
[[ -z "${FURY_WORLD_RAY_FRAMES:-}" ]] || options+=(--ray-frames "$FURY_WORLD_RAY_FRAMES")
[[ -z "${FURY_WORLD_RAY_SPP:-}" ]] || options+=(--ray-spp "$FURY_WORLD_RAY_SPP")
[[ "${FURY_WORLD_PATH_SMOKE:-0}" != 1 ]] || options+=(--path-smoke)
[[ "${FURY_WORLD_GL_SMOKE:-0}" != 1 ]] || options+=(--gl-smoke)
python3 "$root/tests/world_runtime_tests.py" "${options[@]}"
git -C "$root" rev-parse HEAD > "$out/source-revision.txt"
git -C "$root" status --short > "$out/source-worktree.txt"
printf 'World audits, off/on PPMs, raw logs and measured observations: %s\n' "$out"
