#pragma once
#include "character_sculpt.hpp"
#include "fury/generated/hale_anatomy.hpp"

namespace fury {
// Original tailored security cap and close hair fitted to Hale's source scalp.
// The crown, band and curved brim are authored here; no reference asset is used.
inline void make_hale_cap_and_hair(
    character_detail::Sculpt &b,
    const std::function<float(float, float, float)> &anatomy_radius) {
  using namespace character_detail;
  const Vec3 cloth{.10f, .155f, .21f};
  const Vec3 hair = b.m.appearance.hair;
  const auto scalp_center = [&](float y) {
    return lerp(hale_anatomy::head_center.z, hale_anatomy::crown_joint.z,
                smooth((y - .455f) / .037f));
  };
  const auto hairline = [&](float a) {
    return .444f + .020f * std::max(0.f, std::sin(a)) -
           .034f * std::max(0.f, -std::sin(a)) +
           .004f * std::pow(std::fabs(std::cos(a)), 8.f) +
           .00065f * std::sin(9*a+.4f) * std::sin(5*a-.7f);
  };
  const auto hair_point = [&](float a, float t, float lift) {
    const float y = lerp(hairline(a), .496f,
                         std::sin(std::clamp(t, 0.f, 1.f) * pi * .5f));
    const float source_y = y - .003f * smooth((y - .448f) / .048f);
    const float cz = scalp_center(source_y);
    float r = anatomy_radius(std::min(source_y, .4929f), a, cz) +
              (b.near ? .0014f : .0028f) + lift;
    r = std::max(.00018f, r);
    return Vec3{r * std::cos(a), y, cz + r * std::sin(a)};
  };
  // A continuous opaque layer backs every boundary strand. Hair beneath the
  // cap remains an actual fitted scalp, including the rear and temples.
  const auto hair_start = unsigned(b.m.bind_mesh.vertices.size());
  const int hair_columns = b.near ? 48 : 24, hair_rows = b.near ? 14 : 7;
  b.grid(hair_columns, hair_rows,
         [&](float u, float v) {
    const float a = 2 * pi * u;
    const float growth = smooth(v / .09f);
    // A short clipper taper softens the opaque hairline. It changes pigment
    // within the existing palette, never punches alpha holes into the scalp.
    const Vec3 root = hair*.71f+b.m.appearance.skin*.06f;
    const Vec3 color = root*(1-growth)+hair*.79f*growth;
    return P{hair_point(a, v, 0), color, wt(B::Head),
             character_surface_uv(S::HairCrop, u, v)};
  }, true, false);
  const auto hair_pole = b.vertex({{0,.496f,scalp_center(.493f)},hair*.79f,wt(B::Head),character_surface_uv(S::HairCrop,.5f,1.f)});
  for(int i=0;i<hair_columns;++i) b.tri(hair_start+hair_rows*(hair_columns+1)+i,hair_pole,hair_start+hair_rows*(hair_columns+1)+i+1);
  const int strands = b.near ? 86 : 20;
  for (int k = 0; k < strands; ++k) {
    const float angle = 2 * pi * (float(k) + .27f) / strands;
    if (std::sin(angle) > .48f)
      continue; // the cap hides the forehead growth
    b.grid(2, b.near ? 5 : 3, [&](float u, float v) {
      const float a = angle + (u - .5f) * (b.near ? .080f : .20f);
      const float t = lerp(.20f, -.045f, v);
      Vec3 p = hair_point(a, std::max(0.f, t), .00025f);
      if (t < 0)
        p.y += t * .040f;
      return P{p, hair * (.72f + .12f * v), wt(B::Head),
               character_surface_uv(S::HairShortStrands, 1 - u, v)};
    }, false, false);
  }

  const auto crown_point = [&](float a, float t) {
    const float y = lerp(.453f, .500f, std::sin(t * pi * .5f));
    const float source_y = y - .007f * smooth((y - .453f) / .047f);
    const float cz = scalp_center(source_y);
    float r = anatomy_radius(std::min(source_y, .4929f), a, cz) +
              (b.near ? .0038f : .0051f);
    // Shallow panel tension softens the source skull into a structured cap.
    r += .0012f * std::sin(6 * a) * std::sin(pi * t);
    return Vec3{std::max(.00022f, r) * std::cos(a), y,
                cz + std::max(.00022f, r) * std::sin(a)};
  };
  const int columns = b.near ? 48 : 20, rows = b.near ? 14 : 6;
  const auto crown_start = unsigned(b.m.bind_mesh.vertices.size());
  b.grid(columns, rows, [&](float u, float v) {
    return P{crown_point(2 * pi * u, v), cloth, wt(B::Head),
             character_surface_uv(S::Shirt, u, v)};
  }, true, false);
  const auto crown_pole = b.vertex({{0,.500f,scalp_center(.493f)},cloth,wt(B::Head),character_surface_uv(S::Shirt,.5f,1.f)});
  for(int i=0;i<columns;++i) b.tri(crown_start+rows*(columns+1)+i,crown_pole,crown_start+rows*(columns+1)+i+1);
  // An actual woven band surrounds the cap opening. The lower return hides
  // its thickness; the rear closure is a restrained original cloth tab.
  b.grid(columns, b.near ? 2 : 1, [&](float u, float v) {
    const float a = 2 * pi * u;
    Vec3 p = crown_point(a, 0);
    p.y += lerp(-.0030f, .0050f, v);
    const Vec3 radial{std::cos(a), 0, std::sin(a)};
    p += radial * .0010f;
    return P{p, cloth * .70f, wt(B::Head),
             character_surface_uv(S::Shirt, u, v)};
  }, true, false);

  const auto brim = [&](float u, float v) {
    const float a = pi * .5f + lerp(-1.18f, 1.18f, u);
    Vec3 p = crown_point(a, 0);
    const float forward = std::max(0.f, std::sin(a));
    const float extension = .037f * std::pow(forward, .72f) * v;
    p += Vec3{0, 0, 1} * extension;
    p.y -= .003f * v + .0045f * sq(2 * u - 1) * v;
    return p;
  };
  const auto brim_normal = [&](float u, float v) {
    const Vec3 du = brim(std::min(1.f, u + .001f), v) -
                    brim(std::max(0.f, u - .001f), v);
    const Vec3 dv = brim(u, std::min(1.f, v + .001f)) -
                    brim(u, std::max(0.f, v - .001f));
    return normalize(cross(normalize(du), normalize(dv)));
  };
  const int nx = b.near ? 24 : 10, ny = b.near ? 8 : 2;
  for (bool lower : {false, true}) {
    b.grid(nx, ny, [&](float u, float v) {
      const float across = lower ? u : 1 - u;
      Vec3 p = brim(across, v);
      if (lower)
        p -= brim_normal(across, v) * .0015f;
      return P{p, cloth * (lower ? .54f : .81f), wt(B::Head),
               character_surface_uv(S::Leather, u, v)};
    }, false, false);
  }
  // Close the three exposed cut edges of the layered brim.
  for (int edge = 0; edge < 3; ++edge) {
    b.grid(b.near ? 24 : 12, 1, [&](float u, float v) {
      const float across = edge == 0 ? u : float(edge - 1);
      const float depth = edge == 0 ? 1.f : u;
      Vec3 p = brim(across, depth) - brim_normal(across, depth) * (.0015f * v);
      return P{p, cloth * .56f, wt(B::Head),
               character_surface_uv(S::Leather, u, v)};
    }, false, false);
  }
  // The old cap's small neutral metal marker remains unlettered. It encodes
  // no new rank, department insignia, logo or story information.
  const Vec3 front = crown_point(pi * .5f, .23f);
  b.ellipsoid(front + Vec3{0, 0, .0012f}, {.0038f, .0043f, .0009f},
              {.63f, .70f, .66f}, S::Metal, B::Head, b.near ? 12 : 8, 4);
  if (b.near) {
    for (int panel = 0; panel < 6; ++panel) {
      std::vector<Vec3> seam;
      for (int j = 0; j <= 16; ++j) {
        const float a = 2 * pi * float(panel) / 6;
        const float t = lerp(.035f, .83f, float(j) / 16);
        Vec3 p = crown_point(a, t);
        p += Vec3{std::cos(a), .2f, std::sin(a)} * .00025f;
        seam.push_back(p);
      }
      b.curve(seam, .00030f, cloth * .77f, S::Shirt, B::Head, 4);
    }
  }
}
} // namespace fury
