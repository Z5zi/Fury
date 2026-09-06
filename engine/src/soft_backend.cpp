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

class SoftBackend final : public IRenderBackend {
 public:
  ~SoftBackend() override { destroy(); }

  bool create(SDL_Window* window, int width, int height) override {
    destroy();
    m_window = window;
    m_width = std::max(1, width);
    m_height = std::max(1, height);

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
    Log::info("Renderer backend: Software (CPU rasterizer)");
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
    m_view_proj = proj * view;
  }

  void upload_mesh(Mesh& mesh) override {
    mesh.gpu_uploaded = true;  // CPU path; nothing to upload
  }

  void draw_mesh(const Mesh& mesh, const Mat4& model) override {
    const Mat4 mvp = m_view_proj * model;
    const std::size_t nidx = mesh.indices.size();
    for (std::size_t i = 0; i + 2 < nidx; i += 3) {
      SoftVert sv[3];
      bool cull = false;
      for (int k = 0; k < 3; ++k) {
        const Vertex& v = mesh.vertices[mesh.indices[i + static_cast<std::size_t>(k)]];
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
        sv[k].r = v.color.x;
        sv[k].g = v.color.y;
        sv[k].b = v.color.z;
      }
      if (!cull) {
        raster_triangle(sv[0], sv[1], sv[2]);
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
    m_width = std::max(1, width);
    m_height = std::max(1, height);
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
  const char* name() const override { return "Software"; }

 private:
  void raster_triangle(SoftVert v0, SoftVert v1, SoftVert v2) {
    // Edge function rasterizer with perspective-correct attributes via 1/w
    const float min_x = std::floor(std::min({v0.x, v1.x, v2.x}));
    const float max_x = std::ceil(std::max({v0.x, v1.x, v2.x}));
    const float min_y = std::floor(std::min({v0.y, v1.y, v2.y}));
    const float max_y = std::ceil(std::max({v0.y, v1.y, v2.y}));

    const int x0 = std::max(0, static_cast<int>(min_x));
    const int y0 = std::max(0, static_cast<int>(min_y));
    const int x1 = std::min(m_width - 1, static_cast<int>(max_x));
    const int y1 = std::min(m_height - 1, static_cast<int>(max_y));

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
        const std::size_t idx =
            static_cast<std::size_t>(y * m_width + x);
        if (z >= m_depth[idx]) {
          continue;
        }
        m_depth[idx] = z;

        // Approximate perspective: interpolate * rhw then divide
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
        r = std::clamp(r, 0.f, 1.f);
        g = std::clamp(g, 0.f, 1.f);
        b = std::clamp(b, 0.f, 1.f);
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
  Mat4 m_view_proj = Mat4::identity();
  std::vector<std::uint32_t> m_color;
  std::vector<float> m_depth;
};

}  // namespace

std::unique_ptr<IRenderBackend> create_software_backend() {
  return std::make_unique<SoftBackend>();
}

}  // namespace fury
