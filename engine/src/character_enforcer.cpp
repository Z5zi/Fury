#include "character_enforcer_uniform.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/enforcer_anatomy.hpp"
#include "fury/generated/enforcer_hands.hpp"
#include "fury/generated/enforcer_hair.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
namespace fury {

namespace {
using namespace character_detail;
// Original integration of the documented CC0 anatomy data. No upstream
// program logic is included. Surface queries fit Fury's own cosmetics/hair.
float anatomy_front(float x, float y) {
  float best = -1.f;
  for (std::size_t i = 0; i < enforcer_anatomy::indices.size(); i += 3) {
    const Vec3 a =
        enforcer_anatomy::positions[enforcer_anatomy::indices[i]];
    const Vec3 b =
        enforcer_anatomy::positions[enforcer_anatomy::indices[i + 1]];
    const Vec3 c =
        enforcer_anatomy::positions[enforcer_anatomy::indices[i + 2]];
    const float denominator =
        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(denominator) < 1e-12f)
      continue;
    const float u =
        ((x - a.x) * (c.y - a.y) - (y - a.y) * (c.x - a.x)) / denominator;
    const float v =
        ((b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x)) / denominator;
    if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
      best = std::max(best, a.z + u * (b.z - a.z) + v * (c.z - a.z));
  }
  return best;
}
template<class Positions,class Indices>
float radial_intersection(const Positions& positions,const Indices& indices,
                          float y,float angle,float center_z) {
  const Vec3 origin{0, y, center_z},
      direction{std::cos(angle), 0, std::sin(angle)};
  float best = 0.f;
  for (std::size_t i = 0; i < indices.size(); i += 3) {
    const Vec3 a =
        positions[indices[i]];
    const Vec3 b =
        positions[indices[i + 1]];
    const Vec3 c =
        positions[indices[i + 2]];
    if (y < std::min({a.y, b.y, c.y}) - 1e-6f ||
        y > std::max({a.y, b.y, c.y}) + 1e-6f)
      continue;
    const Vec3 e1 = b - a, e2 = c - a, p = cross(direction, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-12f)
      continue;
    const Vec3 t = origin - a;
    const float u = dot(t, p) / det;
    if (u < 0.f || u > 1.f)
      continue;
    const Vec3 q = cross(t, e1);
    const float v = dot(direction, q) / det;
    if (v < 0.f || u + v > 1.f)
      continue;
    const float d = dot(e2, q) / det;
    if (d > best)
      best = d;
  }
  return best;
}
float anatomy_radius(float y,float angle,float center_z) {
  return radial_intersection(enforcer_anatomy::positions,enforcer_anatomy::indices,y,angle,center_z);
}
CharacterInfluence enforcer_neck_weight(Vec3 point) {
  // The rigid jaw/neck seam rises toward the nape. A horizontal cutoff at
  // the chin compressed the posterior neck bend and produced a sharp lump.
  // Intersect the actual source seam in the radial plane through this point.
  // The blend finishes below that seam; the complete skull/jaw is rigid.
  const float x=point.x,z=point.z-enforcer_anatomy::neck_center.z;
  float head_start=1.f,nape_y=enforcer_anatomy::rigid_head_begin_y;
  const auto &seam=enforcer_anatomy::head_neck_join;
  for(std::size_t i=0;i<seam.size();++i) {
    const Vec3 a=seam[i],b=seam[(i+1)%seam.size()];
    if(a.z<enforcer_anatomy::neck_center.z&&std::fabs(a.x)<.01f)nape_y=std::max(nape_y,a.y-.0015f);
    const float az=a.z-enforcer_anatomy::neck_center.z;
    const float dx=b.x-a.x,dz=b.z-a.z;
    const float denominator=dx*z-dz*x;
    if(std::fabs(denominator)<1e-10f)continue;
    const float t=(az*x-a.x*z)/denominator;
    if(t<0.f||t>1.f)continue;
    const float radial=(a.x+t*dx)*x+(az+t*dz)*z;
    if(radial>0.f)head_start=std::min(head_start,a.y+t*(b.y-a.y)-.0015f);
  }
  if(head_start==1.f)head_start=enforcer_anatomy::rigid_head_begin_y;
  // Fit a smooth lower envelope beneath the source boundary. The ear-side
  // chart corner rises abruptly; skin weights need not inherit that corner.
  const float radius=std::sqrt(x*x+z*z);
  const float back=radius>1e-7f?.5f*(1.f-z/radius):0.f;
  head_start=std::min(head_start,enforcer_anatomy::rigid_head_begin_y+
      (nape_y-enforcer_anatomy::rigid_head_begin_y)*back);
  const float neck_start=enforcer_anatomy::neck_center.y-.017f;
  const float neck_mid=enforcer_anatomy::neck_center.y-.004f;
  return point.y < neck_mid ? wt(B::Neck,B::Chest,smooth((point.y-neck_start)/(neck_mid-neck_start)))
                            : wt(B::Head,B::Neck,smooth((point.y-neck_mid)/(head_start-neck_mid)));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::vector<bool> rigid(enforcer_anatomy::positions.size(),false);
  for(std::size_t t=0;t<enforcer_anatomy::indices.size();t+=3) {
    const auto uv=enforcer_anatomy::uv_coordinates[enforcer_anatomy::uv_indices[t]];
    if(uv.y>.8406f)continue; // Only the anatomical neck/shoulder chart blends.
    for(int k=0;k<3;++k)rigid[enforcer_anatomy::indices[t+k]]=true;
  }
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      enforcer_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < enforcer_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = enforcer_anatomy::indices[triangle + corner];
      const auto uv_index = enforcer_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = enforcer_anatomy::positions[index];
        const Vec2 uv = enforcer_anatomy::uv_coordinates[uv_index];
        const float lip =
            g(position.x, 0, .015f) *
            g(position.y, enforcer_anatomy::lip_surface_center.y, .0032f) *
            smooth((position.z - enforcer_anatomy::lip_surface_center.z +
                    .004f) /
                   .009f);
        // Small anatomical pigment zones retain the stable base complexion.
        const float cheeks = g(std::fabs(position.x), .030f, .015f) *
                             g(position.y, enforcer_anatomy::lip_surface_center.y + .012f, .015f) *
                             smooth((position.z - .067f) / .02f);
        const float nose = g(position.x, 0, .009f) *
                           g(position.y, enforcer_anatomy::lip_surface_center.y + .016f, .012f) *
                           smooth((position.z - .081f) / .013f);
        const float front = smooth((position.z - .054f) / .025f);
        const float vascular =
            .5f + .25f * std::sin(position.x * 360 + position.y * 97) +
            .25f * std::sin(position.y * 285 - position.z * 190);
        const float lid = g(std::fabs(position.x), std::fabs(enforcer_anatomy::eye_centers[0].x), .008f) *
                          g(position.y, enforcer_anatomy::eye_centers[0].y, .006f) * front;
        Vec3 color = skin * (1 - .025f * vascular * front);
        color.x *= 1 - .015f * lid;
        color.y *= 1 - .055f * lid;
        color.z *= 1 - .037f * lid;
        color.y *= 1 - .052f * cheeks - .043f * nose;
        color.z *= 1 - .077f * cheeks - .054f * nose;
        color.y *= 1 - .22f * lip;
        color.z *= 1 - .19f * lip;
        const auto vertex =
            b.vertex({position, color, rigid[index]?wt(B::Head):enforcer_neck_weight(position),
                      character_surface_uv(S::Face, uv.x, uv.y)});
        groups[index].push_back(vertex);
        found = corners.emplace(key, vertex).first;
      }
      out[corner] = found->second;
    }
    b.tri(out[0], out[1], out[2]);
  }
  // Continue the complete original attachment loop into the shirt. The source
  // Face data remains untouched; this Skin underlay removes a visible cut edge
  // without forcing the collar onto the anatomical neck. Its lower ring stays
  // hidden inside the closed garment, as a deliberate body attachment boundary.
  auto boundary = enforcer_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = enforcer_anatomy::positions[boundary[i]];
    const Vec3 c =
        enforcer_anatomy::positions[boundary[(i + 1) % boundary.size()]];
    area += a.x * c.z - c.x * a.z;
  }
  if (area < 0)
    std::reverse(boundary.begin(), boundary.end());
  const auto underlay = std::uint32_t(b.m.bind_mesh.vertices.size());
  const int columns = int(boundary.size());
  b.grid(
      columns, 1,
      [&](float u, float v) {
        const auto i = std::size_t(std::lround(u * columns)) % boundary.size();
        Vec3 point = enforcer_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, enforcer_neck_weight(point),
                 character_surface_uv(S::Skin, u, v)};
      },
      true, false);
  // The source correspondence group performs the upper U seam average once.
  b.smooth_seams.pop_back();
  for (int i = 0; i <= columns; ++i)
    groups[boundary[std::size_t(i) % boundary.size()]].push_back(
        underlay + std::uint32_t(columns + 1 + i));
  for (auto &group : groups)
    if (group.size() > 1)
      b.normal_groups.push_back(std::move(group));
  for (std::size_t ei = 0; ei < enforcer_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = enforcer_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? .0008f : -.0008f, -.0007f, -.0015f};
    const Vec3 radius = enforcer_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 8, b.near ? 8 : 6);
    // The imported source already includes connected eyelid rims/folds.
    // Do not add a separately sorted cosmetic tube through the tear corners.
    const int iris_columns = b.near ? 24 : 10, iris_rows = b.near ? 5 : 3;
    const auto iris_base = std::uint32_t(b.m.bind_mesh.vertices.size());
    b.grid(
        iris_columns, iris_rows,
        [&](float u, float v) {
          const float near_r[]{.00005f, .00122f, .00140f,
                               .00225f, .00310f, .00355f};
          const float far_r[]{.00005f, .00122f, .00140f, .00355f};
          int row = std::min(iris_rows, int(std::lround(v * iris_rows)));
          float r = b.near ? near_r[row] : far_r[row], a = 2 * pi * u,
                x = r * std::cos(a), y = r * std::sin(a);
          float z = radius.z * std::sqrt(std::max(0.f, 1 - sq(x / radius.x) -
                                                           sq(y / radius.y))) +
                    .00015f;
          Vec3 color{.005f, .006f, .005f};
          if (r > .00130f) {
            float fiber =
                .88f + .16f * std::sin(a * 37 + .4f * std::sin(a * 11));
            color = b.m.appearance.eyes * fiber;
            if (r > .0031f)
              color = color * .38f;
          }
          return P{eye + Vec3{x, y, z}, color, wt(B::Head),
                   character_surface_uv(S::Eyes, u, v)};
        },
        true, false);
    auto center = b.vertex({eye + Vec3{0, 0, radius.z + .00015f},
                            {.005f, .006f, .005f},
                            wt(B::Head),
                            character_surface_uv(S::Eyes, .5f, .5f)});
    for (int i = 0; i < iris_columns; ++i)
      b.tri(center, iris_base + std::uint32_t(i),
            iris_base + std::uint32_t(i + 1));
    std::vector<Vec3> brow;
    std::vector<float> brow_radius;
    const int count = b.near ? 24 : 4;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, x = eye.x + lerp(-.0118f, .0118f, t),
            outward = eye.x > 0 ? t : 1 - t,
            y = eye.y + .0122f + .0025f * std::sin(pi * outward) -
                .0017f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00010f + .00086f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .74f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 6 : 4);
  }
}

