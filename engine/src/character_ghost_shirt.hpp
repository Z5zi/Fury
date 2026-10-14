#pragma once

#include "character_sculpt.hpp"
#include <tuple>

namespace fury::ghost_shirt_detail {
using namespace character_detail;

// One physical pattern vertex may have several texture corners. Keeping the
// source position AND influence in this table makes sewn UV splits exact under
// every pose; the table is also the authoritative normal-welding relation.
struct ShirtSupport {
  std::vector<P> nodes;
  std::vector<std::array<std::uint32_t, 3>> triangles;
};

struct Pattern {
  Sculpt &sculpt;
  std::vector<std::array<std::uint32_t, 3>> triangles;
  std::vector<P> points;
  std::vector<std::vector<std::uint32_t>> corners;
  std::map<std::tuple<std::uint32_t, float, float>, std::uint32_t> emitted;
  std::uint32_t node(Vec3 p, Vec3 color, CharacterInfluence weights) {
    points.push_back({p, color, weights, {}});
    corners.emplace_back();
    return std::uint32_t(points.size() - 1);
  }
  std::uint32_t corner(std::uint32_t node, Vec2 uv) {
    const auto key = std::make_tuple(node, uv.x, uv.y);
    const auto found = emitted.find(key);
    if (found != emitted.end())
      return found->second;
    P p = points[node];
    p.uv = uv;
    const auto result = sculpt.vertex(p);
    emitted.emplace(key, result);
    corners[node].push_back(result);
    return result;
  }
  void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c, Vec2 ua,
                Vec2 ub, Vec2 uc) {
    sculpt.tri(corner(a, ua), corner(b, ub), corner(c, uc));
    triangles.push_back({a, b, c});
  }
  void quad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d,
            Vec2 ua, Vec2 ub, Vec2 uc, Vec2 ud) {
    triangle(a, b, c, ua, ub, uc);
    triangle(a, c, d, ua, uc, ud);
  }
  void finish() {
    for (auto &group : corners)
      if (group.size() > 1)
        sculpt.normal_groups.push_back(std::move(group));
  }
};

inline CharacterInfluence body_weight(float y) {
  if (y < .14f)
    return wt(B::Spine, B::Pelvis, smooth((y - .025f) / .115f));
  return wt(B::Chest, B::Spine, smooth((y - .14f) / .095f));
}

inline float section(float y, const std::array<float, 6> &values) {
  constexpr std::array<float, 6> levels{.012f, .07f, .14f, .22f, .28f, .333f};
  for (int k = 0; k < 5; ++k)
    if (y <= levels[k + 1])
      return lerp(values[k], values[k + 1],
                  smooth((y - levels[k]) / (levels[k + 1] - levels[k])));
  return values.back();
}

inline Vec3 body_point(float a, float y) {
  const float cs = std::cos(a), sn = std::sin(a);
  const float rx = section(y, {.111f, .108f, .105f, .112f, .119f, .118f});
  const float rz = section(y, {.071f, .072f, .068f, .071f, .067f, .056f});
  // Local broad pulls, rather than equally spaced circumferential corrugation.
  const float front = std::max(0.f, sn), back = std::max(0.f, -sn);
  const float fold = .0018f * g(y, .075f + .039f * cs, .027f) * front * front -
                     .0012f * g(y, .115f + .031f * cs, .023f) * front * front +
                     .0015f * g(y, .205f + .055f * cs, .029f) * front * front +
                     .0038f * g(y, .162f - .033f * cs, .027f) * back * back -
                     .0024f * g(y, .184f - .033f * cs, .024f) * back * back +
                     .0028f * g(y, .095f + .038f * cs, .023f) * back * back +
                     .0010f * g(y, .035f, .024f) * std::sin(3 * a + .7f);
  return {(rx + fold) * cs, y,
          (rz + fold) * sn + .006f * front * front - .002f * back * back};
}

