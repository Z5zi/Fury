#pragma once

#include <cstddef>
#include <string>

namespace fury {

/// Contract / mission board entry for Vaultline (original Harbor Metro jobs).
struct MissionJob {
  const char* id{""};
  const char* title{""};
  const char* district{""};
  int payout_tier{1};   // 1..3 display tier
  int base_payout{0};
  int jewelry_bonus{0};
  float breach_duration{2.5f};
  float loot_duration{7.f};
};

inline constexpr std::size_t kMissionCount = 3;

inline const MissionJob& mission_job(std::size_t index) {
  static const MissionJob kJobs[kMissionCount] = {
      {"meridian_vault", "Meridian Mutual Vault", "Harbor Metro", 3, 10000, 0,
       2.5f, 7.f},
      {"crown_jewelry", "Crown & Cutler Safe", "Harbor East", 2, 6500, 3500, 2.5f,
       7.f},
      {"ashcourt_atm", "Ashcourt Market ATM", "Ashcourt Market", 1, 4200, 0, 1.8f,
       4.5f},
  };
  return kJobs[index % kMissionCount];
}

/// Simple open/closed mission board (M toggles; 1/2/3 selects).
struct MissionBoard {
  bool open{false};
  int selected{0};  // 0..2

  void toggle() { open = !open; }

  /// Returns true if selection changed.
  bool select(int index) {
    if (index < 0 || index >= static_cast<int>(kMissionCount)) {
      return false;
    }
    if (selected == index) {
      return false;
    }
    selected = index;
    return true;
  }

  const MissionJob& current() const {
    return mission_job(static_cast<std::size_t>(selected));
  }

  std::string status_line() const {
    const MissionJob& j = current();
    std::string s = open ? "[BOARD OPEN] " : "[BOARD] ";
    s += "sel=";
    s += std::to_string(selected + 1);
    s += " ";
    s += j.title;
    s += " tier=";
    s += std::to_string(j.payout_tier);
    s += " $";
    s += std::to_string(j.base_payout + j.jewelry_bonus);
    return s;
  }
};

}  // namespace fury
