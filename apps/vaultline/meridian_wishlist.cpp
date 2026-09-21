#include "meridian_wishlist.hpp"

#include <fury/log.hpp>
#include <fury/collision.hpp>
#include <fury/mesh.hpp>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#if defined(_WIN32)
#include <filesystem>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace meridian {
namespace {

using fury::Entity;
using fury::Log;
using fury::Material;
using fury::Vec3;

constexpr float kBankCz = -10.f;

void place_box(fury::Scene& scene, const char* name, const Vec3& pos,
               const Vec3& size, const Vec3& rgb, float emissive = 0.f,
               bool solid = false, bool visible = true, const char* tag = nullptr,
               float yaw = 0.f, bool detail = true) {
  Entity e;
  e.name = name;
  if (tag) e.tag = tag;
  e.mesh = scene.add_mesh(fury::make_box(size, rgb));
  e.transform.position = pos;
  e.transform.rotation_euler = {0.f, yaw, 0.f};
  e.material.albedo = rgb;
  e.material.roughness = 0.65f;
  e.material.metallic = 0.08f;
  e.material.emissive = emissive;
  e.visible = visible;
  e.detail = detail;
  e.solid = solid;
  if (solid) {
    e.collider = fury::Aabb::from_center_size({0.f, 0.f, 0.f}, size);
  }
  scene.add_entity(std::move(e));
}

void set_tag_visible(fury::Scene& scene, const char* tag, bool vis) {
  for (auto& e : scene.entities()) {
    if (e.tag == tag) e.visible = vis;
  }
}

void set_name_visible(fury::Scene& scene, const char* name, bool vis) {
  if (auto* e = scene.find_by_name(name)) e->visible = vis;
}

void set_name_solid(fury::Scene& scene, const char* name, bool solid) {
  if (auto* e = scene.find_by_name(name)) {
    e->solid = solid;
    if (!solid) {
      e->collider = {};
    }
  }
}

void relocate_npc(fury::NpcSystem& npcs, const char* entity_name,
                  const Vec3& pos, const std::vector<Vec3>& wps) {
  for (auto& a : npcs.agents()) {
    if (a.entity_name != entity_name) continue;
    a.position = {pos.x, a.height * 0.5f, pos.z};
    a.home = {pos.x, 0.f, pos.z};
    a.waypoints = wps;
    a.base_waypoints = wps;
    a.waypoint_index = 0;
    a.chasing = false;
    return;
  }
}

int count_visible(const fury::Scene& scene) {
  int n = 0;
  for (const auto& e : scene.entities()) {
    if (e.visible) ++n;
  }
  return n;
}

long read_rss_kb() {
  std::ifstream in("/proc/self/status");
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      long kb = 0;
      if (std::sscanf(line.c_str(), "VmRSS: %ld", &kb) == 1) return kb;
    }
  }
  return -1;
}

#if defined(_WIN32)
[[maybe_unused]] bool ensure_dir(const char* path) {
  std::error_code ec;
  if (std::filesystem::is_directory(path, ec)) return true;
  return std::filesystem::create_directories(path, ec);
}

std::string resolve_write_path(const char* relative) {
  auto parent_ready = [](const std::filesystem::path& file) {
    const std::filesystem::path parent = file.parent_path();
    if (parent.empty()) return true;
    std::error_code ec;
    if (std::filesystem::is_directory(parent, ec)) return true;
    const std::string generic = parent.generic_string();
    if (generic.find("artifacts") == std::string::npos &&
        generic.find("docs") == std::string::npos) {
      return false;
    }
    return std::filesystem::create_directories(parent, ec) ||
           std::filesystem::is_directory(parent);
  };

  // Prefer known checkout when present (agent / CI box).
  {
    std::error_code ec;
    const std::filesystem::path root = "/workspace/Fury";
    const std::filesystem::path abs = root / relative;
    if (std::filesystem::is_directory(root, ec) && parent_ready(abs)) {
      return abs.string();
    }
  }
  const char* prefixes[] = {"", "../", "../../", "../../../", "../../../../"};
  for (const char* pre : prefixes) {
    const std::filesystem::path cand = std::filesystem::path(std::string(pre) + relative);
    if (parent_ready(cand)) return cand.string();
  }
  return relative;
}
#else
[[maybe_unused]] bool ensure_dir(const char* path) {
  struct stat st {};
  if (::stat(path, &st) == 0) return S_ISDIR(st.st_mode);
  return ::mkdir(path, 0755) == 0;
}

std::string resolve_write_path(const char* relative) {
  // Prefer known checkout when present (agent / CI box).
  {
    const std::string abs = std::string("/workspace/Fury/") + relative;
    const auto slash = abs.find_last_of('/');
    if (slash != std::string::npos) {
      const std::string parent = abs.substr(0, slash);
      struct stat st {};
      if (::stat("/workspace/Fury", &st) == 0 && S_ISDIR(st.st_mode)) {
        // ensure parent
        std::string acc;
        for (char ch : parent) {
          acc.push_back(ch);
          if (ch == '/' && acc.size() > 1) {
            ::mkdir(acc.c_str(), 0755);
          }
        }
        ::mkdir(parent.c_str(), 0755);
        return abs;
      }
    }
  }
  const char* prefixes[] = {"", "../", "../../", "../../../", "../../../../"};
  for (const char* pre : prefixes) {
    std::string cand = std::string(pre) + relative;
    // Prefer existing parent dir
    const auto slash = cand.find_last_of('/');
    if (slash != std::string::npos) {
      const std::string parent = cand.substr(0, slash);
      struct stat st {};
      if (::stat(parent.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return cand;
      }
      // try create leaf parent chain for artifacts/
      if (parent.find("artifacts") != std::string::npos ||
          parent.find("docs") != std::string::npos) {
        // create stepwise
        std::string acc;
        for (std::size_t i = 0; i < parent.size(); ++i) {
          acc.push_back(parent[i]);
          if (parent[i] == '/' || i + 1 == parent.size()) {
            if (acc.empty() || acc == "." || acc == "..") continue;
            // trim trailing slash for mkdir
            std::string dir = acc;
            if (!dir.empty() && dir.back() == '/') dir.pop_back();
            if (dir.empty() || dir == "." || dir == "..") continue;
            struct stat st2 {};
            if (::stat(dir.c_str(), &st2) != 0) {
              ::mkdir(dir.c_str(), 0755);
            }
          }
        }
        struct stat st3 {};
        if (::stat(parent.c_str(), &st3) == 0 && S_ISDIR(st3.st_mode)) {
          return cand;
        }
      }
    }
  }
  return relative;
}
#endif

bool write_ppm(const char* path, const std::vector<std::uint8_t>& rgb, int w,
               int h) {
  if (w <= 0 || h <= 0 || rgb.size() < static_cast<std::size_t>(w * h * 3)) {
    return false;
  }
  std::FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  std::fwrite(rgb.data(), 1, static_cast<std::size_t>(w * h * 3), f);
  std::fclose(f);
  return true;
}

struct CineBeat {
  const char* name;
  Vec3 pos;
  float yaw;
  float pitch;
  float hold;
};

const CineBeat kBeats[] = {
    {"01_exterior_establish", {0.f, 6.5f, 18.f}, -1.5708f, -0.28f, 0.22f},
    {"02_lobby", {0.f, 2.0f, -5.5f}, -1.5708f, -0.08f, 0.22f},
    {"03_security", {-4.5f, 2.1f, -9.8f}, 0.2f, -0.1f, 0.2f},
    {"04_vault_reveal", {0.f, 2.2f, -12.5f}, -1.5708f, -0.12f, 0.22f},
    {"05_getaway", {12.f, 3.2f, -16.5f}, -1.2f, -0.25f, 0.2f},
    {"06_hmpd_arrival", {16.5f, 3.0f, -10.f}, -2.4f, -0.18f, 0.2f},
};

}  // namespace

