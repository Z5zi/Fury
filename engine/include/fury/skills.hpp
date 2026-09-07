#pragma once

#include <string>

namespace fury {

/// Skill tree stub (Vaultline 2.6.0) — XP from heists; 3 nodes, 1 rank each.
enum class SkillId : int {
  SilentEntry = 0,    // shorter breach
  FastHands = 1,      // faster loot
  CoolUnderHeat = 2,  // slower heat rise
  Count = 3
};

inline const char* skill_name(SkillId id) {
  switch (id) {
    case SkillId::SilentEntry: return "Silent Entry";
    case SkillId::FastHands: return "Fast Hands";
    case SkillId::CoolUnderHeat: return "Cool Under Heat";
    case SkillId::Count: break;
  }
  return "?";
}

inline const char* skill_blurb(SkillId id) {
  switch (id) {
    case SkillId::SilentEntry: return "Shorter breach window";
    case SkillId::FastHands: return "Faster loot countdown";
    case SkillId::CoolUnderHeat: return "Heat rises slower";
    case SkillId::Count: break;
  }
  return "";
}

/// Persistent operator skills + unspent XP (panel N; 1/2/3 unlock).
struct SkillTree {
  int xp{0};
  int ranks[static_cast<int>(SkillId::Count)]{0, 0, 0};

  static constexpr int kMaxRank = 1;
  static constexpr int kUnlockCost = 100;

  int rank(SkillId id) const {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(SkillId::Count)) {
      return 0;
    }
    return ranks[i];
  }

  bool unlocked(SkillId id) const { return rank(id) > 0; }

  bool can_unlock(SkillId id) const {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(SkillId::Count)) {
      return false;
    }
    return ranks[i] < kMaxRank && xp >= kUnlockCost;
  }

  /// Spend XP to unlock one rank. Returns true on success.
  bool try_unlock(SkillId id) {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(SkillId::Count)) {
      return false;
    }
    if (ranks[i] >= kMaxRank || xp < kUnlockCost) {
      return false;
    }
    xp -= kUnlockCost;
    ranks[i] = 1;
    return true;
  }

  void add_xp(int amount) {
    if (amount > 0) {
      xp += amount;
    }
  }

  /// Breach duration multiplier (Silent Entry → shorter).
  float breach_duration_mul() const {
    return unlocked(SkillId::SilentEntry) ? 0.72f : 1.f;
  }

  /// Extra loot-speed multiplier (Fast Hands).
  float loot_speed_mul() const {
    return unlocked(SkillId::FastHands) ? 1.18f : 1.f;
  }

  /// Heat rise multiplier (Cool Under Heat → slower).
  float heat_rise_mul() const {
    return unlocked(SkillId::CoolUnderHeat) ? 0.78f : 1.f;
  }

  /// XP granted on successful extract (tier 1..4).
  static int xp_for_tier(int payout_tier) {
    const int t = payout_tier < 1 ? 1 : (payout_tier > 4 ? 4 : payout_tier);
    return 35 + t * 25;  // 60 / 85 / 110 / 135
  }

  std::string status_line() const {
    std::string s = "[SKILLS] xp=";
    s += std::to_string(xp);
    for (int i = 0; i < static_cast<int>(SkillId::Count); ++i) {
      s += " | ";
      s += skill_name(static_cast<SkillId>(i));
      s += ranks[i] ? " [x]" : " [ ]";
    }
    return s;
  }
};

struct SkillPanel {
  bool open{false};
};

}  // namespace fury
