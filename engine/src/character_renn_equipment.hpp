#pragma once
#include "character_renn_shirt.hpp"
#include "fury/generated/renn_anatomy.hpp"
#include <map>
#include <tuple>
namespace fury::renn_equipment_detail {
using namespace character_detail;
// Original three-influence interpolation, shared by each mounted layer and
// the actual supporting source triangle rather than its analytic approximation.
inline CharacterInfluence blend(const std::array<CharacterInfluence, 3> &values,
                                const std::array<float, 3> &bary) {
  std::array<float, character_bone_count> weights{};
  for (int k = 0; k < 3; ++k) {
    const auto w = values[k];
    weights[w.first] += bary[k] * w.first_weight;
    weights[w.second] += bary[k] * (1 - w.first_weight - w.third_weight);
    weights[w.third] += bary[k] * w.third_weight;
  }
  std::array<unsigned, character_bone_count> order{};
  for (unsigned i = 0; i < order.size(); ++i)
    order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](unsigned a, unsigned c) {
    return weights[a] > weights[c];
  });
  const float sum = weights[order[0]] + weights[order[1]] + weights[order[2]];
  const float first = std::clamp(weights[order[0]] / sum, 0.f, 1.f),
              third = std::clamp(weights[order[2]] / sum, 0.f, 1 - first);
  return {std::uint8_t(order[0]), std::uint8_t(order[1]), first,
          std::uint8_t(order[2]), third};
}
inline P support(const Sculpt &b, RennUniformSupport range, Vec3 wanted,
                 Vec3 origin, float offset) {
  const Vec3 direction = normalize(wanted - origin);
  float farthest = -1;
  P result{};
  for (std::size_t t = range.begin; t < range.end; t += 3) {
    const auto &mesh = b.m.bind_mesh;
    auto ia = mesh.indices[t], ib = mesh.indices[t + 1],
         ic = mesh.indices[t + 2];
    Vec3 a = mesh.vertices[ia].position * (1 / b.h),
         e = mesh.vertices[ib].position * (1 / b.h) - a,
         f = mesh.vertices[ic].position * (1 / b.h) - a;
    Vec3 p = cross(direction, f);
    float det = dot(e, p);
    if (std::fabs(det) < 1e-12f)
      continue;
    Vec3 q = origin - a;
    float u = dot(q, p) / det;
    Vec3 c = cross(q, e);
    float v = dot(direction, c) / det, d = dot(f, c) / det;
    if (u < -1e-5f || v < -1e-5f || u + v > 1.00001f || d <= farthest)
      continue;
    farthest = d;
    result = {
        origin + direction * (d + offset),
        {},
        blend({b.m.influences[ia], b.m.influences[ib], b.m.influences[ic]},
              {1 - u - v, u, v}),
        {}};
  }
  if (farthest < 0)
    throw std::runtime_error(
        "Renn equipment has no underlying garment support at " +
        std::to_string(wanted.x) + "," + std::to_string(wanted.y) + "," +
        std::to_string(wanted.z));
  return result;
}
struct Shell {
  std::vector<P> nodes;
  std::vector<std::array<unsigned, 3>> faces;
  std::vector<Vec3> lining_directions;
  void quad(unsigned a, unsigned b, unsigned c, unsigned d) {
    faces.push_back({a, b, c});
    faces.push_back({a, c, d});
  }
  void emit(Sculpt &b, float thickness) const {
    // Only nodes incident to a face are emitted. Texture corners retain exact
    // physical positions/weights; cut edges receive their own crease normals.
    std::map<unsigned, unsigned> active;
    std::map<std::pair<unsigned, unsigned>, unsigned> edge_count;
    std::map<std::pair<unsigned, unsigned>, std::pair<unsigned, unsigned>>
        directed;
    std::vector<Vec3> normals(nodes.size());
    for (auto f : faces) {
      Vec3 n =
          cross(nodes[f[1]].p - nodes[f[0]].p, nodes[f[2]].p - nodes[f[0]].p);
      if (dot(n, n) < 1e-19f)
        throw std::runtime_error("Degenerate Renn vest");
      for (auto i : f) {
        normals[i] += n;
        active.emplace(i, 0);
      }
      for (int k = 0; k < 3; ++k) {
        auto a = f[k], c = f[(k + 1) % 3];
        auto key = std::minmax(a, c);
        if (++edge_count[key] > 2)
          throw std::runtime_error("Nonmanifold Renn vest");
        directed[key] = {a, c};
      }
    }
    std::map<unsigned, unsigned> inner;
    for (auto &entry : active) {
      auto id = entry.first;
      P p = nodes[id];
      entry.second = b.vertex(p);
      const float l = length(normals[id]);
      if (l < 1e-10f)
        throw std::runtime_error("Singular Renn vest normal");
      normals[id] = normals[id] * (1 / l);
      if (lining_directions.size() == nodes.size())
        normals[id] = lining_directions[id];
      p.p -= normals[id] * thickness;
      p.c = p.c * .95f;
      inner[id] = b.vertex(p);
    }
    for (auto f : faces) {
      b.tri(active.at(f[0]), active.at(f[1]), active.at(f[2]));
      b.tri(inner.at(f[0]), inner.at(f[2]), inner.at(f[1]));
    }
    for (auto e : edge_count)
      if (e.second == 1) {
        auto edge = directed.at(e.first);
        P p = nodes[edge.first], q = nodes[edge.second], ip = p, iq = q;
        ip.p -= normals[edge.first] * thickness;
        iq.p -= normals[edge.second] * thickness;
        p.c = p.c * .78f;
        q.c = q.c * .78f;
        ip.c = ip.c * .78f;
        iq.c = iq.c * .78f;
        auto a = b.vertex(p), c = b.vertex(ip), d = b.vertex(iq),
             f = b.vertex(q);
        b.tri(a, c, d);
        b.tri(a, d, f);
      }
  }
};
inline P front_fit(const Shell &s, float x, float y, float lift) {
  float depth = -1;
  P out{};
  bool found = false;
  for (auto f : s.faces) {
    const auto &a = s.nodes[f[0]], &b = s.nodes[f[1]], &c = s.nodes[f[2]];
    Vec3 e = b.p - a.p, g = c.p - a.p;
    float det = e.x * g.y - e.y * g.x;
    if (std::fabs(det) < 1e-12f)
      continue;
    float u = ((x - a.p.x) * g.y - (y - a.p.y) * g.x) / det,
          v = (e.x * (y - a.p.y) - e.y * (x - a.p.x)) / det;
    if (u < -1e-5f || v < -1e-5f || u + v > 1.00001f)
      continue;
    float z = a.p.z + u * e.z + v * g.z;
    if (z <= depth)
      continue;
    depth = z;
    found = true;
    out = a;
    out.p = {x, y, z + lift};
    out.w = blend({a.w, b.w, c.w}, {1 - u - v, u, v});
  }
  if (!found)
    throw std::runtime_error("Renn mounted item has no vest support at " +
                             std::to_string(x) + "," + std::to_string(y));
  return out;
}
} // namespace fury::renn_equipment_detail