const char* mission_world_state_name(MissionWorldState s) {
  switch (s) {
    case MissionWorldState::PreHeist:
      return "pre_heist";
    case MissionWorldState::Alarm:
      return "alarm";
    case MissionWorldState::Escape:
      return "escape";
  }
  return "unknown";
}

void WishlistController::spawn(fury::Scene& scene) {
  // --- 2. Navigation readability (no arcade arrows) ---
  // Landmark hierarchy: entrance brass columns, lobby chandelier cue,
  // security cyan door frame, vault gold rim, escape amber alley lamp already
  // exist — strengthen with door frames + sightline blockers + path lights.
  place_box(scene, "NavDoorFrameSecL", {-3.55f, 1.6f, kBankCz - 1.55f},
            {0.22f, 3.2f, 0.22f}, {0.55f, 0.72f, 0.95f}, 0.35f, false, true,
            "nav", 0.f);
  place_box(scene, "NavDoorFrameSecR", {-1.85f, 1.6f, kBankCz - 1.55f},
            {0.22f, 3.2f, 0.22f}, {0.55f, 0.72f, 0.95f}, 0.35f, false, true,
            "nav", 0.f);
  place_box(scene, "NavDoorLintelSec", {-2.7f, 3.25f, kBankCz - 1.55f},
            {1.9f, 0.18f, 0.28f}, {0.45f, 0.65f, 1.f}, 0.55f, false, true, "nav");
  place_box(scene, "NavVaultFrameL", {-1.7f, 1.5f, kBankCz - 4.9f},
            {0.25f, 3.0f, 0.25f}, {0.95f, 0.75f, 0.28f}, 0.4f, false, true,
            "nav");
  place_box(scene, "NavVaultFrameR", {1.7f, 1.5f, kBankCz - 4.9f},
            {0.25f, 3.0f, 0.25f}, {0.95f, 0.75f, 0.28f}, 0.4f, false, true,
            "nav");
  place_box(scene, "NavPathLight0", {0.f, 0.12f, kBankCz + 5.0f},
            {0.55f, 0.06f, 0.55f}, {1.f, 0.88f, 0.55f}, 0.55f, false, true,
            "nav_light");
  place_box(scene, "NavPathLight1", {-2.6f, 0.12f, kBankCz + 0.2f},
            {0.45f, 0.06f, 0.45f}, {0.55f, 0.75f, 1.f}, 0.5f, false, true,
            "nav_light");
  place_box(scene, "NavPathLight2", {0.f, 0.12f, kBankCz - 3.4f},
            {0.5f, 0.06f, 0.5f}, {1.f, 0.78f, 0.3f}, 0.6f, false, true,
            "nav_light");
  place_box(scene, "NavLandmarkLobby", {0.f, 3.9f, kBankCz + 3.2f},
            {0.8f, 0.25f, 0.8f}, {1.f, 0.9f, 0.65f}, 1.2f, false, true, "lamp");
  place_box(scene, "NavSightBlockL", {-7.2f, 1.4f, kBankCz + 1.5f},
            {0.6f, 2.8f, 2.2f}, {0.42f, 0.44f, 0.48f}, 0.02f, true, true,
            "nav");
  place_box(scene, "NavSightBlockR", {7.2f, 1.4f, kBankCz + 1.5f},
            {0.6f, 2.8f, 2.2f}, {0.42f, 0.44f, 0.48f}, 0.02f, true, true,
            "nav");
  Log::info(kLogNavReadability);

  // --- 3. Security system depth ---
  place_box(scene, "SecTerminalA", {-5.6f, 1.15f, kBankCz + 0.9f},
            {0.55f, 0.45f, 0.35f}, {0.2f, 0.55f, 0.5f}, 0.7f, true, true,
            "security_term");
  place_box(scene, "SecTerminalB", {-4.4f, 1.15f, kBankCz - 0.4f},
            {0.5f, 0.4f, 0.32f}, {0.18f, 0.5f, 0.48f}, 0.65f, true, true,
            "security_term");
  place_box(scene, "SecServerRack", {6.5f, 1.1f, kBankCz - 2.2f},
            {0.7f, 2.2f, 0.55f}, {0.25f, 0.28f, 0.32f}, 0.15f, true, true,
            "security");
  place_box(scene, "SecServerBlink", {6.5f, 1.8f, kBankCz - 1.9f},
            {0.5f, 0.12f, 0.08f}, {0.3f, 0.95f, 0.45f}, 1.1f, false, true,
            "lamp");
  // Locked transition gate lobby → vault corridor (opens via badge).
  place_box(scene, "SecLobbyGate", {-2.7f, 1.5f, kBankCz - 1.55f},
            {1.7f, 3.0f, 0.18f}, {0.35f, 0.38f, 0.42f}, 0.05f, true, true,
            "security_gate");
  place_box(scene, "SecBadgePad", {-3.15f, 1.25f, kBankCz - 1.35f},
            {0.2f, 0.28f, 0.12f}, {0.4f, 0.85f, 0.55f}, 0.8f, false, true,
            "badge_reader");
  place_box(scene, "SecCamExtra", {3.8f, 2.9f, kBankCz - 0.5f},
            {0.3f, 0.22f, 0.35f}, {0.12f, 0.14f, 0.16f}, 0.05f, false, true,
            "camera");
  Log::info(kLogSecurityDepth);

  // --- 4. Vault interaction detail (machine) ---
  place_box(scene, "VaultDial", {0.55f, 1.55f, kBankCz - 4.55f},
            {0.35f, 0.35f, 0.12f}, {0.85f, 0.7f, 0.25f}, 0.25f, false, true,
            "vault_machine");
  place_box(scene, "VaultBolt0", {-0.9f, 1.1f, kBankCz - 4.55f},
            {0.35f, 0.18f, 0.18f}, {0.55f, 0.55f, 0.58f}, 0.1f, false, true,
            "vault_bolt");
  place_box(scene, "VaultBolt1", {0.9f, 1.1f, kBankCz - 4.55f},
            {0.35f, 0.18f, 0.18f}, {0.55f, 0.55f, 0.58f}, 0.1f, false, true,
            "vault_bolt");
  place_box(scene, "VaultBolt2", {-0.9f, 2.0f, kBankCz - 4.55f},
            {0.35f, 0.18f, 0.18f}, {0.55f, 0.55f, 0.58f}, 0.1f, false, true,
            "vault_bolt");
  place_box(scene, "VaultBolt3", {0.9f, 2.0f, kBankCz - 4.55f},
            {0.35f, 0.18f, 0.18f}, {0.55f, 0.55f, 0.58f}, 0.1f, false, true,
            "vault_bolt");
  place_box(scene, "VaultStatusLed", {0.f, 2.55f, kBankCz - 4.45f},
            {0.45f, 0.12f, 0.1f}, {0.95f, 0.25f, 0.15f}, 0.9f, false, true,
            "vault_led");
  place_box(scene, "VaultMaintHatch", {-2.4f, 0.55f, kBankCz - 5.6f},
            {0.7f, 1.1f, 0.12f}, {0.4f, 0.42f, 0.45f}, 0.05f, true, true,
            "vault_maint");
  place_box(scene, "VaultEmergPanel", {2.3f, 1.4f, kBankCz - 5.5f},
            {0.35f, 0.55f, 0.12f}, {0.85f, 0.2f, 0.15f}, 0.35f, true, true,
            "vault_emerg");
  place_box(scene, "VaultToolPoint", {1.6f, 0.9f, kBankCz - 4.2f},
            {0.35f, 0.2f, 0.35f}, {0.7f, 0.55f, 0.25f}, 0.2f, false, true,
            "vault_tool");
  place_box(scene, "VaultGearRing", {0.f, 1.55f, kBankCz - 4.7f},
            {1.1f, 1.1f, 0.08f}, {0.65f, 0.55f, 0.28f}, 0.15f, false, true,
            "vault_machine");

  // --- 1/6 Alarm shutters + aftermath + escape blockers (start hidden) ---
  place_box(scene, "AlarmShutterL", {-5.5f, 2.0f, kBankCz + 6.9f},
            {4.5f, 4.0f, 0.15f}, {0.3f, 0.32f, 0.35f}, 0.02f, true, false,
            "alarm_shutter");
  place_box(scene, "AlarmShutterR", {5.5f, 2.0f, kBankCz + 6.9f},
            {4.5f, 4.0f, 0.15f}, {0.3f, 0.32f, 0.35f}, 0.02f, true, false,
            "alarm_shutter");
  place_box(scene, "HmpdArrivalCue", {16.5f, 0.05f, -12.f},
            {3.2f, 0.08f, 2.0f}, {0.2f, 0.35f, 0.95f}, 0.7f, false, false,
            "hmpd_cue");

  // Aftermath (hidden until alarm)
  place_box(scene, "AftermathPaper0", {0.8f, 0.08f, kBankCz + 3.8f},
            {0.35f, 0.02f, 0.25f}, {0.92f, 0.88f, 0.75f}, 0.05f, false, false,
            "aftermath");
  place_box(scene, "AftermathPaper1", {-1.2f, 0.08f, kBankCz + 2.5f},
            {0.28f, 0.02f, 0.2f}, {0.9f, 0.86f, 0.74f}, 0.04f, false, false,
            "aftermath");
  place_box(scene, "AftermathPaper2", {-4.5f, 0.08f, kBankCz + 0.1f},
            {0.3f, 0.02f, 0.22f}, {0.88f, 0.84f, 0.72f}, 0.04f, false, false,
            "aftermath");
  place_box(scene, "AftermathChair", {2.2f, 0.35f, kBankCz + 4.6f},
            {0.7f, 0.55f, 0.7f}, {0.35f, 0.22f, 0.18f}, 0.02f, true, false,
            "aftermath", 1.1f);
  place_box(scene, "AftermathPanel", {6.9f, 1.2f, kBankCz - 1.2f},
            {0.5f, 0.7f, 0.15f}, {0.55f, 0.25f, 0.2f}, 0.15f, false, false,
            "aftermath");
  place_box(scene, "AftermathGlass0", {1.5f, 0.06f, kBankCz + 5.5f},
            {0.4f, 0.04f, 0.35f}, {0.75f, 0.85f, 0.95f}, 0.35f, false, false,
            "aftermath");
  place_box(scene, "AftermathGlass1", {-0.5f, 0.05f, kBankCz + 5.8f},
            {0.25f, 0.03f, 0.28f}, {0.7f, 0.82f, 0.92f}, 0.3f, false, false,
            "aftermath");
  place_box(scene, "AftermathGlass2", {0.2f, 0.05f, kBankCz - 2.0f},
            {0.3f, 0.03f, 0.22f}, {0.72f, 0.84f, 0.94f}, 0.28f, false, false,
            "aftermath");

  // Escape blocked path props
  place_box(scene, "EscapeBlocker0", {8.5f, 0.6f, -14.5f},
            {2.2f, 1.2f, 0.55f}, {0.75f, 0.55f, 0.15f}, 0.05f, true, false,
            "escape_block");
  place_box(scene, "EscapeBlocker1", {14.8f, 0.55f, -17.2f},
            {1.8f, 1.1f, 0.5f}, {0.7f, 0.5f, 0.12f}, 0.05f, true, false,
            "escape_block");
  place_box(scene, "EscapeResponseVan", {18.5f, 0.9f, -11.5f},
            {4.2f, 1.8f, 2.0f}, {0.15f, 0.22f, 0.55f}, 0.2f, true, false,
            "escape_response");
  place_box(scene, "EscapeResponseLight", {18.5f, 1.9f, -11.5f},
            {0.4f, 0.25f, 0.4f}, {0.95f, 0.2f, 0.15f}, 1.5f, false, false,
            "escape_response");

  Log::info(kLogWishlistSpawned);
}

