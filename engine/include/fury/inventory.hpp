#pragma once

#include <string>

namespace fury {

/// Player loot / cash for the Vaultline slice.
struct Inventory {
  int cash{0};
  int loot_bags{0};
  int jewelry{0};

  void clear_carry() {
    loot_bags = 0;
    jewelry = 0;
  }

  int carry_value() const { return loot_bags * 2500 + jewelry * 800; }
};

struct HeistScoreCard {
  int base_payout{0};
  int speed_bonus{0};
  int stealth_bonus{0};  // reserved; stub stays 0 for now
  int failures{0};
  int successes{0};
  int lifetime_cash{0};

  int last_total() const { return base_payout + speed_bonus + stealth_bonus; }
};

/// Purchased Ashcourt fence perks (persistent across jobs / save slots).
struct PlayerPerks {
  int crew{0};         // extra crew loot-speed contribution
  int heat_damp{0};    // reduces heat rise
  int loot_speed{0};   // base loot countdown multiplier bump

  float loot_mul() const { return 1.f + 0.12f * static_cast<float>(loot_speed); }
  float crew_mul() const { return 1.f + 0.10f * static_cast<float>(crew); }
  float heat_rise_mul() const {
    return 1.f / (1.f + 0.35f * static_cast<float>(heat_damp));
  }
};

/// Minimal session snapshot written as local JSON (no third-party JSON lib).
struct SessionSnapshot {
  std::string session_id{"local"};
  std::string world{"Harbor Metro"};
  std::string player_name{"Operator"};
  int cash{0};
  int successes{0};
  int failures{0};
  int lifetime_score{0};
  int heist_target_index{0};  // 0 Meridian, 1 Crown, 2 Ashcourt ATM
  int perk_crew{0};
  int perk_heat_damp{0};
  int perk_loot_speed{0};
  int save_slot{0};
};

bool save_session_json(const std::string& path, const SessionSnapshot& snap);
bool load_session_json(const std::string& path, SessionSnapshot& out);

/// Path helper: vaultline_session_slot{N}.json (N = 0..2).
inline std::string session_slot_path(int slot) {
  return "vaultline_session_slot" + std::to_string(slot) + ".json";
}

}  // namespace fury
