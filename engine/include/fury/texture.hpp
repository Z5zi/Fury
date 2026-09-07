#pragma once

#include "fury/mesh.hpp"

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

/// Sample RGB [0,1] with repeat wrap (nearest). White if empty.
Vec3 sample_image(const Image& img, float u, float v);

}  // namespace fury
