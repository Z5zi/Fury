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

/// Minimal session snapshot written as local JSON (no third-party JSON lib).
struct SessionSnapshot {
  std::string session_id{"local"};
  std::string world{"Harbor Metro"};
  std::string player_name{"Operator"};
  int cash{0};
  int successes{0};
  int failures{0};
  int lifetime_score{0};
  int heist_target_index{0};  // 0 = Meridian Mutual, 1 = jewelry stub
};

bool save_session_json(const std::string& path, const SessionSnapshot& snap);
bool load_session_json(const std::string& path, SessionSnapshot& out);

}  // namespace fury
