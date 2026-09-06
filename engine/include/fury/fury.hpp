#pragma once

/// Fury — lightweight C++17 engine with lit 3D mesh rendering (GL or software) with AO-lite/tonemap.
#include "fury/platform.hpp"
#include "fury/log.hpp"
#include "fury/math.hpp"
#include "fury/transform.hpp"
#include "fury/mesh.hpp"
#include "fury/collision.hpp"
#include "fury/camera.hpp"
#include "fury/scene.hpp"
#include "fury/timer.hpp"
#include "fury/input.hpp"
#include "fury/window.hpp"
#include "fury/renderer.hpp"
#include "fury/application.hpp"
#include "fury/heist.hpp"
#include "fury/inventory.hpp"
#include "fury/net.hpp"
#include "fury/day_night.hpp"
#include "fury/npc.hpp"
#include "fury/audio.hpp"

namespace fury {

inline constexpr const char* engine_name() { return "Fury"; }
inline constexpr int version_major() { return 0; }
inline constexpr int version_minor() { return 5; }
inline constexpr int version_patch() { return 0; }

}  // namespace fury
