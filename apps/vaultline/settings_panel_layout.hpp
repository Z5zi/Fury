#pragma once

#include <fury/bitmap_font.hpp>
#include <fury/settings.hpp>

#include <algorithm>
#include <cmath>

namespace vaultline {

// Fixed menu fonts are independent of gameplay HUD scale. The supported
// single-list viewport is at least 640 x 360; smaller windows need scrolling.
struct SettingsPanelLayout {
  static constexpr float label_scale = 1.6f;
  static constexpr float language_scale = 2.f;
  static constexpr float footer_scale = 1.4f;
  float panel_y{}, panel_height{}, header_y{}, body_y{}, row_pitch{}, footer_y{};

  float row_height() const { return row_pitch - 2.f; }
  float row_y(int row) const { return body_y + float(row) * row_pitch; }
  float centered_y(int row, float height) const {
    return row_y(row) + (row_height() - height) * .5f;
  }
  float label_y(int row) const { return centered_y(row, fury::kBitmapCellH * label_scale); }
  float language_y(int row) const { return centered_y(row, fury::kBitmapCellH * language_scale); }
  float meter_y(int row) const { return centered_y(row, 8.f); }
  float chip_height() const { return row_height() - 2.f; }
  float chip_y(int row) const { return centered_y(row, chip_height()); }
  float footer_line_y(int line) const { return footer_y + 16.f * float(line); }
};

inline SettingsPanelLayout layout_settings_panel(float viewport_height) {
  constexpr float margin = 8.f;
  constexpr float padding = 12.f;
  constexpr float header = 18.f;
  constexpr float header_gap = 8.f;
  constexpr float footer_gap = 8.f;
  constexpr float footer = 32.f;
  constexpr float fixed_height = padding * 2.f + header + header_gap + footer_gap + footer;
  const float pitch = std::clamp(std::floor((viewport_height - margin * 2.f - fixed_height) /
      float(fury::SettingsPanel::kRowCount)), 18.f, 36.f);
  const float panel_height = fixed_height + float(fury::SettingsPanel::kRowCount) * pitch;
  const float panel_y = (viewport_height - panel_height) * .5f;
  const float header_y = panel_y + padding;
  const float body_y = header_y + header + header_gap;
  return {panel_y, panel_height, header_y, body_y, pitch,
          body_y + float(fury::SettingsPanel::kRowCount) * pitch + footer_gap};
}

}  // namespace vaultline
