#include "fury/inventory.hpp"

#include "fury/log.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

namespace fury {
namespace {

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

bool extract_string(const std::string& src, const char* key, std::string& out) {
  const std::string needle = std::string("\"") + key + "\"";
  const auto pos = src.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  const auto colon = src.find(':', pos + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  const auto q1 = src.find('"', colon + 1);
  if (q1 == std::string::npos) {
    return false;
  }
  const auto q2 = src.find('"', q1 + 1);
  if (q2 == std::string::npos) {
    return false;
  }
  out = src.substr(q1 + 1, q2 - q1 - 1);
  return true;
}

bool extract_int(const std::string& src, const char* key, int& out) {
  const std::string needle = std::string("\"") + key + "\"";
  const auto pos = src.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  const auto colon = src.find(':', pos + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  std::size_t i = colon + 1;
  while (i < src.size() && (src[i] == ' ' || src[i] == '\t')) {
    ++i;
  }
  if (i >= src.size()) {
    return false;
  }
  const bool neg = src[i] == '-';
  if (neg) {
    ++i;
  }
  if (i >= src.size() || src[i] < '0' || src[i] > '9') {
    return false;
  }
  int v = 0;
  while (i < src.size() && src[i] >= '0' && src[i] <= '9') {
    v = v * 10 + (src[i] - '0');
    ++i;
  }
  out = neg ? -v : v;
  return true;
}

int rand_inclusive(int lo, int hi) {
  if (hi <= lo) {
    return lo;
  }
  return lo + (std::rand() % (hi - lo + 1));
}

const LootTableEntry* pick_weighted(const LootTable& table) {
  if (!table.entries || table.entry_count <= 0) {
    return nullptr;
  }
  int total = 0;
  for (int i = 0; i < table.entry_count; ++i) {
    total += (table.entries[i].weight > 0) ? table.entries[i].weight : 0;
  }
  if (total <= 0) {
    return &table.entries[0];
  }
  int roll = std::rand() % total;
  for (int i = 0; i < table.entry_count; ++i) {
    const int w = (table.entries[i].weight > 0) ? table.entries[i].weight : 0;
    if (roll < w) {
      return &table.entries[i];
    }
    roll -= w;
  }
  return &table.entries[table.entry_count - 1];
}

}  // namespace

const LootTable& mission_loot_table(std::size_t mission_index) {
  // Rarity weights: higher = more common. Cash fills tables; chips are scarcer.
  // 0 Meridian Mutual — high-tier vault: LedgerDrive more likely
  static const LootTableEntry kMeridian[] = {
      {LootTableEntry::Kind::Cash, LootChip::BearerBond, 40, 400, 1200},
      {LootTableEntry::Kind::Chip, LootChip::BearerBond, 28, 1, 2},
      {LootTableEntry::Kind::Chip, LootChip::Sapphire, 18, 1, 1},
      {LootTableEntry::Kind::Chip, LootChip::LedgerDrive, 14, 1, 1},
  };
  // 1 Crown & Cutler — jewelry front: Sapphire weighted
  static const LootTableEntry kCrown[] = {
      {LootTableEntry::Kind::Cash, LootChip::BearerBond, 35, 200, 800},
      {LootTableEntry::Kind::Chip, LootChip::BearerBond, 22, 1, 1},
      {LootTableEntry::Kind::Chip, LootChip::Sapphire, 32, 1, 2},
      {LootTableEntry::Kind::Chip, LootChip::LedgerDrive, 11, 1, 1},
  };
  // 2 Ashcourt ATM — low tier: mostly cash + occasional bond
  static const LootTableEntry kAtm[] = {
      {LootTableEntry::Kind::Cash, LootChip::BearerBond, 55, 100, 450},
      {LootTableEntry::Kind::Chip, LootChip::BearerBond, 30, 1, 1},
      {LootTableEntry::Kind::Chip, LootChip::Sapphire, 12, 1, 1},
      {LootTableEntry::Kind::Chip, LootChip::LedgerDrive, 3, 1, 1},
  };
  // 3 Harbor Armored Depot — mid tier: bonds + drives
  static const LootTableEntry kDepot[] = {
      {LootTableEntry::Kind::Cash, LootChip::BearerBond, 38, 250, 900},
      {LootTableEntry::Kind::Chip, LootChip::BearerBond, 30, 1, 2},
      {LootTableEntry::Kind::Chip, LootChip::Sapphire, 16, 1, 1},
      {LootTableEntry::Kind::Chip, LootChip::LedgerDrive, 16, 1, 1},
  };
  // 4 Meridian Night Vault finale — premium chips + cash
  static const LootTableEntry kNightVault[] = {
      {LootTableEntry::Kind::Cash, LootChip::BearerBond, 30, 800, 2200},
      {LootTableEntry::Kind::Chip, LootChip::BearerBond, 22, 1, 3},
      {LootTableEntry::Kind::Chip, LootChip::Sapphire, 24, 1, 2},
      {LootTableEntry::Kind::Chip, LootChip::LedgerDrive, 24, 1, 2},
  };
  // 5 North Quay Container Yard — tier 1 heist-lite (mostly cash + bond)
  static const LootTableEntry kNorthQuay[] = {
      {LootTableEntry::Kind::Cash, LootChip::BearerBond, 58, 80, 400},
      {LootTableEntry::Kind::Chip, LootChip::BearerBond, 28, 1, 1},
      {LootTableEntry::Kind::Chip, LootChip::Sapphire, 10, 1, 1},
      {LootTableEntry::Kind::Chip, LootChip::LedgerDrive, 4, 1, 1},
  };

  static const LootTable kTables[] = {
      {kMeridian, 4, 3},
      {kCrown, 4, 2},
      {kAtm, 4, 2},
      {kDepot, 4, 3},
      {kNightVault, 4, 4},
      {kNorthQuay, 4, 2},
  };
  constexpr std::size_t kCount = sizeof(kTables) / sizeof(kTables[0]);
  return kTables[mission_index % kCount];
}

int roll_mission_loot(std::size_t mission_index, Inventory& inv) {
  const LootTable& table = mission_loot_table(mission_index);
  int cash_gained = 0;
  for (int r = 0; r < table.rolls; ++r) {
    const LootTableEntry* e = pick_weighted(table);
    if (!e) {
      continue;
    }
    const int amt = rand_inclusive(e->amount_min, e->amount_max);
    if (amt <= 0) {
      continue;
    }
    if (e->kind == LootTableEntry::Kind::Cash) {
      inv.cash += amt;
      cash_gained += amt;
      Log::info(std::string("Loot drop: cash +$") + std::to_string(amt));
    } else {
      inv.add_chip(e->chip, amt);
      Log::info(std::string("Loot drop: ") + loot_chip_name(e->chip) + " x" +
                std::to_string(amt));
    }
  }
  return cash_gained;
}

bool save_session_json(const std::string& path, const SessionSnapshot& snap) {
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    Log::warn(std::string("save_session_json failed to open ") + path);
    return false;
  }
  out << "{\n"
      << "  \"session_id\": \"" << json_escape(snap.session_id) << "\",\n"
      << "  \"world\": \"" << json_escape(snap.world) << "\",\n"
      << "  \"player_name\": \"" << json_escape(snap.player_name) << "\",\n"
      << "  \"cash\": " << snap.cash << ",\n"
      << "  \"successes\": " << snap.successes << ",\n"
      << "  \"failures\": " << snap.failures << ",\n"
      << "  \"lifetime_score\": " << snap.lifetime_score << ",\n"
      << "  \"heist_target_index\": " << snap.heist_target_index << ",\n"
      << "  \"perk_crew\": " << snap.perk_crew << ",\n"
      << "  \"perk_heat_damp\": " << snap.perk_heat_damp << ",\n"
      << "  \"perk_loot_speed\": " << snap.perk_loot_speed << ",\n"
      << "  \"save_slot\": " << snap.save_slot << ",\n"
      << "  \"mission_complete_0\": " << snap.mission_complete[0] << ",\n"
      << "  \"mission_complete_1\": " << snap.mission_complete[1] << ",\n"
      << "  \"mission_complete_2\": " << snap.mission_complete[2] << ",\n"
      << "  \"mission_complete_3\": " << snap.mission_complete[3] << ",\n"
      << "  \"mission_complete_4\": " << snap.mission_complete[4] << ",\n"
      << "  \"mission_complete_5\": " << snap.mission_complete[5] << ",\n"
      << "  \"item_bearer_bond\": " << snap.item_bearer_bond << ",\n"
      << "  \"item_sapphire\": " << snap.item_sapphire << ",\n"
      << "  \"item_ledger_drive\": " << snap.item_ledger_drive << ",\n"
      << "  \"rep_pierline\": " << snap.rep_pierline << ",\n"
      << "  \"rep_metro_watch\": " << snap.rep_metro_watch << ",\n"
      << "  \"rep_syndicate\": " << snap.rep_syndicate << ",\n"
      << "  \"skill_xp\": " << snap.skill_xp << ",\n"
      << "  \"skill_silent_entry\": " << snap.skill_silent_entry << ",\n"
      << "  \"skill_fast_hands\": " << snap.skill_fast_hands << ",\n"
      << "  \"skill_cool_under_heat\": " << snap.skill_cool_under_heat << ",\n"
      << "  \"daily_claim_ymd\": " << snap.daily_claim_ymd << ",\n"
      << "  \"item_signal_jammer\": " << snap.item_signal_jammer << ",\n"
      << "  \"item_smoke_pellet\": " << snap.item_smoke_pellet << ",\n"
      << "  \"upgrade_better_payouts\": " << snap.upgrade_better_payouts << ",\n"
      << "  \"upgrade_quieter_tools\": " << snap.upgrade_quieter_tools << "\n"
      << "}\n";
  if (!out) {
    Log::warn("save_session_json write error");
    return false;
  }
  Log::info(std::string("Session saved -> ") + path);
  return true;
}

bool load_session_json(const std::string& path, SessionSnapshot& out_snap) {
  std::ifstream in(path);
  if (!in) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();
  SessionSnapshot snap = out_snap;
  extract_string(src, "session_id", snap.session_id);
  extract_string(src, "world", snap.world);
  extract_string(src, "player_name", snap.player_name);
  extract_int(src, "cash", snap.cash);
  extract_int(src, "successes", snap.successes);
  extract_int(src, "failures", snap.failures);
  extract_int(src, "lifetime_score", snap.lifetime_score);
  extract_int(src, "heist_target_index", snap.heist_target_index);
  extract_int(src, "perk_crew", snap.perk_crew);
  extract_int(src, "perk_heat_damp", snap.perk_heat_damp);
  extract_int(src, "perk_loot_speed", snap.perk_loot_speed);
  extract_int(src, "save_slot", snap.save_slot);
  extract_int(src, "mission_complete_0", snap.mission_complete[0]);
  extract_int(src, "mission_complete_1", snap.mission_complete[1]);
  extract_int(src, "mission_complete_2", snap.mission_complete[2]);
  extract_int(src, "mission_complete_3", snap.mission_complete[3]);
  extract_int(src, "mission_complete_4", snap.mission_complete[4]);
  extract_int(src, "mission_complete_5", snap.mission_complete[5]);
  extract_int(src, "item_bearer_bond", snap.item_bearer_bond);
  extract_int(src, "item_sapphire", snap.item_sapphire);
  extract_int(src, "item_ledger_drive", snap.item_ledger_drive);
  extract_int(src, "rep_pierline", snap.rep_pierline);
  extract_int(src, "rep_metro_watch", snap.rep_metro_watch);
  extract_int(src, "rep_syndicate", snap.rep_syndicate);
  extract_int(src, "skill_xp", snap.skill_xp);
  extract_int(src, "skill_silent_entry", snap.skill_silent_entry);
  extract_int(src, "skill_fast_hands", snap.skill_fast_hands);
  extract_int(src, "skill_cool_under_heat", snap.skill_cool_under_heat);
  extract_int(src, "daily_claim_ymd", snap.daily_claim_ymd);
  extract_int(src, "item_signal_jammer", snap.item_signal_jammer);
  extract_int(src, "item_smoke_pellet", snap.item_smoke_pellet);
  extract_int(src, "upgrade_better_payouts", snap.upgrade_better_payouts);
  extract_int(src, "upgrade_quieter_tools", snap.upgrade_quieter_tools);
  out_snap = snap;
  Log::info(std::string("Session loaded <- ") + path);
  return true;
}

namespace {

bool ensure_directory(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  struct stat st {};
  if (::stat(path.c_str(), &st) == 0) {
#ifdef _WIN32
    return (st.st_mode & _S_IFDIR) != 0;
#else
    return S_ISDIR(st.st_mode);
#endif
  }
#ifdef _WIN32
  return _mkdir(path.c_str()) == 0;
#else
  if (::mkdir(path.c_str(), 0755) == 0) {
    return true;
  }
  // Race: another process created it
  if (errno == EEXIST && ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }
  return false;
#endif
}

std::string join_cloud_path(std::string dir, const std::string& filename) {
  while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) {
    dir.pop_back();
  }
  if (dir.empty()) {
    return filename;
  }
#ifdef _WIN32
  return dir + "\\" + filename;
#else
  return dir + "/" + filename;
#endif
}

}  // namespace

bool mirror_session_to_cloud_dir(const std::string& filename,
                                 const SessionSnapshot& snap) {
  const char* cloud = std::getenv("FURY_CLOUD_DIR");
  if (!cloud || cloud[0] == '\0') {
    return false;
  }
  const std::string dir = cloud;
  if (!ensure_directory(dir)) {
    Log::warn(std::string("FURY_CLOUD_DIR mirror: cannot create/open ") + dir);
    return false;
  }
  const std::string dest = join_cloud_path(dir, filename);
  if (!save_session_json(dest, snap)) {
    Log::warn(std::string("FURY_CLOUD_DIR mirror failed -> ") + dest);
    return false;
  }
  Log::info(std::string("Cloud stub mirrored -> ") + dest);
  return true;
}

}  // namespace fury
