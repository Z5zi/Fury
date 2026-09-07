#pragma once

/// Vaultline player settings (3.9.0) — mouse/FOV/audio/quality/a11y; JSON persist.

#include "fury/log.hpp"
#include "fury/quality.hpp"
#include "fury/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace fury {

inline constexpr const char* kSettingsPath = "vaultline_settings.json";

struct VaultlineSettings {
  float mouse_sensitivity{0.0022f};
  float fov_y_degrees{60.f};
  float master_volume{1.f};  // 0..1 (mixer / cue gain)
  int quality{1};            // 0=low 1=med 2=high
  bool show_subtitles{true}; // tips, banter, complication HUD tips
  bool invert_y{false};
  bool colorblind_hud{false};
  float hud_scale{1.f};      // 1.0 normal, ~1.35 large
  bool reduce_flash{false};  // disable lightning screen flash / ambient spike

  void clamp() {
    mouse_sensitivity = std::clamp(mouse_sensitivity, 0.0004f, 0.012f);
    fov_y_degrees = std::clamp(fov_y_degrees, 40.f, 100.f);
    master_volume = std::clamp(master_volume, 0.f, 1.f);
    quality = std::clamp(quality, 0, 2);
    hud_scale = std::clamp(hud_scale, 1.f, 1.6f);
  }

  QualityLevel quality_level() const {
    return static_cast<QualityLevel>(std::clamp(quality, 0, 2));
  }

  void set_quality_level(QualityLevel lvl) {
    quality = static_cast<int>(lvl);
    clamp();
  }
};

namespace settings_detail {

inline bool extract_int(const std::string& src, const char* key, int& out) {
  const std::string needle = std::string("\"") + key + "\"";
  const auto pos = src.find(needle);
  if (pos == std::string::npos) return false;
  const auto colon = src.find(':', pos + needle.size());
  if (colon == std::string::npos) return false;
  std::size_t i = colon + 1;
  while (i < src.size() && (src[i] == ' ' || src[i] == '\t')) ++i;
  if (i >= src.size()) return false;
  const bool neg = src[i] == '-';
  if (neg) ++i;
  if (i >= src.size() || src[i] < '0' || src[i] > '9') return false;
  int v = 0;
  while (i < src.size() && src[i] >= '0' && src[i] <= '9') {
    v = v * 10 + (src[i] - '0');
    ++i;
  }
  out = neg ? -v : v;
  return true;
}

inline bool extract_float(const std::string& src, const char* key, float& out) {
  const std::string needle = std::string("\"") + key + "\"";
  const auto pos = src.find(needle);
  if (pos == std::string::npos) return false;
  const auto colon = src.find(':', pos + needle.size());
  if (colon == std::string::npos) return false;
  std::size_t i = colon + 1;
  while (i < src.size() && (src[i] == ' ' || src[i] == '\t')) ++i;
  if (i >= src.size()) return false;
  char* end = nullptr;
  const float v = std::strtof(src.c_str() + static_cast<std::ptrdiff_t>(i), &end);
  if (end == src.c_str() + static_cast<std::ptrdiff_t>(i)) return false;
  out = v;
  return true;
}

inline bool extract_bool(const std::string& src, const char* key, bool& out) {
  int v = 0;
  if (!extract_int(src, key, v)) {
    // also accept true/false literals
    const std::string needle = std::string("\"") + key + "\"";
    const auto pos = src.find(needle);
    if (pos == std::string::npos) return false;
    const auto colon = src.find(':', pos + needle.size());
    if (colon == std::string::npos) return false;
    const auto t = src.find("true", colon + 1);
    const auto f = src.find("false", colon + 1);
    if (t != std::string::npos && (f == std::string::npos || t < f)) {
      out = true;
      return true;
    }
    if (f != std::string::npos) {
      out = false;
      return true;
    }
    return false;
  }
  out = v != 0;
  return true;
}

}  // namespace settings_detail

inline bool save_settings_json(const std::string& path, const VaultlineSettings& s) {
  VaultlineSettings snap = s;
  snap.clamp();
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    Log::warn(std::string("save_settings_json failed to open ") + path);
    return false;
  }
  out << "{\n"
      << "  \"mouse_sensitivity\": " << snap.mouse_sensitivity << ",\n"
      << "  \"fov_y_degrees\": " << snap.fov_y_degrees << ",\n"
      << "  \"master_volume\": " << snap.master_volume << ",\n"
      << "  \"quality\": " << snap.quality << ",\n"
      << "  \"show_subtitles\": " << (snap.show_subtitles ? 1 : 0) << ",\n"
      << "  \"invert_y\": " << (snap.invert_y ? 1 : 0) << ",\n"
      << "  \"colorblind_hud\": " << (snap.colorblind_hud ? 1 : 0) << ",\n"
      << "  \"hud_scale\": " << snap.hud_scale << ",\n"
      << "  \"reduce_flash\": " << (snap.reduce_flash ? 1 : 0) << "\n"
      << "}\n";
  if (!out) {
    Log::warn("save_settings_json write error");
    return false;
  }
  Log::info(std::string("Settings saved -> ") + path);
  return true;
}

inline bool load_settings_json(const std::string& path, VaultlineSettings& out_s) {
  std::ifstream in(path);
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();
  VaultlineSettings s = out_s;
  settings_detail::extract_float(src, "mouse_sensitivity", s.mouse_sensitivity);
  settings_detail::extract_float(src, "fov_y_degrees", s.fov_y_degrees);
  settings_detail::extract_float(src, "master_volume", s.master_volume);
  settings_detail::extract_int(src, "quality", s.quality);
  settings_detail::extract_bool(src, "show_subtitles", s.show_subtitles);
  settings_detail::extract_bool(src, "invert_y", s.invert_y);
  settings_detail::extract_bool(src, "colorblind_hud", s.colorblind_hud);
  settings_detail::extract_float(src, "hud_scale", s.hud_scale);
  settings_detail::extract_bool(src, "reduce_flash", s.reduce_flash);
  s.clamp();
  out_s = s;
  Log::info(std::string("Settings loaded <- ") + path);
  return true;
}

/// Deuteranopia-ish remaps for key HUD accents (prototype palette toggle).
inline Color colorblind_remap(Color c, bool enabled) {
  if (!enabled) return c;
  // Push reds toward amber/magenta; greens toward blue-cyan.
  const int r = c.r, g = c.g, b = c.b;
  if (r > g + 40 && r > b + 20) {
    // red/orange heat → magenta-amber
    c.r = static_cast<std::uint8_t>(std::clamp(r, 0, 255));
    c.g = static_cast<std::uint8_t>(std::clamp((g + 90) / 2, 0, 255));
    c.b = static_cast<std::uint8_t>(std::clamp(b + 70, 0, 255));
  } else if (g > r + 30 && g > b + 20) {
    // green cash/loot → cyan-blue
    c.r = static_cast<std::uint8_t>(std::clamp(r / 2, 0, 255));
    c.g = static_cast<std::uint8_t>(std::clamp((g + b) / 2, 0, 255));
    c.b = static_cast<std::uint8_t>(std::clamp(std::max(b, g), 0, 255));
  }
  return c;
}

struct SettingsPanel {
  bool open{false};
  int selected{0};  // 0..8 rows
  static constexpr int kRowCount = 9;
};

}  // namespace fury
