#include "character_pax_shirt.hpp"
#include "character_pax_vest.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/pax_anatomy.hpp"
#include "fury/generated/pax_hands.hpp"
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
  for (std::size_t i = 0; i < pax_anatomy::indices.size(); i += 3) {
    const Vec3 a = pax_anatomy::positions[pax_anatomy::indices[i]];
    const Vec3 b = pax_anatomy::positions[pax_anatomy::indices[i + 1]];
    const Vec3 c = pax_anatomy::positions[pax_anatomy::indices[i + 2]];
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
float anatomy_radius(float y, float angle, float center_z) {
  const Vec3 origin{0, y, center_z},
      direction{std::cos(angle), 0, std::sin(angle)};
  float best = 0.f;
  for (std::size_t i = 0; i < pax_anatomy::indices.size(); i += 3) {
    const Vec3 a = pax_anatomy::positions[pax_anatomy::indices[i]];
    const Vec3 b = pax_anatomy::positions[pax_anatomy::indices[i + 1]];
    const Vec3 c = pax_anatomy::positions[pax_anatomy::indices[i + 2]];
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
CharacterInfluence pax_neck_weight(float y) {
  return y < .357f ? wt(B::Neck, B::Chest, smooth((y - .333f) / .024f))
                   : wt(B::Head, B::Neck, smooth((y - .357f) / .027f));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      pax_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < pax_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = pax_anatomy::indices[triangle + corner];
      const auto uv_index = pax_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = pax_anatomy::positions[index];
        const Vec2 uv = pax_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .39130f, .0035f) *
                          smooth((position.z - .078f) / .010f);
        // Small anatomical pigment zones retain the stable base complexion.
        const float cheeks = g(std::fabs(position.x), .030f, .015f) *
                             g(position.y, .404f, .015f) *
                             smooth((position.z - .067f) / .02f);
        const float nose = g(position.x, 0, .009f) *
                           g(position.y, .408f, .012f) *
                           smooth((position.z - .081f) / .013f);
        const float front = smooth((position.z - .054f) / .025f);
        const float vascular =
            .5f + .25f * std::sin(position.x * 360 + position.y * 97) +
            .25f * std::sin(position.y * 285 - position.z * 190);
        const float lid = g(std::fabs(position.x), .0178f, .008f) *
                          g(position.y, .423f, .006f) * front;
        Vec3 color = skin * (1 - .025f * vascular * front);
        color.x *= 1 - .015f * lid;
        color.y *= 1 - .055f * lid;
        color.z *= 1 - .037f * lid;
        color.y *= 1 - .052f * cheeks - .043f * nose;
        color.z *= 1 - .077f * cheeks - .054f * nose;
        color.y *= 1 - .22f * lip;
        color.z *= 1 - .19f * lip;
        const auto vertex =
            b.vertex({position, color, pax_neck_weight(position.y),
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
  auto boundary = pax_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = pax_anatomy::positions[boundary[i]];
    const Vec3 c = pax_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = pax_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, pax_neck_weight(point.y),
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
  for (std::size_t ei = 0; ei < pax_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = pax_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? .0011f : -.0011f, -.0010f, -.00160f};
    const Vec3 radius = pax_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 8 : 6);
    if (b.near) {
      std::vector<Vec3> upper_lid, lower_lid;
      const auto &rim = ei == 0 ? pax_anatomy::left_eye_opening
                                : pax_anatomy::right_eye_opening;
      for (Vec3 p : rim) {
        if (std::fabs(p.x - eye.x) > .0055f)
          continue;
        p.z += .00010f;
        (p.y >= .43078f ? upper_lid : lower_lid).push_back(p);
      }
      for (auto *line : {&upper_lid, &lower_lid})
        std::sort(line->begin(), line->end(),
                  [](Vec3 a, Vec3 c) { return a.x < c.x; });
      const Vec3 rim_color{skin.x * .85f, skin.y * .74f, skin.z * .72f};
      if (lower_lid.size() > 1)
        b.curve(lower_lid, .00010f, rim_color, S::Eyes, B::Head, 6, .6f);
      if (upper_lid.size() > 1)
        b.curve(upper_lid, .00009f, rim_color * .80f, S::Eyes, B::Head, 6, .6f);
    }
    const int iris_columns = b.near ? 24 : 8, iris_rows = b.near ? 5 : 3;
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
    const int count = b.near ? 24 : 8;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, x = eye.x + lerp(-.0118f, .0118f, t),
            outward = eye.x > 0 ? t : 1 - t,
            y = eye.y + .0128f + .0019f * std::sin(pi * outward) -
                .0013f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00012f + .00095f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .74f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 6 : 4);
  }
}

// Pax's relaxed bind turns each palm toward the thigh, with thumbs forward.
// This fixed rigid fitting is shared by source fingers and the sewn wrist ring;
// it is not a finger controller or per-frame hand animation.
Vec3 pax_hand_offset(Vec3 p,float side) {
  p.x*=side;const float a=side*1.04f,c=std::cos(a),sn=std::sin(a);
  return {c*p.x+sn*p.z,p.y,-sn*p.x+c*p.z};
}
void make_hands_and_forearms(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  for (float side : {-1.f, 1.f}) {
    const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
    const B upper = side > 0 ? B::RightUpperArm : B::LeftUpperArm;
    const B hand = side > 0 ? B::RightHand : B::LeftHand;
    const Vec3 wrist{side * .179f, -.043f, .008f};
    std::map<std::pair<unsigned, unsigned>, unsigned> corners;
    std::vector<std::vector<unsigned>> groups(pax_hands::positions.size());
    const auto emit = [&](auto const &indices, auto const &uv_indices) {
      for (std::size_t t = 0; t < indices.size(); t += 3) {
        unsigned out[3];
        for (int k = 0; k < 3; ++k) {
          auto index = indices[t + k], uv = uv_indices[t + k];
          const bool hard = b.near
            ? std::find(pax_hands::hard_normal_triangle_indices.begin(),
                        pax_hands::hard_normal_triangle_indices.end(), t/3) != pax_hands::hard_normal_triangle_indices.end()
            : std::find(pax_hands::far_hard_normal_triangle_indices.begin(),
                        pax_hands::far_hard_normal_triangle_indices.end(), t/3) != pax_hands::far_hard_normal_triangle_indices.end();
          auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) * pax_hands::uv_coordinates.size())
                         : 0));
          auto found = corners.find(key);
          if (found == corners.end()) {
            Vec3 p = pax_hand_offset(pax_hands::positions[index],side);
            p += wrist;
            const Vec2 tex = pax_hands::uv_coordinates[uv];
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
      emit(pax_hands::near_indices, pax_hands::near_uv_indices);
    else
      emit(pax_hands::far_indices, pax_hands::far_uv_indices);
    // Exactly reuse every anatomical wrist point at the terminal forearm ring.
    // The opposite terminal ring tucks inside the open rolled cuff, not a cap.
    auto boundary = pax_hands::wrist_boundary_indices;
    float area = 0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
      auto a = pax_hands::positions[boundary[i]],
           c = pax_hands::positions[boundary[(i + 1) % boundary.size()]];
      area += a.x * c.z - c.x * a.z;
    }
    if (area * side < 0)
      std::reverse(boundary.begin(), boundary.end());
    Vec3 center{};
    for (auto i : boundary)
      center += pax_hands::positions[i];
    center = center * (1.f / boundary.size());
    const unsigned start = b.m.bind_mesh.vertices.size();
    const int columns = boundary.size(), rows = b.near ? 16 : 4;
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
              boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = pax_hand_offset(pax_hands::positions[index],side);
          source += wrist;
          const Vec3 local = pax_hand_offset(pax_hands::positions[index] - center,side);
          const float a = std::atan2(local.z, local.x);
          const float y = lerp(source.y, .142f, v),
                      radius = .016f + .0130f * g(y, .078f, .071f);
          const float x = side * lerp(.179f, .165f, smooth((y + .043f) / .18f));
          Vec3 target{x + radius * std::cos(a), y,
                      .006f + radius * .83f * std::sin(a)};
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
void make_trousers_and_shoes(Sculpt &b) {
  const Vec3 cloth = b.m.appearance.trousers;
  const int columns = b.near ? 28 : 12, rows = b.near ? 24 : 9;
  std::array<std::vector<unsigned>, 2> seams;
  for (float side : {-1.f, 1.f}) {
    const B thigh = side > 0 ? B::RightThigh : B::LeftThigh,
            shin = side > 0 ? B::RightShin : B::LeftShin,
            foot = side > 0 ? B::RightFoot : B::LeftFoot;
    const auto base = unsigned(b.m.bind_mesh.vertices.size());
    b.grid(
        columns, rows,
        [&](float u, float v) {
          float a = 2 * pi * u, cs = std::cos(a), sn = std::sin(a),
                y = lerp(-.459f, -.09f, std::min(v / .78f, 1.f));
          float radius = .027f + .011f * g(y, -.34f, .073f) +
                         .022f * smooth((y + .24f) / .19f);
          const float fold =
              .0028f * g(y, -.22f + .012f * cs, .025f) * sn +
              .0022f * g(y, -.446f, .021f) * std::sin(2 * a + y * 210);
          Vec3 p{side * (.056f + (radius + fold) * cs), y,
                 (radius * 1.10f + fold) * sn - .002f};
          const float join = smooth((v - .78f) / .22f);
          const Vec3 top{side * .110f * std::max(0.f, cs),
                         .057f - .127f * std::max(0.f, -cs),
                         .071f * sn - .002f};
          p = p * (1 - join) + top * join;
          CharacterInfluence w =
              y < -.285f  ? wt(shin)
              : y < -.16f ? wt(shin, thigh, 1 - smooth((y + .285f) / .125f))
                          : wt(thigh, B::Pelvis, 1 - join);
          if (v == 1)
            w = wt(B::Pelvis);
          return P{p, cloth, w, character_surface_uv(S::Pants, u, v)};
        },
        true, false);
    // Mirroring reverses winding while preserving exact shared crotch edges.
    if (side < 0)
      for (std::size_t i = b.m.bind_mesh.indices.size() - columns * rows * 6;
           i < b.m.bind_mesh.indices.size(); i += 3)
        std::swap(b.m.bind_mesh.indices[i + 1], b.m.bind_mesh.indices[i + 2]);
    auto &seam = seams[side > 0 ? 1 : 0];
    for (int i = columns / 4; i <= 3 * columns / 4; ++i)
      seam.push_back(base + rows * (columns + 1) + i);
    const float x = side * .056f;
    b.grid(b.near ? 28 : 10, 3, [&](float u, float v) {
      float a = 2 * pi * u;
      return P{{x + .034f * std::cos(a), lerp(-.5f, -.484f, v),
                .032f + .071f * std::sin(a)},
               {.061f, .065f, .069f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    const auto boot_section=[](float v,const std::array<float,6>&values){
      int k=std::min(4,int(v*5));return lerp(values[k],values[k+1],smooth(v*5-k));
    };
    const auto upper_start=b.m.bind_mesh.indices.size();
    b.grid(b.near ? 32 : 12, b.near ? 10 : 4, [&](float u, float v) {
      float a = 2 * pi * u, yy = lerp(-.484f, -.416f, v);
      return P{{x + boot_section(v,{.034f,.035f,.032f,.026f,.024f,.025f}) * std::cos(a), yy,
                boot_section(v,{.032f,.032f,.024f,.007f,.001f,0.f}) +
                    boot_section(v,{.070f,.069f,.052f,.034f,.027f,.027f}) * std::sin(a)},
               b.m.appearance.shoes,
               wt(foot),
               character_surface_uv(S::Leather, u, v)};
    },true,b.near);
    const auto upper_end=b.m.bind_mesh.indices.size();
    auto shoe_top=[&](float xx,float zz) {
      float highest=-1;
      for(std::size_t i=upper_start;i<upper_end;i+=3) {
        Vec3 a=b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i]].position*(1/b.h),
             c=b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i+1]].position*(1/b.h),
             d=b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i+2]].position*(1/b.h);
        Vec3 e=c-a,f=d-a;float det=e.x*f.z-e.z*f.x;
        if(std::fabs(det)<1e-12f)continue;
        float u=((xx-a.x)*f.z-(zz-a.z)*f.x)/det,v=(e.x*(zz-a.z)-e.z*(xx-a.x))/det;
        if(u>=-1e-6f&&v>=-1e-6f&&u+v<=1.000001f)highest=std::max(highest,a.y+u*e.y+v*f.y);
      }
      if(highest<-.6f||highest>-.40f)throw std::runtime_error("Pax boot lace has no upper support");
      return highest;
    };
    if(b.near) b.grid(4,8,[&](float u,float v){float xx=x+lerp(-.013f,.013f,u),zz=lerp(.029f,.077f,v);return P{{xx,shoe_top(xx,zz)+.00085f,zz},b.m.appearance.shoes*.90f,wt(foot),character_surface_uv(S::Leather,u,v)};},false,false);
    // A shaped ankle underlay bridges the trouser opening to the moving shoe.
    // Its weights continue the shin-to-foot bend, avoiding an empty cuff gap.
    b.grid(
        b.near ? 20 : 12, b.near ? 6 : 3,
        [&](float u, float v) {
          float a = 2 * pi * u, y = lerp(-.477f, -.432f, v),
                rx = lerp(.024f, .0228f, v);
          return P{{x + rx * std::cos(a), y, .005f + rx * .93f * std::sin(a)},
                   Vec3{.071f, .078f, .083f},
                   wt(foot, shin, 1 - smooth((y + .469f) / .030f)),
                   character_surface_uv(S::Shirt, u, v)};
        },
        true, false);
    // Recessed dark laces follow the actual triangulated instep. Side and toe
    // inspection exposed buried laces on the taller early boot envelope.
    for(int lace=0;lace<(b.near?4:2);++lace) {
      float z=.035f+lace*(b.near?.010f:.020f);std::vector<Vec3> line;
      for(int j=0;j<=4;++j){float xx=x+lerp(-.013f,.013f,float(j)/4);line.push_back({xx,shoe_top(xx,z)+.0016f,z});}
      b.curve(line,.00065f,{.15f,.16f,.16f},S::Shirt,foot,b.near?6:4);
    }

  }
  for (std::size_t i = 0; i < seams[0].size(); ++i)
    b.normal_groups.push_back({seams[0][i], seams[1][i]});
  b.grid(
      b.near ? 48 : 20, 3,
      [&](float u, float v) {
        float a = 2 * pi * u;
        return P{{.1105f * std::cos(a), lerp(.0565f, .071f, v),
                  .072f * std::sin(a) - .002f},
                 cloth * .94f,
                 wt(B::Pelvis),
                 character_surface_uv(S::Pants, u, v)};
      },
      true, false);
}
float pax_hairline(float a) {
  const float front = std::max(0.f, std::sin(a)),
              rear = std::max(0.f, -std::sin(a));
  return .441f + .024f * front - .040f * rear +
         .006f * std::pow(std::fabs(std::cos(a)), 10.f) +
         .0008f * std::sin(5 * a + .7f);
}
Vec3 pax_scalp(float a, float t, float lift = 0) {
  const float y =
      lerp(pax_hairline(a), .5f, std::sin(std::clamp(t, 0.f, 1.f) * pi * .5f));
  const float cz =
      lerp(pax_anatomy::head_center.z, pax_anatomy::crown_joint.z,
           smooth((y - .456f) / .037f));
  const float source_y = y - .007f * smooth((y - .448f) / .052f);
  float r =
      anatomy_radius(std::min(source_y, .49295f), a, cz) +
      (.0028f + lift) *
          std::sqrt(std::max(0.f, 1 - sq(std::max(0.f, (y - .478f) / .022f))));
  r += .0007f * std::sin(a * 3 + t * 2) * std::sin(pi * t);
  return {std::max(.00022f, r) * std::cos(a), y,
          cz + std::max(.00022f, r) * std::sin(a)};
}
void make_pax_hair(Sculpt &b) {
  const Vec3 hair = b.m.appearance.hair;
  const int sectors=8,columns=b.near?6:3,rows=b.near?14:7;
  std::array<unsigned,8> starts{};
  for(int sector=0;sector<sectors;++sector) {
    starts[sector]=unsigned(b.m.bind_mesh.vertices.size());
    b.grid(columns,rows,[&](float u,float v){
      float a=2*pi*(float(sector)+u)/sectors;
      return P{pax_scalp(a,.018f+.982f*v,b.near?0:.0028f),hair*.82f,wt(B::Head),character_surface_uv(S::HairCrop,u,v)};
    },false,false);
  }
  // Separate texture sectors retain a realistic short-fiber scale while
  // keeping the actual opaque scalp surface and every geometric seam intact.
  for(int sector=0;sector<sectors;++sector)for(int row=0;row<=rows;++row) {
    const auto first=starts[sector]+row*(columns+1),last=starts[(sector+sectors-1)%sectors]+row*(columns+1)+columns;
    b.m.bind_mesh.vertices[last].position=b.m.bind_mesh.vertices[first].position;
    b.normal_groups.push_back({first,last});
  }
  const auto pole=b.vertex({{0,.5f,pax_anatomy::crown_joint.z},hair*.82f,wt(B::Head),character_surface_uv(S::HairCrop,.5f,1)});
  for(int sector=0;sector<sectors;++sector)for(int i=0;i<columns;++i) {
    auto start=starts[sector]+rows*(columns+1)+i;b.tri(start,pole,start+1);
  }
  // A compact, low wrapped knot. The asymmetric coiled envelope constricts at
  // its root and follows the occiput, rather than perching above the crown.
  const Vec3 knot{-.002f, .434f, -.046f};
  b.ellipsoid({0.f, .434f, -.029f}, {.018f, .017f, .020f}, hair * .83f,
              S::Hair, B::Head, b.near ? 24 : 10, b.near ? 10 : 4);
  b.grid(b.near ? 28 : 10, b.near ? 14 : 4, [&](float u, float v) {
    float a = 2 * pi * u, e = lerp(-pi * .495f, pi * .495f, v);
    float coil =
        1 + .060f * std::sin(3 * a + e * 4) + .028f * std::sin(7 * a - e * 3);
    return P{knot + Vec3{.026f * std::cos(a) * std::cos(e) * coil,
                         .025f * std::sin(e),
                         .020f * std::sin(a) * std::cos(e) * coil},
             hair * (.84f + .045f * std::sin(a * 3 + e * 2)), wt(B::Head),
             character_surface_uv(S::Hair, u, v)};
  });
  // The sheet's width is perpendicular to the local growth tangent. Pure
  // azimuth width folds at the apex where the growth changes vertical sign.
  for (int k = 0; k < (b.near ? 18 : 2); ++k) {
    const float side = k % 2 ? 1.f : -1.f,
                offset = float(k / 2) / (b.near ? 9.f : 2.f);
    const auto center = [&](float v) {
      const float start = pi * .5f + side * (.12f + offset * 1.12f);
      const float angle = start + side * (pi - .12f - offset * 1.12f) * v;
      const float y = lerp(pax_hairline(start) + .005f, .432f, v) +
                      (.030f - .015f * offset) * std::sin(pi * v);
      const float t = std::asin(std::clamp((y - pax_hairline(angle)) /
                                               (.5f - pax_hairline(angle)),
                                           0.f, 1.f)) /
                      (pi * .5f);
      return pax_scalp(angle, t,
                        (b.near ? .0008f : .0038f) + .0005f * std::sin(pi * v));
    };
    b.grid(
        2, b.near ? 10 : 6,
        [&](float u, float v) {
          Vec3 p = center(v),
               tangent = normalize(center(std::min(1.f, v + .001f)) -
                                   center(std::max(0.f, v - .001f)));
          Vec3 normal = normalize(Vec3{p.x, p.y - .437f, p.z - .023f});
          Vec3 width = normalize(cross(tangent, normal));
          p += width * ((.5f - u) * (.0048f - .0015f * v) * (.18f + .82f * std::sin(pi * v)));
          return P{p, hair * .83f, wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
  // Wrapped, masked locks cover the coiled knot with changing growth direction
  // and a broken outline. They are fitted outside its real opaque envelope.
  for (int lock = 0; lock < (b.near ? 8 : 2); ++lock) {
    const float root = 2 * pi * float(lock) / (b.near ? 8.f : 2.f);
    const auto center = [&](float t) {
      const float a = root + 1.4f * t, e = lerp(-1.10f, 1.10f, t),
                  coil = 1 + .060f * std::sin(3 * a + e * 4) +
                         .028f * std::sin(7 * a - e * 3);
      return knot + Vec3{(.026f * coil + .0008f) * std::cos(a) * std::cos(e),
                         .0258f * std::sin(e),
                         (.020f * coil + .0008f) * std::sin(a) * std::cos(e)};
    };
    b.grid(
        2, b.near ? 7 : 5,
        [&](float u, float v) {
          Vec3 p = center(v),
               tangent = normalize(center(std::min(1.f, v + .001f)) -
                                   center(std::max(0.f, v - .001f)));
          Vec3 normal =
              normalize(Vec3{(p.x - knot.x) / .026f, (p.y - knot.y) / .025f,
                             (p.z - knot.z) / .020f});
          p += normalize(cross(tangent, normal)) * ((.5f - u) * .0038f);
          return P{p, hair * .81f, wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
  std::vector<Vec3> tie;
  for (int i = 0; i <= 24; ++i) {
    float a = 2 * pi * i / 24;
    tie.push_back(
        {0.f + .016f * std::cos(a), .434f + .014f * std::sin(a), -.030f});
  }
  b.curve(tie, .0015f, {.023f, .024f, .025f}, S::Rubber, B::Head,
          b.near ? 6 : 4, .60f);
  const int cards = b.near ? 64 : 8;
  for (int k = 0; k < cards; ++k) {
    float a0 = 2 * pi * float(k) / cards;
    bool nape = std::sin(a0) < -.3f;
    float width = b.near ? .060f : .14f;
    b.grid(
        2, b.near ? 6 : 3,
        [&](float u, float v) {
          float a = a0 + (.5f - u) * width + .027f * std::sin(pi * v),
                t = lerp(.21f, nape ? -.040f : -.027f, v);
          Vec3 p = pax_scalp(a, std::max(0.f, t),
                              .0004f + .0005f * std::sin(pi * v));
          if (t < 0)
            p.y += .10f * t;
          return P{p, hair * .84f, wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
}
CharacterRig pax_rig(float height) {
  CharacterRig r;
  r.height = height;
  auto joint = [&](B bone, int parent, Vec3 p) {
    r.joints[bone_index(bone)] = {parent, p * height};
  };
  auto child = [&](B bone, B parent, Vec3 p) {
    joint(bone, int(bone_index(parent)), p);
  };
  joint(B::Pelvis, -1, {0, 0, 0});
  child(B::Spine, B::Pelvis, {0, .13f, 0});
  child(B::Chest, B::Spine, {0, .28f, 0});
  child(B::Neck, B::Chest, pax_anatomy::neck_center);
  child(B::Head, B::Neck, pax_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    bool q = sign > 0;
    B upper = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(upper, B::Chest, {sign * .107f, .322f, 0});
    child(fore, upper, {sign * .165f, .12f, 0});
    child(hand, fore, {sign * .179f, -.044f, 0});
    child(thigh, B::Pelvis, {sign * .056f, 0, 0});
    child(shin, thigh, {sign * .056f, -.225f, 0});
    child(foot, shin, {sign * .056f, -.46f, 0});
  }
  return r;
}
} // namespace
CharacterProfile make_pax_character_profile(float height, CharacterRole role,
                                             std::string_view identity,
                                             CharacterLod lod) {
  if (!std::isfinite(height) || height <= 0 ||
      (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid individual character request");
  CharacterProfile p;
  auto &m = p.model;
  m.role = role;
  m.lod = lod;
  m.seed = character_seed(identity);
  m.appearance = character_appearance(m.seed);
  m.rig = pax_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 36000 : 16000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 180000 : 90000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  const auto shirt_support=make_pax_shirt(b, anatomy_radius, pax_anatomy::neck_center);
  make_hands_and_forearms(b);
  make_trousers_and_shoes(b);
  make_head(b);
  make_pax_hair(b);
  make_pax_vest(b,shirt_support);
  b.finish();
  for(int pass=0;pass<4;++pass) {
    bool changed=false;
    auto &mesh=m.bind_mesh;
    for(std::size_t t=0;t<mesh.indices.size();t+=3) {
      auto &ia=mesh.indices[t],&ic=mesh.indices[t+1],&id=mesh.indices[t+2];
      const auto &a=mesh.vertices[ia],&c=mesh.vertices[ic],&d=mesh.vertices[id];
      const Vec2 low=character_surface_uv(S::Jacket,0,0),high=character_surface_uv(S::Jacket,1,1);
      if(a.uv.x<low.x||a.uv.x>high.x||a.uv.y<low.y||a.uv.y>high.y)continue;
      Vec3 normal=cross(c.position-a.position,d.position-a.position);
      if(dot(normal,a.normal+c.normal+d.normal)>0)continue;
      std::array<unsigned,3> old{ia,ic,id},fresh{};
      for(int k=0;k<3;++k){fresh[k]=mesh.vertices.size();mesh.vertices.push_back(mesh.vertices[old[k]]);m.influences.push_back(m.influences[old[k]]);}
      ia=fresh[0];ic=fresh[1];id=fresh[2];changed=true;
    }
    if(!changed)break;
    for(auto &vertex:mesh.vertices)vertex.normal={};
    b.finish();
  }
  p.material = make_character_surface_material();
  p.material.alpha_cutoff = .4f;
  p.lod_distance = 12;
  p.individualized = true;
  return p;
}
} // namespace fury
