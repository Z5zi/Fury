#include "character_player_body_shirt.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/player_body_anatomy.hpp"
#include "fury/generated/player_body_hands.hpp"
#include "fury/generated/player_body_hair.hpp"
#include "fury/generated/player_body_short_tail.hpp"
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
      for (std::size_t i = 0; i < player_body_anatomy::indices.size(); i += 3) {
        const Vec3 a = player_body_anatomy::positions[player_body_anatomy::indices[i]];
        const Vec3 b = player_body_anatomy::positions[player_body_anatomy::indices[i + 1]];
        const Vec3 c = player_body_anatomy::positions[player_body_anatomy::indices[i + 2]];
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
      const Vec3 origin{
        0, y, center_z
      },
      direction{
        std::cos(angle), 0, std::sin(angle)
      };
      float best = 0.f;
      for (std::size_t i = 0; i < player_body_anatomy::indices.size(); i += 3) {
        const Vec3 a = player_body_anatomy::positions[player_body_anatomy::indices[i]];
        const Vec3 b = player_body_anatomy::positions[player_body_anatomy::indices[i + 1]];
        const Vec3 c = player_body_anatomy::positions[player_body_anatomy::indices[i + 2]];
        if (y < std::min({
          a.y, b.y, c.y
        }) - 1e-6f ||
        y > std::max({
          a.y, b.y, c.y
        }) + 1e-6f)
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
    CharacterInfluence player_body_neck_weight(float y) {
      const float neck = player_body_anatomy::neck_center.y;
      const float floor = player_body_anatomy::bounds_min.y;
      return wt(B::Neck, B::Chest, smooth((y - floor) / (neck - floor)));
    }
    CharacterInfluence player_body_source_weight(std::size_t index) {
      // Membership is generated from THIS actor's source UV chart seams. Exterior
      // jaw, complete mouth interior and every shared corner follow Head together.
      if (player_body_anatomy::head_region[index]) return wt(B::Head);
      const Vec3 p = player_body_anatomy::positions[index];
      const float neck = player_body_anatomy::neck_center.y;
      const float head = player_body_anatomy::neck_head_blend[index];
      const float chest = (1-head)*(1-smooth((p.y-player_body_anatomy::bounds_min.y)/(neck-player_body_anatomy::bounds_min.y)));
      return {
        std::uint8_t(B::Head),std::uint8_t(B::Neck),head,std::uint8_t(B::Chest),chest
      };
    }
    CharacterInfluence player_body_skin_weight(Vec3 p) {
      const Vec3 origin{
        0,p.y,player_body_anatomy::neck_center.z
      },direction=normalize(p-origin);
      float best=-1;
      std::array<float,character_bone_count> total{};
      for(std::size_t t=0;t<player_body_anatomy::indices.size();t+=3){
        unsigned ids[]{
          player_body_anatomy::indices[t],player_body_anatomy::indices[t+1],player_body_anatomy::indices[t+2]
        };
        Vec3 a=player_body_anatomy::positions[ids[0]],c=player_body_anatomy::positions[ids[1]],d=player_body_anatomy::positions[ids[2]];
        Vec3 e=c-a,f=d-a,q=cross(direction,f);
        float det=dot(e,q);
        if(std::fabs(det)<1e-12f)continue;
        Vec3 off=origin-a,r=cross(off,e);
        float u=dot(off,q)/det,v=dot(direction,r)/det,distance=dot(f,r)/det;
        if(u<0||v<0||u+v>1||distance<=best)continue;
        best=distance;
        total={};
        const float bary[]{
          1-u-v,u,v
        };
        for(int j=0;j<3;++j){
          auto w=player_body_source_weight(ids[j]);
          total[w.first]+=bary[j]*w.first_weight;
          total[w.second]+=bary[j]*(1-w.first_weight-w.third_weight);
          total[w.third]+=bary[j]*w.third_weight;
        }
      }
      if(best<0)return player_body_neck_weight(p.y);
      return {
        std::uint8_t(B::Head),std::uint8_t(B::Neck),total[bone_index(B::Head)],std::uint8_t(B::Chest),total[bone_index(B::Chest)]
      };
    }
    void make_head(Sculpt &b) {
      const Vec3 skin = b.m.appearance.skin;
      std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
      std::vector<std::vector<std::uint32_t>> groups(
      player_body_anatomy::positions.size());
      for (std::size_t triangle = 0; triangle < player_body_anatomy::indices.size();
      triangle += 3) {
        std::uint32_t out[3];
        for (int corner = 0; corner < 3; ++corner) {
          const auto index = player_body_anatomy::indices[triangle + corner];
          const auto uv_index = player_body_anatomy::uv_indices[triangle + corner];
          const auto key = std::make_pair(index, uv_index);
          auto found = corners.find(key);
          if (found == corners.end()) {
            const Vec3 position = player_body_anatomy::positions[index];
            const Vec2 uv = player_body_anatomy::uv_coordinates[uv_index];
            const float lip = g(position.x, 0, .016f) *
            g(position.y, player_body_anatomy::lip_center.y, .0040f) *
            smooth((position.z - (player_body_anatomy::lip_center.z-.015f)) / .012f);
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
            g(position.y, player_body_anatomy::eye_centers[0].y, .006f) * front;
            Vec3 color = skin * (1 - .025f * vascular * front);
            color.x *= 1 - .015f * lid;
            color.y *= 1 - .055f * lid;
            color.z *= 1 - .037f * lid;
            color.y *= 1 - .052f * cheeks - .043f * nose;
            color.z *= 1 - .077f * cheeks - .054f * nose;
            color.y *= 1 - .22f * lip;
            color.z *= 1 - .19f * lip;
            if (uv.x > .75f && uv.x < .93f && uv.y < .13645f) color = skin * .16f;
            const auto vertex =
            b.vertex({
              position, color, player_body_source_weight(index),
              character_surface_uv(S::Face, uv.x, uv.y)
            });
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
      auto boundary = player_body_anatomy::attachment_boundary_indices;
      float area = 0;
      for (std::size_t i = 0; i < boundary.size(); ++i) {
        const Vec3 a = player_body_anatomy::positions[boundary[i]];
        const Vec3 c = player_body_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = player_body_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{
          point, skin, player_body_neck_weight(point.y),
          character_surface_uv(S::Skin, u, v)
        };
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
      for (std::size_t ei = 0; ei < player_body_anatomy::eye_centers.size(); ++ei) {
        const Vec3 eye = player_body_anatomy::eye_centers[ei] +
        Vec3{
          ei == 0 ? .00065f : -.00065f, -.00046f, -.0015f
        };
        const Vec3 radius = player_body_anatomy::eye_helper_half_extents[ei];
        b.ellipsoid(eye, radius, {
          .49f, .51f, .48f
        }, S::Eyes, B::Head,
        b.near ? 24 : 12, b.near ? 8 : 6);
        // The intact source already contains upper/lower lids and tear corners.
        // Separate cosmetic micro-tubes reversed at this actor's inner canthus;
        // retain the anatomical loop itself rather than overlay a second eyelid.
        const int iris_columns = b.near ? 24 : 8, iris_rows = b.near ? 5 : 3;
        const auto iris_base = std::uint32_t(b.m.bind_mesh.vertices.size());
        b.grid(
        iris_columns, iris_rows,
        [&](float u, float v) {
          const float near_r[]{
            .00005f, .00122f, .00140f,
            .00225f, .00310f, .00355f
          };
          const float far_r[]{
            .00005f, .00122f, .00140f, .00355f
          };
          int row = std::min(iris_rows, int(std::lround(v * iris_rows)));
          float r = b.near ? near_r[row] : far_r[row], a = 2 * pi * u,
          x = r * std::cos(a), y = r * std::sin(a);
          float z = radius.z * std::sqrt(std::max(0.f, 1 - sq(x / radius.x) -
          sq(y / radius.y))) +
          .00015f;
          Vec3 color{
            .005f, .006f, .005f
          };
          if (r > .00130f) {
            float fiber =
            .88f + .16f * std::sin(a * 37 + .4f * std::sin(a * 11));
            color = b.m.appearance.eyes * fiber;
            if (r > .0031f)
            color = color * .38f;
          }
          return P{
            eye + Vec3{
              x, y, z
            }, color, wt(B::Head),
            character_surface_uv(S::Eyes, u, v)
          };
        },
        true, false);
        auto center = b.vertex({
          eye + Vec3{
            0, 0, radius.z + .00015f
          },
          {
            .005f, .006f, .005f
          },
          wt(B::Head),
          character_surface_uv(S::Eyes, .5f, .5f)
        });
        for (int i = 0; i < iris_columns; ++i)
        b.tri(center, iris_base + std::uint32_t(i),
        iris_base + std::uint32_t(i + 1));
        std::vector<Vec3> brow;
        std::vector<float> brow_radius;
        const int count = b.near ? 24 : 6;
        for (int i = 0; i <= count; ++i) {
          float t = float(i) / count, x = eye.x + lerp(-.0118f, .0118f, t),
          outward = eye.x > 0 ? t : 1 - t,
          y = eye.y + .0122f + .0025f * std::sin(pi * outward) -
          .0011f * outward;
          float z = anatomy_front(x, y);
          brow.push_back({
            x, y, z + .0004f
          });
          brow_radius.push_back(
          .00012f + .00088f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
        }
        b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .74f, S::Hair,
        [](float) {
          return wt(B::Head);
        }, b.near ? 6 : 4);
      }
    }
    Vec3 fit_hand_point(Vec3 p, float side) {
      const float angle = side * (68.f * pi / 180.f);
      p.x *= side;
      return {
        p.x * std::cos(angle) + p.z * std::sin(angle), p.y,
        -p.x * std::sin(angle) + p.z * std::cos(angle)
      };
    }
    void make_hands_and_forearms(Sculpt &b) {
      const Vec3 skin = b.m.appearance.skin;
      for (float side : {
        -1.f, 1.f
      }) {
        const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
        const B upper = side > 0 ? B::RightUpperArm : B::LeftUpperArm;
        const B hand = side > 0 ? B::RightHand : B::LeftHand;
        const Vec3 wrist{
          side * .173f, -.039f, .008f
        };
        std::map<std::pair<unsigned, unsigned>, unsigned> corners;
        std::vector<std::vector<unsigned>> groups(player_body_hands::positions.size());
        const auto emit = [&](auto const &indices, auto const &uv_indices) {
          for (std::size_t t = 0; t < indices.size(); t += 3) {
            unsigned out[3];
            for (int k = 0; k < 3; ++k) {
              auto index = indices[t + k], uv = uv_indices[t + k];
              const bool hard =
              b.near &&
              std::find(player_body_hands::hard_normal_triangle_indices.begin(),
              player_body_hands::hard_normal_triangle_indices.end(),
              t / 3) !=
              player_body_hands::hard_normal_triangle_indices.end();
              auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) * player_body_hands::uv_coordinates.size())
              : 0));
              auto found = corners.find(key);
              if (found == corners.end()) {
                Vec3 p = fit_hand_point(player_body_hands::positions[index], side);
                p += wrist;
                const Vec2 tex = player_body_hands::uv_coordinates[uv];
                auto i = b.vertex({
                  p, skin, wt(hand),
                  character_surface_uv(S::Skin, tex.x, tex.y)
                });
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
        emit(player_body_hands::near_indices, player_body_hands::near_uv_indices);
        else
        emit(player_body_hands::far_indices, player_body_hands::far_uv_indices);
        // Exactly reuse every anatomical wrist point at the terminal forearm ring.
        // The opposite terminal ring tucks inside the open sleeve cuff, not a cap.
        auto boundary = player_body_hands::wrist_boundary_indices;
        float area = 0;
        for (std::size_t i = 0; i < boundary.size(); ++i) {
          auto a = player_body_hands::positions[boundary[i]],
          c = player_body_hands::positions[boundary[(i + 1) % boundary.size()]];
          area += a.x * c.z - c.x * a.z;
        }
        if (area * side < 0)
        std::reverse(boundary.begin(), boundary.end());
        Vec3 center{};
        for (auto i : boundary)
        center += player_body_hands::positions[i];
        center = center * (1.f / boundary.size());
        const unsigned start = b.m.bind_mesh.vertices.size();
        const int columns = boundary.size(), rows = b.near ? 6 : 2;
        b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
          boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = fit_hand_point(player_body_hands::positions[index], side);
          source += wrist;
          const Vec3 local =
          fit_hand_point(player_body_hands::positions[index] - center, side);
          const float a = std::atan2(local.z, local.x);
          const float y = lerp(source.y, .144f, v),
          radius = .014f + .0105f * g(y, .078f, .071f);
          const float x = side * lerp(.173f, .153f, smooth((y + .039f) / .18f));
          Vec3 target{
            x + radius * std::cos(a), y,
            .008f + radius * .83f * std::sin(a)
          };
          Vec3 p = source * (1 - smooth(v / .28f)) + target * smooth(v / .28f);
          CharacterInfluence w =
          y < -.015f ? wt(hand, fore, 1 - smooth((y + .039f) / .04f))
          : wt(fore, upper, smooth((.163f - y) / .081f));
          if (v == 0)
          w = wt(hand);
          return P{
            p, skin, w, character_surface_uv(S::Skin, u, v)
          };
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
    float player_body_ankle_foot_weight(float y) {
      // The narrow cloth cuff and its supported sock share one local ankle blend.
      // Keep the calf on Shin; only the bottom 2.2% of height eases toward Foot.
      return y<-.432f ? lerp(.90f,1.f,smooth((-.432f-y)/.045f))
      : .90f*smooth((-.410f-y)/.022f);
    }
    void make_trousers_and_shoes(Sculpt &b) {
      const Vec3 cloth = b.m.appearance.trousers;
      const int columns = b.near ? 28 : 16, rows = b.near ? 24 : 15;
      constexpr float near_y[]{
        -.432f, -.426f, -.420f, -.390f, -.355f,
        -.325f, -.300f, -.280f, -.265f, -.250f,
        -.235f, -.225f, -.215f, -.200f, -.185f,
        -.170f, -.150f, -.130f, -.110f, -.090f
      };
      constexpr float far_y[]{
        -.432f, -.426f, -.420f, -.390f, -.355f, -.325f, -.280f, -.250f,
        -.225f, -.200f, -.170f, -.130f, -.090f
      };
      std::array<std::vector<unsigned>, 2> seams;
      for (float side : {
        -1.f, 1.f
      }) {
        const B thigh = side > 0 ? B::RightThigh : B::LeftThigh,
        shin = side > 0 ? B::RightShin : B::LeftShin,
        foot = side > 0 ? B::RightFoot : B::LeftFoot;
        const auto base = unsigned(b.m.bind_mesh.vertices.size());
        b.grid(
        columns, rows,
        [&](float u, float v) {
          float a = u == 1.f ? 0.f : 2 * pi * u, cs = std::cos(a),
          sn = std::sin(a);
          const int row = int(std::lround(v * rows)),
          leg_rows = b.near ? 19 : 12;
          const float y = b.near ? near_y[std::min(row, leg_rows)]
          : far_y[std::min(row, leg_rows)];
          float radius = .0255f + .0065f*g(y,-.432f,.038f) + .0095f * g(y, -.34f, .073f) +
          .0205f * smooth((y + .24f) / .19f);
          const float fold =
          .0018f * g(y, -.22f + .012f * cs, .025f) * sn +
          .0012f * g(y, -.446f, .021f) * std::sin(2 * a + y * 210);
          Vec3 p{
            side * (.054f + (radius + fold) * cs), y,
            (radius * 1.10f + fold) * sn - .002f +
            .013f * g(y, -.225f, .19f) * std::max(0.f, -sn)
          };
          const float join =
          smooth(float(row - leg_rows) / float(rows - leg_rows));
          const Vec3 top{
            side * .098f * std::max(0.f, cs),
            .057f - .092f * std::max(0.f, -cs),
            .063f * sn - .002f
          };
          p = p * (1 - join) + top * join;
          // A local anterior bend and broader posterior blend distribute
          // compression around the knee; whole calf/thigh slabs must not
          // rotate into one another in the deep-crouch diagnostic.
          const float knee_span = .100f + .120f * std::max(0.f, -sn);
          const float knee_low = -.225f - .5f * knee_span;
          const float knee_high = -.225f + .5f * knee_span;
          CharacterInfluence w =
          y < knee_low ? wt(shin)
          : y < knee_high
          ? wt(shin, thigh, 1 - smooth((y - knee_low) / knee_span))
          : wt(thigh, B::Pelvis, 1 - join);
          if (y < -.390f && join == 0.f)
          w = wt(foot,shin,player_body_ankle_foot_weight(y));
          if (v == 1)
          w = wt(B::Pelvis);
          return P{
            p, p.y>.034f ? Vec3{.055f,.065f,.075f} : cloth*(.93f+.07f*smooth((y+.25f)/.07f)), w, character_surface_uv(S::Pants, u, v)
          };
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
        const float x = side * .054f;
        b.grid(b.near ? 28 : 10, 3, [&](float u, float v) {
          float a = 2 * pi * u;
          return P{
            {
              x + .032f * std::cos(a), lerp(-.5f, -.481f, v),
              .029f + .067f * std::sin(a)
            },
            {
              .055f, .065f, .075f
            },
            wt(foot),
            character_surface_uv(S::Rubber, u, v)
          };
        });
        const auto boot_first_triangle = b.m.bind_mesh.indices.size();
        b.grid(b.near ? 32 : 10, b.near ? 10 : 3, [&](float u, float v) {
          float a = 2 * pi * u, yy = lerp(-.481f, -.435f, v);
          return P{
            {
              x + lerp(.032f, .0225f, smooth(v)) * std::cos(a), yy,
              lerp(.029f, .004f, smooth(v)) +
              lerp(.066f, .025f, smooth(v)) * std::sin(a)
            },
            b.m.appearance.shoes,
            wt(foot),
            character_surface_uv(S::Leather, u, v)
          };
        });
        const auto boot_last_triangle = b.m.bind_mesh.indices.size();
        const auto boot_top = [&](float px, float pz) {
          float highest = -1.f;
          const auto &mesh = b.m.bind_mesh;
          for (std::size_t t = boot_first_triangle; t < boot_last_triangle;
          t += 3) {
            Vec3 a = mesh.vertices[mesh.indices[t]].position * (1 / b.h),
            c = mesh.vertices[mesh.indices[t + 1]].position * (1 / b.h),
            d = mesh.vertices[mesh.indices[t + 2]].position * (1 / b.h);
            const Vec3 e = c - a, f = d - a;
            float det = e.x * f.z - e.z * f.x;
            if (std::fabs(det) < 1e-12f)
            continue;
            float u = ((px - a.x) * f.z - (pz - a.z) * f.x) / det,
            v = (e.x * (pz - a.z) - e.z * (px - a.x)) / det;
            if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f)
            highest = std::max(highest, a.y + u * e.y + v * f.y);
          }
          if (highest < -.6f)
          throw std::runtime_error("PlayerBody boot detail lacks upper support");
          return highest;
        };
        // A narrow welt separates the resilient sole from the sewn upper. Its
        // volume follows the real toe/heel outline, rather than a flat trim box.
        b.grid(
        b.near ? 24 : 10, 1,
        [&](float u, float v) {
          float a = 2 * pi * u;
          return P{
            {
              x + .0325f * std::cos(a), lerp(-.482f, -.4785f, v),
              .029f + .0672f * std::sin(a)
            },
            b.m.appearance.shoes * .65f,
            wt(foot),
            character_surface_uv(S::Leather, u, v)
          };
        },
        true, false);
        b.grid(
        b.near ? 4 : 2, b.near ? 6 : 2,
        [&](float u, float v) {
          float px = x + lerp(-.0115f, .0115f, u), z = lerp(.023f, .056f, v);
          return P{
            {
              px, boot_top(px, z) + .00045f, z
            },
            b.m.appearance.shoes * .87f,
            wt(foot),
            character_surface_uv(S::Leather, u, v)
          };
        },
        false, false);
        // The sock upper is derived from the actual lower trouser rings, including
        // their LOD-specific points and ankle weights. An independent ellipse and
        // independently sampled blend cut through the cuff in static crouch.
        // The supported top ring terminates inside the actual open cuff while
        // the lower ring continues inside the existing low-top shoe.
        const int sock_rows=2;
        std::array<Vec3,3> ring_centers{};
        for(int j=0;j<3;++j)for(int i=0;i<columns;++i)
        ring_centers[j]+=b.m.bind_mesh.vertices[base+j*(columns+1)+i].position*(1/(b.h*columns));
        b.grid(columns,sock_rows,[&](float u,float v){
          const int row=std::min(sock_rows,int(std::lround(v*sock_rows)));
          const int i=std::min(columns,int(std::lround(u*columns)));
          if(row>=2){
            const int source_column=side>0?i:(columns/2-i+columns)%columns;
            const unsigned source=base+(row-2)*(columns+1)+source_column;
            Vec3 p=b.m.bind_mesh.vertices[source].position*(1/b.h);
            const Vec3 c=ring_centers[row-2];
            p-=normalize(Vec3{
              p.x-c.x,0,p.z-c.z
            })*.005f;
            return P{
              p,Vec3{
                .071f,.078f,.083f
              },b.m.influences[source],character_surface_uv(S::Shirt,u,v)
            };
          }
          const float a=2*pi*u,y=row==0?-.470f:-.452f,rx=row==0?.0225f:.0240f;
          return P{
            {
              x+rx*std::cos(a),y,.005f+rx*.93f*std::sin(a)
            },Vec3{
              .071f,.078f,.083f
            },
            wt(foot,shin,player_body_ankle_foot_weight(y)),character_surface_uv(S::Shirt,u,v)
          };
        },true,false);
        // Individually fitted laces lie over the actual LOD upper and tongue.
        // Their prior fixed coordinates had disappeared inside the raised boot.
        for (int lace = 0; lace < (b.near ? 4 : 1); ++lace) {
          float z = .030f + lace * .007f;
          std::vector<Vec3> path;
          const int segments = b.near ? 4 : 2;
          for (int i = 0; i <= segments; ++i) {
            float t = float(i) / segments, px = x + lerp(-.011f, .011f, t),
            pz = z + .0014f * std::sin(pi * t);
            path.push_back({
              px, boot_top(px, pz) + .0010f, pz
            });
          }
          b.curve(path, .00050f, {
            .10f, .09f, .075f
          }, S::Shirt, foot,
          b.near ? 6 : 4);
        }
      }
      for (std::size_t i = 0; i < seams[0].size(); ++i)
      b.normal_groups.push_back({
        seams[0][i], seams[1][i]
      });
      b.grid(
      b.near ? 48 : 20, 3,
      [&](float u, float v) {
        float a = 2 * pi * u;
        return P{
          {
            .0985f * std::cos(a), lerp(.0565f, .071f, v),
            .064f * std::sin(a) - .002f
          },
          cloth * .94f,
          wt(B::Pelvis),
          character_surface_uv(S::Pants, u, v)
        };
      },
      true, false);
    }
    float player_body_hairline(float a) {
      const float front = std::max(0.f, std::sin(a)),
      rear = std::max(0.f, -std::sin(a));
      return .438f + .024f * front - .040f * rear +
      .006f * std::pow(std::fabs(std::cos(a)), 10.f) +
      .0008f * std::sin(5 * a + .7f);
    }
    Vec3 player_body_scalp(float a, float t, float lift = 0) {
      const float y =
      lerp(player_body_hairline(a), .5f, std::sin(std::clamp(t, 0.f, 1.f) * pi * .5f));
      const float cz =
      lerp(player_body_anatomy::head_center.z, player_body_anatomy::crown_joint.z,
      smooth((y - .456f) / .037f));
      const float source_y = y - .007f * smooth((y - .448f) / .052f);
      float r =
      anatomy_radius(std::min(source_y, .49295f), a, cz) +
      (.0028f + lift) *
      std::sqrt(std::max(0.f, 1 - sq(std::max(0.f, (y - .478f) / .022f))));
      r += .0007f * std::sin(a * 3 + t * 2) * std::sin(pi * t);
      return {
        std::max(.00022f, r) * std::cos(a), y,
        cz + std::max(.00022f, r) * std::sin(a)
      };
    }
    Vec3 player_body_bun(float a,float v,float lift=0.f) {
      // Continuous gathered-knot envelope: a narrow buried root broadens smoothly
      // into an asymmetric folded rear mass. No stepped rings or exposed caps.
      const float e=lerp(-pi*.497f,pi*.497f,v),angle=a+.72f*v;
      const float radius=(.026f+lift)*std::cos(e)*(.82f+.18f*smooth(v/.5f));
      const float fold=1+.035f*std::sin(3*angle+v*5)+.015f*std::sin(7*angle-v*3);
      return {
        .002f+radius*fold*std::cos(angle),.444f+radius*.90f*std::sin(angle),-.047f-.025f*std::sin(e)
      };
    }
    Vec3 player_body_source_fold(float a,float v,float lift=0.f,float detail=1.f) {
      // An asymmetric folded knot with a buried root and a tucked rear center.
      // Broad alternating lobes establish the form; fine strands stay material.
      const float e=lerp(-pi*.497f,pi*.497f,v),angle=a+3.2f*v;
      const float radius=(.026f+lift)*std::cos(e)*(.79f+.21f*smooth(v/.48f));
      const float fold=1+.065f*std::sin(2*angle+v*1.4f)+detail*(.055f*std::sin(7*angle-v*1.2f)+.018f*std::sin(11*angle-v*3.1f));
      const float diagonal=std::remainder(angle-1.8f*v-.55f,2*pi);
      const float crease=(.001f+.0016f*detail)*g(diagonal,0,.30f)*std::sin(pi*v);
      const float shaped=radius*fold-crease;
      return {
        .002f+.91f*shaped*std::cos(angle),.444f+shaped*std::sin(angle),
        -.042f-.024f*std::sin(e)+.017f*g(v,1.f,.23f)+.0018f*std::sin(2*angle)*std::sin(pi*v)
      };
    }
    using PlayerSourceHairNormals = std::vector<std::pair<std::size_t, Vec3>>;
    PlayerSourceHairNormals make_player_body_source_hair(Sculpt&b) {
      std::vector<std::pair<std::size_t,Vec3>> normals;
      const Vec3 hair=b.m.appearance.hair;
      const auto append=[&](const auto&vertices,const auto&indices,bool free_tail=false) {
        const auto base=std::uint32_t(b.m.bind_mesh.vertices.size());
        const auto triangle_begin=std::uint32_t(b.m.bind_mesh.indices.size());
        for(const auto&v:vertices){
          const auto influence=free_tail?wt(B::Head,B::Neck,1.f-(b.near?.65f:.40f)*smooth((.435f-v.p.y)/.070f)):wt(B::Head);
          const auto index=b.vertex({v.p,hair*.78f,influence,character_surface_uv(v.tile?S::HairBundle:S::HairSwept,v.uv.x,v.uv.y)});
          normals.push_back({index,v.n});
        }
        for(std::size_t i=0;i<indices.size();i+=3)b.tri(base+indices[i],base+indices[i+1],base+indices[i+2]);
        if(free_tail) {
          // Preserve smooth UV copies but never merge authored hard-normal
          // splits. Only this free tail needs posed geometric normals.
          std::map<std::array<float,6>,unsigned> lookup;
          std::vector<unsigned> groups(vertices.size());
          std::vector<std::vector<unsigned>> members,triangles;
          for(unsigned i=0;i<vertices.size();++i) {
            const auto& v=vertices[i];
            auto item=lookup.emplace(std::array<float,6>{v.p.x,v.p.y,v.p.z,v.n.x,v.n.y,v.n.z},unsigned(lookup.size()));
            const auto g=item.first->second;
            if(item.second){members.emplace_back();triangles.emplace_back();}
            groups[i]=g;members[g].push_back(base+i);
          }
          for(unsigned t=0;t<indices.size();t+=3) {
            const unsigned ids[]{groups[indices[t]],groups[indices[t+1]],groups[indices[t+2]]};
            for(int k=0;k<3;++k)
              if(k==0 || (ids[k]!=ids[0] && (k==1 || ids[k]!=ids[1])))
                triangles[ids[k]].push_back(triangle_begin+t);
          }
          CharacterDeformedNormalRegion region;
          region.vertex_begin=base;region.vertex_count=unsigned(vertices.size());
          region.member_offsets.push_back(0);region.triangle_offsets.push_back(0);
          for(unsigned g=0;g<members.size();++g) {
            region.members.insert(region.members.end(),members[g].begin(),members[g].end());
            region.triangles.insert(region.triangles.end(),triangles[g].begin(),triangles[g].end());
            region.member_offsets.push_back(unsigned(region.members.size()));
            region.triangle_offsets.push_back(unsigned(region.triangles.size()));
          }
          b.m.deformed_normal_regions.push_back(std::move(region));
        }
      };
      if(b.near)append(player_body_hair::near_vertices,player_body_hair::near_indices);
      else append(player_body_hair::far_vertices,player_body_hair::far_indices);
      if(player_short_tail_enabled()) {
        if(b.near)append(player_body_short_tail::vertices,player_body_short_tail::indices,true);
        else append(player_body_short_tail::far_vertices,player_body_short_tail::far_indices,true);
        auto& region=b.m.deformed_normal_regions.back();
        const auto attach=[&](const auto& edges) {
          for(const auto& e:edges)region.edge_attachments.push_back({region.vertex_begin+e.vertex,region.vertex_begin+e.first,region.vertex_begin+e.second,e.fraction});
        };
        if(b.near)attach(player_body_short_tail::edge_attachments);
        else attach(player_body_short_tail::far_edge_attachments);
        return normals;
      }

      // Keep the Player's compact bun identity. Only the scalp and swept layers
      // come from the licensed core asset; the long ponytail is never imported.
      const Vec3 fold_offset{0,.015f,-.004f};
      b.grid(b.near?40:20,b.near?16:9,[&](float u,float v){
        const float a=u==1.f?0.f:2*pi*u;
        return P{player_body_source_fold(a,v,0,b.near?1.f:0.f)+fold_offset,hair*.76f,wt(B::Head),character_surface_uv(S::HairSourceFold,u,v)};
      });
      return normals;
    }
    void make_player_body_hair(Sculpt &b) {
      const Vec3 hair = b.m.appearance.hair;
      const int columns = b.near ? 48 : 24, rows = b.near ? 14 : 7;
      const auto cap_begin = b.m.bind_mesh.indices.size();
      const auto start = unsigned(b.m.bind_mesh.vertices.size());
      b.grid(
      columns, rows,
      [&](float u, float v) {
        float a = 2 * pi * u;
        return P{
          player_body_scalp(a, .018f + .982f * v, b.near ? 0 : .0028f),
          hair * .82f, wt(B::Head),
          character_surface_uv(S::HairCrop, u, v)
        };
      },
      true, false);
      const auto pole = b.vertex({
        {
          0, .5f, player_body_anatomy::crown_joint.z
        },
        hair * .82f,
        wt(B::Head),
        character_surface_uv(S::HairCrop, .5f, 1.f)
      });
      for (int i = 0; i < columns; ++i)
      b.tri(start + rows * (columns + 1) + i, pole,
      start + rows * (columns + 1) + i + 1);
      const auto cap_end = b.m.bind_mesh.indices.size();
      const auto fit_gathered_card = [&](Vec3 p, bool skin_fallback = false) {
        const Vec3 origin{
          0, p.y, player_body_anatomy::crown_joint.z
        };
        const Vec3 direction = normalize(p - origin);
        float hit = -1;
        Vec3 normal{};
        const auto &mesh = b.m.bind_mesh;
        for (std::size_t t = cap_begin; t < cap_end; t += 3) {
          const Vec3 a = mesh.vertices[mesh.indices[t]].position * (1 / b.h),
          c = mesh.vertices[mesh.indices[t + 1]].position * (1 / b.h),
          d = mesh.vertices[mesh.indices[t + 2]].position * (1 / b.h);
          if (p.y < std::min({
            a.y, c.y, d.y
          }) - 1e-6f ||
          p.y > std::max({
            a.y, c.y, d.y
          }) + 1e-6f)
          continue;
          const Vec3 e1 = c - a, e2 = d - a, q = cross(direction, e2);
          const float det = dot(e1, q);
          if (std::fabs(det) < 1e-12f)
          continue;
          const Vec3 off = origin - a, r = cross(off, e1);
          const float u = dot(off, q) / det, v = dot(direction, r) / det,
          dist = dot(e2, r) / det;
          if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f && dist > hit) {
            hit = dist;
            normal = normalize(cross(e1, e2));
            if (dot(normal, direction) < 0)
            normal = -normal;
          }
        }
        // Only the transparent root corners may be outside the cap's open edge.
        // Opaque gathered fibers sit just above actual cap polygons, not above an
        // analytic envelope that can float millimeters above coarse chords.
        // Boundary-card roots nest into the opaque shell; only their short tips
        // extend onto the scalp. A raised root row casts a dotted seam in path
        // light.
        if (hit > 0)
        return origin + direction * hit +
        normal * (skin_fallback ? -.00010f : (b.near ? .00010f : .00015f));
        if (skin_fallback) {
          const float angle = std::atan2(direction.z, direction.x);
          const float radius = anatomy_radius(p.y, angle, origin.z);
          if (radius > 0)
          return origin + direction * (radius + .00008f);
        }
        return p;
      };
      // A compact high rear knot is fitted to this actor's occiput. Its root,
      // folded volume and central return share one continuous authored surface.
      b.grid(b.near?32:16,b.near?15:7,[&](float u,float v){
        const float a=u==1.f?0.f:2*pi*u;
        return P{
          player_body_bun(a,v),hair*(.88f+.025f*std::sin(3*a+v*5)),wt(B::Head),character_surface_uv(S::Hair,u,v)
        };
      });
      // The sheet's width is perpendicular to the local growth tangent. Pure
      // azimuth width folds at the apex where the growth changes vertical sign.
      for (int k = 0; k < (b.near ? 18 : 4); ++k) {
        const float side = k % 2 ? 1.f : -1.f,
        offset = float(k / 2) / (b.near ? 9.f : 2.f);
        const auto center = [&](float v) {
          const float start = pi * .5f + side * (.12f + offset * 1.12f);
          const float angle = start + side * (pi - .12f - offset * 1.12f) * v;
          const float y = lerp(player_body_hairline(start) + .005f, .443f, v) +
          (.030f - .015f * offset) * std::sin(pi * v);
          const float t = std::asin(std::clamp((y - player_body_hairline(angle)) /
          (.5f - player_body_hairline(angle)),
          0.f, 1.f)) /
          (pi * .5f);
          return player_body_scalp(angle, t,
          (b.near ? .0008f : .0038f) + .0005f * std::sin(pi * v));
        };
        b.grid(
        2, b.near ? 10 : 6,
        [&](float u, float v) {
          Vec3 p = center(v),
          tangent = normalize(center(std::min(1.f, v + .001f)) -
          center(std::max(0.f, v - .001f)));
          Vec3 normal = normalize(Vec3{
            p.x, p.y - .437f, p.z - .023f
          });
          Vec3 width = normalize(cross(tangent, normal));
          p += width * ((.5f - u) * (.0130f - .0030f * v));
          p = fit_gathered_card(p);
          return P{
            p, hair * .83f, wt(B::Head),
            character_surface_uv(S::HairShortStrands, u, v)
          };
        },
        false, false);
      }
      // Opaque bundle texture supplies the knot's fine fiber direction. Separate
      // wide overlay cards formed visible scalloped fins in the rejected trial.
      std::vector<Vec3> tie;
      for (int i = 0; i <= 24; ++i) {
        float a = 2 * pi * i / 24;
        tie.push_back(
        {
          .002f + .0175f * std::cos(a), .444f + .0158f * std::sin(a), -.030f
        });
      }
      b.curve(tie, .0015f, {
        .023f, .024f, .025f
      }, S::Rubber, B::Head,
      b.near ? 6 : 4, .60f);
      const int cards = b.near ? 44 : 12;
      for (int k = 0; k < cards; ++k) {
        float a0 = 2 * pi * float(k) / cards;
        bool nape = std::sin(a0) < -.3f;
        float width = b.near ? .11f : .20f;
        b.grid(
        2, b.near ? 6 : 3,
        [&](float u, float v) {
          float a = a0 + (.5f - u) * width + .027f * std::sin(pi * v),
          t = lerp(.21f, nape ? -.040f : -.027f, v);
          Vec3 p = player_body_scalp(a, std::max(0.f, t),
          .0004f + .0005f * std::sin(pi * v));
          if (t < 0)
          p.y += .10f * t;
          p = fit_gathered_card(p, true);
          return P{
            p, hair * .84f, wt(B::Head),
            character_surface_uv(S::HairShortStrands, u, v)
          };
        },
        false, false);
      }
    }
#include "character_player_body_equipment.inl"
    CharacterRig player_body_rig(float height) {
      CharacterRig r;
      r.height = height;
      auto joint = [&](B bone, int parent, Vec3 p) {
        r.joints[bone_index(bone)] = {
          parent, p * height
        };
      };
      auto child = [&](B bone, B parent, Vec3 p) {
        joint(bone, int(bone_index(parent)), p);
      };
      joint(B::Pelvis, -1, {
        0, 0, 0
      });
      child(B::Spine, B::Pelvis, {
        0, .13f, 0
      });
      child(B::Chest, B::Spine, {
        0, .28f, 0
      });
      child(B::Neck, B::Chest, player_body_anatomy::neck_center);
      child(B::Head, B::Neck, player_body_anatomy::head_center);
      for (float sign : {
        -1.f, 1.f
      }) {
        bool q = sign > 0;
        B upper = q ? B::RightUpperArm : B::LeftUpperArm,
        fore = q ? B::RightForearm : B::LeftForearm,
        hand = q ? B::RightHand : B::LeftHand,
        thigh = q ? B::RightThigh : B::LeftThigh,
        shin = q ? B::RightShin : B::LeftShin,
        foot = q ? B::RightFoot : B::LeftFoot;
        child(upper, B::Chest, {
          sign * .124f, .322f, 0
        });
        child(fore, upper, {
          sign * .153f, .12f, .006f
        });
        child(hand, fore, {
          sign * .173f, -.039f, .008f
        });
        child(thigh, B::Pelvis, {
          sign * .054f, 0, 0
        });
        child(shin, thigh, {
          sign * .054f, -.225f, 0
        });
        child(foot, shin, {
          sign * .054f, -.46f, 0
        });
      }
      return r;
    }
  } // namespace
  CharacterProfile make_player_body_character_profile(float height, CharacterRole role,
  std::string_view identity,
  CharacterLod lod) {
    if (!std::isfinite(height) || height <= 0 ||
    (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid individual character request");
    CharacterProfile p;
#if FURY_ENABLE_EXPERIMENTAL_EQUIPPED_CROUCH
    // Asset-owned equipment policy. Both LODs use the same policy; the numeric
    // animator receives no identity or role and does not infer it from seeds.
    p.animation_profile = CharacterAnimationProfile::EquippedCrouchExperimental;
#endif
    auto &m = p.model;
    m.role = role;
    m.lod = lod;
    m.seed = character_seed(identity);
    m.appearance = character_appearance(m.seed);
    m.rig = player_body_rig(height);
    m.dual_quaternion_skinning = true;
    m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 34000 : 17000);
    m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 120000 : 48000);
    m.influences.reserve(m.bind_mesh.vertices.capacity());
    Sculpt b{
      m, height, lod == CharacterLod::Near, {}, {}
    };
    const auto shirt =
    make_player_body_shirt(b, anatomy_radius, player_body_anatomy::neck_center, player_body_skin_weight);
    make_hands_and_forearms(b);
    make_trousers_and_shoes(b);
    make_head(b);
    PlayerSourceHairNormals source_hair_normals;
    // Validate dependencies even when source hair is disabled. All options are off by default.
    if (player_short_tail_enabled() || player_source_hair_enabled())
      source_hair_normals = make_player_body_source_hair(b);
    else
      make_player_body_hair(b);
    make_player_body_equipment(b, shirt);
    b.finish();
    for (const auto& item : source_hair_normals)
      m.bind_mesh.vertices[item.first].normal = item.second;
    if (!m.deformed_normal_regions.empty()) {
      auto bind = m.bind_mesh;
      apply_character_pose(m, CharacterPose{}, bind);
      for (const auto& region : m.deformed_normal_regions)
        for (unsigned i = region.vertex_begin; i < region.vertex_begin + region.vertex_count; ++i)
          m.bind_mesh.vertices[i].normal = bind.vertices[i].normal;
    }
    p.material = make_character_surface_material();
    p.material.color_detail = make_player_hair_color_detail();
    p.material.alpha_cutoff = .4f;
    p.lod_distance = 12;
    p.individualized = true;
    return p;
  }
} // namespace fury
