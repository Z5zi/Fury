#include "../apps/vaultline/npc_interaction.hpp"
#include "../apps/vaultline/npc_roster.hpp"
#include "../apps/vaultline/meridian_wishlist.hpp"
#include <fury/banter.hpp>
#include <fury/heat.hpp>
#include <fury/mission.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using fury::Vec3;
constexpr float kPi = 3.14159265358979323846f;
int checks = 0;
void require(bool value, const std::string& message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
bool near(float a, float b, float epsilon = 0.0001f) {
  return std::fabs(a - b) <= epsilon;
}
bool same(Vec3 a, Vec3 b, float epsilon = 0.0001f) {
  return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon) && near(a.z, b.z, epsilon);
}
bool same_route(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) if (!same(a[i], b[i])) return false;
  return true;
}
float distance(Vec3 a, Vec3 b) {
  return std::hypot(a.x - b.x, a.z - b.z);
}
fury::NpcSystem roster_system() {
  fury::NpcSystem npcs;
  for (auto& entry : vaultline::make_npc_roster()) npcs.add(std::move(entry.agent));
  return npcs;
}
fury::NpcAgent& agent(fury::NpcSystem& npcs, const char* entity) {
  for (auto& a : npcs.agents()) if (a.entity_name == entity) return a;
  throw std::runtime_error(std::string("Missing production agent ") + entity);
}

// Quantized, byte-order-independent authored-content fingerprints. Expected
// values were generated independently from the exact spawn blocks in
// b2a695b7:apps/vaultline/main.cpp, not from the new factory under test.
std::uint64_t fingerprint(const fury::NpcAgent& a, Vec3 color) {
  std::uint64_t h = 1469598103934665603ULL;
  const auto number = [&](std::int64_t value) {
    auto u = static_cast<std::uint64_t>(value);
    for (int n = 0; n < 8; ++n) {
      h = (h ^ (u & 255u)) * 1099511628211ULL;
      u >>= 8;
    }
  };
  const auto text = [&](const std::string& s) {
    for (unsigned char c : s) h = (h ^ c) * 1099511628211ULL;
    h = (h ^ 0xffu) * 1099511628211ULL;
  };
  const auto scalar = [&](float value) { number(std::llround(double(value) * 1000000.0)); };
  const auto vec = [&](Vec3 value) { scalar(value.x); scalar(value.y); scalar(value.z); };
  text(a.name); text(a.display_name); text(a.entity_name);
  number(static_cast<int>(a.kind)); number(static_cast<int>(a.schedule));
  vec(a.position); vec(a.home); scalar(a.yaw); scalar(a.speed);
  scalar(a.chase_speed); scalar(a.radius); scalar(a.height);
  number(a.waypoint_index); number(a.waypoints.size());
  for (Vec3 waypoint : a.waypoints) vec(waypoint);
  vec(color);
  return h;
}

void test_authored_roster() {
  struct Expected { const char* name; std::uint64_t fingerprint; };
  const Expected expected[] = {
    {"CivA", 2407833419663941542ULL}, {"CivB", 12726853776579968983ULL},
    {"CivC", 2279099256587926832ULL}, {"BankGuard", 14906583790241249583ULL},
    {"BankTeller", 12691263026325231178ULL}, {"BankDeskGuard", 16328057515669868735ULL},
    {"BankCustomer", 1388487788735035547ULL}, {"AlleyHmpd", 9888281559361948701ULL},
    // Approved market detour; Nell retains her home, pace and identity.
    {"CivAsh", 11603154131228227943ULL}, {"CivD", 4269080546033717195ULL},
    // Rina's reviewed ground-level detour avoids buildings and the raised bridge.
    {"CivE", 13713798959989351134ULL}, {"Fence", 7583525604434232304ULL}
  };
  const auto roster = vaultline::make_npc_roster();
  require(roster.size() == 12, "Production roster still has exactly twelve regular NPCs");
  std::set<std::string> ids, entities, labels;
  int civilians = 0, guards = 0, fences = 0;
  for (std::size_t i = 0; i < roster.size(); ++i) {
    const auto& entry = roster[i];
    const auto& a = entry.agent;
    require(a.name == expected[i].name, "Production spawn ordering and internal IDs retained");
    require(fingerprint(a, entry.color) == expected[i].fingerprint,
            a.name + ": authored label/role/schedule/spawn/home/pace/route/color unchanged");
    require(ids.insert(a.name).second && entities.insert(a.entity_name).second &&
                labels.insert(a.label()).second, a.name + ": unique identity, entity and label");
    civilians += a.kind == fury::NpcKind::Civilian;
    guards += a.kind == fury::NpcKind::Guard;
    fences += a.kind == fury::NpcKind::Fence;
  }
  require(civilians == 8 && guards == 3 && fences == 1, "Eight civilians, three guards and Cass");
  const auto crew = vaultline::make_crew_roster();
  require(crew.size() == 2, "Production crew roster retains both original members");
  struct CrewExpected {
    const char* name; const char* label; const char* entity;
    float height; Vec3 position; Vec3 offset; Vec3 color;
  };
  const CrewExpected original_crew[] = {
    {"Crew-Rook", "Rook", "CrewRook", 1.7f, {-2.f, .85f, 14.f}, {-1.8f, 0.f, -1.4f}, {.35f, .75f, .55f}},
    {"Crew-Sparrow", "Sparrow", "CrewSparrow", 1.72f, {2.f, .86f, 14.f}, {1.8f, 0.f, -1.2f}, {.75f, .45f, .35f}}
  };
  for (std::size_t i = 0; i < crew.size(); ++i) {
    const auto& a = crew[i].member;
    const auto& original = original_crew[i];
    require(a.name == original.name && a.display_name == original.label &&
                a.entity_name == original.entity && near(a.height, original.height) &&
                same(a.position, original.position) && same(a.follow_offset, original.offset) &&
                same(crew[i].color, original.color) && near(a.follow_speed, 6.5f) && a.active,
            a.name + ": actual crew factory preserves original identity/spawn/height/follow offset/pace/color");
  }
}

