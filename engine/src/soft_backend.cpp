#include "fury/renderer.hpp"

#include "fury/log.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace fury {
namespace {

struct SoftVert {
  float x, y, z, rhw;
  float r, g, b;
};

inline float cl01(float v) { return std::clamp(v, 0.f, 1.f); }

inline Vec3 tonemap_gamma(Vec3 c) {
  // Reinhard + gamma 2.2
  c.x = c.x / (1.f + c.x);
  c.y = c.y / (1.f + c.y);
  c.z = c.z / (1.f + c.z);
  constexpr float inv_g = 1.f / 2.2f;
  c.x = std::pow(cl01(c.x), inv_g);
  c.y = std::pow(cl01(c.y), inv_g);
  c.z = std::pow(cl01(c.z), inv_g);
  return c;
}

class SoftBackend final : public IRenderBackend {
 public:
  ~SoftBackend() override { destroy(); }

  bool create(SDL_Window* window, int width, int height) override {
    destroy();
    m_window = window;
    m_width = (std::max)(1, width);
    m_height = (std::max)(1, height);

    m_sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_sdl_renderer) {
      m_sdl_renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (!m_sdl_renderer) {
      Log::error(std::string("SoftBackend SDL_CreateRenderer failed: ") +
                 SDL_GetError());
      return false;
    }

    m_texture = SDL_CreateTexture(m_sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, m_width,
                                  m_height);
    if (!m_texture) {
      Log::error(std::string("SoftBackend SDL_CreateTexture failed: ") +
                 SDL_GetError());
      destroy();
      return false;
    }

    m_color.assign(static_cast<std::size_t>(m_width * m_height), 0);
    m_depth.assign(static_cast<std::size_t>(m_width * m_height),
                   std::numeric_limits<float>::infinity());
    Log::info("Renderer backend: Software (lit + point lights + AO-lite + water fresnel stub + bloom-lite + tonemap + HUD)");
    return true;
  }

  void destroy() override {
    if (m_texture) {
      SDL_DestroyTexture(m_texture);
      m_texture = nullptr;
    }
    if (m_sdl_renderer) {
      SDL_DestroyRenderer(m_sdl_renderer);
      m_sdl_renderer = nullptr;
    }
    m_window = nullptr;
    m_color.clear();
    m_depth.clear();
  }

  void begin_frame(const Color& clear) override {
    const std::uint32_t c = (static_cast<std::uint32_t>(clear.a) << 24) |
                            (static_cast<std::uint32_t>(clear.r) << 16) |
                            (static_cast<std::uint32_t>(clear.g) << 8) |
                            static_cast<std::uint32_t>(clear.b);
    std::fill(m_color.begin(), m_color.end(), c);
    std::fill(m_depth.begin(), m_depth.end(),
              std::numeric_limits<float>::infinity());
  }

  void set_view_proj(const Mat4& view, const Mat4& proj) override {
    m_view = view;
    m_proj = proj;
    m_view_proj = proj * view;
  }

  void set_camera_position(const Vec3& pos) override { m_camera_pos = pos; }

  void set_lighting(const Lighting& lighting) override { m_lighting = lighting; }

  void set_time(float seconds) override { m_time = seconds; }

  void upload_mesh(Mesh& mesh) override {
    mesh.gpu_uploaded = true;  // CPU path; nothing to upload
  }

