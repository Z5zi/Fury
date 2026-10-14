#pragma once
#include "character_sparrow_shirt.hpp"
#include <tuple>

namespace fury::sparrow_pack_detail {
using namespace character_detail;
using LegRanges = std::array<std::pair<std::size_t, std::size_t>, 2>;

// Sparrow's shell is authored on the actual sewn shirt, with separate cut
// lines, hanging ease and folded thickness. All fitting stays in the normalized
// rig.
struct Patch {
  std::vector<P> nodes;
  std::vector<std::array<unsigned, 3>> faces;
  std::vector<Vec3> extrusion;
};
inline CharacterInfluence mix_weights(const CharacterInfluence &a,
                                      const CharacterInfluence &c, float t) {
  std::array<float, character_bone_count> values{};
  auto add = [&](CharacterInfluence w, float amount) {
    values[w.first] += w.first_weight * amount;
    values[w.second] += (1 - w.first_weight - w.third_weight) * amount;
    values[w.third] += w.third_weight * amount;
  };
  add(a, 1 - t);
  add(c, t);
  std::array<unsigned, character_bone_count> order{};
  for (unsigned i = 0; i < order.size(); ++i)
    order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](unsigned i, unsigned j) {
    return values[i] > values[j];
  });
  float total = values[order[0]] + values[order[1]] + values[order[2]];
  if (total < .99999f)
    throw std::runtime_error(
        "Sparrow fitting needs more than three influences: retained=" +
        std::to_string(total));
  const float first = values[order[0]] / total;
  const float third = std::min(1 - first, values[order[2]] / total);
  return {std::uint8_t(order[0]), std::uint8_t(order[1]), first,
          std::uint8_t(order[2]), third};
}
inline std::vector<Vec3> patch_normals(const Patch &p) {
  std::vector<Vec3> n(p.nodes.size());
  for (auto t : p.faces) {
    auto normal = cross(p.nodes[t[1]].p - p.nodes[t[0]].p,
                        p.nodes[t[2]].p - p.nodes[t[0]].p);
    if (dot(normal, normal) <= 1e-20f)
      throw std::runtime_error(
          "Degenerate Sparrow garment triangle " + std::to_string(t[0]) + "," +
          std::to_string(t[1]) + "," + std::to_string(t[2]) + " at " +
          std::to_string(p.nodes[t[0]].p.x) + "," +
          std::to_string(p.nodes[t[0]].p.y) + "," +
          std::to_string(p.nodes[t[0]].p.z) + ";" +
          std::to_string(p.nodes[t[1]].p.x) + "," +
          std::to_string(p.nodes[t[1]].p.y) + "," +
          std::to_string(p.nodes[t[1]].p.z) + ";" +
          std::to_string(p.nodes[t[2]].p.x) + "," +
          std::to_string(p.nodes[t[2]].p.y) + "," +
          std::to_string(p.nodes[t[2]].p.z));
    for (auto v : t)
      n[v] += normal;
  }
  for (auto &v : n)
    if (dot(v, v) > 1e-20f)
      v = normalize(v);
  return n;
}
inline void emit_shell(Sculpt &b, const Patch &p, float thickness, S surface) {
  const auto normals = patch_normals(p);
  const auto &extrusion = p.extrusion.empty() ? normals : p.extrusion;
  std::map<std::pair<unsigned, unsigned>, unsigned> edge_count;
  std::map<std::pair<unsigned, unsigned>, std::pair<unsigned, unsigned>>
      oriented;
  for (auto t : p.faces)
    for (int k = 0; k < 3; ++k) {
      const unsigned a = t[k], c = t[(k + 1) % 3];
      const auto edge = std::minmax(a, c);
      if (++edge_count[edge] > 2)
        throw std::runtime_error("Branched Sparrow garment shell");
      oriented[edge] = {a, c};
    }
  std::array<std::vector<std::vector<unsigned>>, 2> groups;
  for (auto &v : groups)
    v.resize(p.nodes.size());
  std::map<std::tuple<unsigned, int, int>, unsigned> corners;
  auto emit = [&](unsigned source, int layer, int chart) {
    const auto key = std::make_tuple(source, layer, chart);
    auto found = corners.find(key);
    if (found != corners.end())
      return found->second;
    P q = p.nodes[source];
    if (layer) {
      q.p -= extrusion[source] * thickness;
      q.c = q.c * .93f;
    }
    float u = q.uv.x;
    (void)chart;
    // A positive rectangular panel mapping preserves local grain scale; no
    // triangle crosses a material tile or invents an angular wrap seam.
    q.uv =
        character_surface_uv(surface, .04f + .90f * u, .025f + .95f * q.uv.y);
    unsigned v = b.vertex(q);
    groups[layer][source].push_back(v);
    corners.emplace(key, v);
    return v;
  };
  for (auto t : p.faces) {
    unsigned outer[3], inner[3];
    for (int k = 0; k < 3; ++k) {
      int chart = 0;
      outer[k] = emit(t[k], 0, chart);
      inner[k] = emit(t[k], 1, chart);
    }
    b.tri(outer[0], outer[1], outer[2]);
    b.tri(inner[0], inner[2], inner[1]);
  }
  for (const auto &entry : edge_count)
    if (entry.second == 1) {
      auto edge = oriented.at(entry.first);
      P a = p.nodes[edge.first], c = p.nodes[edge.second], ia = a, ic = c;
      ia.p -= extrusion[edge.first] * thickness;
      ic.p -= extrusion[edge.second] * thickness;
      for (P *q : {&a, &c, &ia, &ic}) {
        q->c = q->c * .80f;
        q->uv = character_surface_uv(surface, .1f, .1f);
      }
      const auto va = b.vertex(a), vc = b.vertex(c), vi = b.vertex(ia),
                 vj = b.vertex(ic);
      b.tri(va, vi, vj);
      b.tri(va, vj, vc);
    }
  for (auto &layer : groups)
    for (auto &group : layer)
      if (group.size() > 1)
        b.normal_groups.push_back(std::move(group));
}

