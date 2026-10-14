#pragma once

#include "npc_dialogue_hud.hpp"
#include "fury/i18n.hpp"
#include "fury/heist.hpp"
#include "fury/mission.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace vaultline {

// Presentation only. Selection, prerequisites, targets and input stay in the app.
// Keep the short-label font cap unchanged; share the existing bounded HUD path.
inline constexpr std::size_t kMissionBoardGlyphBudget = 384;

struct MissionBoardHudText {
  std::u32string heading, tier, prerequisite, objective, controls;
  std::array<std::u32string, fury::kMissionCount> names, states;
};

inline MissionBoardHudText mission_board_hud_text(fury::Lang lang,
    int selected, bool finale_locked, fury::HeistPhase phase) {
  MissionBoardHudText text;
  const bool es = lang == fury::Lang::Es;
  text.heading = npc_hud_text(es ? "TABLERO DE MISIONES" : "MISSION BOARD");
  text.tier = npc_hud_text(es ? "NIVEL" : "TIER");
  text.prerequisite = npc_hud_text(finale_locked
      ? (es ? "Final bloqueado: completa trabajos 1-4" : "Finale locked: complete jobs 1-4 first")
      : (es ? "Final disponible" : "Finale available"));
  using fury::HeistPhase;
  const bool can_retarget = phase == HeistPhase::Idle || phase == HeistPhase::Success ||
      phase == HeistPhase::Failed;
  switch (phase) {
    case HeistPhase::Escape:
      text.objective = npc_hud_text(fury::tr(lang, fury::StrId::TipEscape)); break;
    case HeistPhase::Success:
      text.objective = npc_hud_text(fury::tr(lang, fury::StrId::TipDone)); break;
    case HeistPhase::Breach:
      text.objective = npc_hud_text(es ? "FASE: ACCESO" : "PHASE: BREACH"); break;
    case HeistPhase::Looting:
      text.objective = npc_hud_text(es ? "FASE: SAQUEO" : "PHASE: LOOTING"); break;
    case HeistPhase::Failed:
      text.objective = npc_hud_text(es ? "FASE: FALLO" : "PHASE: FAILED"); break;
    case HeistPhase::Idle:
    case HeistPhase::Approach:
      text.objective = npc_hud_text(fury::tr(lang, fury::StrId::TipTarget)); break;
  }
  text.controls = npc_hud_text(can_retarget
      ? (es ? "1-6 seleccionar / M cerrar" : "1-6 select / M close")
      : (es ? "1-6 bloqueado en mision / M cerrar" : "1-6 locked during job / M close"));
  for (std::size_t i = 0; i < fury::kMissionCount; ++i) {
    text.names[i] = npc_hud_text(std::to_string(i + 1) + " " + fury::mission_title_tr(lang, i));
    if (int(i) == fury::kFinaleMissionIndex && finale_locked)
      text.states[i] = npc_hud_text(es ? "BLOQUEADA" : "LOCKED");
    else if (int(i) == selected)
      text.states[i] = npc_hud_text(es ? "SELECCIONADA" : "SELECTED");
  }
  return text;
}

struct MissionBoardHudLayout {
  float x{}, y{}, width{}, height{}, scale{};
  float row_y(std::size_t row) const { return y + (22.f + 14.f * float(row)) * scale; }
  float row_height() const { return 12.f * scale; }
};

inline MissionBoardHudLayout layout_mission_board_hud(int width, int height, float hud_scale) {
  MissionBoardHudLayout box;
  // At 640x360 the complete chooser uses 608x296px with 14px-high glyphs.
  // Larger windows allow 21px glyphs. Viewport fit wins over HUD enlargement.
  const float desired = std::min(3.f, 2.f * std::clamp(hud_scale, 1.f, 1.6f) *
      std::min(float(width) / 640.f, float(height) / 360.f));
  box.scale = std::max(0.f, std::min({desired, (width - 32.f) / 304.f,
                                                   (height - 32.f) / 148.f}));
  box.width = 304.f * box.scale;
  box.height = 148.f * box.scale;
  box.x = (width - box.width) * .5f;
  box.y = (height - box.height) * .5f;
  return box;
}

inline float mission_board_text_width(std::u32string_view text, float scale) {
  return text.empty() ? 0.f : (6.f * float(text.size()) - 1.f) * scale;
}

template<class HudRenderer>
inline void draw_mission_board_hud(HudRenderer& r, const fury::MissionBoard& board,
    bool finale_locked, fury::Lang lang, fury::HeistPhase phase,
    int width, int height, float hud_scale) {
  const auto box = layout_mission_board_hud(width, height, hud_scale);
  if (box.scale <= 0.f) return;
  const auto text = mission_board_hud_text(lang, board.selected, finale_locked, phase);
  const float s = box.scale;
  const fury::Color ink{235, 241, 250, 255}, muted{175, 195, 215, 255};
  const fury::Color accent{255, 220, 125, 255}, lock_ink{245, 185, 155, 255};
  std::size_t budget = kMissionBoardGlyphBudget;
  auto line = [&](float x, float y, std::u32string_view value, fury::Color color) {
    draw_npc_hud_text(r, x, y, value, color, s, budget);
  };
  r.draw_hud_rect(box.x, box.y, box.width, box.height, {9, 15, 25, 255});
  r.draw_hud_rect(box.x, box.y, box.width, s, accent);
  line(box.x + 10.f*s, box.y + 8.f*s, text.heading, accent);
  line(box.x + box.width - 10.f*s - mission_board_text_width(text.tier, s),
       box.y + 8.f*s, text.tier, muted);
  for (std::size_t i = 0; i < fury::kMissionCount; ++i) {
    const bool selected = int(i) == board.selected;
    const bool locked = int(i) == fury::kFinaleMissionIndex && finale_locked;
    const float y = box.row_y(i);
    const fury::Color fill = selected ? fury::Color{38, 60, 86, 255}
        : locked ? fury::Color{40, 29, 31, 255} : fury::Color{21, 31, 45, 255};
    r.draw_hud_rect(box.x + 6.f*s, y, box.width - 12.f*s, box.row_height(), fill);
    if (selected) {
      r.draw_hud_rect(box.x + 6.f*s, y, s, box.row_height(), accent);
      r.draw_hud_rect(box.x + 6.f*s, y, box.width - 12.f*s, s, accent);
      r.draw_hud_rect(box.x + 6.f*s, y + box.row_height() - s, box.width - 12.f*s, s, accent);
    }
    line(box.x + 10.f*s, y + 2.f*s, text.names[i], locked ? lock_ink : ink);
    line(box.x + box.width - 10.f*s - mission_board_text_width(text.states[i], s),
         y + 2.f*s, text.states[i], locked ? lock_ink : accent);
    // Four small tier pips retain the existing secondary payout-tier information.
    for (int tier = 0; tier < 4; ++tier) {
      r.draw_hud_rect(box.x + box.width - (29.f - 5.f*float(tier))*s,
          y + 10.f*s, 4.f*s, s,
          tier < fury::mission_job(i).payout_tier ? muted : fury::Color{50, 65, 80, 255});
    }
  }
  line(box.x + 10.f*s, box.y + 110.f*s, text.prerequisite, finale_locked ? lock_ink : muted);
  line(box.x + 10.f*s, box.y + 122.f*s, text.objective, muted);
  line(box.x + 10.f*s, box.y + 134.f*s, text.controls, accent);
}

} // namespace vaultline
