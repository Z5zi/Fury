#include "npc_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>
namespace vaultline {
namespace {
float wrap(float angle) {
  constexpr float pi = 3.14159265359f;
  return std::isfinite(angle) ? std::remainder(angle, 2 * pi) : 0.f;
}
bool finite(const fury::Vec3 &v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
void json_string(std::ostream &out, const std::string &value) {
  out << '"';
  for (unsigned char c : value) {
    if (c == '"' || c == '\\')
      out << '\\' << char(c);
    else if (c >= 32)
      out << char(c);
  }
  out << '"';
}
} // namespace
fury::CharacterRole npc_character_role(const std::string &id,
                                       fury::NpcKind kind) {
  using R = fury::CharacterRole;
  if (id == "PlayerBody")
    return R::Player;
  if (id == "GhostLoop")
    return R::Ghost;
  if (id == "NpcTeller")
    return R::BankStaff;
  if (id == "CrewRook")
    return R::CrewTech;
  if (id == "CrewSparrow")
    return R::CrewScout;
  if (kind == fury::NpcKind::Enforcer)
    return R::Enforcer;
  if (kind == fury::NpcKind::Guard)
    return R::Security;
  if (kind == fury::NpcKind::Fence)
    return R::Fence;
  if (id == "NpcCivD" || id == "NpcCivE")
    return R::Dock;
  if (id == "NpcCivB" || id == "NpcCivAsh")
    return R::Market;
  return R::Commuter;
}
void NpcPresentation::install(fury::Scene &scene, fury::Entity &entity,
                              float height, fury::CharacterRole role,
                              const fury::Vec3 &legacy_color) {
  if (!m_enabled)
    return;
  if (!std::isfinite(height) || height <= 0.f)
    throw std::invalid_argument("Invalid character height");
  auto found = m_actors.find(entity.name);
  if (found == m_actors.end()) {
    Actor actor;
    const auto seed = fury::character_seed(entity.name);
    if (m_individual) {
      auto near_profile = fury::make_character_profile(
          height, role, entity.name, fury::CharacterLod::Near);
      auto far_profile = fury::make_character_profile(height, role, entity.name,
                                                      fury::CharacterLod::Far);
      if (near_profile.animation_profile != far_profile.animation_profile)
        throw std::invalid_argument("Character LOD animation profiles disagree: " + entity.name);
      actor.animation_profile = near_profile.animation_profile;
      actor.individualized = near_profile.individualized;
      actor.lod_distance = near_profile.lod_distance;
      actor.profile_material = near_profile.material;
      actor.near_model = std::move(near_profile.model);
      actor.far_model = std::move(far_profile.model);
    } else {
      actor.near_model = fury::make_character_model(height, role, seed,
                                                    fury::CharacterLod::Near);
      actor.far_model = fury::make_character_model(height, role, seed,
                                                   fury::CharacterLod::Far);
    }
    actor.near_mesh = scene.add_mesh(actor.near_model.bind_mesh);
    actor.far_mesh = scene.add_mesh(actor.far_model.bind_mesh);
    found = m_actors.emplace(entity.name, std::move(actor)).first;
  } else if (std::fabs(found->second.near_model.rig.height - height) > 1e-5f ||
             found->second.near_model.role != role) {
    throw std::invalid_argument(
        "Character respawn changed its registered role/height: " + entity.name);
  }
  auto &actor = found->second;
  actor.animation = {};
  actor.animation.profile = actor.animation_profile;
  actor.animation.sample.seed = actor.near_model.seed;
  actor.animation.sample.idle_time =
      double(actor.near_model.seed % 1024u) / 91.;
  actor.previous_travel = 0;
  actor.pose_elapsed = 0;
  actor.talk_remaining = 0;
  actor.has_travel = false;
  actor.was_visible = entity.visible;
  fury::apply_character_pose(
      actor.near_model,
      fury::sample_character_animation(actor.near_model.rig, actor.animation),
      *actor.near_mesh);
  fury::apply_character_pose(
      actor.far_model,
      fury::sample_character_animation(actor.far_model.rig, actor.animation),
      *actor.far_mesh);
  entity.mesh = actor.near_mesh;
  entity.lod_mesh = actor.far_mesh;
  // Vertex colors already distinguish skin, eyes, hair, fabric and equipment.
  // A second shirt tint would incorrectly color every body part.
  entity.material.albedo = {1, 1, 1};
  entity.material.roughness = .72f;
  entity.material.metallic = 0;
  entity.material.texture = fury::TextureSlot::None;
  entity.material.detail_texture = fury::TextureSlot::None;
  entity.material.textures.reset();
  if (actor.individualized) {
    entity.material = actor.profile_material;
    entity.lod_distance = actor.lod_distance;
  }
  (void)legacy_color;
}
void NpcPresentation::advance(const std::string &id,
                              const NpcMotionSample &motion, float dt,
                              const fury::Vec3 &camera,
                              float lod_mid_distance) {
  if (!m_enabled || !std::isfinite(dt) || dt <= 0 || !finite(motion.position) ||
      !finite(camera) || !std::isfinite(motion.yaw) ||
      !std::isfinite(motion.travel_distance))
    return;
  const auto found = m_actors.find(id);
  if (found == m_actors.end())
    return;
  if (!std::isfinite(lod_mid_distance))
    lod_mid_distance = 0.f;
  auto &actor = found->second;
  if (actor.lod_distance > 0.f)
    lod_mid_distance = actor.lod_distance;
  const float step = std::min(dt, .25f), height = actor.near_model.rig.height;
  fury::CharacterAnimationInput input;
  input.delta_time = step;
  input.travel_speed = motion.actual_speed;
  input.move_weight = motion.move_weight;
  input.yaw = motion.yaw;
  input.turn_rate = motion.turn_rate;
  input.crouch_weight = motion.crouch;
  if (actor.has_travel && std::isfinite(motion.travel_distance))
    input.distance_delta = float(
        std::clamp(motion.travel_distance - actor.previous_travel, 0.0, 10.0));
  actor.previous_travel = motion.travel_distance;
  actor.has_travel = true;
  if (actor.talk_remaining > 0) {
    const fury::Vec3 head = motion.position + fury::Vec3{0, height * .4f, 0};
    const auto delta = actor.listener - head;
    input.attention_yaw = wrap(std::atan2(delta.x, delta.z) - motion.yaw);
    input.attention_pitch =
        std::atan2(delta.y, std::sqrt(delta.x * delta.x + delta.z * delta.z));
    input.attention_weight = std::min(1.f, actor.talk_remaining / .3f);
    input.talk_weight = input.attention_weight;
    actor.talk_remaining = std::max(0.f, actor.talk_remaining - step);
  }
  fury::advance_character_animation(actor.animation, input, height, actor.animation_profile);
  actor.pose_elapsed += step;
  const float distance = fury::length(motion.position - camera);
  const float interval = distance < 18.f   ? 1.f / 30.f
                         : distance < 45.f ? 1.f / 15.f
                                           : 1.f / 8.f;
  if (!motion.visible ||
      (actor.was_visible && actor.pose_elapsed + 1e-5f < interval)) {
    actor.was_visible = motion.visible;
    ++m_deferred_updates;
    return;
  }
  actor.was_visible = true;
  actor.pose_elapsed = 0;
  pose_actor(actor, distance, lod_mid_distance);
}
bool NpcPresentation::refresh_for_review(const std::string &id,
                                         float camera_distance,
                                         float lod_mid_distance) {
  if (!m_enabled || !std::isfinite(camera_distance) || camera_distance < 0.f)
    return false;
  const auto found = m_actors.find(id);
  if (found == m_actors.end())
    return false;
  auto &actor = found->second;
  if (!std::isfinite(lod_mid_distance))
    lod_mid_distance = 0.f;
  if (actor.lod_distance > 0.f)
    lod_mid_distance = actor.lod_distance;
  // No animation, visibility, travel or cadence field is written here.
  pose_actor(actor, camera_distance, lod_mid_distance);
  return true;
}
void NpcPresentation::refresh_for_photo() {
  if (!m_enabled)
    return;
  for (auto &entry : m_actors) {
    auto &actor = entry.second;
    const auto pose =
        fury::sample_character_animation(actor.near_model.rig, actor.animation);
    if (fury::apply_character_pose(actor.near_model, pose, *actor.near_mesh)) {
      ++m_pose_updates;
      m_posed_vertices += actor.near_mesh->vertices.size();
    }
    if (fury::apply_character_pose(actor.far_model, pose, *actor.far_mesh)) {
      ++m_pose_updates;
      m_posed_vertices += actor.far_mesh->vertices.size();
    }
  }
}
void NpcPresentation::pose_actor(Actor &actor, float distance,
                                 float lod_mid_distance) {
  const auto pose =
      fury::sample_character_animation(actor.near_model.rig, actor.animation);
  const float height = actor.near_model.rig.height;
  // Overlapping distance bands cover conservative AABB-based LOD selection.
  if (lod_mid_distance <= 0 || distance < lod_mid_distance + height * 2.f) {
    if (fury::apply_character_pose(actor.near_model, pose, *actor.near_mesh)) {
      ++m_pose_updates;
      m_posed_vertices += actor.near_mesh->vertices.size();
    }
  }
  if (lod_mid_distance > 0 && distance > lod_mid_distance - height * 2.f) {
    if (fury::apply_character_pose(actor.far_model, pose, *actor.far_mesh)) {
      ++m_pose_updates;
      m_posed_vertices += actor.far_mesh->vertices.size();
    }
  }
}

void NpcPresentation::talk(const std::string &id, const fury::Vec3 &listener,
                           float seconds) {
  const auto found = m_actors.find(id);
  if (found == m_actors.end() || !std::isfinite(seconds) || !finite(listener))
    return;
  found->second.listener = listener;
  found->second.talk_remaining = std::clamp(seconds, 0.f, 10.f);
}
bool NpcPresentation::animation_state_for_review(const std::string &id,
                                                    fury::CharacterAnimationState &state) const {
  const auto found=m_actors.find(id);
  if(found==m_actors.end())return false;
  state=found->second.animation;
  return true;
}
NpcPresentationStats NpcPresentation::statistics() const {
  NpcPresentationStats stats;
  stats.actors = m_actors.size();
  stats.pose_updates = m_pose_updates;
  stats.posed_vertices = m_posed_vertices;
  stats.deferred_updates = m_deferred_updates;
  for (const auto &pair : m_actors) {
    stats.near_triangles += pair.second.near_model.bind_mesh.indices.size() / 3;
    stats.far_triangles += pair.second.far_model.bind_mesh.indices.size() / 3;
  }
  return stats;
}
bool NpcPresentation::write_audit(const std::string &path) const {
  std::ofstream out(path);
  if (!out)
    return false;
  const auto stats = statistics();
  out << std::setprecision(9)
      << "{\"enabled\":" << (m_enabled ? "true" : "false")
      << ",\"actors\":" << stats.actors
      << ",\"near_triangles\":" << stats.near_triangles
      << ",\"far_triangles\":" << stats.far_triangles
      << ",\"pose_updates\":" << stats.pose_updates
      << ",\"posed_vertices\":" << stats.posed_vertices
      << ",\"deferred_updates\":" << stats.deferred_updates
      << ",\"profiles\":[";
  bool first = true;
  for (const auto &pair : m_actors) {
    if (!first)
      out << ',';
    first = false;
    const auto &m = pair.second.near_model;
    out << "{\"id\":";
    json_string(out, pair.first);
    out << ",\"role\":";
    json_string(out, fury::character_role_name(m.role));
    out << ",\"animation_profile\":";
    json_string(out, fury::character_animation_profile_name(pair.second.animation_profile));
    out << ",\"individualized\":"
        << (pair.second.individualized ? "true" : "false")
        << ",\"lod_distance\":" << pair.second.lod_distance
        << ",\"height\":" << m.rig.height << ",\"seed\":" << m.seed
        << ",\"near_triangles\":" << m.bind_mesh.indices.size() / 3
        << ",\"far_triangles\":"
        << pair.second.far_model.bind_mesh.indices.size() / 3 << '}';
  }
  out << "]}\n";
  out.flush();
  return out.good();
}
} // namespace vaultline
