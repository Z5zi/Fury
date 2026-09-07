#pragma once

#include <cstddef>
#include <string>

namespace fury {

/// Named loot chips (fence-sellable extras from mission loot tables).
enum class LootChip : int {
  BearerBond = 0,
  Sapphire = 1,
  LedgerDrive = 2,
  Count = 3
};

inline const char* loot_chip_name(LootChip chip) {
  switch (chip) {
    case LootChip::BearerBond: return "BearerBond";
    case LootChip::Sapphire: return "Sapphire";
    case LootChip::LedgerDrive: return "LedgerDrive";
    case LootChip::Count: break;
  }
  return "?";
}

/// Fence sell price per chip unit (Ashcourt Market).
inline int loot_chip_sell_price(LootChip chip) {
  switch (chip) {
    case LootChip::BearerBond: return 1200;
    case LootChip::Sapphire: return 2000;
    case LootChip::LedgerDrive: return 2800;
    case LootChip::Count: break;
  }
  return 0;
}

/// Player loot / cash for the Vaultline slice.
struct Inventory {
  int cash{0};
  int loot_bags{0};
  int jewelry{0};
  /// Persistent chip counts (survive jobs; sell at Ashcourt fence).
  int chips[static_cast<int>(LootChip::Count)]{0, 0, 0};

  void clear_carry() {
    loot_bags = 0;
    jewelry = 0;
  }

  int carry_value() const { return loot_bags * 2500 + jewelry * 800; }

  int chip_count(LootChip chip) const {
    const int i = static_cast<int>(chip);
    if (i < 0 || i >= static_cast<int>(LootChip::Count)) {
      return 0;
    }
    return chips[i];
  }

  void add_chip(LootChip chip, int n) {
    if (n <= 0) {
      return;
    }
    const int i = static_cast<int>(chip);
    if (i < 0 || i >= static_cast<int>(LootChip::Count)) {
      return;
    }
    chips[i] += n;
  }

  /// Removes up to n chips; returns how many were removed.
  int take_chip(LootChip chip, int n) {
    if (n <= 0) {
      return 0;
    }
    const int i = static_cast<int>(chip);
    if (i < 0 || i >= static_cast<int>(LootChip::Count)) {
      return 0;
    }
    const int take = chips[i] < n ? chips[i] : n;
    chips[i] -= take;
    return take;
  }

  int total_chips() const {
    int n = 0;
    for (int i = 0; i < static_cast<int>(LootChip::Count); ++i) {
      n += chips[i];
    }
    return n;
  }
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

/// Weighted loot-table entry: cash bonus or a named chip.
struct LootTableEntry {
  enum class Kind : int { Cash = 0, Chip = 1 };
  Kind kind{Kind::Cash};
  LootChip chip{LootChip::BearerBond};
  int weight{1};
  int amount_min{1};
  int amount_max{1};
};

struct LootTable {
  const LootTableEntry* entries{nullptr};
  int entry_count{0};
  int rolls{2};  // weighted picks per successful extract
};

/// Per-mission loot table (cash + chips with rarity weights).
const LootTable& mission_loot_table(std::size_t mission_index);

/// Roll the mission loot table into inventory (cash bonuses + chips).
/// Returns total cash granted from table rolls.
int roll_mission_loot(std::size_t mission_index, Inventory& inv);

/// Minimal session snapshot written as local JSON (no third-party JSON lib).
struct SessionSnapshot {
  std::string session_id{"local"};
  std::string world{"Harbor Metro"};
  std::string player_name{"Operator"};
  int cash{0};
  int successes{0};
  int failures{0};
  int lifetime_score{0};
  int heist_target_index{0};  // 0 Meridian, 1 Crown, 2 Ashcourt ATM, 3 Harbor Depot
  int perk_crew{0};
  int perk_heat_damp{0};
  int perk_loot_speed{0};
  int save_slot{0};
  /// Per-mission completion flags (0/1) for quest journal (4 Harbor Metro jobs).
  int mission_complete[4]{0, 0, 0, 0};
  /// Persistent named loot chips (1.5.0).
  int item_bearer_bond{0};
  int item_sapphire{0};
  int item_ledger_drive{0};
};

bool save_session_json(const std::string& path, const SessionSnapshot& snap);
bool load_session_json(const std::string& path, SessionSnapshot& out);

/// Path helper: vaultline_session_slot{N}.json (N = 0..2).
inline std::string session_slot_path(int slot) {
  return "vaultline_session_slot" + std::to_string(slot) + ".json";
}

}  // namespace fury
