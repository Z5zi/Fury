#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/metro_watch_anatomy.hpp"
#include "fury/generated/metro_watch_hands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
using namespace fury;
void require(bool value, const char *why) {
  if (!value)
    throw std::runtime_error(why);
}
void require(bool value, const std::string &why) {
  if (!value)
    throw std::runtime_error(why);
}
std::string label(const CharacterModel &m) {
  return (m.lod == CharacterLod::Near ? "Near" : "Far") + std::string(" h=") +
         std::to_string(m.rig.height);
}
bool finite(Vec3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
int surface(Vec2 uv) {
  for (int i = 0; i < int(CharacterSurface::Count); ++i) {
    const auto &r = character_surface_region(CharacterSurface(i));
    const float s = float(character_surface_atlas_size);
    if (uv.x >= r.x / s && uv.x < (r.x + r.size) / s && uv.y >= r.y / s &&
        uv.y < (r.y + r.size) / s)
      return i;
  }
  return -1;
}
bool near(Vec3 a, Vec3 b, float tol = 1e-6f) { return length(a - b) <= tol; }
void mesh_check(const CharacterModel &m, const Mesh &mesh, bool bind) {
  require(mesh.vertices.size() == m.influences.size(),
          "Influence count mismatch");
  require(mesh.indices.size() % 3 == 0, "Incomplete indexed triangle");
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    const auto &v = mesh.vertices[i];
    const auto &w = m.influences[i];
    if (!(finite(v.position) && finite(v.normal) && finite(v.color)))
      throw std::runtime_error("Nonfinite vertex " + std::to_string(i));
    const float normal = length(v.normal);
    if (!(normal > .98f && normal < 1.02f))
      throw std::runtime_error("Invalid normal " + std::to_string(i) +
                               " magnitude " + std::to_string(normal));
    require(surface(v.uv) >= 0, "Vertex outside supported surface tile");
    require(w.first < character_bone_count && w.second < character_bone_count &&
                w.third < character_bone_count,
            "Invalid bone");
    require(w.first_weight >= 0 && w.third_weight >= 0 &&
                w.first_weight + w.third_weight <= 1.00001f,
            "Invalid normalized weight");
  }
  float minimum = 1.f;
  std::size_t reversed = 0;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    const auto a = mesh.indices[i], b = mesh.indices[i + 1],
               c = mesh.indices[i + 2];
    require(a < mesh.vertices.size() && b < mesh.vertices.size() &&
                c < mesh.vertices.size(),
            "Out of range triangle");
    const auto &A = mesh.vertices[a], &B = mesh.vertices[b],
               &C = mesh.vertices[c];
    require(surface(A.uv) == surface(B.uv) && surface(A.uv) == surface(C.uv),
            "Triangle crosses material tile");
    const Vec3 cross_normal =
        cross(B.position - A.position, C.position - A.position);
    const float area =
        dot(cross_normal, cross_normal) /
        (m.rig.height * m.rig.height * m.rig.height * m.rig.height);
    minimum = std::min(minimum, area);
    if (!(area > 1e-24f))
      throw std::runtime_error("Degenerate triangle " + std::to_string(i / 3));
    if (dot(cross_normal, A.normal + B.normal + C.normal) <= 0) {
      if (bind && m.rig.height == 1.82f)
        std::cout
            << "opposed triangle=" << i / 3 << " surface=" << surface(A.uv)
            << " center/h="
            << (A.position.x + B.position.x + C.position.x) / (3 * m.rig.height)
            << ","
            << (A.position.y + B.position.y + C.position.y) / (3 * m.rig.height)
            << ","
            << (A.position.z + B.position.z + C.position.z) / (3 * m.rig.height)
            << '\n';
      ++reversed;
    }
  }
  if (bind)
    require(reversed == 0, "Bind triangle opposes its smoothed vertex normals");
  if (bind)
    std::cout << "bind triangles=" << mesh.indices.size() / 3
              << " minimum cross squared/h4=" << minimum
              << " opposed smooth normals=" << reversed << '\n';
}
void source_head(const CharacterModel &m) {
  std::vector<unsigned> triangles;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3)
    if (surface(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) ==
        int(CharacterSurface::Face))
      triangles.push_back(t);
  require(triangles.size() == metro_watch_anatomy::indices.size() / 3,
          "Dropped or added source Face triangles");
  for (unsigned i = 0; i < triangles.size(); ++i)
    for (int k = 0; k < 3; ++k) {
      const auto &v =
          m.bind_mesh.vertices[m.bind_mesh.indices[triangles[i] + k]];
      const auto expected =
          metro_watch_anatomy::positions[metro_watch_anatomy::indices[3 * i +
                                                                      k]] *
          m.rig.height;
      require(near(v.position, expected, 2e-7f),
              "Source anatomical position or corner order changed");
      const auto source_uv = metro_watch_anatomy::uv_coordinates
          [metro_watch_anatomy::uv_indices[3 * i + k]];
      const auto expected_uv = character_surface_uv(CharacterSurface::Face,
                                                    source_uv.x, source_uv.y);
      require(v.uv.x == expected_uv.x && v.uv.y == expected_uv.y,
              "Canonical per-corner Face UV changed");
    }
  // The connected lower neck underlay must meet every imported attachment
  // point.
  for (auto source : metro_watch_anatomy::attachment_boundary_indices) {
    const Vec3 expected = metro_watch_anatomy::positions[source] * m.rig.height;
    int matches = 0;
    for (const auto &v : m.bind_mesh.vertices)
      if (surface(v.uv) == int(CharacterSurface::Skin) &&
          near(v.position, expected, 3e-7f))
        ++matches;
    require(matches >= 1, "Missing exact connected neck attachment");
  }
}
std::array<std::vector<Vec3>, 2> source_eye_openings() {
  std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> charts;
  for (std::size_t i = 0; i < metro_watch_anatomy::indices.size(); i += 3) {
    const auto uv =
        metro_watch_anatomy::uv_coordinates[metro_watch_anatomy::uv_indices[i]];
    const unsigned chart = uv.x < .08f ? 2 : uv.x < .16f ? 1 : 0;
    for (int c = 0; c < 3; ++c) {
      auto a = metro_watch_anatomy::indices[i + c],
           b = metro_watch_anatomy::indices[i + (c + 1) % 3];
      if (a > b)
        std::swap(a, b);
      charts[{a, b}] |= 1u << chart;
    }
  }
  std::array<std::set<std::uint32_t>, 2> corners;
  for (const auto &e : charts)
    for (unsigned side = 0; side < 2; ++side)
      if (e.second == (1u | (1u << (side + 1)))) {
        corners[side].insert(e.first.first);
        corners[side].insert(e.first.second);
      }
  std::array<std::vector<Vec3>, 2> result;
  for (unsigned side = 0; side < 2; ++side) {
    require(corners[side].size() >= 20,
            "Both independent socket-to-skin seams are recovered");
    for (auto i : corners[side])
      result[side].push_back(metro_watch_anatomy::positions[i]);
  }
  return result;
}

