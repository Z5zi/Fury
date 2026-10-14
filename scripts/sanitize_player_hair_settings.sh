#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/evidence/sanitizers"
mkdir -p "$out"
flags=(-std=c++17 -O1 -g0 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread -I"$root/engine/include" -I"$root/tests")
c++ "${flags[@]}" "$root/engine/src/player_hair.cpp" "$root/tests/player_hair_policy_tests.cpp" -o "$out/policy"
c++ "${flags[@]}" "$root/engine/src/log.cpp" "$root/tests/settings_tests.cpp" -o "$out/settings"
sdl_library_flags=()
if [[ -n "${FURY_DEPS:-}" ]]; then sdl_library_flags+=("-L$FURY_DEPS/root/usr/lib/x86_64-linux-gnu"); fi
c++ "${flags[@]}" "${sdl_library_flags[@]}" $(sdl2-config --cflags) "$root/engine/src/log.cpp" "$root/engine/src/input.cpp" "$root/tests/player_hair_input_tests.cpp" $(sdl2-config --libs) -o "$out/input"
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
for test in policy settings input; do
  "$out/$test" >"$out/$test.log" 2>&1
  echo "$test ASan+UBSan passed"
done