// Syndicate Enforcer's relaxed bind turns each palm toward the thigh, with thumbs
// forward. This fixed rigid fitting is shared by source fingers and the sewn
// wrist ring; it is not a finger controller or per-frame hand animation.
Vec3 enforcer_hand_offset(Vec3 p, float side) {
  p.x *= side;
  const float a = side * 1.16f, c = std::cos(a), sn = std::sin(a);
  return {c * p.x + sn * p.z, p.y, -sn * p.x + c * p.z};
}
void make_hands_and_forearms(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  for (float side : {-1.f, 1.f}) {
    const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
    const B upper = side > 0 ? B::RightUpperArm : B::LeftUpperArm;
    const B hand = side > 0 ? B::RightHand : B::LeftHand;
    const Vec3 wrist{side * .184f, -.043f, .018f};
    std::map<std::pair<unsigned, unsigned>, unsigned> corners;
    std::vector<std::vector<unsigned>> groups(
        enforcer_hands::positions.size());
    const auto emit = [&](auto const &indices, auto const &uv_indices) {
      for (std::size_t t = 0; t < indices.size(); t += 3) {
        unsigned out[3];
        for (int k = 0; k < 3; ++k) {
          auto index = indices[t + k], uv = uv_indices[t + k];
          const bool hard =
              b.near
                  ? std::find(
                        enforcer_hands::hard_normal_triangle_indices.begin(),
                        enforcer_hands::hard_normal_triangle_indices.end(),
                        t / 3) !=
                        enforcer_hands::hard_normal_triangle_indices.end()
                  : std::find(
                        enforcer_hands::far_hard_normal_triangle_indices
                            .begin(),
                        enforcer_hands::far_hard_normal_triangle_indices
                            .end(),
                        t / 3) !=
                        enforcer_hands::far_hard_normal_triangle_indices
                            .end();
          auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) *
                                    enforcer_hands::uv_coordinates.size())
                         : 0));
          auto found = corners.find(key);
          if (found == corners.end()) {
            Vec3 p = enforcer_hand_offset(
                enforcer_hands::positions[index], side);
            p += wrist;
            const Vec2 tex = enforcer_hands::uv_coordinates[uv];
            auto i = b.vertex({p, skin, wt(hand),
                               character_surface_uv(S::Skin, tex.x, tex.y)});
            if (!hard)
              groups[index].push_back(i);
            found = corners.emplace(key, i).first;
          }
          out[k] = found->second;
        }
        if (side > 0)
          b.tri(out[0], out[1], out[2]);
        else
          b.tri(out[0], out[2], out[1]);
      }
    };
    if (b.near)
      emit(enforcer_hands::near_indices, enforcer_hands::near_uv_indices);
    else
      emit(enforcer_hands::far_indices, enforcer_hands::far_uv_indices);
    // Exactly reuse every anatomical wrist point at the terminal forearm ring.
    // The opposite terminal ring tucks inside the uniform cuff, not a cap.
    auto boundary = enforcer_hands::wrist_boundary_indices;
    float area = 0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
      auto
          a = enforcer_hands::positions[boundary[i]],
          c = enforcer_hands::positions[boundary[(i + 1) % boundary.size()]];
      area += a.x * c.z - c.x * a.z;
    }
    if (area * side < 0)
      std::reverse(boundary.begin(), boundary.end());
    Vec3 center{};
    for (auto i : boundary)
      center += enforcer_hands::positions[i];
    center = center * (1.f / boundary.size());
    const unsigned start = b.m.bind_mesh.vertices.size();
    const int columns = boundary.size(), rows = b.near ? 4 : 1;
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
              boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = enforcer_hand_offset(
              enforcer_hands::positions[index], side);
          source += wrist;
          const Vec3 local = enforcer_hand_offset(
              enforcer_hands::positions[index] - center, side);
          const float a = std::atan2(local.z, local.x);
          const float y = lerp(source.y, .005f, v),
                      radius = .0155f + .002f * smooth(v);
          const float x = side * lerp(.184f, .180f, smooth(v));
          Vec3 target{x + radius * std::cos(a), y,
                      .018f + radius * .83f * std::sin(a)};
          Vec3 p = source * (1 - smooth(v / .28f)) + target * smooth(v / .28f);
          CharacterInfluence w =
              y < -.015f ? wt(hand, fore, 1 - smooth((y + .043f) / .04f))
                         : wt(fore, upper, smooth((.163f - y) / .081f));
          if (v == 0)
            w = wt(hand);
          return P{p, skin, w, character_surface_uv(S::Skin, u, v)};
        },
        true, false);
    // Suppress the duplicate periodic seam average at the wrist row.
    b.smooth_seams.erase(b.smooth_seams.end() - (rows + 1));
    for (int i = 0; i <= columns; ++i)
      groups[boundary[i % columns]].push_back(start + i);
    for (auto &g : groups)
      if (g.size() > 1)
        b.normal_groups.push_back(std::move(g));
  }
}
void make_boots(Sculpt &b) {
  for (float sign : {-1.f, 1.f}) {
    const B foot = sign > 0 ? B::RightFoot : B::LeftFoot;
    const float x = sign * .061f;
    // Sole and toe box share the animator's exact supported heel/toe envelope.
    b.grid(b.near ? 28 : 12, b.near ? 3 : 2, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.5f, -.483f, v);
      return P{{x + .032f * std::cos(a), y, .033f + .067f * std::sin(a)},
               {.055f,.065f,.075f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    const auto upper_begin = b.m.bind_mesh.indices.size();
    b.grid(b.near ? 32 : 8, b.near ? 10 : 3, [&](float u, float v) {
      // Spend the coarse samples at the toe and instep curvature changes.
      const float far_sections[4]{0.f, .14f, .36f, 1.f};
      if (!b.near)
        v = far_sections[std::min(3, int(std::lround(v * 3)))];
      float a = 2 * pi * u, y = lerp(-.483f, -.440f, v),
            rx = lerp(.0315f, .021f, smooth(v / .40f)),
            rz = lerp(.066f, .022f, smooth(v / .40f)),
            cz = lerp(.033f, 0.f, smooth(v / .40f));
      return P{{x + rx * std::cos(a), y, cz + rz * std::sin(a)},
               b.m.appearance.shoes,
               wt(foot),
               character_surface_uv(S::Leather, u, v)};
    });
    const auto upper_end = b.m.bind_mesh.indices.size();
    const auto top = [&](float xx, float zz) {
      float high = -1.f;
      for (std::size_t t = upper_begin; t < upper_end; t += 3) {
        const auto &mesh = b.m.bind_mesh;
        const Vec3 p = mesh.vertices[mesh.indices[t]].position * (1 / b.h),
                   q = mesh.vertices[mesh.indices[t + 1]].position * (1 / b.h),
                   r = mesh.vertices[mesh.indices[t + 2]].position * (1 / b.h);
        const float d = (q.x - p.x) * (r.z - p.z) - (q.z - p.z) * (r.x - p.x);
        if (std::fabs(d) < 1e-12f)
          continue;
        const float u = ((xx - p.x) * (r.z - p.z) - (zz - p.z) * (r.x - p.x)) /
                        d,
                    v = ((q.x - p.x) * (zz - p.z) - (q.z - p.z) * (xx - p.x)) /
                        d;
        if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f)
          high = std::max(high, p.y + u * (q.y - p.y) + v * (r.y - p.y));
      }
      if (high < -.5f || high > -.40f)
        throw std::runtime_error(
            "Syndicate Enforcer boot detail has no actual upper support");
      return high;
    };
    // The shaped upper is the visible boot surface. Do not place a separate
    // open tongue plane through the trouser hem; closed laces sit forward of
    // it.
    for (int lace = 0; lace < (b.near ? 4 : 0); ++lace) {
      const float z = .047f + lace * .010f;
      std::vector<Vec3> line;
      for (int j = 0; j <= 4; ++j) {
        const float xx = x + lerp(-.0105f, .0105f, float(j) / 4);
        line.push_back({xx, top(xx, z) + .0015f, z});
      }
      b.curve(line, .00058f, Vec3{.63f,.62f,.54f}*.75f, S::Shirt, foot,
              b.near ? 6 : 4);
    }
  }
}

void make_enforcer_trousers(Sculpt &b) {
  // Original pair-of-pants topology. Each half of one waist loop joins one leg
  // through a shared curved crotch seam. There is no intersecting seat shell
  // or exposed horizontal thigh top under the tucked shirt.
  using enforcer_jacket_detail::Pattern;
  Pattern pattern{b, {}, {}, {}};
  const Vec3 cloth{.095f,.13f,.175f};
  const int half = b.near ? 16 : 6, rise = b.near ? 8 : 4;
  const int count = half + rise;
  const int hip_rows = b.near ? 6 : 3, leg_rows = b.near ? 25 : 12;
  const float rx = .099f, rz = .070f, top = .048f;
  std::vector<std::uint32_t> waist(2 * half), crotch(rise + 1);
  for (int i = 0; i < 2 * half; ++i) {
    const float a = pi * .5f + pi * float(i) / half;
    waist[i] = pattern.node({rx * std::cos(a), top, rz * std::sin(a)}, cloth,
                            wt(B::Pelvis));
  }
  crotch.front() = waist[half];
  crotch.back() = waist[0];
  for (int i = 1; i < rise; ++i) {
    const float t = float(i) / rise;
    const float z = lerp(-rz, rz, t);
    const float y =
        top - .113f * std::pow(std::max(0.f, std::sin(pi * t)), .70f);
    const float support =
        .65f * std::pow(std::max(0.f, std::sin(pi * t)), 1.35f);
    crotch[i] =
        pattern.node({0, y, z}, cloth,
                     {std::uint8_t(B::Pelvis), std::uint8_t(B::LeftThigh),
                      1 - support, std::uint8_t(B::RightThigh), support * .5f});
  }
  const auto tex = [&](float u, float v) {
    return character_surface_uv(S::Pants, .02f + u, .02f + .94f * v);
  };
  for (float sign : {-1.f, 1.f}) {
    const B thigh = sign < 0 ? B::LeftThigh : B::RightThigh;
    const B shin = sign < 0 ? B::LeftShin : B::RightShin;
    std::vector<std::uint32_t> root;
    if (sign < 0) {
      for (int i = 0; i < half; ++i)
        root.push_back(waist[i]);
      for (int i = 0; i < rise; ++i)
        root.push_back(crotch[i]);
    } else {
      for (int i = half; i < 2 * half; ++i)
        root.push_back(waist[i]);
      for (int i = rise; i > 0; --i)
        root.push_back(crotch[i]);
    }
    std::vector<float> phase(count);
    for (int i = 0; i < count; ++i) {
      const Vec3 p = pattern.points[root[i]].p;
      phase[i] = std::atan2(p.z, p.x - sign * .061f);
    }
    auto previous = root;
    for (int j = 1; j <= hip_rows + leg_rows; ++j) {
      std::vector<std::uint32_t> next(count);
      for (int i = 0; i < count; ++i) {
        const float a = phase[i], cs = std::cos(a), sn = std::sin(a);
        Vec3 p{};
        CharacterInfluence weights{};
        if (j <= hip_rows) {
          const float t = float(j) / hip_rows;
          const Vec3 first = pattern.points[root[i]].p;
          const Vec3 end{sign * .061f + .040f * cs, -.105f, .044f * sn - .002f};
          p = first * (1 - t) + end * t;
          // Broad hip/seat ease, concentrated below the waist rather than a
          // separate capsule. The shared rise stays attached to the pelvis.
          p.x += sign * .0030f * std::sin(pi * t) * std::max(0.f, sign * cs);
          p.z -= .0035f * std::sin(pi * t) * std::max(0.f, -sn);
          const auto start = pattern.points[root[i]].w;
          const float amount = smooth(t / .82f);
          const float support = start.third == std::uint8_t(B::RightThigh)
                                    ? 2 * start.third_weight
                                    : 0.f;
          const float pelvis = (1 - support) * (1 - amount);
          const float right =
              support * .5f * (1 - amount) + (sign > 0 ? amount : 0.f);
          weights = {std::uint8_t(B::Pelvis), std::uint8_t(B::LeftThigh),
                     pelvis, std::uint8_t(B::RightThigh),
                     std::min(right, 1 - pelvis)};
        } else {
          const float near_y[]{-.105f, -.121f, -.139f, -.157f, -.173f, -.185f,
                               -.196f, -.205f, -.215f, -.225f, -.235f, -.245f,
                               -.255f, -.267f, -.280f, -.295f, -.313f, -.331f,
                               -.350f, -.370f, -.391f, -.412f, -.431f, -.446f,
                               -.453f, -.459f};
          const float far_y[]{-.105f, -.170f, -.195f, -.210f, -.225f,
                              -.240f, -.255f, -.280f, -.340f, -.400f,
                              -.435f, -.450f, -.459f};
          const int row = j - hip_rows;
          const float y = b.near ? near_y[row] : far_y[row];
          float rad = .0305f + .0075f * smooth((y + .340f) / .240f);
          rad -= .003f * smooth((-.405f - y) / .054f);
          rad += .0012f * std::pow(std::max(0.f, sn), 14.f);
          rad += .0012f * g(y, -.224f, .027f) * std::sin(y * 220 + a) +
                 .0009f * g(y, -.443f, .023f) * std::sin(y * 310 + a * 2);
          // Match the first leg ring exactly to the last rise ring, then
          // transition smoothly to the independently shaped calf/knee.
          const float blend = smooth((-.105f - y) / .065f);
          const float rx = lerp(.040f, rad, blend),
                      rz = lerp(.044f, rad * 1.07f, blend);
          p = {sign * .061f + rx * cs, y,
               rz * sn - .002f +
                   .010f * g(y, -.225f, .19f) * std::max(0.f, -sn) * blend};
          const float span = .10f + .12f * std::max(0.f, -sn),
                      low = -.225f - .5f * span;
          weights = y < low ? wt(shin)
                    : y < low + span
                        ? wt(shin, thigh, 1 - smooth((y - low) / span))
                        : wt(thigh);
          // Hem ease clears the real boot instep while keeping the original
          // narrow calf width. The bottom cuff follows the ankle support.
          p.z += .008f * smooth((-.425f - y) / .025f) *
                 std::pow(std::max(0.f, sn), 2.f);
          p.x += .0015f * smooth((-.425f - y) / .025f) * cs;
          if (y < -.405f)
            weights = wt(shin, sign < 0 ? B::LeftFoot : B::RightFoot,
                         1 - smooth((-.405f - y) / .030f));
        }
        // Posterior ease is narrower than the broad front/side drape. This
        // leaves the calf/hamstring contact envelope clear in deep crouch.
        p.z +=
            (.0120f * g(p.y, -.095f, .070f) + .0045f * g(p.y, -.360f, .075f)) *
            std::max(0.f, -sn);
        next[i] = pattern.node(p, cloth, weights);
      }
      const float v0 = float(j - 1) / (hip_rows + leg_rows),
                  v1 = float(j) / (hip_rows + leg_rows);
      for (int i = 0; i < count; ++i) {
        const int k = (i + 1) % count;
        const float u0 = .94f * float(i) / count,
                    u1 = .94f * float(i + 1) / count;
        // The ring flows down: opposite quad order from an upward torso.
        pattern.quad(previous[i], previous[k], next[k], next[i], tex(u0, v0),
                     tex(u1, v0), tex(u1, v1), tex(u0, v1));
      }
      previous = std::move(next);
    }
    // Narrow ankle return gives the trouser cuff fabric thickness.
    for (int row = 0; row < 2; ++row) {
      std::vector<std::uint32_t> next(count);
      for (int i = 0; i < count; ++i) {
        const P q = pattern.points[previous[i]];
        const float a = phase[i];
        next[i] =
            pattern.node(q.p + Vec3{-.0011f * std::cos(a), row ? .005f : 0,
                                    -.0011f * std::sin(a)},
                         cloth * .94f, q.w);
      }
      for (int i = 0; i < count; ++i) {
        const int k = (i + 1) % count;
        const float u0 = .94f * float(i) / count,
                    u1 = .94f * float(i + 1) / count;
        pattern.quad(previous[i], previous[k], next[k], next[i], tex(u0, .98f),
                     tex(u1, .98f), tex(u1, .99f), tex(u0, .99f));
      }
      previous = std::move(next);
    }
  }
  // A folded waistband stands just outside the tucked service shirt.
  std::vector<std::uint32_t> previous = waist;
  for (int row = 1; row <= 4; ++row) {
    std::vector<std::uint32_t> next(2 * half);
    for (int i = 0; i < 2 * half; ++i) {
      const float a = pi * .5f + pi * float(i) / half;
      const float y = top + .004f * std::min(row, 3);
      const float inset = row == 4 ? .0012f : 0;
      next[i] = pattern.node(
          {(rx - inset) * std::cos(a), y, (rz - inset) * std::sin(a)},
          cloth * .92f, wt(B::Pelvis));
    }
    for (int i = 0; i < 2 * half; ++i) {
      int k = (i + 1) % (2 * half);
      float u0 = .94f * i / (2 * half), u1 = .94f * (i + 1) / (2 * half);
      pattern.quad(previous[i], next[i], next[k], previous[k], tex(u0, .01f),
                   tex(u0, .03f), tex(u1, .03f), tex(u1, .01f));
    }
    previous = std::move(next);
  }
  pattern.finish();
}

CharacterRig enforcer_rig(float height) {
  CharacterRig rig;
  rig.height = height;
  auto joint = [&](B bone, int parent, Vec3 p) {
    rig.joints[bone_index(bone)] = {parent, p * height};
  };
  auto child = [&](B bone, B parent, Vec3 p) {
    joint(bone, int(bone_index(parent)), p);
  };
  joint(B::Pelvis, -1, {0, 0, 0});
  child(B::Spine, B::Pelvis, {0, .13f, 0});
  child(B::Chest, B::Spine, {0, .272f, 0});
  child(B::Neck, B::Chest, enforcer_anatomy::neck_center);
  // Fit the skull rotation pivot between the source neck and head helpers,
  // at the cranio-cervical attachment rather than inside the cheek volume.
  child(B::Head, B::Neck, enforcer_anatomy::neck_center * .35f +
                           enforcer_anatomy::head_center * .65f);
  for (float sign : {-1.f, 1.f}) {
    const bool right = sign > 0;
    const B arm = right ? B::RightUpperArm : B::LeftUpperArm;
    const B fore = right ? B::RightForearm : B::LeftForearm;
    const B hand = right ? B::RightHand : B::LeftHand;
    const B thigh = right ? B::RightThigh : B::LeftThigh;
    const B shin = right ? B::RightShin : B::LeftShin;
    const B foot = right ? B::RightFoot : B::LeftFoot;
    child(arm, B::Chest, {sign * .134f, .310f, 0});
    child(fore, arm, {sign * .161f, .12f, .008f});
    child(hand, fore, {sign * .184f, -.044f, .018f});
    child(thigh, B::Pelvis, {sign * .061f, 0, 0});
    child(shin, thigh, {sign * .061f, -.225f, 0});
    child(foot, shin, {sign * .061f, -.46f, 0});
  }
  return rig;
}

float neck_fit_radius(float y, float angle, float center_z) {
  // .34-.355 are closed neck contours. Higher full-head rays can hit the jaw;
  // continue the measured neck taper instead of fitting the collar to the chin.
  const float lower = std::max(y, .340f);
  if (lower <= .355f)
    return anatomy_radius(lower, angle, center_z);
  const float a = anatomy_radius(.350f, angle, center_z);
  const float c = anatomy_radius(.355f, angle, center_z);
  return std::max(.012f, c + (c - a) * ((lower - .355f) / .005f));
}

// A closed elliptical tube has two periodic seams and no coincident end
// caps. It is used for the tied root and finite pad-retention bands.
void closed_ellipse_loop(Sculpt& b,Vec3 center,Vec3 u_axis,Vec3 v_axis,
                         float rx,float ry,float thickness,Vec3 color,
                         S surface,B bone,int columns=24,int sides=4) {
  const auto base=std::uint32_t(b.m.bind_mesh.vertices.size());
  const Vec3 normal=-cross(u_axis,v_axis);
  b.grid(columns,sides,[&](float u,float v){
    const float a=2*pi*(u==1.f?0.f:u),c=std::cos(a),sn=std::sin(a);
    const float e=2*pi*(v==1.f?0.f:v);
    const Vec3 radial=normalize(u_axis*c+v_axis*sn);
    return P{center+u_axis*(rx*c)+v_axis*(ry*sn)+
      radial*(thickness*std::cos(e))+normal*(thickness*std::sin(e)),
      color,wt(bone),character_surface_uv(surface,u,v)};
  },true,false);
  for(int i=0;i<=columns;++i)b.smooth_seams.push_back({base+i,base+sides*(columns+1)+i});
}

// Individually fitted chestnut tied hair. Every section queries this
// actor's own morphed scalp; a small continuous backing supports the bun.
void make_tied_hair(Sculpt &b) {
  const Vec3 hair=b.m.appearance.hair;
  std::map<unsigned,unsigned> emitted;
  const auto emit=[&](const auto& indices){
    for(std::size_t t=0;t<indices.size();t+=3) {
      unsigned corner[3];
      for(int k=0;k<3;++k) {
        const unsigned source=indices[t+k];
        auto found=emitted.find(source);
        if(found==emitted.end()) {
          const Vec3 p=enforcer_hair::positions[source];
          const float a=std::atan2(p.z-enforcer_anatomy::head_center.z,p.x);
          const float flow=.965f+.025f*std::sin(a*17.f+p.y*31.f);
          const auto id=b.vertex({p,hair*flow,wt(B::Head),character_surface_uv(S::HairCrop,
            std::clamp(.5f+p.x/.15f,0.f,1.f),std::clamp((p.y-.405f)/.095f,0.f,1.f))});
          found=emitted.emplace(source,id).first;
        }
        corner[k]=found->second;
      }
      b.tri(corner[0],corner[1],corner[2]);
    }
  };
  if(b.near)emit(enforcer_hair::near_indices);else emit(enforcer_hair::far_indices);
  // A few supported longitudinal locks follow the actual scalp into the tie.
  // The opaque backing stays complete; these are alpha-cutout strand accents,
  // not a strand simulation or loose tubes through empty space.
  if(b.near) for(float side:{-1.f,1.f}) for(int lock=0;lock<3;++lock) {
    const auto begin=b.m.bind_mesh.indices.size();
    b.grid(2,16,[&](float u,float v){
      const float da=side*(pi-.16f-.28f-lock*.23f);
      const float dy=.024f*pi*std::cos(pi*v)-.012f;
      const float tangent=std::sqrt(sq(.05f*da)+sq(dy));
      const float width=(u-.5f)*.0034f;
      const float a=pi*.5f+side*lerp(.28f+lock*.23f,pi-.16f,v)-dy/tangent*width/.05f;
      const float y=.454f+.024f*std::sin(pi*v)-.012f*v+.05f*da/tangent*width;
      const float cz=enforcer_anatomy::head_center.z;
      const float radius=radial_intersection(enforcer_hair::positions,enforcer_hair::near_indices,y,a,cz);
      if(radius<=0)throw std::runtime_error("Enforcer gathered lock lacks actual scalp support");
      return P{{(radius+.00065f)*std::cos(a),y,cz+(radius+.00065f)*std::sin(a)},
        hair*.97f,wt(B::Head),character_surface_uv(S::HairStrands,u,v)};
    },false,false);
    for(auto t=begin;t<b.m.bind_mesh.indices.size();t+=3) {
      auto& mesh=b.m.bind_mesh;auto i=mesh.indices[t],j=mesh.indices[t+1],k=mesh.indices[t+2];
      const auto a=mesh.vertices[i].position,c=mesh.vertices[j].position,d=mesh.vertices[k].position;
      const auto center=(a+c+d)*(1.f/3.f);
      const Vec3 radial{center.x,0,center.z-enforcer_anatomy::head_center.z*b.h};
      if(dot(cross(c-a,d-a),radial)<0)std::swap(mesh.indices[t+1],mesh.indices[t+2]);
    }
  }
  // Gather the rear volume directly against this scalp, with a continuous
  // corrugated shell instead of detached curves crossing empty space.
  const float scalp_back=enforcer_anatomy::head_center.z-
      anatomy_radius(.442f,1.5f*pi,enforcer_anatomy::head_center.z);
  const Vec3 root{0,.442f,scalp_back-.019f};
  b.grid(b.near?28:10,b.near?12:5,[&](float u,float v){
    const float a=2*pi*u,e=lerp(-.49f*pi,.49f*pi,v);
    const float ridge=.00045f*std::sin(7*a+3*e);
    return P{root+Vec3{(.021f+ridge)*std::cos(a)*std::cos(e),
      (.023f+ridge)*std::sin(e),(.027f+ridge)*std::sin(a)*std::cos(e)},
      hair*(.91f+.022f*std::sin(7*a+3*e)),wt(B::Head),character_surface_uv(S::HairCrop,u,v)};
  });
  // A small matte tie lies on the actual gathered root section.
  const float local_z=.014f, section=std::sqrt(1-sq(local_z/.027f));
  if(b.near) closed_ellipse_loop(b,{0,.442f,root.z+local_z},{1,0,0},{0,1,0},
      .021f*section,.023f*section,.0012f,{.055f,.047f,.038f},S::Rubber,B::Head,24,4);

}

// Original beveled equipment box, with quiet rounded corners and finite sides.
void equipment_box(Sculpt &b, Vec3 center, Vec3 half, Vec3 color, S surface,
                   B bone) {
  const int n = b.near ? 16 : 8;
  b.grid(
      n, 2,
      [&](float u, float v) {
        const float a = 2 * pi * (u == 1.f ? 0.f : u), cs = std::cos(a),
                    sn = std::sin(a);
        const float bevel = .88f + .12f * std::sin(pi * v);
        const auto super = [](float q) {
          return std::copysign(std::pow(std::fabs(q), .35f), q);
        };
        return P{center + Vec3{half.x * super(cs) * bevel,
                               -half.y * super(sn) * bevel,
                               lerp(-half.z, half.z, v)},
                 color, wt(bone), character_surface_uv(surface, u, v)};
      },
      true, true);
}

// Cut each shoulder strip from the actual sewn yoke triangles. An independent
// rectangular sampling grid chords across the convex shoulder and can cut its
// support even when all sampled points are above the shirt. Preserving the
// source triangulation avoids that hidden between-sample defect.
void fitted_uniform_patch(Sculpt &b, unsigned support_end, float side,
                          Vec3 color, bool panel = false, bool back = false,
                          int rib = 0, bool insert = false, bool pad = false) {
  const auto &mesh = b.m.bind_mesh;
  struct V {
    Vec3 p, n;
    CharacterInfluence w;
  };
  using Key = std::array<long long, 3>;
  const auto key = [](Vec3 p) {
    return Key{std::llround(p.x * 1e6), std::llround(p.y * 1e6),
               std::llround(p.z * 1e6)};
  };
  std::map<Key, Vec3> smooth_normals;
  for (unsigned t = 0; t < support_end; t += 3) {
    const auto a = mesh.vertices[mesh.indices[t]].position * (1 / b.h),
               c = mesh.vertices[mesh.indices[t + 1]].position * (1 / b.h),
               d = mesh.vertices[mesh.indices[t + 2]].position * (1 / b.h);
    const Vec3 n = cross(c - a, d - a);
    for (auto p : {a, c, d})
      smooth_normals[key(p)] += n;
  }
  const auto blend = [&](const V &a, const V &c, float f) {
    V r{a.p * (1 - f) + c.p * f, normalize(a.n * (1 - f) + c.n * f), {}};
    std::array<float, character_bone_count> mass{};
    const auto add = [&](const CharacterInfluence &w, float scale) {
      mass[w.first] += scale * w.first_weight;
      mass[w.second] += scale * (1 - w.first_weight - w.third_weight);
      mass[w.third] += scale * w.third_weight;
    };
    add(a.w, 1 - f);
    add(c.w, f);
    std::array<unsigned, character_bone_count> order{};
    for (unsigned i = 0; i < order.size(); ++i)
      order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](unsigned x, unsigned y) { return mass[x] > mass[y]; });
    const float sum = mass[order[0]] + mass[order[1]] + mass[order[2]];
    if (sum < .99999f)
      throw std::runtime_error(
          "Syndicate Enforcer shoulder interpolation exceeds three bones");
    r.w = {std::uint8_t(order[0]), std::uint8_t(order[1]), mass[order[0]] / sum,
           std::uint8_t(order[2]), mass[order[2]] / sum};
    return r;
  };
  std::vector<V> points;
  std::vector<std::array<unsigned, 3>> faces;
  std::map<Key, std::vector<unsigned>> cells;
  const auto node = [&](V value) {
    const auto k = key(value.p);
    for (int dx = -1; dx <= 1; ++dx)
      for (int dy = -1; dy <= 1; ++dy)
        for (int dz = -1; dz <= 1; ++dz) {
          auto found = cells.find(Key{k[0] + dx, k[1] + dy, k[2] + dz});
          if (found == cells.end())
            continue;
          for (auto id : found->second)
            if (length(points[id].p - value.p) < 1e-7f)
              return id;
        }
    const unsigned id = points.size();
    points.push_back(value);
    cells[k].push_back(id);
    return id;
  };
  const float lo = .062f, hi = .081f;
  const auto flare = [](float y) { return .20f * (y - .275f); };
  for (unsigned t = 0; t < support_end; t += 3) {
    std::vector<V> poly;
    for (int c = 0; c < 3; ++c) {
      const auto id = mesh.indices[t + c];
      const auto p = mesh.vertices[id].position * (1 / b.h);
      poly.push_back(
          {p, normalize(smooth_normals[key(p)]), b.m.influences[id]});
    }
    for (int plane = 0; plane < (pad ? 5
                                 : insert  ? 5
                                 : rib ? 4
                                 : panel ? 5
                                         : 3) &&
                        !poly.empty();
         ++plane) {
      const auto distance = [&](const V &v) {
        if(pad) {
          return plane==0 ? side*v.p.x-.108f : plane==1 ? v.p.y-.263f
               : plane==2 ? .346f-v.p.y : plane==3 ? v.p.z+.042f
               : .046f-v.p.z;
        }
        if(insert) {
          const float bevel=.005f*std::clamp((std::fabs(v.p.y-.157f)-.017f)/.006f,0.f,1.f);
          return plane==0 ? v.p.y-.134f : plane==1 ? .180f-v.p.y
               : plane==2 ? side*v.p.x-.014f-bevel
               : plane==3 ? .078f-bevel-side*v.p.x : v.p.z;
        }
        if (rib) {
          const float center = rib == 1 ? .109f : .177f;
          const float span = side * v.p.x * std::tan(pi * .31f);
          return plane == 0   ? v.p.y - (center - .011f)
                 : plane == 1 ? center + .011f - v.p.y
                 : plane == 2 ? span + v.p.z
                              : span - v.p.z;
        }
        if (!panel)
          return plane == 0   ? side * v.p.x - lo - flare(v.p.y)
                 : plane == 1 ? hi + flare(v.p.y) - side * v.p.x
                              : v.p.y - .275f;
        const float top = back ? .302f : .292f,
                    half = .083f +
                           .012f * std::clamp((v.p.y - .074f) / (top - .074f),
                                              0.f, 1.f);
        return plane == 0   ? v.p.y - .074f
               : plane == 1 ? top - v.p.y
               : plane == 2 ? v.p.x + half
               : plane == 3 ? half - v.p.x
                            : (back ? -v.p.z : v.p.z);
      };
      std::vector<V> out;
      V previous = poly.back();
      float dp = distance(previous);
      for (const auto &current : poly) {
        const float dc = distance(current);
        if ((dc >= 0) != (dp >= 0)) {
          auto v = blend(previous, current, dp / (dp - dc));
          out.push_back(v);
        }
        if (dc >= 0)
          out.push_back(current);
        previous = current;
        dp = dc;
      }
      poly = std::move(out);
    }
    if (poly.size() < 3)
      continue;
    const auto first = node(poly[0]);
    for (unsigned i = 1; i + 1 < poly.size(); ++i) {
      const auto a = node(poly[i]), c = node(poly[i + 1]);
      if (first == a || first == c || a == c)
        continue;
      if (length(cross(points[a].p - points[first].p,
                       points[c].p - points[first].p)) > 1e-12f)
        faces.push_back({first, a, c});
    }
  }
  std::map<std::pair<unsigned, unsigned>,
           std::vector<std::pair<unsigned, unsigned>>>
      edges;
  for (const auto &f : faces)
    for (int c = 0; c < 3; ++c) {
      const auto a = f[c], d = f[(c + 1) % 3];
      edges[std::minmax(a, d)].push_back({a, d});
    }
  const auto point = [&](unsigned id, int inner) {
    const auto &q = points[id];
    const float u =
        rib     ? (std::atan2(q.p.z, side * q.p.x) + pi * .31f) / (pi * .62f)
        : panel ? (q.p.x + .080f) / .160f
                : (side * q.p.x - lo - flare(q.p.y)) / (hi - lo);
    const float v = rib     ? (q.p.y - (rib == 1 ? .098f : .166f)) / .022f
                    : panel ? (q.p.y - .074f) / .23f
                            : (std::atan2(q.p.z, q.p.y - .260f) + 1.6f) / 3.2f;
    const float pad_lift=.004f+.009f*g(q.p.y,.302f,.035f)*g(side*q.p.x,.147f,.048f)*g(q.p.z,.002f,.042f);
    const float distance = pad ? (inner ? .0025f : pad_lift)
                           : insert ? (inner ? .0128f : .0160f)
                           : rib     ? (inner ? .0034f : .0048f)
                           : panel ? (inner ? .0054f : .010f)
                                   : (inner ? .0025f : .0040f);
    return P{q.p + q.n * distance, color * (inner ? .9f : 1.f), q.w,
             character_surface_uv(S::Jacket, std::clamp(u, 0.f, 1.f),
                                  std::clamp(v, 0.f, 1.f))};
  };
  std::array<std::vector<unsigned>, 2> layer;
  for (int inner = 0; inner < 2; ++inner)
    for (unsigned i = 0; i < points.size(); ++i)
      layer[inner].push_back(b.vertex(point(i, inner)));
  for (const auto &f : faces) {
    b.tri(layer[0][f[0]], layer[0][f[1]], layer[0][f[2]]);
    b.tri(layer[1][f[0]], layer[1][f[2]], layer[1][f[1]]);
  }
  for (const auto &e : edges) {
    if (e.second.size() != 1)
      continue;
    const auto a = e.second[0].first, c = e.second[0].second;
    const unsigned oa = b.vertex(point(a, 0)), oc = b.vertex(point(c, 0)),
                   ia = b.vertex(point(a, 1)), ic = b.vertex(point(c, 1));
    b.tri(oc, oa, ia);
    b.tri(oc, ia, ic);
  }
}

