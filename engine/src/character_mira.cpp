#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/mira_anatomy.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
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
  for (std::size_t i = 0; i < mira_anatomy::indices.size(); i += 3) {
    const Vec3 a = mira_anatomy::positions[mira_anatomy::indices[i]];
    const Vec3 b = mira_anatomy::positions[mira_anatomy::indices[i + 1]];
    const Vec3 c = mira_anatomy::positions[mira_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < mira_anatomy::indices.size(); i += 3) {
    const Vec3 a = mira_anatomy::positions[mira_anatomy::indices[i]];
    const Vec3 b = mira_anatomy::positions[mira_anatomy::indices[i + 1]];
    const Vec3 c = mira_anatomy::positions[mira_anatomy::indices[i + 2]];
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
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      mira_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < mira_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = mira_anatomy::indices[triangle + corner];
      const auto uv_index = mira_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = mira_anatomy::positions[index];
        const Vec2 uv = mira_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .018f) *
                          g(position.y, .389f, .0045f) *
                          smooth((position.z - .068f) / .012f);
        Vec3 color = skin;
        color.y *= 1 - .21f * lip;
        color.z *= 1 - .15f * lip;
        const auto vertex = b.vertex(
            {position, color,
             wt(B::Head, B::Neck, smooth((position.y - .345f) / .044f)),
             character_surface_uv(S::Face, uv.x, uv.y)});
        groups[index].push_back(vertex);
        found = corners.emplace(key, vertex).first;
      }
      out[corner] = found->second;
    }
    b.tri(out[0], out[1], out[2]);
  }
  for (auto &group : groups)
    if (group.size() > 1)
      b.normal_groups.push_back(std::move(group));
  for (const Vec3 eye : mira_anatomy::eye_centers) {
    b.ellipsoid(eye, {.0088f, .0085f, .0092f}, {.77f, .78f, .74f}, S::Eyes,
                B::Head, b.near ? 24 : 12, b.near ? 10 : 6);
    auto disc = [&](float radius, float depth, Vec3 color) {
      b.ellipsoid(eye + Vec3{0, 0, depth}, {radius, radius, .00055f}, color,
                  S::Eyes, B::Head, b.near ? 24 : 12, b.near ? 5 : 3);
    };
    disc(.0036f, .00905f, b.m.appearance.eyes * .24f);
    disc(.00315f, .00936f, b.m.appearance.eyes);
    disc(.00145f, .00972f, {.006f, .007f, .006f});
    std::vector<Vec3> brow;
    std::vector<float> brow_radius;
    const int count = b.near ? 28 : 12;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, x = eye.x + lerp(-.012f, .012f, t),
            outward = eye.x > 0 ? t : 1 - t,
            y = .4425f + .0030f * std::sin(pi * outward) - .0010f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00016f + .00067f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .78f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 8 : 6);
  }
}
float hair_bottom(float angle) {
  float f = std::max(0.f, std::sin(angle)),
        back = std::max(0.f, -std::sin(angle));
  return .428f + .040f * f - .056f * back - .010f * f * std::cos(angle);
}
Vec3 hair_point(float u, float v) {
  v = std::sin(v * pi * .5f);
  const float angle = 2 * pi * u, y = lerp(hair_bottom(angle), .5f, v);
  const float center_z =
      lerp(mira_anatomy::head_center.z, mira_anatomy::crown_joint.z,
           smooth((y - .46f) / .033f));
  const float sample_y = std::min(y, .489f);
  const float radius = anatomy_radius(sample_y, angle, center_z);
  const float top = std::clamp((y - .489f) / .011f, 0.f, 1.f);
  const float wave = .00045f * std::sin(angle * 13 + v * 3) * std::sin(pi * v);
  const float hair_radius =
      std::max(.00035f, (radius + .0025f + wave) *
                            std::sqrt(std::max(0.f, 1 - top * top)));
  return {hair_radius * std::cos(angle), y,
          center_z + hair_radius * std::sin(angle)};
}
void make_hair(Sculpt &b) {
  Vec3 hair = b.m.appearance.hair;
  hair = hair * .75f + Vec3{.026f, .012f, .007f};
  b.grid(b.near ? 64 : 24, b.near ? 16 : 8, [&](float u, float v) {
    float tone = .93f + .07f * std::sin(2 * pi * u * 7 + v * 4);
    return P{hair_point(u, v), hair * tone, wt(B::Head),
             character_surface_uv(S::Hair, u, v)};
  });
  // Directed overlapping locks continue over crown and down the rear
  // silhouette.
  int locks = b.near ? 18 : 4;
  for (int k = 0; k < locks; ++k) {
    std::vector<Vec3> path;
    std::vector<float> radii;
    int count = b.near ? 10 : 6;
    float root = .08f + float(k) / locks * .88f;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, v = lerp(.88f, .035f, t),
            u = root + .075f * std::sin(t * pi) * std::sin(root * 2 * pi);
      auto p = hair_point(u, v);
      Vec3 radial = normalize(Vec3{p.x, 0, p.z + .006f});
      p += radial * .00005f;
      path.push_back(p);
      radii.push_back(.00045f * (.15f + .85f * std::sin(pi * t)));
    }
    b.tube(
        path, radii, .22f, hair, S::Hair, [](float) { return wt(B::Head); }, 8);
  }
}
CharacterInfluence torso_weight(float y) {
  if (y < .1f)
    return wt(B::Pelvis, B::Spine, 1 - smooth((y - .045f) / .055f));
  if (y < .21f)
    return wt(B::Spine, B::Chest, 1 - smooth((y - .10f) / .11f));
  return wt(B::Chest);
}
float jacket_rx(float y) {
  return .087f + .018f * smooth((y - .10f) / .15f) - .004f * g(y, .14f, .045f);
}
float jacket_rz(float y) {
  return .055f + .012f * g(y, .235f, .07f) + .004f * g(y, .07f, .025f);
}
float jacket_z(float x, float y, bool front = true) {
  const float rx = jacket_rx(y), rz = jacket_rz(y);
  auto fold = [&](float angle) {
    return .0024f * g(y, .085f, .029f) *
               std::sin((y - .04f) * 160 + angle * 2.f) +
           .0018f * g(y, .225f, .06f) * std::sin(angle * 5 + y * 12);
  };
  float lo = front ? 0.f : pi, hi = front ? pi : 2 * pi;
  for (int i = 0; i < 22; ++i) {
    float angle = (lo + hi) * .5f, xx = (rx + fold(angle)) * std::cos(angle);
    if ((front && xx > x) || (!front && xx < x))
      lo = angle;
    else
      hi = angle;
  }
  float angle = (lo + hi) * .5f;
  return (rz + fold(angle)) * std::sin(angle);
}
float anatomy_upper_skin(float x, float z) {
  float best = -1.f;
  for (std::size_t i = 0; i < mira_anatomy::indices.size(); i += 3) {
    auto a = mira_anatomy::positions[mira_anatomy::indices[i]],
         b = mira_anatomy::positions[mira_anatomy::indices[i + 1]],
         c = mira_anatomy::positions[mira_anatomy::indices[i + 2]];
    if (std::min({a.y, b.y, c.y}) > .354f)
      continue;
    float denominator = (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
    if (std::fabs(denominator) < 1e-12f)
      continue;
    float u = ((x - a.x) * (c.z - a.z) - (z - a.z) * (c.x - a.x)) / denominator,
          v = ((b.x - a.x) * (z - a.z) - (b.z - a.z) * (x - a.x)) / denominator;
    if (u >= 0 && v >= 0 && u + v <= 1) {
      float y = a.y + u * (b.y - a.y) + v * (c.y - a.y);
      if (y <= .354f)
        best = std::max(best, y);
    }
  }
  return best;
}
Vec3 collar_point(float angle) {
  float y = .349f + .004f * std::max(0.f, -std::sin(angle));
  float radius = anatomy_radius(y, angle, mira_anatomy::neck_center.z) + .0045f;
  return {radius * std::cos(angle), y,
          mira_anatomy::neck_center.z + radius * std::sin(angle)};
}
void make_torso(Sculpt &b) {
  Vec3 jacket{.19f, .31f, .40f}, pants{.095f, .12f, .125f};
  const auto torso_base = std::uint32_t(b.m.bind_mesh.vertices.size());
  const int torso_columns = b.near ? 40 : 18, torso_rows = b.near ? 24 : 10;
  b.grid(
      b.near ? 40 : 18, b.near ? 24 : 10,
      [&](float u, float v) {
        float y = lerp(.045f, .343f, v), angle = 2 * pi * u,
              front = std::max(0.f, std::sin(angle));
        float rx = jacket_rx(y), rz = jacket_rz(y);
        float fold = .0024f * g(y, .085f, .029f) *
                         std::sin((y - .04f) * 160 + angle * 2.f) +
                     .0018f * g(y, .225f, .06f) * std::sin(angle * 5 + y * 12);
        float yy = y - .021f * std::pow(std::fabs(std::cos(angle)), 2.f) *
                           smooth((v - .80f) / .20f);
        float x = (rx + fold) * std::cos(angle),
              z = (rz + fold) * std::sin(angle);
        Vec3 c = jacket * (.97f + .03f * front);
        return P{{x, yy, z},
                 c,
                 torso_weight(yy),
                 character_surface_uv(S::Jacket, u, v)};
      },
      true, false);
  b.grid(
      b.near ? 40 : 18, b.near ? 5 : 3,
      [&](float u, float v) {
        const float angle = 2 * pi * u, y = .343f;
        const float fold =
            .0024f * g(y, .085f, .029f) *
                std::sin((y - .04f) * 160 + angle * 2.f) +
            .0018f * g(y, .225f, .06f) * std::sin(angle * 5 + y * 12);
        Vec3 outer{(jacket_rx(y) + fold) * std::cos(angle),
                   y - .021f * sq(std::cos(angle)),
                   (jacket_rz(y) + fold) * std::sin(angle)};
        Vec3 inner = collar_point(angle);
        inner.y -= .002f;
        Vec3 point = outer * (1 - v) + inner * v;
        const float skin = anatomy_upper_skin(point.x, point.z);
        point.y += std::max(0.f, skin + .002f - point.y) * smooth(v / .2f) *
                   (1 - smooth((v - .7f) / .3f));
        return P{point, jacket, wt(B::Chest),
                 character_surface_uv(S::Jacket, u, .82f + .16f * v)};
      },
      true, false);
  // The yoke is part of the same garment; shared boundary normals avoid a
  // hard plate-like lighting edge while retaining the authored shoulder form.
  const auto yoke_base = torso_base + (torso_columns + 1) * (torso_rows + 1);
  for (int i = 0; i <= torso_columns; ++i)
    b.normal_groups.push_back(
        {torso_base + std::uint32_t(torso_rows * (torso_columns + 1) + i),
         yoke_base + std::uint32_t(i)});
  std::vector<Vec3> collar;
  const int collar_steps = b.near ? 48 : 24;
  for (int i = 0; i <= collar_steps; ++i)
    collar.push_back(collar_point(2 * pi * float(i) / collar_steps));
  b.curve(collar, .0025f, jacket * .85f, S::Jacket, B::Chest, b.near ? 8 : 5,
          .75f);
  std::vector<Vec3> zip, seamL, seamR;
  const int zip_steps = b.near ? 24 : 10;
  for (int i = 0; i <= zip_steps; ++i) {
    float y = lerp(.052f, .334f, float(i) / zip_steps);
    float z = jacket_z(0, y) + .0018f;
    zip.push_back({0, y, z});
    seamL.push_back({-.006f, y, jacket_z(-.006f, y) + .0018f});
    seamR.push_back({.006f, y, jacket_z(.006f, y) + .0018f});
  }
  b.tube(
      zip, std::vector<float>(zip.size(), .0010f), .50f, {.34f, .35f, .34f},
      S::Metal, [](float v) { return torso_weight(lerp(.052f, .334f, v)); },
      b.near ? 6 : 4);
  for (auto *seam : {&seamL, &seamR})
    b.tube(
        *seam, std::vector<float>(seam->size(), .00065f), .35f, jacket * .86f,
        S::Jacket, [](float v) { return torso_weight(lerp(.052f, .334f, v)); },
        4);
  for (float side : {-1.f, 1.f}) {
    std::vector<Vec3> pocket;
    int n = b.near ? 24 : 12;
    for (int i = 0; i <= n; ++i) {
      float t = float(i) / n, angle = i == n ? 0.f : 2 * pi * t;
      float x =
          side * .052f +
          .027f * std::copysign(std::pow(std::fabs(std::cos(angle)), .35f),
                                std::cos(angle));
      float y = .115f + .034f * std::copysign(
                                    std::pow(std::fabs(std::sin(angle)), .35f),
                                    std::sin(angle));
      pocket.push_back({x, y, jacket_z(x, y) + .0020f});
    }
    b.tube(
        pocket, std::vector<float>(pocket.size(), .0007f), .4f, jacket * .86f,
        S::Jacket, [](float) { return wt(B::Spine); }, b.near ? 5 : 4);
    if (b.near) {
      std::vector<Vec3> panel;
      for (int i = 0; i <= 20; ++i) {
        float y = lerp(.16f, .277f, float(i) / 20),
              x = side * (.063f - .014f * std::sin(pi * float(i) / 20));
        panel.push_back({x, y, jacket_z(x, y) + .0018f});
      }
      b.curve(panel, .00065f, jacket * .88f, S::Jacket, B::Chest, 4);
    }
  }
  // Seat and hip taper. Garment construction is curved, with a narrow
  // waistband.
  b.grid(b.near ? 40 : 20, b.near ? 12 : 7, [&](float u, float v) {
    float y = lerp(-.048f, .068f, v), angle = 2 * pi * u;
    float rx = .080f + .012f * std::sin(pi * v),
          rz = .049f + .013f * std::sin(pi * v);
    return P{{rx * std::cos(angle), y, rz * std::sin(angle) - .004f},
             pants,
             wt(B::Pelvis),
             character_surface_uv(S::Pants, u, v)};
  });
}
void make_limbs(Sculpt &b) {
  auto skin = b.m.appearance.skin;
  Vec3 jacket{.19f, .31f, .40f}, pants{.095f, .12f, .125f};
  for (float sign : {-1.f, 1.f}) {
    bool right = sign > 0;
    B upper = right ? B::RightUpperArm : B::LeftUpperArm,
      fore = right ? B::RightForearm : B::LeftForearm,
      hand = right ? B::RightHand : B::LeftHand;
    float shoulder = sign * .108f, elbow = sign * .126f, wrist = sign * .148f;
    b.grid(b.near ? 24 : 12, b.near ? 24 : 10, [&](float u, float v) {
      float y = lerp(-.042f, .323f, v), angle = 2 * pi * u;
      float x = y < .12f ? lerp(wrist, elbow, smooth((y + .042f) / .162f))
                         : lerp(elbow, shoulder, smooth((y - .12f) / .16f));
      x = lerp(x, sign * .085f, smooth((y - .29f) / .033f));
      float radius =
          .018f + .011f * g(y, .025f, .07f) + .013f * g(y, .24f, .09f);
      radius *= 1 - .25f * smooth((y - .295f) / .028f);
      radius += .0015f * g(y, .125f, .035f) * std::sin(y * 210 + angle * 2.f) +
                .0011f * g(y, -.028f, .018f) * std::sin(y * 370 + angle);
      auto weight = y < .10f   ? wt(fore)
                    : y < .15f ? wt(fore, upper, 1 - smooth((y - .10f) / .05f))
                               : wt(upper, B::Chest, smooth((.32f - y) / .08f));
      return P{
          {x + radius * std::cos(angle), y, radius * .91f * std::sin(angle)},
          jacket,
          weight,
          character_surface_uv(S::Jacket, u, v)};
    });
    // The bag-side hand retains the accepted bind form: rotating its fingers
    // introduces new contacts with the inherited rigid bag. Refine the free
    // hand only, leaving all bag-side geometry and posing byte-identical.
    if (right) {
      // A shaped palm, four independently tapered fingers and opposable thumb.
      b.grid(b.near ? 20 : 10, b.near ? 8 : 4, [&](float u, float v) {
        float y = lerp(-.108f, -.036f, v), angle = 2 * pi * u,
              rx = lerp(.019f, .015f, smooth((v - .5f) / .5f)),
              rz = .0105f + .002f * std::sin(pi * v);
        return P{{wrist + rx * std::cos(angle), y, rz * std::sin(angle) + .0015f},
                 skin,
                 wt(hand),
                 character_surface_uv(S::Skin, u, v)};
      });
      const float offsets[4] = {-.014f, -.0045f, .005f, .014f},
                  lengths[4] = {.043f, .050f, .046f, .035f};
      for (int finger = 0; finger < 4; ++finger) {
        std::vector<Vec3> path;
        std::vector<float> radii;
        int n = b.near ? 10 : 4;
        for (int i = 0; i <= n; ++i) {
          float t = float(i) / n;
          float x = wrist + sign * offsets[finger] * (1 + .14f * t),
                y = -.099f - lengths[finger] * t,
                z = .002f + .0035f * std::sin(t * pi * .8f);
          path.push_back({x, y, z});
          radii.push_back((.0045f - .0014f * t) *
                          (1 + .08f * std::sin(t * 4 * pi)));
        }
        b.tube(
            path, radii, .88f, skin, S::Skin, [&](float) { return wt(hand); },
            b.near ? 8 : 4);
        if (b.near) {
          auto p = path.back();
          p.y += .006f;
          p.z += .0033f;
          b.ellipsoid(p, {.0030f, .0055f, .00055f},
                      skin * .90f + Vec3{.05f, .03f, .03f}, S::Skin, hand, 8, 4);
        }
      }
      std::vector<Vec3> thumb{{wrist - sign * .012f, -.063f, .002f},
                              {wrist - sign * .024f, -.073f, .008f},
                              {wrist - sign * .028f, -.089f, .012f},
                              {wrist - sign * .030f, -.100f, .015f}};
      b.tube(
          thumb, {.007f, .006f, .0051f, .0039f}, .9f, skin, S::Skin,
          [&](float) { return wt(hand); }, b.near ? 8 : 4);
    } else {
      // Resting hand form is authored in the existing hand-local geometry. Keep
      // the covered wrist ring fixed; rotate the palm only below the cuff.
      const auto hand_begin = b.m.bind_mesh.vertices.size();
      b.grid(b.near ? 20 : 10, b.near ? 8 : 4, [&](float u, float v) {
        float angle = 2 * pi * u,
              lateral = sign * std::cos(angle),
              knuckle = -.108f + .006f * lateral * lateral + .003f * lateral,
              y = lerp(knuckle, -.036f, v),
              rx = lerp(.019f, .015f, smooth((v - .5f) / .5f)),
              rz = .0105f + .002f * std::sin(pi * v);
        // The ulnar edge cups toward the palm without narrowing the wrist.
        const float cup = -.0035f * (1 - v) * (1 - v) * lateral * lateral;
        return P{{wrist + rx * std::cos(angle), y,
                   rz * std::sin(angle) + .0015f + cup},
                 skin,
                 wt(hand),
                 character_surface_uv(S::Skin, u, v)};
      });
      const float offsets[4] = {-.014f, -.0045f, .005f, .014f},
                  lengths[4] = {.043f, .050f, .046f, .035f},
                  roots[4] = {-.096f, -.099f, -.097f, -.094f},
                  curl[4] = {.0085f, .0115f, .014f, .016f};
      for (int finger = 0; finger < 4; ++finger) {
        std::vector<Vec3> path;
        std::vector<float> radii;
        int n = b.near ? 10 : 4;
        for (int i = 0; i <= n; ++i) {
          float t = float(i) / n;
          float x = wrist + sign * offsets[finger] * (1 + .14f * t),
                y = roots[finger] - lengths[finger] * t,
                z = .0015f - curl[finger] * t * t;
          path.push_back({x, y, z});
          radii.push_back((.0045f - .0014f * t) *
                          (1 + .08f * std::sin(t * 4 * pi)));
        }
        b.tube(
            path, radii, .88f, skin, S::Skin, [&](float) { return wt(hand); },
            b.near ? 8 : 4);
        if (b.near) {
          // Keep the existing nail shell seated on the curled distal phalanx.
          const float t = 1.f - .006f / lengths[finger];
          const Vec3 tangent = normalize(Vec3{
              sign * offsets[finger] * .14f, -lengths[finger],
              -2.f * curl[finger] * t});
          const Vec3 across = normalize(cross(Vec3{0, 0, 1}, tangent));
          const Vec3 dorsal = normalize(cross(tangent, across));
          const Vec3 p = Vec3{wrist + sign * offsets[finger] * (1 + .14f * t),
                              roots[finger] - lengths[finger] * t,
                              .0015f - curl[finger] * t * t} + dorsal * .0033f;
          b.grid(8, 4, [&](float u, float v) {
            const float a = 2 * pi * u, e = lerp(-pi * .49f, pi * .49f, v);
            return P{p + across * (.0030f * std::cos(a) * std::cos(e)) -
                             tangent * (.0055f * std::sin(e)) +
                             dorsal * (.00055f * std::sin(a) * std::cos(e)),
                     skin * .90f + Vec3{.05f, .03f, .03f}, wt(hand),
                     character_surface_uv(S::Skin, u, v)};
          });
        }
      }
      std::vector<Vec3> thumb{{wrist - sign * .014f, -.064f, -.003f},
                              {wrist - sign * .024f, -.075f, -.013f},
                              {wrist - sign * .025f, -.087f, -.018f},
                              {wrist - sign * .022f, -.100f, -.019f}};
      b.tube(
          thumb, {.007f, .006f, .0051f, .0039f}, .9f, skin, S::Skin,
          [&](float) { return wt(hand); }, b.near ? 8 : 4);
      // Gradual palm-inward rest orientation, with no rig or per-frame changes.
      // Sculpt::finish computes the normals after this bind-shape transform.
      for (auto i = hand_begin; i < b.m.bind_mesh.vertices.size(); ++i) {
        auto &p = b.m.bind_mesh.vertices[i].position;
        const float turn = sign * (pi / 3.f) *
                           smooth((-.045f - p.y / b.h) / .037f);
        const float x = p.x - wrist * b.h, z = p.z;
        p.x = wrist * b.h + std::cos(turn) * x + std::sin(turn) * z;
        p.z = -std::sin(turn) * x + std::cos(turn) * z;
      }
    }
    B thigh = right ? B::RightThigh : B::LeftThigh,
      shin = right ? B::RightShin : B::LeftShin,
      foot = right ? B::RightFoot : B::LeftFoot;
    float x = sign * .057f;
    b.grid(b.near ? 24 : 12, b.near ? 26 : 12, [&](float u, float v) {
      float y = lerp(-.459f, .007f, v), angle = 2 * pi * u;
      float radius = .027f + .009f * g(y, -.34f, .08f) +
                     .018f * smooth((y + .225f) / .22f);
      radius += .0017f * g(y, -.224f, .027f) * std::sin(y * 220 + angle) +
                .0013f * g(y, -.443f, .023f) * std::sin(y * 310 + angle * 2.f);
      auto weight = y < -.325f ? wt(shin)
                    : y < -.125f
                        ? wt(shin, thigh, 1 - smooth((y + .325f) / .200f))
                        : wt(thigh, B::Pelvis, smooth((.045f - y) / .10f));
      return P{
          {x + radius * std::cos(angle), y,
           radius * 1.10f * std::sin(angle) - .001f +
               .013f * g(y, -.225f, .20f) * std::max(0.f, -std::sin(angle))},
          pants,
          weight,
          character_surface_uv(S::Pants, u, v)};
    });
    // Sole and toe box share the animator's exact supported heel/toe envelope.
    b.grid(b.near ? 28 : 14, 3, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.5f, -.483f, v);
      return P{{x + .032f * std::cos(a), y, .033f + .067f * std::sin(a)},
               {.07f, .072f, .068f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    b.grid(b.near ? 32 : 16, b.near ? 10 : 5, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.483f, -.430f, v),
            rx = lerp(.0315f, .022f, smooth(v)),
            rz = lerp(.066f, .027f, smooth(v)),
            cz = lerp(.033f, 0.f, smooth(v));
      return P{{x + rx * std::cos(a), y, cz + rz * std::sin(a)},
               {.17f, .12f, .083f},
               wt(foot),
               character_surface_uv(S::Leather, u, v)};
    });
    for (int lace = 0; lace < (b.near ? 5 : 3); ++lace) {
      float z = .035f + lace * .006f, y = -.445f - lace * .004f;
      std::vector<Vec3> p{
          {x - .012f, y, z}, {x, y + .0018f, z + .002f}, {x + .012f, y, z}};
      b.curve(p, .00085f, {.39f, .37f, .30f}, S::Shirt, foot, b.near ? 6 : 4);
    }
  }
}
void make_bag(Sculpt &b) {
  Vec3 leather{.26f, .16f, .078f};
  b.grid(b.near ? 32 : 16, b.near ? 12 : 6, [&](float u, float v) {
    float a = u == 1.f ? 0.f : 2 * pi * u, y = lerp(-.10f, .025f, v),
          round = .88f + .12f * std::sin(pi * v);
    float x = .139f + .065f * round *
                          std::copysign(std::pow(std::fabs(std::cos(a)), .38f),
                                        std::cos(a));
    float z = .024f + .027f * round *
                          std::copysign(std::pow(std::fabs(std::sin(a)), .38f),
                                        std::sin(a));
    return P{{x, y, z},
             leather,
             wt(B::Pelvis),
             character_surface_uv(S::Leather, u, v)};
  });
  // A single continuous leather ribbon crosses the fitted shoulder yoke.
  // Its width axis is transported along the entire path, avoiding capped
  // triangle spikes where three separate straps previously met.
  std::vector<Vec3> strap;
  const int strap_steps = b.near ? 24 : 14;
  auto strap_point = [&](float t, bool front) {
    float y = lerp(.326f, .018f, t), x = lerp(-.079f, .139f, t);
    float z = jacket_z(x, y, front) + (front ? .003f : -.003f);
    if (t > .79f)
      z = lerp(z, front ? .053f : -.003f, smooth((t - .79f) / .21f));
    return Vec3{x, y, z};
  };
  for (int i = strap_steps; i >= 0; --i)
    strap.push_back(strap_point(float(i) / strap_steps, false));
  const auto back = strap.back(), front = strap_point(0, true);
  for (int i = 1; i < 12; ++i) {
    float t = float(i) / 12;
    Vec3 point = back * (1 - t) + front * t;
    point.y += .008f * std::sin(pi * t);
    strap.push_back(point);
  }
  for (int i = 0; i <= strap_steps; ++i)
    strap.push_back(strap_point(float(i) / strap_steps, true));
  // Fit against the actual tessellated jacket instead of an approximate arch.
  // This is a one-time construction query, never a per-frame cloth simulation.
  for (auto &point : strap) {
    if (point.y < .30f)
      continue;
    float surface_y = -1.f;
    const auto &mesh = b.m.bind_mesh;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
      const auto &va = mesh.vertices[mesh.indices[i]];
      if (va.uv.x < .5f || va.uv.y >= .5f)
        continue; // Jacket atlas rectangle.
      Vec3 a = va.position * (1 / b.h),
           bv = mesh.vertices[mesh.indices[i + 1]].position * (1 / b.h),
           c = mesh.vertices[mesh.indices[i + 2]].position * (1 / b.h);
      if (std::max({a.y, bv.y, c.y}) < .30f ||
          std::min({a.y, bv.y, c.y}) > .36f)
        continue;
      float d = (bv.x - a.x) * (c.z - a.z) - (bv.z - a.z) * (c.x - a.x);
      if (std::fabs(d) < 1e-12f)
        continue;
      float u = ((point.x - a.x) * (c.z - a.z) -
                 (point.z - a.z) * (c.x - a.x)) /
                d,
            v = ((bv.x - a.x) * (point.z - a.z) -
                 (bv.z - a.z) * (point.x - a.x)) /
                d;
      if (u >= 0 && v >= 0 && u + v <= 1)
        surface_y =
            std::max(surface_y, a.y + u * (bv.y - a.y) + v * (c.y - a.y));
    }
    if (surface_y > 0)
      point.y = std::max(point.y, surface_y + .0018f);
  }
  b.tube(strap, std::vector<float>(strap.size(), .009f), .10f, leather * .86f,
         S::Leather,
         [&](float t) {
           auto index =
               std::min(strap.size() - 1,
                        std::size_t(std::lround(t * (strap.size() - 1))));
           return torso_weight(strap[index].y);
         },
         b.near ? 8 : 6, {1, 0, 0});
  std::vector<Vec3> flap;
  int flap_steps = b.near ? 24 : 12;
  for (int i = 0; i <= flap_steps; ++i) {
    float a = i == flap_steps ? 0.f : 2 * pi * float(i) / flap_steps;
    flap.push_back(
        {.139f + .056f * std::copysign(std::pow(std::fabs(std::cos(a)), .4f),
                                       std::cos(a)),
         -.018f + .034f * std::copysign(std::pow(std::fabs(std::sin(a)), .4f),
                                        std::sin(a)),
         .0515f});
  }
  b.curve(flap, .00085f, leather * .58f, S::Leather, B::Pelvis, b.near ? 5 : 4);
  b.ellipsoid({.139f, -.035f, .053f}, {.006f, .004f, .0015f},
              {.42f, .35f, .20f}, S::Metal, B::Pelvis, 16, 6);
}
CharacterRig mira_rig(float height) {
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
  child(B::Chest, B::Spine, {0, .272f, 0});
  child(B::Neck, B::Chest, mira_anatomy::neck_center);
  child(B::Head, B::Neck, mira_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    bool q = sign > 0;
    B upper = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(upper, B::Chest, {sign * .108f, .320f, 0});
    child(fore, upper, {sign * .126f, .12f, 0});
    child(hand, fore, {sign * .148f, -.044f, 0});
    child(thigh, B::Pelvis, {sign * .057f, 0, 0});
    child(shin, thigh, {sign * .057f, -.225f, 0});
    child(foot, shin, {sign * .057f, -.46f, 0});
  }
  return r;
}
} // namespace
CharacterProfile make_mira_character_profile(float height, CharacterRole role,
                                             std::string_view identity,
                                             CharacterLod lod) {
  CharacterProfile profile;
  if (!std::isfinite(height) || height <= 0.f ||
      (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid individual character request");
  auto &m = profile.model;
  m.role = role;
  m.lod = lod;
  m.seed = character_seed(identity);
  m.appearance = character_appearance(m.seed);
  m.rig = mira_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 36000 : 12000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 180000 : 60000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt sculpt{m, height, lod == CharacterLod::Near, {}, {}};
  make_torso(sculpt);
  make_limbs(sculpt);
  make_head(sculpt);
  make_hair(sculpt);
  make_bag(sculpt);
  sculpt.finish();
  profile.material = make_character_surface_material();
  profile.lod_distance = 12.f;
  profile.individualized = true;
  return profile;
}
} // namespace fury
