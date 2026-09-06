#pragma once

#include "fury/math.hpp"
#include "fury/renderer.hpp"

namespace fury {

/// Simple day/night clock: advances time_of_day in [0,1) over day_length seconds.
/// 0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset.
struct DayNightCycle {
  float day_length{180.f};
  float time_of_day{0.32f};  // start mid-morning

  void update(float dt);

  /// 0 = full day, 1 = full night.
  float night_factor() const;
  /// Lamp emissive multiplier (stronger at night).
  float lamp_emissive_mul() const;

  Lighting apply(const Lighting& base) const;
  Color sky_clear() const;
};

}  // namespace fury
