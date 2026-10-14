#pragma once

#include "fury/bitmap_font.hpp"
#include "fury/heist.hpp"
#include "fury/settings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace vaultline {

// Presentation snapshot only. Earnings is HeistScore::lifetime_cash: completed
// job payouts and daily/finale bonuses, excluding cash drops and fence sales.
struct StatusHudState {
  int cash{};
  fury::HeistPhase phase{fury::HeistPhase::Idle};
  float loot{};
  int job_earnings{};
  float heat{};
  float visibility{}; // Accumulated exposure, not a detection probability.
  bool crouching{};
  int crew_nearby{}; // Active NPC crew within the existing 5.5-unit radius.
};

struct StatusHudRow {
  const char* label{};
  std::array<char, 16> value{};
  const char* phase{};
  float fill{};
  fury::Color color{};
};

inline const char* status_phase_name(fury::HeistPhase phase) {
  switch (phase) {
    case fury::HeistPhase::Idle: return "IDLE";
    case fury::HeistPhase::Approach: return "APPROACH";
    case fury::HeistPhase::Breach: return "BREACH";
    case fury::HeistPhase::Looting: return "LOOTING";
    case fury::HeistPhase::Escape: return "ESCAPE";
    case fury::HeistPhase::Success: return "SUCCESS";
    case fury::HeistPhase::Failed: return "FAILED";
  }
  return "UNKNOWN";
}

inline void format_status_ratio(float value, std::array<char, 16>& out) {
  // Labels show the real quantity, never the legacy two-percent meter marker.
  const float normalized = std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
  const int percent = static_cast<int>(std::lround(normalized * 100.f));
  std::snprintf(out.data(), out.size(), "%d/100", percent);
}

inline std::array<StatusHudRow, 6> status_hud_rows(const StatusHudState& s,
                                                bool colorblind) {
  using fury::Color;
  std::array<StatusHudRow, 6> rows{};
  rows[0].label = "CASH";
  fury::format_cash_label(s.cash, rows[0].value.data(), rows[0].value.size());
  rows[0].fill = (std::min)(1.f, static_cast<float>(s.cash) / 50000.f);
  rows[0].color = {50, 200, 90, 230};

  rows[1].label = "LOOT";
  rows[1].phase = status_phase_name(s.phase);
  format_status_ratio(s.loot, rows[1].value);
  rows[1].fill = (std::max)(s.loot, .02f);
  rows[1].color = {220, 180, 40, 230};
  if (s.phase == fury::HeistPhase::Escape) rows[1].color = {80, 180, 255, 230};
  else if (s.phase == fury::HeistPhase::Success) rows[1].color = {90, 255, 140, 230};
  else if (s.phase == fury::HeistPhase::Failed) rows[1].color = {220, 60, 60, 230};

  rows[2].label = "JOB EARNINGS";
  fury::format_cash_label(s.job_earnings, rows[2].value.data(), rows[2].value.size());
  rows[2].fill = (std::min)(1.f, static_cast<float>(s.job_earnings) / 80000.f);
  rows[2].color = {180, 120, 255, 230};

  rows[3].label = "HEAT";
  format_status_ratio(s.heat, rows[3].value);
  rows[3].fill = (std::max)(s.heat, .02f);
  rows[3].color = {255, 160, 40, 230};
  if (s.heat > .66f) rows[3].color = {255, 50, 50, 240};
  else if (s.heat > .33f) rows[3].color = {255, 120, 30, 230};

  rows[4].label = s.crouching ? "CROUCH VIS" : "VISIBILITY";
  const float visibility = std::clamp(s.visibility, 0.f, 1.f);
  format_status_ratio(visibility, rows[4].value);
  rows[4].fill = (std::max)(visibility, .02f);
  rows[4].color = {80, 200, 220, 230};
  if (visibility > .66f) rows[4].color = {255, 90, 160, 240};
  else if (visibility > .33f) rows[4].color = {120, 220, 255, 230};
  if (s.crouching) rows[4].color = {60, 180, 140, 230};

  rows[5].label = "CREW NEAR";
  std::snprintf(rows[5].value.data(), rows[5].value.size(), "%d", s.crew_nearby);
  rows[5].fill = s.crew_nearby > 0 ? .5f * static_cast<float>(s.crew_nearby) : 0.f;
  rows[5].color = {90, 180, 255, 230};
  // Preserve the existing accessibility color mapping (crew is not remapped).
  for (std::size_t i = 0; i < 5; ++i)
    rows[i].color = fury::colorblind_remap(rows[i].color, colorblind);
  return rows;
}

inline float status_text_width(const char* text, float scale) {
  return (static_cast<float>(std::strlen(text)) * 6.f - 1.f) * scale;
}

struct StatusHudLayout {
  float scale{}, glyph{}, bar_x{}, bar_width{}, text_left{}, text_right{};
  float row_pitch{}, meter_offset{}, ready_y{};
  float row_y(std::size_t row) const { return 16.f + 4.f*scale + float(row)*row_pitch; }
  float meter_y(std::size_t row) const { return row_y(row) + meter_offset; }
};

