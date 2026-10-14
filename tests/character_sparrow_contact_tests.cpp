#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace fury;
void require(bool c, const std::string &m) {
  if (!c)
    throw std::runtime_error(m);
}
int surface(Vec2 uv) {
  for (unsigned i = 0; i < unsigned(CharacterSurface::Count); ++i) {
    const auto lo = character_surface_uv(CharacterSurface(i), 0, 0),
               hi = character_surface_uv(CharacterSurface(i), 1, 1);
    if (uv.x >= lo.x - 1e-7f && uv.x <= hi.x + 1e-7f && uv.y >= lo.y - 1e-7f &&
        uv.y <= hi.y + 1e-7f)
      return int(i);
  }
  return -1;
}
struct Box {
  Vec3 lo{100, 100, 100}, hi{-100, -100, -100};
  void add(Vec3 p) {
    lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
    hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
  }
  void add(const Box &b) {
    add(b.lo);
    add(b.hi);
  }
  bool overlap(const Box &b) const {
    return lo.x <= b.hi.x && b.lo.x <= hi.x && lo.y <= b.hi.y &&
           b.lo.y <= hi.y && lo.z <= b.hi.z && b.lo.z <= hi.z;
  }
};
struct Tri {
  std::array<Vec3, 3> p;
  Box box;
  unsigned offset;
  int region;
  float original_y{};
};
bool segment(Vec3 p, Vec3 q, const Tri &t) {
  Vec3 dir = q - p, e = t.p[1] - t.p[0], f = t.p[2] - t.p[0], n = cross(dir, f);
  const Vec3 plane = normalize(cross(e, f));
  const float dp = dot(p - t.p[0], plane), dq = dot(q - t.p[0], plane);
  // Rigidly transformed coplanar cut edges must not turn into crossings due
  // to roundoff. Require a real passage through both sides of the plane.
  if (!((dp > 1e-7f && dq < -1e-7f) || (dq > 1e-7f && dp < -1e-7f)))
    return false;
  float d = dot(e, n);
  if (std::fabs(d) < 1e-13f)
    return false;
  Vec3 x = p - t.p[0], z = cross(x, e);
  float u = dot(x, n) / d, v = dot(dir, z) / d, w = dot(f, z) / d;
  constexpr float eps = 2e-5f;
  return u > eps && v > eps && u + v < 1 - eps && w > eps && w < 1 - eps;
}
bool crossing(const Tri &a, const Tri &b) {
  if (!a.box.overlap(b.box))
    return false;
  for (int k = 0; k < 3; ++k)
    if (segment(a.p[k], a.p[(k + 1) % 3], b) ||
        segment(b.p[k], b.p[(k + 1) % 3], a))
      return true;
  return false;
}
struct Node {
  Box box;
  unsigned begin{}, end{};
  int left{-1}, right{-1};
};
struct Tree {
  const std::vector<Tri> &triangles;
  std::vector<unsigned> order;
  std::vector<Node> nodes;
  explicit Tree(const std::vector<Tri> &t) : triangles(t) {
    order.resize(t.size());
    std::iota(order.begin(), order.end(), 0);
    nodes.reserve(t.size() * 2);
    build(0, order.size());
  }
  int build(unsigned a, unsigned b) {
    int id = nodes.size();
    nodes.push_back({});
    nodes[id].begin = a;
    nodes[id].end = b;
    for (unsigned i = a; i < b; ++i)
      nodes[id].box.add(triangles[order[i]].box);
    if (b - a <= 8)
      return id;
    Vec3 extent = nodes[id].box.hi - nodes[id].box.lo;
    int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2)
                                   : (extent.y > extent.z ? 1 : 2);
    auto pos = [&](unsigned i) {
      auto c = triangles[i].box.lo + triangles[i].box.hi;
      return axis == 0 ? c.x : axis == 1 ? c.y : c.z;
    };
    unsigned m = (a + b) / 2;
    std::nth_element(order.begin() + a, order.begin() + m, order.begin() + b,
                     [&](unsigned i, unsigned j) { return pos(i) < pos(j); });
    nodes[id].left = build(a, m);
    nodes[id].right = build(m, b);
    return id;
  }
  template <class F> void visit(const Tri &t, F fn, int id = 0) const {
    const auto &n = nodes[id];
    if (!n.box.overlap(t.box))
      return;
    if (n.left < 0) {
      for (unsigned i = n.begin; i < n.end; ++i)
        fn(triangles[order[i]]);
    } else {
      visit(t, fn, n.left);
      visit(t, fn, n.right);
    }
  }
};
// Regions come from actual material/tint/bone attachment, not generation order.
// 0 sewn jacket, 1 complete pack, 2 neck, 3/4 hands+forearms,
// 5/6 trousers, 7 front webbing/hardware, 8/9 shoes.
std::vector<std::pair<unsigned, int>> classify(const CharacterModel &m) {
  std::vector<std::pair<unsigned, int>> out;
  const auto &mesh = m.bind_mesh;
  float h = m.rig.height;
  for (unsigned t = 0; t < mesh.indices.size(); t += 3) {
    const auto &v = mesh.vertices[mesh.indices[t]];
    int s = surface(v.uv), r = -1;
    Vec3 c{};
    for (int k = 0; k < 3; ++k)
      c += mesh.vertices[mesh.indices[t + k]].position * (1 / (3 * h));
    if (s == int(CharacterSurface::Jacket)) {
      float scale = v.color.x / m.appearance.shirt.x;
      const bool garment =
          std::fabs(v.color.y - m.appearance.shirt.y * scale) < 1e-6f &&
          std::fabs(v.color.z - m.appearance.shirt.z * scale) < 1e-6f;
      r = garment ? 0 : 1;
    } else if ((s == int(CharacterSurface::Face) && c.y < .386f) ||
               (s == int(CharacterSurface::Skin) && c.y > .29f))
      r = 2;
    else if (s == int(CharacterSurface::Skin) && c.y < .20f)
      r = c.x < 0 ? 3 : 4;
    else if (s == int(CharacterSurface::Metal) && c.y > .10f && c.z > 0)
      r = 7;
    else if (s == int(CharacterSurface::Pants))
      r = c.x < 0 ? 5 : 6;
    else if (s == int(CharacterSurface::Leather) && c.y > .10f && c.z > 0 &&
             std::fabs(c.x) > .045f)
      r = 7;
    else if ((s == int(CharacterSurface::Leather) ||
              s == int(CharacterSurface::Rubber)) &&
             c.y < -.42f)
      r = c.x < 0 ? 8 : 9;
    if (r >= 0)
      out.push_back({t, r});
  }
  return out;
}
bool pair_allowed(int a, int b) {
  if (a > b)
    std::swap(a, b);
  if (a == 0)
    return b == 1 || b == 2 || b == 3 || b == 4 || b == 7;
  if (a == 1)
    return b == 2 || b == 3 || b == 4 || b == 5 || b == 6 || b == 7;
  if (a == 3 || a == 4)
    return b == 5 || b == 6;
  return false;
}
std::vector<Tri> triangles(const Mesh &mesh, float h,
                           const std::vector<std::pair<unsigned, int>> &groups,
                           const Mesh *bind = nullptr) {
  std::vector<Tri> out;
  out.reserve(groups.size());
  for (auto entry : groups) {
    Tri t{{}, {}, entry.first, entry.second};
    for (int i = 0; i < 3; ++i) {
      t.p[i] = mesh.vertices[mesh.indices[entry.first + i]].position * (1 / h);
      t.box.add(t.p[i]);
    }
    const auto &source = bind ? *bind : mesh;
    for (int i = 0; i < 3; ++i)
      t.original_y +=
          source.vertices[source.indices[entry.first + i]].position.y / (3 * h);
    out.push_back(t);
  }
  return out;
}
std::string audit(const std::vector<Tri> &ts) {
  Tree tree(ts);
  for (const auto &t : ts) {
    std::string failure;
    tree.visit(t, [&](const Tri &other) {
      if (!failure.empty() || other.offset <= t.offset ||
          !(pair_allowed(t.region, other.region) ||
            ((t.region == 5 || t.region == 6) &&
             (other.region == 5 || other.region == 6) &&
             t.original_y < -.065f && t.original_y > -.40f &&
             other.original_y < -.065f && other.original_y > -.40f) ||
            (((t.region == 5 || t.region == 6) && other.region >= 8 &&
              t.original_y > -.40f && t.original_y < -.065f) ||
             ((other.region == 5 || other.region == 6) && t.region >= 8 &&
              other.original_y > -.40f && other.original_y < -.065f))))
        return;
      for (auto a : t.p)
        for (auto b : other.p)
          if (length(a - b) < 1e-6f)
            return; // Shared physical seam/edge is intentional contact
      if (crossing(t, other))
        failure = "regions " + std::to_string(t.region) + "/" +
                  std::to_string(other.region) + " triangles " +
                  std::to_string(t.offset / 3) + "/" +
                  std::to_string(other.offset / 3) +
                  " bind_y=" + std::to_string(t.original_y) + "," +
                  std::to_string(other.original_y) +
                  " first_pos=" + std::to_string(t.p[0].x) + "," +
                  std::to_string(t.p[0].y) + "," + std::to_string(t.p[0].z);
    });
    if (!failure.empty())
      return failure;
  }
  return {};
}
void negative_controls(const std::vector<Tri> &ts) {
  std::array<const Tri *, 10> picked{};
  for (const auto &t : ts)
    if (!picked[t.region])
      picked[t.region] = &t;
  for (auto p : picked)
    require(p, "All actual garment/contact regions exist");
  const auto inject = [&](Tri target, Tri moving, int first, int second) {
    Vec3 c = (target.p[0] + target.p[1] + target.p[2]) * (1.f / 3),
         n = normalize(
             cross(target.p[1] - target.p[0], target.p[2] - target.p[0])),
         u = normalize(target.p[1] - target.p[0]);
    float d = std::min(length(target.p[1] - target.p[0]),
                       length(target.p[2] - target.p[0])) *
              .15f;
    moving.p = {c + n * d, c - n * d + u * d * .1f, c + n * d + u * d * .2f};
    moving.box = {};
    for (auto p : moving.p)
      moving.box.add(p);
    require(!audit({target, moving}).empty(),
            "Actual crossing negative control must reject regions " +
                std::to_string(first) + "/" + std::to_string(second));
  };
  for (auto pair : {std::pair<int, int>{0, 1},
                    {1, 2},
                    {1, 3},
                    {1, 4},
                    {3, 5},
                    {4, 6},
                    {1, 7}})
    inject(*picked[pair.first], *picked[pair.second], pair.first, pair.second);
  std::array<std::vector<Tri>, 2> knee;
  for (const auto &t : ts)
    if ((t.region == 5 || t.region == 6) && t.original_y > -.30f &&
        t.original_y < -.20f)
      knee[t.region - 5].push_back(t);
  require(knee[0].size() > 2 && knee[1].size() > 2,
          "Both actual knee neighborhoods exist for self-contact controls");
  inject(knee[0][0], knee[0][1], 5, 5);
  inject(knee[1][0], knee[1][1], 6, 6);
  inject(knee[0][0], knee[1][1], 5, 6);
  inject(knee[0][0], *picked[8], 5, 8);
  inject(knee[1][0], *picked[9], 6, 9);
  std::cout << "Twelve injected real-triangle contact controls passed\n";
}
} // namespace
// Independently intersect the submitted lower trouser and ankle-underlay
// triangles in bind pose. The old global clothing scope intentionally starts
// above the ankle and cannot detect a sock showing through a trouser hem.
std::string ankle_layer_crossing(const std::vector<Tri> &pants,
                                 const std::vector<Tri> &underlay,
                                 const char *layer = "Pants") {
  Tree tree(pants);
  std::string failure;
  for (const auto &sock : underlay) {
    tree.visit(sock, [&](const Tri &cloth) {
      if (failure.empty() && crossing(sock, cloth))
        failure = "ankle Shirt-surface triangle " +
                  std::to_string(sock.offset / 3) + " crosses " +
                  std::string(layer) + "-surface triangle " +
                  std::to_string(cloth.offset / 3) +
                  " at bind y=" + std::to_string(sock.p[0].y) +
                  " z=" + std::to_string(sock.p[0].z);
    });
    if (!failure.empty())
      break;
  }
  return failure;
}
void check_bind_ankle_layers(const fury::CharacterModel &m) {
  std::vector<std::pair<unsigned, int>> pants_ids, sock_ids, shoe_ids;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    const auto &v = m.bind_mesh.vertices[m.bind_mesh.indices[t]];
    const float y =
        (v.position.y +
         m.bind_mesh.vertices[m.bind_mesh.indices[t + 1]].position.y +
         m.bind_mesh.vertices[m.bind_mesh.indices[t + 2]].position.y) /
        (3 * m.rig.height);
    if (y >= -.42f)
      continue;
    if (surface(v.uv) == int(fury::CharacterSurface::Pants))
      pants_ids.push_back({t, 0});
    // The ankle liner is the blue-biased dark cloth. Warm brown laces use
    // the same atlas tile but are a separate sewn upper attachment.
    if (surface(v.uv) == int(fury::CharacterSurface::Shirt) &&
        v.color.z > v.color.x && v.color.z > v.color.y)
      sock_ids.push_back({t, 1});
    // Test the visible outer upper. Flat end caps are internal closures under
    // the leg/sole and intentionally intersect their anatomical attachment.
    const float range_y =
        std::max(
            {v.position.y,
             m.bind_mesh.vertices[m.bind_mesh.indices[t + 1]].position.y,
             m.bind_mesh.vertices[m.bind_mesh.indices[t + 2]].position.y}) -
        std::min({v.position.y,
                  m.bind_mesh.vertices[m.bind_mesh.indices[t + 1]].position.y,
                  m.bind_mesh.vertices[m.bind_mesh.indices[t + 2]].position.y});
    if (surface(v.uv) == int(fury::CharacterSurface::Leather) &&
        range_y > m.rig.height * 1e-5f)
      shoe_ids.push_back({t, 2});
  }
  const auto pants = triangles(m.bind_mesh, m.rig.height, pants_ids),
             socks = triangles(m.bind_mesh, m.rig.height, sock_ids),
             shoes = triangles(m.bind_mesh, m.rig.height, shoe_ids);
  require(!pants.empty() && !socks.empty(),
          "Actual trouser/ankle triangles exist");
  // Rotate and uniformly scale one real underlay triangle through a real
  // trouser triangle. No accepted geometry, tolerance or contact gate changes.
  const auto &target = pants[pants.size() / 2];
  Tri injected = socks[socks.size() / 2];
  const auto center = [](const Tri &t) {
    return (t.p[0] + t.p[1] + t.p[2]) * (1.f / 3);
  };
  const Vec3 a = center(injected), c = center(target),
             source_x = normalize(injected.p[1] - injected.p[0]),
             source_y = normalize(
                 cross(normalize(cross(injected.p[1] - injected.p[0],
                                       injected.p[2] - injected.p[0])),
                       source_x)),
             target_x = normalize(target.p[1] - target.p[0]),
             target_y = normalize(
                 cross(target.p[1] - target.p[0], target.p[2] - target.p[0]));
  float radius = 0;
  for (auto v : injected.p)
    radius = std::max(radius, length(v - a));
  const float scale = .12f * length(target.p[1] - target.p[0]) / radius;
  injected.box = Box{};
  for (auto &v : injected.p) {
    const Vec3 d = v - a;
    v = c + (target_x * dot(d, source_x) + target_y * dot(d, source_y)) * scale;
    injected.box.add(v);
  }
  require(!ankle_layer_crossing(pants, {injected}).empty(),
          "Actual ankle triangle negative control detects penetration");
  const auto failure = ankle_layer_crossing(pants, socks);
  require(failure.empty(), failure);
  const auto shoe_failure = ankle_layer_crossing(shoes, socks, "Leather");
  require(shoe_failure.empty(), shoe_failure);
  std::cout
      << "Independent bind ankle-underlay/Pants/Leather crossing check and "
         "actual-triangle negative control passed\n";
}

