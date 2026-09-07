#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace fury {

/// Achievement stub (Vaultline 4.7.0) — unlock banners + flags in save.
enum class AchievementId : int {
  FirstHeist = 0,    // first successful extract
  StealthAtm = 1,    // Ashcourt ATM with peak heat <= 0.50
  FinaleClear = 2,   // Meridian Night Vault finale
  Millionaire = 3,   // lifetime cash earned >= 1,000,000
  TenHeists = 4,     // 10 successful extracts
  FirstFail = 5,     // first failed heist
  Count = 6
};

inline const char* achievement_title(AchievementId id) {
  switch (id) {
    case AchievementId::FirstHeist: return "First Score";
    case AchievementId::StealthAtm: return "Quiet Withdrawal";
    case AchievementId::FinaleClear: return "Night Vault Cleared";
    case AchievementId::Millionaire: return "Harbor Millionaire";
    case AchievementId::TenHeists: return "Career Operator";
    case AchievementId::FirstFail: return "Lesson Learned";
    case AchievementId::Count: break;
  }
  return "?";
}

inline const char* achievement_blurb(AchievementId id) {
  switch (id) {
    case AchievementId::FirstHeist: return "Complete your first Harbor extract";
    case AchievementId::StealthAtm: return "Finish Ashcourt ATM with peak heat <= 0.50";
    case AchievementId::FinaleClear: return "Clear Meridian Night Vault finale";
    case AchievementId::Millionaire: return "Earn $1,000,000 lifetime cash";
    case AchievementId::TenHeists: return "Complete 10 successful heists";
    case AchievementId::FirstFail: return "Fail a heist (and live to try again)";
    case AchievementId::Count: break;
  }
  return "";
}

/// Persistent unlock flags (0/1) mirrored into SessionSnapshot.
struct AchievementFlags {
  int unlocked[static_cast<int>(AchievementId::Count)]{0, 0, 0, 0, 0, 0};

  bool is_unlocked(AchievementId id) const {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(AchievementId::Count)) {
      return false;
    }
    return unlocked[i] != 0;
  }

  /// Returns true if this call newly unlocked the achievement.
  bool try_unlock(AchievementId id) {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(AchievementId::Count)) {
      return false;
    }
    if (unlocked[i] != 0) {
      return false;
    }
    unlocked[i] = 1;
    return true;
  }

  int unlocked_count() const {
    int n = 0;
    for (int i = 0; i < static_cast<int>(AchievementId::Count); ++i) {
      n += unlocked[i] ? 1 : 0;
    }
    return n;
  }
};

/// Short unlock banner (geometric HUD bars — no bitmap fonts).
struct AchievementBanner {
  float timer{0.f};
  AchievementId id{AchievementId::FirstHeist};

  void trigger(AchievementId unlocked_id) {
    id = unlocked_id;
    timer = 3.4f;
  }

  void update(float dt) {
    if (timer > 0.f) {
      timer = (std::max)(0.f, timer - dt);
    }
  }

  bool active() const { return timer > 0.f; }
};

/// Lifetime career stats (4.7.0) — heists / cash / distance / time.
struct LifetimeStats {
  int heists{0};                 // successful extracts
  int cash_earned{0};            // lifetime cash (mirrors score card)
  float distance_walked{0.f};    // meters (on-foot XZ)
  float time_played{0.f};        // seconds

  void add_distance(float meters) {
    if (meters > 0.f && meters < 8.f) {
      distance_walked += meters;
    }
  }

  void add_time(float dt) {
    if (dt > 0.f && dt < 1.f) {
      time_played += dt;
    }
  }

  int distance_m() const {
    return static_cast<int>(distance_walked);
  }

  int time_sec() const { return static_cast<int>(time_played); }

  std::string status_line() const {
    const int t = time_sec();
    const int hh = t / 3600;
    const int mm = (t % 3600) / 60;
    const int ss = t % 60;
    return std::string("Stats: heists=") + std::to_string(heists) +
           " cash=$" + std::to_string(cash_earned) + " walked=" +
           std::to_string(distance_m()) + "m time=" + std::to_string(hh) +
           "h" + std::to_string(mm) + "m" + std::to_string(ss) + "s";
  }
};

struct CareerStatsPanel {
  bool open{false};
};
/// Alias kept for call sites preferring StatsPanel naming.
using StatsPanel = CareerStatsPanel;

}  // namespace fury
