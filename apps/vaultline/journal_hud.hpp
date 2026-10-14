#pragma once

#include "npc_dialogue_hud.hpp"
#include <fury/mission.hpp>
#include <fury/i18n.hpp>
#include <array>

namespace vaultline {

// Const J presentation only. Completion, availability, input and saves stay in
// their existing owners. Payout tiers are static metadata, never progress.
inline constexpr std::size_t kJournalHudGlyphBudget = 512;
enum class JournalHudStatus { Incomplete, Complete, Locked };
struct JournalHudRow {
  std::u32string name, status, tier;
  JournalHudStatus state{};
};
struct JournalHudText {
  std::u32string heading, count, rule, close;
  std::array<JournalHudRow, fury::kMissionCount> rows;
};
inline JournalHudText journal_hud_text(fury::Lang lang,
    const fury::QuestJournal& journal, bool finale_locked) {
  const bool es = lang == fury::Lang::Es;
  JournalHudText text;
  text.heading = npc_hud_text(es ? "DIARIO" : "JOURNAL");
  text.count = npc_hud_text(std::string(es ? "COMPLETAS " : "COMPLETE ") +
      std::to_string(journal.completed_count()) + "/" + std::to_string(fury::kMissionCount));
  int core = 0;
  for (int i = 0; i < fury::kFinalePrereqCount; ++i) if (journal.complete[i]) ++core;
  const char* rule = finale_locked
      ? (es ? "FINAL BLOQUEADO / COMPLETA TRABAJOS 1-4: " : "FINAL LOCKED / COMPLETE JOBS 1-4: ")
      : (es ? "FINAL DISPONIBLE / TRABAJOS 1-4 COMPLETOS: " : "FINAL UNLOCKED / JOBS 1-4 COMPLETE: ");
  text.rule = npc_hud_text(std::string(rule) + std::to_string(core) + "/" +
      std::to_string(fury::kFinalePrereqCount));
  text.close = npc_hud_text(es ? "J CERRAR" : "J CLOSE");
  for (std::size_t i = 0; i < text.rows.size(); ++i) {
    auto& row = text.rows[i];
    const bool done = journal.complete[i] != 0;
    const bool locked = static_cast<int>(i) == fury::kFinaleMissionIndex && finale_locked;
    row.state = done ? JournalHudStatus::Complete : locked ? JournalHudStatus::Locked
                                                        : JournalHudStatus::Incomplete;
    row.name = npc_hud_text(std::to_string(i+1) + " " + fury::mission_title_tr(lang, i));
    std::string status = done ? (es ? "COMPLETA" : "COMPLETE")
        : locked ? (es ? "BLOQUEADA" : "LOCKED") : (es ? "NO COMPLETA" : "NOT COMPLETE");
    if (i == 5) status += es ? " / OPCIONAL" : " / OPTIONAL";
    row.status = npc_hud_text(status);
    row.tier = npc_hud_text(std::string(es ? "NIVEL DE PAGO " : "PAYOUT TIER ") +
        std::to_string(fury::mission_job(i).payout_tier));
  }
  return text;
}
inline float journal_hud_text_width(std::u32string_view value) {
  return value.empty() ? 0.f : 6.f * static_cast<float>(value.size()) - 1.f;
}
struct JournalHudLayout {
  float x{}, y{}, width{}, height{}, scale{};
  float row_y(std::size_t i) const { return 32.f + 18.f * static_cast<float>(i); }
};
inline JournalHudLayout layout_journal_hud(int width, int height, float hud_scale) {
  JournalHudLayout box;
  const float desired = std::min(3.f, 2.f * std::clamp(hud_scale, 1.f, 1.6f) *
      std::min(float(width)/640.f, float(height)/360.f));
  box.scale = std::max(0.f, std::min({desired, (width-32.f)/304.f, (height-32.f)/164.f}));
  box.width = 304.f*box.scale; box.height = 164.f*box.scale;
  box.x = (width-box.width)*.5f; box.y = (height-box.height)*.5f;
  return box;
}
inline bool journal_hud_visible(bool open, bool gameplay, bool higher_modal, bool replay) {
  return open && gameplay && !higher_modal && !replay;
}
template<class HudRenderer>
inline void draw_journal_hud(HudRenderer& r, const JournalHudText& text,
    int width, int height, float hud_scale) {
  const auto box = layout_journal_hud(width, height, hud_scale);
  const float s = box.scale;
  if (s <= 0.f) return;
  const fury::Color ink{238,244,250,255}, muted{183,204,210,255}, accent{255,216,130,255};
  r.draw_hud_rect(box.x, box.y, box.width, box.height, {9,19,22,255});
  r.draw_hud_rect(box.x, box.y, box.width, s, accent);
  std::size_t budget = kJournalHudGlyphBudget;
  auto line = [&](float x, float y, std::u32string_view value, fury::Color color) {
    draw_npc_hud_text(r, box.x+x*s, box.y+y*s, value, color, s, budget);
  };
  line(10,8,text.heading,accent);
  line(294-journal_hud_text_width(text.count),8,text.count,ink);
  line(10,20,text.rule,muted);
  for (std::size_t i = 0; i < text.rows.size(); ++i) {
    const auto& row = text.rows[i];
    const float y = box.row_y(i);
    r.draw_hud_rect(box.x+6*s,box.y+(y-1)*s,box.width-12*s,17*s,{23,36,40,255});
    const auto status_ink = row.state == JournalHudStatus::Complete ? fury::Color{144,245,179,255}
        : row.state == JournalHudStatus::Locked ? fury::Color{255,178,164,255} : muted;
    line(10,y,row.name,ink);
    line(10,y+9,row.status,status_ink);
    line(294-journal_hud_text_width(row.tier),y+9,row.tier,accent);
  }
  line(10,152,text.close,accent);
}
} // namespace vaultline