  void draw_mesh(const Mesh& mesh, const Mat4& model,
                 const Material& material) override {
    const Mat4 mvp = m_view_proj * model;
    const Vec3 sun = normalize(m_lighting.sun_direction * -1.f);
    const float water_pulse =
        0.85f + 0.15f * std::sin(m_time * 1.7f + material.uv_scroll_u * 3.f);
    const std::size_t nidx = mesh.indices.size();
    for (std::size_t i = 0; i + 2 < nidx; i += 3) {
      SoftVert sv[3];
      bool cull = false;
      for (int k = 0; k < 3; ++k) {
        const Vertex& v =
            mesh.vertices[mesh.indices[i + static_cast<std::size_t>(k)]];
        const Vec4 clip = mul(mvp, Vec4{v.position, 1.f});
        if (clip.w <= 1e-5f) {
          cull = true;
          break;
        }
        const float rhw = 1.f / clip.w;
        const float ndc_x = clip.x * rhw;
        const float ndc_y = clip.y * rhw;
        const float ndc_z = clip.z * rhw;
        sv[k].x = (ndc_x * 0.5f + 0.5f) * static_cast<float>(m_width);
        sv[k].y = (1.f - (ndc_y * 0.5f + 0.5f)) * static_cast<float>(m_height);
        sv[k].z = ndc_z;
        sv[k].rhw = rhw;

        const Vec3 n = normalize(transform_direction(model, v.normal));
        const float ndotl = (std::max)(0.f, dot(n, sun));
        Vec3 base = Vec3{v.color.x * material.albedo.x,
                         v.color.y * material.albedo.y,
                         v.color.z * material.albedo.z};
        if (material.texture == TextureSlot::Water) {
          // Stronger refraction tint on soft path
          base = Vec3{base.x * 0.28f * water_pulse,
                      base.y * 0.72f * water_pulse,
                      base.z * 1.15f * water_pulse};
          base = Vec3{base.x * 0.55f + 0.02f, base.y * 0.85f + 0.08f,
                      base.z * 1.05f + 0.14f};
        } else if (material.texture == TextureSlot::Asphalt) {
          base = base * 0.85f;
        } else if (material.texture == TextureSlot::Brick) {
          base = Vec3{base.x * 1.05f, base.y * 0.85f, base.z * 0.75f};
        } else if (material.texture == TextureSlot::Metal) {
          base = Vec3{base.x * 0.92f, base.y * 0.95f, base.z * 1.02f};
        } else if (material.texture == TextureSlot::Glass) {
          base = Vec3{base.x * 0.75f + 0.05f, base.y * 0.9f + 0.08f,
                      base.z * 1.1f + 0.12f};
        }

        const Vec3 world = transform_point(model, v.position);
        const Vec3 view_dir = normalize(m_camera_pos - world);
        const float hemi = cl01(n.y * 0.5f + 0.5f);
        const float cavity = cl01(dot(n, view_dir));
        float ao = (0.42f + 0.58f * hemi) * (0.65f + 0.35f * cavity);
        ao = 1.f - m_lighting.ao_strength * (1.f - ao);

        // Cheap Blinn + wet anisotropic streak on asphalt
        const Vec3 H = normalize(sun + view_dir);
        float shininess = 4.f + (1.f - cl01(material.roughness)) * 96.f;
        float spec = std::pow((std::max)(0.f, dot(n, H)), shininess) *
                     (1.f - material.roughness * 0.85f);
        const float wet = cl01(material.wetness);
        if (material.texture == TextureSlot::Asphalt && wet > 0.01f) {
          const Vec3 T = normalize(std::fabs(n.x) > 0.7f ? Vec3{0.f, 0.f, 1.f}
                                                         : Vec3{1.f, 0.f, 0.f});
          const float th = dot(T, H);
          const float aniso =
              std::pow((std::max)(0.f, 1.f - th * th), 8.f + 32.f * wet);
          spec = (spec * (1.f - 0.65f * wet) + aniso * 0.65f * wet) *
                 (1.f + 1.2f * wet);
        }
        const float metal = cl01(material.metallic);
        Vec3 spec_col{0.04f + (base.x - 0.04f) * metal,
                      0.04f + (base.y - 0.04f) * metal,
                      0.04f + (base.z - 0.04f) * metal};
        const float metal_diff = 1.f - metal * 0.9f;

        Vec3 lit = m_lighting.ambient * ao +
                   m_lighting.sun_color *
                       (m_lighting.sun_intensity *
                        (ndotl * metal_diff + spec) * ao);
        // fold specular color
        lit.x += m_lighting.sun_color.x * m_lighting.sun_intensity * spec_col.x *
                 spec * ao * 0.35f;
        lit.y += m_lighting.sun_color.y * m_lighting.sun_intensity * spec_col.y *
                 spec * ao * 0.35f;
        lit.z += m_lighting.sun_color.z * m_lighting.sun_intensity * spec_col.z *
                 spec * ao * 0.35f;
        const int pc = (std::max)(0, (std::min)(m_lighting.point_light_count,
                                            Lighting::kMaxPointLights));
        for (int li = 0; li < pc; ++li) {
          const auto& pl = m_lighting.point_lights[li];
          const Vec3 to_l = pl.position - world;
          const float dist_l = length(to_l);
          const float rad = (std::max)(pl.radius, 0.5f);
          float atten = 1.f - cl01(dist_l / rad);
          atten *= atten;
          if (atten <= 1e-4f) {
            continue;
          }
          const Vec3 Lp = to_l * (1.f / (std::max)(dist_l, 0.001f));
          const float nd = (std::max)(0.f, dot(n, Lp));
          lit.x += pl.color.x * pl.intensity * atten * nd * ao * metal_diff;
          lit.y += pl.color.y * pl.intensity * atten * nd * ao * metal_diff;
          lit.z += pl.color.z * pl.intensity * atten * nd * ao * metal_diff;
        }
        Vec3 col{base.x * lit.x + base.x * material.emissive,
                 base.y * lit.y + base.y * material.emissive,
                 base.z * lit.z + base.z * material.emissive};

        // Soft-path reflection stub: fake fresnel toward fog/sky (no 2nd camera;
        // reflections flag off on soft GL — keep a muted tint always for water).
        if (material.texture == TextureSlot::Water &&
            m_lighting.enable_reflections) {
          const float fres =
              std::pow(1.f - cl01(dot(n, view_dir)), 3.f) *
              cl01(m_lighting.reflection_strength) * 0.55f;  // muted vs GL
          col.x = col.x * (1.f - fres) + m_lighting.fog_color.x * fres;
          col.y = col.y * (1.f - fres) + m_lighting.fog_color.y * fres;
          col.z = col.z * (1.f - fres) +
                  (m_lighting.fog_color.z * 0.7f + 0.25f) * fres;
        }

        // Bloom-lite bright-pass for emissives
        if (m_lighting.enable_bloom && material.emissive > 0.05f) {
          const float bright =
              (std::max)(base.x, (std::max)(base.y, base.z)) * material.emissive;
          const float pass = (std::max)(bright - 0.55f, 0.f);
          const float bamt =
              pass * pass * (1.2f + material.emissive) *
              cl01(m_lighting.bloom_strength);
          col.x += base.x * bamt;
          col.y += base.y * bamt;
          col.z += base.z * bamt;
        }

        const float dist = length(world - m_camera_pos);
        float fog = 1.f;
        if (m_lighting.fog_end > m_lighting.fog_start) {
          fog = cl01((m_lighting.fog_end - dist) /
                     (m_lighting.fog_end - m_lighting.fog_start));
        }
        col.x = m_lighting.fog_color.x * (1.f - fog) + col.x * fog;
        col.y = m_lighting.fog_color.y * (1.f - fog) + col.y * fog;
        col.z = m_lighting.fog_color.z * (1.f - fog) + col.z * fog;
        col = tonemap_gamma(col);

        sv[k].r = cl01(col.x);
        sv[k].g = cl01(col.y);
        sv[k].b = cl01(col.z);
      }
      if (!cull) {
        raster_triangle(sv[0], sv[1], sv[2]);
      }
    }
  }