inline StatusHudLayout layout_status_hud(int window_width, int window_height, float hud_scale) {
  StatusHudLayout box;
  box.scale = std::clamp(hud_scale, 1.f, 1.6f);
  box.bar_x = 28.f + (box.scale - 1.f) * 8.f;
  // The minimap remains 150 pixels wide, 16 pixels from the right edge.
  // Reserve an eight-pixel gap. Only content is constrained; the established
  // outer panel and NPC-nameplate obstacle stay at 340*HS by 148*HS.
  const float bar_right = std::min(box.bar_x + 316.f*box.scale,
                                    float(window_width) - 166.f - 8.f);
  box.bar_width = std::max(0.f, bar_right - box.bar_x);
  box.text_left = box.bar_x + 4.f*box.scale;
  box.text_right = bar_right - 4.f*box.scale;
  // JOB EARNINGS (71 cells) + $2147483647 (65 cells) is the widest
  // valid row. A stable cap avoids resizing when money/phase values change.
  const float money_fit = (box.text_right-box.text_left-12.f*box.scale)/136.f;
  box.glyph = std::max(1.f, std::min(2.f*box.scale, money_fit));
  box.row_pitch = 22.f*box.scale;
  box.meter_offset = 15.f*box.scale;
  const float normal_ready = 16.f + 132.f*box.scale;
  // At short heights reserve the existing vehicle stack (58px from the normal
  // ready baseline through radio bottom) above the compass's forward notch at
  // H-118. Tighten only vertical gaps; font size, six rows and panel stay fixed.
  box.ready_y = std::min(normal_ready, std::max(148.f, float(window_height)-118.f-58.f));
  if (box.ready_y < normal_ready) {
    box.meter_offset = 7.f*box.glyph + box.scale;
    const float first_row_bottom = box.row_y(0) + box.meter_offset + 2.f*box.scale;
    box.row_pitch = (box.ready_y-box.scale-first_row_bottom)/5.f;
  }
  return box;
}

struct StatusAuxLayout {
  float offset{}, ready_y{}, tier_x{16.f}, tier_y{210.f}, crouch_x{16.f}, crouch_y{170.f};
};

inline StatusAuxLayout layout_status_auxiliary(const StatusHudLayout& box,
                                                int window_width, bool in_vehicle) {
  StatusAuxLayout aux;
  aux.offset = box.ready_y-148.f;
  aux.ready_y = box.ready_y+(in_vehicle?32.f:0.f);
  const float cluster_bottom = in_vehicle ? 206.f+aux.offset : aux.ready_y+22.f;
  // Keep the original HS=1 coordinates. At larger sizes, relocate only a small
  // existing marker that intersects the scaled status/readiness/vehicle stack.
  if (box.scale>1.f && aux.crouch_y<cluster_bottom) {
    aux.crouch_x = 16.f+220.f+8.f;
    aux.crouch_y = box.ready_y;
  }
  if (box.scale>1.f && aux.tier_y<cluster_bottom) {
    aux.tier_x = 16.f+220.f+8.f+120.f+8.f;
    aux.tier_y = box.ready_y;
    if (aux.tier_x+200.f>float(window_width)-166.f && aux.tier_y<166.f+8.f)
      aux.tier_y = 166.f+8.f; // Below the unchanged minimap when sharing its x lane.
  }
  return aux;
}

inline void draw_status_hud(fury::Renderer& r, const StatusHudState& state,
                            int window_width, int window_height, float hud_scale, bool colorblind) {
  const auto box = layout_status_hud(window_width, window_height, hud_scale);
  // Bounds intentionally match the existing status/nameplate obstacle rectangle.
  r.draw_hud_rect(16.f, 16.f, 340.f * box.scale, 148.f * box.scale, {12, 16, 24, 235});
  const auto rows = status_hud_rows(state, colorblind);
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto& row = rows[i];
    const float y = box.row_y(i);
    const float value_x = box.text_right - status_text_width(row.value.data(), box.glyph);
    const fury::Color text{238, 244, 250, 255};
    fury::draw_bitmap_text(r, box.text_left, y, row.label, text, box.glyph);
    fury::draw_bitmap_text(r, value_x, y, row.value.data(), text, box.glyph);
    if (row.phase) {
      const float phase_x = value_x - 12.f * box.scale - status_text_width(row.phase, box.glyph);
      fury::draw_bitmap_text(r, phase_x, y, row.phase, text, box.glyph);
    }
    r.draw_hud_rect(box.bar_x, box.meter_y(i), box.bar_width, 2.f * box.scale, {40, 50, 60, 220});
    r.draw_hud_rect(box.bar_x, box.meter_y(i), box.bar_width * row.fill, 2.f * box.scale, row.color);
  }
}

} // namespace vaultline
