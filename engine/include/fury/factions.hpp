#pragma once

#include <algorithm>
#include <string>

namespace fury {

/// Harbor Metro faction stubs (original names — no third-party IP).
enum class FactionId : int {
  PierlineCrew = 0,       // player crew standing
  MetroWatch = 1,         // municipal patrol / cops
  AshcourtSyndicate = 2,  // fence rivals (tension)
  Count = 3
};

inline const char* faction_name(FactionId id) {
  switch (id) {
    case FactionId::PierlineCrew:
      return "Pierline Crew";
    case FactionId::MetroWatch:
      return "Metro Watch";
    case FactionId::AshcourtSyndicate:
      return "Ashcourt Syndicate";
    case FactionId::Count:
      break;
  }
  return "?";
}

inline const char* faction_short(FactionId id) {
  switch (id) {
    case FactionId::PierlineCrew:
      return "Pierline";
    case FactionId::MetroWatch:
      return "MetroWatch";
    case FactionId::AshcourtSyndicate:
      return "Syndicate";
    case FactionId::Count:
      break;
  }
  return "?";
}

/// Reputation ints clamped to [-100, 100] per faction.
struct FactionReputations {
  int pierline{0};    // player crew — heist success raises
  int metro_watch{0}; // cops — heist success lowers; low → faster pursuits
  int syndicate{0};   // fence rivals — selling raises tension

  static constexpr int kMin = -100;
  static constexpr int kMax = 100;

  static int clamp_rep(int v) {
    return (std::max)(kMin, (std::min)(kMax, v));
  }

  int get(FactionId id) const {
    switch (id) {
      case FactionId::PierlineCrew:
        return pierline;
      case FactionId::MetroWatch:
        return metro_watch;
      case FactionId::AshcourtSyndicate:
        return syndicate;
      case FactionId::Count:
        break;
    }
    return 0;
  }

  void set(FactionId id, int v) {
    v = clamp_rep(v);
    switch (id) {
      case FactionId::PierlineCrew:
        pierline = v;
        break;
      case FactionId::MetroWatch:
        metro_watch = v;
        break;
      case FactionId::AshcourtSyndicate:
        syndicate = v;
        break;
      case FactionId::Count:
        break;
    }
  }

  void add(FactionId id, int delta) { set(id, get(id) + delta); }

  /// Successful extract: crew standing up, Metro Watch hostility down.
  void on_heist_success() {
    add(FactionId::PierlineCrew, 8);
    add(FactionId::MetroWatch, -10);
  }

  /// Selling loot at the Ashcourt fence slightly raises Syndicate tension.
  void on_fence_sell() { add(FactionId::AshcourtSyndicate, 3); }

  /// Low Metro Watch standing → pursuits spawn sooner (seconds between spawns).
  float pursuit_spawn_interval() const {
    // Neutral: 0.65s. At -100: ~0.28s. At +100: ~0.85s (slightly slower).
    const float t = static_cast<float>(metro_watch) / 100.f;  // -1..1
    return (std::max)(0.22f, 0.65f + t * 0.28f);
  }

  /// High Pierline standing → Ashcourt shop perk discount (1 = full price).
  float shop_price_mul() const {
    if (pierline >= 70) {
      return 0.75f;  // 25% off
    }
    if (pierline >= 40) {
      return 0.85f;  // 15% off
    }
    if (pierline >= 20) {
      return 0.92f;  // slight courtesy
    }
    return 1.f;
  }

  bool has_shop_discount() const { return pierline >= 20; }

  std::string status_line() const {
    return std::string("reps Pierline=") + std::to_string(pierline) +
           " MetroWatch=" + std::to_string(metro_watch) +
           " Syndicate=" + std::to_string(syndicate);
  }
};

}  // namespace fury
