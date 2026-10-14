#pragma once
#include "character_sculpt.hpp"
#include "fury/generated/vale_anatomy.hpp"

namespace fury {
// Vale's own six-panel crown, low raked band and closed curved brim. All form
// is authored from his measured scalp and original uniform palette. References
// inform construction only; no commercial cap pattern or texture is used.
inline void
make_vale_cap(character_detail::Sculpt &b,
              const std::function<float(float, float, float)> &radius) {
  using namespace character_detail;
  const Vec3 cloth{.10f, .155f, .21f}, hair = b.m.appearance.hair;
  const auto axis = [&](float y) {
    return lerp(vale_anatomy::head_center.z, vale_anatomy::skin_crown_center.z,
                smooth((y - .448f) / .041f));
  };
  const auto hairline = [](float a) {
    const float front = std::max(0.f, std::sin(a)),
                back = std::max(0.f, -std::sin(a));
    const float wrap = std::atan2(std::sin(a), std::cos(a));
    const float temple =
        .008f * (g(wrap, .40f, .17f) + g(wrap, pi - .40f, .17f));
    return .444f + .017f * front - .036f * back +
           .006f * std::pow(std::fabs(std::cos(a)), 8.f) - temple +
           .0007f * std::sin(9 * a + .4f) * back;
  };
  const auto growth = [&](float a, float t, float lift) {
    const float y = lerp(hairline(a) - .001f, .489f,
                         std::sin(std::clamp(t, 0.f, .985f) * pi * .5f));
    const float cz = axis(y), r = radius(std::min(y, .48896f), a, cz) +
                                  (b.near ? .0015f : .0028f) + lift;
    return Vec3{r * std::cos(a), y + .004f * smooth(t), cz + r * std::sin(a)};
  };
  const int hc = b.near ? 40 : 20, hr = b.near ? 12 : 7;
  const unsigned hb = b.m.bind_mesh.vertices.size();
  b.grid(
      hc, hr,
      [&](float u, float v) {
        const float a = 2 * pi * u, t = .985f * v, fade = smooth(v / .15f);
        const Vec3 root = hair * .22f + b.m.appearance.skin * .78f;
        return P{growth(a, t, 0), root * (1 - fade) + hair * .86f * fade,
                 wt(B::Head), character_surface_uv(S::HairCrop, u, v)};
      },
      true, false);
  const unsigned hp = b.vertex({{0, .493f, axis(.489f)},
                                hair * .86f,
                                wt(B::Head),
                                character_surface_uv(S::HairCrop, .5f, 1)});
  for (int k = 0; k < hc; ++k)
    b.tri(hb + hr * (hc + 1) + k, hp, hb + hr * (hc + 1) + k + 1);
  // Vale's short growth is the continuous matte crop above. Thin masked
  // cards made a visibly striped nape at portrait scale, so no boundary cards
  // are added. The fitted opaque backing retains complete scalp coverage.
  const auto crown = [&](float a, float t) {
    const float base = .450f + .0040f * std::sin(a);
    const float source_y = lerp(base - .0022f, .489f, std::sin(t * pi * .5f));
    const float cz = axis(source_y),
                lift = (b.near ? .0033f : .0048f) * std::cos(t * pi * .5f);
    float r = radius(std::min(source_y, .48896f), a, cz) + lift;
    r += .0008f * std::sin(6 * a + .2f) * std::sin(pi * t);
    return Vec3{r * std::cos(a), lerp(base, .500f, std::sin(t * pi * .5f)),
                cz + r * std::sin(a)};
  };
  const int c = b.near ? 40 : 20, rows = b.near ? 12 : 6;
  const unsigned start = b.m.bind_mesh.vertices.size();
  b.grid(
      c, rows,
      [&](float u, float v) {
        return P{crown(2 * pi * u, v * .985f), cloth, wt(B::Head),
                 character_surface_uv(S::Shirt, u, v)};
      },
      true, false);
  const unsigned pole = b.vertex({{0, .5f, axis(.489f)},
                                  cloth,
                                  wt(B::Head),
                                  character_surface_uv(S::Shirt, .5f, 1)});
  for (int k = 0; k < c; ++k)
    b.tri(start + rows * (c + 1) + k, pole, start + rows * (c + 1) + k + 1);
  // The band folds inwards at its lower edge; the crown extends inside it.
  b.grid(
      c, 4,
      [&](float u, float v) {
        const float a = 2 * pi * u;
        const int row = int(std::lround(v * 4));
        const float dy[]{-.002f, .0038f, .0038f, -.002f, -.002f};
        const float dr[]{.001f, .001f, -.0005f, -.0005f, .001f};
        Vec3 p = crown(a, 0) +
                 Vec3{std::cos(a) * dr[row], dy[row], std::sin(a) * dr[row]};
        return P{p, cloth * .72f, wt(B::Head),
                 character_surface_uv(S::Shirt, u, v)};
      },
      true, false);
  const auto brim = [&](float u, float v) {
    const float a = pi * .5f + lerp(-1.13f, 1.13f, u);
    Vec3 p = crown(a, 0);
    p.z += .037f * std::pow(std::max(0.f, std::sin(a)), .68f) * v;
    p.y -= .0028f * v + .004f * sq(2 * u - 1) * v;
    return p;
  };
  const int nx = b.near ? 24 : 10, ny = b.near ? 4 : 3;
  // Store the physical grid once. Every return uses its exact boundary
  // samples, so the closed brim has neither a buried open root nor T-junctions
  // from oversampling a side beyond the top surface's radial divisions.
  std::array<std::vector<Vec3>, 2> points;
  for (int bottom = 0; bottom < 2; ++bottom)
    for (int j = 0; j <= ny; ++j)
      for (int i = 0; i <= nx; ++i) {
        Vec3 p = brim(float(i) / nx, float(j) / ny);
        p.y -= bottom * .0012f;
        points[bottom].push_back(p);
      }
  for (int bottom = 0; bottom < 2; ++bottom)
    b.grid(
        nx, ny,
        [&](float u, float v) {
          const int i = int(std::lround(u * nx)),
                    j = int(std::lround(v * ny));
          return P{points[bottom][j * (nx + 1) + (bottom ? i : nx - i)],
                   cloth * (bottom ? .55f : .83f), wt(B::Head),
                   character_surface_uv(S::Leather, u, v)};
        },
        false, false);
  for (int edge = 0; edge < 4; ++edge)
    b.grid(
        edge < 2 ? nx : ny, 1,
        [&](float u, float v) {
          int i = 0, j = 0;
          if (edge == 0) { i = nx - int(std::lround(u * nx)); j = ny; }
          if (edge == 1) { i = int(std::lround(u * nx)); j = 0; }
          if (edge == 2) { i = 0; j = ny - int(std::lround(u * ny)); }
          if (edge == 3) { i = nx; j = int(std::lround(u * ny)); }
          return P{points[int(std::lround(v))][j * (nx + 1) + i],
                   cloth * .61f, wt(B::Head),
                   character_surface_uv(S::Leather, u, v)};
        },
        false, false);
  if (b.near)
    for (int k = 0; k < 6; ++k) {
      std::vector<Vec3> path;
      const float a = 2 * pi * k / 6.f;
      for (int j = 0; j <= 12; ++j) {
        Vec3 p = crown(a, lerp(.08f, .91f, j / 12.f));
        p += Vec3{std::cos(a), .2f, std::sin(a)} * .00020f;
        path.push_back(p);
      }
      b.curve(path, .00022f, cloth * .76f, S::Shirt, B::Head, 4);
    }
  const Vec3 mark = crown(pi * .5f, .21f);
  b.ellipsoid(mark + Vec3{0, 0, .0010f}, {.0033f, .0042f, .00065f},
              {.63f, .70f, .66f}, S::Metal, B::Head, b.near ? 12 : 6, b.near?4:3);
  // A small rear fabric adjustment overlap is part of the band, without marks.
  b.grid(
      b.near ? 12 : 6, 2,
      [&](float u, float v) {
        const float a = -pi * .5f + lerp(-.24f, .24f, 1 - u);
        Vec3 p = crown(a, 0);
        p += Vec3{std::cos(a) * .0016f, lerp(-.001f, .0033f, v),
                  std::sin(a) * .0016f};
        return P{p, cloth * .61f, wt(B::Head),
                 character_surface_uv(S::Shirt, u, v)};
      },
      false, false);
}
} // namespace fury
