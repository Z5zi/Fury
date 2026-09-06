#include "fury/day_night.hpp"

#include <algorithm>
#include <cmath>

namespace fury {
namespace {

float wrap01(float t) {
  t = std::fmod(t, 1.f);
  if (t < 0.f) {
    t += 1.f;
  }
  return t;
}

float smoothstep(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

Vec3 lerp3(const Vec3& a, const Vec3& b, float t) {
  return a * (1.f - t) + b * t;
}

}  // namespace

void DayNightCycle::update(float dt) {
  if (day_length > 1e-3f) {
    time_of_day = wrap01(time_of_day + dt / day_length);
  }
}

float DayNightCycle::night_factor() const {
  // Day roughly 0.22..0.78; night elsewhere. Soften transitions.
  const float day =
      smoothstep(0.18f, 0.28f, time_of_day) *
      (1.f - smoothstep(0.72f, 0.82f, time_of_day));
  return 1.f - day;
}

float DayNightCycle::lamp_emissive_mul() const {
  const float n = night_factor();
  return 0.40f + 2.8f * n;  // dim by day, brighter at night for readability
}

Lighting DayNightCycle::apply(const Lighting& base) const {
  Lighting lit = base;
  const float n = night_factor();
  const float angle = time_of_day * 6.28318530718f;
  // Sun arcs east→west; y negative = shining down.
  const float elev = std::sin(angle);  // + at noon-ish (t=0.25..0.75 → elev positive at 0.5)
  const float az = std::cos(angle);
  lit.sun_direction = normalize(Vec3{az * 0.65f, -0.35f - 0.7f * std::max(elev, 0.f),
                                     -0.45f + 0.25f * az});
  if (lit.sun_direction.y > -0.05f) {
    lit.sun_direction.y = -0.05f;
    lit.sun_direction = normalize(lit.sun_direction);
  }

  const Vec3 day_sun{1.f, 0.96f, 0.88f};
  const Vec3 dusk_sun{1.f, 0.55f, 0.28f};
  const Vec3 night_sun{0.35f, 0.45f, 0.75f};
  const float dusk_w =
      smoothstep(0.68f, 0.76f, time_of_day) * (1.f - smoothstep(0.82f, 0.90f, time_of_day)) +
      smoothstep(0.10f, 0.18f, time_of_day) * (1.f - smoothstep(0.24f, 0.32f, time_of_day));

  Vec3 sun_col = lerp3(day_sun, night_sun, n);
  sun_col = lerp3(sun_col, dusk_sun, dusk_w);
  lit.sun_color = sun_col;
  lit.sun_intensity = base.sun_intensity * (1.05f - 0.78f * n);
  lit.ambient = lerp3(Vec3{0.18f, 0.22f, 0.30f}, Vec3{0.09f, 0.11f, 0.18f}, n);

  const Vec3 day_fog{0.52f, 0.64f, 0.82f};
  const Vec3 night_fog{0.08f, 0.10f, 0.18f};
  lit.fog_color = lerp3(day_fog, night_fog, n);
  lit.fog_start = base.fog_start * (1.f - 0.25f * n);
  lit.fog_end = base.fog_end * (1.f - 0.15f * n);
  return lit;
}

Color DayNightCycle::sky_clear() const {
  const float n = night_factor();
  const Vec3 day_c{78.f / 255.f, 118.f / 255.f, 168.f / 255.f};
  const Vec3 dusk_c{180.f / 255.f, 90.f / 255.f, 55.f / 255.f};
  const Vec3 night_c{12.f / 255.f, 16.f / 255.f, 32.f / 255.f};
  const float dusk_w =
      smoothstep(0.68f, 0.76f, time_of_day) * (1.f - smoothstep(0.82f, 0.90f, time_of_day)) +
      smoothstep(0.10f, 0.18f, time_of_day) * (1.f - smoothstep(0.24f, 0.32f, time_of_day));
  Vec3 c = lerp3(day_c, night_c, n);
  c = lerp3(c, dusk_c, dusk_w);
  Color out;
  out.r = static_cast<std::uint8_t>(std::clamp(c.x, 0.f, 1.f) * 255.f);
  out.g = static_cast<std::uint8_t>(std::clamp(c.y, 0.f, 1.f) * 255.f);
  out.b = static_cast<std::uint8_t>(std::clamp(c.z, 0.f, 1.f) * 255.f);
  out.a = 255;
  return out;
}

}  // namespace fury
