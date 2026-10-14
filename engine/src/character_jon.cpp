#include "character_sculpt.hpp"
#include "fury/generated/jon_anatomy.hpp"
namespace fury {
namespace {
using namespace character_detail;
float anatomy_front(float x, float y) {
  float best = -1.f;
  for (std::size_t i = 0; i < jon_anatomy::indices.size(); i += 3) {
    const Vec3 a = jon_anatomy::positions[jon_anatomy::indices[i]];
    const Vec3 b = jon_anatomy::positions[jon_anatomy::indices[i + 1]];
    const Vec3 c = jon_anatomy::positions[jon_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < jon_anatomy::indices.size(); i += 3) {
    const Vec3 a = jon_anatomy::positions[jon_anatomy::indices[i]];
    const Vec3 b = jon_anatomy::positions[jon_anatomy::indices[i + 1]];
    const Vec3 c = jon_anatomy::positions[jon_anatomy::indices[i + 2]];
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
  std::vector<std::vector<std::uint32_t>> groups(jon_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < jon_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = jon_anatomy::indices[triangle + corner];
      const auto uv_index = jon_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = jon_anatomy::positions[index];
        const Vec2 uv = jon_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .018f) *
                          g(position.y, .392f, .0045f) *
                          smooth((position.z - .068f) / .012f);
        Vec3 color = skin;
        color.y *= 1 - .13f * lip;
        color.z *= 1 - .09f * lip;
        const auto vertex = b.vertex(
            {position, color,
             wt(B::Head, B::Neck, smooth((position.y - .355f) / .022f)),
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
  for (std::size_t ei = 0; ei < jon_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = jon_anatomy::eye_centers[ei];
    const Vec3 radius = jon_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 10 : 6);
    const int iris_columns = b.near ? 32 : 12, iris_rows = b.near ? 5 : 3;
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
    const int count = b.near ? 28 : 12;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, x = eye.x + lerp(-.012f, .012f, t),
            outward = eye.x > 0 ? t : 1 - t,
            y = eye.y + .015f + .0023f * std::sin(pi * outward) -
                .0010f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00024f + .00091f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .78f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 8 : 6);
  }
}
CharacterInfluence torso_weight(float y) {
  if (y < .11f)
    return wt(B::Pelvis, B::Spine, 1 - smooth((y - .035f) / .075f));
  if (y < .235f)
    return wt(B::Spine, B::Chest, 1 - smooth((y - .11f) / .125f));
  return wt(B::Chest);
}
float shirt_rx(float y) {
  return .089f + .034f * smooth((y - .10f) / .23f) - .003f * g(y, .15f, .06f);
}
float shirt_rz(float y) {
  return .055f + .014f * g(y, .245f, .11f) + .003f * g(y, .07f, .04f);
}
float shirt_front(float x, float y) {
  float ratio = std::clamp(x / shirt_rx(y), -.999f, .999f);
  return shirt_rz(y) * std::sqrt(1 - ratio * ratio);
}
Vec3 collar_point(float a) {
  float y = .354f - .011f * std::max(0.f, std::sin(a));
  float r = anatomy_radius(y, a, jon_anatomy::neck_center.z) + .0035f;
  return {r * std::cos(a), y, jon_anatomy::neck_center.z + r * std::sin(a)};
}
void make_shirt(Sculpt &b) {
  const auto shirt = b.m.appearance.shirt;
  const int nc = b.near ? 32 : 16, nr = b.near ? 20 : 8;
  const auto base = std::uint32_t(b.m.bind_mesh.vertices.size());
  auto outer = [&](float a) {
    float y = .342f - .020f * sq(std::cos(a));
    return Vec3{shirt_rx(.342f) * std::cos(a), y,
                shirt_rz(.342f) * std::sin(a)};
  };
  b.grid(
      nc, nr,
      [&](float u, float v) {
        float y = lerp(.045f, .342f, v), a = 2 * pi * u,
              fold = .0018f * g(y, .10f, .04f) * std::sin(a * 3 + y * 140) +
                     .0012f * g(y, .25f, .06f) * std::sin(a * 5 + y * 30);
        float yy = y - .020f * sq(std::cos(a)) * smooth((v - .80f) / .2f);
        return P{
            {(shirt_rx(y) + fold * (1 - smooth((v - .8f) / .2f))) * std::cos(a),
             yy,
             (shirt_rz(y) + fold * (1 - smooth((v - .8f) / .2f))) *
                 std::sin(a)},
            shirt,
            torso_weight(yy),
            character_surface_uv(S::Shirt, u, v)};
      },
      true, false);
  const auto yoke = std::uint32_t(b.m.bind_mesh.vertices.size());
  b.grid(
      nc, b.near ? 8 : 4,
      [&](float u, float v) {
        float a = 2 * pi * u;
        Vec3 p = outer(a) * (1 - v) + collar_point(a) * v;
        Vec3 radial{p.x, 0, p.z - jon_anatomy::neck_center.z};
        const float length = std::sqrt(dot(radial, radial));
        if (length > 1e-6f) {
          const float angle = std::atan2(radial.z, radial.x);
          const float clearance =
              anatomy_radius(p.y, angle, jon_anatomy::neck_center.z) + .0025f;
          p += radial * (std::max(0.f, clearance - length) / length) *
               smooth(v / .20f);
        }
        return P{p, shirt, wt(B::Chest),
                 character_surface_uv(S::Shirt, u, .8f + .2f * v)};
      },
      true, false);
  for (int i = 0; i <= nc; ++i)
    b.normal_groups.push_back(
        {base + std::uint32_t(nr * (nc + 1) + i), yoke + std::uint32_t(i)});
  // Soft bound neckline on a work Henley. The binding sits on the garment,
  // rather than using rigid floating collar leaves under the apron loop.
  b.grid(
      b.near ? 40 : 20, 3,
      [&](float u, float v) {
        float a = 2 * pi * u;
        Vec3 p = collar_point(a);
        Vec3 radial = normalize(Vec3{p.x, 0, p.z - jon_anatomy::neck_center.z});
        p += radial * (.0008f + .0040f * v);
        p.y -= .0060f * v;
        p.y += .0010f * std::sin(pi * v);
        return P{p, shirt * .86f, wt(B::Chest),
                 character_surface_uv(S::Shirt, u, v)};
      },
      true, false);
  std::vector<Vec3> placket;
  for (int i = 0; i <= 8; ++i) {
    float y = lerp(.282f, .333f, float(i) / 8);
    placket.push_back({0, y, shirt_front(0, y) + .0018f});
  }
  b.tube(placket, std::vector<float>(placket.size(), .0034f), .15f,
         shirt * .97f, S::Shirt,
         [](float t) { return torso_weight(lerp(.282f, .333f, t)); }, 8,
         {1, 0, 0});
  for (int i = 0; i < (b.near ? 2 : 1); ++i) {
    float y = .299f + i * .030f;
    b.ellipsoid({0, y, shirt_front(0, y) + .003f}, {.0016f, .0016f, .0006f},
                {.30f, .26f, .24f}, S::Rubber, B::Chest, b.near ? 12 : 8, 4);
  }
  // Pants seat and waistband remain distinct from the apron.
  b.grid(b.near ? 32 : 14, b.near ? 10 : 4, [&](float u, float v) {
    float y = lerp(-.049f, .068f, v), a = 2 * pi * u,
          rx = .083f + .010f * std::sin(pi * v),
          rz = .052f + .010f * std::sin(pi * v);
    return P{{rx * std::cos(a), y, rz * std::sin(a) - .003f},
             b.m.appearance.trousers,
             wt(B::Pelvis),
             character_surface_uv(S::Pants, u, v)};
  });
}
void make_limbs(Sculpt &b) {
  auto skin = b.m.appearance.skin;
  Vec3 jacket = b.m.appearance.shirt, pants = b.m.appearance.trousers;
  for (float sign : {-1.f, 1.f}) {
    bool right = sign > 0;
    B upper = right ? B::RightUpperArm : B::LeftUpperArm,
      fore = right ? B::RightForearm : B::LeftForearm,
      hand = right ? B::RightHand : B::LeftHand;
    float shoulder = sign * .119f, elbow = sign * .138f, wrist = sign * .159f;
    // Exposed forearm, with the sleeve and rolled cuff built independently.
    auto arm_x = [&](float y) {
      return y < .12f ? lerp(wrist, elbow, smooth((y + .042f) / .162f))
                      : lerp(elbow, shoulder, smooth((y - .12f) / .20f));
    };
    auto arm_weight = [&](float y) {
      return y < .10f    ? wt(fore)
             : y < .155f ? wt(fore, upper, 1 - smooth((y - .10f) / .055f))
                         : wt(upper, B::Chest, smooth((.335f - y) / .075f));
    };
    b.grid(b.near ? 20 : 8, b.near ? 14 : 7, [&](float u, float v) {
      float y = lerp(-.045f, .18f, v), a = 2 * pi * u,
            rx = .0148f + .010f * g(y, .085f, .075f), rz = rx * .86f;
      float muscle = .0015f * g(y, .088f, .07f) * std::cos(a * 2 - .5f);
      return P{
          {arm_x(y) + (rx + muscle) * std::cos(a), y, rz * std::sin(a) + .001f},
          skin,
          arm_weight(y),
          character_surface_uv(S::Skin, u, v)};
    });
    b.grid(b.near ? 20 : 10, b.near ? 12 : 6, [&](float u, float v) {
      float y = lerp(.15f, .326f, v), a = 2 * pi * u, x = arm_x(y);
      x = lerp(x, sign * .098f, smooth((y - .296f) / .030f));
      float radius = .034f + .004f * g(y, .255f, .07f) -
                     .014f * smooth((y - .293f) / .033f);
      float fold = .0027f * g(y, .177f, .035f) * std::sin(a * 2 + y * 230) +
                   .0014f * g(y, .208f, .045f) * std::sin(a * 3 - y * 165) +
                   .0009f * g(y, .27f, .05f) * std::sin(a * 4 + y * 27);
      return P{{x + (radius + fold) * std::cos(a), y,
                (radius * .95f + fold) * std::sin(a)},
               jacket,
               arm_weight(y),
               character_surface_uv(S::Shirt, u, v)};
    });
    // A real, irregular double roll, rather than a painted cuff line.
    for (int roll = 0; roll < (b.near ? 2 : 1); ++roll)
      b.grid(b.near ? 24 : 12, b.near ? 5 : 3, [&](float u, float v) {
        float a = 2 * pi * u, y = .149f + roll * .009f +
                                  v * (b.near ? .012f : .021f) +
                                  .0015f * std::sin(a * 2 + .4f);
        float radius =
            .034f + .0033f * std::sin(pi * v) + .0008f * std::cos(a * 3);
        return P{
            {arm_x(y) + radius * std::cos(a), y, radius * .96f * std::sin(a)},
            jacket * (roll ? .91f : .96f),
            arm_weight(y),
            character_surface_uv(S::Shirt, u, v)};
      });
    // A relaxed working hand; preserve the bare wrist joining the forearm.
    const auto hand_begin = b.m.bind_mesh.vertices.size();
    b.grid(b.near ? 20 : 10, b.near ? 6 : 3, [&](float u, float v) {
      float angle = 2 * pi * u, lateral = sign * std::cos(angle),
            arc = .0045f * lateral * lateral + .0025f * lateral,
            y = lerp(-.108f, -.036f, v) + arc * (1 - smooth(v / (2.f / 3.f))),
            rx = lerp(.019f, .015f, smooth((v - .5f) / .5f)) +
                 .0018f * (1 - smooth(v / (2.f / 3.f))),
            rz = .0105f + .002f * std::sin(pi * v);
      return P{{wrist + rx * std::cos(angle), y, rz * std::sin(angle) + .0015f -
                    .0028f * (1 - v) * (1 - v) * lateral * lateral *
                    (1 - smooth((v - .5f) * 6.f))},
               skin,
               wt(hand),
               character_surface_uv(S::Skin, u, v)};
    });
    const float offsets[4] = {-.014f, -.0045f, .005f, .014f},
                lengths[4] = {.043f, .050f, .046f, .035f},
                roots[4] = {-.0975f, -.099f, -.098f, -.095f},
                curl[4] = {.006f, .009f, .011f, .013f};
    for (int finger = 0; finger < 4; ++finger) {
      std::vector<Vec3> path;
      std::vector<float> radii;
      int n = b.near ? 10 : 2;
      for (int i = 0; i <= n; ++i) {
        float t = float(i) / n;
        float x = wrist + sign * offsets[finger] * (1 + .14f * t),
              y = roots[finger] - lengths[finger] * t,
              z = .0015f - .0018f * sq(offsets[finger] / .019f) -
                    curl[finger] * t * t;
        path.push_back({x, y, z});
        radii.push_back((.0045f - .0014f * t) *
                        (1 + .08f * std::sin(t * 4 * pi)));
      }
      b.tube(
          path, radii, .88f, skin, S::Skin, [&](float) { return wt(hand); },
          b.near ? 8 : 4);
      if (b.near) {
        // Align each existing nail shell to its distal finger surface.
        const float t = 1.f - .006f / lengths[finger];
        const Vec3 tangent = normalize(Vec3{
            sign * offsets[finger] * .14f, -lengths[finger],
            -2.f * curl[finger] * t});
        const Vec3 across = normalize(cross(Vec3{0, 0, 1}, tangent));
        const Vec3 dorsal = normalize(cross(tangent, across));
        const Vec3 p = Vec3{wrist + sign * offsets[finger] * (1 + .14f * t),
                            roots[finger] - lengths[finger] * t,
                            .0015f - .0018f * sq(offsets[finger] / .019f) -
                    curl[finger] * t * t} + dorsal * .0029f;
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
    std::vector<Vec3> thumb{{wrist - sign * .0105f, -.064f, -.001f},
                            {wrist - sign * .024f, -.075f, -.009f},
                            {wrist - sign * .026f, -.089f, -.014f},
                            {wrist - sign * .024f, -.102f, -.015f}};
    b.tube(
        thumb, {.007f, .006f, .0051f, .0039f}, .9f, skin, S::Skin,
        [&](float) { return wt(hand); }, b.near ? 8 : 4);
    // A modest palm-inward rest turn, below the untouched wrist junction.
    // Only construction-time geometry changes; the existing hand bone is kept.
    for (auto i = hand_begin; i < b.m.bind_mesh.vertices.size(); ++i) {
      auto &p = b.m.bind_mesh.vertices[i].position;
      const float turn = sign * (pi * 7.f / 36.f) *
                         smooth((-.060f - p.y / b.h) / .038f);
      const float x = p.x - wrist * b.h, z = p.z;
      p.x = wrist * b.h + std::cos(turn) * x + std::sin(turn) * z;
      p.z = -std::sin(turn) * x + std::cos(turn) * z;
    }
    B thigh = right ? B::RightThigh : B::LeftThigh,
      shin = right ? B::RightShin : B::LeftShin,
      foot = right ? B::RightFoot : B::LeftFoot;
    float x = sign * .059f;
    b.grid(b.near ? 20 : 10, b.near ? 22 : 9, [&](float u, float v) {
      const float far_y[]{-.459f, -.38f,  -.29f, -.265f, -.240f,
                          -.215f, -.190f, -.15f, -.075f, .007f};
      float y = b.near ? lerp(-.459f, .007f, v)
                       : far_y[std::min(9, int(std::lround(v * 9)))],
            angle = 2 * pi * u;
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
               .012f * g(y, -.225f, .20f) * std::max(0.f, -std::sin(angle))},
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
    b.grid(b.near ? 32 : 12, b.near ? 10 : 4, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.483f, -.430f, v),
            rx = lerp(.0315f, .022f, smooth(v)),
            rz = lerp(.066f, .027f, smooth(v)),
            cz = lerp(.033f, 0.f, smooth(v));
      return P{{x + rx * std::cos(a), y, cz + rz * std::sin(a)},
               b.m.appearance.shoes,
               wt(foot),
               character_surface_uv(S::Leather, u, v)};
    });
    for (int lace = 0; lace < (b.near ? 5 : 3); ++lace) {
      float z = .035f + lace * .006f, y = -.445f - lace * .004f;
      std::vector<Vec3> p{
          {x - .012f, y, z}, {x, y + .0018f, z + .002f}, {x + .012f, y, z}};
      b.curve(p, .00085f, Vec3{.34f, .32f, .28f}, S::Shirt, foot,
              b.near ? 6 : 4);
    }
  }
}

// Jon's gathered hair and complete apron are authored below.
float hairline(float angle) {
  float front = std::max(0.f, std::sin(angle)),
        back = std::max(0.f, -std::sin(angle));
  return .434f + .031f * front - .039f * back -
         .004f * std::cos(angle) * front +
         .0010f * std::sin(angle * 7) * (.3f + back);
}
Vec3 scalp(float angle, float y, float lift = .003f) {
  float z = lerp(jon_anatomy::head_center.z, jon_anatomy::crown_joint.z,
                 smooth((y - .46f) / .033f));
  float r = anatomy_radius(std::min(y, .489f), angle, z);
  if (y > .489f)
    r = std::max(.0003f, (r + lift) * std::sqrt(std::max(
                                          0.f, 1 - sq((y - .489f) / .011f))));
  else
    r += lift;
  return {r * std::cos(angle), y, z + r * std::sin(angle)};
}
void make_hair(Sculpt &b) {
  const Vec3 hair = b.m.appearance.hair * .88f;
  b.grid(b.near ? 48 : 19, b.near ? 16 : 7, [&](float u, float v) {
    float a = 2 * pi * u, y = lerp(hairline(a), .5f, std::sin(v * pi * .5f));
    Vec3 p = scalp(a, y,
                   .0006f + .0024f * smooth(v / .15f) +
                       .0008f * std::sin(a * 5 + v * 3) * std::sin(pi * v) +
                       (b.near ? 0.f : .0025f));
    return P{p, hair * (.97f + .03f * std::sin(a * 3)), wt(B::Head),
             character_surface_uv(S::Hair, u, v)};
  });
  // Broad gathered sections flow from the front/temples toward a compact knot.
  const int locks = b.near ? 8 : 4, steps = b.near ? 18 : 8;
  for (int k = 0; k < locks; ++k) {
    float side = k % 2 ? 1.f : -1.f,
          offset = float(k / 2) / float((locks + 1) / 2);
    std::vector<Vec3> path;
    std::vector<float> radii;
    for (int j = 0; j <= steps; ++j) {
      float t = float(j) / steps;
      float a = pi * .5f +
                side * (.12f + offset * .72f + (pi - .18f - offset * .72f) * t);
      float start_angle = pi * .5f + side * (.12f + offset * .72f);
      float y = lerp(hairline(start_angle) + .002f, .421f, t) +
                (.035f - offset * .006f) * std::sin(pi * t);
      y = std::min(y, .4965f);
      Vec3 p = scalp(a, y, .0031f);
      path.push_back(p);
      radii.push_back((.0016f + .0010f * std::sin(pi * t)) *
                      (.25f + .75f * std::sin(pi * std::min(.98f, t + .01f))));
    }
    b.tube(
        path, radii, .25f, hair * (.98f + .025f * std::sin(float(k))), S::Hair,
        [](float) { return wt(B::Head); }, 8);
  }
  // The knot is a group of overlapping wrapped lobes, with a visible root/tie.
  const Vec3 knot{.002f, .427f, -.053f};
  b.ellipsoid({.002f, .428f, -.034f}, {.018f, .016f, .021f}, hair, S::Hair,
              B::Head, b.near ? 24 : 10, b.near ? 10 : 4);
  b.ellipsoid(knot, {.025f, .023f, .023f}, hair * .95f, S::Hair, B::Head,
              b.near ? 24 : 14, b.near ? 12 : 7);
  const std::array<Vec3, 3> lobes{{{-.008f, .008f, -.002f},
                                   {.006f, -.006f, -.006f},
                                   {.002f, .010f, -.013f}}};
  for (int i = 0; i < (b.near ? 3 : 1); ++i)
    b.ellipsoid(knot + lobes[i],
                {.019f - i * .001f, .014f + i * .001f, .015f - i * .002f},
                hair * (.94f + .025f * i), S::Hair, B::Head, b.near ? 22 : 12,
                b.near ? 10 : 6);
  std::vector<Vec3> tie;
  const int tie_steps = b.near ? 32 : 10;
  for (int i = 0; i <= tie_steps; ++i) {
    float a = 2 * pi * float(i) / tie_steps;
    tie.push_back(
        {.002f + .017f * std::cos(a), .429f + .015f * std::sin(a), -.041f});
  }
  b.curve(tie, .0022f, {.030f, .025f, .022f}, S::Rubber, B::Head,
          b.near ? 8 : 6, .6f);
  if (b.near) {
    // Actual alpha-masked hair cards soften the hairline. Their roots are
    // tucked beneath the opaque mass and tips lie close to the measured scalp.
    for (int card = 0; card < 18; ++card) {
      const float center = lerp(pi * .10f, pi * .90f, (card + .5f) / 18.f);
      b.grid(
          2, 8,
          [&](float u, float v) {
            float a = center + (.5f - u) * .115f + .018f * std::sin(pi * v);
            float y = hairline(a) + lerp(.014f, -.006f, v);
            float lift = .00015f + .00055f * std::sin(pi * v);
            return P{scalp(a, y, lift), hair, wt(B::Head),
                     character_surface_uv(S::HairStrands, u, v)};
          },
          false, false);
    }
    // Sparse wrapped strands break the knot's smooth outline without turning
    // the entire character into a sorted transparent draw.
    for (int card = 0; card < 6; ++card) {
      const float root = 2 * pi * float(card) / 6;
      b.grid(
          2, 8,
          [&](float u, float v) {
            float a = root + .85f * v + (.5f - u) * .17f,
                  e = lerp(.18f * pi, .80f * pi, v);
            Vec3 p = knot + Vec3{.026f * std::cos(a) * std::sin(e),
                                 .024f * std::cos(e),
                                 .024f * std::sin(a) * std::sin(e)};
            return P{p, hair, wt(B::Head),
                     character_surface_uv(S::HairStrands, u, v)};
          },
          false, false);
    }
  }
}
float apron_width(float y) {
  return y < .05f ? lerp(.110f, .095f, smooth((y + .145f) / .195f))
                  : lerp(.095f, .054f, smooth((y - .05f) / .23f));
}
Vec3 apron_point(float u, float v, float back_offset = 0.f) {
  float y = lerp(-.145f, .282f, v), x = apron_width(y) * (1 - 2 * u);
  // The tied waist releases gradually into a shallow wrapped skirt.
  // This is a bind-shape field only: the existing leg-follow weights remain.
  const float falling = smooth((.055f - y) / .080f);
  // Clear the upper trouser envelope before the free skirt rolls rearward.
  const float release = falling * (2 - falling);
  const float across = 2 * u - 1;
  float attached = shirt_front(x, y) + .006f;
  attached += .0020f * std::sin(u * 8 * pi + .3f) * smooth((.065f - y) / .17f) +
      .0012f * std::sin(u * 4 * pi) * g(y, .095f, .07f);
  const float folds = .0040f *
      (g(across, -.42f, .22f) + g(across, .42f, .22f)) *
      (1 - .35f * smooth((-.035f - y) / .11f));
  // The observed 2.1 m/s route brings each upper thigh through this band.
  // A broad waist-supported relief preserves the lower wrap without relying
  // on a slower diagnostic gait or adding runtime cloth/collision logic.
  const float thigh_relief = .005f * g(y, -.035f, .028f) *
      (g(x, -.073f, .022f) + g(x, .073f, .022f));
  const float skirt = .066f + .002f * g(y, -.08f, .075f) -
                      .023f * sq(across) + folds + thigh_relief;
  float z = lerp(attached, skirt, release);
  y += .007f * sq(2 * u - 1) * (1 - smooth((v - .02f) / .10f));
  return {x, y, z - back_offset};
}
CharacterInfluence apron_weight(float x, float y) {
  if (y > .065f)
    return torso_weight(y);
  const float follow = smooth((.065f - y) / .20f);
  const float right = smooth((x + .055f) / .11f);
  return {std::uint8_t(B::Pelvis), std::uint8_t(B::LeftThigh), 1 - follow,
          std::uint8_t(B::RightThigh), follow * right};
}
void make_apron(Sculpt &b) {
  const Vec3 cloth = b.m.appearance.accent;
  const int cols = b.near ? 24 : 12, rows = b.near ? 20 : 10;
  const auto front_base = b.m.bind_mesh.vertices.size();
  for (int back = 0; back < 2; ++back) {
    const auto sheet_base = b.m.bind_mesh.vertices.size();
    const auto sheet_index = b.m.bind_mesh.indices.size();
    const int sheet_rows = (!b.near && back) ? 5 : rows;
    b.grid(
        cols, sheet_rows,
        [&](float u, float v) {
          if (!b.near && back)
            v *= .45667447f; // Retain the authored backing UV range.
          Vec3 p = apron_point(back ? 1 - u : u, v, back ? .0012f : 0.f);
          return P{p, cloth * (back ? .92f : 1.f), apron_weight(p.x, p.y),
                   character_surface_uv(S::Jacket, u, v)};
        },
        false, false);
    if (back) {
      // The two faces of the cloth share lattice points and skinning weights.
      // Far uses the first six front rows; it keeps the same backing budget.
      for (int row = 0; row <= sheet_rows; ++row)
        for (int col = 0; col <= cols; ++col) {
          const auto dst = sheet_base + row * (cols + 1) + col;
          const auto src = front_base + row * (cols + 1) + (cols - col);
          b.m.bind_mesh.vertices[dst].position =
              b.m.bind_mesh.vertices[src].position - Vec3{0, 0, .0012f * b.h};
          b.m.influences[dst] = b.m.influences[src];
        }
      // Mirrored columns reverse the facing. Match the front diagonal too;
      // opposite diagonals cross when a nonplanar skinned quad bends.
      for (int row = 0; row < sheet_rows; ++row)
        for (int col = 0; col < cols; ++col) {
          const auto a = std::uint32_t(sheet_base + row * (cols + 1) + col);
          const auto b1 = a + cols + 1;
          const auto index = sheet_index + 6 * (row * cols + col);
          auto &indices = b.m.bind_mesh.indices;
          indices[index] = a;
          indices[index + 1] = b1;
          indices[index + 2] = a + 1;
          indices[index + 3] = a + 1;
          indices[index + 4] = b1;
          indices[index + 5] = b1 + 1;
        }
    }
  }
  const int seam_steps = b.near ? 24 : 10;
  const int seam_sides = b.near ? 8 : 6;
  for (float edge : {0.f, 1.f}) {
    std::vector<Vec3> path;
    for (int i = 0; i <= seam_steps; ++i)
      path.push_back(apron_point(edge, float(i) / seam_steps) +
                     Vec3{0, 0, .0008f});
    b.tube(
        path, std::vector<float>(path.size(), .0011f), .4f, cloth * .91f,
        S::Jacket,
        [edge](float t) {
          auto p = apron_point(edge, t);
          return apron_weight(p.x, p.y);
        },
        seam_sides);
  }
  std::vector<Vec3> hem;
  for (int i = 0; i <= seam_steps; ++i)
    hem.push_back(apron_point(float(i) / seam_steps, 0) +
                  Vec3{0, .001f, .001f});
  b.tube(
      hem, std::vector<float>(hem.size(), .0013f), .45f, cloth * .95f,
      S::Jacket,
      [](float t) {
        auto p = apron_point(t, 0);
        return apron_weight(p.x, p.y);
      },
      seam_sides);
  // Unbroken neck loop, routed behind the collar and attached to the bib
  // corners.
  std::vector<Vec3> loop{{.052f, .282f, shirt_front(.052f, .282f) + .007f},
                         {.047f, .310f, .067f},
                         {.043f, .341f, .047f}};
  const int loop_steps = b.near ? 24 : 12;
  for (int i = 0; i <= loop_steps; ++i) {
    float a = lerp(.25f, -pi - .25f, float(i) / loop_steps), y = .356f;
    float r = anatomy_radius(y, a, jon_anatomy::neck_center.z) + .008f;
    loop.push_back(
        {r * std::cos(a), y, jon_anatomy::neck_center.z + r * std::sin(a)});
  }
  loop.push_back({-.043f, .341f, .047f});
  loop.push_back({-.047f, .310f, .067f});
  loop.push_back({-.052f, .282f, shirt_front(-.052f, .282f) + .007f});
  b.tube(loop, std::vector<float>(loop.size(), .0048f), .18f, cloth * .96f,
         S::Jacket, [](float) { return wt(B::Chest); }, b.near ? 10 : 6,
         {1, 0, 0});
  // Two waist straps meet at a soft crossed bow on the back.
  std::vector<Vec3> waist;
  const int waist_steps = b.near ? 36 : 18;
  for (int i = 0; i <= waist_steps; ++i) {
    float a = lerp(pi * .20f, -pi * 1.20f, float(i) / waist_steps);
    waist.push_back({.094f * std::cos(a), .055f, .061f * std::sin(a) - .002f});
  }
  b.tube(waist, std::vector<float>(waist.size(), .0048f), .17f, cloth * .91f,
         S::Jacket, [](float) { return wt(B::Pelvis); }, seam_sides, {0, 1, 0});
  for (float side : {-1.f, 1.f}) {
    std::vector<Vec3> bow;
    std::vector<float> widths;
    const int bow_steps = b.near ? 16 : 8;
    for (int i = 0; i <= bow_steps; ++i) {
      float t = float(i) / bow_steps, a = 2 * pi * t;
      bow.push_back({side * (.002f + .022f * std::sin(pi * t)),
                     .053f + .007f * std::sin(a),
                     -.066f - .002f * std::sin(pi * t)});
      widths.push_back(.0033f);
    }
    b.tube(bow, widths, .2f, cloth * .96f, S::Jacket,
           [](float) { return wt(B::Pelvis); }, seam_sides, {0, 1, 0});
    std::vector<Vec3> tail{{side * .004f, .05f, -.067f},
                           {side * .013f, .026f, -.066f},
                           {side * .018f, -.001f, -.069f},
                           {side * .015f, -.033f, -.066f}};
    b.tube(tail, std::vector<float>(tail.size(), .0045f), .16f, cloth,
           S::Jacket, [](float) { return wt(B::Pelvis); }, seam_sides,
           {1, 0, 0});
  }
  // A curved open-top patch pocket with real depth and a doubled top hem.
  b.grid(
      b.near ? 20 : 10, b.near ? 12 : 6,
      [&](float u, float v) {
        float x = .071f * (1 - 2 * u), y = lerp(-.093f, -.024f, v);
        float apron_u = (1 - x / apron_width(y)) * .5f;
        Vec3 p = apron_point(apron_u, (y + .145f) / .427f);
        p.z += .0022f + .0030f * std::sin(pi * u) * std::sin(pi * v * .65f);
        return P{p, cloth * 1.025f, apron_weight(p.x, p.y),
                 character_surface_uv(S::Jacket, u, v)};
      },
      false, false);
  std::vector<Vec3> pocket_hem;
  const int pocket_steps = b.near ? 24 : 10;
  for (int i = 0; i <= pocket_steps; ++i) {
    float u = float(i) / pocket_steps, x = .071f * (1 - 2 * u), y = -.024f;
    Vec3 p = apron_point((1 - x / apron_width(y)) * .5f, (y + .145f) / .427f);
    p.z += .005f;
    pocket_hem.push_back(p);
  }
  b.tube(
      pocket_hem, std::vector<float>(pocket_hem.size(), .0011f), .5f,
      cloth * .92f, S::Jacket,
      [](float t) { return apron_weight(.071f * (1 - 2 * t), -.024f); },
      seam_sides);
}
CharacterRig jon_rig(float height) {
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
  child(B::Neck, B::Chest, jon_anatomy::neck_center);
  child(B::Head, B::Neck, jon_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    bool q = sign > 0;
    B upper = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(upper, B::Chest, {sign * .119f, .322f, 0});
    child(fore, upper, {sign * .138f, .12f, 0});
    child(hand, fore, {sign * .159f, -.044f, 0});
    child(thigh, B::Pelvis, {sign * .059f, 0, 0});
    child(shin, thigh, {sign * .059f, -.225f, 0});
    child(foot, shin, {sign * .059f, -.46f, 0});
  }
  return r;
}
} // namespace
CharacterProfile make_jon_character_profile(float height, CharacterRole role,
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
  m.rig = jon_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 36000 : 16000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 180000 : 90000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  make_shirt(b);
  make_limbs(b);
  make_head(b);
  make_hair(b);
  make_apron(b);
  b.finish();
  p.material = make_character_surface_material();
  p.material.alpha_cutoff = .4f;
  p.lod_distance = 12;
  p.individualized = true;
  return p;
}
} // namespace fury