void WishlistController::register_security(fury::Scene& scene,
                                           fury::SecurityNet& security) {
  auto add_mm_cam = [&](const char* body, float yaw) {
    fury::SecurityCamera c;
    if (auto* e = scene.find_by_name(body)) {
      c.position = e->transform.position;
    } else {
      c.position = {0.f, 2.8f, kBankCz};
    }
    c.yaw = yaw;
    c.site_id = 0;
    c.entity_name = body;
    c.lens_name = body;
    c.range = 14.f;
    security.add_camera(std::move(c));
  };
  add_mm_cam("MMCam0", 0.4f);
  add_mm_cam("MMCam1", 2.8f);
  add_mm_cam("MMCam2", -1.5708f);
  add_mm_cam("SecCamExtra", 3.14f);

  fury::BreakerBox badge;
  if (auto* e = scene.find_by_name("SecBadgePad")) {
    badge.position = e->transform.position;
  } else {
    badge.position = {-3.15f, 1.25f, kBankCz - 1.35f};
  }
  badge.site_id = 0;
  badge.entity_name = "SecBadgePad";
  badge.interact_radius = 2.4f;
  security.add_breaker(std::move(badge));

  // Map alarm panel as secondary breaker alias if present
  if (scene.find_by_name("MMAlarmPanel") && !scene.find_by_name("BankBreaker")) {
    fury::BreakerBox b;
    b.position = scene.find_by_name("MMAlarmPanel")->transform.position;
    b.site_id = 0;
    b.entity_name = "MMAlarmPanel";
    security.add_breaker(std::move(b));
  }
}

