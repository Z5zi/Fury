# Fury

**Fury** is a lightweight, original C++17 game engine focused on a clean core:
windowing, input, a simple SDL2 clear/present renderer, logging, timing, and a
small math layer with optional **x86_64 assembly** kernels.

> Not Unreal. Not Unity. Not a GTA clone. Just Fury.

## Features

- **C++17** engine library (`fury_engine`) + demo app (`fury_demo`)
- **Cross-platform** CMake build for **Linux** and **Windows**
- **SDL2** window, input (Esc / quit), and accelerated renderer clear
- **Optional NASM** float3 dot product (`engine/math/asm/fury_dot_x64.asm`) when
  building on `x86_64` with NASM available (`FURY_HAS_ASM`)
- C++ fallback in `engine/src/math.cpp` when assembly is unavailable
- GitHub Actions CI for `ubuntu-latest` and `windows-latest`

## Layout

```
Fury/
  CMakeLists.txt
  README.md
  LICENSE
  .gitignore
  .github/workflows/ci.yml
  engine/
    CMakeLists.txt
    include/fury/          # public headers
    src/                  # engine sources
    math/asm/             # NASM (+ optional GAS) kernels
  apps/demo/
    CMakeLists.txt
    main.cpp
```

## Dependencies

| Platform | Packages / tools                                      |
|----------|--------------------------------------------------------|
| Linux    | `cmake`, `g++` (or clang), `libsdl2-dev`, `nasm`, `pkg-config` |
| Windows  | CMake, MSVC or Clang, SDL2 (e.g. vcpkg), NASM          |

### Debian / Ubuntu

```bash
sudo apt-get install -y cmake g++ libsdl2-dev nasm pkg-config
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The demo binary lands at:

```text
build/apps/demo/fury_demo
```

### Windows (vcpkg example)

```bat
vcpkg install sdl2:x64-windows
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Run the demo

Linux (with a display):

```bash
./build/apps/demo/fury_demo
```

Headless / CI smoke (Xvfb):

```bash
xvfb-run -a ./build/apps/demo/fury_demo
```

Controls:

- **Esc** — quit
- Window close — quit

The demo opens a window, clears a dark indigo color each frame, and logs FPS
about once per second.

## Assembly math

On `x86_64`, CMake enables `ASM_NASM` and defines `FURY_HAS_ASM=1` when a NASM
compiler is found. The kernel `fury_dot3_asm` implements:

```text
dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
```

- Linux / SysV: `engine/math/asm/fury_dot_x64.asm` (default path)
- Win64: same file assembled with `-dWIN64`
- Optional GAS mirror: `engine/math/asm/fury_dot_x64.S` (not used by default)

At runtime the demo logs whether the NASM or C++ backend is active.

## License

MIT — see [LICENSE](LICENSE).

## Repository

Intended public home: [https://github.com/Z5zi/Fury](https://github.com/Z5zi/Fury)
