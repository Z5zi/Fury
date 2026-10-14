#pragma once
#include "character_vale_shirt.hpp"
#include <tuple>

namespace fury::vale_vest_detail {
using namespace character_detail;
struct Sample {
  P p;
  Vec3 n;
};
inline CharacterInfluence blend_weights(CharacterInfluence a,
                                        CharacterInfluence c, float t) {
  std::array<float, character_bone_count> w{};
  auto add = [&](CharacterInfluence q, float k) {
    w[q.first] += k * q.first_weight;
    w[q.second] += k * (1 - q.first_weight - q.third_weight);
    w[q.third] += k * q.third_weight;
  };
  add(a, 1 - t);
  add(c, t);
  std::array<unsigned, character_bone_count> order{};
  for (unsigned i = 0; i < order.size(); ++i)
    order[i] = i;
  std::stable_sort(order.begin(), order.end(),
                   [&](unsigned x, unsigned z) { return w[x] > w[z]; });
  const float total = w[order[0]] + w[order[1]] + w[order[2]],
              first = w[order[0]] / total, third = w[order[2]] / total;
  return {std::uint8_t(order[0]), std::uint8_t(order[1]), first,
          std::uint8_t(order[2]), std::min(1 - first, third)};
}
inline Sample blend(Sample a, Sample c, float t) {
  if (t < .0005f)
    return a;
  if (t > .9995f)
    return c;
  a.p.p = a.p.p * (1 - t) + c.p.p * t;
  a.p.w = blend_weights(a.p.w, c.p.w, t);
  a.n = normalize(a.n * (1 - t) + c.n * t);
  return a;
}
using Cut = std::function<float(Vec3)>;
inline std::vector<Sample> clip(std::vector<Sample> poly,
                                const std::vector<Cut> &cuts) {
  for (const auto &cut : cuts) {
    std::vector<Sample> out;
    for (unsigned i = 0; i < poly.size(); ++i) {
      const auto a = poly[i], c = poly[(i + 1) % poly.size()];
      const float da = cut(a.p.p), dc = cut(c.p.p);
      if (da >= 0)
        out.push_back(a);
      if ((da >= 0) != (dc >= 0)) {
        float low = 0, high = 1;
        for (int k = 0; k < 22; ++k) {
          const float t = (low + high) * .5f;
          const float d = cut(a.p.p * (1 - t) + c.p.p * t);
          if ((d >= 0) == (da >= 0))
            low = t;
          else
            high = t;
        }
        out.push_back(blend(a, c, (low + high) * .5f));
      }
    }
    poly = std::move(out);
    if (poly.size() < 3)
      return {};
  }
  bool changed = true;
  while (changed && poly.size() >= 3) {
    changed = false;
    for (unsigned i = 0; i < poly.size(); ++i) {
      const Vec3 a = poly[(i + poly.size() - 1) % poly.size()].p.p,
                 c = poly[i].p.p, d = poly[(i + 1) % poly.size()].p.p,
                 e = d - a;
      if (dot(e, e) > 1e-16f && dot(c - a, d - c) >= 0 &&
          dot(cross(c - a, e), cross(c - a, e)) < sq(.00010f) * dot(e, e)) {
        poly.erase(poly.begin() + i);
        changed = true;
        break;
      }
    }
  }
  return poly;
}
struct Shell {
  std::vector<Sample> points;
  std::vector<std::array<unsigned, 3>> triangles;
  std::map<std::tuple<long long, long long, long long>, std::vector<unsigned>>
      welded;
  unsigned point(Sample p) {
    const auto key = std::make_tuple(std::llround(p.p.p.x * 1e6),
                                     std::llround(p.p.p.y * 1e6),
                                     std::llround(p.p.p.z * 1e6));
    for (int x = -1; x <= 1; ++x)
      for (int y = -1; y <= 1; ++y)
        for (int z = -1; z <= 1; ++z) {
          auto q = welded.find({std::get<0>(key) + x, std::get<1>(key) + y,
                                std::get<2>(key) + z});
          if (q != welded.end())
            for (auto i : q->second)
              if (length(points[i].p.p - p.p.p) < 1e-6)
                return i;
        }
    unsigned id = points.size();
    points.push_back(p);
    welded[key].push_back(id);
    return id;
  }
  void polygon(const std::vector<Sample> &poly) {
    if (poly.size() < 3)
      return;
    const unsigned a = point(poly[0]);
    for (unsigned k = 1; k + 1 < poly.size(); ++k) {
      const unsigned c = point(poly[k]), d = point(poly[k + 1]);
      if (a == c || a == d || c == d)
        continue;
      const Vec3 n =
          cross(points[c].p.p - points[a].p.p, points[d].p.p - points[a].p.p);
      if (dot(n, n) > 1e-18f)
        triangles.push_back({a, c, d});
    }
  }
  void emit(Sculpt &b, float thickness) const {
    std::vector<unsigned> remap(points.size(), ~0u);
    unsigned n = 0;
    for (auto t : triangles)
      for (auto i : t)
        if (remap[i] == ~0u)
          remap[i] = n++;
    std::vector<unsigned> inverse(n);
    for (unsigned i = 0; i < remap.size(); ++i)
      if (remap[i] != ~0u)
        inverse[remap[i]] = i;
    const unsigned start = b.m.bind_mesh.vertices.size();
    for (auto i : inverse)
      b.vertex(points[i].p);
    for (auto i : inverse) {
      auto p = points[i];
      p.p.p -= p.n * thickness;
      p.p.c = p.p.c * .94f;
      b.vertex(p.p);
    }
    std::map<std::pair<unsigned, unsigned>, unsigned> counts;
    std::map<std::pair<unsigned, unsigned>, std::pair<unsigned, unsigned>>
        directed;
    for (auto t : triangles) {
      b.tri(start + remap[t[0]], start + remap[t[1]], start + remap[t[2]]);
      b.tri(start + n + remap[t[0]], start + n + remap[t[2]],
            start + n + remap[t[1]]);
      for (int i = 0; i < 3; ++i) {
        unsigned a = t[i], c = t[(i + 1) % 3];
        auto e = std::minmax(a, c);
        if (++counts[e] > 2)
          throw std::runtime_error("Vale garment shell branches");
        directed[e] = {a, c};
      }
    }
    for (auto edge : counts)
      if (edge.second == 1) {
        const auto e = directed.at(edge.first);
        P a = points[e.first].p, c = points[e.second].p, ia = a, ic = c;
        ia.p -= points[e.first].n * thickness;
        ic.p -= points[e.second].n * thickness;
        a.c = a.c * .79f;
        c.c = c.c * .79f;
        ia.c = ia.c * .79f;
        ic.c = ic.c * .79f;
        const unsigned q = b.vertex(a), r = b.vertex(ia), ss = b.vertex(ic),
                       t = b.vertex(c);
        b.tri(q, r, ss);
        b.tri(q, ss, t);
      }
  }
};
inline Sample front(const Shell &s, float x, float y) {
  bool found = false;
  Sample result{};
  float best = -1;
  for (auto t : s.triangles) {
    const Sample a = s.points[t[0]], c = s.points[t[1]], d = s.points[t[2]];
    const Vec3 e = c.p.p - a.p.p, f = d.p.p - a.p.p;
    const float det = e.x * f.y - e.y * f.x;
    if (std::fabs(det) < 1e-12f)
      continue;
    float u = ((x - a.p.p.x) * f.y - (y - a.p.p.y) * f.x) / det,
          v = (e.x * (y - a.p.p.y) - e.y * (x - a.p.p.x)) / det;
    if (u < -1e-5f || v < -1e-5f || u + v > 1.00001f)
      continue;
    float z = a.p.p.z + u * e.z + v * f.z;
    if (z <= best)
      continue;
    best = z;
    found = true;
    result = blend(blend(a, c, u / std::max(1e-6f, 1 - v)), d, v);
    result.p.p = {x, y, z};
  }
  if (!found)
    throw std::runtime_error("Vale equipment lacks physical support");
  return result;
}
} // namespace fury::vale_vest_detail
namespace fury {
inline void make_vale_vest(character_detail::Sculpt &b,
                           const vale_shirt_detail::ShirtSupport &shirt) {
  using namespace character_detail;
  using namespace vale_vest_detail;
  const Vec3 vest{.08f, .12f, .165f}, dark{.055f, .065f, .075f};
  std::vector<Vec3> normals(shirt.nodes.size());
  for (auto t : shirt.triangles) {
    Vec3 n = cross(shirt.nodes[t[1]].p - shirt.nodes[t[0]].p,
                   shirt.nodes[t[2]].p - shirt.nodes[t[0]].p);
    for (auto i : t)
      normals[i] += n;
  }
  for (auto &n : normals)
    if (dot(n, n) > 1e-20f)
      n = normalize(n);
  // Short pointed shirt collar, independently tailored to Vale's broad neck.
  // Leaves follow the true shirt support, with a closed folded edge.
  for (float side : {-1.f, 1.f}) {
    Shell leaf;
    const std::array<Vec2, 5> outline{{{.010f, .353f},
                                       {.041f, .350f},
                                       {.044f, .340f},
                                       {.026f, .318f},
                                       {.007f, .331f}}};
    std::vector<Cut> cuts{[](Vec3 p) { return p.z; }};
    for (unsigned e = 0; e < outline.size(); ++e) {
      const Vec2 a = outline[e], c = outline[(e + 1) % outline.size()];
      cuts.push_back([a, c, side](Vec3 p) {
        return -((c.x - a.x) * (p.y - a.y) - (c.y - a.y) * (side * p.x - a.x));
      });
    }
    for (auto t : shirt.triangles) {
      std::vector<Sample> poly;
      for (auto i : t)
        poly.push_back({shirt.nodes[i], normals[i]});
      poly = clip(std::move(poly), cuts);
      for (auto &q : poly) {
        const Vec3 p = q.p.p;
        q.p.p += q.n * (.0017f + .0043f * g(p.y, .330f, .019f));
        q.p.c = {.16f * .96f, .22f * .96f, .28f * .96f};
        q.p.uv = character_surface_uv(S::Shirt, (side * p.x - .007f) / .042f,
                                      (p.y - .305f) / .048f);
      }
      leaf.polygon(poly);
    }
    leaf.emit(b, .0008f);
  }
  const std::vector<Cut> vest_cuts{
      [](Vec3 p) {
        return p.y - (.077f + .008f * g(std::fabs(p.x), .105f, .025f));
      },
      [](Vec3 p) {
        const float f = smooth((p.z - .005f) / .050f);
        return .370f - (.082f * f + .036f * (1 - f)) * g(p.x, 0, .055f) - p.y;
      },
      [](Vec3 p) {
        return std::max(sq((p.y - .276f) / .091f) + sq((p.z - .003f) / .068f) -
                            1.f,
                        (.089f - std::fabs(p.x)) / .010f);
      }};
  Shell shell;
  for (auto t : shirt.triangles) {
    std::vector<Sample> poly;
    for (auto i : t)
      poly.push_back({shirt.nodes[i], normals[i]});
    poly = clip(std::move(poly), vest_cuts);
    for (auto &q : poly) {
      const Vec3 p = q.p.p;
      const float drape = .0005f * g(p.y, .095f, .025f);
      q.p.p += q.n * ((b.near ? .0068f : .0120f) + drape);
      q.p.c = vest;
      q.p.uv = character_surface_uv(S::Jacket,
                                    .5f + std::atan2(p.x, std::fabs(p.z)) / pi,
                                    smooth((p.y - .077f) / .30f));
    }
    shell.polygon(poly);
  }
  shell.emit(b, .0023f);
  const auto patch = [&](float cx, float cy, float hx, float hy, Vec3 color,
                         S surface, float lift, bool shield) {
    Shell p;
    const std::vector<Cut> cuts{[&](Vec3 q) { return q.x - cx + hx; },
                                [&](Vec3 q) { return cx + hx - q.x; },
                                [&](Vec3 q) { return q.y - cy + hy; },
                                [&](Vec3 q) { return cy + hy - q.y; },
                                [](Vec3 q) { return q.z; },
                                [&](Vec3 q) {
                                  return shield ? (q.y - cy + hy) * .72f +
                                                      hx * .28f -
                                                      std::fabs(q.x - cx)
                                                : 1.f;
                                }};
    for (auto t : shell.triangles) {
      std::vector<Sample> poly;
      for (auto i : t)
        poly.push_back(shell.points[i]);
      poly = clip(std::move(poly), cuts);
      for (auto &q : poly) {
        const Vec3 p = q.p.p;
        q.p.p += q.n * lift;
        q.p.c = color;
        q.p.uv = character_surface_uv(surface, (p.x - cx + hx) / (2 * hx),
                                      (p.y - cy + hy) / (2 * hy));
      }
      p.polygon(poly);
    }
    p.emit(b, .00065f);
  };
  patch(-.046f, .239f, .013f, .017f, {.64f, .68f, .53f}, S::Metal, .0016f,
        true);
  for (float side : {-1.f, 1.f}) {
    patch(side * .048f, .146f, .027f, .025f, vest * .89f, S::Jacket, .0012f,
          false);
    patch(side * .048f, .167f, .027f, .002f, dark, S::Shirt, b.near ? .0022f : .0045f, false);
  }
  // Two broad side-overlap closures wrap around the existing vest. Their
  // vertices retain the actual supporting triangle's deformation weights.
  for (float side : {-1.f, 1.f})
    for (float level : {.121f, .164f}) {
      Shell tab;
      std::vector<Cut> cuts{[&](Vec3 p) { return p.y - level; },
                            [&](Vec3 p) { return level + .019f - p.y; },
                            [](Vec3 p) { return p.z + .026f; },
                            [](Vec3 p) { return .026f - p.z; },
                            [&](Vec3 p) { return side * p.x - .090f; }};
      for (auto t : shell.triangles) {
        std::vector<Sample> poly;
        for (auto i : t)
          poly.push_back(shell.points[i]);
        poly = clip(std::move(poly), cuts);
        for (auto &q : poly) {
          Vec3 p = q.p.p;
          q.p.p += q.n * .0014f;
          q.p.c = dark;
          q.p.uv = character_surface_uv(S::Leather, (p.z + .026f) / .052f,
                                        (p.y - level) / .019f);
        }
        tab.polygon(poly);
      }
      tab.emit(b, .0007f);
    }
  // Compact rigid radio clipped to a separately fitted backing pocket.
  patch(.068f, .237f, .015f, .029f, dark, S::Leather, .0017f, false);
  Sample mount = front(shell, .068f, .241f);
  Vec3 center = mount.p.p + Vec3{0, 0, .012f};
  const auto radio_begin = b.m.influences.size();
  b.grid(b.near ? 24 : 8, b.near ? 8 : 2, [&](float u, float v) {
    const float a = u == 1 ? 0 : 2 * pi * u, k = .88f + .12f * std::sin(pi * v);
    return P{center +
                 Vec3{.0115f * k *
                          std::copysign(std::pow(std::fabs(std::cos(a)), .38f),
                                        std::cos(a)),
                      lerp(-.025f, .025f, v),
                      .009f * k *
                          std::copysign(std::pow(std::fabs(std::sin(a)), .38f),
                                        std::sin(a))},
             dark, wt(B::Chest), character_surface_uv(S::Rubber, u, v)};
  });
  for (int row = 0; row < (b.near ? 5 : 2); ++row) {
    float y = center.y - .012f + row * (b.near ? .004f : .008f);
    b.curve({{center.x - .007f, y, center.z + .0092f},
             {center.x + .007f, y, center.z + .0092f}},
            .00060f, dark * .55f, S::Rubber, B::Chest, 4);
  }
  b.tube(
      {center + Vec3{.006f, .022f, 0}, center + Vec3{.006f, .030f, 0},
       center + Vec3{.009f, .072f, -.006f}},
      {.002f, .0014f, .0007f}, 1, dark, S::Rubber,
      [](float) { return wt(B::Chest); }, b.near ? 8 : 4);
  b.ellipsoid(center + Vec3{-.005f, .027f, 0}, {.0027f, .0040f, .0027f},
              dark * .7f, S::Rubber, B::Chest, b.near ? 12 : 6, b.near?4:3);
  for (std::size_t i = radio_begin; i < b.m.influences.size(); ++i)
    b.m.influences[i] = mount.p.w;
  // Belt and left-hip pouch preserve the established equipment placement.
  b.grid(
      b.near ? 40 : 20, 4,
      [&](float u, float v) {
        const float a = 2 * pi * u;
        int row = int(std::lround(v * 4));
        const float yy[]{.048f, .070f, .070f, .048f, .048f},
            dr[]{0, 0, -.0013f, -.0013f, 0};
        return P{{(.098f + dr[row]) * std::cos(a), yy[row],
                  (.065f + dr[row]) * std::sin(a)},
                 dark,
                 wt(B::Pelvis),
                 character_surface_uv(S::Leather, u, v)};
      },
      true, false);
  b.ellipsoid({0, .059f, .066f}, {.0090f, .0073f, .0015f}, {.55f, .56f, .51f},
              S::Metal, B::Pelvis, b.near ? 16 : 6, b.near?5:3);
  b.grid(b.near ? 24 : 8, b.near ? 8 : 2, [&](float u, float v) {
    float a = u == 1 ? 0 : 2 * pi * u, k = .90f + .10f * std::sin(pi * v);
    return P{{-.109f + .018f * k *
                           std::copysign(std::pow(std::fabs(std::cos(a)), .35f),
                                         std::cos(a)),
              lerp(-.013f, .063f, v),
              .010f + .022f * k *
                          std::copysign(std::pow(std::fabs(std::sin(a)), .35f),
                                        std::sin(a))},
             dark,
             wt(B::Pelvis),
             character_surface_uv(S::Leather, u, v)};
  });
  b.grid(
      b.near ? 12 : 6, b.near?4:2,
      [&](float u, float v) {
        return P{{lerp(-.126f, -.092f, 1 - u), lerp(.043f, .064f, v),
                  .0324f + .002f * std::sin(pi * v)},
                 dark * .80f,
                 wt(B::Pelvis),
                 character_surface_uv(S::Leather, u, v)};
      },
      false, false);
  b.ellipsoid({-.109f, .047f, .0345f}, {.0025f, .0025f, .0007f},
              {.38f, .41f, .40f}, S::Metal, B::Pelvis, b.near ? 12 : 6, b.near?4:3);
}
} // namespace fury
