#pragma once
#include "character_ghost_shirt.hpp"
#include <tuple>

namespace fury::ghost_vest_detail {
using namespace character_detail;
using LegRanges = std::array<std::pair<std::size_t, std::size_t>, 2>;

// GhostLoop's shell is authored on the actual sewn shirt, with separate cut lines,
// hanging ease and folded thickness. All fitting stays in the normalized rig.
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
        "GhostLoop fitting needs more than three influences: retained=" +
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
      throw std::runtime_error("Degenerate GhostLoop garment triangle");
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
        throw std::runtime_error("Branched GhostLoop garment shell");
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
    if (chart)
      u += 1;
    // A positive uniform angular mapping, split at the back center. Fitting
    // data stays local; no triangle spans the atlas gutter at that seam.
    q.uv =
        character_surface_uv(surface, .04f + .44f * u, .025f + .95f * q.uv.y);
    unsigned v = b.vertex(q);
    groups[layer][source].push_back(v);
    corners.emplace(key, v);
    return v;
  };
  for (auto t : p.faces) {
    float low = 2, high = -1;
    for (auto i : t) {
      low = std::min(low, p.nodes[i].uv.x);
      high = std::max(high, p.nodes[i].uv.x);
    }
    unsigned outer[3], inner[3];
    for (int k = 0; k < 3; ++k) {
      int chart = high - low > .5f && p.nodes[t[k]].uv.x < .5f ? 1 : 0;
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
    throw std::runtime_error("GhostLoop trim lacks actual vest support at " +
                             std::to_string(x) + "," + std::to_string(y));
  return result;
}
} // namespace fury::ghost_vest_detail