void test_real_routes_and_schedules() {
  for (int hz : {30, 60}) {
    auto npcs = roster_system();
    npcs.apply_schedules(true);
    const auto originals = npcs.agents();
    std::vector<int> arrivals(originals.size(), 0);
    const float dt = 1.f / float(hz);
    for (int frame = 0; frame < 240 * hz; ++frame) {
      std::vector<int> indices;
      for (const auto& a : npcs.agents()) indices.push_back(a.waypoint_index);
      npcs.update(dt, .45f);
      for (std::size_t i = 0; i < originals.size(); ++i) {
        const auto& a = npcs.agents()[i];
        require(std::isfinite(a.position.x) && std::isfinite(a.position.z) &&
                    std::isfinite(a.yaw), a.name + ": finite route simulation");
        require(near(a.position.y, a.height * 0.5f), a.name + ": grounded on authored height");
        require(a.waypoint_index >= 0 && std::size_t(a.waypoint_index) < a.waypoints.size(),
                a.name + ": valid route index");
        if (a.waypoint_index != indices[i]) {
          require(a.waypoint_index == (indices[i] + 1) % int(a.waypoints.size()),
                  a.name + ": waypoints visited in authored order");
          ++arrivals[i];
        }
      }
    }
    for (std::size_t i = 0; i < originals.size(); ++i) {
      const auto& a = npcs.agents()[i];
      const auto& original = originals[i];
      require(arrivals[i] >= int(original.waypoints.size()) * 2,
              a.name + ": completes at least two actual patrol circuits at " + std::to_string(hz) + " Hz");
      require(same_route(a.waypoints, original.waypoints) &&
                  same_route(a.base_waypoints, original.waypoints), a.name + ": route coordinates never drift");
      require(a.name == original.name && a.entity_name == original.entity_name &&
                  a.display_name == original.display_name && a.kind == original.kind &&
                  a.schedule == original.schedule, a.name + ": gameplay identity survives simulation");
      require(a.travel_distance > 10.0, a.name + ": measured route travel advances");
    }
    // Repeated day/night transitions must always derive from the day route,
    // never progressively shrink it or forget a fence/civilian's duty state.
    for (int cycle = 0; cycle < 3; ++cycle) {
      for (auto& a : npcs.agents()) a.chasing = true;
      npcs.apply_schedules(false);
      const auto night_start = npcs.agents();
      int on_duty = 0;
      for (std::size_t i = 0; i < originals.size(); ++i) {
        const auto& a = npcs.agents()[i];
        const auto& original = originals[i];
        on_duty += a.on_duty;
        if (a.schedule == fury::NpcSchedule::DayOnly) {
          require(!a.on_duty && !a.chasing, a.name + ": night cancels duty and chase");
        } else {
          require(a.on_duty, a.name + ": night watch/sparse civilian remains present");
        }
        if (a.schedule == fury::NpcSchedule::NightTighten) {
          require(near(a.speed, original.speed * 1.45f), a.name + ": night watch pace preserved");
          for (std::size_t w = 0; w < a.waypoints.size(); ++w) {
            const Vec3 base = original.waypoints[w];
            require(same(a.waypoints[w], {a.home.x + (base.x - a.home.x) * .52f,
                         base.y, a.home.z + (base.z - a.home.z) * .52f}),
                    a.name + ": night route uses authored 52 percent compression");
          }
        }
      }
      require(on_duty == 4, "Night retains Mira and three guards; seven day civilians and Cass rest");
      for (auto& a : npcs.agents()) a.chasing = false;
      for (int frame = 0; frame < hz * 2; ++frame) npcs.update(dt, .45f);
      for (std::size_t i = 0; i < originals.size(); ++i) {
        const auto& a = npcs.agents()[i];
        if (a.schedule == fury::NpcSchedule::DayOnly) {
          require(same(a.position, night_start[i].position) &&
                      a.waypoint_index == night_start[i].waypoint_index &&
                      a.anim_phase == night_start[i].anim_phase &&
                      a.breathe_phase == night_start[i].breathe_phase,
                  a.name + ": off-duty agent does not move or animate");
          require(a.actual_speed == 0.f, a.name + ": off-duty motion signal is zero");
        }
      }
      npcs.apply_schedules(true);
      for (std::size_t i = 0; i < originals.size(); ++i) {
        const auto& a = npcs.agents()[i];
        require(a.on_duty && same_route(a.waypoints, originals[i].waypoints) &&
                    near(a.speed, originals[i].speed), a.name + ": daytime restores exact base route and pace");
      }
    }
  }
}

void test_chase_and_culling() {
  for (auto kind : {fury::NpcKind::Civilian, fury::NpcKind::Fence,
                    fury::NpcKind::Guard, fury::NpcKind::Enforcer}) {
    fury::NpcSystem npcs;
    fury::NpcAgent a;
    a.name = "EligibilityFixture"; a.kind = kind; a.position = {0.f, .9f, 0.f};
    a.waypoints = {{-10.f, 0.f, 0.f}, {-10.f, 0.f, 8.f}};
    a.chasing = true; a.chase_target = {12.f, 0.f, 0.f};
    npcs.add(a);
    for (int frame = 0; frame < 120; ++frame) npcs.update(1.f / 60.f, .45f);
    const bool threat = kind == fury::NpcKind::Guard || kind == fury::NpcKind::Enforcer;
    require(threat ? npcs.agents()[0].position.x > 0.f : npcs.agents()[0].position.x < 0.f,
            "Only Guard/Enforcer kinds chase; civilian and fence retain patrol behavior");
  }
  auto npcs = roster_system();
  npcs.apply_schedules(true);
  npcs.update(.1f, .45f);
  const auto culled_start = npcs.agents();
  for (int frame = 0; frame < 60; ++frame) npcs.update(1.f / 60.f, .45f, {500.f, 0.f, 500.f}, 70.f);
  for (std::size_t i = 0; i < npcs.agents().size(); ++i) {
    require(same(npcs.agents()[i].position, culled_start[i].position) &&
                npcs.agents()[i].actual_speed == 0.f, "Far non-chasing production NPCs skip travel");
  }
  auto& guard = agent(npcs, "NpcGuard");
  const auto route = guard.waypoints;
  const int resume_index = guard.waypoint_index;
  const Vec3 start = guard.position;
  guard.chasing = true;
  guard.chase_target = start + Vec3{6.f, 0.f, 6.f};
  for (int frame = 0; frame < 240; ++frame) npcs.update(1.f / 60.f, .45f, {500.f, 0.f, 500.f}, 70.f);
  require(distance(guard.position, guard.chase_target) >= .45f + guard.radius - .0001f &&
              distance(guard.position, guard.chase_target) <= .45f + guard.radius + .0011f,
          "Chasing guard reaches summed-radius arrival outside distance cull");
  require(guard.waypoint_index == resume_index && same_route(guard.waypoints, route),
          "Chase preserves the actual patrol route and resume index");
  guard.chasing = false;
  bool resumed = false;
  for (int frame = 0; frame < 1800; ++frame) {
    npcs.update(1.f / 60.f, .45f);
    if (guard.waypoint_index != resume_index) { resumed = true; break; }
  }
  require(resumed, "Production bank guard reaches pending patrol waypoint after chase ends");
  auto& fence = agent(npcs, "NpcFence");
  fence.chasing = true;
  npcs.apply_schedules(false);
  require(!fence.on_duty && !fence.chasing, "Night schedule suppresses forced civilian/fence chase");
}

fury::CrewSystem crew_system() {
  fury::CrewSystem crew;
  for (auto& entry : vaultline::make_crew_roster()) crew.add(std::move(entry.member));
  return crew;
}

void test_crew_follow_and_boost() {
  const Vec3 player{7.f, 1.7f, -5.f};
  for (float yaw : {0.f, kPi * .5f, kPi, -kPi * .5f}) {
    auto crew = crew_system();
    require(near(crew.loot_speed_boost(player), 1.f), "Distant crew provide no loot boost");
    for (int frame = 0; frame < 600; ++frame) crew.update(1.f / 60.f, player, yaw, true);
    for (const auto& c : crew.members()) {
      const Vec3 forward{std::cos(yaw), 0.f, std::sin(yaw)};
      const Vec3 right{-std::sin(yaw), 0.f, std::cos(yaw)};
      Vec3 target = player + right * c.follow_offset.x + forward * c.follow_offset.z;
      target.y = c.height * .5f;
      require(same(c.position, target, .003f), c.name + ": follows behind Camera's actual facing at every cardinal yaw");
      require(c.travel_distance > 10.0, c.name + ": follow travels rather than teleporting");
    }
    require(crew.nearby_count(player, 5.5f) == 2 && near(crew.loot_speed_boost(player, 5.5f), 1.35f),
            "Both nearby active crew grant original 35 percent loot bonus");
    const auto stopped = crew.members();
    for (int frame = 0; frame < 120; ++frame) crew.update(1.f / 60.f, {100.f, 0.f, 100.f}, yaw, false);
    for (std::size_t i = 0; i < stopped.size(); ++i) {
      require(same(crew.members()[i].position, stopped[i].position) &&
                  crew.members()[i].actual_speed == 0.f, "Crew stop following after active phases");
    }
    crew.members()[0].active = false;
    const auto inactive = crew.members()[0];
    for (int frame = 0; frame < 120; ++frame) crew.update(1.f / 60.f, player, yaw, true);
    require(same(crew.members()[0].position, inactive.position) &&
                crew.members()[0].anim_phase == inactive.anim_phase,
            "Inactive crew stay still even during a job");
    require(near(crew.loot_speed_boost(player, 5.5f), 1.175f), "One active nearby crew grants 17.5 percent bonus");
    crew.members()[1].active = false;
    require(near(crew.loot_speed_boost(player, 5.5f), 1.f), "Inactive crew grant no bonus");
  }
}

