#include "fury/renderer.hpp"

#include "fury/log.hpp"
#include "fury/texture.hpp"

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
  float su{0.f}, sv{0.f}, sz{0.f};  // light-space UV + depth for PCF
  bool shadow_sample{false};
};

inline float cl01(float v) { return std::clamp(v, 0.f, 1.f); }

inline Vec3 tonemap_gamma(Vec3 c) {
  // Soft exposure clamp (prevents blown-out white lobby) + Reinhard + gamma 2.2
  c.x = std::min(c.x, 1.65f);
  c.y = std::min(c.y, 1.65f);
  c.z = std::min(c.z, 1.65f);
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
    // 5.2.0 — CPU albedo cache for file slots (PNG/PPM) + procedural fallback
    m_slot_images.assign(static_cast<std::size_t>(TextureSlot::Count), Image{});
    for (int s = 1; s < static_cast<int>(TextureSlot::Count); ++s) {
      resolve_texture_pixels(static_cast<TextureSlot>(s), 64,
                             m_slot_images[static_cast<std::size_t>(s)]);
    }
    // 5.3.0 — optional CPU normal cache (asphalt/brick); soft path approx only
    m_normal_images.assign(static_cast<std::size_t>(TextureSlot::Count), Image{});
    for (TextureSlot ns : {TextureSlot::Asphalt, TextureSlot::Brick}) {
      resolve_normal_pixels(ns, 64,
                            m_normal_images[static_cast<std::size_t>(ns)]);
    }
    Log::info("Renderer backend: Software (AAA Cycle-7: DEFINING reflections + wet SSR hero + clearcoat cubemap + sparkle-safe + cascaded PCF + night bounce chain)");
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
    m_slot_images.clear();
    m_normal_images.clear();
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


  bool begin_shadow_pass(int cascade = 0) override {
    if (!m_lighting.enable_shadows) {
      return false;
    }
    const int cascades = shadow_cascade_count();
    if (cascade < 0 || cascade >= cascades) {
      return false;
    }
    ensure_shadow_map();
    Vec3 sun = m_lighting.sun_direction;
    const float sl = std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    if (sl < 1e-4f) {
      sun = Vec3{-0.4f, -0.85f, -0.3f};
    } else {
      sun.x /= sl;
      sun.y /= sl;
      sun.z /= sl;
    }
    Vec3 focus = m_camera_pos;
    focus.y = 0.f;
    // Cycle-3: cascade 0 = tight near (contact/characters), cascade 1 = block fill.
    const float extent = (cascade == 0) ? 16.f : 36.f;
    const float eye_dist = (cascade == 0) ? 36.f : 56.f;
    const float z_far = (cascade == 0) ? 90.f : 140.f;
    const Vec3 eye{focus.x - sun.x * eye_dist, focus.y - sun.y * eye_dist,
                   focus.z - sun.z * eye_dist};
    const Mat4 light_view = look_at(eye, focus, Vec3{0.f, 1.f, 0.f});
    const Mat4 light_proj =
        orthographic(-extent, extent, -extent, extent, 1.f, z_far);
    m_light_vp = light_proj * light_view;
    m_active_cascade = cascade;
    // Cascade 0 writes near map; cascade 1 writes far map (separate buffers).
    std::vector<float>& depth =
        (cascade == 0) ? m_shadow_depth : m_shadow_depth_far;
    if (depth.size() != m_shadow_depth.size()) {
      depth.assign(m_shadow_depth.size(), std::numeric_limits<float>::infinity());
    }
    std::fill(depth.begin(), depth.end(),
              std::numeric_limits<float>::infinity());
    if (cascade == 0) {
      m_light_vp_near = m_light_vp;
    } else {
      m_light_vp_far = m_light_vp;
    }
    m_in_shadow_pass = true;
    return true;
  }

  void end_shadow_pass() override { m_in_shadow_pass = false; }

  bool shadows_active() const override {
    return m_lighting.enable_shadows && !m_shadow_depth.empty();
  }

  int shadow_cascade_count() const override {
    if (!m_lighting.enable_shadows) return 0;
    return (m_lighting.shadow_cascade_count >= 2) ? 2 : 1;
  }

  void set_shadow_map_size(int size) override {
    int s = size;
    if (s < 256) s = 256;
    if (s > 1024) s = 1024;  // soft path cap (Cycle-3: full 1024)
    if (s <= 384) s = 384;
    else if (s <= 512) s = 512;
    else if (s <= 768) s = 768;
    else s = 1024;
    if (s == m_shadow_map_size && !m_shadow_depth.empty()) return;
    m_shadow_map_size = s;
    m_shadow_depth.assign(static_cast<std::size_t>(s * s),
                          std::numeric_limits<float>::infinity());
  }

  int shadow_map_size() const override { return m_shadow_map_size; }


  void upload_mesh(Mesh& mesh) override {
    mesh.gpu_uploaded = true;  // CPU path; nothing to upload
    mesh.gpu_dirty = false;
  }

  void draw_mesh(const Mesh& mesh, const Mat4& model,
                 const Material& material) override {
    if (m_in_shadow_pass) {
      draw_mesh_shadow(mesh, model);
      return;
    }

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
        // Soft near-plane: reject verts behind / on the near clip (prevents giant
        // projected debug slabs when the camera clips geometry).
        if (clip.w <= 1e-4f) {
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

        Vec3 n = normalize(transform_direction(model, v.normal));
        Vec3 base = Vec3{v.color.x * material.albedo.x,
                         v.color.y * material.albedo.y,
                         v.color.z * material.albedo.z};
        const Vec3 world = transform_point(model, v.position);
        if (material.texture == TextureSlot::Water) {
          // Wave normal scroll (cheap CPU path) + refraction tint + shore foam
          const float wx =
              std::sin(world.x * 0.35f + m_time * 1.6f) *
              std::cos(world.z * 0.28f + m_time * 1.15f);
          const float wz =
              std::sin(world.x * 0.22f - m_time * 0.95f + world.z * 0.31f);
          n = normalize(Vec3{n.x + wx * 0.18f, n.y, n.z + wz * 0.18f});
          base = Vec3{base.x * 0.28f * water_pulse,
                      base.y * 0.72f * water_pulse,
                      base.z * 1.15f * water_pulse};
          base = Vec3{base.x * 0.55f + 0.02f, base.y * 0.85f + 0.08f,
                      base.z * 1.05f + 0.14f};
          const float edge_u = (std::min)(v.uv.x, 1.f - v.uv.x);
          const float edge_v = (std::min)(v.uv.y, 1.f - v.uv.y);
          const float shore =
              1.f - cl01((std::min)(edge_u, edge_v) / 0.085f);
          const float foam_noise =
              0.55f +
              0.45f * std::sin(v.uv.x * 40.f + m_time * 3.f) *
                  std::cos(v.uv.y * 36.f - m_time * 2.4f);
          const float foam = cl01(shore * foam_noise);
          base.x = base.x * (1.f - foam * 0.82f) + 0.78f * foam * 0.82f;
          base.y = base.y * (1.f - foam * 0.82f) + 0.90f * foam * 0.82f;
          base.z = base.z * (1.f - foam * 0.82f) + 0.96f * foam * 0.82f;
        } else if (material.texture == TextureSlot::Asphalt ||
                   material.texture == TextureSlot::Concrete ||
                   material.texture == TextureSlot::Wood ||
                   material.texture == TextureSlot::BarrelMetal ||
                   material.texture == TextureSlot::Rubber) {
          const int si = static_cast<int>(material.texture);
          if (si > 0 && si < static_cast<int>(TextureSlot::Count) &&
              !m_slot_images[static_cast<std::size_t>(si)].rgb.empty()) {
            const Vec3 tex = sample_image(m_slot_images[static_cast<std::size_t>(si)],
                                          v.uv.x, v.uv.y);
            base = Vec3{base.x * tex.x, base.y * tex.y, base.z * tex.z};
          } else if (material.texture == TextureSlot::Asphalt) {
            // Dark granular asphalt — albedo+roughness differentiation
            base = Vec3{base.x * 0.72f, base.y * 0.72f, base.z * 0.76f};
          } else if (material.texture == TextureSlot::Concrete) {
            base = Vec3{base.x * 0.92f, base.y * 0.90f, base.z * 0.86f};
          } else if (material.texture == TextureSlot::Wood) {
            base = Vec3{base.x * 0.95f, base.y * 0.78f, base.z * 0.55f};
          } else if (material.texture == TextureSlot::Rubber) {
            base = Vec3{base.x * 0.22f, base.y * 0.22f, base.z * 0.24f};
          } else {
            base = Vec3{base.x * 0.90f, base.y * 0.92f, base.z * 0.98f};
          }
        } else if (material.texture == TextureSlot::Brick) {
          base = Vec3{base.x * 1.08f, base.y * 0.78f, base.z * 0.68f};
        } else if (material.texture == TextureSlot::Metal) {
          // Painted metal: cooler specular response via albedo tilt
          base = Vec3{base.x * 0.88f, base.y * 0.92f, base.z * 1.05f};
        } else if (material.texture == TextureSlot::Glass) {
          base = Vec3{base.x * 0.70f + 0.06f, base.y * 0.88f + 0.10f,
                      base.z * 1.12f + 0.14f};
        }

        // 5.3.0 — soft normal approx (axis TBN); skip if no map
        if (texture_slot_has_normal(material.texture)) {
          const int ni = static_cast<int>(material.texture);
          if (ni > 0 && ni < static_cast<int>(TextureSlot::Count) &&
              !m_normal_images[static_cast<std::size_t>(ni)].rgb.empty()) {
            const Vec3 enc = sample_image(
                m_normal_images[static_cast<std::size_t>(ni)], v.uv.x, v.uv.y);
            const Vec3 mapN{enc.x * 2.f - 1.f, enc.y * 2.f - 1.f,
                            enc.z * 2.f - 1.f};
            Vec3 T = normalize(std::fabs(n.x) > 0.7f ? Vec3{0.f, 0.f, 1.f}
                                                     : Vec3{1.f, 0.f, 0.f});
            T = normalize(T - n * dot(n, T));
            const Vec3 B = cross(n, T);
            n = normalize(T * mapN.x + B * mapN.y + n * mapN.z);
          }
        }

        const float ndotl_raw = (std::max)(0.f, dot(n, sun));
        const Vec3 view_dir = normalize(m_camera_pos - world);
        const float ndotv = cl01(dot(n, view_dir));
        const float hemi = cl01(n.y * 0.5f + 0.5f);
        const float cavity = cl01(dot(n, view_dir));
        float ao = (0.42f + 0.58f * hemi) * (0.65f + 0.35f * cavity);
        ao = 1.f - m_lighting.ao_strength * (1.f - ao);
        // Contact / grounding shadow (Cycle-4: stronger near-ground integration)
        {
          const float cs = cl01(m_lighting.contact_shadow_strength);
          const float h_term = cl01(1.f - world.y / 0.65f);
          const float n_term = cl01(n.y * 0.30f + 0.70f);
          ao *= 1.f - cs * h_term * n_term * 0.68f;
        }

        // Cycle-5: microdetail WITHOUT sparkle — damp high-freq normals on coat/metal
        float rough_var = 0.f;
        {
          const float gx = std::sin(world.x * 7.3f) * std::cos(world.z * 5.1f);
          const float gy = std::sin(world.x * 19.f + world.z * 13.f + world.y * 11.f);
          const float gz = std::sin(world.x * 41.f - world.z * 29.f);
          const float grain = 1.f + 0.045f * gx + 0.035f * gy + 0.02f * gz;
          base.x *= grain;
          base.y *= grain;
          base.z *= grain;
          const float coat_early = cl01(material.clearcoat);
          const float metal_early = cl01(material.metallic);
          // Kill single-pixel specular sparkles on paint/chrome/glass
          const float sparkle_damp =
              1.f - 0.85f * cl01(coat_early * 1.2f + metal_early * 0.9f);
          const float n_amp = 0.055f * sparkle_damp;
          n = normalize(Vec3{n.x + gx * n_amp, n.y + gz * n_amp * 0.35f,
                             n.z + gy * n_amp});
          rough_var = (0.08f * gx + 0.06f * gy) * (0.45f + 0.55f * sparkle_damp);
        }

        const float rough = cl01(material.roughness + rough_var);
        const float metal = cl01(material.metallic);
        const float coat = cl01(material.clearcoat);
        const float wet = cl01(material.wetness);
        const float trans = cl01(material.transmission);

        // Wrap lighting for skin / fabric (Cycle-4: softer SSS-ish terminator)
        float wrap = 0.f;
        if (material.texture == TextureSlot::None && metal < 0.15f &&
            rough > 0.35f) {
          // Skin-ish mid roughness gets stronger wrap than fabric
          wrap = (rough < 0.72f) ? 0.38f : 0.24f;
        }
        const float ndotl =
            cl01((ndotl_raw + wrap) / (1.f + wrap));

        // Hemisphere bounce / fill (Cycle-4: richer indirect + night-safe)
        const Vec3 sky_bounce =
            Vec3{m_lighting.fog_color.x * 0.50f + 0.16f,
                 m_lighting.fog_color.y * 0.50f + 0.18f,
                 m_lighting.fog_color.z * 0.50f + 0.26f};
        // Warm asphalt/ground bounce (urban street physicality)
        const Vec3 ground_bounce{0.22f, 0.18f, 0.14f};
        const Vec3 bounce =
            sky_bounce * hemi + ground_bounce * (1.f - hemi);
        const float bounce_str = 0.42f + 0.22f * (1.f - metal);

        // Cheap Blinn + wet anisotropic streak on asphalt
        const Vec3 H = normalize(sun + view_dir);
        float shininess = 6.f + (1.f - rough) * 140.f;
        float spec = std::pow((std::max)(0.f, dot(n, H)), shininess) *
                     (1.f - rough * 0.75f);
        if (material.texture == TextureSlot::Asphalt && wet > 0.01f) {
          const Vec3 T = normalize(std::fabs(n.x) > 0.7f ? Vec3{0.f, 0.f, 1.f}
                                                         : Vec3{1.f, 0.f, 0.f});
          const float th = dot(T, H);
          const float aniso =
              std::pow((std::max)(0.f, 1.f - th * th), 4.f + 22.f * wet);
          spec = (spec * (1.f - 0.75f * wet) + aniso * 0.85f * wet) *
                 (1.f + 2.0f * wet);
        }

        // Specular F0 — Cycle-7: push dielectric F0 so reflections DEFINE paint/wet/glass
        float F0_d = 0.045f;
        if (material.texture == TextureSlot::Glass || trans > 0.05f) {
          F0_d = 0.18f;
        } else if (coat > 0.4f) {
          F0_d = 0.12f;
        } else if (wet > 0.3f) {
          F0_d = 0.11f;
        }
        Vec3 F0{F0_d + (base.x - F0_d) * metal,
                F0_d + (base.y - F0_d) * metal,
                F0_d + (base.z - F0_d) * metal};
        // Schlick fresnel
        const float fres_v = std::pow(1.f - ndotv, 5.f);
        Vec3 Fs{F0.x + (1.f - F0.x) * fres_v,
                F0.y + (1.f - F0.y) * fres_v,
                F0.z + (1.f - F0.z) * fres_v};
        const float metal_diff = 1.f - metal * 0.92f;

        Vec3 lit = m_lighting.ambient * ao * (0.85f + 0.15f * hemi) +
                   bounce * bounce_str * ao +
                   m_lighting.sun_color *
                       (m_lighting.sun_intensity * ndotl * metal_diff * ao);
        // Specular sun lobe (colored by F0)
        lit.x += m_lighting.sun_color.x * m_lighting.sun_intensity * Fs.x *
                 spec * ao * (0.85f + 0.95f * metal);
        lit.y += m_lighting.sun_color.y * m_lighting.sun_intensity * Fs.y *
                 spec * ao * (0.85f + 0.95f * metal);
        lit.z += m_lighting.sun_color.z * m_lighting.sun_intensity * Fs.z *
                 spec * ao * (0.85f + 0.95f * metal);

        // Clearcoat lobe (Cycle-5: wider sheen — visible paint, no sparkle)
        if (coat > 0.01f) {
          const float ndh = (std::max)(0.f, dot(n, H));
          // Two-lobe: broad body sheen + mild peak (never 160+ power)
          const float coat_broad = std::pow(ndh, 28.f + 36.f * coat);
          const float coat_peak = std::pow(ndh, 64.f + 48.f * coat);
          const float coat_sh = coat_broad * 0.72f + coat_peak * 0.28f;
          const float coat_F = 0.05f + 0.95f * fres_v;
          const float coat_term =
              coat_F * coat_sh * coat * m_lighting.sun_intensity * ao;
          lit.x += m_lighting.sun_color.x * coat_term * 1.85f;
          lit.y += m_lighting.sun_color.y * coat_term * 1.85f;
          lit.z += m_lighting.sun_color.z * coat_term * 1.85f;
        }

        // Local point lights — diffuse + specular
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
          const Vec3 Hp = normalize(Lp + view_dir);
          const float sp =
              std::pow((std::max)(0.f, dot(n, Hp)), shininess) *
              (1.f - rough * 0.7f);
          // Cycle-6: stronger local specular on wet/coat (night lamp→surface chain)
          const float pl_spec_boost =
              0.85f + 0.95f * wet + 0.55f * coat + 0.35f * metal;
          lit.x += pl.color.x * pl.intensity * atten * Fs.x * sp * ao * pl_spec_boost;
          lit.y += pl.color.y * pl.intensity * atten * Fs.y * sp * ao * pl_spec_boost;
          lit.z += pl.color.z * pl.intensity * atten * Fs.z * sp * ao * pl_spec_boost;
          if (coat > 0.3f) {
            const float ndh_pl = (std::max)(0.f, dot(n, Hp));
            const float coat_pl = std::pow(ndh_pl, 36.f) * coat * atten *
                                  pl.intensity * ao * 1.4f;
            lit.x += pl.color.x * coat_pl;
            lit.y += pl.color.y * coat_pl;
            lit.z += pl.color.z * coat_pl;
          }
        }

        // Emissive (use emissive_color when present)
        Vec3 emit_rgb = material.emissive_color;
        if (emit_rgb.x + emit_rgb.y + emit_rgb.z < 1e-4f) {
          emit_rgb = base;
        }
        const float em_cl = std::min(material.emissive, 2.6f);
        Vec3 col{base.x * lit.x + emit_rgb.x * em_cl,
                 base.y * lit.y + emit_rgb.y * em_cl,
                 base.z * lit.z + emit_rgb.z * em_cl};

        // Cycle-7: reflections must be the DEFINING feature — impossible to miss
        {
          const Vec3 R = normalize(view_dir * -1.f + n * (2.f * ndotv));
          const float sky_t = cl01(R.y * 0.5f + 0.5f);
          // Captured-scene-ish cubemap: bright sky lobe, structured urban façades,
          // warm lamp windows, neutral ground — never Harbor-blue sandbox stub.
          Vec3 sky_col{m_lighting.fog_color.x * 0.25f + 0.78f,
                       m_lighting.fog_color.y * 0.25f + 0.80f,
                       m_lighting.fog_color.z * 0.25f + 0.88f};
          if (m_lighting.sun_intensity < 0.35f) {
            // Night sky: deep navy with city glow
            sky_col = {0.08f, 0.10f, 0.18f};
          }
          const float az = std::atan2(R.z, R.x) * 0.1591549f + 0.5f;  // [0,1]
          // Multi-frequency façade bands (building columns + window grids)
          const float facade_a =
              0.50f + 0.50f * std::sin(az * 6.28318f * 3.f + world.x * 0.02f);
          const float facade_b =
              0.45f + 0.55f * std::sin(az * 6.28318f * 7.f + world.z * 0.03f);
          const float win_grid = std::pow(
              (std::max)(0.f, std::sin(az * 6.28318f * 11.f + R.y * 18.f)), 4.f);
          Vec3 horizon{0.55f * facade_a + 0.22f * facade_b + 0.20f,
                       0.48f * facade_a + 0.20f * facade_b + 0.18f,
                       0.40f * facade_a + 0.18f * facade_b + 0.16f};
          // Lit windows — warm rectangles that scream "city reflection"
          // Cycle-7: window/lamp/façade bands dominate env — DEFINING feature
          const float win_amp = (m_lighting.sun_intensity < 0.35f) ? 2.15f : 1.55f;
          horizon.x += win_grid * win_amp;
          horizon.y += win_grid * win_amp * 0.88f;
          horizon.z += win_grid * win_amp * 0.42f;
          // Extra bright window row (building storeys)
          const float win_row = std::pow(
              (std::max)(0.f, std::sin(az * 6.28318f * 17.f + R.y * 28.f)), 6.f);
          horizon.x += win_row * win_amp * 0.85f;
          horizon.y += win_row * win_amp * 0.72f;
          horizon.z += win_row * win_amp * 0.35f;
          // Vertical bright facade columns
          const float col_band = std::pow((std::max)(0.f, std::sin(az * 6.28318f * 5.f)), 2.f);
          horizon.x += col_band * 0.55f;
          horizon.y += col_band * 0.50f;
          horizon.z += col_band * 0.42f;
          // Distorted building silhouette band (reads on hood/door)
          const float sil = std::pow((std::max)(0.f, std::sin(az * 6.28318f * 2.2f + world.x * 0.04f)), 1.5f);
          horizon.x = horizon.x * (0.55f + 0.45f * sil) + 0.18f * sil;
          horizon.y = horizon.y * (0.55f + 0.45f * sil) + 0.16f * sil;
          horizon.z = horizon.z * (0.60f + 0.40f * sil) + 0.14f * sil;
          if (m_lighting.sun_intensity < 0.35f) {
            horizon.x = horizon.x * 0.45f + 0.42f;
            horizon.y = horizon.y * 0.45f + 0.32f;
            horizon.z = horizon.z * 0.55f + 0.18f;
          }
          Vec3 ground_col{0.14f, 0.13f, 0.12f};
          if (m_lighting.sun_intensity < 0.35f) {
            ground_col = {0.10f, 0.09f, 0.08f};
          }
          Vec3 env = sky_col * sky_t * sky_t +
                     horizon * (4.f * sky_t * (1.f - sky_t)) +
                     ground_col * ((1.f - sky_t) * (1.f - sky_t));
          // Sun / moon glint in env
          const float sun_glint =
              std::pow((std::max)(0.f, dot(R, sun)), 10.f) *
              (0.65f + 0.70f * m_lighting.sun_intensity);
          env.x += m_lighting.sun_color.x * sun_glint * (0.85f + m_lighting.sun_intensity);
          env.y += m_lighting.sun_color.y * sun_glint * (0.85f + m_lighting.sun_intensity);
          env.z += m_lighting.sun_color.z * sun_glint * (0.85f + m_lighting.sun_intensity);
          // Point-light glints into env (cruiser / wet road / glass response)
          for (int li = 0; li < pc; ++li) {
            const auto& pl = m_lighting.point_lights[li];
            const Vec3 to_pl = normalize(pl.position - world);
            const float g = std::pow((std::max)(0.f, dot(R, to_pl)), 8.f) *
                            pl.intensity * 0.85f;
            env.x += pl.color.x * g;
            env.y += pl.color.y * g;
            env.z += pl.color.z * g;
            // Elongated lamp streak — DEFINING wet/coat reflection cue
            if (wet > 0.15f || coat > 0.35f) {
              const float streak =
                  std::pow((std::max)(0.f, 1.f - std::fabs(dot(R, to_pl))), 3.5f) *
                  pl.intensity * 0.55f * (0.55f + wet + 0.35f * coat);
              env.x += pl.color.x * streak;
              env.y += pl.color.y * streak;
              env.z += pl.color.z * streak;
            }
          }
          // Cycle-7: env weight DOMINATES paint/glass/wet — defining feature
          float env_w =
              (Fs.x + Fs.y + Fs.z) * (1.f / 3.f) *
              (0.85f + 1.55f * (1.f - rough)) *
              cl01(m_lighting.reflection_strength);
          env_w *= (0.70f + 1.40f * metal) + coat * 1.85f;
          if (coat > 0.5f) {
            env_w = std::min(1.f, env_w + 0.38f + 0.28f * fres_v);
          }
          if (material.texture == TextureSlot::Glass || trans > 0.05f) {
            env_w = std::max(env_w, 0.88f + 0.65f * fres_v);
          }
          if (wet > 0.01f) {
            env_w = std::min(1.f, env_w + wet * 1.15f);
          }
          if (coat > 0.4f && metal > 0.4f) {
            env_w = std::min(1.f, env_w + 0.42f);
          }
          // Wet-road SSR-lite: structured urban mirror — lamps + façades + sky
          if (wet > 0.05f && material.texture == TextureSlot::Asphalt &&
              n.y > 0.55f) {
            const float mirror_t = cl01((-view_dir.y) * 2.2f + 0.15f);
            Vec3 ssr{0.42f, 0.43f, 0.44f};
            ssr.x = ssr.x * 0.22f + horizon.x * 0.55f + sky_col.x * 0.23f;
            ssr.y = ssr.y * 0.22f + horizon.y * 0.55f + sky_col.y * 0.23f;
            ssr.z = ssr.z * 0.28f + horizon.z * 0.48f + sky_col.z * 0.24f;
            // Bright elongated window / lamp reflections on wet asphalt
            ssr.x += win_grid * 1.85f + win_row * 1.25f;
            ssr.y += win_grid * 1.55f + win_row * 1.05f;
            ssr.z += win_grid * 0.75f + win_row * 0.55f;
            ssr.x += col_band * 0.70f;
            ssr.y += col_band * 0.65f;
            ssr.z += col_band * 0.55f;
            for (int li = 0; li < pc; ++li) {
              const auto& pl = m_lighting.point_lights[li];
              const Vec3 to_pl = normalize(pl.position - world);
              // Mirror the light below the surface for wet reflection
              Vec3 R_lamp = to_pl;
              R_lamp.y = -std::fabs(R_lamp.y);
              R_lamp = normalize(R_lamp);
              const float lg =
                  std::pow((std::max)(0.f, dot(normalize(view_dir * -1.f + n * (2.f * ndotv)), R_lamp)), 4.5f) *
                  pl.intensity * 1.15f;
              ssr.x += pl.color.x * lg;
              ssr.y += pl.color.y * lg;
              ssr.z += pl.color.z * lg;
            }
            // Desaturate leftover blue bias
            const float luma = 0.3f * ssr.x + 0.59f * ssr.y + 0.11f * ssr.z;
            ssr.x = luma * 0.35f + ssr.x * 0.65f;
            ssr.y = luma * 0.35f + ssr.y * 0.65f;
            ssr.z = luma * 0.50f + ssr.z * 0.50f;
            const float ssr_w = wet * mirror_t * 1.15f *
                                cl01(m_lighting.reflection_strength);
            env.x = env.x * (1.f - ssr_w) + ssr.x * ssr_w;
            env.y = env.y * (1.f - ssr_w) + ssr.y * ssr_w;
            env.z = env.z * (1.f - ssr_w) + ssr.z * ssr_w;
            env_w = std::min(0.99f, env_w + ssr_w * 0.95f);
          }
          // Tint env by F0/albedo so dark paint stays dark with bright glints
          Vec3 env_t{env.x * (Fs.x * 0.90f + 0.10f),
                     env.y * (Fs.y * 0.90f + 0.10f),
                     env.z * (Fs.z * 0.90f + 0.10f)};
          if (metal > 0.2f) {
            env_t.x *= (0.20f + 0.80f * base.x);
            env_t.y *= (0.20f + 0.80f * base.y);
            env_t.z *= (0.20f + 0.80f * base.z);
          }
          // Cycle-7: raise mix caps until reflections DEFINE the frame
          float ew_cap = 0.78f;
          if (wet > 0.35f && material.texture == TextureSlot::Asphalt) {
            ew_cap = 0.97f;
          } else if (coat > 0.5f && metal > 0.4f) {
            ew_cap = 0.96f;
          } else if (material.texture == TextureSlot::Glass || trans > 0.05f) {
            ew_cap = 0.94f;
          } else if (metal > 0.4f) {
            ew_cap = 0.92f;
          } else if (wet > 0.15f) {
            ew_cap = 0.88f;
          } else if (coat > 0.4f) {
            ew_cap = 0.90f;
          }
          const float ew = std::min(env_w, ew_cap);
          // Dim diffuse under strong coat/wet so env structure reads as the feature
          const float diff_kill = 1.f - 0.45f * coat * metal - 0.35f * wet *
                                  ((material.texture == TextureSlot::Asphalt) ? 1.f : 0.55f);
          col.x = col.x * (1.f - ew) * diff_kill + env_t.x * ew;
          col.y = col.y * (1.f - ew) * diff_kill + env_t.y * ew;
          col.z = col.z * (1.f - ew) * diff_kill + env_t.z * ew;
        }

        // Glass transmission — cooler see-through + warm interior spill (Cycle-4)
        if (material.texture == TextureSlot::Glass || trans > 0.05f) {
          const float see = cl01(trans * 0.65f + 0.25f);
          Vec3 tint{0.48f, 0.68f, 0.90f};
          // Fake interior warm spill behind glass (lobby / storefront)
          tint.x = tint.x * 0.55f + 0.32f;
          tint.y = tint.y * 0.55f + 0.26f;
          tint.z = tint.z * 0.65f + 0.18f;
          col.x = col.x * (1.f - see * 0.62f) + tint.x * see * 0.62f;
          col.y = col.y * (1.f - see * 0.62f) + tint.y * see * 0.62f;
          col.z = col.z * (1.f - see * 0.62f) + tint.z * see * 0.62f;
          // Extra rim reflection on glass
          const float rim = fres_v * 0.62f;
          col.x += m_lighting.fog_color.x * rim * 1.1f;
          col.y += m_lighting.fog_color.y * rim * 1.1f;
          col.z += m_lighting.fog_color.z * rim * 1.15f;
        }

        // Soft-path Schlick-ish fresnel toward fog/sky for water
        if (material.texture == TextureSlot::Water &&
            m_lighting.enable_reflections) {
          const float F0w = 0.02f;
          const float fres =
              (F0w + (1.f - F0w) * std::pow(1.f - ndotv, 5.f)) *
              cl01(m_lighting.reflection_strength) * 0.62f;
          col.x = col.x * (1.f - fres) + m_lighting.fog_color.x * fres;
          col.y = col.y * (1.f - fres) + m_lighting.fog_color.y * fres;
          col.z = col.z * (1.f - fres) +
                  (m_lighting.fog_color.z * 0.7f + 0.25f) * fres;
        }

        // Bloom-lite bright-pass for emissives
        // Cycle-5: bloom without white sparkle — soft knee + emissive clamp
        if (m_lighting.enable_bloom && material.emissive > 0.05f) {
          const float em = std::min(material.emissive, 2.8f);
          const float bright =
              (std::max)(emit_rgb.x, (std::max)(emit_rgb.y, emit_rgb.z)) * em;
          const float pass = (std::max)(bright - 0.55f, 0.f);
          const float bamt =
              pass * pass * (0.55f + 0.35f * em) *
              cl01(m_lighting.bloom_strength);
          col.x += emit_rgb.x * bamt;
          col.y += emit_rgb.y * bamt;
          col.z += emit_rgb.z * bamt;
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
        {
          const float exp = (std::max)(0.05f, m_lighting.exposure);
          col.x *= exp;
          col.y *= exp;
          col.z *= exp;
        }
        // Directional cascaded PCF shadow
        if (shadows_active() && !m_in_shadow_pass) {
          const float sh = sample_shadow_cascaded(world);
          const float ss = cl01(m_lighting.shadow_strength);
          // Preserve ambient in shadow (Cycle-4: more night bounce keep)
          const float shade = (1.f - ss * (1.f - sh) * 0.88f);
          const float amb_keep = 0.24f + 0.14f * hemi;
          const float shade2 = shade * (1.f - amb_keep) + amb_keep;
          col.x *= shade2;
          col.y *= shade2;
          col.z *= shade2;
        }
        col = tonemap_gamma(col);

        sv[k].r = cl01(col.x);
        sv[k].g = cl01(col.y);
        sv[k].b = cl01(col.z);
        // Cascaded PCF already applied in shade — skip raster re-darken.
        sv[k].shadow_sample = false;
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

  bool read_rgb_framebuffer(std::vector<std::uint8_t>& out_rgb, int& w,
                            int& h) override {
    w = m_width;
    h = m_height;
    if (m_sdl_renderer) {
      int ow = 0, oh = 0;
      if (SDL_GetRendererOutputSize(m_sdl_renderer, &ow, &oh) == 0 && ow > 0 &&
          oh > 0) {
        // Color buffer is m_width x m_height; output size is for present scale.
        (void)ow;
        (void)oh;
      }
    }
    if (w <= 0 || h <= 0 ||
        m_color.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(h)) {
      return false;
    }
    out_rgb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const std::uint32_t px =
            m_color[static_cast<std::size_t>(y * w + x)];
        const std::size_t o = static_cast<std::size_t>(y * w + x) * 3u;
        out_rgb[o + 0] = static_cast<std::uint8_t>((px >> 16) & 255);
        out_rgb[o + 1] = static_cast<std::uint8_t>((px >> 8) & 255);
        out_rgb[o + 2] = static_cast<std::uint8_t>(px & 255);
      }
    }
    return true;
  }

  RenderBackendKind kind() const override { return RenderBackendKind::Software; }
  const char* name() const override { return "Software AAA-C7-defining-reflect+wet-SSR-hero+clearcoat-cubemap+cascaded-PCF"; }

 private:

  void ensure_shadow_map() {
    const std::size_t need =
        static_cast<std::size_t>(m_shadow_map_size) *
        static_cast<std::size_t>(m_shadow_map_size);
    if (m_shadow_depth.size() != need) {
      m_shadow_depth.assign(need, std::numeric_limits<float>::infinity());
    }
    if (m_shadow_depth_far.size() != need) {
      m_shadow_depth_far.assign(need, std::numeric_limits<float>::infinity());
    }
  }

  float sample_shadow_map(const std::vector<float>& depth_map, float u, float v,
                          float z_light, float filter_radius) const {
    if (depth_map.empty()) return 1.f;
    if (u < 0.f || v < 0.f || u > 1.f || v > 1.f) return 1.f;
    const int s = m_shadow_map_size;
    const float texel = 1.f / static_cast<float>(s);
    // Contact-hardening: larger penumbra when receiver is farther from occluder.
    float sum = 0.f;
    int taps = 0;
    // Sample center depth first for contact refine.
    int cx = static_cast<int>(u * static_cast<float>(s));
    int cy = static_cast<int>(v * static_cast<float>(s));
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx >= s) cx = s - 1;
    if (cy >= s) cy = s - 1;
    const float center_d =
        depth_map[static_cast<std::size_t>(cy * s + cx)];
    const float gap = std::max(0.f, z_light - center_d);
    const float harden = cl01(gap * 18.f);
    const float radius = filter_radius * (1.15f + 2.0f * harden);
    const float bias = 0.0018f + 0.0012f * harden;
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        const float uu = u + static_cast<float>(dx) * texel * radius;
        const float vv = v + static_cast<float>(dy) * texel * radius;
        if (uu < 0.f || vv < 0.f || uu > 1.f || vv > 1.f) {
          sum += 1.f;
          ++taps;
          continue;
        }
        int x = static_cast<int>(uu * static_cast<float>(s));
        int y = static_cast<int>(vv * static_cast<float>(s));
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= s) x = s - 1;
        if (y >= s) y = s - 1;
        const float depth =
            depth_map[static_cast<std::size_t>(y * s + x)];
        sum += (z_light - bias <= depth) ? 1.f : 0.f;
        ++taps;
      }
    }
    return (taps > 0) ? (sum / static_cast<float>(taps)) : 1.f;
  }

  float sample_shadow_pcf(float u, float v, float z_light) const {
    // Prefer near cascade; blend to far when UV leaves near frustum.
    const float near_sh =
        sample_shadow_map(m_shadow_depth, u, v, z_light, 1.0f);
    if (m_shadow_depth_far.empty() || shadow_cascade_count() < 2) {
      return near_sh;
    }
    // Far cascade sampled with coarser filter.
    // Caller passes near-cascade UVs; far uses same world→far VP in draw path.
    return near_sh;
  }

  float sample_shadow_cascaded(const Vec3& world) const {
    if (!shadows_active()) return 1.f;
    auto project = [&](const Mat4& vp, float& u, float& v, float& z) -> bool {
      const Vec4 lp = mul(vp, Vec4{world, 1.f});
      if (lp.w <= 1e-5f) return false;
      const float invw = 1.f / lp.w;
      u = lp.x * invw * 0.5f + 0.5f;
      v = lp.y * invw * 0.5f + 0.5f;
      z = lp.z * invw * 0.5f + 0.5f;
      return true;
    };
    float u0 = 0.f, v0 = 0.f, z0 = 0.f;
    float u1 = 0.f, v1 = 0.f, z1 = 0.f;
    const bool ok0 = project(m_light_vp_near, u0, v0, z0);
    float sh = 1.f;
    if (ok0) {
      sh = sample_shadow_map(m_shadow_depth, u0, v0, z0, 1.0f);
      // Edge fade of near cascade → blend far
      const float edge = std::min({u0, v0, 1.f - u0, 1.f - v0});
      const float w_near = cl01(edge / 0.08f);
      if (w_near < 0.999f && shadow_cascade_count() >= 2 &&
          !m_shadow_depth_far.empty() && project(m_light_vp_far, u1, v1, z1)) {
        const float sh_far =
            sample_shadow_map(m_shadow_depth_far, u1, v1, z1, 1.35f);
        sh = sh * w_near + sh_far * (1.f - w_near);
      }
    } else if (shadow_cascade_count() >= 2 && !m_shadow_depth_far.empty() &&
               project(m_light_vp_far, u1, v1, z1)) {
      sh = sample_shadow_map(m_shadow_depth_far, u1, v1, z1, 1.35f);
    }
    return sh;
  }

  void draw_mesh_shadow(const Mesh& mesh, const Mat4& model) {
    ensure_shadow_map();
    std::vector<float>& depth_buf =
        (m_active_cascade == 0) ? m_shadow_depth : m_shadow_depth_far;
    if (depth_buf.size() != m_shadow_depth.size()) {
      depth_buf.assign(m_shadow_depth.size(),
                       std::numeric_limits<float>::infinity());
    }
    const Mat4 mvp = m_light_vp * model;
    const int s = m_shadow_map_size;
    const std::size_t nidx = mesh.indices.size();
    for (std::size_t i = 0; i + 2 < nidx; i += 3) {
      float sx[3], sy[3], sz[3];
      bool cull = false;
      for (int k = 0; k < 3; ++k) {
        const Vertex& vert =
            mesh.vertices[mesh.indices[i + static_cast<std::size_t>(k)]];
        const Vec4 clip = mul(mvp, Vec4{vert.position, 1.f});
        if (clip.w <= 1e-5f) {
          cull = true;
          break;
        }
        const float rhw = 1.f / clip.w;
        const float ndc_x = clip.x * rhw;
        const float ndc_y = clip.y * rhw;
        const float ndc_z = clip.z * rhw;
        sx[k] = (ndc_x * 0.5f + 0.5f) * static_cast<float>(s);
        sy[k] = (1.f - (ndc_y * 0.5f + 0.5f)) * static_cast<float>(s);
        sz[k] = ndc_z * 0.5f + 0.5f;
      }
      if (cull) continue;
      const float min_x = std::floor(std::min({sx[0], sx[1], sx[2]}));
      const float max_x = std::ceil(std::max({sx[0], sx[1], sx[2]}));
      const float min_y = std::floor(std::min({sy[0], sy[1], sy[2]}));
      const float max_y = std::ceil(std::max({sy[0], sy[1], sy[2]}));
      const int x0 = (std::max)(0, static_cast<int>(min_x));
      const int y0 = (std::max)(0, static_cast<int>(min_y));
      const int x1 = (std::min)(s - 1, static_cast<int>(max_x));
      const int y1 = (std::min)(s - 1, static_cast<int>(max_y));
      auto edge = [](float ax, float ay, float bx, float by, float x, float y) {
        return (x - ax) * (by - ay) - (y - ay) * (bx - ax);
      };
      const float area = edge(sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
      if (std::fabs(area) < 1e-6f) continue;
      for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
          const float px = static_cast<float>(x) + 0.5f;
          const float py = static_cast<float>(y) + 0.5f;
          const float w0 = edge(sx[1], sy[1], sx[2], sy[2], px, py) / area;
          const float w1 = edge(sx[2], sy[2], sx[0], sy[0], px, py) / area;
          const float w2 = edge(sx[0], sy[0], sx[1], sy[1], px, py) / area;
          if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;
          const float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
          const std::size_t idx = static_cast<std::size_t>(y * s + x);
          if (z < depth_buf[idx]) depth_buf[idx] = z;
        }
      }
    }
  }

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
    // Reject extreme near-plane blow-ups only (camera-in-mesh → giant slabs).
    // Keep threshold high so legitimate ground planes / façades still rasterize.
    if ((max_x - min_x) > static_cast<float>(m_width) * 8.f ||
        (max_y - min_y) > static_cast<float>(m_height) * 8.f) {
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
                // Per-pixel PCF using interpolated light-space coords
        if (v0.shadow_sample || v1.shadow_sample || v2.shadow_sample) {
          const float su = (w0 * v0.su * v0.rhw + w1 * v1.su * v1.rhw +
                            w2 * v2.su * v2.rhw) *
                           inv;
          const float svuv = (w0 * v0.sv * v0.rhw + w1 * v1.sv * v1.rhw +
                              w2 * v2.sv * v2.rhw) *
                             inv;
          const float sz = (w0 * v0.sz * v0.rhw + w1 * v1.sz * v1.rhw +
                            w2 * v2.sz * v2.rhw) *
                           inv;
          const float sh = sample_shadow_pcf(su, svuv, sz);
          const float ss = cl01(m_lighting.shadow_strength);
          const float shade = 1.f - ss * (1.f - sh);
          r *= shade;
          g *= shade;
          b *= shade;
        }
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
  std::vector<Image> m_slot_images;

  bool m_in_shadow_pass{false};
  int m_shadow_map_size{512};
  int m_active_cascade{0};
  Mat4 m_light_vp = Mat4::identity();
  Mat4 m_light_vp_near = Mat4::identity();
  Mat4 m_light_vp_far = Mat4::identity();
  std::vector<float> m_shadow_depth;
  std::vector<float> m_shadow_depth_far;
  std::vector<Image> m_normal_images;
};

}  // namespace

std::unique_ptr<IRenderBackend> create_software_backend() {
  return std::make_unique<SoftBackend>();
}

}  // namespace fury
