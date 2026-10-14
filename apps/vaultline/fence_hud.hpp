#pragma once

#include "npc_dialogue_hud.hpp"
#include <fury/crafting.hpp>
#include <fury/factions.hpp>
#include <fury/i18n.hpp>
#include <array>
#include <iomanip>
#include <sstream>

namespace vaultline {

// A const presentation snapshot. Input, transactions, timers and saves belong
// to main.cpp. No changes to the shared short-label cap or dialogue budget.
inline constexpr std::size_t kFenceHudGlyphBudget = 768;
inline constexpr std::array<int, 3> kFenceHudPerkBases{3500, 4500, 4000};
enum class FenceHudReason { Ready, Closed, Approach, Maximum, Owned, Cash, Empty };
struct FenceHudRow {
  std::u32string detail, reason;
  FenceHudReason state{};
  int amount{}; // Next charged price for an available tier, or fixed sale proceeds.
  bool has_price{}, selected{};
};
struct FenceHudText {
  std::u32string heading, cash, availability, discount, sales, controls, close;
  std::array<FenceHudRow, 8> rows;
};
inline const char* fence_hud_reason(FenceHudReason state, bool es) {
  switch (state) {
    case FenceHudReason::Ready: return es ? "LISTO" : "READY";
    case FenceHudReason::Closed: return es ? "CERRADO" : "CLOSED";
    case FenceHudReason::Approach: return es ? "ACERCATE" : "APPROACH SHOP";
    case FenceHudReason::Maximum: return "MAX";
    case FenceHudReason::Owned: return es ? "POSEIDO" : "OWNED";
    case FenceHudReason::Cash: return es ? "FALTA DINERO" : "NEED CASH";
    case FenceHudReason::Empty: return es ? "SIN STOCK" : "EMPTY";
  }
  return "?";
}
inline FenceHudText fence_hud_text(fury::Lang lang, const fury::Inventory& inventory,
    const fury::PlayerPerks& perks, const fury::FenceUpgrades& upgrades,
    const fury::FactionReputations& reputation, int selected, bool shop_open,
    bool near_shop, float distance, float radius) {
  FenceHudText text;
  const bool es = lang == fury::Lang::Es;
  text.heading = npc_hud_text(es ? "PERISTA ASHCOURT" : "ASHCOURT FENCE");
  text.cash = npc_hud_text("$" + std::to_string(inventory.cash));
  std::ostringstream position;
  position << (shop_open ? (es ? "ABIERTO" : "OPEN") : (es ? "CERRADO" : "CLOSED"))
      << (es ? " / DISTANCIA " : " / DISTANCE ");
  // Distance is descriptive; the exact existing range boolean decides eligibility.
  if (std::isfinite(distance)) {
    if (distance < 10000.f) position << std::fixed << std::setprecision(1) << distance;
    else position << std::scientific << std::setprecision(1) << distance;
  } else position << "?";
  position << " / " << (es ? "RADIO " : "RANGE ") << std::fixed << std::setprecision(1) << radius;
  text.availability = npc_hud_text(position.str());
  const float multiplier = reputation.shop_price_mul();
  const int discount = static_cast<int>(std::lround((1.f - multiplier) * 100.f));
  text.discount = npc_hud_text(std::string(es ? "DESCUENTO COMPRAS " : "PURCHASE DISCOUNT ") +
      std::to_string(discount) + (es ? " POR CIENTO" : " PERCENT"));
  const char* names_en[] = {"Crew perk", "Heat dampener", "Loot speed", "Better Payouts", "Quieter Tools"};
  const char* names_es[] = {"Apoyo de equipo", "Menos calor", "Saqueo rapido", "Mejores pagos", "Herramientas sigilosas"};
  const int levels[] = {perks.crew, perks.heat_damp, perks.loot_speed};
  using R = FenceHudReason;
  for (int i = 0; i < 5; ++i) {
    auto& row = text.rows[static_cast<std::size_t>(i)];
    const bool maximum = i < 3 && levels[i] >= 3;
    const bool owned = i == 3 ? upgrades.better_payouts : i == 4 && upgrades.quieter_tools;
    std::string detail = std::to_string(i+1) + " " + (es ? names_es[i] : names_en[i]);
    if (i < 3) detail += " " + std::to_string(levels[i]) + "/3";
    row.has_price = !maximum && !owned;
    if (row.has_price) {
      // The bases and float/rounding expression are bound to unchanged owners by
      // the focused source and price parity tests. Never calculate a MAX next tier.
      const int base = i < 3 ? kFenceHudPerkBases[static_cast<std::size_t>(i)] * (levels[i] + 1)
          : i == 3 ? fury::FenceUpgrades::kBetterPayoutsCost : fury::FenceUpgrades::kQuieterToolsCost;
      row.amount = static_cast<int>(static_cast<float>(base) * multiplier + .5f);
      detail += " $" + std::to_string(row.amount);
    } else detail += maximum ? " MAX" : (es ? " POSEIDO" : " OWNED");
    row.state = !shop_open ? R::Closed : !near_shop ? R::Approach : maximum ? R::Maximum
        : owned ? R::Owned : inventory.cash < row.amount ? R::Cash : R::Ready;
    row.detail = npc_hud_text(detail);
    row.reason = npc_hud_text(fence_hud_reason(row.state, es));
  }
  text.sales = npc_hud_text(es ? "S: VENDER FICHA ELEGIDA" : "S: SELL SELECTED CHIP");
  for (int i = 0; i < 3; ++i) {
    auto& row = text.rows[static_cast<std::size_t>(i+5)];
    const auto chip = static_cast<fury::LootChip>(i);
    const int count = inventory.chip_count(chip);
    row.selected = i == selected;
    row.has_price = true;
    row.amount = fury::loot_chip_sell_price(chip);
    row.detail = npc_hud_text(std::string(row.selected ? "S " : "  ") + fury::loot_chip_name(chip) +
        " x" + std::to_string(count) + " $" + std::to_string(row.amount));
    row.state = !shop_open ? R::Closed : !near_shop ? R::Approach : count <= 0 ? R::Empty : R::Ready;
    row.reason = npc_hud_text(fence_hud_reason(row.state, es));
  }
  text.controls = npc_hud_text(es ? "1-5 COMPRAR / IZQ-DER ELEGIR" : "1-5 BUY / LEFT-RIGHT SELECT");
  text.close = npc_hud_text(es ? "S VENDER UNO / B CERRAR" : "S SELL ONE / B CLOSE");
  return text;
}
inline float fence_hud_text_width(std::u32string_view value) {
  return value.empty() ? 0.f : 6.f * static_cast<float>(value.size()) - 1.f;
}
struct FenceHudLayout {
  float x{}, y{}, width{}, height{}, scale{};
  float row_y(std::size_t i) const { return y + (i < 5 ? 40.f + 11.f*float(i) : 106.f + 11.f*float(i-5))*scale; }
};
inline FenceHudLayout layout_fence_hud(int width, int height, float hud_scale) {
  FenceHudLayout box;
  const float desired = std::min(3.f, 2.f * std::clamp(hud_scale, 1.f, 1.6f) *
      std::min(float(width)/640.f, float(height)/360.f));
  box.scale = std::max(0.f, std::min({desired, (width-32.f)/304.f, (height-32.f)/164.f}));
  box.width = 304.f*box.scale; box.height = 164.f*box.scale;
  box.x = (width-box.width)*.5f; box.y = (height-box.height)*.5f;
  return box;
}
inline bool fence_hud_visible(bool open, bool gameplay, bool higher_modal, bool replay) {
  return open && gameplay && !higher_modal && !replay;
}
template<class HudRenderer>
inline void draw_fence_hud(HudRenderer& r, const FenceHudText& text,
    int width, int height, float hud_scale) {
  const auto box = layout_fence_hud(width, height, hud_scale);
  const float s = box.scale;
  if (s <= 0.f) return;
  const fury::Color ink{238,244,250,255}, muted{183,204,210,255}, accent{255,216,130,255};
  r.draw_hud_rect(box.x, box.y, box.width, box.height, {9,19,22,255});
  r.draw_hud_rect(box.x, box.y, box.width, s, accent);
  std::size_t budget = kFenceHudGlyphBudget;
  auto line = [&](float x, float y, std::u32string_view value, fury::Color color) {
    draw_npc_hud_text(r, box.x+x*s, box.y+y*s, value, color, s, budget);
  };
  line(10,8,text.heading,accent);
  line(294-fence_hud_text_width(text.cash),8,text.cash,ink);
  line(10,18,text.availability,muted);
  line(10,28,text.discount,muted);
  for (std::size_t i=0; i<text.rows.size(); ++i) {
    const auto& row=text.rows[i];
    const float y=box.row_y(i);
    r.draw_hud_rect(box.x+6*s,y,box.width-12*s,9*s,
        row.selected ? fury::Color{47,65,42,255} : fury::Color{23,36,40,255});
    if (row.selected) r.draw_hud_rect(box.x+6*s,y,s,9*s,accent);
    // One measured flowing identity/value string plus a right-aligned reason.
    // No fixed price column: full signed counts and long ES names retain room.
    const float ty=(y-box.y)/s+1;
    line(10,ty,row.detail,ink);
    line(294-fence_hud_text_width(row.reason),ty,row.reason,
        row.state == FenceHudReason::Ready ? accent : muted);
  }
  line(10,96,text.sales,accent);
  line(10,143,text.controls,accent);
  line(10,153,text.close,accent);
}
} // namespace vaultline