  void draw_hud_rect(float x, float y, float w, float h,
                     const Color& color) override {
    const int x0 = (std::max)(0, static_cast<int>(std::floor(x)));
    const int y0 = (std::max)(0, static_cast<int>(std::floor(y)));
    const int x1 = (std::min)(m_width, static_cast<int>(std::ceil(x + w)));
    const int y1 = (std::min)(m_height, static_cast<int>(std::ceil(y + h)));
    if (x0 >= x1 || y0 >= y1) {
      return;
    }
    const float a = color.a / 255.f;
    const float ia = 1.f - a;
    for (int py = y0; py < y1; ++py) {
      for (int px = x0; px < x1; ++px) {
        const std::size_t idx = static_cast<std::size_t>(py * m_width + px);
        const std::uint32_t dst = m_color[idx];
        const int dr = static_cast<int>((dst >> 16) & 255);
        const int dg = static_cast<int>((dst >> 8) & 255);
        const int db = static_cast<int>(dst & 255);
        const int r = static_cast<int>(dr * ia + color.r * a);
        const int g = static_cast<int>(dg * ia + color.g * a);
        const int b = static_cast<int>(db * ia + color.b * a);
        m_color[idx] = (255u << 24) | (static_cast<std::uint32_t>(r) << 16) |
                       (static_cast<std::uint32_t>(g) << 8) |
                       static_cast<std::uint32_t>(b);
      }
    }
  }