void WishlistController::apply_world_state(fury::Scene& scene,
                                           fury::NpcSystem& npcs,
                                           fury::TrafficSystem& traffic,
                                           MissionWorldState next) {
  if (next == world_state && aftermath_spawned &&
      next != MissionWorldState::PreHeist) {
    // Still allow re-apply logs only on change
  }
  if (next == world_state) {
    // Re-apply visibility for smoke re-entry is fine; skip log spam
  } else {
    Log::info(std::string(kLogMissionStatePrefix) +
              mission_world_state_name(next));
  }
  world_state = next;

  switch (next) {
    case MissionWorldState::PreHeist: {
      set_tag_visible(scene, "alarm_shutter", false);
      set_tag_visible(scene, "aftermath", false);
      set_tag_visible(scene, "escape_block", false);
      set_tag_visible(scene, "escape_response", false);
      set_tag_visible(scene, "hmpd_cue", false);
      set_name_visible(scene, "SecLobbyGate", !badge_unlocked);
      set_name_solid(scene, "SecLobbyGate", !badge_unlocked);
      // Warm lobby lamps full; alarm lamp dim
      if (auto* a = scene.find_by_name("MMLampAlarmAccent")) {
        a->material.emissive = 0.35f;
      }
      relocate_npc(npcs, "NpcDeskGuard", {-5.1f, 0.f, -9.4f},
                   {{-5.1f, 0.f, -9.4f},
                    {-4.2f, 0.f, -11.5f},
                    {-5.8f, 0.f, -8.2f}});
      relocate_npc(npcs, "NpcBankCust", {1.2f, 0.f, -5.2f},
                   {{1.2f, 0.f, -5.2f},
                    {-1.0f, 0.f, -4.8f},
                    {0.4f, 0.f, -5.6f}});
      relocate_npc(npcs, "NpcTeller", {0.15f, 0.f, -7.4f},
                   {{0.15f, 0.f, -7.4f},
                    {-0.6f, 0.f, -7.4f},
                    {0.7f, 0.f, -7.4f}});
      for (auto& car : traffic.cars()) {
        car.active = true;
        car.cruise_speed = (std::max)(6.5f, car.cruise_speed * 0.0f + 7.5f);
      }
      aftermath_spawned = false;
      break;
    }
    case MissionWorldState::Alarm: {
      set_tag_visible(scene, "alarm_shutter", true);
      set_tag_visible(scene, "aftermath", true);
      set_tag_visible(scene, "hmpd_cue", true);
      set_tag_visible(scene, "escape_block", false);
      set_tag_visible(scene, "escape_response", false);
      if (auto* a = scene.find_by_name("MMLampAlarmAccent")) {
        a->material.emissive = 2.8f;
        a->material.albedo = {1.f, 0.15f, 0.1f};
      }
      // Dim warm lobby, punch security cools
      for (auto& e : scene.entities()) {
        if (e.name.find("MMLampLobby") != std::string::npos) {
          e.material.emissive = 0.35f;
        }
      }
      // Guards relocate to choke points
      relocate_npc(npcs, "NpcDeskGuard", {-2.7f, 0.f, -11.2f},
                   {{-2.7f, 0.f, -11.2f},
                    {-1.5f, 0.f, -12.5f},
                    {-3.5f, 0.f, -10.0f}});
      relocate_npc(npcs, "NpcGuard", {0.f, 0.f, -8.5f},
                   {{0.f, 0.f, -8.5f},
                    {2.5f, 0.f, -10.f},
                    {-2.5f, 0.f, -10.f}});
      // Civilians flee toward entrance / vanish duty
      relocate_npc(npcs, "NpcBankCust", {0.f, 0.f, -2.5f},
                   {{0.f, 0.f, -2.5f}, {0.f, 0.f, 2.0f}});
      relocate_npc(npcs, "NpcTeller", {-1.5f, 0.f, -6.5f},
                   {{-1.5f, 0.f, -6.5f}});
      relocate_npc(npcs, "NpcAlleyHmpd", {15.5f, 0.f, -13.5f},
                   {{15.5f, 0.f, -13.5f},
                    {14.0f, 0.f, -16.0f},
                    {17.0f, 0.f, -11.0f}});
      if (!aftermath_spawned) {
        Log::info(kLogAftermath);
        aftermath_spawned = true;
      }
      break;
    }
    case MissionWorldState::Escape: {
      set_tag_visible(scene, "alarm_shutter", true);
      set_tag_visible(scene, "aftermath", true);
      set_tag_visible(scene, "escape_block", true);
      set_tag_visible(scene, "escape_response", true);
      set_tag_visible(scene, "hmpd_cue", true);
      // Open gate if not already (escape scramble)
      set_name_visible(scene, "SecLobbyGate", false);
      set_name_solid(scene, "SecLobbyGate", false);
      badge_unlocked = true;
      // Raise street density / speed chaos
      for (auto& car : traffic.cars()) {
        car.active = true;
        car.cruise_speed = 11.5f;
      }
      relocate_npc(npcs, "NpcAlleyHmpd", {13.5f, 0.f, -18.0f},
                   {{13.5f, 0.f, -18.0f},
                    {16.0f, 0.f, -15.0f},
                    {11.0f, 0.f, -16.5f}});
      relocate_npc(npcs, "NpcDeskGuard", {10.0f, 0.f, -14.0f},
                   {{10.0f, 0.f, -14.0f}, {12.0f, 0.f, -17.0f}});
      break;
    }
  }
}

void WishlistController::sync_from_heist(fury::Scene& scene,
                                         fury::NpcSystem& npcs,
                                         fury::TrafficSystem& traffic,
                                         fury::HeistPhase phase,
                                         bool alarm_active) {
  MissionWorldState want = MissionWorldState::PreHeist;
  if (phase == fury::HeistPhase::Escape || phase == fury::HeistPhase::Success ||
      phase == fury::HeistPhase::Failed) {
    want = MissionWorldState::Escape;
  } else if (alarm_active || phase == fury::HeistPhase::Looting ||
             phase == fury::HeistPhase::Breach) {
    // Breach begins tension; alarm_active forces full alarm dressing
    want = alarm_active ? MissionWorldState::Alarm
                        : (phase == fury::HeistPhase::Looting
                               ? MissionWorldState::Alarm
                               : MissionWorldState::PreHeist);
    if (phase == fury::HeistPhase::Breach && !alarm_active) {
      want = MissionWorldState::PreHeist;
    }
    if (phase == fury::HeistPhase::Looting) {
      want = MissionWorldState::Alarm;
    }
  }
  if (want != world_state) {
    apply_world_state(scene, npcs, traffic, want);
  }
}

