#pragma once

#include "npc_dialogue_hud.hpp"
#include <fury/inventory.hpp>
#include <fury/i18n.hpp>
#include <array>

namespace vaultline {

// Read-only I presentation. Input, accounting and save ownership stay in main.
inline constexpr std::size_t kInventoryHudGlyphBudget = 256;
struct InventoryHudRow { std::u32string label, value; };
struct InventoryHudText {
  std::u32string heading, chips, close;
  std::array<InventoryHudRow, 4> rows;
};
inline InventoryHudText inventory_hud_text(fury::Lang lang, const fury::Inventory& inventory) {
  const bool es = lang == fury::Lang::Es;
  InventoryHudText text;
  text.heading = npc_hud_text(es ? "INVENTARIO" : "INVENTORY");
  text.chips = npc_hud_text(es ? "FICHAS" : "CHIPS");
  text.close = npc_hud_text(es ? "I CERRAR" : "I CLOSE");
  text.rows[0] = {npc_hud_text(es ? "EFECTIVO" : "CASH"),
                  npc_hud_text("$" + std::to_string(inventory.cash))};
  for (int i = 0; i < 3; ++i) {
    const auto chip = static_cast<fury::LootChip>(i);
    text.rows[static_cast<std::size_t>(i+1)] = {
        npc_hud_text(fury::loot_chip_name(chip)),
        npc_hud_text(std::to_string(inventory.chip_count(chip)))};
  }
  return text;
}
inline float inventory_hud_text_width(std::u32string_view value) {
  return value.empty() ? 0.f : 6.f * static_cast<float>(value.size()) - 1.f;
}
struct InventoryHudLayout {
  float x{}, y{}, width{}, height{}, scale{};
  float row_y(std::size_t i) const { return i == 0 ? 24.f : 52.f + 12.f*float(i-1); }
};
inline InventoryHudLayout layout_inventory_hud(int width, int height, float hud_scale) {
  InventoryHudLayout box;
  box.scale = std::max(0.f, std::min({2.f * std::clamp(hud_scale, 1.f, 1.6f),
      3.f, (width-32.f)/264.f, (height-32.f)/106.f}));
  box.width = 264.f*box.scale; box.height = 106.f*box.scale;
  box.x = (width-box.width)*.5f; box.y = (height-box.height)*.5f;
  return box;
}
inline bool inventory_hud_visible(bool open, bool gameplay, bool higher_modal, bool replay) {
  return open && gameplay && !higher_modal && !replay;
}
template<class HudRenderer>
inline void draw_inventory_hud(HudRenderer& r, const InventoryHudText& text,
    int width, int height, float hud_scale) {
  const auto box = layout_inventory_hud(width, height, hud_scale);
  const float s = box.scale;
  if (s <= 0.f) return;
  const fury::Color ink{238,244,250,255}, muted{183,204,210,255}, accent{255,216,130,255};
  r.draw_hud_rect(box.x, box.y, box.width, box.height, {9,19,22,255});
  r.draw_hud_rect(box.x, box.y, box.width, s, accent);
  std::size_t budget = kInventoryHudGlyphBudget;
  auto line = [&](float x, float y, std::u32string_view value, fury::Color color) {
    draw_npc_hud_text(r, box.x+x*s, box.y+y*s, value, color, s, budget);
  };
  line(10,8,text.heading,accent);
  line(10,40,text.chips,muted);
  for (std::size_t i=0; i<text.rows.size(); ++i) {
    const auto& row=text.rows[i];
    const float y=box.row_y(i);
    r.draw_hud_rect(box.x+6*s,box.y+(y-1)*s,box.width-12*s,9*s,{23,36,40,255});
    line(10,y,row.label,ink);
    line(254-inventory_hud_text_width(row.value),y,row.value,ink);
  }
  line(10,92,text.close,accent);
}
} // namespace vaultline
