#include "npc_interaction.hpp"

#include <algorithm>
#include <cmath>

namespace vaultline {

NpcTalkFocus find_npc_talk_focus(const fury::NpcSystem& npcs,
                               const fury::CrewSystem& crew,
                               const fury::Vec3& camera_position,
                               float camera_yaw) {
  NpcTalkFocus result;
  constexpr float kTalkRadius = 5.5f;
  constexpr float kLookDot = 0.55f;
  const float fx = std::cos(camera_yaw);
  const float fz = std::sin(camera_yaw);
  float best_score = -1.f;
  const auto consider = [&](const std::string& id, const char* label,
                            fury::DialogueRole role, const fury::Vec3& position) {
    const float dx = position.x - camera_position.x;
    const float dz = position.z - camera_position.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (!std::isfinite(distance) || distance > kTalkRadius || distance < 1e-3f) {
      return;
    }
    const float look = (dx * fx + dz * fz) / distance;
    if (!std::isfinite(look) || look < kLookDot) {
      return;
    }
    const float score = look * 2.f + (1.f - distance / kTalkRadius);
    if (score > best_score) {
      best_score = score;
      result.entity_name = id;
      result.label = label;
      result.role = role;
      result.nameplate_fill = 0.35f + 0.55f * std::clamp(look, 0.f, 1.f);
    }
  };
  for (const auto& agent : npcs.agents()) {
    if (!agent.on_duty) continue;
    fury::DialogueRole role = fury::DialogueRole::Civilian;
    if (agent.kind == fury::NpcKind::Guard ||
        agent.kind == fury::NpcKind::Enforcer) {
      role = fury::DialogueRole::Guard;
    } else if (agent.kind == fury::NpcKind::Fence) {
      role = fury::DialogueRole::Fence;
    }
    consider(agent.entity_name, agent.label(), role, agent.position);
  }
  for (const auto& member : crew.members()) {
    if (member.active) {
      consider(member.entity_name, member.label(), fury::DialogueRole::Crew,
               member.position);
    }
  }
  return result;
}

}  // namespace vaultline