namespace fury {
inline void make_ghost_vest(character_detail::Sculpt &b,
                           const ghost_shirt_detail::ShirtSupport &shirt,
                           const ghost_vest_detail::LegRanges & /*trousers*/) {
  using namespace character_detail;
  using namespace ghost_vest_detail;
  const Vec3 cyan{.11f, .49f, .53f}, tape{.57f, .79f, .75f};
  const Patch substrate{shirt.nodes, shirt.triangles, {}};
  const auto normal = patch_normals(substrate);
  Patch vest;
  using Cell = std::tuple<long long, long long, long long>;
  std::map<Cell, std::vector<unsigned>> welded;
  std::vector<Vec3> source_points;
  const auto keep_node = [&](FitPoint q) {
    const Vec3 p = q.vertex.p;
    const long long cell_x = std::llround(p.x * 1e6),
                    cell_y = std::llround(p.y * 1e6),
                    cell_z = std::llround(p.z * 1e6);
    const auto key = Cell{cell_x, cell_y, cell_z};
    // Adjacent clip edges are evaluated in opposite arithmetic orders. Join
    // actual equal points across neighboring buckets, then reuse position and
    // weights exactly. A hash-bin boundary cannot create an internal wall.
    for (int dx = -1; dx <= 1; ++dx)
      for (int dy = -1; dy <= 1; ++dy)
        for (int dz = -1; dz <= 1; ++dz) {
          auto f = welded.find(Cell{cell_x + dx, cell_y + dy, cell_z + dz});
          if (f == welded.end())
            continue;
          for (unsigned id : f->second)
            if (length(source_points[id] - p) < 1e-7f)
              return id;
        }
    q.vertex.p +=
        q.normal *
        (.0034f +
         .0080f * g(std::fabs(p.x), .105f, .032f) * g(p.y, .231f, .066f) +
         .0060f * g(p.y, .062f, .038f) +
         .0014f * g(p.y, .083f, .025f) * (.8f + .2f * std::sin(p.x * 61)));
    q.vertex.c = cyan;
    q.vertex.uv = {.5f + std::atan2(p.x, -p.z) / (2 * pi),
                   smooth((p.y - .052f) / .31f)};
    unsigned id = vest.nodes.size();
    vest.nodes.push_back(q.vertex);
    vest.extrusion.push_back(q.normal);
    source_points.push_back(p);
    welded[key].push_back(id);
    return id;
  };
  const std::array<std::function<float(Vec3)>, 4> boundaries{
      {[](Vec3 p) {
         return p.y - (.081f + .008f * g(std::fabs(p.x), .113f, .020f) +
                       .008f * g(p.x, 0, .018f) * smooth(p.z / .04f));
       },
       [](Vec3 p) {
         float front = smooth((p.z - .008f) / .05f);
         return .350f - .083f * front + 1.35f * std::fabs(p.x) * front - p.y;
       },
       [](Vec3 p) { return p.z > 0 ? std::fabs(p.x) - .0034f : 1.f; },
       // The loose work vest has a larger opening than the sewn shirt sleeve.
       // Removing this armpit crescent keeps the turned edge outside the shirt
       // fold at full arm swing instead of binding both layers to one opening.
       [](Vec3 p) {
         return sq((p.y - .268f) / .075f) + sq((p.z - .003f) / .049f) - 1.f +
                3.f * smooth((.090f - std::fabs(p.x)) / .018f);
       }}};
  for (auto t : substrate.faces) {
    std::vector<FitPoint> polygon;
    for (auto i : t)
      polygon.push_back({substrate.nodes[i], normal[i]});
    for (const auto &boundary : boundaries) {
      std::vector<FitPoint> clipped;
      for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto &a = polygon[i], &c = polygon[(i + 1) % polygon.size()];
        float da = boundary(a.vertex.p), dc = boundary(c.vertex.p);
        if (da >= 0)
          clipped.push_back(a);
        if ((da >= 0) != (dc >= 0)) {
          // Solve the authored curved cut on the actual source edge. Merely
          // interpolating its signed values left a sawtooth shoulder outline.
          float lo = 0, hi = 1;
          for (int iteration = 0; iteration < 20; ++iteration) {
            float mid = (lo + hi) * .5f;
            float dm = boundary(a.vertex.p * (1 - mid) + c.vertex.p * mid);
            if ((dm >= 0) == (da >= 0))
              lo = mid;
            else
              hi = mid;
          }
          clipped.push_back(lerp_fit(a, c, (lo + hi) * .5f));
        }
      }
      polygon = std::move(clipped);
      if (polygon.size() < 3)
        break;
    }
    if (polygon.size() < 3)
      continue;
    unsigned first = keep_node(polygon[0]);
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
      unsigned c = keep_node(polygon[i]), d = keep_node(polygon[i + 1]);
      if (first != c && first != d && c != d)
        vest.faces.push_back({first, c, d});
    }
  }
  emit_shell(b, vest, .0008f, S::Jacket);
  // The horizontal identification band reuses complete vest facets between
  // its two sewn source rows. Identical influences and facet correspondence
  // avoid nonlinear skinning drift at a coarse Far clipped strip.
  std::map<std::pair<unsigned,int>,unsigned> tape_vertices;
  std::map<unsigned,std::vector<unsigned>> tape_seams;
  for(const auto face:vest.faces) {
    bool band=true;
    for(const unsigned i:face)band &= source_points[i].y>=.171f-1e-6f && source_points[i].y<=.185f+1e-6f;
    if(!band)continue;
    float low=2,high=-1;for(const unsigned i:face){low=std::min(low,vest.nodes[i].uv.x);high=std::max(high,vest.nodes[i].uv.x);}
    unsigned out[3];
    for(int k=0;k<3;++k) {
      const unsigned source=face[k];const int chart=high-low>.5f&&vest.nodes[source].uv.x<.5f?1:0;
      const auto key=std::make_pair(source,chart);auto found=tape_vertices.find(key);
      if(found==tape_vertices.end()) {
        P p=vest.nodes[source];p.p+=vest.extrusion[source]*.00075f;p.c=tape;
        p.uv=character_surface_uv(S::Shirt,.04f+.44f*(p.uv.x+chart),(source_points[source].y-.171f)/.014f);
        const auto vertex=b.vertex(p);tape_seams[source].push_back(vertex);
        found=tape_vertices.emplace(key,vertex).first;
      }
      out[k]=found->second;
    }
    b.tri(out[0],out[1],out[2]);
  }
  for(auto& seam:tape_seams)if(seam.second.size()>1)b.normal_groups.push_back(std::move(seam.second));
  // Narrow front binding follows the V and the parted lower placket.
  for (float side : {-1.f, 1.f}) {
    const auto begin = b.m.bind_mesh.indices.size();
    b.grid(
        1, b.near ? 18 : 7,
        [&](float u, float v) {
          float y = lerp(.098f, .265f, v), x = side * (.0060f + .0016f * u);
          P p = surface_sample(vest, x, y, true, .00055f);
          p.c = cyan * .64f;
          p.uv = character_surface_uv(S::Jacket, u, v);
          return p;
        },
        false, false);
    if (side > 0)
      for (std::size_t i = begin; i < b.m.bind_mesh.indices.size(); i += 3)
        std::swap(b.m.bind_mesh.indices[i + 1], b.m.bind_mesh.indices[i + 2]);
  }
}
} // namespace fury
