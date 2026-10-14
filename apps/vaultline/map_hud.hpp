#pragma once

#include "npc_dialogue_hud.hpp"
#include <fury/i18n.hpp>

#include <array>
#include <limits>

namespace vaultline {

// Presentation only. No hit boxes, input, eligibility mutation, or travel state.
inline constexpr std::size_t kMapHudGlyphBudget = 384;
using MapDistrictNames = std::array<std::string_view, 6>;

inline int map_hud_cooldown_seconds(float cooldown) {
  if (!(cooldown > 0.f)) return 0;
  // Display rounding must never say 0S while the unchanged handler still blocks.
  return int(std::min(double(std::numeric_limits<int>::max()),
                      std::max(1.0, std::ceil(double(cooldown)))));
}

enum class MapHudReason { LoftFocus, OutsideLoft, Cooldown, InsufficientCash, Ready };

inline MapHudReason map_hud_reason(int focus, bool in_safehouse, float cooldown,
                                   int cash, int fare) {
  if (focus == 4) return MapHudReason::LoftFocus;
  if (!in_safehouse) return MapHudReason::OutsideLoft;
  if (cooldown > 0.f) return MapHudReason::Cooldown;
  if (cash < fare) return MapHudReason::InsufficientCash;
  return MapHudReason::Ready;
}

struct MapHudText {
  std::u32string heading;
  std::array<std::u32string, 6> names;
  std::array<std::u32string, 4> footer;
  int focus{};
  MapHudReason reason{};
};

inline MapHudText map_hud_text(fury::Lang lang, const MapDistrictNames& names,
    int focus, bool in_safehouse, float cooldown, int cash, int fare) {
  MapHudText text;
  const bool es = lang == fury::Lang::Es;
  text.focus = std::clamp(focus, 0, 5);
  text.reason = map_hud_reason(focus, in_safehouse, cooldown, cash, fare);
  text.heading = npc_hud_text(es ? "MAPA" : "MAP");
  for (std::size_t i = 0; i < names.size(); ++i)
    text.names[i] = npc_hud_text(std::to_string(i + 1) + " " + std::string(names[i]));
  text.footer[0] = npc_hud_text(std::string(es ? "DESTINO " : "FOCUS ") +
      std::to_string(text.focus + 1) + ": " + std::string(names[text.focus]));
  const std::string seconds = std::to_string(map_hud_cooldown_seconds(cooldown));
  std::string values = std::string(es ? "TARIFA $" : "FARE $") + std::to_string(fare) +
      (es ? " / EFECTIVO $" : " / CASH $") + std::to_string(cash);
  if (cooldown > 0.f) values += (es ? " / ESPERA " : " / COOLDOWN ") + seconds + "S";
  text.footer[1] = npc_hud_text(values);
  std::string reason;
  switch (text.reason) {
    case MapHudReason::LoftFocus:
      reason = es ? "HARBOR LOFT: ELIGE OTRO DISTRITO" : "HARBOR LOFT: CHOOSE ANOTHER DISTRICT"; break;
    case MapHudReason::OutsideLoft:
      reason = es ? "VIAJA SOLO DESDE HARBOR LOFT" : "TRAVEL FROM HARBOR LOFT ONLY"; break;
    case MapHudReason::Cooldown:
      reason = (es ? "ESPERA " : "WAIT ") + seconds +
          (es ? "S PARA VIAJAR" : "S BEFORE TRAVEL"); break;
    case MapHudReason::InsufficientCash:
      reason = std::string(es ? "NECESITAS $" : "NEED $") + std::to_string(fare) +
          (es ? " PARA VIAJAR" : " TO TRAVEL"); break;
    case MapHudReason::Ready:
      reason = es ? "LISTO - RETURN PARA VIAJAR" : "READY - RETURN TO TRAVEL"; break;
  }
  text.footer[2] = npc_hud_text(reason);
  text.footer[3] = npc_hud_text(es ? "1-6/CLIC MAPA / RETURN VIAJAR / TAB/ESC CERRAR"
                               : "1-6/CLICK MAP / RETURN TRAVEL / TAB/ESC CLOSE");
  return text;
}

inline float map_hud_text_width(std::u32string_view text, float scale = 1.f) {
  return text.empty() ? 0.f : (6.f * float(text.size()) - 1.f) * scale;
}

struct MapHudLayout {
  float heading_x{}, heading_y{}, heading_scale{};
  std::array<float, 6> name_x{}, name_y{};
  float legend_scale{};
  float footer_x{}, footer_y{}, footer_width{}, footer_height{}, footer_scale{};
};

inline MapHudLayout layout_map_hud(int width, int height, float hud_scale,
                                   float geometry_top, const MapHudText& text) {
  MapHudLayout box;
  const float w = float(width), h = float(height);
  const float mx = .08f*w, my = .08f*h, mw = .84f*w;
  const float desired = 1.5f * std::clamp(hud_scale, 1.f, 1.6f) *
      std::min(w / 640.f, h / 360.f);
  // Fixed geographic rectangles take priority over HUD enlargement. The legend
  // starts above the panel to also clear the existing 3px focus outline.
  const float legend_y = my - 5.f, lane = mw - 12.f;
  const bool wide = width >= 960;
  float max_name = 0.f, total_names = 0.f;
  for (const auto& name : text.names) {
    max_name = std::max(max_name, map_hud_text_width(name));
    total_names += map_hud_text_width(name);
  }
  const float width_cap = wide ? (lane - 50.f) / total_names
                              : (lane / 3.f - 4.f) / max_name;
  const float height_cap = (geometry_top - 2.f - legend_y) / (wide ? 7.f : 16.f);
  box.legend_scale = std::max(0.f, std::min({desired, wide ? 2.f : desired,
                                            width_cap, height_cap}));
  float next_x = mx + 6.f;
  for (std::size_t i = 0; i < text.names.size(); ++i) {
    box.name_x[i] = wide ? next_x : mx + 6.f + float(i % 3) * lane / 3.f;
    box.name_y[i] = legend_y + (wide ? 0.f : float(i / 3) * 9.f * box.legend_scale);
    next_x += map_hud_text_width(text.names[i], box.legend_scale) + 10.f;
  }
  box.heading_x = mx + 6.f;
  box.heading_scale = std::min(2.f, box.legend_scale);
  box.heading_y = std::max(0.f, my - 24.f);
  box.footer_x = mx + 12.f;
  box.footer_y = .8f*h + 4.f;
  box.footer_width = mw - 24.f;
  float max_footer = 0.f;
  for (const auto& line : text.footer)
    max_footer = std::max(max_footer, map_hud_text_width(line));
  box.footer_scale = std::max(0.f, std::min({3.f, desired, (.2f*h - 12.f) / 40.f,
                                           box.footer_width / (max_footer + 6.f)}));
  box.footer_height = 40.f * box.footer_scale;
  return box;
}

template<class HudRenderer>
inline void draw_map_index(HudRenderer& r, float x, float y, int index,
                            bool focused, std::size_t& budget) {
  const std::u32string digit(1, char32_t(U'1' + index));
  draw_npc_hud_text(r, x + 5.25f, y + .75f, digit,
      focused ? fury::Color{10,16,26,255} : fury::Color{245,245,250,255}, 1.5f, budget);
}

template<class HudRenderer>
inline void draw_map_hud(HudRenderer& r, const MapHudLayout& box,
                         const MapHudText& text, std::size_t& budget) {
  const fury::Color ink{240,244,250,255}, gold{255,220,125,255};
  draw_npc_hud_text(r, box.heading_x, box.heading_y, text.heading, gold,
                    box.heading_scale, budget);
  for (std::size_t i = 0; i < text.names.size(); ++i) {
    if (int(i) == text.focus)
      r.draw_hud_rect(box.name_x[i] - 4.f, box.name_y[i] + 2.f*box.legend_scale,
          2.f, 3.f*box.legend_scale, gold);
    draw_npc_hud_text(r, box.name_x[i], box.name_y[i], text.names[i],
                      int(i) == text.focus ? gold : ink, box.legend_scale, budget);
  }
  r.draw_hud_rect(box.footer_x, box.footer_y, box.footer_width, box.footer_height,
                  fury::Color{9,15,25,255});
  for (std::size_t i = 0; i < text.footer.size(); ++i) {
    const auto color = i == 0 || i == 3 ? gold : i == 2
        ? (text.reason == MapHudReason::Ready ? fury::Color{130,240,185,255}
                                             : fury::Color{255,195,150,255}) : ink;
    draw_npc_hud_text(r, box.footer_x + 3.f*box.footer_scale,
        box.footer_y + (3.f + 9.f*float(i))*box.footer_scale,
        text.footer[i], color, box.footer_scale, budget);
  }
}

} // namespace vaultline