inline Vec3 armhole_point(float a, float y) {
  constexpr float half_angle = 3 * pi / 16;
  const float side = std::cos(a) < 0 ? -1.f : 1.f;
  const float u = std::sin(a) / std::sin(half_angle);
  const float v = (y - .269f) / .064f;
  const float uc = std::clamp(u, -1.f, 1.f);
  const float vc = std::clamp(v, -1.f, 1.f);
  const float U = uc * std::sqrt(1 - .5f * vc * vc);
  const float V = vc * std::sqrt(1 - .5f * uc * uc);
  float ac = std::asin(uc * std::sin(half_angle));
  if (side < 0)
    ac = pi - ac;
  const Vec3 original = body_point(ac, .269f + .064f * vc);
  const Vec3 round{side * (.108f + .010f * V + .007f * U * U),
                   .269f + .064f * V, .042f * U + .003f};
  const float support = (1 - smooth((std::fabs(u) - 1) / .72f)) *
                        (1 - smooth((std::fabs(v) - 1) / .65f));
  Vec3 ease=(round-original)*support;
  // Keep the horizontal identification-band sewing rows truly horizontal;
  // only the upper underarm needs the rounded vertical cut-out easing.
  ease.y*=smooth((y-.190f)/.015f);
  return body_point(a,y)+ease;
}

inline CharacterInfluence torso_weight(float a, float y) {
  constexpr float half_angle = 3 * pi / 16;
  const float u = std::sin(a) / std::sin(half_angle);
  const float v = (y - .269f) / .064f;
  const float arm = (.74f - .22f * smooth((v + 1) * .5f)) *
                    (1 - smooth((std::fabs(u) - 1) / .72f)) *
                    (1 - smooth((std::fabs(v) - 1) / 1.00f)) *
                    smooth((y - .180f) / .035f);
  if (arm <= 0)
    return body_weight(y);
  const auto body = body_weight(y);
  return {body.first, body.second, (1 - arm) * body.first_weight,
          std::uint8_t(std::cos(a) < 0 ? B::LeftUpperArm : B::RightUpperArm),
          arm};
}
} // namespace fury::ghost_shirt_detail

