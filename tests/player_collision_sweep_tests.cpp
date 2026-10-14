#include "fury/camera.hpp"
#include "fury/collision.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace fury;
unsigned checks = 0;

void require(bool condition, const char* message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}

bool same(const Vec3& a, const Vec3& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

// Frozen retained Metro witness, with the authored post's world-space proxy.
// Expected stops/faces are immutable float observations, not solver outputs
// calculated at test runtime. No production-private helpers are included.
const Aabb metro = Aabb::from_center_size({10.f, 1.5f, 12.f}, {.14f, 3.f, .14f});
const Vec3 west{9.447225570678711f, 1.7000000476837158f, 12.000000953674316f};
const Vec3 west_desired{10.197225570678711f, 1.7000000476837158f, 12.000000953674316f};
const Vec3 east{10.552774429321289f, 1.7000000476837158f, 12.000000953674316f};
const Vec3 east_desired{9.802774429321289f, 1.7000000476837158f, 12.000000953674316f};
constexpr float west_face = 9.480000495910645f;
constexpr float east_face = 10.519999504089355f;
constexpr float north_face = 11.480000495910645f;
constexpr float south_face = 12.519999504089355f;
constexpr float west_stop = 9.479000091552734f;
constexpr float east_stop = 10.520999908447266f;
constexpr float radius = .45f;

const Vec3& certified(const GroundedMoveResult& result) {
  require(result.outcome == GroundedMoveOutcome::CertifiedSweep,
          "valid start was not certified");
  require(result.sweep.accepted_position.has_value(), "missing certified position");
  require(!result.legacy_position, "certified motion also enabled legacy recovery");
  require(result.sweep.contacts <= player_sweep_max_contacts, "contact budget exceeded");
  return *result.sweep.accepted_position;
}

void expect_clear(Vec3 start, Vec3 desired, const std::vector<Aabb>& solids = {metro}) {
  const auto result = resolve_grounded_player_movement(start, desired, radius, solids);
  require(same(certified(result), desired), "clear/tangent/away target changed");
  require(result.sweep.outcome == PlayerSweepOutcome::Clear, "clear path reported blocked");
  require(!result.sweep.blocked_x && !result.sweep.blocked_z && result.sweep.contacts == 0,
          "clear path retained stale contact state");
}

void retained_crossings() {
  struct Case { Vec3 start, desired; float stop; bool from_west; };
  const Case cases[] = {
      {west, west_desired, west_stop, true},
      {east, east_desired, east_stop, false},
      {{8.f, 1.7f, 12.f}, {17.f, 1.7f, 12.f}, west_stop, true},
      {{12.f, 1.7f, 12.f}, {3.f, 1.7f, 12.f}, east_stop, false},
      {{-500.f, 1.7f, 12.f}, {500.f, 1.7f, 12.f}, west_stop, true},
      {{500.f, 1.7f, 12.f}, {-500.f, 1.7f, 12.f}, east_stop, false},
  };
  for (const auto& item : cases) {
    const auto direct = sweep_player_collision(item.start, item.desired, radius, {metro});
    const auto result = resolve_grounded_player_movement(item.start, item.desired, radius, {metro});
    const Vec3 p = certified(result);
    require(direct.accepted_position && same(*direct.accepted_position, p),
            "dispatch changed the public sweep result");
    require(result.sweep.outcome == PlayerSweepOutcome::Blocked &&
                result.sweep.blocked_x && !result.sweep.blocked_z && result.sweep.contacts == 1,
            "Metro crossing did not produce one X contact");
    require(p.x == item.stop && p.y == item.desired.y && p.z == item.desired.z,
            "retained Metro near-side stop changed");
    // For these constant-Z segments, staying on the start's side of the frozen
    // face proves no open-interior crossing, even when the target is far beyond.
    require(item.from_west ? p.x < west_face && p.x >= item.start.x
                           : p.x > east_face && p.x <= item.start.x,
            "accepted movement tunneled or moved behind its start");
  }
  for (bool from_north : {true, false}) {
    const Vec3 a{10.f, 1.7f, from_north ? 10.f : 14.f};
    const Vec3 b{10.f, 1.7f, from_north ? 14.f : 10.f};
    const auto result = resolve_grounded_player_movement(a, b, radius, {metro});
    const Vec3 p = certified(result);
    require(result.sweep.blocked_z && !result.sweep.blocked_x && result.sweep.contacts == 1,
            "Metro Z crossing did not block only Z");
    require(from_north ? p.z < north_face && p.z >= a.z : p.z > south_face && p.z <= a.z,
            "accepted Z segment crossed the frozen near face");
    require(p.x == a.x && p.y == b.y, "Z collision moved X or Y");
  }
}

void contact_and_slide() {
  expect_clear({west_face, 1.7f, 12.f}, {west_face, 1.7f, 13.f});
  expect_clear({east_face, 1.7f, 12.f}, {east_face, 1.7f, 11.f});
  expect_clear({10.f, 1.7f, north_face}, {11.f, 1.7f, north_face});
  expect_clear({10.f, 1.7f, south_face}, {9.f, 1.7f, south_face});
  expect_clear({west_face, 1.7f, 12.f}, {8.f, 1.7f, 12.f});
  expect_clear({east_face, 1.7f, 12.f}, {12.f, 1.7f, 12.f});
  expect_clear(west, {west_face, west.y, west.z});
  expect_clear(east, {east_face, east.y, east.z});
  expect_clear(west, west);
  expect_clear(west, west_desired, {});
  expect_clear({8.f, 1.7f, 11.4f}, {12.f, 1.7f, 11.4f});
  for (float face : {west_face, east_face}) {
    const Vec3 a{face, 1.7f, 12.f};
    const auto result = resolve_grounded_player_movement(a, {10.f, 1.7f, 12.f}, radius, {metro});
    require(same(certified(result), a), "inward motion from exact contact crossed its face");
    require(result.sweep.outcome == PlayerSweepOutcome::Blocked && result.sweep.contacts == 1,
            "inward face contact did not block");
  }
  const auto slide = resolve_grounded_player_movement({9.f, 1.7f, 12.f}, {10.f, 1.7f, 13.f}, radius, {metro});
  require(same(certified(slide), {west_stop, 1.7f, 13.f}), "slide lost its free Z component");
  require(slide.sweep.blocked_x && !slide.sweep.blocked_z, "slide blocked the free axis");
}

void released_camera_requests() {
  // Camera::update supplies real held/released desired positions. This tests
  // Camera + public resolver composition, not Application's velocity projection,
  // owner dispatch, SDL key delivery, or a rendered/native gameplay loop.
  for (bool from_west : {true, false}) {
    for (float dt : {1.f / 120.f, 1.f / 60.f, 1.f / 12.f, .25f, 1.f}) {
      for (bool held : {true, false}) {
        Camera camera;
        camera.position = from_west ? west : east;
        camera.fly_mode = false;
        camera.yaw = camera.yaw_target = 0.f;
        camera.pitch = camera.pitch_target = 0.f;
        camera.move_speed = 9.f;
        camera.velocity = {from_west ? 9.f : -9.f, 0.f, 0.f};
        camera.coyote_wish = {from_west ? 1.f : -1.f, 0.f, 0.f};
        camera.coyote_timer = .12f;
        const Vec3 start = camera.position;
        InputState input;
        input.key_w = held && from_west;
        input.key_s = held && !from_west;
        camera.update(input, dt);
        const auto result = resolve_grounded_player_movement(start, camera.position, radius, {metro});
        const Vec3 p = certified(result);
        require(from_west ? p.x < west_face && p.x >= start.x
                          : p.x > east_face && p.x <= start.x,
                "held/released Camera request tunneled");
        require(p.z == start.z && p.y == camera.position.y, "coast collision changed Z/Y");
        if (!held && dt == 1.f / 60.f) {
          require(from_west ? camera.position.x > west_face : camera.position.x < east_face,
                  "release fixture no longer requests a crossing");
          require(result.sweep.outcome == PlayerSweepOutcome::Blocked,
                  "release coast crossing was not blocked");
        }
      }
    }
  }
}

void height_and_radius() {
  const Aabb low = Aabb::from_center_size({10.f, .5f, 12.f}, {.14f, 1.f, .14f});
  const Aabb overhead = Aabb::from_center_size({10.f, 2.5f, 12.f}, {.14f, 1.f, .14f});
  struct Case { Aabb solid; float start_y, desired_y; bool blocked; };
  const Case cases[] = {
      {low, 1.7f, 1.05f, true}, {low, 1.05f, 1.7f, false},
      {low, 8.f, 1.5f, true}, {low, 1.f, std::nextafter(1.5f, 2.f), false},
      {metro, 1.f, 3.5f, true}, {metro, 1.f, std::nextafter(3.5f, 4.f), false},
      {overhead, 1.7f, 1.7f, false}, {overhead, 1.7f, 2.f, true},
  };
  for (const auto& item : cases) {
    const Vec3 desired{12.f, item.desired_y, 12.f};
    const auto result = resolve_grounded_player_movement({8.f, item.start_y, 12.f}, desired, radius, {item.solid});
    const Vec3 p = certified(result);
    require(p.y == item.desired_y, "sweep changed desired eye height");
    require((result.sweep.outcome == PlayerSweepOutcome::Blocked) == item.blocked,
            "desired-eye vertical gate changed");
    require(item.blocked ? p.x == west_stop : same(p, desired), "height gate selected wrong endpoint");
  }
  // Binary-exact box and radii give independent expanded faces 1.5, 1, and 0.5.
  const Aabb exact{{3.f, 1.f, 0.f}, {1.f, 1.f, 1.f}};
  for (float r : {.5f, 1.f, 1.5f}) {
    const auto result = resolve_grounded_player_movement({0.f, 1.7f, 0.f}, {6.f, 1.7f, 0.f}, r, {exact});
    const Vec3 p = certified(result);
    require(p.x < 2.f - r && p.x > 2.f - r - .002f, "caller radius ignored");
  }
}

void malformed_input_priority() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  const Vec3 embedded{10.f, 1.7f, 12.f};
  const auto held = [](Vec3 a, Vec3 b, float r, const std::vector<Aabb>& solids) {
    const auto result = resolve_grounded_player_movement(a, b, r, solids);
    require(result.outcome == GroundedMoveOutcome::Held &&
                result.sweep.outcome == PlayerSweepOutcome::InvalidInput,
            "malformed input was mistaken for valid motion/InvalidStart");
    require(!result.sweep.accepted_position && !result.legacy_position,
            "malformed input enabled legacy recovery or certification");
    require(result.sweep.contacts == 0 && !result.sweep.blocked_x && !result.sweep.blocked_z,
            "invalid input moved before validation completed");
  };
  for (float r : {0.f, -.45f, nan, inf, 1025.f}) held(embedded, west_desired, r, {metro});
  for (Vec3 bad : {Vec3{nan, 1.7f, 12.f}, Vec3{10.f, inf, 12.f},
                   Vec3{10.f, 1.7f, -inf}, Vec3{1025.f, 1.7f, 12.f}}) {
    held(bad, west_desired, radius, {metro});
    held(embedded, bad, radius, {metro});
  }
  held({-600.f, 1.7f, 12.f}, {600.f, 1.7f, 12.f}, radius, {});
  for (Aabb bad : {Aabb{{nan, 10.f, 0.f}, {1.f, 1.f, 1.f}},
                   Aabb{{0.f, 10.f, 0.f}, {1.f, -1.f, 1.f}},
                   Aabb{{0.f, 10.f, 0.f}, {1.f, inf, 1.f}},
                   Aabb{{1024.f, 10.f, 0.f}, {1.f, 1.f, 1.f}},
                   Aabb{{0.f, 1024.f, 0.f}, {1.f, 1.f, 1.f}}}) {
    held(embedded, west_desired, radius, {metro, bad});
    held(embedded, west_desired, radius, {bad, metro});
    held(west, west_desired, radius, {metro, bad});
  }
}

