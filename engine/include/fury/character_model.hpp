#pragma once

#include "fury/mesh.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace fury {

/// Original harbor wardrobe silhouettes. Identity/appearance is seeded separately
/// from occupation, so role changes do not change a person's skin, hair or build.
enum class CharacterRole : std::uint8_t {
  Commuter, Market, Dock, Security, Fence, CrewScout, CrewTech,
  Enforcer, Player, Ghost, BankStaff, Count
};
enum class CharacterLod : std::uint8_t { Near, Far };

enum class CharacterBone : std::uint8_t {
  Pelvis, Spine, Chest, Neck, Head,
  LeftUpperArm, LeftForearm, LeftHand,
  RightUpperArm, RightForearm, RightHand,
  LeftThigh, LeftShin, LeftFoot,
  RightThigh, RightShin, RightFoot,
  Count
};
constexpr std::size_t character_bone_count = static_cast<std::size_t>(CharacterBone::Count);
constexpr std::size_t bone_index(CharacterBone bone) { return static_cast<std::size_t>(bone); }

struct CharacterJoint {
  /// Parent index (-1 for pelvis). Bind axes are world-aligned, Y up, +Z forward.
  int parent{-1};
  Vec3 bind_position;
};
struct CharacterRig {
  std::array<CharacterJoint, character_bone_count> joints{};
  float height{1.75f};
};
struct CharacterInfluence {
  std::uint8_t first{0};
  std::uint8_t second{0};
  float first_weight{1.f};
  /// Optional third influence; second has the remaining 1-first-third weight.
  /// Appended fields keep existing two-bone aggregate initializers compatible.
  std::uint8_t third{0};
  float third_weight{0.f};
};
struct CharacterAppearance {
  Vec3 skin, hair, eyes, shirt, trousers, shoes, accent;
  float shoulder_scale{1.f};
  float waist_scale{1.f};
  float face_scale{1.f};
  std::uint8_t hair_style{0};
  std::uint8_t face_style{0};
};
/// Opt-in normal reconstruction for a small differentially skinned surface.
/// CSR groups preserve authored UV seams and hard-normal splits. Triangle
/// offsets address bind_mesh.indices; construction owns all allocations.
struct CharacterDeformedNormalRegion {
  struct EdgeAttachment {
    std::uint32_t vertex{}, first{}, second{};
    float fraction{};
  };
  std::uint32_t vertex_begin{}, vertex_count{};
  std::vector<std::uint32_t> member_offsets, members;
  std::vector<std::uint32_t> triangle_offsets, triangles;
  /// UV-cut corners stay on their original posed edge, avoiding tiny artificial
  /// folds when a nonlinear weight field is evaluated at an inserted corner.
  std::vector<EdgeAttachment> edge_attachments;
};
struct CharacterModel {
  Mesh bind_mesh;
  std::vector<CharacterInfluence> influences;
  CharacterRig rig;
  CharacterAppearance appearance;
  CharacterRole role{CharacterRole::Commuter};
  CharacterLod lod{CharacterLod::Near};
  std::uint32_t seed{0};
  /// Opt-in rigid dual-quaternion blending; generic models retain legacy LBS.
  bool dual_quaternion_skinning{false};
  std::vector<CharacterDeformedNormalRegion> deformed_normal_regions;
};
struct CharacterPose {
  /// Object-space skin matrices: animated_global * inverse(bind_global).
  /// Default construction is the bind pose. No scale/shear: rigid bone motion.
  std::array<Mat4, character_bone_count> skin_matrices;
  CharacterPose();
};

/// Portable FNV-1a ID seed (never std::hash, process RNG, role, or array index).
std::uint32_t character_seed(std::string_view stable_id);
CharacterAppearance character_appearance(std::uint32_t seed);
const char* character_role_name(CharacterRole role);
/// Height is preserved, not silently clamped; invalid/nonpositive height throws.
CharacterModel make_character_model(float height, CharacterRole role,
                                   std::uint32_t seed,
                                   CharacterLod lod = CharacterLod::Near);
/// Local Euler radians, composed Rz * Ry * Rx. Root translation is object-space.
/// Parents precede children. Rest joints are translations with world-aligned axes.
CharacterPose make_character_pose(
    const CharacterRig& rig,
    const std::array<Vec3, character_bone_count>& local_euler,
    Vec3 root_translation = {});
/// Initialize output once with model.bind_mesh, then skin positions/normals in
/// place. No allocations/index writes; geometry identity/handles stay stable.
/// Returns true (and marks dirty once) only if at least one vertex changes.
/// Throws for mismatched vertex/index counts, writing into the bind mesh, or
/// nonfinite skin matrices, invalid bone indices/weights, or non-rigid matrices
/// when dual-quaternion skinning is enabled. Validates before any output writes.
/// Caller must preserve the initialized index order.
bool apply_character_pose(const CharacterModel& model, const CharacterPose& pose,
                          Mesh& output);

}  // namespace fury