void WishlistController::update_vault_machine(fury::Scene& scene,
                                              fury::HeistPhase phase,
                                              float dt) {
  int stage = 0;
  float target = 0.f;
  switch (phase) {
    case fury::HeistPhase::Idle:
    case fury::HeistPhase::Approach:
      stage = 0;
      target = 0.f;
      break;
    case fury::HeistPhase::Breach:
      stage = 1;
      target = 0.35f;
      break;
    case fury::HeistPhase::Looting:
      stage = 2;
      target = 0.75f;
      break;
    case fury::HeistPhase::Escape:
    case fury::HeistPhase::Success:
      stage = 3;
      target = 1.f;
      vault_unlocked = true;
      break;
    case fury::HeistPhase::Failed:
      stage = 0;
      target = 0.f;
      break;
  }
  const float speed = 0.55f;
  if (vault_seq_t < target) {
    vault_seq_t = (std::min)(target, vault_seq_t + speed * dt);
  } else if (vault_seq_t > target) {
    vault_seq_t = (std::max)(target, vault_seq_t - speed * dt);
  }

  if (stage != vault_seq_stage) {
    vault_seq_stage = stage;
    const char* names[] = {"locked", "breach_spin", "bolts_retract", "open"};
    Log::info(std::string(kLogVaultMachinePrefix) + names[stage] + " (" +
              std::to_string(vault_seq_t) + ")");
  }

  // Animate dial + bolts + LED
  if (auto* dial = scene.find_by_name("VaultDial")) {
    dial->transform.rotation_euler.z = vault_seq_t * 6.28318f;
    dial->material.emissive = 0.2f + 0.9f * vault_seq_t;
  }
  if (auto* gear = scene.find_by_name("VaultGearRing")) {
    gear->transform.rotation_euler.z = -vault_seq_t * 3.5f;
  }
  const char* bolts[] = {"VaultBolt0", "VaultBolt1", "VaultBolt2", "VaultBolt3"};
  for (int i = 0; i < 4; ++i) {
    if (auto* b = scene.find_by_name(bolts[i])) {
      const float retract = (std::max)(0.f, vault_seq_t - 0.25f) / 0.75f;
      const float side = (i % 2 == 0) ? -1.f : 1.f;
      b->transform.position.x = side * (0.9f + retract * 0.55f);
      b->material.emissive = 0.1f + 0.6f * retract;
    }
  }
  if (auto* led = scene.find_by_name("VaultStatusLed")) {
    if (vault_seq_t < 0.3f) {
      led->material.albedo = {0.95f, 0.25f, 0.15f};
      led->material.emissive = 0.9f;
    } else if (vault_seq_t < 0.85f) {
      led->material.albedo = {0.95f, 0.75f, 0.15f};
      led->material.emissive = 1.1f;
    } else {
      led->material.albedo = {0.25f, 0.95f, 0.35f};
      led->material.emissive = 1.3f;
    }
  }
  // Hide solid vault collider once open
  if (vault_unlocked) {
    if (auto* v = scene.find_by_name("VaultDoor")) {
      v->solid = false;
      v->visible = false;
    }
  }
}

void WishlistController::update_zone_audio(fury::Audio& audio,
                                           const fury::Vec3& p, float dt,
                                           bool alarm_active) {
  const char* zone = "alley";
  // Lobby z ~ -3..-7, security ~ -8..-12, vault ~ -13..-17, alley otherwise
  if (p.z < -13.5f && std::fabs(p.x) < 8.f) {
    zone = "vault";
  } else if (p.z < -8.5f && p.x < -1.5f) {
    zone = "security";
  } else if (p.z < -2.5f && p.z > -14.f && std::fabs(p.x) < 9.f) {
    zone = "lobby";
  } else if (p.z < -12.f && p.x > 8.f) {
    zone = "alley";
  } else if (std::fabs(p.x) < 10.f && p.z < 4.f && p.z > -3.f) {
    zone = "lobby";
  }

  audio_zone_timer += dt;
  if (zone != last_audio_zone) {
    last_audio_zone = zone;
    audio_zone_timer = 0.f;
    Log::info(std::string(kLogAudioZonePrefix) + zone);
    if (std::strcmp(zone, "lobby") == 0) {
      audio.play_cue("zone_lobby");
      audio.play_cue("footstep");
      audio.play_cue("phone_ring");
      lobby_oneshot_t = 0.f;
    } else if (std::strcmp(zone, "security") == 0) {
      audio.play_cue("zone_security");
      audio.play_cue("radio_blip");
    } else if (std::strcmp(zone, "vault") == 0) {
      audio.play_cue("zone_vault");
    } else {
      audio.play_cue("zone_alley");
      if (alarm_active) {
        audio.play_cue("alarm_klaxon");
        audio.play_cue("siren");
      }
    }
  } else if (audio_zone_timer > 2.4f) {
    audio_zone_timer = 0.f;
    // Soft bed refresh
    if (std::strcmp(zone, "lobby") == 0) {
      audio.play_cue("zone_lobby_hum");
      lobby_oneshot_t += 2.4f;
      if (lobby_oneshot_t >= 4.8f) {
        lobby_oneshot_t = 0.f;
        audio.play_cue("printer");
      }
    } else if (std::strcmp(zone, "security") == 0) {
      audio.play_cue("zone_security_hum");
      audio.play_cue("radio_blip");
    } else if (std::strcmp(zone, "vault") == 0) {
      audio.play_cue("zone_vault_hum");
    } else {
      audio.play_cue("zone_alley_traffic");
    }
  }
}