int main(int argc, char **argv) {
  bool quick = false, ordinary = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--quick")
      quick = true;
    else if (arg == "--ordinary")
      ordinary = true;
    else {
      std::cerr << "Unknown contact option\n";
      return 2;
    }
  }
  std::cout << (ordinary
                    ? "ORDINARY idle/walk/run/talk/static crouch and attention"
                    : "STRICT including crouch-walk")
            << " contact scope\n";
  int failures = 0;
  for (float h : {1.35f, 1.72f, 2.10f}) {
    if (quick && h != 1.72f)
      continue;
    for (auto lod : {fury::CharacterLod::Near, fury::CharacterLod::Far}) {
      auto p = fury::make_character_profile(h, fury::CharacterRole::CrewScout,
                                            "CrewSparrow", lod);
      auto &m = p.model;
      auto groups = classify(m);
      auto out = m.bind_mesh;
      const std::string context =
          std::string(lod == fury::CharacterLod::Near ? "near" : "far") +
          " height=" + std::to_string(h);
      try {
        auto ts = triangles(out, h, groups, &m.bind_mesh);
        negative_controls(ts);
        auto fail = audit(ts);
        require(fail.empty(), "bind " + fail);
        check_bind_ankle_layers(m);
        for (int mode = 0; mode < 7; ++mode)
          for (int frame = 0; frame < 32; frame += quick ? 8 : 2) {
            if (ordinary && mode == 5)
              continue;
            fury::CharacterAnimationSample s;
            s.seed = m.seed;
            s.phase_cycles = frame / 32.;
            s.idle_time = frame / 8.;
            if (mode == 1 || mode == 2 || mode == 5 || mode == 6) {
              s.move_weight = 1;
              s.speed = mode == 6 ? 6.5f
                                  : h * (mode == 2   ? 2.8f
                                         : mode == 5 ? .55f
                                                     : .8f);
            }
            if (mode == 3) {
              s.talk_weight = s.attention_weight = 1;
              s.attention_yaw = .8f;
              s.attention_pitch = -.2f;
            }
            if (mode == 4 || mode == 5)
              s.crouch_weight = 1;
            fury::apply_character_pose(
                m, fury::sample_character_animation(m.rig, s), out);
            fail = audit(triangles(out, h, groups, &m.bind_mesh));
            require(fail.empty(), "mode=" + std::to_string(mode) + " phase=" +
                                      std::to_string(frame) + " " + fail);
          }
        for (float yaw : {-.9f, 0.f, .9f})
          for (float pitch : {-.4f, .4f})
            for (int attention_frame : {0, 8, 16, 24, 32}) {
              fury::CharacterAnimationSample s;
              s.seed = m.seed;
              s.attention_weight = s.talk_weight = 1;
              s.idle_time = attention_frame / 8.;
              s.attention_yaw = yaw;
              s.attention_pitch = pitch;
              fury::apply_character_pose(
                  m, fury::sample_character_animation(m.rig, s), out);
              fail = audit(triangles(out, h, groups, &m.bind_mesh));
              require(fail.empty(), "attention yaw=" + std::to_string(yaw) +
                                        " pitch=" + std::to_string(pitch) +
                                        " " + fail);
            }
        std::cout << context << " actual pose/contact audit passed\n";
      } catch (const std::exception &e) {
        ++failures;
        std::cerr << context << ": " << e.what() << '\n';
      }
    }
  }
  return failures ? 1 : 0;
}
