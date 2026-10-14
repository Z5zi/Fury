#include "fury/renderer.hpp"

#include "fury/log.hpp"
#include "fury/surface_detail.hpp"
#include "fury/texture.hpp"

#include <SDL.h>
#include <cstdlib>
#include <cstring>

namespace fury {

Renderer::~Renderer() { destroy(); }

bool Renderer::create(SDL_Window* window, int width, int height,
                      bool window_is_opengl, RenderBackendKind preferred) {
  destroy();
  if (!window) {
    Log::error("Renderer::create: null window");
    return false;
  }

  m_surface_detail=true;
  m_surface_detail_resolution=512;
  if(const char* detail=std::getenv("FURY_SURFACE_DETAIL")) {
    if(std::strcmp(detail,"0")==0) m_surface_detail=false;
    else if(std::strcmp(detail,"1")!=0) { Log::error("FURY_SURFACE_DETAIL must be 0 or 1"); return false; }
  }
  if(const char* resolution=std::getenv("FURY_SURFACE_DETAIL_RES")) {
    char* end{}; const auto value=std::strtol(resolution,&end,10);
    if(end==resolution || *end || (value!=128 && value!=256 && value!=512)) {
      Log::error("FURY_SURFACE_DETAIL_RES must be 128, 256 or 512"); return false;
    }
    m_surface_detail_resolution=unsigned(value);
  }
  const char* requested = std::getenv("FURY_RENDERER");
  if (preferred == RenderBackendKind::CpuRayTracing ||
      (preferred == RenderBackendKind::None && requested && std::strcmp(requested, "cpu-ray") == 0)) {
    auto cpu = create_cpu_ray_backend();
    if (!cpu || !cpu->create(window, width, height)) return false;
    m_backend = std::move(cpu);
    m_backend->set_lighting(m_lighting);
    return true;
  }
  if (preferred==RenderBackendKind::Direct3D12 ||
      (preferred==RenderBackendKind::None && requested && std::strcmp(requested, "dx12") == 0)) {
    auto dx12 = create_dx12_backend();
    if (!dx12 || !dx12->create(window, width, height)) {
      Log::error("Requested DX12 renderer could not initialize; no silent fallback");
      return false;
    }
    m_backend = std::move(dx12);
    m_backend->set_lighting(m_lighting);
    return true;
  }

  if (window_is_opengl && preferred!=RenderBackendKind::Software) {
    auto gl = create_gl_backend();
    if (gl && gl->create(window, width, height)) {
      m_backend = std::move(gl);
      m_backend->set_lighting(m_lighting);
      if (m_msaa_samples > 0) m_backend->set_msaa_samples(m_msaa_samples);
      return true;
    }
    Log::warn("OpenGL backend unavailable; falling back to software");
  }

  auto soft = create_software_backend();
  if (!soft || !soft->create(window, width, height)) {
    Log::error("Failed to create software renderer backend");
    return false;
  }
  m_backend = std::move(soft);
  m_backend->set_lighting(m_lighting);
  // Soft path: set_msaa_samples is a no-op
  if (m_msaa_samples > 0) m_backend->set_msaa_samples(m_msaa_samples);
  return true;
}

void Renderer::destroy() {
  if (m_backend) {
    m_backend->destroy();
    m_backend.reset();
  }
  for(auto& maps:m_surface_maps) maps.reset();
}

void Renderer::begin_frame(const Color& clear) {
  if (m_backend) m_backend->begin_frame(clear);
}

void Renderer::set_view_proj(const Mat4& view, const Mat4& proj) {
  if (m_backend) m_backend->set_view_proj(view, proj);
}

void Renderer::set_camera_position(const Vec3& pos) {
  if (m_backend) m_backend->set_camera_position(pos);
}

void Renderer::set_lighting(const Lighting& lighting) {
  m_lighting = lighting;
  if (m_backend) m_backend->set_lighting(lighting);
}

void Renderer::set_time(float seconds) {
  if (m_backend) m_backend->set_time(seconds);
}

void Renderer::draw_mesh(const Mesh& mesh, const Mat4& model,
                         const Material& material, std::uint64_t object_id) {
  if (m_backend) {
    m_backend->set_object_id(object_id);
    Material effective=material;
    const TextureSlot profile=material.detail_texture!=TextureSlot::None ? material.detail_texture:material.texture;
    const auto* maps=material.textures.get();
    const bool authored_maps=maps && (maps->base_color.valid() || maps->normal.valid() ||
                                     maps->metallic_roughness.valid() || maps->emissive.valid());
    // glTF imports carry emissive strength 1 even when their emission factor is
    // black. Test radiance semantics, not that scalar alone.
    const bool emitting=material.emissive>0.f && (!maps || material.emissive_color.x>0.f ||
      material.emissive_color.y>0.f || material.emissive_color.z>0.f);
    // Preserve authored maps, glass, water and legacy emissive semantics. This
    // is a shared detail layer for deliberately selected opaque surfaces only.
    if(m_surface_detail && supports_surface_detail(profile) && !authored_maps &&
       !emitting && material.transmission<=0.f && !material.alpha_blend && material.alpha_cutoff<0.f) {
      const unsigned rotation=unsigned(material.detail_rotation)%4;
      auto& detail=m_surface_maps[static_cast<std::size_t>(profile)*4+rotation];
      if(!detail) detail=surface_detail_textures(profile,m_surface_detail_resolution,rotation);
      effective.textures=detail;
      if(material.detail_use_mesh_uvs) effective.world_uv_scale=0.f;
      else if(effective.world_uv_scale<=0.f) effective.world_uv_scale=surface_detail_uv_per_meter(profile);
    }
    m_backend->draw_mesh(mesh, model, effective);
  }
}

bool Renderer::configure(const RenderSettings& settings) {
  return m_backend && m_backend->configure(settings);
}
RenderStatistics Renderer::statistics() const {
  return m_backend ? m_backend->statistics() : RenderStatistics{};
}
RenderSettings Renderer::settings() const { return m_backend ? m_backend->settings() : RenderSettings{}; }
void Renderer::reset_history() { if (m_backend) m_backend->reset_history(); }

#if !FURY_HAS_DX12
std::unique_ptr<IRenderBackend> create_dx12_backend() { return {}; }
#endif

void Renderer::draw_hud_rect(float x, float y, float w, float h,
                             const Color& color) {
  if (m_backend) m_backend->draw_hud_rect(x, y, w, h, color);
}

void Renderer::end_frame() {
  if (m_backend) m_backend->end_frame();
}

void Renderer::upload_mesh(Mesh& mesh) {
  if (m_backend) m_backend->upload_mesh(mesh);
}

void Renderer::resize(int width, int height) {
  if (m_backend) m_backend->resize(width, height);
}

RenderBackendKind Renderer::backend_kind() const {
  return m_backend ? m_backend->kind() : RenderBackendKind::None;
}

const char* Renderer::backend_name() const {
  return m_backend ? m_backend->name() : "None";
}


bool Renderer::begin_shadow_pass(int cascade) {
  return m_backend ? m_backend->begin_shadow_pass(cascade) : false;
}

void Renderer::end_shadow_pass() {
  if (m_backend) m_backend->end_shadow_pass();
}

bool Renderer::shadows_active() const {
  return m_backend ? m_backend->shadows_active() : false;
}

int Renderer::shadow_cascade_count() const {
  return m_backend ? m_backend->shadow_cascade_count() : 0;
}

void Renderer::set_shadow_map_size(int size) {
  m_lighting.shadow_map_size = size;
  if (m_backend) m_backend->set_shadow_map_size(size);
}

int Renderer::shadow_map_size() const {
  return m_backend ? m_backend->shadow_map_size() : m_lighting.shadow_map_size;
}

void Renderer::set_msaa_samples(int samples) {
  int s = samples;
  if (s < 0) s = 0;
  if (s > 4) s = 4;
  if (s == 1) s = 2;
  if (s == 3) s = 4;
  m_msaa_samples = s;
  if (m_backend) m_backend->set_msaa_samples(s);
}

int Renderer::msaa_samples() const {
  return m_backend ? m_backend->msaa_samples() : m_msaa_samples;
}

bool Renderer::read_rgb_framebuffer(std::vector<std::uint8_t>& out_rgb, int& w,
                                    int& h) {
  return m_backend ? m_backend->read_rgb_framebuffer(out_rgb, w, h) : false;
}

}  // namespace fury
