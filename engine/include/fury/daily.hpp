#pragma once

#include <ctime>
#include <string>

namespace fury {

/// One rotating daily bonus objective (hash of local date). Vaultline 2.6.0.
struct DailyContractDef {
  const char* id{""};
  const char* title{""};
  const char* blurb{""};  // e.g. finish ATM without heat>0.5
  int mission_index{0};   // required job index
  float max_heat{0.5f};   // peak heat during run must be <= this
  int cash_bonus{2000};
};

inline int calendar_ymd_local() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  return (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
}

inline unsigned hash_ymd(int ymd) {
  unsigned h = static_cast<unsigned>(ymd) * 2654435761u;
  h ^= h >> 16;
  h *= 0x7feb352du;
  h ^= h >> 15;
  h *= 0x846ca68bu;
  h ^= h >> 16;
  return h;
}

inline const DailyContractDef& daily_contract_for_ymd(int ymd) {
  // Original Harbor Metro dailies — no third-party IP.
  static const DailyContractDef kPool[] = {
      {"quiet_atm", "Quiet ATM", "Finish Ashcourt ATM with peak heat <= 0.50", 2,
       0.50f, 1800},
      {"clean_meridian", "Clean Meridian",
       "Finish Meridian Mutual with peak heat <= 0.40", 0, 0.40f, 2800},
      {"cool_crown", "Cool Crown", "Finish Crown & Cutler with peak heat <= 0.55",
       1, 0.55f, 2200},
      {"shadow_depot", "Shadow Depot",
       "Finish Harbor Depot with peak heat <= 0.45", 3, 0.45f, 2400},
      {"quay_quiet", "Quay Quiet",
       "Finish North Quay Yard with peak heat <= 0.50", 5, 0.50f, 1600},
  };
  constexpr int kCount = static_cast<int>(sizeof(kPool) / sizeof(kPool[0]));
  const unsigned idx = hash_ymd(ymd) % static_cast<unsigned>(kCount);
  return kPool[idx];
}

inline const DailyContractDef& todays_daily() {
  return daily_contract_for_ymd(calendar_ymd_local());
}

/// Runtime tracker + claim state (persisted via SessionSnapshot).
struct DailyContracts {
  int claim_ymd{0};  // YYYYMMDD when bonus was claimed (0 = never)

  const DailyContractDef& today() const { return todays_daily(); }

  bool claimed_today() const { return claim_ymd == calendar_ymd_local(); }

  bool matches_run(int mission_index, float peak_heat) const {
    const DailyContractDef& d = today();
    return mission_index == d.mission_index && peak_heat <= d.max_heat + 1e-4f;
  }

  /// On successful extract: if objective met and not yet claimed, grant bonus.
  /// Returns cash granted (0 if none).
  int try_claim_on_success(int mission_index, float peak_heat) {
    if (claimed_today()) {
      return 0;
    }
    if (!matches_run(mission_index, peak_heat)) {
      return 0;
    }
    claim_ymd = calendar_ymd_local();
    return today().cash_bonus;
  }

  std::string status_line() const {
    const DailyContractDef& d = today();
    std::string s = claimed_today() ? "[DAILY DONE] " : "[DAILY] ";
    s += d.title;
    s += " — ";
    s += d.blurb;
    s += " (+$";
    s += std::to_string(d.cash_bonus);
    s += ")";
    return s;
  }
};

}  // namespace fury