std::uint64_t text_fingerprint(const std::set<std::string>& strings) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const auto& text : strings) {
    for (unsigned char byte : text) hash = (hash ^ byte) * 1099511628211ULL;
    hash = (hash ^ 255u) * 1099511628211ULL;
  }
  return hash;
}

void test_dialogue_selection_and_barks() {
  const fury::CrewSystem empty_crew;
  for (const auto& entry : vaultline::make_npc_roster()) {
    fury::NpcSystem npcs;
    npcs.add(entry.agent);
    const auto& a = npcs.agents()[0];
    const auto expected_role = a.kind == fury::NpcKind::Guard ? fury::DialogueRole::Guard :
                               a.kind == fury::NpcKind::Fence ? fury::DialogueRole::Fence :
                               fury::DialogueRole::Civilian;
    for (int direction = 0; direction < 8; ++direction) {
      const float yaw = float(direction) * kPi * .25f;
      const Vec3 forward{std::cos(yaw), 0.f, std::sin(yaw)};
      const Vec3 camera = a.position - forward * 2.f;
      const auto focus = vaultline::find_npc_talk_focus(npcs, empty_crew, camera, yaw);
      require(focus && focus.entity_name == a.entity_name && focus.label == a.label() &&
                  focus.role == expected_role, a.name + ": true camera look selects original identity/speaker/role");
      require(near(focus.nameplate_fill, .9f), a.name + ": on-axis nameplate fill preserved");
      require(!vaultline::find_npc_talk_focus(npcs, empty_crew, camera, yaw + kPi),
              a.name + ": facing away does not select dialogue");
    }
    const Vec3 camera = a.position - Vec3{5.49f, 0.f, 0.f};
    require(bool(vaultline::find_npc_talk_focus(npcs, empty_crew, camera, 0.f)), "Just inside talk radius is selectable");
    require(!vaultline::find_npc_talk_focus(npcs, empty_crew, a.position - Vec3{5.51f, 0.f, 0.f}, 0.f),
            "Beyond talk radius is excluded");
    require(!vaultline::find_npc_talk_focus(npcs, empty_crew, a.position, 0.f), "Coincident position avoids undefined facing");
    npcs.agents()[0].on_duty = false;
    require(!vaultline::find_npc_talk_focus(npcs, empty_crew, camera, 0.f), "Off-duty NPC cannot be talked to");
  }
  auto crew = crew_system();
  const fury::NpcSystem empty_npcs;
  for (std::size_t i = 0; i < crew.members().size(); ++i) {
    crew.members()[0].active = i == 0; crew.members()[1].active = i == 1;
    const auto& c = crew.members()[i];
    const auto focus = vaultline::find_npc_talk_focus(empty_npcs, crew, c.position - Vec3{2.f, 0.f, 0.f}, 0.f);
    require(focus && focus.role == fury::DialogueRole::Crew && focus.label == c.label() &&
                focus.entity_name == c.entity_name, "Rook/Sparrow keep individual speaker identity and Crew role");
  }
  fury::NpcSystem enforcer;
  fury::NpcAgent boss; boss.kind = fury::NpcKind::Enforcer; boss.name = "Enforcer";
  boss.entity_name = "NpcEnforcer"; boss.display_name = "Syndicate Enforcer";
  boss.position = {2.f, .975f, 0.f}; enforcer.add(boss);
  const auto focus = vaultline::find_npc_talk_focus(enforcer, empty_crew, {}, 0.f);
  require(focus && focus.role == fury::DialogueRole::Guard && focus.label == "Syndicate Enforcer",
          "Dynamic Enforcer remains a guard-role dialogue threat with its own speaker label");

  // Independently generated from baseline dialogue.hpp/banter.hpp at b2a695b7.
  const std::uint64_t expected_barks[] = {
    1736695126097642325ULL, 15107670116981863222ULL,
    2678156793815830292ULL, 7098616750113673870ULL
  };
  const std::uint64_t expected_banter[] = {
    15628025080496719139ULL, 7852025396619826415ULL,
    2797103322577355525ULL, 12336266420306584448ULL,
    12781344850378150058ULL, 4014082607619981312ULL
  };
  std::set<std::string> all_lines;
  for (auto role : {fury::DialogueRole::Civilian, fury::DialogueRole::Guard,
                    fury::DialogueRole::Fence, fury::DialogueRole::Crew}) {
    fury::DialogueBarks barks;
    std::set<std::string> role_lines;
    std::vector<std::string> lines{"stale"};
    for (int turn = 0; turn < 12; ++turn) {
      barks.pick(role, lines);
      require(lines.size() == std::size_t(1 + turn % 3), "Q bark rotation yields original one-to-three lines");
      for (const auto& line : lines) {
        require(!line.empty() && line != "stale", "Bark selection clears old text and returns authored content");
        role_lines.insert(line);
      }
    }
    require(role_lines.size() == 6, "All six authored barks remain reachable per role");
    require(text_fingerprint(role_lines) == expected_barks[static_cast<int>(role)],
            "Every original dialogue bark string is unchanged from the baseline");
    for (const auto& line : role_lines) require(all_lines.insert(line).second, "Dialogue role tables remain distinct");
  }
  require(all_lines.size() == 24, "All twenty-four authored bark strings available");
  fury::CrewBanter banter;
  require(std::string(banter.next_line(fury::HeistPhase::Idle)).empty(), "Idle does not invent crew banter");
  for (auto phase : {fury::HeistPhase::Approach, fury::HeistPhase::Breach, fury::HeistPhase::Looting,
                     fury::HeistPhase::Escape, fury::HeistPhase::Success, fury::HeistPhase::Failed}) {
    std::set<std::string> phase_lines;
    for (int i = 0; i < 4; ++i) {
      const std::string line = banter.next_line(phase);
      require(line.rfind("Rook:", 0) == 0 || line.rfind("Sparrow:", 0) == 0, "Phase banter keeps a named crew speaker");
      require(fury::CrewBanter::hud_label(line.c_str()) == "[CREW] " + line, "Crew banter HUD wrapper retained");
      phase_lines.insert(line);
    }
    require(phase_lines.size() == 4, "Every mission phase retains four rotating crew lines");
    require(text_fingerprint(phase_lines) == expected_banter[static_cast<int>(phase) - 1],
            "Every original phase-banter string is unchanged from the baseline");
  }
}