float projected_front(Vec3 p, const Mesh &mesh,
                      const std::vector<std::size_t> &triangles) {
  float front = -std::numeric_limits<float>::infinity();
  for (auto t : triangles) {
    const auto a = mesh.vertices[mesh.indices[t]].position;
    const auto b = mesh.vertices[mesh.indices[t + 1]].position;
    const auto c = mesh.vertices[mesh.indices[t + 2]].position;
    const float d = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(d) < 1e-14f)
      continue;
    const float u = ((p.x - a.x) * (c.y - a.y) - (p.y - a.y) * (c.x - a.x)) / d;
    const float v = ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) / d;
    if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
      front = std::max(front, a.z + u * (b.z - a.z) + v * (c.z - a.z));
  }
  return front;
}

void check_socket_fit(const CharacterModel &model) {
  const auto openings = source_eye_openings();
  std::vector<std::size_t> eyes, face;
  for (std::size_t t = 0; t < model.bind_mesh.indices.size(); t += 3) {
    const int material =
        surface(model.bind_mesh.vertices[model.bind_mesh.indices[t]].uv);
    if (material == int(CharacterSurface::Eyes))
      eyes.push_back(t);
    if (material == int(CharacterSurface::Face))
      face.push_back(t);
  }
  float nearest_gap = std::numeric_limits<float>::infinity(),
        largest_protrusion = -nearest_gap;
  for (const auto &ring : openings) {
    Vec3 center{};
    for (auto p : ring)
      center += p;
    center = center * (1.f / ring.size());
    // Every rim point and inward spoke has real globe/iris geometry behind it.
    for (auto rim : ring)
      for (float fraction : {0.f, .25f, .5f, .75f, 1.f}) {
        const auto p =
            (center * (1 - fraction) + rim * fraction) * model.rig.height;
        const float front = projected_front(p, model.bind_mesh, eyes);
        require(std::isfinite(front),
                label(model) +
                    "eye mesh covers every socket rim and aperture spoke");
        if (fraction < 1) {
          const float skin_front = projected_front(p, model.bind_mesh, face);
          require(front > skin_front + model.rig.height * .0001f,
                  label(model) +
                      "eye remains visible through the actual socket aperture");
        }
        if (fraction == 1) {
          const float delta = (front - p.z) / model.rig.height;
          nearest_gap = std::min(nearest_gap, delta);
          largest_protrusion = std::max(largest_protrusion, delta);
        }
      }
  }
  std::cout << label(model) << ": actual socket-rim eye depth/h " << nearest_gap
            << " .. " << largest_protrusion << '\n';
  require(nearest_gap > -.003f && largest_protrusion < .002f,
          label(model) + " actual eye rim range/h " +
              std::to_string(nearest_gap) + ".." +
              std::to_string(largest_protrusion) + " exceeds [-.003,.002]");
}

