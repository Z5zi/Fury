#pragma once

/// Mid-loot heist complications + rare Syndicate Enforcer boss stub (3.8.0).
/// Original Harbor Metro only — no Rockstar / GTA IP.

#include <cstdint>
#include <algorithm>

namespace fury {

enum class ComplicationKind : int {
  None = 0,
  PowerFlicker,    // lights dim briefly
  ExtraGuard,      // spawn an extra chasing guard near vault
  LockJam,         // pause loot progress ~1.5s
  CivilianCallIn,  // heat spike (someone called it in)
  Count
};

inline const char* complication_name(ComplicationKind k) {
  switch (k) {
    case ComplicationKind::PowerFlicker:
      return "Power flicker";
    case ComplicationKind::ExtraGuard:
      return "Extra guard";
    case ComplicationKind::LockJam:
      return "Lock jam";
    case ComplicationKind::CivilianCallIn:
      return "Civilian call-in";
    default:
      return "None";
  }
}

inline const char* complication_tip(ComplicationKind k) {
  switch (k) {
    case ComplicationKind::PowerFlicker:
      return "Lights dim — power flicker";
    case ComplicationKind::ExtraGuard:
      return "Extra guard inbound";
    case ComplicationKind::LockJam:
      return "Lock jam — loot paused";
    case ComplicationKind::CivilianCallIn:
      return "Civilian call-in — heat spike";
    default:
      return "";
  }
}

/// Lightweight mid-loot event roller for Vaultline.
struct HeistComplications {
  float next_roll_in{2.2f};
  float flicker_remaining{0.f};  // ambient dim timer
  float tip_timer{0.f};
  ComplicationKind tip_kind{ComplicationKind::None};
  ComplicationKind last_kind{ComplicationKind::None};
  int events_this_loot{0};
  bool extra_guard_alive{false};
  bool enforcer_alive{false};
  bool enforcer_checked{false};  // rolled spawn once per loot

  void reset_run() {
    next_roll_in = 2.2f;
    flicker_remaining = 0.f;
    tip_timer = 0.f;
    tip_kind = ComplicationKind::None;
    last_kind = ComplicationKind::None;
    events_this_loot = 0;
    extra_guard_alive = false;
    enforcer_alive = false;
    enforcer_checked = false;
  }

  void on_leave_loot() {
    next_roll_in = 2.2f;
    flicker_remaining = 0.f;
    events_this_loot = 0;
    // tip may keep showing briefly
  }

  void trigger_tip(ComplicationKind k, float seconds = 2.8f) {
    tip_kind = k;
    tip_timer = seconds;
    last_kind = k;
  }

  /// Advance timers. When looting, may roll a random complication.
  /// Returns the new event kind (or None). Caller applies side effects.
  ComplicationKind update(float dt, bool looting, std::uint32_t& rng) {
    if (tip_timer > 0.f) {
      tip_timer = (std::max)(0.f, tip_timer - dt);
    }
    if (flicker_remaining > 0.f) {
      flicker_remaining = (std::max)(0.f, flicker_remaining - dt);
    }
    if (!looting) {
      return ComplicationKind::None;
    }
    // Cap events per loot so Meridian stays completable.
    if (events_this_loot >= 2) {
      return ComplicationKind::None;
    }
    next_roll_in -= dt;
    if (next_roll_in > 0.f) {
      return ComplicationKind::None;
    }
    rng = rng * 1664525u + 1013904223u;
    const float u =
        static_cast<float>((rng >> 8) & 0xffffffu) / 16777215.f;
    // ~38% chance to fire when the timer elapses; else re-arm.
    if (u > 0.38f) {
      next_roll_in = 1.6f + u * 2.4f;
      return ComplicationKind::None;
    }
    rng = rng * 1664525u + 1013904223u;
    const unsigned pick = (rng >> 16) % 4u;
    ComplicationKind kind = ComplicationKind::None;
    switch (pick) {
      case 0:
        kind = ComplicationKind::PowerFlicker;
        break;
      case 1:
        kind = ComplicationKind::ExtraGuard;
        break;
      case 2:
        kind = ComplicationKind::LockJam;
        break;
      default:
        kind = ComplicationKind::CivilianCallIn;
        break;
    }
    // Don't re-spawn extra guard if one is already out.
    if (kind == ComplicationKind::ExtraGuard && extra_guard_alive) {
      kind = ComplicationKind::LockJam;
    }
    ++events_this_loot;
    next_roll_in = 3.2f + u * 2.8f;
    trigger_tip(kind);
    if (kind == ComplicationKind::PowerFlicker) {
      flicker_remaining = 1.35f;
    }
    return kind;
  }

  /// Rare boss stub on high-tier jobs (tier >= 3). Call once when entering Looting.
  bool try_spawn_enforcer(int payout_tier, std::uint32_t& rng) {
    if (enforcer_checked || enforcer_alive) {
      return false;
    }
    enforcer_checked = true;
    if (payout_tier < 3) {
      return false;
    }
    rng = rng * 1664525u + 1013904223u;
    const float u =
        static_cast<float>((rng >> 8) & 0xffffffu) / 16777215.f;
    // ~22% on tier 3, ~35% on finale (tier 4+)
    const float chance = payout_tier >= 4 ? 0.35f : 0.22f;
    if (u > chance) {
      return false;
    }
    enforcer_alive = true;
    return true;
  }

  float flicker_dim01() const {
    if (flicker_remaining <= 0.f) {
      return 0.f;
    }
    // Pulse dim: strongest mid-flicker
    const float t = flicker_remaining / 1.35f;
    const float pulse = t * (1.f - t) * 4.f;  // 0..1 peak
    return std::clamp(0.35f + 0.65f * pulse, 0.f, 1.f);
  }
};

}  // namespace fury