struct MissionFixture {
  fury::Scene scene;
  fury::NpcSystem npcs{roster_system()};
  fury::TrafficSystem traffic;
  fury::SecurityNet security;
  fury::HeistController heist;
  fury::CrewSystem crew{crew_system()};
  meridian::WishlistController world;
  std::unique_ptr<fury::Audio> audio{fury::create_null_audio()};
  MissionFixture() {
    npcs.apply_schedules(true);
    world.spawn(scene);
    // The real Wishlist spawn owns gate/security/machine/response props. The
    // bank's pre-existing door is supplied here so its collision/visibility
    // handoff can be asserted without constructing or rendering the whole city.
    fury::Entity door; door.name = "VaultDoor"; door.solid = true;
    scene.add_entity(std::move(door));
    // The bank builder owns these camera bodies, outside Wishlist::spawn.
    // Supply its authored transforms (harbor_assets.cpp, bank center 0,-10)
    // so this reduced world exercises real cameras rather than missing bodies.
    const Vec3 bank_cameras[] = {{-6.5f, 2.8f, -5.5f}, {6.5f, 2.8f, -5.5f},
                                 {0.f, 2.9f, -13.2f}};
    for (int i = 0; i < 3; ++i) {
      fury::Entity camera; camera.name = "MMCam" + std::to_string(i);
      camera.transform.position = bank_cameras[i];
      scene.add_entity(std::move(camera));
    }
    world.register_security(scene, security);
    fury::TrafficCar car; car.entity_name = "TrafficFixture";
    car.waypoints = {{20.f, 0.f, 30.f}, {-20.f, 0.f, 30.f}};
    traffic.configure({car});
    heist.vault_position = {0.f, 0.f, -15.2f};
    heist.escape_position = {fury::HeistController::MeridianEscapeSpawnX, 0.f,
                              fury::HeistController::MeridianEscapeSpawnZ};
    heist.approach_radius = 5.5f; heist.interact_radius = 3.8f; heist.escape_radius = 5.f;
    heist.loot_fail_timeout = 28.f;
    const auto& job = fury::mission_job(0);
    heist.base_payout = job.base_payout; heist.jewelry_bonus = job.jewelry_bonus;
    heist.breach_duration = job.breach_duration; heist.loot_duration = job.loot_duration;
    audio->init();
  }
  void tick(Vec3 player, bool interact = false, float dt = 1.f / 60.f) {
    npcs.update(dt, .45f);
    const auto phase = heist.phase();
    const bool follow = phase == fury::HeistPhase::Approach || phase == fury::HeistPhase::Breach ||
                        phase == fury::HeistPhase::Looting || phase == fury::HeistPhase::Escape;
    crew.update(dt, player, -kPi * .5f, follow);
    heist.loot_speed_mul = crew.loot_speed_boost(player, 5.5f);
    heist.update(player, interact, dt);
    // Public production controller integration, not apply_net_sync or forced
    // presentation states. Keyboard routing/collision navigation remain outside
    // this deterministic fixture and are explicitly not claimed as tested.
    // Match main's ordering: security reacts before this frame's world sync.
    world.update_security_gameplay(scene, npcs, security, *audio, player, dt, false);
    world.sync_from_heist(scene, npcs, traffic, heist.phase(), false);
    world.update_vault_machine(scene, heist.phase(), dt);
  }
};

// Exercise the production registrar with absent and present scene bodies.
// Coordinates are authored data, including origin/default coordinates; presence
// alone decides validity. Wishlist cameras use the body as the optional lens.
void test_security_camera_body_presence() {
  const char* names[] = {"MMCam0", "MMCam1", "MMCam2", "SecCamExtra"};
  const float yaws[] = {0.4f, 2.8f, -1.5708f, 3.14f};
  const Vec3 positions[] = {{0.f, 0.f, 0.f}, {0.f, 3.f, 0.f},
                            {0.f, 2.8f, -9.f}, {3.f, 4.f, 5.f}};
  for (unsigned mask = 0; mask < 16; ++mask) {
    fury::Scene scene;
    fury::SecurityNet security;
    meridian::WishlistController world;
    for (unsigned i = 0; i < 4; ++i) {
      if (mask & (1u << i)) {
        fury::Entity body; body.name = names[i]; body.transform.position = positions[i];
        scene.add_entity(std::move(body));
      } else {
        fury::Entity lens; lens.name = std::string(names[i]) + "Lens";
        scene.add_entity(std::move(lens));
      }
    }
    for (int cycle = 0; cycle < 2; ++cycle) {
      security.clear();
      world.register_security(scene, security);
      std::size_t expected_count = 0;
      for (unsigned i = 0; i < 4; ++i) {
        if (!(mask & (1u << i))) continue;
        const auto& c = security.cameras().at(expected_count++);
        require(c.entity_name == names[i] && c.lens_name == names[i] &&
                    same(c.position, positions[i], 0.f) && c.yaw == yaws[i] &&
                    c.site_id == 0 && c.range == 14.f && c.cone_half_rad == 0.55f,
                "Present camera retains every authored field, including origin/default coordinates");
      }
      require(security.cameras().size() == expected_count,
              "Missing body never registers a fabricated camera, even with a lens-only entity");
      if (mask == 0) {
        fury::VisibilityMeter visibility;
        require(security.update(1.f, {0.f, 1.7f, 12.f}, false, false,
                                false, 100.f, visibility) == 0.f && visibility.value == 0.f,
                "Empty camera scene adds no camera heat or visibility at spawn");
      }
    }
  }
}

using SecurityTarget = meridian::SecurityInteractionTarget;
struct SecurityTargetCase {
  const char* name;
  SecurityTarget target;
  float radius;
};
const SecurityTargetCase security_targets[] = {
  {"SecBadgePad", SecurityTarget::BadgePad, 2.6f},
  {"MMBadgeScan", SecurityTarget::BadgeScan, 2.6f},
  {"VaultMaintHatch", SecurityTarget::MaintenanceHatch, 2.4f},
  {"VaultToolPoint", SecurityTarget::VaultToolPoint, 2.4f},
  {"VaultEmergPanel", SecurityTarget::EmergencyPanel, 2.4f},
  {"SecTerminalA", SecurityTarget::TerminalA, 2.5f},
  {"SecTerminalB", SecurityTarget::TerminalB, 2.5f}
};
void add_security_entity(fury::Scene& scene, const char* name, Vec3 position = {},
                         bool visible = true) {
  fury::Entity e;
  e.name = name; e.transform.position = position; e.visible = visible;
  e.solid = true; e.material.albedo = {.4f, .5f, .6f}; e.material.emissive = .3f;
  scene.add_entity(std::move(e));
}

// Query through const references repeatedly, then verify every existing action
// effect remains untouched. This catches a prompt that accidentally performs
// its action while selecting the affordance, including while off screen.
void expect_security_query(const meridian::WishlistController& world,
                           const fury::Scene& scene, Vec3 player,
                           SecurityTarget expected, const std::string& context) {
  const auto before = world;
  const auto entities_before = scene.entities();
  for (int i = 0; i < 3; ++i) {
    require(world.security_interaction_at(scene, player) == expected, context);
  }
  require(world.badge_unlocked == before.badge_unlocked &&
              world.lockdown_active == before.lockdown_active &&
              world.vault_seq_t == before.vault_seq_t &&
              world.vault_seq_stage == before.vault_seq_stage &&
              world.vault_unlocked == before.vault_unlocked &&
              world.world_state == before.world_state &&
              world.guard_investigating == before.guard_investigating &&
              world.guard_escalated == before.guard_escalated &&
              world.guard_react_t == before.guard_react_t,
          context + ": query leaves controller action state unchanged");
  require(scene.entities().size() == entities_before.size(), context + ": query preserves entity count");
  for (std::size_t i = 0; i < entities_before.size(); ++i) {
    const auto& a = entities_before[i];
    const auto& b = scene.entities()[i];
    require(a.name == b.name && a.visible == b.visible && a.solid == b.solid &&
                same(a.transform.position, b.transform.position, 0.f) &&
                same(a.transform.rotation_euler, b.transform.rotation_euler, 0.f) &&
                same(a.transform.scale, b.transform.scale, 0.f) &&
                same(a.material.albedo, b.material.albedo, 0.f) &&
                a.material.emissive == b.material.emissive,
            context + ": query leaves scene order, transforms, gate and material state unchanged");
  }
}

