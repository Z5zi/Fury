#pragma once

/// Graphics quality presets for Vaultline — cull, shadows, bloom, reflections, fog.

#include "fury/renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace fury {

enum class QualityLevel : int {
  Low = 0,
  Med = 1,
  High = 2,
  Count
};

struct QualityPreset {
  QualityLevel level{QualityLevel::Med};
  float cull_distance{90.f};
  int shadow_map_size{1024};
  int shadow_cascade_count{1};
  bool enable_bloom{true};
  float bloom_strength{0.45f};
  bool enable_reflections{true};
  float reflection_strength{0.55f};
  float fog_start{40.f};
  float fog_end{150.f};
  float camera_far{360.f};

  static QualityPreset make(QualityLevel lvl) {
    QualityPreset p;
    p.level = lvl;
    switch (lvl) {
      case QualityLevel::Low:
        p.cull_distance = 55.f;
        p.shadow_map_size = 512;
        p.shadow_cascade_count = 1;
        p.enable_bloom = false;
        p.bloom_strength = 0.f;
        p.enable_reflections = false;
        p.reflection_strength = 0.f;
        p.fog_start = 28.f;
        p.fog_end = 85.f;
        p.camera_far = 200.f;
        break;
      case QualityLevel::High:
        p.cull_distance = 140.f;
        p.shadow_map_size = 2048;
        p.shadow_cascade_count = 2;  // 2-cascade directional stub
        p.enable_bloom = true;
        p.bloom_strength = 0.55f;
        p.enable_reflections = true;
        p.reflection_strength = 0.65f;
        p.fog_start = 55.f;
        p.fog_end = 220.f;
        p.camera_far = 480.f;
        break;
      case QualityLevel::Med:
      default:
        p.level = QualityLevel::Med;
        p.cull_distance = 90.f;
        p.shadow_map_size = 1024;
        p.shadow_cascade_count = 1;
        p.enable_bloom = true;
        p.bloom_strength = 0.45f;
        p.enable_reflections = true;
        p.reflection_strength = 0.55f;
        p.fog_start = 40.f;
        p.fog_end = 150.f;
        p.camera_far = 360.f;
        break;
    }
    return p;
  }

  void cycle() {
    const int n = static_cast<int>(QualityLevel::Count);
    *this = make(static_cast<QualityLevel>(
        (static_cast<int>(level) + 1) % n));
  }

  const char* name() const {
    switch (level) {
      case QualityLevel::Low:
        return "low";
      case QualityLevel::High:
        return "high";
      case QualityLevel::Med:
      default:
        return "med";
    }
  }

  /// Apply cull / FX / fog into lighting + return cull for AppConfig.
  void apply_to_lighting(Lighting& lit) const {
    lit.fog_start = fog_start;
    lit.fog_end = fog_end;
    lit.enable_bloom = enable_bloom;
    lit.bloom_strength = bloom_strength;
    lit.enable_reflections = enable_reflections;
    lit.reflection_strength = reflection_strength;
    lit.shadow_map_size = shadow_map_size;
    lit.shadow_cascade_count = shadow_cascade_count;
    // Low: softer / cheaper shadows; high: stronger contact + 2 cascades
    if (level == QualityLevel::Low) {
      lit.shadow_strength = 0.28f;
    } else if (level == QualityLevel::High) {
      lit.shadow_strength = 0.55f;
    } else {
      lit.shadow_strength = 0.45f;
    }
  }

  static QualityLevel parse_env(const char* env) {
    if (!env || !env[0]) return QualityLevel::Med;
    std::string v;
    for (const char* p = env; *p; ++p) {
      const unsigned char c = static_cast<unsigned char>(*p);
      v.push_back(static_cast<char>(std::tolower(c)));
    }
    if (v == "low" || v == "lo" || v == "0" || v == "l") return QualityLevel::Low;
    if (v == "high" || v == "hi" || v == "2" || v == "h") return QualityLevel::High;
    if (v == "med" || v == "medium" || v == "mid" || v == "1" || v == "m")
      return QualityLevel::Med;
    return QualityLevel::Med;
  }
};

}  // namespace fury