struct FitPoint {
  P vertex;
  Vec3 normal;
};
inline FitPoint lerp_fit(const FitPoint &a, const FitPoint &c, float t) {
  FitPoint q = a;
  q.vertex.p = a.vertex.p * (1 - t) + c.vertex.p * t;
  q.vertex.w = mix_weights(a.vertex.w, c.vertex.w, t);
  q.normal = normalize(a.normal * (1 - t) + c.normal * t);
  return q;
}
inline P surface_sample(const Patch &patch, float x, float y, bool front,
                        float lift) {
  bool found = false;
  float best = front ? -10.f : 10.f;
  P result{};
  for (auto tri : patch.faces) {
    const P &a = patch.nodes[tri[0]], &b = patch.nodes[tri[1]],
            &c = patch.nodes[tri[2]];
    Vec3 e = b.p - a.p, f = c.p - a.p;
    float det = e.x * f.y - e.y * f.x;
    if (std::fabs(det) < 1e-12f)
      continue;
    float u = ((x - a.p.x) * f.y - (y - a.p.y) * f.x) / det,
          v = (e.x * (y - a.p.y) - e.y * (x - a.p.x)) / det;
    if (u < -1e-6f || v < -1e-6f || u + v > 1.000001f)
      continue;
    float z = a.p.z + u * e.z + v * f.z;
    if (front ? z <= best : z >= best)
      continue;
    found = true;
    best = z;
    result = a;
    result.p = {x, y, z + (front ? lift : -lift)};
    u = std::max(0.f, u);
    v = std::max(0.f, v);
    float sum = u + v;
    result.w = sum > 1e-7f ? mix_weights(a.w, mix_weights(b.w, c.w, v / sum),
                                         std::min(1.f, sum))
                           : a.w;
  }
  if (!found)
    throw std::runtime_error("Sparrow trim lacks actual garment support at " +
                             std::to_string(x) + "," + std::to_string(y));
  return result;
}
} // namespace fury::sparrow_pack_detail

