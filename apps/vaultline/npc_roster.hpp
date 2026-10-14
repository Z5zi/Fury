#pragma once
#include <fury/npc.hpp>
#include <fury/crew.hpp>
#include <vector>
namespace vaultline {
struct NpcSpawnSpec { fury::NpcAgent agent; fury::Vec3 color; };
/// Exact production regular roster, shared by gameplay and regression tests.
/// Dynamic reinforcement/enforcer spawns remain owned by their controllers.
std::vector<NpcSpawnSpec> make_npc_roster();
struct CrewSpawnSpec { fury::CrewMember member; fury::Vec3 color; };
std::vector<CrewSpawnSpec> make_crew_roster();
}
