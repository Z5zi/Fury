#pragma once

#include "fury/texture.hpp"

namespace fury {

struct MaterialTangentFrame { Vec3 tangent,bitangent; };
struct MaterialProjection { Vec2 uv; MaterialTangentFrame frame; };
struct TextureFootprint { Vec2 dx,dy; };

/// World-anchored tiles/metre. Dominant absolute geometric normal, ties Y/Z/X.
/// Y: (x,z), Z: (x,-y), X: (z,-y). Axes do not flip on mirrored/back faces.
MaterialProjection world_planar_projection(Vec3 world,Vec3 geometric,float scale);
/// Gram-Schmidt tangent; preserve the authored/projected bitangent handedness.
MaterialTangentFrame orthonormalize_material_frame(Vec3 normal,MaterialTangentFrame frame);

/// Quotient-rule derivatives of perspective-interpolated UVs. The UV argument
/// is the divided interpolant; numerator derivatives are those of UV / clip.w.
TextureFootprint perspective_texture_footprint(Vec2 uv,Vec2 numerator_dx,
    Vec2 numerator_dy,float inverse_w,float inverse_w_dx,float inverse_w_dy);
/// Project a ray cone's full world-space diameter onto a triangle, including
/// the grazing-angle ellipse, then convert its two axes to UV differentials.
TextureFootprint ray_cone_texture_footprint(Vec3 edge1,Vec3 edge2,Vec2 delta_uv1,
    Vec2 delta_uv2,Vec3 ray_direction,float diameter);
/// Largest singular value of the texel-space derivatives (isotropic filtering).
float material_texture_lod(const RgbaImage& image,TextureFootprint footprint);

/// Repeat bilinear/trilinear filtering. Decode sRGB RGB before both filters;
/// alpha and data maps stay linear. Mip vector index 0 is level 1, not level 0.
/// Missing chains retain exact level-zero bilinear behavior; malformed chains
/// stop at the last valid half-size level. Normal decoding/normalization belongs
/// to shading after filtering, so normal_scale applies in tangent space.
Vec4 sample_material_texture_lod(const RgbaImage& image,
    const std::vector<RgbaImage>& mips,Vec2 uv,bool srgb,float lod);
Vec4 sample_material_texture(const RgbaImage& image,
    const std::vector<RgbaImage>& mips,Vec2 uv,bool srgb,TextureFootprint footprint={});

/// Shared CPU/GL validation contract for bounded color-detail mappings.
bool material_color_detail_region_valid(const MaterialColorDetailRegion& region);
/// Override only color/alpha in the first matching window, preserving all bits
/// of base outside it or when detail is absent/invalid. Source UVs repeat;
/// derivatives are remapped before computing detail LOD. Other PBR channels
/// always use the original material UVs. Publish the object immutably.
Vec4 apply_material_color_detail(const MaterialColorDetail* detail,Vec4 base,
    Vec2 uv,TextureFootprint footprint={},bool srgb=true);

} // namespace fury