void source_hand_topology() {
  for (bool far : {false, true}) {
    std::map<std::pair<unsigned, unsigned>, unsigned> edges;
    std::map<unsigned, std::set<unsigned>> adj;
    const auto check = [&](auto const &indices) {
      for (unsigned i = 0; i < indices.size(); i += 3)
        for (int k = 0; k < 3; ++k) {
          unsigned a = indices[i + k], b = indices[i + (k + 1) % 3];
          require(a != b, "Repeated hand index");
          adj[a].insert(b);
          adj[b].insert(a);
          if (a > b)
            std::swap(a, b);
          ++edges[{a, b}];
        }
    };
    if (far)
      check(metro_watch_hands::far_indices);
    else
      check(metro_watch_hands::near_indices);
    unsigned boundary = 0;
    for (auto const &e : edges) {
      require(e.second <= 2, "Nonmanifold hand");
      boundary += e.second == 1;
    }
    require(boundary == 20, "Hand wrist boundary changed");
    std::set<unsigned> seen;
    std::vector<unsigned> stack{adj.begin()->first};
    while (!stack.empty()) {
      auto v = stack.back();
      stack.pop_back();
      if (!seen.insert(v).second)
        continue;
      for (auto n : adj[v])
        stack.push_back(n);
    }
    require(seen.size() == adj.size(), "Hand has disconnected fingers/palm");
    const std::size_t t = far ? metro_watch_hands::far_indices.size() / 3
                              : metro_watch_hands::near_indices.size() / 3;
    require(int(adj.size()) - int(edges.size()) + int(t) == 1,
            "Hand topology has extra hole");
  }
}
#include "character_metro_watch_contacts.hpp"

