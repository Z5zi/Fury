#include "fury/texture.hpp"

#include "fury/log.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

namespace fury {
namespace {

bool ends_with_ci(const std::string& s, const char* ext) {
  const std::size_t n = std::char_traits<char>::length(ext);
  if (s.size() < n) {
    return false;
  }
  for (std::size_t i = 0; i < n; ++i) {
    const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[s.size() - n + i])));
    const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
    if (a != b) {
      return false;
    }
  }
  return true;
}

bool read_file_bytes(const std::string& path, std::vector<std::uint8_t>& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const auto len = in.tellg();
  if (len <= 0) {
    return false;
  }
  in.seekg(0, std::ios::beg);
  out.resize(static_cast<std::size_t>(len));
  in.read(reinterpret_cast<char*>(out.data()), len);
  return static_cast<bool>(in) || in.eof();
}

// Skip PPM whitespace / comments (#...).
bool ppm_skip(std::istream& in) {
  while (in) {
    const int c = in.peek();
    if (c == EOF) {
      return false;
    }
    if (c == '#') {
      in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c))) {
      in.get();
      continue;
    }
    break;
  }
  return static_cast<bool>(in);
}

}  // namespace

bool load_ppm(const std::string& path, Image& out) {
  out = Image{};
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::string magic;
  if (!(in >> magic)) {
    return false;
  }
  if (magic != "P6" && magic != "P3") {
    return false;
  }
  ppm_skip(in);
  int w = 0, h = 0, maxv = 0;
  if (!(in >> w >> h)) {
    return false;
  }
  ppm_skip(in);
  if (!(in >> maxv) || w <= 0 || h <= 0 || maxv <= 0 || maxv > 255) {
    return false;
  }
  // Consume single whitespace after maxval before binary payload.
  const int ws = in.get();
  if (ws == EOF) {
    return false;
  }
  const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u;
  out.width = w;
  out.height = h;
  out.rgb.resize(n);
  if (magic == "P6") {
    in.read(reinterpret_cast<char*>(out.rgb.data()),
            static_cast<std::streamsize>(n));
    if (!in) {
      out = Image{};
      return false;
    }
  } else {
    for (std::size_t i = 0; i < n; ++i) {
      int v = 0;
      if (!(in >> v)) {
        out = Image{};
        return false;
      }
      out.rgb[i] = static_cast<std::uint8_t>(
          (std::max)(0, (std::min)(255, v * 255 / maxv)));
    }
  }
  return true;
}

bool load_stb_image(const std::string& path, Image& out) {
  out = Image{};
  std::vector<std::uint8_t> file;
  if (!read_file_bytes(path, file)) {
    return false;
  }
  int w = 0, h = 0, comp = 0;
  stbi_uc* data = stbi_load_from_memory(file.data(), static_cast<int>(file.size()),
                                        &w, &h, &comp, 3);
  if (!data || w <= 0 || h <= 0) {
    if (data) {
      stbi_image_free(data);
    }
    return false;
  }
  out.width = w;
  out.height = h;
  const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u;
  out.rgb.assign(data, data + n);
  stbi_image_free(data);
  return true;
}

bool load_image(const std::string& path, Image& out) {
  if (ends_with_ci(path, ".ppm") || ends_with_ci(path, ".pnm")) {
    return load_ppm(path, out);
  }
  if (ends_with_ci(path, ".png") || ends_with_ci(path, ".jpg") ||
      ends_with_ci(path, ".jpeg") || ends_with_ci(path, ".bmp") ||
      ends_with_ci(path, ".tga")) {
    return load_stb_image(path, out);
  }
  // Unknown extension: try PPM then STB.
  if (load_ppm(path, out)) {
    return true;
  }
  return load_stb_image(path, out);
}

bool load_texture_asset(const char* filename, Image& out) {
  if (!filename || !filename[0]) {
    out = Image{};
    return false;
  }
  static const char* kPrefixes[] = {
      "assets/textures/",
      "../assets/textures/",
      "../../assets/textures/",
      "../../../assets/textures/",
      "./",
  };
  for (const char* prefix : kPrefixes) {
    const std::string path = std::string(prefix) + filename;
    if (load_image(path, out)) {
      return true;
    }
  }
  out = Image{};
  return false;
}