void test_security_selector_radii_and_presence() {
  for (const auto& c : security_targets) {
    const float inside = std::nextafter(c.radius, 0.f);
    const float outside = std::nextafter(c.radius, std::numeric_limits<float>::infinity());
    // Single-axis floats make the exact inclusive boundary unambiguous. Y is
    // deliberately far away: the existing interaction contract is XZ only.
    for (float offset : {inside, c.radius, outside}) {
      for (bool along_x : {true, false}) {
        fury::Scene scene; fury::SecurityNet security; meridian::WishlistController world;
        add_security_entity(scene, c.name, {0.f, -100.f, 0.f}, false);
        const Vec3 player = along_x ? Vec3{offset, 100.f, 0.f} : Vec3{0.f, 100.f, -offset};
        const bool in_range = offset <= c.radius;
        expect_security_query(world, scene, player, in_range ? c.target : SecurityTarget::None,
                              std::string(c.name) + ": exact radius, adjacent float and XZ-only hidden target");
        require(world.try_security_interact(scene, security, player) == in_range,
                std::string(c.name) + ": action consumes precisely the query's inclusive radius");
      }
    }
    fury::Scene scene; fury::SecurityNet security; meridian::WishlistController world;
    add_security_entity(scene, c.name, {20.f, 0.f, 20.f}, false);
    add_security_entity(scene, c.name);  // nearest duplicate must not win
    expect_security_query(world, scene, {}, SecurityTarget::None,
                          std::string(c.name) + ": first named entity wins even when later duplicate is near");
    require(!world.try_security_interact(scene, security, {}), "Action also rejects a later in-range duplicate");
    scene.entities().front().transform.position = {};
    scene.entities().back().transform.position = {20.f, 0.f, 20.f};
    expect_security_query(world, scene, {}, c.target,
                          std::string(c.name) + ": hidden first duplicate remains interactable");
    require(world.try_security_interact(scene, security, {}), "Action accepts the same hidden first duplicate");
  }
  fury::Scene empty; fury::SecurityNet security; meridian::WishlistController world;
  expect_security_query(world, empty, {}, SecurityTarget::None, "Missing entities do not fabricate origin targets");
  require(!world.try_security_interact(empty, security, {}), "Empty scene does not consume interaction");
  add_security_entity(empty, "SecBadgePadDecoration");
  add_security_entity(empty, "VaultToolPoint_1");
  expect_security_query(world, empty, {}, SecurityTarget::None, "Partial name matches are not actionable targets");
}

void test_security_selector_order_and_states() {
  // For every pair, insert the lower-priority target first and place it closer.
  // Selection must still follow the old branch/alias order, not scene or distance order.
  for (std::size_t first = 0; first < std::size(security_targets); ++first) {
    for (std::size_t second = first + 1; second < std::size(security_targets); ++second) {
      fury::Scene scene; fury::SecurityNet security; meridian::WishlistController world;
      add_security_entity(scene, security_targets[second].name);
      add_security_entity(scene, security_targets[first].name,
                          {security_targets[first].radius - .05f, 0.f, 0.f});
      add_security_entity(scene, "SecLobbyGate");
      world.lockdown_active = true; security.set_lockdown(true);
      expect_security_query(world, scene, {}, security_targets[first].target,
                            std::string(security_targets[first].name) + " retains priority over nearer " +
                                security_targets[second].name);
      require(world.try_security_interact(scene, security, {}), "Selected priority branch consumes interaction");
      if (first <= 1) {
        require(world.badge_unlocked && !world.lockdown_active && !security.lockdown() &&
                    world.vault_seq_t == 0.f && !scene.find_by_name("SecLobbyGate")->solid,
                "Higher-priority badge branch opens gate without engaging a nearer tool");
      } else if (first <= 3) {
        require(world.vault_seq_t == .2f && !world.badge_unlocked && world.lockdown_active &&
                    security.lockdown() && scene.find_by_name("SecLobbyGate")->solid,
                "Higher-priority maintenance/tool branch does not activate a nearer terminal");
      } else if (first == 4) {
        require(world.vault_seq_t == 0.f && !world.badge_unlocked && world.lockdown_active &&
                    security.lockdown() && scene.find_by_name("SecLobbyGate")->solid,
                "Higher-priority emergency branch consumes without terminal bypass side effects");
      } else {
        require(world.badge_unlocked && !world.lockdown_active && !security.lockdown(),
                "Selected terminal retains lockdown bypass effect");
      }
    }
  }
  for (bool unlocked : {false, true}) {
    for (bool lockdown : {false, true}) {
      fury::Scene scene; fury::SecurityNet security; meridian::WishlistController world;
      add_security_entity(scene, "SecBadgePad"); add_security_entity(scene, "VaultToolPoint");
      add_security_entity(scene, "SecLobbyGate");
      world.badge_unlocked = unlocked; world.lockdown_active = lockdown;
      security.set_lockdown(lockdown);
      const bool badge_eligible = !unlocked || lockdown;
      expect_security_query(world, scene, {}, badge_eligible ? SecurityTarget::BadgePad : SecurityTarget::VaultToolPoint,
                            "Badge availability depends on both unlocked and lockdown state");
      require(world.try_security_interact(scene, security, {}), "Badge state matrix always has an eligible action");
      const auto* gate = scene.find_by_name("SecLobbyGate");
      const auto* pad = scene.find_by_name("SecBadgePad");
      if (badge_eligible) {
        require(world.badge_unlocked && !world.lockdown_active && !security.lockdown() &&
                    !gate->solid && !gate->visible && world.vault_seq_t == 0.f &&
                    same(pad->material.albedo, {.25f, .95f, .45f}, 0.f) && pad->material.emissive == 1.4f,
                "Badge action preserves unlock, both lockdown states, gate and pad feedback");
      } else {
        require(world.badge_unlocked && !world.lockdown_active && !security.lockdown() &&
                    gate->solid && gate->visible && world.vault_seq_t == .2f && pad->material.emissive == .3f,
                "Already unlocked badge falls through to the tool without replaying badge effects");
      }
    }
  }
  for (const char* terminal : {"SecTerminalA", "SecTerminalB"}) {
    for (bool lockdown : {false, true}) {
      fury::Scene scene; fury::SecurityNet security; meridian::WishlistController world;
      add_security_entity(scene, terminal); add_security_entity(scene, "SecLobbyGate");
      world.lockdown_active = lockdown; security.set_lockdown(lockdown);
      expect_security_query(world, scene, {}, terminal[11] == 'A' ? SecurityTarget::TerminalA : SecurityTarget::TerminalB,
                            "Terminal remains actionable with or without lockdown");
      require(world.try_security_interact(scene, security, {}), "Terminal action consumes interaction");
      require(world.badge_unlocked == lockdown && !world.lockdown_active && !security.lockdown() &&
                  scene.find_by_name("SecLobbyGate")->solid == !lockdown &&
                  scene.find_by_name("SecLobbyGate")->visible == !lockdown && !security.site_disabled(0),
              "Terminal opens gate only during lockdown and never trips camera power on its own");
    }
  }
  // The scanner alias updates the pad's feedback even if that pad is out of range.
  fury::Scene scene; fury::SecurityNet security; meridian::WishlistController world;
  add_security_entity(scene, "MMBadgeScan"); add_security_entity(scene, "SecBadgePad", {50.f, 0.f, 50.f});
  add_security_entity(scene, "SecLobbyGate");
  expect_security_query(world, scene, {}, SecurityTarget::BadgeScan, "Scanner alias works independently of pad distance");
  require(world.try_security_interact(scene, security, {}) && world.badge_unlocked &&
              !scene.find_by_name("SecLobbyGate")->solid && scene.find_by_name("SecBadgePad")->material.emissive == 1.4f,
          "Scanner alias retains shared badge side effects");
  expect_security_query(world, scene, {}, SecurityTarget::None, "Unlocked scanner disappears when no later branch is near");
  require(!world.try_security_interact(scene, security, {}), "Unlocked scanner alone no longer consumes interaction");
}