void make_vest_and_equipment(Sculpt &b, EnforcerSupportRanges ranges) {
  const unsigned shirt_end=b.m.bind_mesh.indices.size();

  const Vec3 vest{.12f,.14f,.16f}, webbing{.055f,.065f,.075f};
  // These two finite carrier panels preserve the actual body's triangulation
  // and support weights, including the upper-arm influence around armholes.
  for (bool back : {false, true})
    fitted_uniform_patch(b, ranges.body_end, 1.f, vest, true, back);
  // Four finite rib bands are cut from the same support triangles, so their
  // close fit survives arm swing instead of chording through coarser cloth.
  for (float side : {-1.f, 1.f})
    for (int rib : {1, 2})
      fitted_uniform_patch(b, ranges.body_end, side, webbing, false, false,
                           rib);
  // Each finite strap continues from the front carrier over the actual
  // shoulder surface and down into the rear carrier. No floating end tabs.
  for (float side : {-1.f, 1.f})
    fitted_uniform_patch(b, ranges.yoke_end, side, vest);
  // The two original accent inserts are cut from the same actual support
  // triangulation as the carrier, then given finite bevel-like side returns.
  for(float side:{-1.f,1.f})
    fitted_uniform_patch(b,ranges.body_end,side,b.m.appearance.accent*.55f,false,false,0,true);
  // Rounded padding is fitted to this actor's actual sewn deltoid/sleeve
  // root and inherits each supporting triangle's weights. A finite inner skin
  // and closed perimeter give thickness without a floating spherical pad.
  for(float side:{-1.f,1.f})
    fitted_uniform_patch(b,shirt_end,side,webbing,false,false,0,false,true);
  // The belt follows the narrow waist instead of intersecting a rectangular
  // hip.
  b.grid(
      b.near ? 48 : 16, 4,
      [&](float u, float v) {
        const int row = std::min(4, int(std::lround(v * 4)));
        const float levels[5]{.036f, .057f, .057f, .036f, .036f};
        const float inset[5]{0, 0, .0014f, .0014f, 0};
        const float a = 2 * pi * (u == 1.f ? 0.f : u), y = levels[row];
        Vec3 p = enforcer_jacket_detail::body_point(a, y);
        p.x += .0060f * std::cos(a);
        p.z += .0060f * std::sin(a);
        p.x = std::copysign(
            std::max(std::fabs(p.x), .1020f * std::fabs(std::cos(a))),
            std::cos(a));
        p.z = std::copysign(
            std::max(std::fabs(p.z), .0750f * std::fabs(std::sin(a))),
            std::sin(a));
        p.x -= inset[row] * std::cos(a);
        p.z -= inset[row] * std::sin(a);
        return P{p, webbing, enforcer_jacket_detail::body_weight(y),
                 character_surface_uv(S::Leather, u, v)};
      },
      true, false);
  equipment_box(b, {0, .0465f, .0768f}, {.0115f, .0085f, .0018f},
                {.55f, .56f, .51f}, S::Metal, B::Pelvis);
  // A small back keeper makes the rear construction visible and functional.
  equipment_box(b, {.024f, .045f, -.0767f}, {.006f, .014f, .0015f}, webbing,
                S::Leather, B::Pelvis);
}
} // namespace

CharacterProfile make_enforcer_character_profile(float height,
                                                    CharacterRole role,
                                                    std::string_view identity,
                                                    CharacterLod lod) {
  if (!std::isfinite(height) || height <= 0.f ||
      (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid Syndicate Enforcer profile request");
  CharacterProfile profile;
  auto &m = profile.model;
  m.role = role;
  m.lod = lod;
  m.seed = character_seed(identity);
  m.appearance = character_appearance(m.seed);
  m.rig = enforcer_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 32000 : 15000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 150000 : 80000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  const auto shirt_body_end = make_enforcer_jacket(
      b, neck_fit_radius, enforcer_anatomy::neck_center);
  make_vest_and_equipment(b, shirt_body_end);
  make_enforcer_trousers(b);
  make_boots(b);
  make_hands_and_forearms(b);
  make_head(b);
  make_tied_hair(b);
  b.finish();
  profile.material = make_character_surface_material();
  profile.material.alpha_cutoff = .4f;
  profile.lod_distance = 12.f;
  profile.individualized = true;
  return profile;
}
} // namespace fury
