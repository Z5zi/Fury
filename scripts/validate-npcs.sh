#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/build}"
ctest_bin="${CTEST_BIN:-ctest}"
cmake_bin="${CMAKE_BIN:-cmake}"
"$cmake_bin" --build "$build" --parallel "${BUILD_JOBS:-2}"
"$ctest_bin" --test-dir "$build" --output-on-failure \
  -R 'character_|npc_|application_capture|software_raster|cpu_ray|audio' -E '^npc_runtime$'
# Uses the real executable and isolated saves. Temporary evidence is removed
# unless NPC_VALIDATION_OUTPUT names a new output directory to keep it.
args=(--vaultline "$build/apps/vaultline/vaultline" --source-root "$root")
if [[ -n "${NPC_VALIDATION_OUTPUT:-}" ]]; then args+=(--output-dir "$NPC_VALIDATION_OUTPUT"); fi
python3 "$root/tests/npc_runtime_tests.py" "${args[@]}"