  void end_frame() override {
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_texture, nullptr, &pixels, &pitch) == 0) {
      auto* dst = static_cast<std::uint8_t*>(pixels);
      const int row_bytes = m_width * 4;
      for (int y = 0; y < m_height; ++y) {
        std::memcpy(dst + y * pitch,
                    m_color.data() + static_cast<std::size_t>(y * m_width),
                    static_cast<std::size_t>(row_bytes));
      }
      SDL_UnlockTexture(m_texture);
    }
    SDL_RenderCopy(m_sdl_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_sdl_renderer);
  }

  void resize(int width, int height) override {
    if (width == m_width && height == m_height) {
      return;
    }
    m_width = (std::max)(1, width);
    m_height = (std::max)(1, height);
    if (m_texture) {
      SDL_DestroyTexture(m_texture);
    }
    m_texture = SDL_CreateTexture(m_sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, m_width,
                                  m_height);
    m_color.assign(static_cast<std::size_t>(m_width * m_height), 0);
    m_depth.assign(static_cast<std::size_t>(m_width * m_height),
                   std::numeric_limits<float>::infinity());
  }

  RenderBackendKind kind() const override { return RenderBackendKind::Software; }
  const char* name() const override { return "Software lit+AO+reflect-stub+bloom"; }

 private:
  void raster_triangle(SoftVert v0, SoftVert v1, SoftVert v2) {
    const float min_x = std::floor(std::min({v0.x, v1.x, v2.x}));
    const float max_x = std::ceil(std::max({v0.x, v1.x, v2.x}));
    const float min_y = std::floor(std::min({v0.y, v1.y, v2.y}));
    const float max_y = std::ceil(std::max({v0.y, v1.y, v2.y}));

    const int x0 = (std::max)(0, static_cast<int>(min_x));
    const int y0 = (std::max)(0, static_cast<int>(min_y));
    const int x1 = (std::min)(m_width - 1, static_cast<int>(max_x));
    const int y1 = (std::min)(m_height - 1, static_cast<int>(max_y));

    auto edge = [](const SoftVert& a, const SoftVert& b, float x, float y) {
      return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
    };

    const float area = edge(v0, v1, v2.x, v2.y);
    if (std::fabs(area) < 1e-6f) {
      return;
    }

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;
        const float w0 = edge(v1, v2, px, py) / area;
        const float w1 = edge(v2, v0, px, py) / area;
        const float w2 = edge(v0, v1, px, py) / area;
        if (w0 < 0.f || w1 < 0.f || w2 < 0.f) {
          continue;
        }

        const float z = w0 * v0.z + w1 * v1.z + w2 * v2.z;
        const std::size_t idx = static_cast<std::size_t>(y * m_width + x);
        if (z >= m_depth[idx]) {
          continue;
        }
        m_depth[idx] = z;

        const float rhw = w0 * v0.rhw + w1 * v1.rhw + w2 * v2.rhw;
        const float inv = (rhw > 1e-8f) ? (1.f / rhw) : 1.f;
        float r = (w0 * v0.r * v0.rhw + w1 * v1.r * v1.rhw +
                   w2 * v2.r * v2.rhw) *
                  inv;
        float g = (w0 * v0.g * v0.rhw + w1 * v1.g * v1.rhw +
                   w2 * v2.g * v2.rhw) *
                  inv;
        float b = (w0 * v0.b * v0.rhw + w1 * v1.b * v1.rhw +
                   w2 * v2.b * v2.rhw) *
                  inv;
        r = cl01(r);
        g = cl01(g);
        b = cl01(b);
        const auto R = static_cast<std::uint32_t>(r * 255.f);
        const auto G = static_cast<std::uint32_t>(g * 255.f);
        const auto B = static_cast<std::uint32_t>(b * 255.f);
        m_color[idx] = (255u << 24) | (R << 16) | (G << 8) | B;
      }
    }
  }

  SDL_Window* m_window{nullptr};
  SDL_Renderer* m_sdl_renderer{nullptr};
  SDL_Texture* m_texture{nullptr};
  int m_width{0};
  int m_height{0};
  Mat4 m_view = Mat4::identity();
  Mat4 m_proj = Mat4::identity();
  Mat4 m_view_proj = Mat4::identity();
  Vec3 m_camera_pos{};
  Lighting m_lighting{};
  float m_time{0.f};
  std::vector<std::uint32_t> m_color;
  std::vector<float> m_depth;
};

}  // namespace

std::unique_ptr<IRenderBackend> create_software_backend() {
  return std::make_unique<SoftBackend>();
}

}  // namespace fury