const char* texture_slot_asset_name(TextureSlot slot) {
  switch (slot) {
    case TextureSlot::Asphalt:
      return "asphalt.png";
    case TextureSlot::Wood:
      return "crate_wood.png";
    case TextureSlot::BarrelMetal:
      return "barrel_metal.png";
    default:
      return nullptr;
  }
}

void fill_procedural_texture(TextureSlot slot, int size, Image& out) {
  size = (std::max)(1, size);
  out.width = size;
  out.height = size;
  out.rgb.resize(static_cast<std::size_t>(size * size * 3));
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const std::size_t i = static_cast<std::size_t>((y * size + x) * 3);
      std::uint8_t r = 128, g = 128, b = 128;
      switch (slot) {
        case TextureSlot::Checker: {
          const bool c = ((x / 8) ^ (y / 8)) & 1;
          r = g = b = c ? 210 : 55;
          break;
        }
        case TextureSlot::Asphalt: {
          const int n = ((x * 13 + y * 7) ^ (x * y)) & 31;
          r = static_cast<std::uint8_t>(40 + n);
          g = static_cast<std::uint8_t>(40 + n);
          b = static_cast<std::uint8_t>(44 + n);
          if ((x + y) % 17 == 0) {
            r = g = b = 70;
          }
          if ((y % 21) == 0) {
            r = static_cast<std::uint8_t>((std::min)(255, static_cast<int>(r) + 18));
            g = static_cast<std::uint8_t>((std::min)(255, static_cast<int>(g) + 16));
            b = static_cast<std::uint8_t>((std::min)(255, static_cast<int>(b) + 10));
          }
          break;
        }
        case TextureSlot::Concrete: {
          const int n = ((x * 3 + y * 5) ^ (x << 2)) & 47;
          r = static_cast<std::uint8_t>(150 + n);
          g = static_cast<std::uint8_t>(148 + n);
          b = static_cast<std::uint8_t>(142 + n / 2);
          break;
        }
        case TextureSlot::Water: {
          const float fx = static_cast<float>(x) / static_cast<float>(size);
          const float fy = static_cast<float>(y) / static_cast<float>(size);
          const float w =
              0.5f + 0.5f * std::sin(fx * 18.f + fy * 6.f) * std::cos(fy * 14.f);
          const float w2 =
              0.5f + 0.5f * std::sin(fx * 9.f - fy * 11.f + 1.3f);
          r = static_cast<std::uint8_t>(12 + w * 22.f + w2 * 8.f);
          g = static_cast<std::uint8_t>(55 + w * 70.f + w2 * 18.f);
          b = static_cast<std::uint8_t>(110 + w * 95.f + w2 * 30.f);
          break;
        }
        case TextureSlot::Brick: {
          const int bw = 10;
          const int bh = 5;
          const int row = y / bh;
          const int ox = (row & 1) ? (bw / 2) : 0;
          const int lx = (x + ox) % bw;
          const int ly = y % bh;
          const bool mortar = (lx == 0) || (ly == 0);
          const int n = ((x * 9 + y * 3) ^ (row * 17)) & 31;
          if (mortar) {
            r = static_cast<std::uint8_t>(150 + (n & 15));
            g = static_cast<std::uint8_t>(140 + (n & 15));
            b = static_cast<std::uint8_t>(128 + (n & 7));
          } else {
            r = static_cast<std::uint8_t>(140 + n);
            g = static_cast<std::uint8_t>(70 + n / 2);
            b = static_cast<std::uint8_t>(55 + n / 3);
          }
          break;
        }
        case TextureSlot::Metal:
        case TextureSlot::BarrelMetal: {
          const float fx = static_cast<float>(x) / static_cast<float>(size);
          const float fy = static_cast<float>(y) / static_cast<float>(size);
          const int n = ((x * 17 + y * 11) ^ (x << 1)) & 63;
          const float streak =
              0.55f + 0.45f * std::sin(fy * 40.f + fx * 3.f);
          const int v = static_cast<int>(95 + n * 0.7f + streak * 55.f);
          r = static_cast<std::uint8_t>((std::min)(255, v));
          g = static_cast<std::uint8_t>((std::min)(255, v + 4));
          b = static_cast<std::uint8_t>((std::min)(255, v + 10));
          break;
        }
        case TextureSlot::Glass: {
          const float fx = static_cast<float>(x) / static_cast<float>(size);
          const float fy = static_cast<float>(y) / static_cast<float>(size);
          const float edge =
              (std::min)((std::min)(fx, fy), (std::min)(1.f - fx, 1.f - fy));
          const float tint = 0.55f + 0.45f * edge;
          const int n = ((x * 5) ^ (y * 9)) & 15;
          r = static_cast<std::uint8_t>(90 + tint * 40.f + n);
          g = static_cast<std::uint8_t>(130 + tint * 50.f + n);
          b = static_cast<std::uint8_t>(160 + tint * 70.f + n);
          break;
        }
        case TextureSlot::Wood: {
          const int n = ((x * 17 + y * 3) ^ (x * y)) & 31;
          const int grain = static_cast<int>(
              18.f * std::fabs(std::fmod(x * 0.35f + (y % 7) * 0.1f, 4.f) - 2.f));
          r = static_cast<std::uint8_t>((std::max)(0, 140 + n - grain));
          g = static_cast<std::uint8_t>((std::max)(0, 95 + n / 2 - grain / 2));
          b = static_cast<std::uint8_t>((std::max)(0, 55 + n / 3 - grain / 3));
          if (x % 16 == 0 || y % 32 == 0) {
            r = static_cast<std::uint8_t>((std::max)(0, static_cast<int>(r) - 40));
            g = static_cast<std::uint8_t>((std::max)(0, static_cast<int>(g) - 30));
            b = static_cast<std::uint8_t>((std::max)(0, static_cast<int>(b) - 20));
          }
          break;
        }
        default:
          r = g = b = 255;
          break;
      }
      out.rgb[i] = r;
      out.rgb[i + 1] = g;
      out.rgb[i + 2] = b;
    }
  }
}