void expect_uncertified(const GroundedMoveResult& result, Vec3 legacy) {
  require(result.outcome == GroundedMoveOutcome::LegacyRecoveryUncertified,
          "embedded start lost explicit uncertified legacy status");
  require(result.sweep.outcome == PlayerSweepOutcome::InvalidStart &&
              !result.sweep.accepted_position, "embedded start was falsely sweep-certified");
  require(result.legacy_position && same(*result.legacy_position, legacy),
          "frozen legacy endpoint changed");
}

void legacy_is_not_a_safe_path() {
  // Far desired endpoint is clear, but the segment leaves the post's interior.
  // Preserve the observed unsafe legacy behavior without calling it recovery.
  const auto far = resolve_grounded_player_movement({10.f, 1.7f, 12.f}, {14.f, 1.7f, 12.f}, radius, {metro});
  expect_uncertified(far, {14.f, 1.7f, 12.f});
  const auto embedded = sweep_player_collision({10.f, 1.7f, 12.f}, west, radius, {metro});
  require(embedded.outcome == PlayerSweepOutcome::InvalidStart && !embedded.accepted_position,
          "direct sweep repaired an invalid start");
  const auto ulp = resolve_grounded_player_movement(
      {std::nextafter(west_face, 10.f), 1.7f, 12.f}, {14.f, 1.7f, 12.f}, radius, {metro});
  expect_uncertified(ulp, {14.f, 1.7f, 12.f});
  expect_clear({std::nextafter(west_face, 0.f), 1.7f, 12.f}, {8.f, 1.7f, 12.f});

  // Small, deliberately overlapping synthetic fixture. Legacy's two passes
  // finish at x=-0.5, still strictly inside the first expanded box (-1.5,1.5).
  // This is a residual-overlap regression, not a claim to replay Crown/Ashcourt.
  const std::vector<Aabb> overlapping = {
      {{0.f, 1.f, 0.f}, {1.f, 1.f, 10.f}}, {{-2.f, 1.f, 0.f}, {1.f, 1.f, 10.f}}};
  const auto residual = resolve_grounded_player_movement({0.f, 1.7f, 0.f}, {0.f, 1.05f, 0.f}, .5f, overlapping);
  expect_uncertified(residual, {-.5f, 1.05f, 0.f});
  require(residual.legacy_position->x > -1.5f && residual.legacy_position->x < 1.5f &&
              residual.legacy_position->z > -10.5f && residual.legacy_position->z < 10.5f,
          "fixture no longer demonstrates inherited residual overlap");

  // Desired Y can activate a previously inactive low solid at the same XZ.
  const Aabb low{{10.f, .5f, 12.f}, {.07f, .5f, .07f}};
  expect_clear({10.f, 1.05f, 12.f}, {10.f, 1.7f, 12.f}, {low});
  expect_uncertified(resolve_grounded_player_movement(
      {10.f, 1.7f, 12.f}, {14.f, 1.05f, 12.f}, radius, {low}), {14.f, 1.05f, 12.f});
}