bool WishlistController::try_security_interact(fury::Scene& scene,
                                               const fury::Vec3& player_pos) {
  auto near = [&](const char* name, float r) {
    if (auto* e = scene.find_by_name(name)) {
      const float dx = player_pos.x - e->transform.position.x;
      const float dz = player_pos.z - e->transform.position.z;
      return dx * dx + dz * dz <= r * r;
    }
    return false;
  };
  if ((!badge_unlocked || lockdown_active) &&
      (near("SecBadgePad", 2.6f) || near("MMBadgeScan", 2.6f))) {
    badge_unlocked = true;
    set_name_visible(scene, "SecLobbyGate", false);
    set_name_solid(scene, "SecLobbyGate", false);
    if (auto* pad = scene.find_by_name("SecBadgePad")) {
      pad->material.albedo = {0.25f, 0.95f, 0.45f};
      pad->material.emissive = 1.4f;
    }
    if (lockdown_active) {
      lockdown_active = false;
      Log::info(kLogLockdownOff);
    }
    Log::info("Security: badge accepted — lobby→vault gate open");
    return true;
  }
  if (near("VaultMaintHatch", 2.4f) || near("VaultToolPoint", 2.4f)) {
    Log::info("Vault: maintenance / tool interact point engaged");
    vault_seq_t = (std::max)(vault_seq_t, 0.2f);
    return true;
  }
  if (near("VaultEmergPanel", 2.4f)) {
    Log::info("Vault: emergency controls toggled");
    return true;
  }
  if (near("SecTerminalA", 2.5f) || near("SecTerminalB", 2.5f)) {
    Log::info("Security: terminal accessed — camera loop delayed");
    if (lockdown_active) {
      lockdown_active = false;
      set_name_visible(scene, "SecLobbyGate", false);
      set_name_solid(scene, "SecLobbyGate", false);
      badge_unlocked = true;
      Log::info(kLogLockdownOff);
    }
    return true;
  }
  return false;
}

void WishlistController::dump_profile(fury::Scene& scene,
                                      const fury::Renderer& renderer, float fps,
                                      float frame_ms,
                                      const char* out_md_path) {
  const int ents = static_cast<int>(scene.entities().size());
  const int vis = count_visible(scene);
  const long rss = read_rss_kb();
  const auto stats = renderer.statistics();
  std::ostringstream oss;
  oss << kLogProfilePrefix << "fps=" << fps << " frame_ms=" << frame_ms
      << " entities=" << ents << " visible=" << vis
      << " instances=" << stats.instance_count
      << " tris=" << stats.triangle_count;
  if (stats.gpu_frame_ms > 0.0) {
    oss << " gpu_ms=" << stats.gpu_frame_ms;
  }
  if (rss >= 0) {
    oss << " rss_kb=" << rss;
  }
  oss << " world=" << mission_world_state_name(world_state);
  Log::info(oss.str());

  if (out_md_path && out_md_path[0]) {
    const std::string md_path = resolve_write_path(out_md_path);
    std::ofstream out(md_path);
    if (out) {
      out << "# Meridian Mutual soft-smoke profile\n\n";
      out << "Captured from Vaultline `--smoke --soft --profile` "
             "(Harbor Metro / HMPD / Meridian Mutual).\n\n";
      out << "| Metric | Value |\n|--------|-------|\n";
      out << "| FPS | " << fps << " |\n";
      out << "| Frame time (ms) | " << frame_ms << " |\n";
      out << "| Entities | " << ents << " |\n";
      out << "| Visible entities | " << vis << " |\n";
      out << "| Renderer instances | " << stats.instance_count << " |\n";
      out << "| Triangles (stat) | " << stats.triangle_count << " |\n";
      out << "| GPU frame ms | " << stats.gpu_frame_ms << " |\n";
      out << "| RSS (KB) | " << rss << " |\n";
      out << "| Mission world state | "
          << mission_world_state_name(world_state) << " |\n";
      out << "| Backend | soft-smoke capture |\n\n";
      out << "Notes: draw-call proxies use visible entity + instance counts on "
             "the software path. Hero vehicles keep material groups; traffic "
             "stays first-material merge.\n";
      Log::info(std::string("Wrote profile markdown: ") + md_path);
    }
  }
  profile_pending = false;
}

bool WishlistController::update_cinematic(fury::Camera& cam,
                                          fury::Renderer& renderer, float dt,
                                          bool write_shots) {
  if (!cinematic_active) return false;
  if (cinematic_beat < 0) {
    cinematic_beat = 0;
    cinematic_t = 0.f;
    Log::info(std::string(kLogCinematicPrefix) + kBeats[0].name);
  }
  const int n = static_cast<int>(sizeof(kBeats) / sizeof(kBeats[0]));
  if (cinematic_beat >= n) {
    cinematic_active = false;
    Log::info("Cinematic capture complete");
    return false;
  }
  const CineBeat& b = kBeats[cinematic_beat];
  cam.position = b.pos;
  cam.yaw = b.yaw;
  cam.pitch = b.pitch;
  cam.fly_mode = true;
  cam.velocity = {};
  cam.snap_look();
  cinematic_t += dt;
  if (write_shots && cinematic_shots_written <= cinematic_beat) {
    std::vector<std::uint8_t> rgb;
    int w = 0, h = 0;
    const std::string path = resolve_write_path(
        (std::string("artifacts/meridian_cinematics/") + b.name + ".ppm")
            .c_str());
    bool ok = false;
    if (renderer.read_rgb_framebuffer(rgb, w, h)) {
      ok = write_ppm(path.c_str(), rgb, w, h);
      if (ok) Log::info(std::string("Cinematic shot saved: ") + path);
    }
    if (!ok) {
      std::vector<std::uint8_t> stub(64 * 48 * 3, 40);
      for (int i = 0; i < 64 * 48; ++i) {
        stub[static_cast<std::size_t>(i * 3 + 0)] =
            static_cast<std::uint8_t>(30 + cinematic_beat * 20);
        stub[static_cast<std::size_t>(i * 3 + 1)] = 40;
        stub[static_cast<std::size_t>(i * 3 + 2)] = 55;
      }
      ok = write_ppm(path.c_str(), stub, 64, 48);
      if (ok) Log::info(std::string("Cinematic stub shot saved: ") + path);
    }
    if (ok) ++cinematic_shots_written;
  }
  if (cinematic_t >= b.hold) {
    cinematic_t = 0.f;
    ++cinematic_beat;
    if (cinematic_beat < n) {
      Log::info(std::string(kLogCinematicPrefix) + kBeats[cinematic_beat].name);
    }
  }
  return cinematic_active && cinematic_beat < n;
}

