#pragma once

/// Lightweight weather stub for Vaultline — rain / auto-drizzle (fog + wet asphalt hooks).

#include "fury/day_night.hpp"
#include "fury/math.hpp"
#include "fury/renderer.hpp"

#include <algorithm>
#include <cmath>

namespace fury {

enum class WeatherMode : int {
  Clear = 0,
  Rain = 1,
  AutoDrizzle = 2,
  Count
};

struct WeatherStub {
  WeatherMode mode{WeatherMode::Clear};

  void cycle() {
    const int n = static_cast<int>(WeatherMode::Count);
    mode = static_cast<WeatherMode>((static_cast<int>(mode) + 1) % n);
  }

  const char* mode_name() const {
    switch (mode) {
      case WeatherMode::Clear:
        return "clear";
      case WeatherMode::Rain:
        return "rain";
      case WeatherMode::AutoDrizzle:
        return "auto-drizzle";
      default:
        return "?";
    }
  }

  /// Rain intensity in [0,1]. Auto-drizzle peaks near dusk / early morning.
  float intensity(float time_of_day) const {
    if (mode == WeatherMode::Clear) {
      return 0.f;
    }
    if (mode == WeatherMode::Rain) {
      return 1.f;
    }
    // Soft lobes around dawn (~0.20) and dusk (~0.78)
    auto lobe = [](float t, float center, float width) {
      const float d = std::fabs(t - center);
      const float x = std::clamp(1.f - d / width, 0.f, 1.f);
      return x * x * (3.f - 2.f * x);
    };
    const float a = lobe(time_of_day, 0.20f, 0.12f);
    const float b = lobe(time_of_day, 0.78f, 0.14f);
    return std::clamp(0.15f + 0.75f * (std::max)(a, b), 0.f, 1.f);
  }

  Lighting apply(const Lighting& base, float rain) const {
    Lighting lit = base;
    if (rain <= 0.01f) {
      return lit;
    }
    const float r = std::clamp(rain, 0.f, 1.f);
    lit.fog_start = base.fog_start * (1.f - 0.62f * r);
    lit.fog_end = base.fog_end * (1.f - 0.48f * r);
    const Vec3 wet_fog{0.42f, 0.48f, 0.55f};
    lit.fog_color = lit.fog_color * (1.f - 0.55f * r) + wet_fog * (0.55f * r);
    lit.sun_intensity *= (1.f - 0.42f * r);
    lit.ambient = lit.ambient * (1.f - 0.15f * r) +
                  Vec3{0.10f, 0.12f, 0.16f} * (0.15f * r);
    return lit;
  }

  /// Darker / glossier asphalt for wet streets (+ wetness for aniso specular hack).
  void tint_asphalt(Vec3& albedo, float& roughness, float& metallic,
                    float& wetness, const Vec3& dry_albedo, float dry_rough,
                    float dry_metal, float rain) const {
    const float r = std::clamp(rain, 0.f, 1.f);
    const Vec3 wet_alb{dry_albedo.x * 0.42f, dry_albedo.y * 0.45f,
                       dry_albedo.z * 0.50f};
    albedo = dry_albedo * (1.f - r) + wet_alb * r;
    roughness = dry_rough * (1.f - 0.55f * r);
    metallic = dry_metal + (0.22f - dry_metal) * r;
    wetness = r;
  }
};

}  // namespace fury