void test_security_authored_positions_and_repeat_tool() {
  MissionFixture f;
  const Vec3 badge = f.scene.find_by_name("SecBadgePad")->transform.position;
  require(f.world.try_security_interact(f.scene, f.security, badge) && f.world.badge_unlocked,
          "Authored-position regression begins with a real badge unlock");
  const Vec3 a{-.0597607f, 1.7f, -12.5154f};
  const Vec3 b{-.0597608f, 1.7f, -11.7926f};
  expect_security_query(f.world, f.scene, a, SecurityTarget::VaultToolPoint,
                        "Position A selects the actual in-range vault tool after badge unlock");
  expect_security_query(f.world, f.scene, b, SecurityTarget::None,
                        "Position B has no eligible wishlist interaction after badge unlock");
  require(!f.security.near_live_breaker(a) && !f.security.near_live_breaker(b),
          "Neither authored regression position has a live breaker fallback");
  require(!f.world.try_security_interact(f.scene, f.security, b) && f.world.vault_seq_t == 0.f,
          "Position B does not consume E or progress the vault tool");
  require(f.world.try_security_interact(f.scene, f.security, a) && f.world.vault_seq_t == .2f,
          "Position A consumes E and engages the vault tool");
  for (float progress : {.2f, .7f}) {
    f.world.vault_seq_t = progress;
    expect_security_query(f.world, f.scene, a, SecurityTarget::VaultToolPoint,
                          "Tool stays actionable after earlier engagement or later vault progress");
    require(f.world.try_security_interact(f.scene, f.security, a) && f.world.vault_seq_t == progress,
            "Repeated tool interaction still consumes E without lowering existing vault progress");
  }
}

void test_security_compound_breaker_action() {
  MissionFixture f;
  const Vec3 badge = f.scene.find_by_name("SecBadgePad")->transform.position;
  const auto camera = f.security.cameras().back();
  const Vec3 observed = camera.position + Vec3{std::cos(camera.yaw) * 3.f, 0.f, std::sin(camera.yaw) * 3.f};
  fury::VisibilityMeter visibility;
  require(f.security.update(.1f, observed, false, false, false, 100.f, visibility) > 0.f,
          "Compound-action fixture begins with a real live camera cone");
  f.world.lockdown_active = true; f.security.set_lockdown(true);
  expect_security_query(f.world, f.scene, badge, SecurityTarget::BadgePad,
                        "Live breaker overlap retains the wishlist badge action");
  require(f.security.near_live_breaker(badge) && f.security.lockdown() && !f.security.site_disabled(0),
          "Prompt queries leave breaker, camera power and network lockdown untouched");
  // The production live-breaker branch intentionally executes BOTH calls in
  // this order, even when the wishlist action consumed E. This checks their
  // concrete gate/camera effects; keyboard routing and rendering are not claimed.
  require(f.world.try_security_interact(f.scene, f.security, badge), "Compound action first consumes the badge interaction");
  require(f.world.badge_unlocked && !f.world.lockdown_active && !f.security.lockdown() &&
              !f.scene.find_by_name("SecLobbyGate")->solid && !f.scene.find_by_name("SecLobbyGate")->visible &&
              f.security.near_live_breaker(badge) && !f.security.site_disabled(0),
          "Wishlist action opens gate and clears lockdown while leaving the real breaker live");
  require(f.security.try_trip_breaker(badge) == 0 && f.security.site_disabled(0) &&
              !f.security.near_live_breaker(badge),
          "Same interaction then trips the real registered breaker and disables site cameras");
  require(f.security.update(.1f, observed, false, false, false, 100.f, visibility) == 0.f,
          "Compound interaction's camera power effect removes the actual cone heat");
  expect_security_query(f.world, f.scene, badge, SecurityTarget::TerminalB,
                        "After badge unlock and breaker trip, authored TerminalB remains the next eligible action");
  require(f.world.try_security_interact(f.scene, f.security, badge) && f.security.try_trip_breaker(badge) == -1,
          "Repeated interaction consumes the remaining terminal without retripping the breaker");
}