bool resolve_texture_pixels(TextureSlot slot, int procedural_size, Image& out) {
  const char* name = texture_slot_asset_name(slot);
  if (name) {
    // Prefer PNG (STB); fall back to sibling .ppm if present.
    if (load_texture_asset(name, out)) {
      static bool logged[static_cast<int>(TextureSlot::Count)]{};
      const int idx = static_cast<int>(slot);
      if (idx >= 0 && idx < static_cast<int>(TextureSlot::Count) && !logged[idx]) {
        logged[idx] = true;
        Log::info(std::string("Texture loaded: assets/textures/") + name);
      }
      return true;
    }
    std::string ppm = name;
    const auto dot = ppm.find_last_of('.');
    if (dot != std::string::npos) {
      ppm = ppm.substr(0, dot) + ".ppm";
      if (load_texture_asset(ppm.c_str(), out)) {
        static bool logged_ppm[static_cast<int>(TextureSlot::Count)]{};
        const int idx = static_cast<int>(slot);
        if (idx >= 0 && idx < static_cast<int>(TextureSlot::Count) &&
            !logged_ppm[idx]) {
          logged_ppm[idx] = true;
          Log::info(std::string("Texture loaded: assets/textures/") + ppm);
        }
        return true;
      }
    }
  }
  fill_procedural_texture(slot, procedural_size, out);
  return out.width > 0 && out.height > 0 && !out.rgb.empty();
}

const char* texture_slot_normal_asset_name(TextureSlot slot) {
  switch (slot) {
    case TextureSlot::Asphalt:
      return "asphalt_n.png";
    case TextureSlot::Brick:
      return "brick_n.png";
    default:
      return nullptr;
  }
}

bool texture_slot_has_normal(TextureSlot slot) {
  return slot == TextureSlot::Asphalt || slot == TextureSlot::Brick;
}

