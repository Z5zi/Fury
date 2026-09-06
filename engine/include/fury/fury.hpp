#pragma once

/// Fury — a lightweight C++17 game engine (SDL2 window/input/render clear).
#include "fury/platform.hpp"
#include "fury/log.hpp"
#include "fury/math.hpp"
#include "fury/timer.hpp"
#include "fury/input.hpp"
#include "fury/window.hpp"
#include "fury/renderer.hpp"
#include "fury/application.hpp"

namespace fury {

inline constexpr const char* engine_name() { return "Fury"; }
inline constexpr int version_major() { return 0; }
inline constexpr int version_minor() { return 1; }
inline constexpr int version_patch() { return 0; }

}  // namespace fury
