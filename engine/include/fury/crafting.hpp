#pragma once

#include "fury/inventory.hpp"

#include <algorithm>

#include <string>

namespace fury {

/// Craft recipes at the Harbor loft workbench (Vaultline 3.6.0).
enum class CraftRecipe : int {
  SignalJammer = 0,  // BearerBond + LedgerDrive → passive camera-heat damp
  SmokePellet = 1,   // Sapphire + BearerBond → one-shot heat dump
  Count = 2
};

inline const char* craft_recipe_name(CraftRecipe id) {
  switch (id) {
    case CraftRecipe::SignalJammer: return "SignalJammer";
    case CraftRecipe::SmokePellet: return "SmokePellet";
    case CraftRecipe::Count: break;
  }
  return "?";
}

inline const char* craft_recipe_blurb(CraftRecipe id) {
  switch (id) {
    case CraftRecipe::SignalJammer: return "Cuts camera heat while owned";
    case CraftRecipe::SmokePellet: return "Instant heat drop (consumable)";
    case CraftRecipe::Count: break;
  }
  return "";
}

/// Crafted gear (persist in save). Jammer is 0/1 owned; pellets stack.
struct CraftInventory {
  int signal_jammer{0};
  int smoke_pellet{0};

  /// Camera heat multiplier while a jammer is owned.
  float camera_heat_mul() const {
    return signal_jammer > 0 ? 0.45f : 1.f;
  }

  bool can_craft_jammer(const Inventory& inv) const {
    return signal_jammer <= 0 && inv.chip_count(LootChip::BearerBond) >= 1 &&
           inv.chip_count(LootChip::LedgerDrive) >= 1;
  }

  bool can_craft_smoke(const Inventory& inv) const {
    return inv.chip_count(LootChip::Sapphire) >= 1 &&
           inv.chip_count(LootChip::BearerBond) >= 1;
  }

  /// Craft SignalJammer from 1 BearerBond + 1 LedgerDrive. Returns true on success.
  bool try_craft_jammer(Inventory& inv) {
    if (signal_jammer > 0) {
      return false;
    }
    if (inv.chip_count(LootChip::BearerBond) < 1 ||
        inv.chip_count(LootChip::LedgerDrive) < 1) {
      return false;
    }
    inv.take_chip(LootChip::BearerBond, 1);
    inv.take_chip(LootChip::LedgerDrive, 1);
    signal_jammer = 1;
    return true;
  }

  /// Craft SmokePellet from 1 Sapphire + 1 BearerBond.
  bool try_craft_smoke(Inventory& inv) {
    if (inv.chip_count(LootChip::Sapphire) < 1 ||
        inv.chip_count(LootChip::BearerBond) < 1) {
      return false;
    }
    inv.take_chip(LootChip::Sapphire, 1);
    inv.take_chip(LootChip::BearerBond, 1);
    ++smoke_pellet;
    return true;
  }

  /// Consume one SmokePellet — dumps heat toward 0. Returns true if used.
  bool try_use_smoke(float& heat_value) {
    if (smoke_pellet <= 0) {
      return false;
    }
    --smoke_pellet;
    heat_value = (std::max)(0.f, heat_value - 0.55f);
    return true;
  }

  std::string status_line() const {
    std::string s = "[CRAFT] jammer=";
    s += signal_jammer ? "yes" : "no";
    s += " smoke=";
    s += std::to_string(smoke_pellet);
    return s;
  }
};

struct CraftPanel {
  bool open{false};
};

/// Permanent Ashcourt fence unlocks (cash, one-time).
struct FenceUpgrades {
  bool better_payouts{false};  // +10% job payout
  bool quieter_tools{false};   // shorter breach; stronger with Silent Entry

  static constexpr int kBetterPayoutsCost = 8000;
  static constexpr int kQuieterToolsCost = 6500;

  float payout_mul() const { return better_payouts ? 1.10f : 1.f; }

  /// Extra breach duration mul (stacks with skill Silent Entry).
  float quieter_breach_mul(bool silent_entry) const {
    if (!quieter_tools) {
      return 1.f;
    }
    // Synergy: Silent Entry + Quieter Tools punches breach further.
    return silent_entry ? 0.78f : 0.90f;
  }

  std::string status_line() const {
    std::string s = "[UPGRADE] payouts=";
    s += better_payouts ? "on" : "off";
    s += " quieter=";
    s += quieter_tools ? "on" : "off";
    return s;
  }
};

}  // namespace fury
