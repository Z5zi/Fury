#pragma once

#include "fury/mesh.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fury {

/// CPU RGB8 image (tightly packed, top-left origin). Used by file albedo loads.
struct Image {
  int width{0};
  int height{0};
  std::vector<std::uint8_t> rgb;
};

struct RgbaImage {
  int width{0},height{0};
  std::vector<std::uint8_t> pixels;
  bool valid() const { return width>0 && height>0 && pixels.size()==std::size_t(width)*height*4; }
};
/// A half-open window in repeating material UVs, mapped affinely into a
/// bounded subrectangle of the detail image. Bounds must be finite, ascending,
/// and in [0,1]. First matching valid window wins; invalid windows are ignored.
struct MaterialColorDetailRegion {
  Vec2 source_min{},source_max{};
  Vec2 detail_min{},detail_max{1.f,1.f};
  /// Replace RGB while retaining the original atlas alpha (e.g. opaque scalp).
  bool preserve_base_alpha{false};
};
struct MaterialColorDetail {
  static constexpr unsigned kMaxRegions=4;
  RgbaImage color;
  /// Additional levels only: index 0 is half-size level 1, not level zero.
  std::vector<RgbaImage> color_mips;
  std::array<MaterialColorDetailRegion,kMaxRegions> regions{};
  unsigned region_count{0};
  /// Decoded pixel payload, including level zero and every retained mip.
  /// Excludes small object/vector metadata and any renderer-owned GPU copy.
  std::size_t pixel_bytes() const {
    std::size_t result=color.pixels.size();
    for(const auto& mip:color_mips) result+=mip.pixels.size();
    return result;
  }
};

struct MaterialTextures {
  RgbaImage base_color,normal,metallic_roughness,emissive;
  /// Optional additional mip levels (index 0 = half-size level 1). Color mips
  /// are filtered in linear light; normal mips are renormalized.
  std::vector<RgbaImage> base_color_mips,normal_mips,metallic_roughness_mips,emissive_mips;
  std::string source;
};
enum class TextureEncoding { Linear, SRGB, Normal };
bool decode_rgba_image(const std::uint8_t* bytes,std::size_t size,RgbaImage& out);
bool load_rgba_image(const std::string& path,RgbaImage& out);
/// Build a complete mip chain. sRGB is averaged in linear light; normals are renormalized.
std::vector<RgbaImage> build_mip_chain(RgbaImage image,TextureEncoding encoding);

/// Load binary/ascii PPM (P6 / P3). Returns false on I/O or parse failure.
bool load_ppm(const std::string& path, Image& out);

/// Load PNG / JPEG / etc via vendored stb_image (RGB forced).
bool load_stb_image(const std::string& path, Image& out);

/// Try PPM then STB by extension / content. Cleared on failure.
bool load_image(const std::string& path, Image& out);

/// Resolve `assets/textures/<filename>` from common cwd layouts (repo / build).
bool load_texture_asset(const char* filename, Image& out);

/// Optional on-disk name for a TextureSlot (nullptr = procedural-only).
const char* texture_slot_asset_name(TextureSlot slot);

/// Fill procedural RGB for a slot (64x64 typical). Used when no file asset.
void fill_procedural_texture(TextureSlot slot, int size, Image& out);

/// Prefer file albedo under assets/textures/, else procedural fill.
/// Logs once per slot when a file loads successfully.
bool resolve_texture_pixels(TextureSlot slot, int procedural_size, Image& out);

/// Optional on-disk normal map name for a slot (nullptr = no file normal).
const char* texture_slot_normal_asset_name(TextureSlot slot);

/// True when the slot ships / supports a normal map (asphalt / brick in 5.3.0).
bool texture_slot_has_normal(TextureSlot slot);

/// Fill procedural tangent-space normal RGB (flat = 128,128,255) for a slot.
void fill_procedural_normal(TextureSlot slot, int size, Image& out);

/// Prefer file normal under assets/textures/, else procedural fill (when slot has normals).
bool resolve_normal_pixels(TextureSlot slot, int procedural_size, Image& out);

/// Sample RGB [0,1] with repeat wrap (nearest). White if empty.
Vec3 sample_image(const Image& img, float u, float v);

/// Sample RGBA Image RGB [0,1] with clamp wrap (nearest). White if empty.
Vec3 sample_rgba_image(const RgbaImage& img, float u, float v);

}  // namespace fury
