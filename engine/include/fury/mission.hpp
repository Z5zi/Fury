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
  float breach_duration{1.8f};
  float loot_duration{4.5f};
};

inline constexpr std::size_t kMissionCount = 4;

inline const MissionJob& mission_job(std::size_t index) {
  // Tuned for 1.0.0+: a Meridian Mutual run is reliably completable in ~2–5 min
  // including walk/drive; core breach+loot is ~6s, escape timeout generous.
  // 1.2.0 adds Harbor Armored Depot (tier 2, short loot).
  static const MissionJob kJobs[kMissionCount] = {
      {"meridian_vault", "Meridian Mutual Vault", "Harbor Metro", 3, 9000, 0,
       1.8f, 4.5f},
      {"crown_jewelry", "Crown & Cutler Safe", "Harbor East", 2, 5500, 2500, 1.6f,
       4.0f},
      {"ashcourt_atm", "Ashcourt Market ATM", "Ashcourt Market", 1, 3500, 0, 1.2f,
       3.0f},
      {"harbor_depot", "Harbor Armored Depot", "Harbor Metro", 2, 6200, 0, 1.5f,
       3.2f},
  };
  return kJobs[index % kMissionCount];
}

/// Simple open/closed mission board (M toggles; 1/2/3/4 selects).
struct MissionBoard {
  bool open{false};
  int selected{0};  // 0..kMissionCount-1

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


/// Quest journal (J) — lists Harbor Metro jobs + persisted completion flags.
struct QuestJournal {
  bool open{false};
  int complete[4]{0, 0, 0, 0};  // 0 incomplete, 1 done (matches kMissionCount)

  void toggle() { open = !open; }

  void mark_complete(int index) {
    if (index >= 0 && index < static_cast<int>(kMissionCount)) {
      complete[index] = 1;
    }
  }

  int completed_count() const {
    int n = 0;
    for (int i = 0; i < static_cast<int>(kMissionCount); ++i) {
      if (complete[i]) ++n;
    }
    return n;
  }

  std::string status_line() const {
    std::string s = open ? "[JOURNAL OPEN] " : "[JOURNAL] ";
    s += std::to_string(completed_count());
    s += "/";
    s += std::to_string(static_cast<int>(kMissionCount));
    s += " complete";
    for (int i = 0; i < static_cast<int>(kMissionCount); ++i) {
      s += " | ";
      s += complete[i] ? "[x] " : "[ ] ";
      s += mission_job(static_cast<std::size_t>(i)).title;
    }
    return s;
  }
};

}  // namespace fury