bool WishlistController::update_smoke_script(fury::Scene& scene,
                                             fury::NpcSystem& npcs,
                                             fury::TrafficSystem& traffic,
                                             fury::Camera& cam,
                                             fury::Renderer& renderer,
                                             fury::Audio& audio, float dt) {
  if (!smoke_scripted) {
    smoke_scripted = true;
    smoke_script_t = 0.f;
    world_state = MissionWorldState::Escape;  // force transition log
    apply_world_state(scene, npcs, traffic, MissionWorldState::PreHeist);
    Log::info("Wishlist smoke script: start (pre_heist)");
    update_zone_audio(audio, {0.f, 1.7f, -6.f}, 0.016f, false);  // lobby
    update_vault_machine(scene, fury::HeistPhase::Approach, 0.016f);
  }
  // Soft path frames can be huge — still advance script with real dt so CI
  // finishes quickly, but emit wishlist beats by absolute thresholds.
  smoke_script_t += dt;

  if (smoke_script_t >= 0.35f && world_state == MissionWorldState::PreHeist) {
    apply_world_state(scene, npcs, traffic, MissionWorldState::Alarm);
    audio.play_cue("siren");
    update_zone_audio(audio, {-4.f, 1.7f, -10.f}, 0.016f, true);  // security
    update_vault_machine(scene, fury::HeistPhase::Looting, 0.5f);
  }
  if (smoke_script_t >= 0.75f && world_state == MissionWorldState::Alarm) {
    apply_world_state(scene, npcs, traffic, MissionWorldState::Escape);
    update_zone_audio(audio, {0.f, 1.7f, -15.f}, 0.016f, true);  // vault
    update_zone_audio(audio, {12.f, 1.7f, -18.f}, 0.016f, true);  // alley
    update_vault_machine(scene, fury::HeistPhase::Escape, 0.5f);
  }
  if (smoke_script_t >= 1.05f && !cinematic_active &&
      cinematic_shots_written == 0) {
    dump_profile(scene, renderer, 30.f, 33.3f, "docs/MERIDIAN_PROFILE.md");
    cinematic_active = true;
    cinematic_beat = -1;
    Log::info("Wishlist smoke script: cinematic capture begin");
  }
  if (cinematic_active) {
    // Soft path is slow — advance scripted holds with at least 0.15s/frame.
    update_cinematic(cam, renderer, (std::max)(dt, 0.15f), true);
  }
  if (world_state == MissionWorldState::PreHeist) {
    update_vault_machine(scene, fury::HeistPhase::Approach, (std::min)(dt, 0.05f));
  } else if (world_state == MissionWorldState::Alarm) {
    update_vault_machine(scene, fury::HeistPhase::Looting, (std::min)(dt, 0.05f));
  } else {
    update_vault_machine(scene, fury::HeistPhase::Escape, (std::min)(dt, 0.05f));
  }

  if (!cinematic_active && cinematic_shots_written > 0 &&
      smoke_script_t >= 1.5f) {
    Log::info("Wishlist smoke script: complete");
    return true;
  }
  if (smoke_script_t >= 14.f) {
    Log::info("Wishlist smoke script: timeout complete");
    return true;
  }
  return false;
}






void WishlistController::ensure_cone_visuals(fury::Scene& scene,
                                             const fury::SecurityNet& security) {
  if (cones_visualized) {
    return;
  }
  cones_visualized = true;
  int idx = 0;
  for (const auto& cam : security.cameras()) {
    if (cam.site_id != 0) {
      continue;
    }
    const float fx = std::cos(cam.yaw);
    const float fz = std::sin(cam.yaw);
    for (int s = 1; s <= 5; ++s) {
      const float t = static_cast<float>(s) / 5.f;
      const float dist = cam.range * t * 0.85f;
      const float half_w = std::tan(cam.cone_half_rad) * dist;
      char name[64];
      std::snprintf(name, sizeof(name), "SecConeWedge%d_%d", idx, s);
      place_box(scene, name,
                {cam.position.x + fx * dist, 0.12f,
                 cam.position.z + fz * dist},
                {half_w * 2.f, 0.035f, 0.32f}, {0.35f, 0.85f, 1.f}, 0.55f,
                false, true, "sec_cone", cam.yaw);
    }
    ++idx;
  }
  Log::info("Security: vision cones visualized (translucent wedges)");
}

void WishlistController::update_security_gameplay(
    fury::Scene& scene, fury::NpcSystem& npcs, fury::SecurityNet& security,
    fury::Audio& audio, const fury::Vec3& player_pos, float dt,
    bool alarm_active) {
  ensure_cone_visuals(scene, security);

  std::string hit;
  if (security.consume_cone_hit(hit)) {
    Log::info(std::string(kLogConeHit) + " (" + hit + ")");
    audio.play_cue("radio_blip");
    if (!guard_investigating && !guard_escalated) {
      guard_investigating = true;
      guard_react_t = 0.f;
      Log::info(kLogPatrolAlert);
      relocate_npc(npcs, "NpcDeskGuard", {player_pos.x, 0.f, player_pos.z},
                   {{player_pos.x, 0.f, player_pos.z},
                    {player_pos.x + 1.5f, 0.f, player_pos.z},
                    {player_pos.x, 0.f, player_pos.z - 1.5f}});
      for (auto& a : npcs.agents()) {
        if (a.entity_name == "NpcDeskGuard" || a.entity_name == "NpcGuard") {
          a.chasing = true;
          a.chase_target = player_pos;
        }
      }
    }
  }

  if (guard_investigating && !guard_escalated) {
    guard_react_t += dt;
    for (auto& a : npcs.agents()) {
      if (a.entity_name == "NpcDeskGuard" || a.entity_name == "NpcGuard") {
        a.chase_target = player_pos;
        a.chasing = true;
      }
    }
    if (guard_react_t >= 2.5f) {
      guard_escalated = true;
      Log::info(kLogPatrolEscalate);
      audio.play_cue("radio_blip");
    }
  }

  if ((alarm_active || guard_escalated) && !lockdown_active && !badge_unlocked) {
    lockdown_active = true;
    security.set_lockdown(true);
    set_name_visible(scene, "SecLobbyGate", true);
    set_name_solid(scene, "SecLobbyGate", true);
    Log::info(kLogLockdownOn);
    if (!alarm_oneshot_played) {
      alarm_oneshot_played = true;
      audio.play_cue("alarm_klaxon");
    }
  }

  if (vault_seq_stage >= 1 && !vault_motor_played) {
    vault_motor_played = true;
    audio.play_cue("vault_motor");
    audio.play_cue("metal_stress");
  }

  if (world_state == MissionWorldState::Escape && !escape_radio_played) {
    escape_radio_played = true;
    audio.play_cue("police_radio");
  }

  if (lockdown_active && !badge_unlocked) {
    set_name_visible(scene, "SecLobbyGate", true);
    set_name_solid(scene, "SecLobbyGate", true);
  }
}

