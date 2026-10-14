#pragma once

#include "npc_dialogue_hud.hpp"
#include "status_hud.hpp"
#include "fury/achievements.hpp"
#include "fury/i18n.hpp"

#include <array>
#include <vector>

namespace vaultline {

// Read-only presentation snapshot. Timers, achievement flags and mission events
// retain their original owners; a hidden notice is never queued or extended.
struct ResultNoticeState {
  float result_timer{};
  bool success{}, finale{};
  float achievement_timer{};
  fury::AchievementId achievement{fury::AchievementId::FirstHeist};
  fury::HeistPhase phase{fury::HeistPhase::Idle};
  fury::Lang language{fury::Lang::En};
};
struct ResultNoticeRect { float x{}, y{}, width{}, height{}; };
inline bool notice_intersects(ResultNoticeRect a, ResultNoticeRect b) {
  return a.x < b.x+b.width && b.x < a.x+a.width &&
         a.y < b.y+b.height && b.y < a.y+a.height;
}
struct ResultNoticeContext {
  int width{}, height{};
  float hud_scale{1.f};
  bool blocked{}, in_vehicle{}, crouching{}, tier_visible{true};
  bool banter{}, objective{}, pursuit{}, alarm{}, guidance{};
  bool door{}, breaker{}, closed_shop{}, interior{}, safehouse{};
  std::vector<ResultNoticeRect> protected_rects;
};
enum class ResultNoticeKind { Result, Achievement };
struct ResultNoticeRow {
  ResultNoticeKind kind{};
  std::u32string text;
  float fade{};
};
struct ResultNoticeLayout {
  ResultNoticeRect rect;
  float glyph{2.f}, row_height{};
  std::vector<ResultNoticeRow> rows;
  bool compact{}, result_suppressed{}, achievement_suppressed{};
};
inline bool result_notice_is_current(const ResultNoticeState& state) {
  return state.phase == (state.success ? fury::HeistPhase::Success : fury::HeistPhase::Failed);
}
inline const char* result_notice_text(const ResultNoticeState& state, bool compact) {
  const bool es = state.language == fury::Lang::Es;
  if (!result_notice_is_current(state)) {
    if (state.success) return es ? "ANTERIOR: COMPLETADO" : "LAST JOB COMPLETE";
    return es ? "ANTERIOR: FALLO" : "LAST JOB FAILED";
  }
  // Retain the authored finale line where space allows. Its compact label is
  // factual, and neither form changes the current phase guide or onboarding.
  if (state.success && state.finale)
    return compact ? (es ? "FINAL COMPLETADO" : "FINALE COMPLETE") : "Pierline holds the Harbor";
  return state.success ? (es ? "TRABAJO COMPLETADO" : "JOB COMPLETE")
                       : (es ? "TRABAJO FALLIDO" : "JOB FAILED");
}
inline float result_notice_text_width(std::u32string_view text, float glyph) {
  return text.empty() ? 0.f : (float(text.size())*6.f-1.f)*glyph;
}
inline std::vector<ResultNoticeRect> result_notice_obstacles(const ResultNoticeContext& c) {
  const auto status = layout_status_hud(c.width,c.height,c.hud_scale);
  const auto aux = layout_status_auxiliary(status,c.width,c.in_vehicle);
  const float W=float(c.width), H=float(c.height);
  // Reserve actual status content, not its intentionally generous blank backing.
  std::vector<ResultNoticeRect> out{{16,16,340.f*status.scale,status.ready_y-16.f},
    {W-166,16,150,150},{16,aux.ready_y,220,22},{W-56,230,40,40},
    {16,H-40,160,24},{W-130,H-40,114,24}};
  if(c.in_vehicle)out.push_back({16,152+aux.offset,180,54});
  if(c.tier_visible)out.push_back({aux.tier_x,aux.tier_y,200,18});
  if(c.crouching)out.push_back({aux.crouch_x,aux.crouch_y,120,18});
  if(c.banter)out.push_back({W*.5f-260,H-148,520,26});
  if(c.objective)out.push_back({W*.5f-130,H-118,260,24});
  if(c.pursuit)out.push_back({W-120,178,56,40});
  if(c.alarm)out.push_back({W-56,178,40,40});
  if(c.guidance)out.push_back({W*.5f-284,H-78,568,28});
  if(c.door)out.push_back({W*.5f-90,H-118,180,34});
  else if(c.breaker)out.push_back({W*.5f-110,H-118,220,34});
  else if(c.closed_shop)out.push_back({W*.5f-130,H-118,260,34});
  if(c.interior&&!c.door)out.push_back({W*.5f-60,H-112,120,18});
  if(c.safehouse)out.push_back({W*.5f-200,H-178,400,42});
  out.insert(out.end(),c.protected_rects.begin(),c.protected_rects.end());
  return out;
}
inline ResultNoticeLayout layout_result_notice(const ResultNoticeState& state,
                                               const ResultNoticeContext& c) {
  ResultNoticeLayout box;
  const bool result=state.result_timer>0.f, achievement=state.achievement_timer>0.f;
  box.result_suppressed=result;box.achievement_suppressed=achievement;
  if(c.blocked||(!result&&!achievement)||c.width<64||c.height<64)return box;
  std::vector<ResultNoticeRow> rows;
  if(result)rows.push_back({ResultNoticeKind::Result,npc_hud_text(result_notice_text(state,false)),
                            std::clamp(state.result_timer/.4f,0.f,1.f)});
  if(achievement)rows.push_back({ResultNoticeKind::Achievement,
      npc_hud_text(fury::achievement_title(state.achievement)),
      std::clamp(state.achievement_timer/.45f,0.f,1.f)});
  const auto obstacles=result_notice_obstacles(c);
  auto clear=[&](ResultNoticeRect r) {
    if(r.x<8||r.y<8||r.x+r.width>c.width-8||r.y+r.height>c.height-8)return false;
    for(const auto& o:obstacles)if(notice_intersects(r,o))return false;
    return true;
  };
  auto width_for=[&](float glyph) {
    float width=0;for(const auto& row:rows)width=std::max(width,result_notice_text_width(row.text,glyph));
    return std::ceil(width+28.f);
  };
  auto accept=[&](ResultNoticeRect rect,float glyph,float row_height,
                  const std::vector<ResultNoticeRow>& visible,bool compact) {
    box.rect=rect;box.glyph=glyph;box.row_height=row_height;box.rows=visible;box.compact=compact;
    for(const auto& row:visible) {
      if(row.kind==ResultNoticeKind::Result)box.result_suppressed=false;
      else box.achievement_suppressed=false;
    }
  };
  const float hs=std::clamp(c.hud_scale,1.f,1.6f);
  const float left=16.f+340.f*hs+8.f,right=float(c.width)-174.f;
  const float glyph=std::min(3.f,2.f*hs),row_height=std::ceil(7.f*glyph+12.f);
  const float wide_width=width_for(glyph);
  ResultNoticeRect wide{left+(right-left-wide_width)*.5f,16,wide_width,row_height*float(rows.size())};
  if(wide_width<=right-left&&clear(wide)) {accept(wide,glyph,row_height,rows,false);return box;}
  const bool terminal=state.phase==fury::HeistPhase::Failed||state.phase==fury::HeistPhase::Success;
  ResultNoticeRect footer{(c.width-width_for(2.f))*.5f,float(c.height)-122.f,
                          width_for(2.f),20.f*float(rows.size())};
  if(terminal&&clear(footer)) {accept(footer,2.f,20.f,rows,false);return box;}

  // One complete readable row fits the narrow gap. Current terminal result is
  // already stated by status/guidance, so prefer the unique achievement title.
  // After reset prioritize the historical result until its ORIGINAL timer ends.
  auto selected=rows.front();
  if(achievement&&(!result||result_notice_is_current(state)))selected=rows.back();
  if(selected.kind==ResultNoticeKind::Result)
    selected.text=npc_hud_text(result_notice_text(state,true));
  const auto status=layout_status_hud(c.width,c.height,c.hud_scale);
  const float compact_width=std::min(268.f,float(c.width)-128.f-244.f);
  ResultNoticeRect compact{244,std::max(178.f,status.ready_y),compact_width,26};
  if(result_notice_text_width(selected.text,2.f)+28.f<=compact_width&&clear(compact))
    accept(compact,2.f,26.f,{selected},true);
  // No legal slot: explicit per-frame suppression; no state write or queue.
  return box;
}

template<class HudRenderer>
inline void draw_result_notice(HudRenderer& r,const ResultNoticeState& state,
                                const ResultNoticeLayout& box) {
  std::size_t budget=128;
  for(std::size_t i=0;i<box.rows.size();++i) {
    const auto& row=box.rows[i];const float y=box.rect.y+float(i)*box.row_height;
    const bool achievement=row.kind==ResultNoticeKind::Achievement;
    const auto alpha=[&](float a){return static_cast<std::uint8_t>(a*row.fade);};
    const fury::Color accent=achievement?fury::Color{255,210,90,alpha(240)}:
        (state.success?fury::Color{90,255,140,alpha(240)}:fury::Color{255,100,100,alpha(240)});
    r.draw_hud_rect(box.rect.x,y,box.rect.width,box.row_height,{9,15,24,alpha(235)});
    r.draw_hud_rect(box.rect.x,y,3,box.row_height,accent);
    const float text_y=y+(box.row_height-7.f*box.glyph)*.5f;
    draw_npc_hud_text(r,box.rect.x+14,text_y,row.text,{238,244,250,alpha(255)},box.glyph,budget);
    if(achievement) {
      // The old six-pip achievement ID remains, now with its complete title.
      for(int p=0;p<int(fury::AchievementId::Count);++p)
        r.draw_hud_rect(box.rect.x+14.f+float(p)*6.f,y+box.row_height-2.f,4,2,
            p==int(state.achievement)?accent:fury::Color{60,80,110,alpha(160)});
    }
  }
}
} // namespace vaultline