namespace fury {
// Sparrow's original compact daypack. The support is the exact sewn jacket,
// including its yoke. No commercial pack mesh or pattern is used.
inline void
make_sparrow_pack(character_detail::Sculpt &b,
                  const sparrow_shirt_detail::ShirtSupport &jacket) {
  using namespace character_detail;
  using namespace sparrow_pack_detail;
  const Vec3 webbing{.052f, .061f, .061f}, pack_color{.19f, .265f, .24f};
  const Patch substrate{jacket.nodes, jacket.triangles, {}};
  const auto normals = patch_normals(substrate);
  for (float side : {-1.f, 1.f}) {
    Patch strap;
    const auto strap_center = [](float y) {
      return .0545f + .008f * smooth((y - .112f) / .19f);
    };
    const auto strap_half_width = [](float y) {
      return .007f + .005f * smooth((y - .15f) / .12f);
    };
    std::vector<Vec3> originals;
    const auto keep = [&](FitPoint q) {
      for (unsigned k = 0; k < originals.size(); ++k)
        if (length(originals[k] - q.vertex.p) < 1e-7f)
          return k;
      const Vec3 p = q.vertex.p;
      q.vertex.p += q.normal * (b.near ? .0040f : .0050f);
      q.vertex.c = webbing;
      q.vertex.uv = {
          std::clamp((side * p.x - strap_center(p.y) + strap_half_width(p.y)) /
                         (2 * strap_half_width(p.y)),
                     0.f, 1.f),
          std::clamp((p.y - .112f) / .25f, 0.f, 1.f)};
      unsigned i = strap.nodes.size();
      strap.nodes.push_back(q.vertex);
      strap.extrusion.push_back(q.normal);
      originals.push_back(p);
      return i;
    };
    const std::array<std::function<float(Vec3)>, 3> cuts{
        {[=](Vec3 p) {
           return side * p.x - strap_center(p.y) + strap_half_width(p.y);
         },
         [=](Vec3 p) {
           return strap_center(p.y) + strap_half_width(p.y) - side * p.x;
         },
         [](Vec3 p) { return p.y - .112f; }}};
    for (auto t : substrate.faces) {
      std::vector<FitPoint> poly;
      for (auto i : t)
        poly.push_back({substrate.nodes[i], normals[i]});
      for (const auto &cut : cuts) {
        std::vector<FitPoint> clipped;
        for (std::size_t i = 0; i < poly.size(); ++i) {
          const auto &a = poly[i];
          const auto &c = poly[(i + 1) % poly.size()];
          const float da = cut(a.vertex.p), dc = cut(c.vertex.p);
          if (da >= 0)
            clipped.push_back(a);
          if ((da >= 0) != (dc >= 0)) {
            float lo = 0, hi = 1;
            for (int step = 0; step < 20; ++step) {
              float mid = (lo + hi) * .5f;
              float dm = cut(a.vertex.p * (1 - mid) + c.vertex.p * mid);
              if ((dm >= 0) == (da >= 0))
                lo = mid;
              else
                hi = mid;
            }
            clipped.push_back(lerp_fit(a, c, (lo + hi) * .5f));
          }
        }
        poly = std::move(clipped);
        if (poly.size() < 3)
          break;
      }
      if (poly.size() < 3)
        continue;
      unsigned a = keep(poly[0]);
      for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
        unsigned c = keep(poly[i]), d = keep(poly[i + 1]);
        if (a != c && a != d && c != d)
          strap.faces.push_back({a, c, d});
      }
    }
    emit_shell(b, strap, .0017f, S::Leather);
  }
  // Rounded side gusset and broad rear panel. A close padded face rests above
  // the jacket; the volume follows the chest, without floating shoulder bars.
  const int columns = b.near ? 48 : 16, rows = b.near ? 8 : 4;
  auto outline = [](float a, float scale, float z) {
    const float cs = std::cos(a), sn = std::sin(a);
    return Vec3{
        .072f * std::copysign(std::pow(std::fabs(cs), .55f), cs) * scale,
        .216f +
            .096f * std::copysign(std::pow(std::fabs(sn), .55f), sn) * scale,
        z};
  };
  const auto pack_triangle_begin = b.m.bind_mesh.indices.size();
  b.grid(columns, rows, [&](float u, float v) {
    const float scale = .92f + .08f * std::sin(pi * v);
    Vec3 p =
        outline(u == 1.f ? 0.f : 2 * pi * u, scale, lerp(-.077f, -.145f, v));
    const float low = smooth((.180f - p.y) / .055f);
    p.z -= .003f * low;
    return P{p, pack_color * (1 - .055f * std::sin(pi * v)), wt(B::Chest),
             character_surface_uv(S::Jacket, u, v)};
  });
  const auto pack_triangle_end = b.m.bind_mesh.indices.size();
  Patch pack_support;
  std::map<unsigned, unsigned> pack_nodes;
  for (std::size_t t = pack_triangle_begin; t < pack_triangle_end; t += 3) {
    std::array<unsigned, 3> tri;
    for (int k = 0; k < 3; ++k) {
      unsigned source = b.m.bind_mesh.indices[t + k];
      auto found = pack_nodes.find(source);
      if (found == pack_nodes.end()) {
        const auto &v = b.m.bind_mesh.vertices[source];
        unsigned local = pack_support.nodes.size();
        pack_support.nodes.push_back(
            {v.position * (1 / b.h), v.color, b.m.influences[source], {}});
        found = pack_nodes.emplace(source, local).first;
      }
      tri[k] = found->second;
    }
    pack_support.faces.push_back(tri);
  }
  // A separate shallow rear pocket has a rolled lip and four closed edges.
  // Its backing follows the actual submitted pack triangles at both LODs.
  Patch pocket;
  const int pocket_n = b.near ? 8 : 3;
  for (int j = 0; j <= pocket_n; ++j)
    for (int i = 0; i <= pocket_n; ++i) {
      const float u = float(i) / pocket_n, v = float(j) / pocket_n;
      float x = lerp(-.048f, .048f, u) * (1 - .08f * sq(2 * v - 1));
      float y = lerp(.155f, .233f, v) + .004f * sq(2 * u - 1) * (1 - v);
      auto p =
          surface_sample(pack_support, x, y, false,
                         .0015f + .0020f * std::sin(pi * u) * smooth(v / .5f));
      p.c = pack_color * (v > .90f ? .62f : .98f);
      p.uv = {u, v};
      pocket.nodes.push_back(p);
    }
  for (int j = 0; j < pocket_n; ++j)
    for (int i = 0; i < pocket_n; ++i) {
      unsigned a = j * (pocket_n + 1) + i, c = a + pocket_n + 1;
      pocket.faces.push_back({a, c, c + 1});
      pocket.faces.push_back({a, c + 1, a + 1});
    }
  emit_shell(b, pocket, .0012f, S::Jacket);
  // Upper load-transfer webbing bridges the jacket-fitted shoulder strap into
  // the pack's sewn anchor. Only the buried end lies inside the fabric shell.
  for (float side : {-1.f, 1.f}) {
    auto start = surface_sample(substrate, side * .061f, .303f, false, .0042f);
    std::vector<Vec3> path{start.p, {side * .047f, .300f, -.086f}};
    b.tube(
        path, {.009f, .008f}, .13f, webbing, S::Leather,
        [&](float t) { return mix_weights(start.w, wt(B::Chest), smooth(t)); },
        4, {1, 0, 0});
  }
  // A padded back-facing contact panel gives the pack a softer shoulder fit.
  b.grid(b.near ? 32 : 12, 3, [&](float u, float v) {
    Vec3 p = outline(u == 1.f ? 0.f : 2 * pi * u,
                     .80f + .045f * std::sin(pi * v), lerp(-.067f, -.078f, v));
    return P{p, webbing, wt(B::Chest), character_surface_uv(S::Shirt, u, v)};
  });
  // Finite rear-panel piping, top zipper, and two distinct pull loops.
  std::vector<Vec3> piping;
  std::vector<float> radii;
  for (int i = 0; i <= columns; ++i) {
    piping.push_back(outline(2 * pi * float(i) / columns, .935f, -.146f));
    radii.push_back(.0011f);
  }
  b.tube(piping, radii, 1.f, pack_color * .64f, S::Leather,
         [](float) { return wt(B::Chest); }, b.near ? 6 : 4, {0, 0, 1});
  std::vector<Vec3> zipper;
  const int zip_count = b.near ? 24 : 10;
  for (int i = 0; i <= zip_count; ++i) {
    float a = lerp(.1f * pi, .9f * pi, float(i) / zip_count);
    zipper.push_back(outline(a, 1.001f, -.126f));
  }
  b.curve(zipper, .0012f, webbing, S::Leather, B::Chest, b.near ? 6 : 4);
  for (float side : {-1.f, 1.f}) {
    b.curve({{side * .017f, .312f, -.124f},
             {side * .018f, .301f, -.137f},
             {side * .020f, .296f, -.137f}},
            .0016f, webbing, S::Leather, B::Chest, b.near ? 6 : 4);
  }
  // A short carry handle has two sewn anchors and an open grasping aperture.
  std::vector<Vec3> handle;
  const int handle_rows = b.near ? 12 : 6;
  for (int i = 0; i <= handle_rows; ++i) {
    float v = float(i) / handle_rows;
    handle.push_back(
        {lerp(-.021f, .021f, v), .310f + .014f * std::sin(pi * v), -.093f});
  }
  b.curve(handle, .0030f, webbing, S::Leather, B::Chest, b.near ? 8 : 4, .48f);
  // Shallow front utility pocket and asymmetric ochre tab retain the original
  // scout cue. Their surface follows the actual triangle support.
  for (float side : {-1.f, 1.f}) {
    b.grid(
        b.near ? 10 : 4, b.near ? 10 : 4,
        [&](float u, float v) {
          const float x = side * lerp(.016f, .046f, side > 0 ? 1 - u : u),
                      y = lerp(.086f, .144f + .007f * u, v);
          auto p =
              surface_sample(substrate, x, y, true,
                             (b.near ? .0014f : .0025f) +
                                 .0018f * std::sin(pi * u) * std::sin(pi * v));
          p.c = b.m.appearance.shirt * .96f;
          p.uv = character_surface_uv(S::Jacket, u, v);
          return p;
        },
        false, false);
  }
  b.grid(
      b.near ? 6 : 3, b.near ? 6 : 3,
      [&](float u, float v) {
        auto p = surface_sample(substrate, lerp(.086f, .074f, u),
                                lerp(.157f, .176f, v), true, .0015f);
        p.c = b.m.appearance.accent;
        p.uv = character_surface_uv(S::Shirt, u, v);
        return p;
      },
      false, false);
  // Quiet diagonal zipper underlap. It is independently fitted at every row
  // and carries the same body influences as the supporting jacket.
  b.grid(
      4, b.near ? 36 : 16,
      [&](float u, float v) {
        float y = lerp(.071f, .329f, v),
              x = lerp(.015f, -.014f, v) + lerp(.0022f, -.0022f, u);
        auto p = surface_sample(substrate, x, y, true,
                                .0013f + .0003f * std::sin(pi * u));
        p.c = webbing;
        p.uv = character_surface_uv(S::Leather, u, v);
        return p;
      },
      false, false);
  // A compact slider and open pull loop make the zip a constructed fastening.
  // This upper placket is entirely Chest weighted; fit it to the submitted
  // jacket triangles instead of a guessed planar front.
  const float slider_y = .312f;
  const float slider_x = lerp(.015f, -.014f, (slider_y - .071f) / .258f);
  const auto slider =
      surface_sample(substrate, slider_x, slider_y, true, .0032f);
  const Vec3 hardware{.17f, .18f, .17f};
  b.ellipsoid(slider.p, {.0024f, .0040f, .0013f}, hardware, S::Metal, B::Chest,
              b.near ? 8 : 4, b.near ? 4 : 3);
  std::vector<Vec3> pull;
  const int pull_segments = b.near ? 8 : 6;
  for (int i = 0; i <= pull_segments; ++i) {
    const float angle = 2 * pi * float(i) / pull_segments;
    Vec3 q = slider.p + Vec3{.0022f * std::sin(angle),
                             -.0053f + .0050f * std::cos(angle), .0026f};
    pull.push_back(q);
  }
  b.curve(pull, .00045f, hardware * .82f, S::Metal, B::Chest, b.near ? 6 : 4,
          .8f);
}
} // namespace fury
