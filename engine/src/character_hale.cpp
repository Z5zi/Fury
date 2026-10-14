#include "character_hale_shirt.hpp"
#include "character_hale_vest.hpp"
#include "character_hale_cap.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/hale_anatomy.hpp"
#include "fury/generated/hale_hands.hpp"
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
  for (std::size_t i = 0; i < hale_anatomy::indices.size(); i += 3) {
    const Vec3 a = hale_anatomy::positions[hale_anatomy::indices[i]];
    const Vec3 b = hale_anatomy::positions[hale_anatomy::indices[i + 1]];
    const Vec3 c = hale_anatomy::positions[hale_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < hale_anatomy::indices.size(); i += 3) {
    const Vec3 a = hale_anatomy::positions[hale_anatomy::indices[i]];
    const Vec3 b = hale_anatomy::positions[hale_anatomy::indices[i + 1]];
    const Vec3 c = hale_anatomy::positions[hale_anatomy::indices[i + 2]];
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
CharacterInfluence hale_neck_weight(float y) {
  return y < .357f ? wt(B::Neck, B::Chest, smooth((y - .333f) / .024f))
                   : wt(B::Head, B::Neck, smooth((y - .357f) / .027f));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      hale_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < hale_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = hale_anatomy::indices[triangle + corner];
      const auto uv_index = hale_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = hale_anatomy::positions[index];
        const Vec2 uv = hale_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .38915f, .0032f) *
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
            b.vertex({position, color, hale_neck_weight(position.y),
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
  auto boundary = hale_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = hale_anatomy::positions[boundary[i]];
    const Vec3 c = hale_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = hale_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, hale_neck_weight(point.y),
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
  for (std::size_t ei = 0; ei < hale_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = hale_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? .0011f : -.0011f, -.0010f, b.near ? -.00195f : -.00170f};
    const Vec3 radius = hale_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 8 : 6);
    if (b.near) {
      std::vector<Vec3> upper_lid, lower_lid;
      const auto &rim = ei == 0 ? hale_anatomy::left_eye_opening
                                : hale_anatomy::right_eye_opening;
      for (Vec3 p : rim) {
        if (std::fabs(p.x - eye.x) > .0055f)
          continue;
        p.z += .00010f;
        (p.y >= .42975f ? upper_lid : lower_lid).push_back(p);
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

// Hale's relaxed bind turns each palm toward the thigh, with thumbs forward.
// This fixed rigid fitting is shared by source fingers and the sewn wrist ring;
// it is not a finger controller or per-frame hand animation.
Vec3 hale_hand_offset(Vec3 p,float side) {
  p.x*=side;const float a=side*.94f,c=std::cos(a),sn=std::sin(a);
  return {c*p.x+sn*p.z,p.y,-sn*p.x+c*p.z};
}
void make_hands_and_forearms(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  for (float side : {-1.f, 1.f}) {
    const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
    const B upper = side > 0 ? B::RightUpperArm : B::LeftUpperArm;
    const B hand = side > 0 ? B::RightHand : B::LeftHand;
    const Vec3 wrist{side * .168f, -.044f, .018f};
    std::map<std::pair<unsigned, unsigned>, unsigned> corners;
    std::vector<std::vector<unsigned>> groups(hale_hands::positions.size());
    const auto emit = [&](auto const &indices, auto const &uv_indices) {
      for (std::size_t t = 0; t < indices.size(); t += 3) {
        unsigned out[3];
        for (int k = 0; k < 3; ++k) {
          auto index = indices[t + k], uv = uv_indices[t + k];
          const bool hard = b.near
            ? std::find(hale_hands::hard_normal_triangle_indices.begin(),
                        hale_hands::hard_normal_triangle_indices.end(), t/3) != hale_hands::hard_normal_triangle_indices.end()
            : std::find(hale_hands::far_hard_normal_triangle_indices.begin(),
                        hale_hands::far_hard_normal_triangle_indices.end(), t/3) != hale_hands::far_hard_normal_triangle_indices.end();
          auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) * hale_hands::uv_coordinates.size())
                         : 0));
          auto found = corners.find(key);
          if (found == corners.end()) {
            Vec3 p = hale_hand_offset(hale_hands::positions[index],side);
            p += wrist;
            const Vec2 tex = hale_hands::uv_coordinates[uv];
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
      emit(hale_hands::near_indices, hale_hands::near_uv_indices);
    else
      emit(hale_hands::far_indices, hale_hands::far_uv_indices);
    // Exactly reuse every anatomical wrist point at the terminal forearm ring.
    // The opposite terminal ring tucks inside the open rolled cuff, not a cap.
    auto boundary = hale_hands::wrist_boundary_indices;
    float area = 0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
      auto a = hale_hands::positions[boundary[i]],
           c = hale_hands::positions[boundary[(i + 1) % boundary.size()]];
      area += a.x * c.z - c.x * a.z;
    }
    if (area * side < 0)
      std::reverse(boundary.begin(), boundary.end());
    Vec3 center{};
    for (auto i : boundary)
      center += hale_hands::positions[i];
    center = center * (1.f / boundary.size());
    const unsigned start = b.m.bind_mesh.vertices.size();
    const int columns = boundary.size(), rows = b.near ? 6 : 2;
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
              boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = hale_hand_offset(hale_hands::positions[index],side);
          source += wrist;
          const Vec3 local = hale_hand_offset(hale_hands::positions[index] - center,side);
          const float a = std::atan2(local.z, local.x);
          const float y = lerp(source.y, -.014f, v),
                      radius = .016f + .0130f * g(y, .078f, .071f);
          const float x = side * lerp(.168f, .166f, smooth((y + .044f) / .030f));
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
void make_trousers_and_shoes(Sculpt &b) {
  const Vec3 cloth{.095f, .13f, .175f};
  const int columns = b.near ? 28 : 12, rows = b.near ? 24 : 9;
  std::array<std::vector<unsigned>, 2> seams;
  for (float side : {-1.f, 1.f}) {
    const B thigh = side > 0 ? B::RightThigh : B::LeftThigh,
            shin = side > 0 ? B::RightShin : B::LeftShin,
            foot = side > 0 ? B::RightFoot : B::LeftFoot;
    // The cloth cuff, ankle upper and underlay share one ankle bend field.
    // Letting a shin-rigid trouser end rotate through a foot-rigid instep is
    // not resolved by a bind-only clearance offset.
    const auto ankle_weight = [&](float y) {
      return wt(foot,shin,1-smooth((y+.420f)/.045f));
    };
    const auto base = unsigned(b.m.bind_mesh.vertices.size());
    b.grid(
        columns, rows,
        [&](float u, float v) {
          float a = 2 * pi * u, cs = std::cos(a), sn = std::sin(a),
                y = lerp(-.431f, -.09f, std::min(v / .78f, 1.f));
          if (!b.near) {
            // Keep the existing Far triangle budget, but place its scarce
            // longitudinal rows through the real ankle bend. A single long
            // cuff triangle otherwise shortcuts the shared weight field.
            constexpr std::array<float,8> levels{
                -.431f,-.414f,-.394f,-.374f,-.285f,-.220f,-.155f,-.090f};
            const float row=std::min(v*rows,7.f);
            const int k=std::min(6,int(row));
            y=lerp(levels[k],levels[k+1],row-k);
          }
          float radius = .027f + .011f * g(y, -.34f, .073f) +
                         .018f * smooth((y + .24f) / .19f);
          const float fold =
              .0028f * g(y, -.22f + .012f * cs, .025f) * sn +
              .0022f * g(y, -.446f, .021f) * std::sin(2 * a + y * 210);
          Vec3 p{side * (.056f + (radius + fold) * cs), y,
                 (radius * 1.10f + fold) * sn - .002f};
          const float join = smooth((v - .78f) / .22f);
          const Vec3 top{side * .093f * std::max(0.f, cs),
                         .057f - .127f * std::max(0.f, -cs),
                         .061f * sn - .002f};
          p = p * (1 - join) + top * join;
          CharacterInfluence w =
              y < -.375f ? ankle_weight(y)
              : y < -.285f ? wt(shin)
              : y < -.16f ? wt(shin, thigh, 1 - smooth((y + .285f) / .125f))
                          : wt(thigh, B::Pelvis, 1 - join);
          // Preserve the fixed cuff and shin rings. The first intermediate
          // row follows the ankle support more at the front than at the heel,
          // where the unchanged boot has a shorter profile. This smooth static
          // field keeps the cloth outside the boot through ankle articulation.
          // The remaining rows distribute the bend into the retained shin.
          const int ankle_row = int(std::lround(v * rows));
          if (ankle_row == 1) {
            const float front = smooth((sn + 1.f) * .5f);
            w = wt(foot, shin, b.near ? lerp(.46f, .61f, front)
                                     : lerp(.49f, .71f, front));
          } else if (b.near && ankle_row >= 2 && ankle_row <= 3) {
            constexpr std::array<float, 2> foot_weights{0.271595182f, 0.073970130f};
            w = wt(foot, shin, foot_weights[ankle_row - 2]);
          } else if (!b.near && ankle_row == 2) {
            w = wt(foot, shin, 0.209824965f);
          }
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
                    boot_section(v,{.070f,.069f,.052f,.034f,.0250f,.0240f}) * std::sin(a)},
               b.m.appearance.shoes,
               // The ankle upper follows the shin above the flexible bend.
               // A fully rigid shaft can sweep through the trouser cuff in
               // crouch-walk even when its bind silhouette looks acceptable.
               ankle_weight(yy),
               character_surface_uv(S::Leather, u, v)};
    },true,b.near);
    const auto upper_end=b.m.bind_mesh.indices.size();
    auto shoe_support=[&](float xx,float zz) {
      float highest=-1,foot_weight=1;
      for(std::size_t i=upper_start;i<upper_end;i+=3) {
        Vec3 a=b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i]].position*(1/b.h),
             c=b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i+1]].position*(1/b.h),
             d=b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i+2]].position*(1/b.h);
        Vec3 e=c-a,f=d-a;float det=e.x*f.z-e.z*f.x;
        if(std::fabs(det)<1e-12f)continue;
        float u=((xx-a.x)*f.z-(zz-a.z)*f.x)/det,v=(e.x*(zz-a.z)-e.z*(xx-a.x))/det;
        if(u>=-1e-6f&&v>=-1e-6f&&u+v<=1.000001f) {
          const float y=a.y+u*e.y+v*f.y;
          if(y>highest) {
            highest=y;
            const float wa=b.m.influences[b.m.bind_mesh.indices[i]].first_weight,
                        wc=b.m.influences[b.m.bind_mesh.indices[i+1]].first_weight,
                        wd=b.m.influences[b.m.bind_mesh.indices[i+2]].first_weight;
            foot_weight=std::clamp((1-u-v)*wa+u*wc+v*wd,0.f,1.f);
          }
        }
      }
      if(highest<-.6f||highest>-.40f)throw std::runtime_error("Hale boot lace has no upper support");
      return std::make_pair(highest,wt(foot,shin,foot_weight));
    };
    if(b.near) b.grid(4,8,[&](float u,float v){float xx=x+lerp(-.013f,.013f,u),zz=lerp(.029f,.077f,v);const auto support=shoe_support(xx,zz);return P{{xx,support.first+.00085f,zz},b.m.appearance.shoes*.90f,support.second,character_surface_uv(S::Leather,u,v)};},false,false);
    // A shaped ankle underlay bridges the trouser opening to the moving shoe.
    // Its weights continue the shin-to-foot bend, avoiding an empty cuff gap.
    b.grid(
        b.near ? 20 : 12, b.near ? 6 : 3,
        [&](float u, float v) {
          float a = 2 * pi * u, y = lerp(-.477f, -.414f, v),
                rx = lerp(.024f, .0228f, v);
          return P{{x + rx * std::cos(a), y, .005f + rx * .93f * std::sin(a)},
                   Vec3{.071f, .078f, .083f},
                   ankle_weight(y),
                   character_surface_uv(S::Shirt, u, v)};
        },
        true, false);
    // Recessed dark laces follow the actual triangulated instep. Side and toe
    // inspection exposed buried laces on the taller early boot envelope.
    for(int lace=0;lace<(b.near?4:0);++lace) {
      float z=.035f+lace*(b.near?.010f:.020f);std::vector<Vec3> line;
      for(int j=0;j<=4;++j){float xx=x+lerp(-.013f,.013f,float(j)/4);line.push_back({xx,shoe_support(xx,z).first+.0016f,z});}
      b.tube(line,std::vector<float>(line.size(),.00065f),1.f,{.15f,.16f,.16f},S::Shirt,
             [&](float t){return shoe_support(x+lerp(-.013f,.013f,t),z).second;},b.near?6:4);
    }

  }
  for (std::size_t i = 0; i < seams[0].size(); ++i)
    b.normal_groups.push_back({seams[0][i], seams[1][i]});
  b.grid(
      b.near ? 48 : 20, 3,
      [&](float u, float v) {
        float a = 2 * pi * u;
        return P{{.0935f * std::cos(a), lerp(.0565f, .071f, v),
                  .062f * std::sin(a) - .002f},
                 cloth * .94f,
                 wt(B::Pelvis),
                 character_surface_uv(S::Pants, u, v)};
      },
      true, false);
}
CharacterRig hale_rig(float height) {
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
  child(B::Neck, B::Chest, hale_anatomy::neck_center);
  child(B::Head, B::Neck, hale_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    bool q = sign > 0;
    B upper = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(upper, B::Chest, {sign * .111f, .314f, 0});
    child(fore, upper, {sign * .151f, .12f, .008f});
    child(hand, fore, {sign * .168f, -.044f, .018f});
    child(thigh, B::Pelvis, {sign * .056f, 0, 0});
    child(shin, thigh, {sign * .056f, -.225f, 0});
    child(foot, shin, {sign * .056f, -.46f, 0});
  }
  return r;
}
} // namespace
CharacterProfile make_hale_character_profile(float height, CharacterRole role,
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
  m.rig = hale_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 36000 : 16000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 180000 : 90000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  const auto shirt_support=make_hale_shirt(b, anatomy_radius, hale_anatomy::neck_center);
  make_hale_collar(b,shirt_support);
  make_hands_and_forearms(b);
  make_trousers_and_shoes(b);
  make_head(b);
  make_hale_cap_and_hair(b, anatomy_radius);
  make_hale_vest(b,shirt_support);
  b.finish();
  p.material = make_character_surface_material();
  p.material.alpha_cutoff = .4f;
  p.lod_distance = 12;
  p.individualized = true;
  return p;
}
} // namespace fury