void bounded_contacts() {
  // Binary-exact expanded faces x=1, z=1. Equal and sequential contacts
  // exercise both blocked axes without injecting outcomes/private helpers.
  const Aabb x_wall{{2.f, 1.f, 0.f}, {.5f, 1.f, 10.f}};
  const Aabb z_wall{{0.f, 1.f, 2.f}, {10.f, 1.f, .5f}};
  const Vec3 desired{3.f, 1.7f, 3.f};
  require(player_sweep_max_contacts == 4, "documented contact budget changed");
  for (bool sequential : {false, true}) {
    const Vec3 start = sequential ? Vec3{0.f, 1.7f, -2.f} : Vec3{-1.f, 1.7f, -1.f};
    Vec3 expected;
    for (int order = 0; order != 3; ++order) {
      std::vector<Aabb> walls = order == 1 ? std::vector<Aabb>{z_wall, x_wall}
                                          : std::vector<Aabb>{x_wall, z_wall};
      if (order == 2) { walls.push_back(x_wall); walls.push_back(z_wall); }
      const auto result = resolve_grounded_player_movement(start, desired, .5f, walls);
      const Vec3 p = certified(result);
      require(result.sweep.outcome == PlayerSweepOutcome::Blocked &&
                  result.sweep.blocked_x && result.sweep.blocked_z, "corner did not block both axes");
      require(result.sweep.contacts == (sequential ? 2u : 1u), "equal/sequential contact count changed");
      require(p.x < 1.f && p.z < 1.f && p.x >= start.x && p.z >= start.z,
              "corner endpoint crossed a frozen face");
      if (order == 0) expected = p;
      else require(same(p, expected), "solid order/duplicates changed corner result");
    }
  }
  // The current 2D solver permanently zeros at least one new axis per contact,
  // so valid finite geometry reaches at most two contacts. Its budget is four:
  // no natural ContactLimit or NumericalFailure is claimed by this test.
}
}  // namespace

int main() {
  try {
    retained_crossings();
    contact_and_slide();
    released_camera_requests();
    height_and_radius();
    malformed_input_priority();
    legacy_is_not_a_safe_path();
    bounded_contacts();
    std::cout << "player_collision_sweep: " << checks << " checks passed\n";
    std::cout << "Public API only; ContactLimit/NumericalFailure not induced; legacy remains uncertified\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "player_collision_sweep failed at check " << checks << ": " << error.what() << '\n';
    return 1;
  }
}
