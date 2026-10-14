#pragma once

#include "fury/texture.hpp"

#include <memory>

namespace fury {

/// Original, deterministic, physically scaled PBR detail for the six legacy
/// opaque surface slots. Never replaces an imported material's authored maps.
bool supports_surface_detail(TextureSlot slot);

/// Repeat frequency in inverse metres: 2 m tiles (0.5), except 1 m steel (1).
/// Unsupported slots return zero and retain their existing UV mapping.
float surface_detail_uv_per_meter(TextureSlot slot);

/// Generate immutable, repeat-wrapped albedo / normal / metallic-roughness maps.
/// Accepted resolutions are 128, 256 and 512; others throw invalid_argument.
/// quarter_turns must be 0..3. Maps rotate clockwise, including tangent normals.
/// Unsupported slots return nullptr. Validation occurs before the slot check.
///
/// Albedo is sRGB RGBA8; normal is linear tangent-space XYZ (positive V points
/// along the texture's increasing row); packed linear RGBA8 is R=255 (unused),
/// G=roughness multiplier, B=metallic multiplier, A=255. No emissive map.
/// Corresponding *_mips vectors contain levels 1 through the final 1x1 image.
///
/// Results are shared and cached at one active (resolution, quarter_turns) pair.
/// Six profiles * three RGBA maps plus mips use less than 24 MiB at 512, excluding
/// maps still owned by callers. Switching either key evicts the old cache owners;
/// it does not invalidate returned shared_ptrs. Thread-safe; no disk or network.
std::shared_ptr<const MaterialTextures> surface_detail_textures(
    TextureSlot slot, unsigned resolution = 512, unsigned quarter_turns = 0);

}  // namespace fury