void test_security_investigation_and_bypass() {
  for (bool badge : {true, false}) {
    MissionFixture f;
    require(f.security.cameras().size() == 4 && f.security.breakers().size() >= 1,
            "Production Meridian camera and badge registrations exist");
    const auto camera = f.security.cameras().back();
    const Vec3 observed = camera.position + Vec3{std::cos(camera.yaw) * 3.f, 0.f, std::sin(camera.yaw) * 3.f};
    fury::VisibilityMeter visibility;
    const float heat = f.security.update(.1f, observed, false, false, false, 100.f, visibility);
    require(heat > 0.f && visibility.normalized() > 0.f, "A real camera cone raises heat and detection");
    const auto desk_before = agent(f.npcs, "NpcDeskGuard");
    const auto bank_before = agent(f.npcs, "NpcGuard");
    f.world.update_security_gameplay(f.scene, f.npcs, f.security, *f.audio, observed, .1f, false);
    for (const auto& before : {desk_before, bank_before}) {
      const auto& after = agent(f.npcs, before.entity_name.c_str());
      require(same(after.position, before.position) && same(after.home, before.home) &&
                  same_route(after.waypoints, before.waypoints) &&
                  same_route(after.base_waypoints, before.base_waypoints) &&
                  after.waypoint_index == before.waypoint_index &&
                  after.travel_distance == before.travel_distance,
              before.name + ": camera alert never teleports or rewrites physical position/home/patrol/index");
    }
    require(f.world.guard_investigating && agent(f.npcs, "NpcDeskGuard").chasing &&
                agent(f.npcs, "NpcGuard").chasing, "Actual cone hit triggers named guard investigation/chase");
    for (int i = 0; i < 180; ++i) {
      const Vec3 previous_desk = agent(f.npcs, "NpcDeskGuard").position;
      const Vec3 previous_bank = agent(f.npcs, "NpcGuard").position;
      f.npcs.update(1.f / 60.f, .45f);
      require(distance(previous_desk, agent(f.npcs, "NpcDeskGuard").position) <=
                  desk_before.chase_speed / 60.f + .0001f &&
                  distance(previous_bank, agent(f.npcs, "NpcGuard").position) <=
                  bank_before.chase_speed / 60.f + .0001f,
              "Alerted guards approach through distance-limited actual locomotion");
      f.world.update_security_gameplay(f.scene, f.npcs, f.security, *f.audio, observed, 1.f / 60.f, false);
      if (i < 140) require(!f.world.guard_escalated, "Investigation cannot escalate before its 2.5-second reaction timer");
    }
    require(f.world.guard_escalated && f.world.lockdown_active && f.security.lockdown(),
            "Investigation escalates after 2.5 seconds and both lockdown states agree");
    require(f.scene.find_by_name("SecLobbyGate")->solid && f.scene.find_by_name("SecLobbyGate")->visible,
            "Escalation closes the real lobby gate");
    require(!f.world.try_security_interact(f.scene, f.security, {200.f, 0.f, 200.f}),
            "Out-of-range interaction cannot bypass security");
    // TerminalA is outside the badge radius; TerminalB overlaps that radius.
    const Vec3 interact = f.scene.find_by_name(badge ? "SecBadgePad" : "SecTerminalA")->transform.position;
    if (!badge) require(distance(interact, f.scene.find_by_name("SecBadgePad")->transform.position) > 2.6f,
                        "Terminal case genuinely exercises terminal branch, outside badge radius");
    require(f.world.try_security_interact(f.scene, f.security, interact), "Badge/terminal consumes an in-range interaction");
    require(f.world.badge_unlocked && !f.world.lockdown_active && !f.security.lockdown(),
            "Badge/terminal immediately clears BOTH controller and SecurityNet lockdown");
    require(!f.scene.find_by_name("SecLobbyGate")->solid && !f.scene.find_by_name("SecLobbyGate")->visible,
            "Badge/terminal opens the physical gate");
    f.world.update_security_gameplay(f.scene, f.npcs, f.security, *f.audio, observed, .1f, true);
    require(!f.security.lockdown() && !f.world.lockdown_active, "Authorized bypass survives later alarm ticks");
    for (int i = 0; i < 240; ++i) f.npcs.update(1.f / 60.f, .45f);
    for (const auto& before : {desk_before, bank_before}) {
      const auto& after = agent(f.npcs, before.entity_name.c_str());
      require(distance(after.position, observed) >= .45f + after.radius - .0001f &&
                  distance(after.position, observed) <= .45f + after.radius + .0011f &&
                  after.travel_distance > 1.0,
              before.name + ": physically reaches sighting with summed-radius clearance");
      require(same_route(after.waypoints, before.waypoints) &&
                  after.waypoint_index == before.waypoint_index,
              before.name + ": sustained investigation preserves pending patrol target");
      // Exercise heat with the actual settled guard, rather than camera heat.
      for(auto phase:{fury::HeistPhase::Breach,fury::HeistPhase::Looting,fury::HeistPhase::Escape}) {
        fury::HeatMeter guard_heat;
        require(guard_heat.guard_radius==8.5f,"Guard heat radius remains authored 8.5 m");
        const bool failed=guard_heat.update(.25f,phase,observed,after.position,false);
        require(guard_heat.value>0.f && !failed,"Stopped threat retains phase-appropriate guard heat pressure");
        if(phase!=fury::HeistPhase::Escape) {
          guard_heat.value=.99f;
          require(guard_heat.update(.25f,phase,observed,after.position,false),
                  "Stopped threat still reaches risky maximum-heat failure");
        }
      }
      fury::HeatMeter idle_heat;idle_heat.value=1.f;
      require(!idle_heat.update(.25f,fury::HeistPhase::Idle,observed,after.position,false)&&
                  idle_heat.value<1.f,"Idle remains nonfailing and decays guard heat");
    }
    // Chase cancellation is a controller input; this checks the preserved route
    // can actually resume, without inventing an automatic alert-expiry rule.
    f.world.guard_investigating = false; f.world.guard_escalated = false;
    agent(f.npcs, "NpcDeskGuard").chasing = false;
    agent(f.npcs, "NpcGuard").chasing = false;
    int resumed_targets = 0;
    for (int i = 0; i < 1200; ++i) {
      const int previous_index = agent(f.npcs, "NpcDeskGuard").waypoint_index;
      f.npcs.update(1.f / 60.f, .45f);
      resumed_targets += agent(f.npcs, "NpcDeskGuard").waypoint_index != previous_index;
    }
    require(resumed_targets >= 2 && agent(f.npcs, "NpcDeskGuard").travel_distance > 8.0 &&
                same_route(agent(f.npcs, "NpcDeskGuard").waypoints, desk_before.waypoints),
            "Investigation guard resumes traversing its original patrol after chase cancellation");
  }
  MissionFixture f;
  const Vec3 badge = f.scene.find_by_name("SecBadgePad")->transform.position;
  require(f.security.near_live_breaker(badge) && f.security.try_trip_breaker(badge) == 0,
          "Real badge breaker disables the Meridian camera site");
  fury::VisibilityMeter visibility;
  const auto camera = f.security.cameras().back();
  const Vec3 observed = camera.position + Vec3{std::cos(camera.yaw) * 3.f, 0.f, std::sin(camera.yaw) * 3.f};
  require(f.security.update(.1f, observed, false, false, false, 100.f, visibility) == 0.f,
          "Disabled camera site stops generating camera heat");
}