void poses(const CharacterModel &m, bool ordinary_only) {
  Mesh posed = m.bind_mesh;
  const auto *vptr = posed.vertices.data();
  const auto *iptr = posed.indices.data();
  const auto indices = posed.indices;
  unsigned samples = 0;
  MetroContacts contacts(m);
  contacts.check(m.bind_mesh, label(m) + " bind");
  for (int mode = 0; mode < 12; ++mode)
    for (int step = 0; step < 24; ++step) {
      if (ordinary_only && (mode == 4 || mode == 9))
        continue;
      CharacterAnimationSample s;
      s.seed = m.seed;
      s.idle_time = step / 12.;
      s.phase_cycles = step / 24.;
      if (mode == 1 || mode == 2 || mode == 4) {
        s.speed = mode == 2 ? 4.f : mode == 4 ? 1.f : 1.8f;
        s.move_weight = 1;
      }
      if (mode == 3 || mode == 4)
        s.crouch_weight = 1;
      if (mode == 5)
        s.talk_weight = 1;
      if (mode == 6) {
        s.attention_weight = 1;
        s.attention_yaw = step % 2 ? .9f : -.9f;
        s.attention_pitch = step % 3 ? .4f : -.4f;
      }
      if (mode >= 7 && mode <= 9) {
        s.speed = m.rig.height * (mode == 7 ? .85f : mode == 8 ? 2.6f : .55f);
        s.move_weight = 1;
        if (mode == 9)
          s.crouch_weight = 1;
      }
      if (mode == 10 || mode == 11) {
        s.attention_weight = 1;
        s.attention_yaw = step % 2 ? .9f : -.9f;
        s.attention_pitch = step % 3 ? .4f : -.4f;
        if (mode == 10)
          s.talk_weight = 1;
        else
          s.crouch_weight = 1;
      }
      auto pose = sample_character_animation(m.rig, s);
      apply_character_pose(m, pose, posed);
      mesh_check(m, posed, false);
      for (unsigned i = 0; i < posed.vertices.size(); ++i) {
        const auto bone = m.influences[i].first;
        if (bone == bone_index(CharacterBone::LeftFoot) ||
            bone == bone_index(CharacterBone::RightFoot))
          require(posed.vertices[i].position.y >= -.5001f * m.rig.height,
                  "Actual boot sole penetrates ground");
        require(
            near(posed.vertices[i].color, m.bind_mesh.vertices[i].color, 0.f),
            "Pose changed exact vertex palette");
      }
      contacts.check(posed, label(m) + " mode=" + std::to_string(mode) +
                                " phase=" + std::to_string(step));
      require(posed.vertices.data() == vptr && posed.indices.data() == iptr,
              "Pose changed storage allocation");
      require(posed.indices == indices, "Pose changed indexed geometry");
      ++samples;
    }
  std::cout << "pose samples=" << samples
            << " stable mesh buffers/indices, finite surfaces, all "
               "garment/wrist/boot contacts\n";
}
void exact_height(const CharacterModel &m) {
  float low = 100.f, high = -100.f;
  for (auto const &v : m.bind_mesh.vertices) {
    low = std::min(low, v.position.y);
    high = std::max(high, v.position.y);
  }
  require(std::fabs(low + m.rig.height * .5f) < 1e-6f,
          "Soles changed actor floor");
  require(std::fabs(high - m.rig.height * .5f) < 1e-6f,
          "Cap changed actor ceiling");
  std::cout << "height=" << m.rig.height
            << " actual vertical extent=" << high - low << '\n';
}
} // namespace
int main(int argc, char **argv) {
  try {
    const bool ordinary_only =
        argc == 2 && std::strcmp(argv[1], "--ordinary") == 0;
    require(argc == 1 || ordinary_only,
            "Expected no arguments for full stress checks, or --ordinary");
    source_hand_topology();
    metro_intersection_controls();
    auto texture = character_surface_textures();
    for (float h : {1.82f, 1.35f, 2.10f})
      for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
        auto p = make_character_profile(h, CharacterRole::Security,
                                        "NpcExtraGuard", lod);
        auto &m = p.model;
        require(p.individualized, "Metro Watch did not select own profile");
        require(m.bind_mesh.indices.size() / 3 <=
                    (lod == CharacterLod::Near ? 40000u : 16000u),
                "Metro Watch exceeds established 40000 Near /16000 Far "
                "triangle cap: " +
                    std::to_string(m.bind_mesh.indices.size() / 3));
        require(m.seed == 0x3e2ad98fu, "Stable seed changed");
        require(m.role == CharacterRole::Security && m.rig.height == h,
                "Canonical role/height changed");
        require(m.dual_quaternion_skinning,
                "Missing individual rigid skinning");
        require(p.material.textures == texture,
                "Profile duplicates shared immutable texture atlas");
        const auto original = character_appearance(m.seed);
        require(near(m.appearance.skin, original.skin) &&
                    near(m.appearance.hair, original.hair),
                "Original skin/hair palette changed");
        mesh_check(m, m.bind_mesh, true);
        exact_height(m);
        metro_closed_layers(m);
        metro_scalp_coverage(m);
        source_head(m);
        check_socket_fit(m);
        poses(m, ordinary_only);
        if (h == 1.82f)
          metro_contact_classification_controls(m);
      }
    for (auto id :
         {"NpcExtraGuard_extra", "npcextraguard", "unknown_security_actor", "UnrelatedGuard"}) {
      auto p = make_character_profile(1.82f, CharacterRole::Security, id,
                                      CharacterLod::Near);
      require(!p.individualized,
              "Metro Watch captured an unregistered or non-exact identity");
    }
    auto wrong = make_character_profile(1.82f, CharacterRole::Market,
                                        "NpcExtraGuard", CharacterLod::Near);
    require(!wrong.individualized, "Profile matches wrong appearance role");
    std::cout
        << "PASS Metro Watch source, topology, height, identity, "
           "palettes, LOD, "
        << (ordinary_only ? 1440 : 1728)
        << " pose/contact samples and isolation"
        << (ordinary_only
                ? " (full crouch-walk remains a separate strict failure gate)"
                : "")
        << '\n';
    return 0;
  } catch (std::exception const &e) {
    std::cerr << "FAIL Metro Watch: " << e.what() << '\n';
    return 1;
  }
}
