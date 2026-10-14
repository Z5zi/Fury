#pragma once

#include <fury/crew.hpp>
#include <fury/dialogue.hpp>
#include <fury/npc.hpp>

#include <string>

namespace vaultline {

/// Shared production selection for the nameplate and Q dialogue. Positions and
/// camera yaw use Camera::forward's +X-at-zero convention, not actor-model yaw.
struct NpcTalkFocus {
  std::string entity_name;
  std::string label;
  fury::DialogueRole role{fury::DialogueRole::Civilian};
  float nameplate_fill{0.f};
  explicit operator bool() const { return !entity_name.empty(); }
};

NpcTalkFocus find_npc_talk_focus(const fury::NpcSystem& npcs,
                               const fury::CrewSystem& crew,
                               const fury::Vec3& camera_position,
                               float camera_yaw);

}  // namespace vaultline