void test_genuine_meridian_mission_and_reset() {
  MissionFixture f;
  const auto original = f.npcs.agents();
  const Vec3 vault = f.heist.vault_position;
  const Vec3 approach = vault + Vec3{0.f, 1.7f, 4.5f};
  require(f.heist.phase() == fury::HeistPhase::Idle, "Real heist starts idle");
  f.tick(approach);
  require(f.heist.phase() == fury::HeistPhase::Approach, "Entering real approach radius starts approach");
  f.tick(approach, true);
  require(f.heist.phase() == fury::HeistPhase::Approach, "Out-of-breach-range interaction cannot skip approach");
  f.tick({200.f, 0.f, 200.f});
  require(f.heist.phase() == fury::HeistPhase::Idle, "Leaving approach radius cancels approach");
  f.tick(vault);
  for (int i = 0; i < 420; ++i) f.tick(vault);  // allow actual crew travel into range
  require(f.crew.nearby_count(vault, 5.5f) == 2, "Both original crew follow into heist loot radius");
  f.tick(vault, true);
  require(f.heist.phase() == fury::HeistPhase::Breach && f.world.vault_seq_stage == 1,
          "An actual interaction enters breach and drives the vault machine");
  int breach_frames = 0;
  while (f.heist.phase() == fury::HeistPhase::Breach && breach_frames++ < 240) f.tick(vault);
  require(f.heist.phase() == fury::HeistPhase::Looting && breach_frames >= 100,
          "Real 1.8-second breach timer reaches looting without forced phase injection");
  require(f.world.world_state == meridian::MissionWorldState::Alarm &&
              f.scene.find_by_name("AlarmShutterL")->visible && f.world.aftermath_spawned,
          "Real looting drives alarm shutters and aftermath");
  require(distance(agent(f.npcs, "NpcGuard").position, {0.f, 0.f, -8.5f}) < .1f &&
              distance(agent(f.npcs, "NpcBankCust").position, {0.f, 0.f, -2.5f}) < .1f,
          "Alarm relocates production guard and fleeing customer by stable entity names");
  require(agent(f.npcs, "NpcGuard").actual_speed == 0.f &&
              same(agent(f.npcs, "NpcGuard").velocity, {}) &&
              agent(f.npcs, "NpcGuard").move_weight == 0.f,
          "Mission relocation clears stale locomotion signals instead of inventing a stride");
  // Drive the production alarm input, then verify that a real Escape transition
  // releases both lockdown representations, even without a prior badge bypass.
  f.world.update_security_gameplay(f.scene, f.npcs, f.security, *f.audio, vault, 1.f / 60.f, true);
  require(f.world.lockdown_active && f.security.lockdown(), "Alarm input establishes lockdown before escape");
  const float paused_remaining = f.heist.loot_remaining();
  f.heist.loot_pause_remaining = .25f;
  for (int i = 0; i < 12; ++i) f.tick(vault);
  require(near(f.heist.loot_remaining(), paused_remaining), "Lock-jam complication pauses real loot countdown");
  f.heist.loot_pause_remaining = 0.f;
  int loot_frames = 0;
  while (f.heist.phase() == fury::HeistPhase::Looting && loot_frames++ < 600) f.tick(vault);
  require(f.heist.phase() == fury::HeistPhase::Escape && loot_frames < 240,
          "Nearby-crew multiplier accelerates genuine 4.5-second loot timer");
  require(f.heist.inventory().loot_bags == 2 && near(f.heist.loot_progress(), 1.f), "Escape carries two full loot bags");
  require(f.world.world_state == meridian::MissionWorldState::Escape &&
              f.scene.find_by_name("EscapeBlocker0")->visible &&
              f.scene.find_by_name("EscapeResponseVan")->visible &&
              near(f.traffic.cars()[0].cruise_speed, 11.5f),
          "Real escape drives blockers, response vehicle visibility and traffic pace");
  require(!f.scene.find_by_name("SecLobbyGate")->solid && !f.world.lockdown_active,
          "Real escape transition immediately opens gate and clears controller lockdown");
  f.tick(vault);  // next security tick mirrors the just-applied world transition
  require(!f.security.lockdown(), "Escape releases network lockdown on next production-order security tick");
  require(f.world.vault_unlocked && !f.scene.find_by_name("VaultDoor")->solid,
          "Real mission vault machine releases the physical vault door");
  require(distance(agent(f.npcs, "NpcAlleyHmpd").position, {13.5f, 0.f, -18.f}) < .1f,
          "Alley officer relocates to the authored escape response route");
  f.tick(f.heist.escape_position);
  require(f.heist.phase() == fury::HeistPhase::Success && f.heist.last_payout() > 9000 &&
              f.heist.score().successes == 1 && f.heist.score().failures == 0 &&
              f.heist.inventory().cash == f.heist.last_payout() && f.heist.inventory().loot_bags == 0,
          "Reaching actual extraction completes payout exactly once and clears carried bags");
  const int cash = f.heist.inventory().cash;
  f.tick(f.heist.escape_position);
  require(f.heist.inventory().cash == cash && f.heist.score().successes == 1, "Success does not duplicate payout on later frames");
  f.tick(f.heist.escape_position, true);
  require(f.heist.phase() == fury::HeistPhase::Idle &&
              f.world.world_state == meridian::MissionWorldState::PreHeist,
          "Interaction resets the successful heist back to pre-heist world state");
  for (const char* name : {"NpcGuard", "NpcDeskGuard", "NpcAlleyHmpd", "NpcBankCust", "NpcTeller"}) {
    const auto& restored = agent(f.npcs, name);
    const auto it = std::find_if(original.begin(), original.end(), [&](const fury::NpcAgent& a) { return a.entity_name == name; });
    require(it != original.end() && same_route(restored.waypoints, it->waypoints) &&
                same_route(restored.base_waypoints, it->base_waypoints) &&
                same(restored.home, it->home) && same(restored.position, it->position) &&
                near(restored.speed, it->speed) && !restored.chasing,
            std::string(name) + ": reset restores authored spawn/home/day route/pace after mission relocations");
  }
  f.npcs.apply_schedules(false);
  f.npcs.apply_schedules(true);
  require(same_route(agent(f.npcs, "NpcGuard").waypoints, original[3].waypoints),
          "Schedule changes after reset retain original bank-guard route");
  require(!f.scene.find_by_name("EscapeBlocker0")->visible && !f.world.aftermath_spawned,
          "Reset hides escape dressing and clears aftermath state");
}

void test_mission_failure_paths() {
  fury::HeistController heist;
  heist.update({}, false, .1f); heist.update({}, true, .1f);
  heist.update({100.f, 0.f, 100.f}, false, .1f);
  require(heist.phase() == fury::HeistPhase::Failed && heist.score().failures == 1 &&
              heist.last_payout() == 0, "Leaving vault during real breach fails without payout");
  heist.update({}, true, .1f);
  require(heist.phase() == fury::HeistPhase::Idle, "Failed job can reset");
  MissionFixture f;
  f.tick(f.heist.vault_position);
  f.tick(f.heist.vault_position, true);
  fury::HeatMeter heat;
  fury::VisibilityMeter visibility;
  const auto camera = f.security.cameras().back();
  const Vec3 observed = camera.position + Vec3{std::cos(camera.yaw) * 3.f, 0.f,
                                               std::sin(camera.yaw) * 3.f};
  bool visited_looting = false;
  for (int frame = 0; frame < 1000 && f.heist.phase() != fury::HeistPhase::Failed; ++frame) {
    constexpr float dt = 1.f / 60.f;
    heat.value = std::min(1.f, heat.value + f.security.update(
        dt, observed, false, false, true, 0.f, visibility));
    f.tick(observed);
    visited_looting = visited_looting || f.heist.phase() == fury::HeistPhase::Looting;
    if (heat.update(dt, f.heist.phase(), observed, agent(f.npcs, "NpcDeskGuard").position, false) ||
        (heat.is_max() && f.heist.phase() == fury::HeistPhase::Escape)) {
      f.heist.force_fail();
    }
  }
  require(visited_looting && heat.is_max() && f.heist.phase() == fury::HeistPhase::Failed &&
              f.heist.score().failures == 1 && f.heist.last_payout() == 0 &&
              f.heist.inventory().loot_bags == 0,
          "Real advancing breach/loot plus camera detection and guard heat can burn the job without payout");
  f.tick(observed);
  require(f.world.world_state == meridian::MissionWorldState::Escape,
          "Failed actual mission retains its intended response-world state");
  f.tick(f.heist.escape_position, true);
  require(f.heist.phase() == fury::HeistPhase::Idle &&
              f.world.world_state == meridian::MissionWorldState::PreHeist,
          "Failed mission resets the actual world as well as the heist controller");
  require(!f.world.guard_investigating && !f.world.guard_escalated && !f.world.lockdown_active &&
              f.world.guard_react_t == 0.f,
          "Failed-job reset clears stale guard investigation/escalation before the next patrol tick");
  auto originals = roster_system();
  originals.apply_schedules(true);
  for (const char* name : {"NpcGuard", "NpcAlleyHmpd", "NpcDeskGuard", "NpcBankCust", "NpcTeller"}) {
    require(same_route(agent(f.npcs, name).waypoints, agent(originals, name).waypoints),
            std::string(name) + ": failed-job reset restores authored patrols");
  }
}

}  // namespace

int main() {
  try {
    test_authored_roster();
    test_real_routes_and_schedules();
    test_chase_and_culling();
    test_crew_follow_and_boost();
    test_dialogue_selection_and_barks();
    test_security_camera_body_presence();
    test_security_selector_radii_and_presence();
    test_security_selector_order_and_states();
    test_security_authored_positions_and_repeat_tool();
    test_security_compound_breaker_action();
    test_security_investigation_and_bypass();
    test_genuine_meridian_mission_and_reset();
    test_mission_failure_paths();
    std::cout << "npc_gameplay: " << checks << " checks passed; actual 12-NPC roster, 30/60 Hz patrols, "
              << "schedule/chase/crew/dialogue/security and real Meridian state-machine flow\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "npc_gameplay FAILED after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
