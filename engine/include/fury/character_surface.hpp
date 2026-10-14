#pragma once

#include "fury/texture.hpp"

#include <cstdint>
#include <memory>

namespace fury {

/// Stable regions of the original, vertex-tinted character material atlas.
enum class CharacterSurface : std::uint8_t {
  Face, Jacket, Hair, Skin, Shirt, Pants, Leather, Eyes, Rubber, Metal,
  HairStrands, HairCrop, HairShortStrands, HairSwept, HairBundle, HairSourceFold, Count
};

constexpr int character_surface_atlas_size = 1536;
constexpr int character_surface_gutter = 32;
/// The 24x24 final atlas level preserves isolation between the smallest regions.
/// Do not append smaller mips: they cannot represent these separate materials.
constexpr int character_surface_max_mip = 6;

struct CharacterSurfaceRegion {
  int x, y, size;
  int gutter{character_surface_gutter};
};

/// Pixel rectangles, including gutters. Throws std::out_of_range for Count or
/// an invalid enum. Image row/UV V zero is the atlas top; local V is not flipped.
const CharacterSurfaceRegion& character_surface_region(CharacterSurface surface);
/// Clamp finite local UVs to [0,1], then map to the interior texel centers.
/// Throws std::invalid_argument for nonfinite UVs. Wrapped geometry must have
/// separate U=0 and U=1 vertices, never a triangle crossing atlas rectangles.
/// Face accepts the anatomical head's unchanged original per-corner UVs and
/// repacks each supported chart with a positive uniform affine transform.
/// The main head receives 1.44x more linear space inside its 1024px tile.
/// There is no V flip, rotation, per-actor texture, or cylindrical projection.
/// Face coordinates outside the supported source charts retain the clamped
/// identity mapping, including the 0/1 endpoints used to inspect tile bounds.
/// HairStrands is a clamped hair-card mask: U crosses the strands, V runs from
/// their roots near zero to their tapered tips near one. Use alpha_cutoff=.4.
/// HairCrop is an opaque short-fiber cap: U wraps, V follows the crop's growth.
/// HairShortStrands uses the same clamped card mask at short-card scale with
/// shallow relief and a matte response. Use alpha_cutoff=.4 on its near cards.
Vec2 character_surface_uv(CharacterSurface surface, float u, float v);

/// One immutable process-wide set: sRGB base-color multipliers, tangent normals,
/// and linear ORM (R=1, G=roughness, B=metallic), all with six additional mips.
/// Base color preserves the mesh's linear vertex tint; it is not an untinted
/// photographic skin/clothing palette. No baked lighting or emission.
/// HairStrands/HairShortStrands and opted-in source-hair cells use cutout alpha; other
/// maps/tiles are opaque. Hair/HairCrop supply the underlying volume; HairCrop is the
/// rougher isotropic approximation for a dense short crop, not anisotropic hair.
std::shared_ptr<const MaterialTextures> character_surface_textures();
/// Complete atlas material: white albedo, roughness/metallic factors both one,
/// normal scale one, UV0 mapping, opaque. The atlas makes only hardware metallic.
Material make_character_surface_material();

/// Explicit Player-only source groom. Exact 0/1; defaults off.
bool player_source_hair_enabled();
/// Short tied-tail option. Exact 0/1; defaults off and requires source hair.
bool player_short_tail_enabled();
/// Optional Player-only 0/512/1024 color/alpha detail. Defaults off.
std::shared_ptr<const MaterialColorDetail> make_player_hair_color_detail();

}  // namespace fury