namespace fury {
inline void
make_renn_cap(character_detail::Sculpt &b,
              const std::function<float(float, float, float)> &radius) {
  using namespace character_detail;
  const Vec3 cloth{.10f, .155f, .21f}, hair = b.m.appearance.hair;
  auto center = [](float y) {
    return lerp(renn_anatomy::head_center.z, renn_anatomy::skin_crown_center.z,
                smooth((y - .451f) / .040f));
  };
  auto line = [](float a) {
    return .445f + .017f * std::max(0.f, std::sin(a)) -
           .033f * std::max(0.f, -std::sin(a)) +
           .002f * std::pow(std::fabs(std::cos(a)), 8.f) +
           .0005f * std::sin(11 * a + .2f);
  };
  auto hairpoint = [&](float a, float v) {
    float t = std::sin(v * pi * .5f), sy = lerp(line(a), .4909f, t),
          y = sy + .0024f * t;
    float cz = center(sy), r = radius(sy, a, cz) + (b.near ? .0016f : .0031f);
    return Vec3{r * std::cos(a), y, cz + r * std::sin(a)};
  };
  const int hc = b.near ? 48 : 24, hr = b.near ? 12 : 6;
  auto first = unsigned(b.m.bind_mesh.vertices.size());
  b.grid(
      hc, hr,
      [&](float u, float v) {
        float a = 2 * pi * u;
        Vec3 color = hair * (.65f + .22f * smooth(v / .12f)) +
                     b.m.appearance.skin * (.13f * (1 - smooth(v / .09f)));
        return P{hairpoint(a, v), color, wt(B::Head),
                 character_surface_uv(S::HairCrop, u, v)};
      },
      true, false);
  auto hp = b.vertex({{0, .4934f, center(.491f)},
                      hair * .87f,
                      wt(B::Head),
                      character_surface_uv(S::HairCrop, .5f, 1)});
  for (int i = 0; i < hc; ++i)
    b.tri(first + hr * (hc + 1) + i, hp, first + hr * (hc + 1) + i + 1);
  // Sparse short taper ribbons soften the nape and sideburn edge; the opaque
  // scalp below them supplies continuous coverage in both distance levels.
  for (int i = 0, n = b.near ? 48 : 16; i < n; ++i) {
    float a = 2 * pi * (i + .35f) / n;
    if (std::sin(a) > .5f)
      continue;
    b.grid(
        2, b.near ? 4 : 1,
        [&](float u, float v) {
          float aa = a + (u - .5f) * .07f, t = lerp(.13f, -.025f, v);
          Vec3 p = hairpoint(aa, std::max(0.f, t));
          p += Vec3{std::cos(aa), 0, std::sin(aa)} * .00025f;
          if (t < 0)
            p.y += t * .055f;
          return P{p, hair * .8f, wt(B::Head),
                   character_surface_uv(S::HairShortStrands, 1 - u, v)};
        },
        false, false);
  }
  auto crown = [&](float a, float v) {
    float t = std::sin(v * pi * .5f), sy = lerp(.450f, .4909f, t),
          y = lerp(.450f, .5f, t), cz = center(sy);
    float r = radius(sy, a, cz) + (b.near ? .0044f : .0082f);
    r += .0008f * std::cos(6 * a) * std::sin(pi * v);
    return Vec3{r * std::cos(a), y, cz + r * std::sin(a)};
  };
  const int cc = b.near ? 24 : 12, cr = b.near ? 8 : 4;
  first = unsigned(b.m.bind_mesh.vertices.size());
  const auto crown_triangles_begin = b.m.bind_mesh.indices.size();
  b.grid(
      cc, cr,
      [&](float u, float v) {
        return P{crown(2 * pi * u, v), cloth, wt(B::Head),
                 character_surface_uv(S::Shirt, u, v)};
      },
      true, false);
  auto pole = b.vertex({{0, .5f, center(.491f)},
                        cloth,
                        wt(B::Head),
                        character_surface_uv(S::Shirt, .5f, 1)});
  for (int i = 0; i < cc; ++i)
    b.tri(first + cr * (cc + 1) + i, pole, first + cr * (cc + 1) + i + 1);
  // Complete the cap's inner crown and close the root. The old turned band
  // alone left the crown as a single open sheet. Use the actual triangulated
  // shell's normals, retaining the source head clearance on every side.
  const auto crown_triangles_end = b.m.bind_mesh.indices.size();
  const unsigned crown_vertices =
      unsigned(b.m.bind_mesh.vertices.size()) - first;
  std::vector<Vec3> crown_normals(crown_vertices);
  for (auto t = crown_triangles_begin; t < crown_triangles_end; t += 3) {
    const auto a = b.m.bind_mesh.indices[t], c = b.m.bind_mesh.indices[t + 1],
               d = b.m.bind_mesh.indices[t + 2];
    const auto &va = b.m.bind_mesh.vertices[a], &vc = b.m.bind_mesh.vertices[c],
               &vd = b.m.bind_mesh.vertices[d];
    const Vec3 normal =
        cross(vc.position - va.position, vd.position - va.position);
    crown_normals[a - first] += normal;
    crown_normals[c - first] += normal;
    crown_normals[d - first] += normal;
  }
  for (int row = 0; row <= cr; ++row) {
    const unsigned a = row * (cc + 1), c = a + cc;
    const Vec3 sum = crown_normals[a] + crown_normals[c];
    crown_normals[a] = crown_normals[c] = sum;
  }
  const unsigned inside = unsigned(b.m.bind_mesh.vertices.size());
  for (unsigned i = 0; i < crown_vertices; ++i) {
    const auto original = b.m.bind_mesh.vertices[first + i];
    const Vec3 normal = normalize(crown_normals[i]);
    b.vertex({original.position * (1 / b.h) - normal * .0012f, cloth * .92f,
              wt(B::Head), original.uv});
  }
  for (auto t = crown_triangles_begin; t < crown_triangles_end; t += 3)
    b.tri(inside + b.m.bind_mesh.indices[t] - first,
          inside + b.m.bind_mesh.indices[t + 2] - first,
          inside + b.m.bind_mesh.indices[t + 1] - first);
  for (int row = 0; row <= cr; ++row)
    b.smooth_seams.push_back({inside + unsigned(row * (cc + 1)),
                              inside + unsigned(row * (cc + 1) + cc)});
  auto cap_corner = [&](unsigned id) {
    const auto v = b.m.bind_mesh.vertices[id];
    const auto copy = b.vertex({{}, cloth * .64f, wt(B::Head), v.uv});
    b.m.bind_mesh.vertices[copy].position = v.position;
    return copy;
  };
  for (int i = 0; i < cc; ++i) {
    const auto a = cap_corner(first + i), c = cap_corner(inside + i),
               d = cap_corner(inside + i + 1), e = cap_corner(first + i + 1);
    b.tri(a, d, c);
    b.tri(a, e, d);
  }
  // Circumferential woven band and actual turned lower edge.
  b.grid(
      cc, 4,
      [&](float u, float v) {
        float a = 2 * pi * u;
        int row = std::min(4, int(std::lround(v * 4)));
        const float dy[]{-.003f, .005f, .005f, -.003f, -.003f},
            dr[]{.0015f, .0015f, .0002f, .0002f, .0015f};
        Vec3 p = crown(a, std::asin(std::max(0.f, dy[row]) / .050f) * 2 / pi);
        p.y = .450f + dy[row];
        p += Vec3{std::cos(a), 0, std::sin(a)} * dr[row];
        return P{p, cloth * .78f, wt(B::Head),
                 character_surface_uv(S::Shirt, u, v)};
      },
      true, false);
  // Compact curved peak. A finite 1.9mm-normalized thickness is maintained at
  // every border, including the tips; it is never a zero-area double sheet.
  const int bc = b.near ? 24 : 12, br = b.near ? 4 : 2;
  std::vector<unsigned> upper((bc + 1) * (br + 1)), lower = upper;
  for (int face = 0; face < 2; ++face)
    for (int j = 0; j <= br; ++j)
      for (int i = 0; i <= bc; ++i) {
        float u = float(i) / bc, v = float(j) / br,
              a = pi * .5f + lerp(-1.23f, 1.23f, u);
        Vec3 p = crown(a, 0);
        float extension =
            .003f + .034f * std::pow(std::max(0.f, std::sin(pi * u)), .7f);
        p.z += extension * v;
        p.x += .004f * std::cos(a) * v;
        p.y += .0010f - .0060f * v * v - .0028f * sq(std::cos(a)) * v -
               (face ? .0019f : 0.f);
        auto id = b.vertex({p, cloth * (face ? .64f : .91f), wt(B::Head),
                            character_surface_uv(S::Leather, u, v)});
        (face ? lower : upper)[j * (bc + 1) + i] = id;
      }
  for (int j = 0; j < br; ++j)
    for (int i = 0; i < bc; ++i) {
      unsigned q = j * (bc + 1) + i, r = q + bc + 1;
      b.tri(upper[q], upper[r + 1], upper[r]);
      b.tri(upper[q], upper[q + 1], upper[r + 1]);
      b.tri(lower[q], lower[r], lower[r + 1]);
      b.tri(lower[q], lower[r + 1], lower[q + 1]);
    }
  auto copy = [&](unsigned i) {
    const auto v = b.m.bind_mesh.vertices[i];
    const auto out = b.vertex({{}, v.color, b.m.influences[i], v.uv});
    b.m.bind_mesh.vertices[out].position = v.position;
    return out;
  };
  auto edge = [&](unsigned a, unsigned c) {
    auto pa = copy(upper[a]), ia = copy(lower[a]), ic = copy(lower[c]),
         pc = copy(upper[c]);
    b.tri(pa, ia, ic);
    b.tri(pa, ic, pc);
  };
  for (int i = 0; i < bc; ++i) {
    edge(i, i + 1);
    edge(br * (bc + 1) + i + 1, br * (bc + 1) + i);
  }
  for (int j = 0; j < br; ++j) {
    edge((j + 1) * (bc + 1), j * (bc + 1));
    edge(j * (bc + 1) + bc, (j + 1) * (bc + 1) + bc);
  }
  // Six restrained crown seams clarify the sewn panel construction.
  if (b.near)
    for (int k = 0; k < 6; ++k) {
      std::vector<Vec3> p;
      float a = 2 * pi * k / 6;
      for (int j = 1; j < 6; ++j) {
        auto q = crown(a, float(j) / 6);
        q += Vec3{std::cos(a), .25f, std::sin(a)} * .0003f;
        p.push_back(q);
      }
      b.curve(p, .00024f, cloth * .87f, S::Shirt, B::Head, 4, .5f);
    }
  Vec3 badge = crown(pi * .5f, .18f) + Vec3{0, 0, .0016f};
  b.ellipsoid(badge, {.0064f, .0068f, .0012f}, {.63f, .70f, .66f}, S::Metal,
              B::Head, b.near ? 16 : 6, b.near ? 4 : 3);
}
inline void make_renn_vest(character_detail::Sculpt &b,
                           RennUniformSupport cloth) {
  using namespace character_detail;
  using namespace renn_equipment_detail;
  Shell shell;
  const Vec3 color{.08f, .12f, .165f};
  // Original garment-on-garment construction: start from the sewn body/yoke
  // triangles, widen only their normal envelope, then trim a deeper neckline
  // and smoothly elliptical armholes. Shared cut-edge ids preserve all seams.
  struct Anchor {
    Vec3 p, n;
    CharacterInfluence w;
  };
  std::vector<Anchor> anchor;
  std::map<std::tuple<float, float, float>, unsigned> canonical;
  std::vector<std::array<unsigned, 3>> source;
  for (std::size_t t = cloth.begin; t < cloth.end; t += 3) {
    std::array<unsigned, 3> f{};
    for (int k = 0; k < 3; ++k) {
      unsigned id = b.m.bind_mesh.indices[t + k];
      Vec3 p = b.m.bind_mesh.vertices[id].position * (1 / b.h);
      auto key = std::make_tuple(p.x, p.y, p.z);
      auto found = canonical.find(key);
      if (found == canonical.end()) {
        unsigned i = anchor.size();
        anchor.push_back({p, {}, b.m.influences[id]});
        found = canonical.emplace(key, i).first;
      }
      f[k] = found->second;
    }
    Vec3 n =
        cross(anchor[f[1]].p - anchor[f[0]].p, anchor[f[2]].p - anchor[f[0]].p);
    for (auto i : f)
      anchor[i].n += n;
    source.push_back(f);
  }
  for (auto &a : anchor) {
    a.n = normalize(a.n);
    shell.nodes.push_back({a.p + a.n * lerp(b.near ? .0103f : .0110f, .0050f,
                                            smooth((a.p.y - .285f) / .060f)),
                           color,
                           a.w,
                           {}});
  }
  auto field = [](Vec3 p, int plane) {
    if (plane == 0)
      return p.y - .093f;
    if (plane == 1)
      return .341f - .030f * std::max(0.f, p.z) / .08f - p.y;
    const float side = plane == 2 ? -1.f : 1.f;
    return sq((p.x - side * .133f) / .071f) + sq((p.y - .264f) / .080f) +
           sq((p.z - .002f) / .064f) - 1.f;
  };
  std::map<std::tuple<int, unsigned, unsigned>, unsigned> intersections;
  auto intersection = [&](unsigned a, unsigned c, int plane) {
    if (c < a)
      std::swap(a, c);
    auto key = std::make_tuple(plane, a, c);
    auto old = intersections.find(key);
    if (old != intersections.end())
      return old->second;
    float lo = 0, hi = 1, va = field(anchor[a].p, plane);
    // Every crossing is solved against the actual nonlinear surface, not a
    // linearized signed-distance approximation that leaves serrated edges.
    for (int it = 0; it < 32; ++it) {
      float mid = (lo + hi) * .5f;
      float vm = field(anchor[a].p * (1 - mid) + anchor[c].p * mid, plane);
      if ((vm >= 0) == (va >= 0))
        lo = mid;
      else
        hi = mid;
    }
    float t = (lo + hi) * .5f;
    if (t < 1e-7f)
      return a;
    if (t > 1 - 1e-7f)
      return c;
    const unsigned out = anchor.size();
    auto w = blend({anchor[a].w, anchor[c].w, anchor[c].w}, {1 - t, t, 0});
    anchor.push_back({anchor[a].p * (1 - t) + anchor[c].p * t,
                      normalize(anchor[a].n * (1 - t) + anchor[c].n * t), w});
    P p = shell.nodes[a];
    p.p = shell.nodes[a].p * (1 - t) + shell.nodes[c].p * t;
    p.w = w;
    shell.nodes.push_back(p);
    intersections.emplace(key, out);
    return out;
  };
  for (auto f : source) {
    std::vector<unsigned> polygon{f[0], f[1], f[2]};
    for (int plane = 0; plane < 4 && !polygon.empty(); ++plane) {
      std::vector<unsigned> clipped;
      for (unsigned k = 0; k < polygon.size(); ++k) {
        unsigned a = polygon[k], c = polygon[(k + 1) % polygon.size()];
        bool aa = field(anchor[a].p, plane) >= 0,
             cc = field(anchor[c].p, plane) >= 0;
        if (aa)
          clipped.push_back(a);
        if (aa != cc)
          clipped.push_back(intersection(a, c, plane));
      }
      polygon = std::move(clipped);
    }
    for (unsigned k = 1; k + 1 < polygon.size(); ++k) {
      auto a = polygon[0], c = polygon[k], d = polygon[k + 1];
      if (dot(cross(shell.nodes[c].p - shell.nodes[a].p,
                    shell.nodes[d].p - shell.nodes[a].p),
              cross(shell.nodes[c].p - shell.nodes[a].p,
                    shell.nodes[d].p - shell.nodes[a].p)) > 1e-19f)
        shell.faces.push_back({a, c, d});
    }
  }
  for (auto a : anchor)
    shell.lining_directions.push_back(a.n);
  for (auto &p : shell.nodes)
    p.uv = character_surface_uv(S::Jacket, .5f + p.p.x * b.h / .75f,
                                (p.p.y - .09f) * b.h / .70f);
  shell.emit(b, .0024f);
  // Quiet front closure and shoulder webbing follow the actual shell, including
  // its torso skin blend. They are attachment details, not luminous strips.
  b.grid(
      2, b.near ? 6 : 4,
      [&](float u, float v) {
        float y = lerp(.101f, .312f, v), x = lerp(-.004f, .004f, u);
        P p = front_fit(shell, x, y, .0013f);
        p.c = color * .67f;
        p.uv = character_surface_uv(S::Jacket, u, v);
        return p;
      },
      false, false);
  for (float side : {-1.f, 1.f})
    b.grid(
        2, b.near ? 6 : 4,
        [&](float u, float v) {
          float y = lerp(.110f, .277f, v),
                x = side * (.068f + .0018f * (u - .5f));
          P p = front_fit(shell, x, y, .0008f);
          p.c = color * .9f;
          p.uv = character_surface_uv(S::Jacket, u, v);
          return p;
        },
        false, false);
  auto mounted = [&](float x, float y, float hx, float hy, float base,
                     float thickness, Vec3 tint, S surface) {
    const int count = b.near ? 24 : 8;
    auto start = unsigned(b.m.bind_mesh.vertices.size());
    auto ring = [&](float u, float v) {
      float a = u == 1 ? 0 : -2 * pi * u;
      float xx = x + hx * std::copysign(std::pow(std::fabs(std::cos(a)), .42f),
                                        std::cos(a)),
            yy = y + hy * std::copysign(std::pow(std::fabs(std::sin(a)), .42f),
                                        std::sin(a));
      P p = front_fit(shell, xx, yy, base + thickness * v);
      p.c = tint;
      p.uv = character_surface_uv(surface, u, v);
      return p;
    };
    b.grid(count, 2, ring, true, false);
    for (int end = 0; end < 2; ++end) {
      P middle = front_fit(shell, x, y, base + thickness * end);
      middle.c = tint;
      middle.uv = character_surface_uv(surface, .5f, .5f);
      auto center = b.vertex(middle);
      std::vector<unsigned> ids;
      for (int i = 0; i <= count; ++i)
        ids.push_back(b.vertex(ring(float(i) / count, float(end))));
      for (int i = 0; i < count; ++i) {
        if (end)
          b.tri(center, ids[i + 1], ids[i]);
        else
          b.tri(center, ids[i], ids[i + 1]);
      }
    }
    (void)start;
  };
  // The existing badge/radio stay on the same left/right sides. No new rank,
  // employer logo or biographical insignia is invented.
  mounted(-.049f, .253f, .012f, .016f, .0012f, .0018f, {.055f, .065f, .075f},
          S::Leather);
  mounted(-.049f, .253f, .0098f, .0135f, .0031f, .0011f, {.64f, .68f, .53f},
          S::Metal);
  mounted(.063f, .247f, .0125f, .026f, .0045f, .014f, {.055f, .065f, .075f},
          S::Rubber);
  if (b.near)
    for (int k = 0; k < 4; ++k) {
      float y = .246f + .004f * k;
      b.grid(
          1, 1,
          [&](float u, float v) {
            P p =
                front_fit(shell, lerp(.056f, .070f, u), y + .0006f * v, .0189f);
            p.c = {.03f, .035f, .04f};
            p.uv = character_surface_uv(S::Rubber, u, v);
            return p;
          },
          false, false);
    }
  // Stub antenna shares the supported radio mount's skinning, rather than
  // receiving a separate moving transform.
  P radio_anchor = front_fit(shell, .071f, .270f, .0113f);
  std::vector<Vec3> wire;
  for (int i = 0; i <= 8; ++i) {
    float t = float(i) / 8;
    wire.push_back(radio_anchor.p + Vec3{-.002f * t, .033f * t, .0015f * t});
  }
  b.tube(
      wire, std::vector<float>(wire.size(), .00125f), 1, {.055f, .065f, .075f},
      S::Rubber, [&](float) { return radio_anchor.w; }, b.near ? 8 : 4);
  // Belt pouch has a rounded sewn gusset and a turned flap. Its inward face
  // intentionally mounts to the belt; its outside is clear of the relaxed hand.
  const Vec3 dark{.055f, .065f, .075f};
  b.grid(b.near ? 24 : 8, b.near ? 10 : 3, [&](float u, float v) {
    float a = 2 * pi * u, bulge = .90f + .10f * std::sin(pi * v);
    return P{{-.109f + .018f * std::cos(a) * bulge, lerp(-.021f, .061f, v),
              .008f + .024f * std::sin(a) * bulge},
             dark,
             wt(B::Pelvis),
             character_surface_uv(S::Leather, u, v)};
  });
  b.grid(b.near ? 16 : 8, b.near ? 8 : 2, [&](float u, float v) {
    float a = 2 * pi * u;
    return P{{-.109f + .0178f * std::cos(a), lerp(.039f, .064f, v),
              .0318f + .0020f * std::sin(a)},
             dark * .88f,
             wt(B::Pelvis),
             character_surface_uv(S::Leather, u, v)};
  });
  b.ellipsoid({-.109f, .047f, .0342f}, {.0025f, .0025f, .0007f},
              {.15f, .17f, .17f}, S::Metal, B::Pelvis, 8, 4);
}
} // namespace fury
