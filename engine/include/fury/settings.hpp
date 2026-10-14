#pragma once

/// Vaultline player settings (from 3.9; language in 4.8; baseline in 5.0) — mouse/FOV/audio/quality/a11y/i18n; JSON persist.

#include "fury/log.hpp"
#include "fury/quality.hpp"
#include "fury/player_hair.hpp"
#include "fury/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>

namespace fury {

inline constexpr const char* kSettingsPath = "vaultline_settings.json";

struct VaultlineSettings {
  float mouse_sensitivity{0.0022f};
  float fov_y_degrees{60.f};
  float master_volume{1.f};  // 0..1 (mixer / cue gain)
  int player_hair{0};        // requested preference: 0=Auto 1=Original 2=Balanced
  int quality{1};            // 0=low 1=med 2=high
  bool show_subtitles{true}; // tips, banter, complication HUD tips
  bool invert_y{false};
  bool colorblind_hud{false};
  float hud_scale{1.f};      // 1.0 normal, ~1.35 large
  bool reduce_flash{false};  // disable lightning screen flash / ambient spike
  int language{0};           // 0=EN 1=ES (i18n stub 4.8.0)
  int trace_mode{1};         // 0=ray traced, 1=path traced (DX12)
  int upscaler{0};           // 0=native, 1=FSR, 2=XeSS
  int upscale_quality{1};    // native AA / quality / balanced / performance / ultra

  void clamp() {
    mouse_sensitivity = std::clamp(mouse_sensitivity, 0.0004f, 0.012f);
    fov_y_degrees = std::clamp(fov_y_degrees, 40.f, 100.f);
    master_volume = std::clamp(master_volume, 0.f, 1.f);
    quality = std::clamp(quality, 0, 2);
    if (player_hair < 0 || player_hair > 2) player_hair = 0;
    hud_scale = std::clamp(hud_scale, 1.f, 1.6f);
    language = std::clamp(language, 0, 1);
    trace_mode = std::clamp(trace_mode, 0, 1);
    upscaler = std::clamp(upscaler, 0, 2);
    upscale_quality = std::clamp(upscale_quality, 0, 4);
  }

  PlayerHairPreference player_hair_preference() const {
    return player_hair >= 0 && player_hair <= 2 ? static_cast<PlayerHairPreference>(player_hair) : PlayerHairPreference::Auto;
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

inline std::vector<std::string> top_level_fields(const std::string& src, bool include_known = false) {
  const char* known[] = {"mouse_sensitivity","fov_y_degrees","master_volume","quality",
    "player_hair","show_subtitles","invert_y","colorblind_hud","hud_scale","reduce_flash",
    "language","trace_mode","upscaler","upscale_quality"};
  std::vector<std::string> fields;
  std::size_t i = 0;
  auto whitespace = [&] { while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i]))) ++i; };
  whitespace();
  if (i == src.size() || src[i++] != '{') return fields;
  while (i < src.size()) {
    whitespace();
    if (i == src.size() || src[i] == '}') break;
    const auto start = i;
    if (src[i++] != '"') return {};
    const auto key_start = i;
    while (i < src.size() && src[i] != '"') {
      if (src[i] == '\\') ++i;
      if (i < src.size()) ++i;
    }
    if (i == src.size()) return {};
    const auto key = src.substr(key_start,i-key_start);
    ++i; whitespace();
    if (i == src.size() || src[i++] != ':') return {};
    bool quoted = false, escaped = false;
    int depth = 0;
    for (; i < src.size(); ++i) {
      const char c = src[i];
      if (quoted) {
        if (escaped) escaped = false;
        else if (c == '\\') escaped = true;
        else if (c == '"') quoted = false;
      } else if (c == '"') quoted = true;
      else if (c == '[' || c == '{') ++depth;
      else if ((c == '}' || c == ',') && depth == 0) break;
      else if (c == ']' || c == '}') --depth;
    }
    if (quoted || depth != 0 || i == src.size()) return {};
    bool recognized = false;
    for (const auto* name : known) if (key == name) recognized = true;
    if (!recognized || include_known) fields.push_back(src.substr(start,i-start));
    if (src[i++] == '}') break;
  }
  return fields;
}

// The new preference uses exact top-level bounded values. Do not let a nested
// extension, fractional number, integer overflow or malformed token choose hair.
inline int player_hair_choice(const std::string& src) {
  int choice = 0;
  for (const auto& field : top_level_fields(src, true)) {
    constexpr const char* key = "\"player_hair\"";
    constexpr std::size_t key_size = 13;
    if (field.compare(0,key_size,key) != 0) continue;
    std::size_t i = key_size;
    while (i < field.size() && std::isspace(static_cast<unsigned char>(field[i]))) ++i;
    if (i == field.size() || field[i++] != ':') continue;
    while (i < field.size() && std::isspace(static_cast<unsigned char>(field[i]))) ++i;
    const auto begin = i;
    while (i < field.size() && !std::isspace(static_cast<unsigned char>(field[i]))) ++i;
    const auto value = field.substr(begin,i-begin);
    while (i < field.size() && std::isspace(static_cast<unsigned char>(field[i]))) ++i;
    choice = i == field.size() && value == "1" ? 1 : i == field.size() && value == "2" ? 2 : 0;
  }
  return choice;
}

}  // namespace settings_detail

inline bool save_settings_json(const std::string& path, const VaultlineSettings& s) {
  VaultlineSettings snap = s;
  snap.clamp();
  std::ifstream previous(path);
  std::ostringstream previous_json;
  previous_json << previous.rdbuf();
  const auto extras = settings_detail::top_level_fields(previous_json.str());
  previous.close();
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
      << "  \"player_hair\": " << snap.player_hair << ",\n"
      << "  \"show_subtitles\": " << (snap.show_subtitles ? 1 : 0) << ",\n"
      << "  \"invert_y\": " << (snap.invert_y ? 1 : 0) << ",\n"
      << "  \"colorblind_hud\": " << (snap.colorblind_hud ? 1 : 0) << ",\n"
      << "  \"hud_scale\": " << snap.hud_scale << ",\n"
      << "  \"reduce_flash\": " << (snap.reduce_flash ? 1 : 0) << ",\n"
      << "  \"language\": " << snap.language << ",\n"
      << "  \"trace_mode\": " << snap.trace_mode << ",\n"
      << "  \"upscaler\": " << snap.upscaler << ",\n"
      << "  \"upscale_quality\": " << snap.upscale_quality;
  for (const auto& field : extras) out << ",\n  " << field;
  out << "\n}\n";
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
  s.player_hair = settings_detail::player_hair_choice(src);
  settings_detail::extract_bool(src, "show_subtitles", s.show_subtitles);
  settings_detail::extract_bool(src, "invert_y", s.invert_y);
  settings_detail::extract_bool(src, "colorblind_hud", s.colorblind_hud);
  settings_detail::extract_float(src, "hud_scale", s.hud_scale);
  settings_detail::extract_bool(src, "reduce_flash", s.reduce_flash);
  settings_detail::extract_int(src, "language", s.language);
  settings_detail::extract_int(src, "trace_mode", s.trace_mode);
  settings_detail::extract_int(src, "upscaler", s.upscaler);
  settings_detail::extract_int(src, "upscale_quality", s.upscale_quality);
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
  int selected{0};
  static constexpr int kPlayerHairRow = 13;
  static constexpr int kRowCount = 14;
};

}  // namespace fury
