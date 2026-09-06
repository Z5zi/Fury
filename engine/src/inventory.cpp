#include "fury/inventory.hpp"

#include "fury/log.hpp"

#include <fstream>
#include <sstream>

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

}  // namespace

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
      << "  \"heist_target_index\": " << snap.heist_target_index << "\n"
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
  out_snap = snap;
  Log::info(std::string("Session loaded <- ") + path);
  return true;
}

}  // namespace fury