namespace fury {
// Original GhostLoop garment construction. All dimensions are normalized by height,
// +Z faces forward and the pelvis is the origin. No reference mesh, texture or
// commercial garment pattern is used. The supplied query is the source neck
// radial intersection, measured from (0,y,neck_center.z).
inline ghost_shirt_detail::ShirtSupport
make_ghost_shirt(character_detail::Sculpt &b,
                const std::function<float(float, float, float)> &anatomy_radius,
                const std::function<CharacterInfluence(Vec3)> &neck_weight,
                Vec3 neck_center) {
  using namespace character_detail;
  using namespace ghost_shirt_detail;
  Pattern pattern{b, {}, {}, {}, {}};
  const Vec3 cloth = b.m.appearance.shirt;
  const int columns = b.near ? 48 : 24;
  const int below = b.near ? 10 : 6, hole_rows = b.near ? 8 : 4;
  const int rows = below + hole_rows, half_hole = b.near ? 4 : 2;
  const auto angle = [&](int column) {
    return pi * .5f + 2 * pi * float(column % columns) / columns;
  };
  const auto height = [&](int row) {
    // The two identification-band rows are physical garment rows. The vest
    // and tape can share complete posed facets without clipped-weight drift.
    if(row==below-2)return .171f;
    if(row==below-1)return .185f;
    return row <= below ? lerp(.057f, .205f, float(row) / below)
                        : lerp(.205f, .333f, float(row - below) / hole_rows);
  };
  const auto uv = [&](float metres_u, float metres_v) {
    return character_surface_uv(S::Shirt, .025f + metres_u / .75f,
                                .025f + metres_v / .70f);
  };
  std::vector<std::vector<std::uint32_t>> torso(
      rows + 1, std::vector<std::uint32_t>(columns));
  for (int j = 0; j <= rows; ++j)
    for (int i = 0; i < columns; ++i) {
      const float a = angle(i), y = height(j);
      torso[j][i] =
          pattern.node(armhole_point(a, y), cloth, torso_weight(a, y));
    }
  const auto body_index = [&](int j, int i) { return torso[j][i % columns]; };
  // Two body charts retain cloth's metre scale; the front/back centers are
  // exact physical seams with distinct texture corners.
  std::vector<float> circumference(columns + 1);
  for (int i = 1; i <= columns; ++i)
    circumference[i] =
        circumference[i - 1] +
        length(body_point(angle(i), .23f) - body_point(angle(i - 1), .23f)) *
            b.h;
  const auto body_uv = [&](int j, int i, int chart) {
    return uv(circumference[i] - circumference[chart * columns / 2],
              (height(j) - .012f) * b.h);
  };
  for (int j = 0; j < rows; ++j)
    for (int i = 0; i < columns; ++i) {
      bool hole = false;
      for (int center : {columns / 4, 3 * columns / 4})
        hole |= j >= below && i >= center - half_hole && i < center + half_hole;
      if (hole)
        continue;
      const int chart = i / (columns / 2);
      pattern.quad(body_index(j, i), body_index(j + 1, i),
                   body_index(j + 1, i + 1), body_index(j, i + 1),
                   body_uv(j, i, chart), body_uv(j + 1, i, chart),
                   body_uv(j + 1, i + 1, chart), body_uv(j, i + 1, chart));
    }

  // Shoulder yoke terminates in the real neck envelope. Its first row is the
  // actual torso row, including the upper edges of both open armholes.
  std::vector<std::uint32_t> previous = torso.back();
  std::vector<Vec3> neckline(columns);
  for (int i = 0; i < columns; ++i) {
    const float a = angle(i), front = std::max(0.f, std::sin(a));
    const float y = neck_center.y + .005f - .008f * front +
                    .005f * std::max(0.f, -std::sin(a));
    const float r = anatomy_radius(y, a, neck_center.z) + .0048f;
    neckline[i] = {r * std::cos(a), y, neck_center.z + r * std::sin(a)};
  }
  const int yoke_rows = 4;
  for (int j = 1; j <= yoke_rows; ++j) {
    const float v = float(j) / yoke_rows;
    std::vector<std::uint32_t> next(columns);
    for (int i = 0; i < columns; ++i) {
      const float a = angle(i);
      const P &start = pattern.points[torso.back()[i]];
      Vec3 p = start.p * (1 - v) + neckline[i] * v;
      p.y += .002f * std::sin(pi * v);
      const Vec3 radial{p.x, 0, p.z - neck_center.z};
      const float r = length(radial);
      if (r > 1e-6f) {
        const float actual_angle = std::atan2(radial.z, radial.x);
        const float skin = anatomy_radius(p.y, actual_angle, neck_center.z);
        p += radial * (std::max(0.f, skin + .0038f - r) / r);
      }
      auto weights = torso_weight(a, .333f);
      // The clavicle-side yoke supports the turned collar and follows the
      // chest. Transfer sleeve influence through the outer shoulder only;
      // retaining it under the collar makes carrying-bind retargets deform
      // the coarse support triangle differently from its fitted cloth rim.
      weights.third_weight *= 1 - smooth(v / .5f);
      weights.first_weight = 1 - weights.third_weight;
      if(v>.5f) {
        const auto skin=neck_weight(p);const float blend=smooth((v-.5f)/.5f);
        std::array<float,character_bone_count> parts{};
        const auto add=[&](CharacterInfluence w,float f){parts[w.first]+=f*w.first_weight;parts[w.second]+=f*(1-w.first_weight-w.third_weight);parts[w.third]+=f*w.third_weight;};
        add(weights,1-blend);add(skin,blend);
        const float head=std::clamp(parts[bone_index(B::Head)],0.f,1.f),chest=std::clamp(parts[bone_index(B::Chest)],0.f,1-head);
        weights={std::uint8_t(B::Head),std::uint8_t(B::Neck),head,std::uint8_t(B::Chest),chest};
      }
      next[i] = pattern.node(p, cloth, weights);
    }
    for (int i = 0; i < columns; ++i) {
      const int k = (i + 1) % columns, chart = i / (columns / 2);
      auto tex = [&](int ii, float vv) {
        return uv((circumference[ii] - circumference[chart * columns / 2]) *
                      lerp(1.f, .53f, vv),
                  (.321f + .052f * vv) * b.h);
      };
      const float low = float(j - 1) / yoke_rows;
      pattern.quad(previous[i], next[i], next[k], previous[k], tex(i, low),
                   tex(i, v), tex(i + 1, v), tex(i + 1, low));
    }
    previous = std::move(next);
  }
  const auto neckline_nodes = previous;

  ShirtSupport support{pattern.points, pattern.triangles};
  // GhostLoop's compact work shirt has a quiet bound round neckline. The outer
  // loop is the actual yoke boundary; three return rows turn the edge inward
  // without a floating slab or a chest cap across the neck opening.
  previous = neckline_nodes;
  for (int row = 1; row <= 3; ++row) {
    std::vector<std::uint32_t> next(columns);
    for (int i = 0; i < columns; ++i) {
      const P &base = pattern.points[neckline_nodes[i]];
      const float a = angle(i);
      const Vec3 radial{std::cos(a), 0, std::sin(a)};
      Vec3 p = base.p;
      if (row == 1)
        p += Vec3{0, .0026f, 0} + radial * .0005f;
      else if (row == 2)
        p += Vec3{0, .0026f, 0} - radial * .0012f;
      else
        p -= radial * .0012f;
      const float radius=length(Vec3{p.x,0,p.z-neck_center.z});
      const float required=anatomy_radius(p.y,a,neck_center.z)+(row==1?.0048f:.0031f);
      if(radius<required)p+=radial*(required-radius);
      next[i] = pattern.node(p, cloth * .94f, neck_weight(p));
    }
    for (int i = 0; i < columns; ++i) {
      int k = (i + 1) % columns;
      pattern.quad(previous[i], next[i], next[k], previous[k],
                   uv(float(i) * .008f, (row - 1) * .003f),
                   uv(float(i) * .008f, row * .003f),
                   uv(float(i + 1) * .008f, row * .003f),
                   uv(float(i + 1) * .008f, (row - 1) * .003f));
    }
    previous = std::move(next);
  }

  // Sleeves are extruded from the ordered boundary of each missing torso
  // region. The first ring reuses the torso nodes: there is no cap, buried
  // sleeve tube, or solid chest surface across either opening.
  for (int center : {columns / 4, 3 * columns / 4}) {
    std::vector<std::uint32_t> root;
    const int lo = center - half_hole, hi = center + half_hole;
    for (int i = lo; i < hi; ++i)
      root.push_back(body_index(below, i));
    for (int j = below; j < rows; ++j)
      root.push_back(body_index(j, hi));
    for (int i = hi; i > lo; --i)
      root.push_back(body_index(rows, i));
    for (int j = rows; j > below; --j)
      root.push_back(body_index(j, lo));
    const int count = int(root.size());
    const float side = center < columns / 2 ? -1.f : 1.f;
    const B upper = side < 0 ? B::LeftUpperArm : B::RightUpperArm;
    const B fore = side < 0 ? B::LeftForearm : B::RightForearm;
    std::vector<float> phase(count), distance(count + 1);
    for (int i = 0; i < count; ++i) {
      const Vec3 p = pattern.points[root[i]].p;
      phase[i] = std::atan2((p.z - .003f) / .042f, (p.y - .269f) / .064f);
      if (i)
        distance[i] = distance[i - 1] + length(pattern.points[root[i]].p -
                                               pattern.points[root[i - 1]].p) *
                                            b.h;
    }
    distance[count] =
        distance[count - 1] +
        length(pattern.points[root.front()].p - pattern.points[root.back()].p) *
            b.h;
    previous = root;
    const int cap_rows = b.near ? 4 : 2, lower_rows = b.near ? 7 : 4;
    const int sleeve_rows = cap_rows + lower_rows;
    float previous_run = 0;
    for (int j = 1; j <= sleeve_rows + 2; ++j) {
      std::vector<std::uint32_t> next(count);
      const float run = std::min(1.f, float(j) / sleeve_rows);
      for (int i = 0; i < count; ++i) {
        const float a = phase[i], cs = std::cos(a), sn = std::sin(a);
        Vec3 p{};
        CharacterInfluence weights{};
        if (j <= cap_rows) {
          const float t = float(j) / cap_rows;
          const P &start = pattern.points[root[i]];
          const Vec3 end{side * (.150f + .031f * cs), .216f + .0035f * cs,
                         .004f + .031f * sn};
          p = start.p * (1 - t) + end * t;
          p.x += side * .014f * std::sin(pi * t) * (.5f + .5f * cs);
          // The underarm arches up and then turns down into the sleeve. A
          // straight interpolation would reverse direction at the first arm
          // ring and create a folding coarse-LOD triangle during a walk.
          p.y += std::sin(pi * t) *
                 (.030f * std::max(0.f, cs) + .020f * std::max(0.f, -cs));
          const float arm = lerp(start.w.third_weight, 1.f, smooth(t));
          const float torso_chest =
              start.w.first_weight / std::max(1e-5f, 1 - start.w.third_weight);
          weights = {std::uint8_t(B::Chest), std::uint8_t(B::Spine),
                     (1 - arm) * torso_chest, std::uint8_t(upper), arm};
        } else {
          const float t = std::min(1.f, float(j - cap_rows) / lower_rows);
          const float y = lerp(.216f, .114f, t);
          const float elbow_t = smooth((.216f - y) / .096f);
          const float wrist_t = smooth((.12f - y) / .16f);
          const float x = lerp(.150f, .158f, elbow_t) + .022f * wrist_t;
          const float z = lerp(.006f, .008f, elbow_t) + .010f * wrist_t;
          const float front = sq(std::max(0.f, sn));
          const float back = sq(std::max(0.f, -sn));
          const float fold = .0028f * g(y, .114f + .014f * cs, .020f) * front -
                             .0017f * g(y, .140f - .010f * cs, .018f) * front +
                             .0018f * g(y, .080f - .013f * cs, .017f) * front +
                             .0014f * g(y, .127f, .037f) * back;
          float r = lerp(.031f, .026f, elbow_t) - .007f * wrist_t + fold;
          r += .0034f * g(y, .125f, .012f);
          p = {side * (x + r * cs), y + .0035f * cs * (1 - t),
               z + r * (1.03f - .05f * t) * sn};
          if (j == sleeve_rows + 1) {
            p.x -= side * .00135f * cs;
            p.y -= .0003f;
            p.z -= .00135f * sn;
          } else if (j == sleeve_rows + 2) {
            p.x -= side * .00135f * cs;
            p.y += .006f;
            p.z -= .00135f * sn;
          }
          weights = wt(fore, upper, smooth((.163f - y) / .081f));
        }
        const float cuff = smooth((run - .76f) / .16f);
        next[i] = pattern.node(p, cloth * (1 - .045f * cuff), weights);
      }
      std::vector<float> next_distance(count + 1);
      float next_run = previous_run;
      for (int i = 0; i < count; ++i) {
        next_run +=
            length(pattern.points[next[i]].p - pattern.points[previous[i]].p) *
            b.h / count;
        next_distance[i + 1] =
            next_distance[i] + length(pattern.points[next[(i + 1) % count]].p -
                                      pattern.points[next[i]].p) *
                                   b.h;
      }
      for (int i = 0; i < count; ++i) {
        const int k = (i + 1) % count;
        pattern.quad(previous[i], next[i], next[k], previous[k],
                     uv(distance[i], previous_run),
                     uv(next_distance[i], next_run),
                     uv(next_distance[i + 1], next_run),
                     uv(distance[i + 1], previous_run));
      }
      previous = std::move(next);
      distance = std::move(next_distance);
      previous_run = next_run;
    }
  }

  // A continuous turned hem is a narrow return, not a solid body end cap.
  previous = torso.front();
  for (int row = 1; row <= 2; ++row) {
    std::vector<std::uint32_t> next(columns);
    for (int i = 0; i < columns; ++i) {
      const P &outer = pattern.points[torso.front()[i]];
      const Vec3 radial = normalize(Vec3{outer.p.x, 0, outer.p.z});
      const Vec3 p =
          outer.p - radial * .0014f + Vec3{0, row == 1 ? -.0003f : .007f, 0};
      next[i] = pattern.node(p, cloth * .95f, outer.w);
    }
    for (int i = 0; i < columns; ++i) {
      const int k = (i + 1) % columns, chart = i / (columns / 2);
      const float u0 = circumference[i] - circumference[chart * columns / 2];
      const float u1 =
          circumference[i + 1] - circumference[chart * columns / 2];
      pattern.quad(previous[i], previous[k], next[k], next[i],
                   uv(u0, .01f * (row - 1)), uv(u1, .01f * (row - 1)),
                   uv(u1, .01f * row), uv(u0, .01f * row));
    }
    previous = std::move(next);
  }

  pattern.finish();
  return support;
}
} // namespace fury
