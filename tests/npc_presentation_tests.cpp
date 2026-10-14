#include "../apps/vaultline/npc_presentation.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
bool count_allocations = false;
std::size_t allocations = 0;
}
#if defined(_MSC_VER)
# define TEST_NOINLINE __declspec(noinline)
#else
# define TEST_NOINLINE __attribute__((noinline))
#endif
TEST_NOINLINE void* operator new(std::size_t size) {
  if (count_allocations) ++allocations;
  if (void* p = std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc();
}
TEST_NOINLINE void* operator new[](std::size_t size) { return ::operator new(size); }
TEST_NOINLINE void operator delete(void* p) noexcept { std::free(p); }
TEST_NOINLINE void operator delete[](void* p) noexcept { std::free(p); }
TEST_NOINLINE void operator delete(void* p, std::size_t) noexcept { std::free(p); }
TEST_NOINLINE void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace fury;
using vaultline::NpcMotionSample;
using vaultline::NpcPresentation;
int checks = 0;
void require(bool condition, const char* message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
bool same(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool same_mesh(const Mesh& a, const Mesh& b, float tolerance = 0.f) {
  if (a.indices != b.indices || a.vertices.size() != b.vertices.size()) return false;
  for (std::size_t i = 0; i < a.vertices.size(); ++i) {
    const auto& x = a.vertices[i];
    const auto& y = b.vertices[i];
    if (length(x.position - y.position) > tolerance || length(x.normal - y.normal) > tolerance ||
        !same(x.color, y.color) || x.uv.x != y.uv.x || x.uv.y != y.uv.y ||
        x.opacity != y.opacity) return false;
  }
  return true;
}
float max_displacement(const Mesh& a, const Mesh& b, float min_y = -100.f) {
  require(a.vertices.size() == b.vertices.size(), "Compared actors must have matching topology");
  float result = 0.f;
  for (std::size_t i = 0; i < a.vertices.size(); ++i)
    if (a.vertices[i].position.y >= min_y)
      result = std::max(result, length(a.vertices[i].position - b.vertices[i].position));
  return result;
}
void finite_mesh(const Mesh& mesh) {
  for (const auto& v : mesh.vertices) {
    require(std::isfinite(v.position.x) && std::isfinite(v.position.y) &&
                std::isfinite(v.position.z) && std::isfinite(v.normal.x) &&
                std::isfinite(v.normal.y) && std::isfinite(v.normal.z),
            "Wrapper must never submit nonfinite skinned vertices or normals");
  }
}
void same_gameplay(const Entity& a, const Entity& b) {
  require(a.name == b.name && a.tag == b.tag, "Presentation preserves actor name and gameplay tag");
  require(same(a.transform.position, b.transform.position) &&
              same(a.transform.rotation_euler, b.transform.rotation_euler) &&
              same(a.transform.scale, b.transform.scale),
          "Presentation never writes authoritative entity transforms");
  require(a.solid == b.solid && same(a.collider.center, b.collider.center) &&
              same(a.collider.half_extents, b.collider.half_extents),
          "Presentation never changes collision or solidity");
  require(a.visible == b.visible && a.detail == b.detail,
          "Presentation leaves schedule/mission visibility and detail policy intact");
}
void same_material(const Material& a, const Material& b) {
  require(same(a.albedo, b.albedo) && a.metallic == b.metallic &&
              a.roughness == b.roughness && a.emissive == b.emissive &&
              a.texture == b.texture && a.detail_texture == b.detail_texture &&
              a.detail_rotation == b.detail_rotation && a.detail_use_mesh_uvs == b.detail_use_mesh_uvs &&
              a.world_uv_scale == b.world_uv_scale && a.uv_scroll_u == b.uv_scroll_u &&
              a.uv_scroll_v == b.uv_scroll_v && a.wetness == b.wetness &&
              a.transmission == b.transmission && a.index_of_refraction == b.index_of_refraction &&
              a.opacity == b.opacity && a.alpha_cutoff == b.alpha_cutoff &&
              a.normal_scale == b.normal_scale && a.double_sided == b.double_sided &&
              a.alpha_blend == b.alpha_blend && same(a.emissive_color, b.emissive_color) &&
              a.textures == b.textures, "Disabled presentation preserves every legacy material field");
}
Mesh legacy_mesh() {
  Mesh mesh;
  mesh.vertices = {{{-.2f, -.8f, 0.f}}, {{.2f, -.8f, 0.f}}, {{0.f, .8f, 0.f}}};
  mesh.indices = {0, 1, 2};
  return mesh;
}
Entity make_entity(const std::string& name = "NpcPresentationFixture") {
  Entity e;
  e.name = name;
  e.tag = "mission_fixture";
  e.transform.position = {13.f, .875f, -8.f};
  e.transform.rotation_euler = {.01f, .6f, -.01f};
  e.transform.scale = {1.1f, 1.f, .95f};
  e.solid = true;
  e.collider = {{.1f, -.2f, .3f}, {.4f, .875f, .4f}};
  e.detail = true;
  e.material.albedo = {.15f, .45f, .65f};
  e.material.metallic = .1f;
  e.material.roughness = .6f;
  e.material.texture = TextureSlot::Checker;
  e.material.detail_texture = TextureSlot::Concrete;
  e.material.detail_rotation = 2;
  e.material.detail_use_mesh_uvs = true;
  e.material.world_uv_scale = 2.f;
  e.material.uv_scroll_u = .3f;
  e.material.uv_scroll_v = .1f;
  e.material.wetness = .2f;
  e.material.transmission = .1f;
  e.material.index_of_refraction = 1.45f;
  e.material.opacity = .9f;
  e.material.alpha_cutoff = .2f;
  e.material.normal_scale = .8f;
  e.material.double_sided = false;
  e.material.alpha_blend = true;
  e.material.emissive = .05f;
  e.material.emissive_color = {.2f, .4f, .7f};
  return e;
}
struct Fixture {
  Scene scene;
  NpcPresentation presentation;
  Entity* entity{};
  NpcMotionSample sample;
  explicit Fixture(bool enabled = true, const std::string& id = "NpcPresentationFixture")
      : presentation(enabled) {
    scene.entities().reserve(4);
    Entity e = make_entity(id);
    e.mesh = scene.add_mesh(legacy_mesh());
    e.lod_mesh = scene.add_mesh(legacy_mesh());
    entity = &scene.add_entity(std::move(e));
    presentation.install(scene, *entity, 1.75f, CharacterRole::Commuter, {.1f, .2f, .3f});
    sample.position = entity->transform.position;
    sample.yaw = entity->transform.rotation_euler.y;
  }
  void tick(float dt = 1.f / 60.f, float camera_distance = 5.f, float lod = 30.f) {
    presentation.advance(entity->name, sample, dt, sample.position + Vec3{camera_distance, 0, 0}, lod);
  }
};
void test_role_mapping() {
  using R = CharacterRole;
  using K = NpcKind;
  const auto role = vaultline::npc_character_role;
  require(role("PlayerBody", K::Civilian) == R::Player &&
              role("GhostLoop", K::Civilian) == R::Ghost &&
              role("CrewRook", K::Civilian) == R::CrewTech &&
              role("CrewSparrow", K::Civilian) == R::CrewScout, "Player, ghost and crew keep explicit visual roles");
  require(role("NpcGuard", K::Guard) == R::Security && role("NpcFence", K::Fence) == R::Fence &&
              role("NpcEnforcer", K::Enforcer) == R::Enforcer, "Gameplay kinds map to security/fence/enforcer wardrobe");
  require(role("NpcCivD", K::Civilian) == R::Dock && role("NpcCivE", K::Civilian) == R::Dock &&
              role("NpcCivB", K::Civilian) == R::Market && role("NpcCivAsh", K::Civilian) == R::Market &&
              role("NpcTeller", K::Civilian) == R::BankStaff &&
              role("NpcCivA", K::Civilian) == R::Commuter, "Authored civilian IDs map to district wardrobes");
}
void test_disabled_preserves_legacy() {
  Scene scene;
  Entity e = make_entity();
  e.visible = false;
  e.mesh = scene.add_mesh(legacy_mesh());
  e.lod_mesh = scene.add_mesh(legacy_mesh());
  const Entity before = e;
  const Mesh near = *e.mesh, far = *e.lod_mesh;
  NpcPresentation disabled(false);
  disabled.install(scene, e, 1.75f, CharacterRole::Commuter, {.9f, .8f, .7f});
  disabled.talk(e.name, {1, 2, 3});
  for (int i = 0; i < 100; ++i) disabled.advance(e.name, {}, .1f, {}, 30.f);
  same_gameplay(before, e);
  same_material(before.material, e.material);
  require(e.mesh == before.mesh && e.lod_mesh == before.lod_mesh && scene.meshes().size() == 2,
          "Disabled install retains legacy mesh pointers and scene allocations");
  require(same_mesh(near, *e.mesh) && same_mesh(far, *e.lod_mesh), "Disabled update never deforms legacy geometry");
  const auto stats = disabled.statistics();
  require(!disabled.enabled() && stats.actors == 0 && stats.pose_updates == 0 &&
              stats.posed_vertices == 0 && stats.deferred_updates == 0,
          "Disabled presentation incurs no registered actor or pose work");
}
void test_gameplay_and_respawn_contracts() {
  Fixture f;
  same_gameplay(make_entity(), *f.entity);
  Entity sentinel = make_entity("VaultDoor");
  sentinel.tag = "vault";
  sentinel.visible = false;
  f.scene.add_entity(sentinel);
  const Entity original = *f.entity;
  const Mesh initial_near = *f.entity->mesh, initial_far = *f.entity->lod_mesh;
  const auto solids = f.scene.collect_solids();
  const auto near_id = f.entity->mesh->geometry_identity, far_id = f.entity->lod_mesh->geometry_identity;
  const auto* near_data = f.entity->mesh->vertices.data();
  const auto* far_data = f.entity->lod_mesh->vertices.data();
  const auto* near_indices = f.entity->mesh->indices.data();
  const auto* far_indices = f.entity->lod_mesh->indices.data();
  const auto near_capacity = f.entity->mesh->vertices.capacity(), far_capacity = f.entity->lod_mesh->vertices.capacity();
  f.entity->mesh->gpu_vao = 11; f.entity->mesh->gpu_vbo = 12; f.entity->mesh->gpu_ibo = 13;
  f.entity->lod_mesh->gpu_vao = 21; f.entity->lod_mesh->gpu_vbo = 22; f.entity->lod_mesh->gpu_ibo = 23;
  f.entity->mesh->gpu_uploaded = f.entity->lod_mesh->gpu_uploaded = true;
  f.presentation.talk(f.entity->name, f.sample.position + Vec3{3, 1, 2});
  f.sample.actual_speed = 2.f; f.sample.move_weight = 1.f;
  allocations = 0; count_allocations = true;
  for (int frame = 0; frame < 360; ++frame) {
    f.sample.travel_distance += 2.0 / 60.0;
    f.tick(1.f / 60.f, float(frame % 80));
  }
  count_allocations = false;
  require(allocations == 0, "Wrapper hot path performs zero heap allocations");
  require(max_displacement(initial_near, *f.entity->mesh) > .02f, "Moving wrapper genuinely changes posed vertices");
  require(f.scene.meshes().size() == 4 && f.presentation.statistics().actors == 1,
          "One registered actor creates exactly two detail meshes");
  for (int respawn = 0; respawn < 20; ++respawn) {
    Entity replacement = original;
    replacement.mesh = nullptr; replacement.lod_mesh = nullptr;
    replacement.transform.position.x += float(respawn);
    allocations = 0; count_allocations = true;
    f.presentation.install(f.scene, replacement, 1.75f, CharacterRole::Commuter, {1, 0, 0});
    count_allocations = false;
    require(allocations == 0, "Registered actor respawn reuses allocations");
    require(replacement.mesh == original.mesh && replacement.lod_mesh == original.lod_mesh,
            "Respawn reuses stable renderer mesh pointers");
    require(same_mesh(initial_near, *replacement.mesh, 2e-6f) && same_mesh(initial_far, *replacement.lod_mesh, 2e-6f),
            "Respawn restores deterministic identity and seed-specific idle pose");
    require(f.scene.meshes().size() == 4 && f.presentation.statistics().actors == 1,
            "Repeated respawn cannot add geometry or duplicate presentation actors");
  }
  require(f.entity->mesh->geometry_identity == near_id && f.entity->lod_mesh->geometry_identity == far_id &&
              f.entity->mesh->vertices.data() == near_data && f.entity->lod_mesh->vertices.data() == far_data &&
              f.entity->mesh->indices.data() == near_indices && f.entity->lod_mesh->indices.data() == far_indices &&
              f.entity->mesh->vertices.capacity() == near_capacity && f.entity->lod_mesh->vertices.capacity() == far_capacity,
          "Animation and respawn preserve geometry identity, storage and topology");
  require(f.entity->mesh->gpu_vao == 11 && f.entity->mesh->gpu_vbo == 12 && f.entity->mesh->gpu_ibo == 13 &&
              f.entity->lod_mesh->gpu_vao == 21 && f.entity->lod_mesh->gpu_vbo == 22 && f.entity->lod_mesh->gpu_ibo == 23 &&
              f.entity->mesh->gpu_uploaded && f.entity->lod_mesh->gpu_uploaded,
          "Animation and respawn preserve renderer handles and upload ownership");
  same_gameplay(original, *f.entity);
  same_gameplay(sentinel, *f.scene.find_by_name("VaultDoor"));
  const auto after_solids = f.scene.collect_solids();
  require(solids.size() == after_solids.size(), "Presentation leaves collision population untouched");
  for (std::size_t i = 0; i < solids.size(); ++i)
    require(same(solids[i].center, after_solids[i].center) && same(solids[i].half_extents, after_solids[i].half_extents),
            "Presentation leaves collected gameplay AABBs untouched");
  require(same(f.entity->material.albedo, {1, 1, 1}) && f.entity->material.metallic == 0 &&
              std::fabs(f.entity->material.roughness - .72f) < 1e-6f &&
              f.entity->material.texture == TextureSlot::None && f.entity->material.detail_texture == TextureSlot::None,
          "Detailed actor uses neutral material so skin, hair and clothing retain vertex palette");
  for (float height : {1.7f, 0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    bool rejected = false;
    try { f.presentation.install(f.scene, *f.entity, height, CharacterRole::Commuter, {}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Registered actor rejects changed or invalid height");
  }
  bool rejected = false;
  try { f.presentation.install(f.scene, *f.entity, 1.75f, CharacterRole::Security, {}); }
  catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "Registered actor cannot silently change role");
}
void test_motion_talk_and_determinism() {
  Fixture idle, moving, talking, replica;
  const Mesh bind_idle = *idle.entity->mesh;
  moving.sample.actual_speed = 2.f; moving.sample.move_weight = 1.f;
  talking.presentation.talk(talking.entity->name, talking.sample.position + Vec3{3, 1, 3}, 1.f);
  for (int i = 0; i < 45; ++i) {
    moving.sample.travel_distance += 2.0 / 60.0;
    idle.tick(); moving.tick(); talking.tick(); replica.tick();
  }
  require(max_displacement(bind_idle, *idle.entity->mesh) > .001f, "Idle has nonzero breathing and weight shift");
  require(max_displacement(*idle.entity->mesh, *moving.entity->mesh) > .05f,
          "Measured travel selects a visibly distinct locomotion pose");
  require(max_displacement(*idle.entity->mesh, *talking.entity->mesh, .2f) > .015f,
          "Conversation changes upper-body attention and gestures");
  require(same_mesh(*idle.entity->mesh, *replica.entity->mesh), "Same stable ID and input history reproduce exact mesh output");
  for (int i = 0; i < 600; ++i) { idle.tick(); talking.tick(); }
  require(max_displacement(*idle.entity->mesh, *talking.entity->mesh) < .0001f,
          "Expired conversation smoothly returns to the deterministic idle pose");
  Fixture other(true, "DifferentPersistentActor");
  require(other.entity->mesh->geometry_identity != idle.entity->mesh->geometry_identity &&
              other.entity->mesh != idle.entity->mesh, "Different stable actors never share mutable geometry");
  Fixture frozen, speed_only;
  speed_only.sample.actual_speed = 5.f; speed_only.sample.move_weight = 0.f;
  for (int i = 0; i < 60; ++i) { frozen.tick(); speed_only.tick(); }
  require(same_mesh(*frozen.entity->mesh, *speed_only.entity->mesh),
          "Requested speed without travel or move weight cannot create a walk cycle");
}
void test_lod_cadence_and_visibility() {
  Fixture near, mid, far;
  const auto n0 = near.entity->mesh->geometry_revision, nf0 = near.entity->lod_mesh->geometry_revision;
  const auto m0 = mid.entity->mesh->geometry_revision, mf0 = mid.entity->lod_mesh->geometry_revision;
  const auto f0 = far.entity->mesh->geometry_revision, ff0 = far.entity->lod_mesh->geometry_revision;
  for (int i = 0; i < 120; ++i) { near.tick(); mid.tick(1.f / 60.f, 30.f); far.tick(1.f / 60.f, 60.f); }
  require(near.entity->mesh->geometry_revision - n0 == 60 && near.entity->lod_mesh->geometry_revision == nf0,
          "Near cadence skins only high detail at 30 Hz");
  require(mid.entity->mesh->geometry_revision - m0 == 30 && mid.entity->lod_mesh->geometry_revision - mf0 == 30,
          "LOD overlap skins both persistent meshes at 15 Hz");
  require(far.entity->mesh->geometry_revision == f0 && far.entity->lod_mesh->geometry_revision - ff0 == 15,
          "Far cadence skins only proxy at bounded approximately 8 Hz");
  const auto stats = near.presentation.statistics();
  require(stats.pose_updates == 60 && stats.deferred_updates == 60 &&
              stats.posed_vertices == 60 * near.entity->mesh->vertices.size() &&
              stats.near_triangles == near.entity->mesh->indices.size() / 3 &&
              stats.far_triangles == near.entity->lod_mesh->indices.size() / 3,
          "Audit statistics report actual pose, vertex, deferred and geometry work");
  Fixture hidden, visible;
  hidden.sample.visible = false;
  const auto hidden_rev = hidden.entity->mesh->geometry_revision;
  for (int i = 0; i < 60; ++i) { hidden.tick(); visible.tick(); }
  require(hidden.entity->mesh->geometry_revision == hidden_rev && hidden.presentation.statistics().pose_updates == 0,
          "Hidden actor advances animation without CPU skinning");
  hidden.sample.visible = true;
  hidden.tick(1.f / 120.f); visible.tick(1.f / 120.f);
  require(hidden.entity->mesh->geometry_revision == hidden_rev + 1,
          "Reappearing actor immediately refreshes its current pose below normal interval");
  visible.tick(.25f); hidden.tick(.25f);
  require(same_mesh(*hidden.entity->mesh, *visible.entity->mesh, 2e-6f),
          "Hidden actor's animation clock remains continuous when it reappears");
  Fixture no_lod;
  const auto proxy_revision = no_lod.entity->lod_mesh->geometry_revision;
  for (int i = 0; i < 30; ++i) no_lod.tick(.1f, 90.f, 0.f);
  require(no_lod.entity->mesh->geometry_revision > 1 && no_lod.entity->lod_mesh->geometry_revision == proxy_revision,
          "Disabled renderer LOD always updates near mesh even far from camera");
}
void test_review_refresh() {
  Fixture once, repeated;
  for (Fixture *f : {&once, &repeated}) {
    f->sample.actual_speed = 4.f; f->sample.move_weight = 1.f;
    for (int i = 0; i < 60; ++i) {
      f->sample.travel_distance += 4.0 / 60.0;
      f->tick();
    }
    f->sample.actual_speed = 0; f->sample.move_weight = 0;
    for (int i = 0; i < 180; ++i) f->tick(1.f / 60.f, 60.f, 30.f);
  }
  const Mesh stale = *once.entity->mesh, far_before = *once.entity->lod_mesh;
  const auto *near_pointer = once.entity->mesh; const auto *far_pointer = once.entity->lod_mesh;
  const Entity original = *once.entity;
  require(once.presentation.refresh_for_review(once.entity->name, 3.2f, 30.f),
          "Review refresh finds the registered live actor");
  require(max_displacement(stale, *once.entity->mesh) > .01f,
          "Newly visible Near LOD refreshes a stale walking mesh to current idle animation");
  require(same_mesh(far_before, *once.entity->lod_mesh),
          "Close review refresh leaves the already current Far mesh untouched");
  for (int i = 0; i < 20; ++i)
    require(repeated.presentation.refresh_for_review(repeated.entity->name, 3.2f, 30.f),
            "Repeated diagnostic refresh keeps the same live actor");
  require(same_mesh(*once.entity->mesh, *repeated.entity->mesh),
          "Twenty review refreshes equal one: no animation-time advancement");
  require(once.entity->mesh == near_pointer && once.entity->lod_mesh == far_pointer,
          "Review refresh retains original mesh allocation and LOD ownership");
  same_gameplay(original, *once.entity);
  same_material(original.material, once.entity->material);
  const auto before = once.presentation.statistics();
  for (float bad : {-1.f, std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::infinity()})
    require(!once.presentation.refresh_for_review(once.entity->name, bad, 30.f),
            "Malformed review distance is rejected without update");
  require(!once.presentation.refresh_for_review("unknown_actor", 3.f, 30.f),
          "Review refresh does not manufacture an unregistered actor");
  require(once.presentation.statistics().pose_updates == before.pose_updates &&
              once.presentation.statistics().deferred_updates == before.deferred_updates,
          "Rejected review inputs preserve pose/cadence counters");
  for(int i=0;i<5;++i){once.tick();repeated.tick();}
  require(same_mesh(*once.entity->mesh,*repeated.entity->mesh) &&
              once.presentation.statistics().deferred_updates == repeated.presentation.statistics().deferred_updates,
          "Review refresh does not perturb subsequent live animation or ordinary update cadence");
}
void test_invalid_lod_policy() {
  Fixture checked, disabled;
  for (float threshold : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity()}) {
    checked.tick(.25f, 60.f, threshold);
    disabled.tick(.25f, 60.f, 0.f);
  }
  require(same_mesh(*checked.entity->mesh, *disabled.entity->mesh) &&
              checked.presentation.statistics().pose_updates == 3,
          "Nonfinite LOD threshold safely falls back to continuously posed near geometry");
}
void test_invalid_samples_and_recovery() {
  const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
  Fixture reference, checked;
  const Mesh original = *checked.entity->mesh;
  for (float dt : {0.f, -1.f, nan, inf}) checked.tick(dt);
  for (int input = 0; input < 7; ++input) {
    NpcMotionSample bad = checked.sample;
    Vec3 camera = bad.position;
    if (input == 0) bad.position.x = nan;
    if (input == 1) bad.position.z = inf;
    if (input == 2) bad.yaw = inf;
    if (input == 3) bad.yaw = nan;
    if (input == 4) bad.travel_distance = nan;
    if (input == 5) bad.travel_distance = std::numeric_limits<double>::infinity();
    if (input == 6) camera.y = nan;
    checked.presentation.advance(checked.entity->name, bad, .25f, camera, 30.f);
  }
  checked.presentation.advance("unknown_actor", {}, .25f, {}, 30.f);
  checked.presentation.talk("unknown_actor", {}, 1.f);
  checked.presentation.talk(checked.entity->name, {nan, 0, 0}, 1.f);
  checked.presentation.talk(checked.entity->name, {}, inf);
  require(same_mesh(original, *checked.entity->mesh) && checked.presentation.statistics().pose_updates == 0 &&
              checked.presentation.statistics().deferred_updates == 0,
          "Nonpositive/nonfinite dt, malformed spatial samples and unknown IDs have no side effects");
  for (int i = 0; i < 60; ++i) { reference.tick(); checked.tick(); }
  require(same_mesh(*reference.entity->mesh, *checked.entity->mesh), "Ignored malformed input does not corrupt later valid animation");
  checked.presentation.talk(checked.entity->name, checked.sample.position + Vec3{3, 1, 2}, 1.f);
  checked.sample.actual_speed = inf; checked.sample.move_weight = nan;
  checked.sample.turn_rate = nan; checked.sample.crouch = inf;
  checked.sample.yaw = std::numeric_limits<float>::max();
  checked.sample.travel_distance = 1e200;
  for (int i = 0; i < 10; ++i) checked.tick(.25f);
  finite_mesh(*checked.entity->mesh); finite_mesh(*checked.entity->lod_mesh);
  checked.sample = reference.sample;
  checked.sample.actual_speed = 2.f; checked.sample.move_weight = 1.f;
  for (int i = 0; i < 90; ++i) { checked.sample.travel_distance += 1.0 / 30.0; checked.tick(); }
  finite_mesh(*checked.entity->mesh);
  Fixture hitch, capped;
  hitch.tick(5.f); capped.tick(.25f);
  require(same_mesh(*hitch.entity->mesh, *capped.entity->mesh), "Long update hitch is capped to the same .25-second animation step");
}
void test_audit_output() {
  Fixture f(true, "Fixture\"With\\Escape");
  for (int i = 0; i < 10; ++i) f.tick();
  const auto path = std::filesystem::temp_directory_path() /
      ("fury-npc-presentation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
  require(f.presentation.write_audit(path.string()), "Audit writes successfully to a writable path");
  std::ifstream in(path);
  std::stringstream contents; contents << in.rdbuf();
  const auto json = contents.str();
  require(json.find("\"enabled\":true") != std::string::npos && json.find("\"actors\":1") != std::string::npos &&
              json.find("\"seed\":" + std::to_string(character_seed(f.entity->name))) != std::string::npos &&
              json.find("Fixture\\\"With\\\\Escape") != std::string::npos,
          "Audit includes actor count, stable identity seed and correctly escaped ID");
  in.close(); std::filesystem::remove(path);
  require(!f.presentation.write_audit((path / "missing" / "audit.json").string()),
          "Audit reports unwritable destination instead of claiming success");
}
void test_read_only_animation_witness() {
  Fixture f;
  f.sample.actual_speed=3.15f;f.sample.move_weight=1.f;
  for(int i=0;i<24;++i){f.sample.travel_distance+=3.15/60.;f.tick();}
  const auto stats=f.presentation.statistics();const auto mesh=*f.entity->mesh;
  CharacterAnimationState first{},second{};
  require(f.presentation.animation_state_for_review(f.entity->name,first) && first.initialized && first.gait_started,
          "Diagnostic witness returns real initialized production animation");
  require(f.presentation.animation_state_for_review(f.entity->name,second) && first.sample.phase_cycles==second.sample.phase_cycles && first.sample.speed==second.sample.speed && first.support_fraction==second.support_fraction,
          "Repeated state reads do not advance gait or time");
  first.sample.seed=123;
  require(f.presentation.animation_state_for_review(f.entity->name,second) && second.sample.seed==character_seed(f.entity->name),
          "Diagnostic state is a detached copy of the actor state");
  require(same_mesh(mesh,*f.entity->mesh) && stats.pose_updates==f.presentation.statistics().pose_updates && stats.deferred_updates==f.presentation.statistics().deferred_updates,
          "Read-only state witness preserves meshes and ordinary pose cadence");
  first.sample.seed=123;
  require(!f.presentation.animation_state_for_review("missing_player_witness",first) && first.sample.seed==123,
          "Unknown diagnostic identity is rejected without mutating caller state");
}
// Compare named fields rather than padding bytes, including recovery data that
// may be inactive at the instant a photo is entered.
bool same_animation(const CharacterAnimationState& a, const CharacterAnimationState& b) {
  const auto& x = a.sample;
  const auto& y = b.sample;
  return x.phase_cycles == y.phase_cycles && x.idle_time == y.idle_time &&
      x.speed == y.speed && x.stride_length == y.stride_length &&
      x.move_weight == y.move_weight && x.turn_rate == y.turn_rate &&
      x.attention_yaw == y.attention_yaw && x.attention_pitch == y.attention_pitch &&
      x.attention_weight == y.attention_weight && x.talk_weight == y.talk_weight &&
      x.crouch_weight == y.crouch_weight && x.seed == y.seed &&
      a.previous_yaw == b.previous_yaw && a.initialized == b.initialized &&
      a.gait_started == b.gait_started && a.support_fraction == b.support_fraction &&
      a.settling_feet == b.settling_feet && a.settled_feet == b.settled_feet &&
      a.settling_index == b.settling_index && a.settling_first == b.settling_first &&
      a.settling_time == b.settling_time && a.settling_z == b.settling_z &&
      a.settling_start_z == b.settling_start_z &&
      a.settling_start_lift == b.settling_start_lift &&
      a.settling_start_pitch == b.settling_start_pitch &&
      a.settling_lift == b.settling_lift && a.settling_pitch == b.settling_pitch &&
      a.recovery_time == b.recovery_time && a.recovering_step == b.recovering_step &&
      a.recovery_foot == b.recovery_foot && a.recovery_z == b.recovery_z &&
      a.recovery_lift == b.recovery_lift && a.recovery_pitch == b.recovery_pitch &&
      a.recovery_root_z == b.recovery_root_z && a.profile == b.profile;
}
CharacterAnimationState current_animation(const NpcPresentation& presentation, const std::string& id) {
  CharacterAnimationState state;
  require(presentation.animation_state_for_review(id, state), "Photo fixture has a registered animation witness");
  return state;
}
float maximum_component(Vec3 value) {
  return std::max({std::fabs(value.x), std::fabs(value.y), std::fabs(value.z)});
}
bool same_cached_mesh(const Mesh& a, const Mesh& b) {
  if (a.indices != b.indices || a.vertices.size() != b.vertices.size()) return false;
  for (std::size_t i = 0; i < a.vertices.size(); ++i) {
    const auto& x = a.vertices[i];
    const auto& y = b.vertices[i];
    // The existing skin kernel retains values within 1e-6 per component.
    // Different skin histories can therefore differ below that same bound.
    if (maximum_component(x.position - y.position) > 1e-6f ||
        maximum_component(x.normal - y.normal) > 1e-6f ||
        !same(x.color, y.color) || x.uv.x != y.uv.x || x.uv.y != y.uv.y || x.opacity != y.opacity)
      return false;
  }
  return true;
}
struct PhotoActor {
  std::string id;
  CharacterRole role;
  float height;
  NpcMotionSample motion;
  CharacterModel near_model, far_model;
};
struct PhotoFixture {
  Scene scene;
  NpcPresentation presentation;
  std::vector<PhotoActor> actors;
  PhotoFixture() {
    scene.entities().reserve(8);
    actors.reserve(8);
  }
  void add(const std::string& id, CharacterRole role, bool visible = true, float height = 1.75f) {
    Entity e = make_entity(id);
    e.visible = visible;
    e.mesh = scene.add_mesh(legacy_mesh());
    e.lod_mesh = scene.add_mesh(legacy_mesh());
    auto& installed = scene.add_entity(std::move(e));
    presentation.install(scene, installed, height, role, {.1f, .2f, .3f});
    auto near_profile = make_character_profile(height, role, id, CharacterLod::Near);
    auto far_profile = make_character_profile(height, role, id, CharacterLod::Far);
    if (id == "CrewSparrow")
      require(near_profile.individualized && far_profile.individualized,
              "Photo regression includes Sparrow's actual individualized Near and Far assets");
    PhotoActor actor;
    actor.id = id;
    actor.role = role;
    actor.height = height;
    actor.motion.position = installed.transform.position;
    actor.motion.yaw = installed.transform.rotation_euler.y;
    actor.motion.visible = visible;
    actor.near_model = std::move(near_profile.model);
    actor.far_model = std::move(far_profile.model);
    actors.push_back(std::move(actor));
  }
  Entity& entity(std::size_t i) { return *scene.find_by_name(actors[i].id); }
  void tick(std::size_t i, float dt, float distance) {
    auto& a = actors[i];
    presentation.advance(a.id, a.motion, dt, a.motion.position + Vec3{distance, 0, 0}, 30.f);
  }
  void populate() {
    add("PhotoGenericNear", CharacterRole::Commuter);
    add("PhotoGenericFar", CharacterRole::Dock);
    add("CrewSparrow", CharacterRole::CrewScout);
    add("PhotoHidden", CharacterRole::Security, false);
  }
  void prepare() {
    for (auto& a : actors)
      presentation.talk(a.id, a.motion.position + Vec3{3.f, 1.3f, -2.f}, 1.6f);
    for (int frame = 0; frame < 43; ++frame) {
      for (std::size_t i = 0; i < actors.size(); ++i) {
        auto& m = actors[i].motion;
        m.actual_speed = 2.2f; m.move_weight = 1.f; m.crouch = .8f;
        m.yaw += .01f; m.turn_rate = .6f;
        m.travel_distance += 2.2 / 60.;
        tick(i, 1.f / 60.f, i == 0 ? 5.f : 60.f);
      }
    }
  }
};
struct PhotoReference {
  CharacterAnimationState state;
  Mesh near, far;
  PhotoReference(const PhotoFixture& fixture, std::size_t i)
      : state(current_animation(fixture.presentation, fixture.actors[i].id)),
        near(fixture.actors[i].near_model.bind_mesh), far(fixture.actors[i].far_model.bind_mesh) {
    const auto& actor = fixture.actors[i];
    // Construct both expected meshes independently of presentation's refresh.
    // The same current-state pose is applied to both independent asset models.
    const auto pose = sample_character_animation(actor.near_model.rig, state);
    apply_character_pose(actor.near_model, pose, near);
    apply_character_pose(actor.far_model, pose, far);
  }
  void check(PhotoFixture& fixture, std::size_t i) const {
    const auto& e = fixture.entity(i);
    require(same_cached_mesh(near, *e.mesh) && same_cached_mesh(far, *e.lod_mesh),
            "Photo Near and Far both match an independent pose sampled from the complete current state");
    require(same_animation(state, current_animation(fixture.presentation, fixture.actors[i].id)),
            "Photo refresh preserves every animation sample, gait, settling, recovery and profile field");
  }
};
struct MeshOwnership {
  Mesh* mesh;
  const Vertex* vertices;
  const std::uint32_t* index_data;
  std::size_t vertex_count, vertex_capacity, index_capacity;
  std::vector<std::uint32_t> indices;
  std::uint64_t identity;
  unsigned vao, vbo, ibo;
  bool uploaded;
  explicit MeshOwnership(Mesh* m)
      : mesh(m), vertices(m->vertices.data()), index_data(m->indices.data()),
        vertex_count(m->vertices.size()), vertex_capacity(m->vertices.capacity()),
        index_capacity(m->indices.capacity()), indices(m->indices), identity(m->geometry_identity),
        vao(m->gpu_vao), vbo(m->gpu_vbo), ibo(m->gpu_ibo), uploaded(m->gpu_uploaded) {}
  void check(Mesh* current) const {
    require(current == mesh && current->geometry_identity == identity &&
                current->vertices.data() == vertices && current->indices.data() == index_data &&
                current->vertices.size() == vertex_count && current->vertices.capacity() == vertex_capacity &&
                current->indices.capacity() == index_capacity && current->indices == indices,
            "Photo refresh retains mesh pointers, geometry identity, storage capacity and exact topology");
    require(current->gpu_vao == vao && current->gpu_vbo == vbo && current->gpu_ibo == ibo &&
                current->gpu_uploaded == uploaded,
            "Photo refresh retains GPU handles and upload ownership");
  }
};
void test_photo_refresh_all_actors_and_current_pose() {
  PhotoFixture f;
  f.populate();
  f.prepare();
  Entity sentinel = make_entity("UnregisteredPhotoSentinel");
  sentinel.mesh = f.scene.add_mesh(legacy_mesh());
  sentinel.lod_mesh = f.scene.add_mesh(legacy_mesh());
  f.scene.add_entity(sentinel);
  const Mesh sentinel_near = *sentinel.mesh, sentinel_far = *sentinel.lod_mesh;
  std::vector<PhotoReference> expected;
  std::vector<Entity> entities;
  std::vector<MeshOwnership> ownership;
  for (std::size_t i = 0; i < f.actors.size(); ++i) {
    auto& e = f.entity(i);
    expected.emplace_back(f, i);
    require(max_displacement(*e.mesh, expected.back().near) > 1e-5f &&
                max_displacement(*e.lod_mesh, expected.back().far) > 1e-5f,
            "Every photo fixture starts with both LODs stale against its current animation");
    entities.push_back(e);
    for (Mesh* mesh : {e.mesh, e.lod_mesh}) {
      mesh->gpu_vao = 100 + unsigned(ownership.size()) * 3;
      mesh->gpu_vbo = mesh->gpu_vao + 1; mesh->gpu_ibo = mesh->gpu_vao + 2;
      mesh->gpu_uploaded = true; mesh->gpu_dirty = false;
      ownership.emplace_back(mesh);
    }
  }
  const auto stats = f.presentation.statistics();
  const auto mesh_count = f.scene.meshes().size();
  allocations = 0;
  const auto wall_start = std::chrono::steady_clock::now();
  const auto cpu_start = std::clock();
  count_allocations = true;
  f.presentation.refresh_for_photo();
  count_allocations = false;
  const auto cpu_end = std::clock();
  const auto wall_end = std::chrono::steady_clock::now();
  const auto entry_allocations = allocations;
  const auto refreshed = f.presentation.statistics();
  std::size_t expected_vertices = 0;
  for (std::size_t i = 0; i < f.actors.size(); ++i) {
    expected[i].check(f, i);
    auto& e = f.entity(i);
    ownership[2 * i].check(e.mesh); ownership[2 * i + 1].check(e.lod_mesh);
    require(e.mesh->gpu_dirty && e.lod_mesh->gpu_dirty, "Both changed photo LODs request GPU vertex refresh");
    same_gameplay(entities[i], e); same_material(entities[i].material, e.material);
    require(e.lod_distance == entities[i].lod_distance && e.material.color_detail == entities[i].material.color_detail,
            "Photo refresh preserves actor-specific LOD policy and color-detail ownership");
    expected_vertices += e.mesh->vertices.size() + e.lod_mesh->vertices.size();
  }
  require(refreshed.actors == stats.actors && refreshed.deferred_updates == stats.deferred_updates &&
              refreshed.near_triangles == stats.near_triangles && refreshed.far_triangles == stats.far_triangles &&
              refreshed.pose_updates == stats.pose_updates + 2 * f.actors.size() &&
              refreshed.posed_vertices == stats.posed_vertices + expected_vertices &&
              f.scene.meshes().size() == mesh_count,
          "Photo refresh covers all registered meshes with honest work counters and no new scene geometry");
  same_gameplay(sentinel, *f.scene.find_by_name(sentinel.name));
  require(same_mesh(sentinel_near, *sentinel.mesh) && same_mesh(sentinel_far, *sentinel.lod_mesh),
          "Photo refresh leaves unregistered scene geometry alone");
  std::vector<std::uint64_t> revisions;
  for (const auto& o : ownership) {
    revisions.push_back(o.mesh->geometry_revision);
    o.mesh->gpu_dirty = false;
  }
  for (int repeat = 0; repeat < 20; ++repeat) f.presentation.refresh_for_photo();
  for (std::size_t i = 0; i < ownership.size(); ++i) {
    ownership[i].check(ownership[i].mesh);
    require(ownership[i].mesh->geometry_revision == revisions[i] && !ownership[i].mesh->gpu_dirty,
            "Identical photo refreshes do not revise geometry or request redundant GPU uploads");
  }
  for (std::size_t i = 0; i < f.actors.size(); ++i) expected[i].check(f, i);
  const auto repeated = f.presentation.statistics();
  require(repeated.pose_updates == refreshed.pose_updates && repeated.posed_vertices == refreshed.posed_vertices &&
              repeated.deferred_updates == refreshed.deferred_updates,
          "Identical photo refreshes leave actual skinning and deferred-work counters unchanged");
  // Report measured entry cost; setup/reference meshes/assertions are outside
  // the interval. Allocation counts are evidence, not a new zero-allocation gate.
  std::cout << "photo refresh, first call with " << f.actors.size() << " installed actors: wall_us="
            << std::chrono::duration<double, std::micro>(wall_end - wall_start).count();
  if (cpu_start != std::clock_t(-1) && cpu_end != std::clock_t(-1))
    std::cout << " process_cpu_us=" << 1e6 * double(cpu_end - cpu_start) / CLOCKS_PER_SEC;
  else
    std::cout << " process_cpu_us=unavailable";
  std::cout << " allocations=" << entry_allocations << '\n';
}
void test_photo_refresh_preserves_future_behavior() {
  PhotoFixture refreshed, control;
  refreshed.populate(); control.populate();
  refreshed.prepare(); control.prepare();
  refreshed.presentation.refresh_for_photo();
  bool saw_settling = false, saw_recovery = false;
  float maximum_position_difference = 0.f, maximum_normal_difference = 0.f;
  float maximum_position_component = 0.f, maximum_normal_component = 0.f;
  auto compare_future_mesh = [&](const Mesh& a, const Mesh& b) {
    require(same_cached_mesh(a, b),
            "Future ordinary photo/control meshes agree under the unchanged per-component skin-cache rule");
    for (std::size_t j = 0; j < a.vertices.size(); ++j) {
      const auto position = a.vertices[j].position - b.vertices[j].position;
      const auto normal = a.vertices[j].normal - b.vertices[j].normal;
      maximum_position_difference = std::max(maximum_position_difference, length(position));
      maximum_normal_difference = std::max(maximum_normal_difference, length(normal));
      maximum_position_component = std::max(maximum_position_component, maximum_component(position));
      maximum_normal_component = std::max(maximum_normal_component, maximum_component(normal));
    }
  };
  for (int frame = 0; frame < 180; ++frame) {
    for (std::size_t i = 0; i < refreshed.actors.size(); ++i) {
      auto& m = refreshed.actors[i].motion;
      const bool moving = (frame >= 22 && frame < 32) || frame >= 91;
      m.actual_speed = moving ? 2.4f : 0.f; m.move_weight = moving ? 1.f : 0.f;
      m.crouch = frame < 120 ? .8f : 0.f;
      m.yaw += .013f; m.turn_rate = .78f;
      m.visible = !(frame >= 65 && frame < 75);
      m.travel_distance += moving ? 2.4 / 60. : 0.;
      // A discontinuous travel reset must retain the same previous-travel
      // behavior after photo entry; a later tick resumes from this new value.
      if (frame == 110) m.travel_distance = 0.;
      control.actors[i].motion = m;
      const float dt = frame == 0 || frame == 75 ? 1.f / 240.f : 1.f / 60.f;
      const float distance = frame < 45 ? (i == 0 ? 5.f : 60.f) :
          frame < 100 ? 30.f : frame < 140 ? 5.f : 60.f;
      const auto before = refreshed.presentation.statistics();
      const auto control_before = control.presentation.statistics();
      const auto near_revision = control.entity(i).mesh->geometry_revision;
      const auto far_revision = control.entity(i).lod_mesh->geometry_revision;
      refreshed.tick(i, dt, distance); control.tick(i, dt, distance);
      const auto after = refreshed.presentation.statistics();
      const auto control_after = control.presentation.statistics();
      require(after.deferred_updates - before.deferred_updates ==
                  control_after.deferred_updates - control_before.deferred_updates &&
                  after.pose_updates - before.pose_updates == control_after.pose_updates - control_before.pose_updates &&
                  after.posed_vertices - before.posed_vertices == control_after.posed_vertices - control_before.posed_vertices,
              "Photo entry leaves each future ordinary visibility, cadence and LOD-work decision unchanged");
      const auto state = current_animation(refreshed.presentation, refreshed.actors[i].id);
      require(same_animation(state, current_animation(control.presentation, control.actors[i].id)),
              "Photo entry leaves future travel, turning, conversation expiry, crouch and recovery identical to control");
      saw_settling = saw_settling || state.settling_feet;
      saw_recovery = saw_recovery || state.recovering_step;
      if (control.entity(i).mesh->geometry_revision != near_revision)
        compare_future_mesh(*refreshed.entity(i).mesh, *control.entity(i).mesh);
      if (control.entity(i).lod_mesh->geometry_revision != far_revision)
        compare_future_mesh(*refreshed.entity(i).lod_mesh, *control.entity(i).lod_mesh);
    }
    if (frame % 11 == 0) {
      std::vector<CharacterAnimationState> states;
      for (const auto& a : refreshed.actors) states.push_back(current_animation(refreshed.presentation, a.id));
      refreshed.presentation.refresh_for_photo();
      for (std::size_t i = 0; i < refreshed.actors.size(); ++i)
        require(same_animation(states[i], current_animation(refreshed.presentation, refreshed.actors[i].id)),
                "Photo refresh also preserves complete state during active settling and recovery");
    }
  }
  require(saw_settling && saw_recovery, "Future-behavior regression actually exercises foot settling and interrupted recovery");
  std::cout << "photo/control future mesh maxima: position_distance=" << maximum_position_difference
            << " normal_distance=" << maximum_normal_difference
            << " position_component=" << maximum_position_component
            << " normal_component=" << maximum_normal_component << '\n';
  for (std::size_t i = 0; i < refreshed.actors.size(); ++i)
    require(current_animation(refreshed.presentation, refreshed.actors[i].id).sample.talk_weight < .0001f,
            "The unchanged conversation timer reaches its ordinary expired state");
  Fixture briefly_hidden, hidden_control;
  for (Fixture* f : {&briefly_hidden, &hidden_control}) {
    f->tick(1.f / 30.f); // Complete an ordinary pose and reset its cadence timer.
    f->sample.visible = false;
    f->tick(1.f / 240.f);
  }
  briefly_hidden.presentation.refresh_for_photo();
  const auto hidden_before = briefly_hidden.presentation.statistics();
  const auto control_hidden_before = hidden_control.presentation.statistics();
  briefly_hidden.sample.visible = hidden_control.sample.visible = true;
  briefly_hidden.tick(1.f / 240.f); hidden_control.tick(1.f / 240.f);
  require(briefly_hidden.presentation.statistics().pose_updates == hidden_before.pose_updates + 1 &&
              hidden_control.presentation.statistics().pose_updates == control_hidden_before.pose_updates + 1 &&
              briefly_hidden.presentation.statistics().deferred_updates == hidden_before.deferred_updates &&
              hidden_control.presentation.statistics().deferred_updates == control_hidden_before.deferred_updates &&
              same_animation(current_animation(briefly_hidden.presentation, briefly_hidden.entity->name),
                             current_animation(hidden_control.presentation, hidden_control.entity->name)) &&
              same_cached_mesh(*briefly_hidden.entity->mesh, *hidden_control.entity->mesh),
          "Photo refresh preserves immediate reappearance after a brief hide below the ordinary pose interval");
}
void test_photo_refresh_disabled_empty_and_late_install() {
  Fixture disabled(false);
  const Entity original = *disabled.entity;
  const Mesh near = *original.mesh, far = *original.lod_mesh;
  const MeshOwnership near_owner(original.mesh), far_owner(original.lod_mesh);
  const auto near_revision = original.mesh->geometry_revision, far_revision = original.lod_mesh->geometry_revision;
  const bool near_dirty = original.mesh->gpu_dirty, far_dirty = original.lod_mesh->gpu_dirty;
  disabled.presentation.refresh_for_photo();
  same_gameplay(original, *disabled.entity); same_material(original.material, disabled.entity->material);
  near_owner.check(disabled.entity->mesh); far_owner.check(disabled.entity->lod_mesh);
  require(same_mesh(near, *disabled.entity->mesh) && same_mesh(far, *disabled.entity->lod_mesh) &&
              original.mesh->geometry_revision == near_revision && original.lod_mesh->geometry_revision == far_revision &&
              original.mesh->gpu_dirty == near_dirty && original.lod_mesh->gpu_dirty == far_dirty &&
              disabled.presentation.statistics().actors == 0 && disabled.presentation.statistics().pose_updates == 0 &&
              disabled.presentation.statistics().posed_vertices == 0 && disabled.presentation.statistics().deferred_updates == 0,
          "Disabled photo refresh is a complete legacy-geometry and statistics no-op");
  PhotoFixture f;
  f.presentation.refresh_for_photo();
  require(f.scene.meshes().empty() && f.presentation.statistics().actors == 0 &&
              f.presentation.statistics().pose_updates == 0,
          "An enabled photo refresh before any actor installs is an empty no-op");
  f.add("PhotoEarly", CharacterRole::Commuter);
  const PhotoReference early(f, 0);
  f.presentation.refresh_for_photo();
  early.check(f, 0);
  const auto early_state = current_animation(f.presentation, f.actors[0].id);
  const auto early_near_revision = f.entity(0).mesh->geometry_revision;
  const auto early_far_revision = f.entity(0).lod_mesh->geometry_revision;
  f.add("CrewSparrow", CharacterRole::CrewScout, false);
  f.actors[1].motion.actual_speed = 2.f; f.actors[1].motion.move_weight = 1.f;
  for (int frame = 0; frame < 9; ++frame) {
    f.actors[1].motion.travel_distance += 2. / 60.; f.tick(1, 1.f / 60.f, 60.f);
  }
  const PhotoReference late(f, 1);
  f.presentation.refresh_for_photo(); late.check(f, 1);
  require(f.presentation.statistics().actors == 2 &&
              same_animation(early_state, current_animation(f.presentation, f.actors[0].id)) &&
              f.entity(0).mesh->geometry_revision == early_near_revision &&
              f.entity(0).lod_mesh->geometry_revision == early_far_revision,
          "The next photo includes a late hidden install and leaves already-current earlier actors unchanged");
  auto& respawn = f.entity(1);
  const MeshOwnership respawn_near(respawn.mesh), respawn_far(respawn.lod_mesh);
  const auto mesh_count = f.scene.meshes().size();
  respawn.transform.position.x += 11.f;
  f.presentation.install(f.scene, respawn, f.actors[1].height, f.actors[1].role, {});
  const auto reset = current_animation(f.presentation, f.actors[1].id);
  require(!reset.initialized && !reset.gait_started, "Reinstall gives photo refresh a newly reset animation state");
  const PhotoReference reset_reference(f, 1);
  f.presentation.refresh_for_photo(); reset_reference.check(f, 1);
  f.actors[1].motion.position = respawn.transform.position;
  f.actors[1].motion.travel_distance = 123.;
  f.tick(1, 1.f / 240.f, 5.f);
  const auto initialized = current_animation(f.presentation, f.actors[1].id);
  require(initialized.sample.phase_cycles == 0., "Reinstall's first travel sample does not invent travelled distance");
  const PhotoReference reinstalled(f, 1);
  f.presentation.refresh_for_photo(); reinstalled.check(f, 1);
  respawn_near.check(respawn.mesh); respawn_far.check(respawn.lod_mesh);
  require(f.scene.meshes().size() == mesh_count && f.presentation.statistics().actors == 2,
          "Photo entry after reinstall reuses the actor's existing Near and Far allocations");
}
}  // namespace
int main() {
  try {
    test_role_mapping();
    test_disabled_preserves_legacy();
    test_gameplay_and_respawn_contracts();
    test_motion_talk_and_determinism();
    test_lod_cadence_and_visibility();
    test_review_refresh();
    test_invalid_lod_policy();
    test_invalid_samples_and_recovery();
    test_audit_output();
    test_read_only_animation_witness();
    test_photo_refresh_all_actors_and_current_pose();
    test_photo_refresh_preserves_future_behavior();
    test_photo_refresh_disabled_empty_and_late_install();
    std::cout << "npc_presentation: " << checks << " checks passed; cached identity/respawn, zero-allocation update, "
              << "legacy parity, gameplay isolation, idle/move/talk, LOD, visibility and finite guards\n";
    return 0;
  } catch (const std::exception& error) {
    count_allocations = false;
    std::cerr << "npc_presentation FAILED after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