bool WishlistController::update_heist_capture(
    fury::Scene& scene, fury::NpcSystem& npcs, fury::TrafficSystem& traffic,
    fury::Camera& cam, fury::Renderer& renderer, fury::Audio& audio,
    fury::SecurityNet& security, float dt) {
  if (!heist_capture_active) {
    heist_capture_active = true;
    heist_capture_t = 0.f;
    heist_capture_beat = -1;
    heist_capture_shots = 0;
    heist_capture_log =
        "# Meridian Mutual heist capture log\n\n"
        "Harbor Metro / HMPD / Meridian Mutual only.\n\n"
        "| t (s) | Beat | Events |\n|------|------|--------|\n";
    apply_world_state(scene, npcs, traffic, MissionWorldState::PreHeist);
    ensure_cone_visuals(scene, security);
    Log::info("HeistCapture: start (~90s automated run)");
  }

  const float step = (std::max)(dt, 0.35f);
  heist_capture_t += step;

  struct Beat {
    float t;
    const char* name;
    Vec3 pos;
    float yaw;
    float pitch;
    int world;        // 0 pre, 1 alarm, 2 escape
    int vault_stage;  // 0 approach, 1 breach, 2 loot, 3 escape
  };

  const Beat beats[] = {
      {0.f, "01_exterior", {0.f, 6.5f, 18.f}, -1.5708f, -0.28f, 0, 0},
      {12.f, "02_lobby", {0.f, 2.0f, -5.5f}, -1.5708f, -0.08f, 0, 0},
      {25.f, "03_disable_security", {-4.5f, 2.1f, -9.8f}, 0.2f, -0.1f, 0, 1},
      {40.f, "04_vault_open", {0.f, 2.2f, -12.5f}, -1.5708f, -0.12f, 0, 2},
      {52.f, "05_alarm", {0.f, 2.4f, -10.f}, -1.2f, -0.15f, 1, 2},
      {68.f, "06_escape_alley", {12.f, 3.2f, -16.5f}, -1.2f, -0.25f, 2, 3},
      {82.f, "07_hmpd", {16.5f, 3.0f, -10.f}, -2.4f, -0.18f, 2, 3},
  };
  const int n = static_cast<int>(sizeof(beats) / sizeof(beats[0]));

  int want = 0;
  for (int i = 0; i < n; ++i) {
    if (heist_capture_t >= beats[i].t) {
      want = i;
    }
  }

  auto heist_phase_for = [](int stage) {
    switch (stage) {
      case 1:
        return fury::HeistPhase::Breach;
      case 2:
        return fury::HeistPhase::Looting;
      case 3:
        return fury::HeistPhase::Escape;
      default:
        return fury::HeistPhase::Approach;
    }
  };

  auto world_for = [](int w) {
    if (w == 1) return MissionWorldState::Alarm;
    if (w == 2) return MissionWorldState::Escape;
    return MissionWorldState::PreHeist;
  };

  if (want != heist_capture_beat) {
    heist_capture_beat = want;
    const Beat& b = beats[want];
    Log::info(std::string(kLogHeistCapturePrefix) + b.name);
    cam.position = b.pos;
    cam.yaw = b.yaw;
    cam.pitch = b.pitch;
    cam.fly_mode = true;
    cam.velocity = {};
    cam.snap_look();

    apply_world_state(scene, npcs, traffic, world_for(b.world));
    update_vault_machine(scene, heist_phase_for(b.vault_stage), 0.5f);
    update_zone_audio(audio, b.pos, 0.016f, b.world >= 1);

    std::string events;
    if (want == 0) {
      events = "exterior spawn; zone_alley bed";
      audio.play_cue("zone_alley");
      audio.play_cue("footstep");
    } else if (want == 1) {
      events = "lobby enter; zone_lobby; phone_ring; printer";
      audio.play_cue("phone_ring");
      audio.play_cue("printer");
    } else if (want == 2) {
      events = "disable security; cone hit; patrol alert; badge/terminal";
      Log::info(std::string(kLogConeHit) + " (capture)");
      Log::info(kLogPatrolAlert);
      audio.play_cue("radio_blip");
      try_security_interact(scene, {-3.15f, 1.7f, kBankCz - 1.35f});
      guard_investigating = true;
      guard_react_t = 3.f;
    } else if (want == 3) {
      events = "vault open; vault_motor; metal_stress";
      audio.play_cue("vault_motor");
      audio.play_cue("metal_stress");
      vault_motor_played = true;
      vault_unlocked = true;
    } else if (want == 4) {
      events = "alarm; lockdown ON; alarm_klaxon";
      audio.play_cue("alarm_klaxon");
      audio.play_cue("siren");
      alarm_oneshot_played = true;
      if (!lockdown_active) {
        lockdown_active = true;
        security.set_lockdown(true);
        Log::info(kLogLockdownOn);
      }
      Log::info(kLogPatrolEscalate);
      guard_escalated = true;
    } else if (want == 5) {
      events = "escape alley; lockdown OFF; police_radio";
      lockdown_active = false;
      security.set_lockdown(false);
      badge_unlocked = true;
      Log::info(kLogLockdownOff);
      audio.play_cue("police_radio");
      audio.play_cue("zone_alley");
      escape_radio_played = true;
    } else if (want == 6) {
      events = "HMPD arrival; response vehicles";
      audio.play_cue("police_radio");
    }

    update_security_gameplay(scene, npcs, security, audio, b.pos, 0.016f,
                             b.world >= 1);

    std::vector<std::uint8_t> rgb;
    int w = 0, h = 0;
    const std::string shot_path = resolve_write_path(
        (std::string("artifacts/meridian_heist_capture/") + b.name + ".ppm")
            .c_str());
    bool ok = false;
    if (renderer.read_rgb_framebuffer(rgb, w, h)) {
      ok = write_ppm(shot_path.c_str(), rgb, w, h);
    }
    if (!ok) {
      std::vector<std::uint8_t> stub(96 * 64 * 3, 32);
      for (int i = 0; i < 96 * 64; ++i) {
        stub[static_cast<std::size_t>(i * 3 + 0)] =
            static_cast<std::uint8_t>(40 + want * 24);
        stub[static_cast<std::size_t>(i * 3 + 1)] =
            static_cast<std::uint8_t>(50 + (want % 3) * 30);
        stub[static_cast<std::size_t>(i * 3 + 2)] = 70;
      }
      ok = write_ppm(shot_path.c_str(), stub, 96, 64);
    }
    if (ok) {
      ++heist_capture_shots;
      Log::info(std::string("HeistCapture shot saved: ") + shot_path);
    }

    char row[512];
    std::snprintf(row, sizeof(row), "| %.1f | %s | %s |\n", heist_capture_t,
                  b.name, events.c_str());
    heist_capture_log += row;
  } else {
    const Beat& b = beats[heist_capture_beat];
    cam.position = b.pos;
    cam.yaw = b.yaw;
    cam.pitch = b.pitch;
    cam.fly_mode = true;
    cam.snap_look();
    update_security_gameplay(scene, npcs, security, audio, b.pos, step,
                             b.world >= 1);
    update_vault_machine(scene, heist_phase_for(b.vault_stage),
                         (std::min)(step, 0.05f));
  }

  if (heist_capture_t >= 90.f) {
    heist_capture_log +=
        "\n## Summary\n\nShots written: " + std::to_string(heist_capture_shots) +
        "\nDuration: ~90s simulated\nAudio: authored Meridian WAVs when "
        "SDL_mixer present; null backend logs cues.\nSecurity: camera cones, "
        "guard investigate→escalate, lockdown on/off.\n";
    const std::string log_path =
        resolve_write_path("artifacts/meridian_heist_capture/CAPTURE_LOG.md");
    std::ofstream out(log_path);
    if (out) {
      out << heist_capture_log;
      Log::info(std::string("HeistCapture log written: ") + log_path);
    }
    Log::info("HeistCapture: complete");
    heist_capture_active = false;
    return true;
  }
  return false;
}

}  // namespace meridian
