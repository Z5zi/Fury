#pragma once

#include "npc_dialogue_hud.hpp"
#include "fury/heist.hpp"
#include "fury/i18n.hpp"

namespace vaultline {

// Presentation only: onboarding history never determines the current job phase.
enum class MissionGuidanceKind { Board, Target, Approach, Breach, Looting, Escape, Success, Failed };
enum class MissionObjectiveCue { None, Board, Target, Escape };

// Read-only presentation of the next interaction. Unknown context never
// advertises a breach; the caller supplies authoritative current selections.
enum class ApproachSecurityTarget { Unknown, None, Badge, Hatch, Tool, Emergency, Terminal };
enum class ApproachInteractionCue {
  Neutral, Breach, Badge, BadgeAndBreaker, Breaker, Tool, ToolBlocksBreach,
  Hatch, HatchBlocksBreach, Emergency, Terminal
};

struct ApproachInteractionContext {
  bool blocked{true};
  bool vehicle{}, door_snap{}, connected_join{};
  bool breach_in_range{}, live_breaker{}, badge_breaker_site0{};
  ApproachSecurityTarget security{ApproachSecurityTarget::Unknown};
};

inline ApproachInteractionCue approach_interaction_cue(const ApproachInteractionContext& c) {
  using Cue = ApproachInteractionCue;
  using Target = ApproachSecurityTarget;
  if (c.blocked || c.vehicle || c.door_snap || c.connected_join) return Cue::Neutral;
  if (c.live_breaker) {
    if (c.security == Target::None) return Cue::Breaker;
    if (c.security == Target::Badge && c.badge_breaker_site0) return Cue::BadgeAndBreaker;
    return Cue::Neutral; // Unlabelled compound actions must not fall through to breach.
  }
  switch (c.security) {
    case Target::None: return c.breach_in_range ? Cue::Breach : Cue::Neutral;
    case Target::Badge: return Cue::Badge;
    case Target::Hatch: return c.breach_in_range ? Cue::HatchBlocksBreach : Cue::Hatch;
    case Target::Tool: return c.breach_in_range ? Cue::ToolBlocksBreach : Cue::Tool;
    case Target::Emergency: return Cue::Emergency;
    case Target::Terminal: return Cue::Terminal;
    case Target::Unknown: return Cue::Neutral;
  }
  return Cue::Neutral;
}

inline const char* approach_interaction_text(fury::Lang lang, ApproachInteractionCue cue) {
  const bool es = lang == fury::Lang::Es;
  switch (cue) {
    case ApproachInteractionCue::Breach:
      return es ? "E: ABRIR OBJETIVO" : "E: BREACH TARGET";
    case ApproachInteractionCue::Badge:
      return es ? "E: TARJETA - ABRIR REJA" : "E: BADGE - OPEN GATE";
    case ApproachInteractionCue::BadgeAndBreaker:
      return es ? "E: TARJETA - ABRIR REJA / APAGAR CAMARAS"
                : "E: BADGE - OPEN GATE / CAMERAS OFF";
    case ApproachInteractionCue::Breaker:
      return es ? "E: INTERRUPTOR - APAGAR CAMARAS" : "E: BREAKER - DISABLE CAMERAS";
    case ApproachInteractionCue::Tool:
      return es ? "E: USAR HERRAMIENTA" : "E: USE VAULT TOOL";
    case ApproachInteractionCue::ToolBlocksBreach:
      return es ? "E: USAR HERRAMIENTA - REUBICATE PARA ABRIR"
                : "E: USE VAULT TOOL - REPOSITION TO BREACH";
    case ApproachInteractionCue::Hatch:
      return es ? "E: USAR ESCOTILLA" : "E: USE HATCH";
    case ApproachInteractionCue::HatchBlocksBreach:
      return es ? "E: USAR ESCOTILLA - REUBICATE PARA ABRIR"
                : "E: USE HATCH - REPOSITION TO BREACH";
    case ApproachInteractionCue::Emergency:
      return es ? "E: PANEL DE EMERGENCIA" : "E: EMERGENCY PANEL";
    case ApproachInteractionCue::Terminal:
      return es ? "E: ACCEDER A TERMINAL DE SEGURIDAD" : "E: ACCESS SECURITY TERMINAL";
    case ApproachInteractionCue::Neutral: break;
  }
  return fury::tr(lang, fury::StrId::TipTarget);
}

struct MissionGuidance {
  MissionGuidanceKind kind;
  MissionObjectiveCue objective;
  int tutorial_pip; // -1 after onboarding completion or while the job is terminal.
};

inline MissionGuidance mission_guidance(fury::HeistPhase phase, int onboard_step) {
  using fury::HeistPhase;
  const bool tutorial = onboard_step >= 0 && onboard_step < 3;
  switch (phase) {
    case HeistPhase::Failed:
      return {MissionGuidanceKind::Failed, MissionObjectiveCue::None, -1};
    case HeistPhase::Success:
      return {MissionGuidanceKind::Success, MissionObjectiveCue::None, -1};
    case HeistPhase::Escape:
      return {MissionGuidanceKind::Escape, MissionObjectiveCue::Escape, tutorial ? 2 : -1};
    case HeistPhase::Looting:
      return {MissionGuidanceKind::Looting, MissionObjectiveCue::Target, tutorial ? 1 : -1};
    case HeistPhase::Breach:
      return {MissionGuidanceKind::Breach, MissionObjectiveCue::Target, tutorial ? 1 : -1};
    case HeistPhase::Approach:
      return {MissionGuidanceKind::Approach, MissionObjectiveCue::Target, tutorial ? 1 : -1};
    case HeistPhase::Idle:
      if (onboard_step == 0)
        return {MissionGuidanceKind::Board, MissionObjectiveCue::Board, 0};
      return {MissionGuidanceKind::Target, MissionObjectiveCue::Target, tutorial ? 1 : -1};
  }
  return {MissionGuidanceKind::Target, MissionObjectiveCue::Target, -1};
}

inline const char* mission_guidance_text(fury::Lang lang, MissionGuidanceKind kind,
    ApproachInteractionCue approach_cue = ApproachInteractionCue::Neutral) {
  const bool es = lang == fury::Lang::Es;
  switch (kind) {
    case MissionGuidanceKind::Board: return fury::tr(lang, fury::StrId::TipBoard);
    case MissionGuidanceKind::Target:
      return es ? "Sigue la brujula al objetivo" : "Follow compass to target";
    case MissionGuidanceKind::Approach: return approach_interaction_text(lang, approach_cue);
    case MissionGuidanceKind::Breach:
      return es ? "Abriendo — quedate cerca del objetivo" : "Breaching — stay near target";
    case MissionGuidanceKind::Looting:
      return es ? "Saqueando — mira el progreso" : "Looting — watch loot meter";
    case MissionGuidanceKind::Escape: return fury::tr(lang, fury::StrId::TipEscape);
    case MissionGuidanceKind::Success:
      return es ? "Trabajo completado — M abre el tablero" : "Job complete — M opens mission board";
    case MissionGuidanceKind::Failed:
      return es ? "Trabajo fallido — M abre el tablero" : "Job failed — M opens mission board";
  }
  return "";
}

template<class HudRenderer>
inline void draw_mission_guidance(HudRenderer& r, const MissionGuidance& guidance,
                                 fury::Lang lang, float width, float height,
                                 bool quality_notice_active,
                                 ApproachInteractionCue approach_cue = ApproachInteractionCue::Neutral) {
  using fury::Color;
  Color background{18, 28, 44, 200};
  if (guidance.objective == MissionObjectiveCue::Target) background = {44, 36, 18, 200};
  if (guidance.kind == MissionGuidanceKind::Escape || guidance.kind == MissionGuidanceKind::Success)
    background = {18, 44, 28, 200};
  if (guidance.kind == MissionGuidanceKind::Failed) background = {44, 18, 18, 200};
  r.draw_hud_rect(width * .5f - 284.f, height - 78.f, 568.f, 28.f, background);
  const auto text = npc_hud_text(mission_guidance_text(lang, guidance.kind, approach_cue));
  std::size_t budget = 44;
  draw_npc_hud_text(r, width * .5f - 264.f, height - 72.f, text,
                    Color{220, 240, 255, 240}, 2.f, budget);
  // F6 shares only this redundant row. Completed tutorials and terminal jobs
  // retain their phase instruction without suggesting tutorial progress.
  if (!quality_notice_active && guidance.tutorial_pip >= 0) {
    for (int i = 0; i < 3; ++i) {
      const bool done = i < guidance.tutorial_pip;
      const bool current = i == guidance.tutorial_pip;
      r.draw_hud_rect(width * .5f - 40.f + float(i) * 28.f, height - 42.f, 20.f, 8.f,
          current ? Color{255, 200, 80, 240}
                  : (done ? Color{80, 200, 120, 220} : Color{50, 60, 80, 180}));
    }
  }
}

} // namespace vaultline
