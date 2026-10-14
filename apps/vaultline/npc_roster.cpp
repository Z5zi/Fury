#include "npc_roster.hpp"
#include <utility>
namespace vaultline {
std::vector<NpcSpawnSpec> make_npc_roster() {
  std::vector<NpcSpawnSpec> result;
  result.reserve(12);
  {
    fury::NpcAgent a;
    a.name = "CivA";
    a.display_name = "Mira Vale";
    a.entity_name = "NpcCivA";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.75f;
    a.position = {-12.f, 0.875f, 10.f};
    a.speed = 2.4f;
    a.waypoints = {{-12.f, 0.f, 10.f}, {12.f, 0.f, 10.f}, {12.f, 0.f, -18.f},
                   {-12.f, 0.f, -18.f}};
    a.schedule = fury::NpcSchedule::Always;  // sparse night presence
    a.home = {-12.f, 0.f, 10.f};
    result.push_back({std::move(a), {0.55f, 0.72f, 0.85f}});
  }
  {
    fury::NpcAgent a;
    a.name = "CivB";
    a.display_name = "Jon Keel";
    a.entity_name = "NpcCivB";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.7f;
    a.position = {18.f, 0.85f, 22.f};
    a.speed = 2.1f;
    a.waypoints = {{18.f, 0.f, 22.f}, {34.f, 0.f, 22.f}, {34.f, 0.f, 8.f},
                   {18.f, 0.f, 8.f}};
    a.schedule = fury::NpcSchedule::DayOnly;
    a.home = {18.f, 0.f, 22.f};
    result.push_back({std::move(a), {0.85f, 0.62f, 0.45f}});
  }
  {
    fury::NpcAgent a;
    a.name = "CivC";
    a.display_name = "Tessa Quill";
    a.entity_name = "NpcCivC";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.65f;
    a.position = {88.f, 0.825f, 8.f};
    a.speed = 2.0f;
    a.waypoints = {{88.f, 0.f, 8.f}, {102.f, 0.f, 8.f}, {102.f, 0.f, 18.f},
                   {88.f, 0.f, 18.f}, {70.f, 0.f, 6.f}};
    a.schedule = fury::NpcSchedule::DayOnly;
    a.home = {88.f, 0.f, 8.f};
    result.push_back({std::move(a), {0.65f, 0.80f, 0.55f}});
  }
  {
    fury::NpcAgent g;
    g.name = "BankGuard";
    g.display_name = "Sgt. Hale";
    g.entity_name = "NpcGuard";
    g.kind = fury::NpcKind::Guard;
    g.height = 1.85f;
    g.position = {4.f, 0.925f, -2.f};
    g.speed = 1.6f;
    g.chase_speed = 3.5f;
    g.waypoints = {{4.f, 0.f, -2.f}, {-4.f, 0.f, -2.f}, {-4.f, 0.f, 4.f},
                   {4.f, 0.f, 4.f}, {0.f, 0.f, -6.f}};
    g.schedule = fury::NpcSchedule::NightTighten;
    g.home = {0.f, 0.f, 0.f};
    result.push_back({std::move(g), {0.25f, 0.35f, 0.55f}});
  }
  // Meridian Mutual interior anchors (teller / desk guard / lobby customer / alley HMPD)
  {
    fury::NpcAgent a;
    a.name = "BankTeller";
    a.display_name = "Lia Merrow";
    a.entity_name = "NpcTeller";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.68f;
    a.position = {0.15f, 0.84f, -8.9f};
    a.speed = 0.35f;
    a.waypoints = {{0.15f, 0.f, -8.9f}, {-0.6f, 0.f, -8.9f}, {0.7f, 0.f, -8.9f}};
    a.schedule = fury::NpcSchedule::DayOnly;
    a.home = {0.15f, 0.f, -8.9f};
    result.push_back({std::move(a), {0.42f, 0.55f, 0.62f}});
  }
  {
    fury::NpcAgent g;
    g.name = "BankDeskGuard";
    g.display_name = "Ofc. Renn";
    g.entity_name = "NpcDeskGuard";
    g.kind = fury::NpcKind::Guard;
    g.height = 1.82f;
    g.position = {-5.1f, 0.91f, -9.4f};
    g.speed = 1.15f;
    g.chase_speed = 3.4f;
    g.waypoints = {{-5.1f, 0.f, -9.4f}, {-4.2f, 0.f, -11.5f}, {-5.8f, 0.f, -8.2f}};
    g.schedule = fury::NpcSchedule::Always;
    g.home = {-5.1f, 0.f, -9.4f};
    result.push_back({std::move(g), {0.22f, 0.32f, 0.48f}});
  }
  {
    fury::NpcAgent a;
    a.name = "BankCustomer";
    a.display_name = "Owen Pike";
    a.entity_name = "NpcBankCust";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.74f;
    a.position = {1.2f, 0.87f, -5.2f};
    a.speed = 0.9f;
    a.waypoints = {{1.2f, 0.f, -5.2f}, {-1.0f, 0.f, -4.8f}, {0.4f, 0.f, -5.6f}};
    a.schedule = fury::NpcSchedule::DayOnly;
    a.home = {1.2f, 0.f, -5.2f};
    result.push_back({std::move(a), {0.62f, 0.48f, 0.40f}});
  }
  {
    fury::NpcAgent g;
    g.name = "AlleyHmpd";
    g.display_name = "Ofc. Vale";
    g.entity_name = "NpcAlleyHmpd";
    g.kind = fury::NpcKind::Guard;
    g.height = 1.86f;
    g.position = {16.2f, 0.93f, -12.0f};
    g.speed = 1.4f;
    g.chase_speed = 3.8f;
    g.waypoints = {{16.2f, 0.f, -12.0f}, {14.5f, 0.f, -15.5f}, {17.5f, 0.f, -10.5f}};
    g.schedule = fury::NpcSchedule::NightTighten;
    g.home = {16.2f, 0.f, -12.0f};
    result.push_back({std::move(g), {0.18f, 0.28f, 0.55f}});
  }
  // Ashcourt civilian
  {
    fury::NpcAgent a;
    a.name = "CivAsh";
    a.display_name = "Nell Ash";
    a.entity_name = "NpcCivAsh";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.75f;
    a.position = {-88.f, 0.875f, 42.f};
    a.speed = 1.9f;
    // Walk around the market fixtures and scenery; preserve home and activity order.
    a.waypoints = {{-88.0f, 0.f, 42.0f},
                   {-80.7f, 0.f, 42.0f},
                   {-80.8f, 0.f, 49.1f},
                   {-80.94f, 0.f, 48.94f},
                   {-83.16f, 0.f, 47.16f},
                   {-83.19f, 0.f, 45.84f},
                   {-88.81f, 0.f, 45.84f},
                   {-91.7f, 0.f, 50.0f},
                   {-88.81f, 0.f, 45.84f},
                   {-79.33f, 0.f, 42.67f},
                   {-71.715f, 0.f, 39.185f},
                   {-69.33f, 0.f, 28.67f},
                   {-69.3f, 0.f, 28.0f},
                   {-69.33f, 0.f, 28.67f},
                   {-71.715f, 0.f, 39.185f},
                   {-81.64f, 0.f, 41.26f}};
    a.schedule = fury::NpcSchedule::DayOnly;
    a.home = {-88.f, 0.f, 42.f};
    result.push_back({std::move(a), {0.72f, 0.58f, 0.40f}});
  }
  // Extra daytime civilians (4.4.0 denser day streets)
  {
    fury::NpcAgent a;
    a.name = "CivD";
    a.display_name = "Pax Wren";
    a.entity_name = "NpcCivD";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.72f;
    a.position = {-6.f, 0.86f, 28.f};
    a.speed = 2.2f;
    a.waypoints = {{-6.f, 0.f, 28.f}, {8.f, 0.f, 28.f}, {8.f, 0.f, 14.f},
                   {-6.f, 0.f, 14.f}};
    a.schedule = fury::NpcSchedule::DayOnly;
    a.home = {-6.f, 0.f, 28.f};
    result.push_back({std::move(a), {0.70f, 0.68f, 0.90f}});
  }
  {
    fury::NpcAgent a;
    a.name = "CivE";
    a.display_name = "Rina Holt";
    a.entity_name = "NpcCivE";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.68f;
    a.position = {48.f, 0.84f, -8.f};
    a.speed = 2.05f;
    // Keep home; use the open pavement around Bldg21 and Bldg11.
    // Stay north of the raised bridge deck/rail: this controller has no step-up.
    a.waypoints = {{48.f, 0.f, -8.f}, {57.f, 0.f, -8.f}, {57.f, 0.f, -3.f},
                   {62.f, 0.f, 1.2f}, {53.5f, 0.f, 0.f}};
    a.schedule = fury::NpcSchedule::DayOnly;
    a.home = {48.f, 0.f, -8.f};
    result.push_back({std::move(a), {0.90f, 0.70f, 0.55f}});
  }
  // Ashcourt fence broker (near shop) — unique Q dialogue role; day open hours only
  {
    fury::NpcAgent f;
    f.name = "Fence";
    f.display_name = "Cass Vesper";
    f.entity_name = "NpcFence";
    f.kind = fury::NpcKind::Fence;
    f.height = 1.78f;
    f.position = {-84.f, 0.89f, 46.f};
    f.speed = 1.2f;
    f.waypoints = {{-84.f, 0.f, 46.f}, {-88.f, 0.f, 50.f}, {-82.f, 0.f, 50.f},
                   {-86.f, 0.f, 45.f}};
    f.schedule = fury::NpcSchedule::DayOnly;  // open hours only
    f.home = {-86.f, 0.f, 48.f};
    result.push_back({std::move(f), {0.90f, 0.55f, 0.28f}});
  }

  return result;
}
std::vector<CrewSpawnSpec> make_crew_roster() {
  std::vector<CrewSpawnSpec> result;
  result.reserve(2);
  {
    fury::CrewMember c;
    c.name = "Crew-Rook";
    c.display_name = "Rook";
    c.entity_name = "CrewRook";
    c.height = 1.7f;
    c.follow_offset = {-1.8f, 0.f, -1.4f};
    c.position = {-2.f, 0.85f, 14.f};
    result.push_back({std::move(c),{0.35f,0.75f,0.55f}});
  }
  {
    fury::CrewMember c;
    c.name = "Crew-Sparrow";
    c.display_name = "Sparrow";
    c.entity_name = "CrewSparrow";
    c.height = 1.72f;
    c.follow_offset = {1.8f, 0.f, -1.2f};
    c.position = {2.f, 0.86f, 14.f};
    result.push_back({std::move(c),{0.75f,0.45f,0.35f}});
  }
  return result;
}
}
