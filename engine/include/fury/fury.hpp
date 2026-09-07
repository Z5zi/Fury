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
#include "fury/heat.hpp"
#include "fury/mission.hpp"
#include "fury/crew.hpp"
#include "fury/banter.hpp"
#include "fury/particles.hpp"
#include "fury/weather.hpp"
#include "fury/pursuit.hpp"
#include "fury/traffic.hpp"
#include "fury/factions.hpp"
#include "fury/cutscene.hpp"
#include "fury/quality.hpp"
#include "fury/interior.hpp"
#include "fury/skills.hpp"
#include "fury/daily.hpp"
#include "fury/photo_mode.hpp"
#include "fury/replay.hpp"

namespace fury {

inline constexpr const char* engine_name() { return "Fury"; }
inline constexpr int version_major() { return 2; }
inline constexpr int version_minor() { return 7; }
inline constexpr int version_patch() { return 0; }

}  // namespace fury