void fill_procedural_normal(TextureSlot slot, int size, Image& out) {
  size = (std::max)(1, size);
  // Height field then central-difference normals (tangent-space RGB).
  std::vector<float> height(static_cast<std::size_t>(size * size), 0.5f);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      float h = 0.5f;
      switch (slot) {
        case TextureSlot::Asphalt: {
          const int n = ((x * 13 + y * 7) ^ (x * y)) & 31;
          h = 0.45f + static_cast<float>(n) / 80.f;
          if ((x + y) % 17 == 0) {
            h += 0.08f;
          }
          if ((y % 21) == 0) {
            h -= 0.12f;
          }
          h += 0.04f * std::sin(x * 1.7f + y * 0.9f) * std::cos(y * 2.1f);
          break;
        }
        case TextureSlot::Brick: {
          const int bw = 10;
          const int bh = 5;
          const int row = y / bh;
          const int ox = (row & 1) ? (bw / 2) : 0;
          const int lx = (x + ox) % bw;
          const int ly = y % bh;
          const bool mortar = (lx == 0) || (ly == 0);
          const int n = ((x * 9 + y * 3) ^ (row * 17)) & 31;
          if (mortar) {
            h = 0.15f + static_cast<float>(n & 7) / 80.f;
          } else {
            h = 0.65f + static_cast<float>(n) / 90.f +
                0.03f * std::sin(x * 0.8f + y * 0.4f);
          }
          break;
        }
        default:
          h = 0.5f;
          break;
      }
      height[static_cast<std::size_t>(y * size + x)] = h;
    }
  }

  const float strength = (slot == TextureSlot::Brick) ? 8.f : 6.5f;
  out.width = size;
  out.height = size;
  out.rgb.resize(static_cast<std::size_t>(size * size * 3));
  auto at = [&](int x, int y) -> float {
    x = (x % size + size) % size;
    y = (y % size + size) % size;
    return height[static_cast<std::size_t>(y * size + x)];
  };
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float dx = (at(x + 1, y) - at(x - 1, y)) * strength;
      const float dy = (at(x, y + 1) - at(x, y - 1)) * strength;
      float nx = -dx;
      float ny = dy;
      float nz = 1.f;
      const float inv = 1.f / std::sqrt(nx * nx + ny * ny + nz * nz);
      nx *= inv;
      ny *= inv;
      nz *= inv;
      const std::size_t i = static_cast<std::size_t>((y * size + x) * 3);
      out.rgb[i] = static_cast<std::uint8_t>((std::max)(
          0, (std::min)(255, static_cast<int>((nx * 0.5f + 0.5f) * 255.f))));
      out.rgb[i + 1] = static_cast<std::uint8_t>((std::max)(
          0, (std::min)(255, static_cast<int>((ny * 0.5f + 0.5f) * 255.f))));
      out.rgb[i + 2] = static_cast<std::uint8_t>((std::max)(
          0, (std::min)(255, static_cast<int>((nz * 0.5f + 0.5f) * 255.f))));
    }
  }
}

bool resolve_normal_pixels(TextureSlot slot, int procedural_size, Image& out) {
  if (!texture_slot_has_normal(slot)) {
    out = Image{};
    return false;
  }
  const char* name = texture_slot_normal_asset_name(slot);
  if (name) {
    if (load_texture_asset(name, out)) {
      static bool logged[static_cast<int>(TextureSlot::Count)]{};
      const int idx = static_cast<int>(slot);
      if (idx >= 0 && idx < static_cast<int>(TextureSlot::Count) && !logged[idx]) {
        logged[idx] = true;
        Log::info(std::string("Normal map loaded: assets/textures/") + name);
      }
      return true;
    }
    std::string ppm = name;
    const auto dot = ppm.find_last_of('.');
    if (dot != std::string::npos) {
      ppm = ppm.substr(0, dot) + ".ppm";
      if (load_texture_asset(ppm.c_str(), out)) {
        static bool logged_ppm[static_cast<int>(TextureSlot::Count)]{};
        const int idx = static_cast<int>(slot);
        if (idx >= 0 && idx < static_cast<int>(TextureSlot::Count) &&
            !logged_ppm[idx]) {
          logged_ppm[idx] = true;
          Log::info(std::string("Normal map loaded: assets/textures/") + ppm);
        }
        return true;
      }
    }
  }
  fill_procedural_normal(slot, procedural_size, out);
  return out.width > 0 && out.height > 0 && !out.rgb.empty();
}

Vec3 sample_image(const Image& img, float u, float v) {
  if (img.width <= 0 || img.height <= 0 ||
      img.rgb.size() < static_cast<std::size_t>(img.width * img.height * 3)) {
    return Vec3{1.f, 1.f, 1.f};
  }
  // Repeat wrap
  u = u - std::floor(u);
  v = v - std::floor(v);
  if (u < 0.f) {
    u += 1.f;
  }
  if (v < 0.f) {
    v += 1.f;
  }
  int x = static_cast<int>(u * static_cast<float>(img.width));
  int y = static_cast<int>(v * static_cast<float>(img.height));
  if (x >= img.width) {
    x = img.width - 1;
  }
  if (y >= img.height) {
    y = img.height - 1;
  }
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  const std::size_t i =
      static_cast<std::size_t>((y * img.width + x) * 3);
  return Vec3{img.rgb[i] / 255.f, img.rgb[i + 1] / 255.f, img.rgb[i + 2] / 255.f};
}

}  // namespace fury
