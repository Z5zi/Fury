#include "fury/renderer.hpp"

#include "fury/log.hpp"
#include "fury/texture.hpp"
#include "fury/material_sampling.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstring>
#include <limits>
#include <vector>
#include <unordered_set>

namespace fury {
namespace {

// Keep attributes in homogeneous space until all six clip planes have been
// applied. Intersections interpolate *un-divided* varyings, just as a GPU does.
struct ClipVertex {
  Vec4 clip;
  Vec3 world, normal, color;
  Vec2 uv;
  float opacity{1.f};
};
struct SoftVert {
  float x, y, z, rhw;
  ClipVertex attributes;
};
using TangentFrame=MaterialTangentFrame;
struct TransparentTriangle {
  SoftVert vertices[3];
  TangentFrame frame;
  Material material;
  float distance;
};

inline float cl01(float v) { return std::clamp(v, 0.f, 1.f); }
inline Vec3 multiply(Vec3 a, Vec3 b) { return {a.x*b.x, a.y*b.y, a.z*b.z}; }
inline float fifth(float x) { const float x2=x*x; return x2*x2*x; }
inline bool finite(Vec3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline bool finite(Vec4 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) &&
         std::isfinite(v.z) && std::isfinite(v.w);
}

inline Vec3 tonemap_gamma(Vec3 c) {
  auto channel=[](float v) {
    v=(std::max)(v,0.f);
    return std::pow(v/(1.f+v),1.f/2.2f);
  };
  return {channel(c.x),channel(c.y),channel(c.z)};
}

// Material colors are sRGB; alpha, normal, and metallic/roughness are linear.
// Decode before bilinear filtering, avoiding dark fringes between bright texels.
Vec4 sample_texture(const std::uint8_t* pixels,int width,int height,int channels,
                    float u,float v,bool srgb) {
  if(!pixels || width<=0 || height<=0 || !std::isfinite(u) || !std::isfinite(v))
    return {1.f,1.f,1.f,1.f};
  static const std::array<float,256> linear=[] {
    std::array<float,256> values{};
    for(int i=0;i<256;++i) {
      const float c=i/255.f;
      values[i]=c<=.04045f ? c/12.92f : std::pow((c+.055f)/1.055f,2.4f);
    }
    return values;
  }();
  const float x=(u-std::floor(u))*width-.5f;
  const float y=(v-std::floor(v))*height-.5f;
  const int ix=static_cast<int>(std::floor(x)), iy=static_cast<int>(std::floor(y));
  const float fx=x-ix, fy=y-iy;
  auto texel=[&](int tx,int ty) {
    tx=(tx+width)%width; ty=(ty+height)%height;
    const auto* p=pixels+(static_cast<std::size_t>(ty)*width+tx)*channels;
    return Vec4{srgb?linear[p[0]]:p[0]/255.f,
                srgb?linear[p[1]]:p[1]/255.f,
                srgb?linear[p[2]]:p[2]/255.f, channels==4?p[3]/255.f:1.f};
  };
  const Vec4 a=texel(ix,iy), b=texel(ix+1,iy),
             c=texel(ix,iy+1), d=texel(ix+1,iy+1);
  auto bilerp=[&](float aa,float bb,float cc,float dd) {
    return (aa*(1-fx)+bb*fx)*(1-fy)+(cc*(1-fx)+dd*fx)*fy;
  };
  return {bilerp(a.x,b.x,c.x,d.x),bilerp(a.y,b.y,c.y,d.y),
          bilerp(a.z,b.z,c.z,d.z),bilerp(a.w,b.w,c.w,d.w)};
}
Vec3 sample_texture(const Image& image,Vec2 uv,bool srgb) {
  if(image.width<=0 || image.height<=0 || image.rgb.size()<
      static_cast<std::size_t>(image.width)*image.height*3) return {1,1,1};
  const auto c=sample_texture(image.rgb.data(),image.width,image.height,3,
                             uv.x,uv.y,srgb);
  return {c.x,c.y,c.z};
}

double clip_distance(const ClipVertex& v,int plane) {
  const double w=v.clip.w;
  switch(plane) {
    case 0: return w+v.clip.x;
    case 1: return w-v.clip.x;
    case 2: return w+v.clip.y;
    case 3: return w-v.clip.y;
    case 4: return w+v.clip.z; // OpenGL projection: -w <= z <= w
    default: return w-v.clip.z;
  }
}
ClipVertex interpolate(const ClipVertex& a,const ClipVertex& b,float t) {
  const float s=1.f-t;
  return {{a.clip.x*s+b.clip.x*t,a.clip.y*s+b.clip.y*t,
           a.clip.z*s+b.clip.z*t,a.clip.w*s+b.clip.w*t},
          a.world*s+b.world*t,a.normal*s+b.normal*t,a.color*s+b.color*t,
          {a.uv.x*s+b.uv.x*t,a.uv.y*s+b.uv.y*t},a.opacity*s+b.opacity*t};
}

// A convex triangle clipped against six planes has at most nine vertices.
int clip_triangle(std::array<ClipVertex,12>& vertices) {
  unsigned outside_any=0,outside_all=63;
  for(int i=0;i<3;++i) {
    unsigned code=0;
    for(int plane=0;plane<6;++plane) if(clip_distance(vertices[i],plane)<0.) code|=1u<<plane;
    outside_any|=code; outside_all&=code;
  }
  if(outside_all) return 0;
  if(!outside_any) return 3;
  int count=3;
  std::array<ClipVertex,12> output{};
  for(int plane=0;plane<6 && count;++plane) {
    if(!(outside_any&(1u<<plane))) continue;
    int written=0;
    ClipVertex previous=vertices[count-1];
    double previous_distance=clip_distance(previous,plane);
    for(int i=0;i<count;++i) {
      const auto& current=vertices[i];
      const double distance=clip_distance(current,plane);
      if((distance>=0.f)!=(previous_distance>=0.f)) {
        const float t=static_cast<float>(previous_distance/(previous_distance-distance));
        output[written++]=interpolate(previous,current,cl01(t));
      }
      if(distance>=0.f) output[written++]=current;
      previous=current; previous_distance=distance;
    }
    count=written;
    std::copy_n(output.begin(),count,vertices.begin());
  }
  return count;
}

class SoftBackend final : public IRenderBackend {
 public:
  ~SoftBackend() override { destroy(); }

  bool create(SDL_Window* window, int width, int height) override {
    destroy();
    m_window = window;
    m_stats={};
    m_stats.adapter="CPU / SDL software rasterizer";
    m_stats.cpu_threads=1;
    m_width = (std::max)(1, width);
    m_height = (std::max)(1, height);

    // No accelerated presentation path: software means CPU-only, including SDL.
    m_sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
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

    m_stats.render_width=m_stats.output_width=static_cast<unsigned>(m_width);
    m_stats.render_height=m_stats.output_height=static_cast<unsigned>(m_height);
    m_color.assign(static_cast<std::size_t>(m_width) * m_height, 0);
    m_linear.assign(m_color.size(), Vec3{});
    m_depth.assign(static_cast<std::size_t>(m_width) * m_height,
                   std::numeric_limits<float>::infinity());
    // Resolve legacy slot textures once; material maps remain shared and immutable.
    m_slot_images.assign(static_cast<std::size_t>(TextureSlot::Count), Image{});
    for (int s = 1; s < static_cast<int>(TextureSlot::Count); ++s) {
      resolve_texture_pixels(static_cast<TextureSlot>(s), 64,
                             m_slot_images[static_cast<std::size_t>(s)]);
    }
    // Legacy slots use the same per-pixel UV tangent frame as imported normal maps.
    m_normal_images.assign(static_cast<std::size_t>(TextureSlot::Count), Image{});
    for (TextureSlot ns : {TextureSlot::Asphalt, TextureSlot::Brick}) {
      resolve_normal_pixels(ns, 64,
                            m_normal_images[static_cast<std::size_t>(ns)]);
    }
    Log::info("Renderer backend: CPU software rasterizer (clipping + per-pixel PBR + textures + alpha + water + HUD)");
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
    m_linear.clear();
    m_transparent.clear();
    m_vertices.clear();
    m_seen_meshes.clear();
    m_depth.clear();
    m_slot_images.clear();
    m_normal_images.clear();
  }

  void begin_frame(const Color& clear) override {
    m_frame_start=std::chrono::steady_clock::now();
    m_stats.triangle_count=m_stats.unique_triangle_count=0;
    m_stats.instance_count=0;
    m_stats.cpu_frame_ms=0;
    m_seen_meshes.clear();
    const std::uint32_t c = (static_cast<std::uint32_t>(clear.a) << 24) |
                            (static_cast<std::uint32_t>(clear.r) << 16) |
                            (static_cast<std::uint32_t>(clear.g) << 8) |
                            static_cast<std::uint32_t>(clear.b);
    std::fill(m_color.begin(), m_color.end(), c);
    // The API clear color is display encoded. Invert our display transform so
    // transparent surfaces composite in scene-linear light over that exact clear.
    auto scene=[](std::uint8_t c) {
      const float linear=std::pow(c/255.f,2.2f);
      return linear/(std::max)(1.f-linear,1e-5f);
    };
    std::fill(m_linear.begin(),m_linear.end(),Vec3{scene(clear.r),scene(clear.g),scene(clear.b)});
    m_transparent.clear();
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
    mesh.gpu_dirty = false;
  }

  void draw_mesh(const Mesh& mesh, const Mat4& model,
                 const Material& material) override {
    const bool unique_mesh=m_seen_meshes.insert(mesh.geometry_identity).second;
    bool counted_instance=false;
    const Mat4 mvp = m_view_proj * model;
    Mat4 inverse_model;
    const bool invertible=inverse(model,inverse_model);
    const Mat4 normal_matrix=invertible?transpose(inverse_model):Mat4::identity();
    const Vec3 axis_x{model.m[0],model.m[1],model.m[2]},axis_y{model.m[4],model.m[5],model.m[6]},axis_z{model.m[8],model.m[9],model.m[10]};
    const bool mirrored=dot(axis_x,cross(axis_y,axis_z))<0.f;
    // Indexed meshes share vertices across triangles. Transform each once per
    // draw instead of repeating three matrix transforms for every index.
    m_vertices.resize(mesh.vertices.size());
    for(std::size_t i=0;i<mesh.vertices.size();++i) {
      const auto& v=mesh.vertices[i];
      m_vertices[i]={mul(mvp,Vec4{v.position,1.f}),transform_point(model,v.position),
                     transform_direction(normal_matrix,v.normal),v.color,v.uv,v.opacity};
    }
    for(std::size_t i=0;i+2<mesh.indices.size();i+=3) {
      std::array<ClipVertex,12> vertices{};
      bool valid=true;
      for(int k=0;k<3;++k) {
        const auto index=mesh.indices[i+static_cast<std::size_t>(k)];
        if(index>=mesh.vertices.size()) { valid=false; break; }
        const auto& v=m_vertices[index];
        vertices[k]=v;
        if(!finite(v.clip) || !finite(v.world) || !finite(v.normal) || !finite(v.color) ||
           !std::isfinite(v.uv.x) || !std::isfinite(v.uv.y) || !std::isfinite(v.opacity)) {
          valid=false; break;
        }
      }
      if(!valid) { ++m_stats.validation_errors; continue; }
      if(mirrored) std::swap(vertices[1],vertices[2]);
      ++m_stats.triangle_count;
      if(unique_mesh) ++m_stats.unique_triangle_count;
      if(!counted_instance) { ++m_stats.instance_count; counted_instance=true; }
      const Vec3 e1=vertices[1].world-vertices[0].world;
      const Vec3 e2=vertices[2].world-vertices[0].world;
      const Vec3 face_normal=normalize(cross(e1,e2));
      const bool world_uv=material.world_uv_scale>0 && std::isfinite(material.world_uv_scale);
      if(world_uv) for(int k=0;k<3;++k)
        vertices[k].uv=world_planar_projection(vertices[k].world,face_normal,material.world_uv_scale).uv;
      const Vec2 duv1{vertices[1].uv.x-vertices[0].uv.x,vertices[1].uv.y-vertices[0].uv.y};
      const Vec2 duv2{vertices[2].uv.x-vertices[0].uv.x,vertices[2].uv.y-vertices[0].uv.y};
      const float determinant=duv1.x*duv2.y-duv1.y*duv2.x;
      TangentFrame frame{};
      if(world_uv) frame=world_planar_projection(vertices[0].world,face_normal,material.world_uv_scale).frame;
      else if(std::fabs(determinant)>1e-10f) {
        frame.tangent=(e1*duv2.y-e2*duv1.y)*(1.f/determinant);
        frame.bitangent=(e2*duv1.x-e1*duv2.x)*(1.f/determinant);
      }
      for(int k=0;k<3;++k)
        if(!invertible || dot(vertices[k].normal,vertices[k].normal)<1e-12f)
          vertices[k].normal=face_normal;
      const int count=clip_triangle(vertices);
      for(int k=1;k+1<count;++k) {
        SoftVert projected[3];
        const ClipVertex triangle[3]={vertices[0],vertices[k],vertices[k+1]};
        valid=true;
        for(int c=0;c<3;++c) {
          const auto& v=triangle[c];
          if(v.clip.w<=1e-7f) { valid=false; break; }
          const float rhw=1.f/v.clip.w;
          projected[c]={(v.clip.x*rhw*.5f+.5f)*m_width,
                        (.5f-v.clip.y*rhw*.5f)*m_height,v.clip.z*rhw,rhw,v};
        }
        if(!valid) continue;
        if(material.alpha_blend) {
          // NDC depth also works with orthographic/custom projections. Triangle
          // sorting is approximate for intersecting translucent geometry.
          m_transparent.push_back({{projected[0],projected[1],projected[2]},frame,material,
                                  (projected[0].z+projected[1].z+projected[2].z)/3.f});
        } else {
          raster_triangle(projected[0],projected[1],projected[2],frame,material);
        }
      }
    }
  }

  void draw_hud_rect(float x, float y, float w, float h,
                     const Color& color) override {
    flush_transparent();
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h)) return;
    const int x0=static_cast<int>(std::clamp(std::floor(double(x)),0.,double(m_width)));
    const int y0=static_cast<int>(std::clamp(std::floor(double(y)),0.,double(m_height)));
    const int x1=static_cast<int>(std::clamp(std::ceil(double(x)+w),0.,double(m_width)));
    const int y1=static_cast<int>(std::clamp(std::ceil(double(y)+h),0.,double(m_height)));
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
    flush_transparent();
    if(!m_texture || !m_sdl_renderer) return;
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
    ++m_stats.frame_index;
    m_stats.cpu_frame_ms=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-m_frame_start).count();
  }

  void resize(int width, int height) override {
    m_transparent.clear();
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
    m_stats.render_width=m_stats.output_width=static_cast<unsigned>(m_width);
    m_stats.render_height=m_stats.output_height=static_cast<unsigned>(m_height);
    m_color.assign(static_cast<std::size_t>(m_width) * m_height, 0);
    m_linear.assign(m_color.size(), Vec3{});
    m_depth.assign(static_cast<std::size_t>(m_width) * m_height,
                   std::numeric_limits<float>::infinity());
  }

  bool read_rgb_framebuffer(std::vector<std::uint8_t>& out_rgb, int& w,
                            int& h) override {
    flush_transparent();
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

  RenderStatistics statistics() const override { return m_stats; }

  RenderBackendKind kind() const override { return RenderBackendKind::Software; }
  const char* name() const override { return "CPU software rasterizer (per-pixel PBR)"; }

 private:
  void flush_transparent() {
    std::stable_sort(m_transparent.begin(),m_transparent.end(),
                    [](const auto& a,const auto& b) { return a.distance>b.distance; });
    for(const auto& triangle:m_transparent)
      raster_triangle(triangle.vertices[0],triangle.vertices[1],triangle.vertices[2],
                      triangle.frame,triangle.material);
    m_transparent.clear();
  }

  Vec3 shade(const ClipVertex& v,const TangentFrame& frame,
             const Material& material,const Vec3& sampled_base,Vec2 uv,TextureFootprint footprint,
             bool front_facing) const {
    Vec3 base=multiply(multiply(v.color,material.albedo),sampled_base);
    Vec3 n=normalize(v.normal);
    const Vec3 view_dir=normalize(m_camera_pos-v.world);
    // Face orientation comes from triangle winding, as with gl_FrontFacing.
    // A front-facing concave surface may have a smooth normal facing away.
    if(material.double_sided && !front_facing) n=-n;
    float roughness=cl01(material.roughness), metallic=cl01(material.metallic);
    const auto* textures=material.textures.get();
    if(textures && textures->metallic_roughness.valid()) {
      const Vec4 mr=sample_material_texture(textures->metallic_roughness,textures->metallic_roughness_mips,uv,false,footprint);
      roughness*=mr.y; metallic*=mr.z;
    }
    roughness=(std::max)(roughness,.045f);
    Vec3 encoded_normal;
    bool normal_mapped=false;
    if(textures && textures->normal.valid()) {
      const Vec4 enc=sample_material_texture(textures->normal,textures->normal_mips,uv,false,footprint);
      encoded_normal={enc.x,enc.y,enc.z}; normal_mapped=true;
    } else if(!textures && texture_slot_has_normal(material.texture)) {
      const auto& image=m_normal_images[static_cast<std::size_t>(material.texture)];
      if(!image.rgb.empty()) { encoded_normal=sample_texture(image,uv,false); normal_mapped=true; }
    }
    if(normal_mapped) {
      const auto basis=orthonormalize_material_frame(n,frame);
      const Vec3 tangent=basis.tangent,bitangent=basis.bitangent;
      const Vec3 map_n{(encoded_normal.x*2.f-1.f)*material.normal_scale,
                       (encoded_normal.y*2.f-1.f)*material.normal_scale,
                       encoded_normal.z*2.f-1.f};
      n=normalize(tangent*map_n.x+bitangent*map_n.y+n*map_n.z);
    }
    if(material.texture==TextureSlot::Water && !textures) {
      const float wx=std::sin(v.world.x*.35f+m_time*1.6f)*std::cos(v.world.z*.28f+m_time*1.15f);
      const float wz=std::sin(v.world.x*.22f-m_time*.95f+v.world.z*.31f);
      n=normalize(Vec3{n.x+wx*.18f,n.y,n.z+wz*.18f});
      const float pulse=.85f+.15f*std::sin(m_time*1.7f+material.uv_scroll_u*3.f);
      base={base.x*.28f*pulse*.55f+.02f,base.y*.72f*pulse*.85f+.08f,
            base.z*1.15f*pulse*1.05f+.14f};
      const float shore=1.f-cl01(std::min({v.uv.x,1.f-v.uv.x,v.uv.y,1.f-v.uv.y})/.085f);
      const float noise=.55f+.45f*std::sin(v.uv.x*40.f+m_time*3.f)*std::cos(v.uv.y*36.f-m_time*2.4f);
      const float foam=cl01(shore*noise)*.82f;
      base=base*(1.f-foam)+Vec3{.78f,.90f,.96f}*foam;
    }
    const float hemi=cl01(n.y*.5f+.5f), cavity=cl01(dot(n,view_dir));
    const float ao=1.f-cl01(m_lighting.ao_strength)*(1.f-(.42f+.58f*hemi)*(.65f+.35f*cavity));
    Vec3 color=multiply(base,m_lighting.ambient)*(ao*(1.f-metallic*.8f));
    const Vec3 f0=Vec3{.04f,.04f,.04f}*(1.f-metallic)+base*metallic;
    auto illuminate=[&](Vec3 light_dir,Vec3 radiance) {
      constexpr float pi=3.14159265358979323846f;
      const float ndotl=cl01(dot(n,light_dir)), ndotv=(std::max)(cl01(dot(n,view_dir)),1e-4f);
      if(ndotl<=0.f) return Vec3{};
      const Vec3 half_vector=normalize(light_dir+view_dir);
      const float ndoth=cl01(dot(n,half_vector)), vdoth=cl01(dot(view_dir,half_vector));
      const float alpha=roughness*roughness, alpha2=alpha*alpha;
      const float denominator=ndoth*ndoth*(alpha2-1.f)+1.f;
      const float distribution=alpha2/(pi*(std::max)(denominator*denominator,1e-9f));
      const float k=(roughness+1.f)*(roughness+1.f)/8.f;
      const float geometry=ndotl/(ndotl*(1.f-k)+k)*ndotv/(ndotv*(1.f-k)+k);
      const Vec3 fresnel=f0+(Vec3{1,1,1}-f0)*fifth(1.f-vdoth);
      const Vec3 specular=fresnel*(distribution*geometry/(4.f*ndotl*ndotv));
      const Vec3 diffuse=multiply(Vec3{1,1,1}-fresnel,base)*((1.f-metallic)/pi);
      return multiply(diffuse+specular,radiance)*(ndotl*ao);
    };
    const Vec3 sun=normalize(-m_lighting.sun_direction);
    color+=illuminate(sun,m_lighting.sun_color*m_lighting.sun_intensity);
    const int count=std::clamp(m_lighting.point_light_count,0,Lighting::kMaxPointLights);
    for(int i=0;i<count;++i) {
      const auto& light=m_lighting.point_lights[i];
      const Vec3 to_light=light.position-v.world;
      const float distance=length(to_light);
      const float attenuation=1.f-cl01(distance/(std::max)(light.radius,.5f));
      if(attenuation>0.f)
        color+=illuminate(to_light*(1.f/(std::max)(distance,.001f)),
                          light.color*(light.intensity*attenuation*attenuation));
    }
    // Preserve the wet-road streak effect while using the same per-pixel inputs.
    if(material.texture==TextureSlot::Asphalt && material.wetness>0.f) {
      const Vec3 h=normalize(sun+view_dir);
      const Vec3 t=normalize(frame.tangent);
      const float th=dot(t,h), wet=cl01(material.wetness);
      const float streak=std::pow(cl01(1.f-th*th),8.f+32.f*wet)*wet*.18f;
      color+=multiply(f0,m_lighting.sun_color)*(streak*m_lighting.sun_intensity);
    }
    Vec3 emission=textures?material.emissive_color*material.emissive:
                          multiply(base,material.emissive_color)*material.emissive;
    if(textures && textures->emissive.valid()) {
      const Vec4 tex=sample_material_texture(textures->emissive,textures->emissive_mips,uv,true,footprint);
      emission=multiply(emission,{tex.x,tex.y,tex.z});
    }
    color+=emission;
    if(m_lighting.enable_bloom) {
      const float pass=(std::max)(std::max({emission.x,emission.y,emission.z})-.55f,0.f);
      color+=emission*(pass*cl01(m_lighting.bloom_strength));
    }
    if(material.texture==TextureSlot::Water && m_lighting.enable_reflections) {
      const float fresnel=(.02f+.98f*fifth(1.f-cl01(dot(n,view_dir))))*
                           cl01(m_lighting.reflection_strength)*.62f;
      color=color*(1.f-fresnel)+Vec3{m_lighting.fog_color.x,m_lighting.fog_color.y,
                                    m_lighting.fog_color.z*.7f+.25f}*fresnel;
    }
    if(m_lighting.fog_end>m_lighting.fog_start) {
      const float fog=cl01((m_lighting.fog_end-length(v.world-m_camera_pos))/
                          (m_lighting.fog_end-m_lighting.fog_start));
      color=color*fog+m_lighting.fog_color*(1.f-fog);
    }
    return color;
  }

  void raster_triangle(SoftVert v0,SoftVert v1,SoftVert v2,
                       const TangentFrame& frame,const Material& material) {
    auto edge=[](const SoftVert& a,const SoftVert& b,float x,float y) {
      // Canonical line coefficients make a reversed shared edge exactly the
      // negative of its neighbor, avoiding tiny gaps from subtracting different
      // vertex origins before multiplication.
      // Double intermediates retain small triangles at large viewport offsets.
      return float(double(x)*(double(b.y)-a.y)+double(y)*(double(a.x)-b.x)+
                   (double(a.y)*b.x-double(a.x)*b.y));
    };
    float area=edge(v0,v1,v2.x,v2.y);
    if(!std::isfinite(area) || std::fabs(area)<1e-8f) return;
    // The downward screen Y axis and edge convention make CCW fronts positive.
    // Preserve facing before normalizing coverage winding; mirrored models have
    // already had their authored winding restored before homogeneous clipping.
    const bool front_facing=area>0.f;
    if(!material.double_sided && !front_facing) return;
    if(!front_facing) { std::swap(v1,v2); area=-area; }
    const int x0=(std::max)(0,static_cast<int>(std::ceil(std::min({v0.x,v1.x,v2.x})-.5f)));
    const int y0=(std::max)(0,static_cast<int>(std::ceil(std::min({v0.y,v1.y,v2.y})-.5f)));
    const int x1=(std::min)(m_width-1,static_cast<int>(std::floor(std::max({v0.x,v1.x,v2.x})-.5f)));
    const int y1=(std::min)(m_height-1,static_cast<int>(std::floor(std::max({v0.y,v1.y,v2.y})-.5f)));
    // Half-open coverage: a shared edge belongs to exactly one triangle. This
    // matters for transparent quads, where double shading otherwise leaves seams.
    auto top_left=[](const SoftVert& a,const SoftVert& b) {
      const float dy=b.y-a.y, dx=b.x-a.x;
      return dy>0.f || (dy==0.f && dx<0.f);
    };
    const bool inclusive0=top_left(v1,v2),inclusive1=top_left(v2,v0),inclusive2=top_left(v0,v1);
    const float inv_area=1.f/area;
    // Barycentric gradients are constant on each clipped triangle. Differentiate
    // UV/w and 1/w separately, then apply the quotient rule at every fragment.
    const float wx[3]={(v2.y-v1.y)*inv_area,(v0.y-v2.y)*inv_area,(v1.y-v0.y)*inv_area};
    const float wy[3]={(v1.x-v2.x)*inv_area,(v2.x-v0.x)*inv_area,(v0.x-v1.x)*inv_area};
    const SoftVert* vertices[3]={&v0,&v1,&v2};
    Vec2 numerator_dx{},numerator_dy{}; float inverse_w_dx=0,inverse_w_dy=0;
    for(int i=0;i<3;++i) {
      const auto& v=*vertices[i];
      inverse_w_dx+=wx[i]*v.rhw; inverse_w_dy+=wy[i]*v.rhw;
      numerator_dx.x+=wx[i]*v.rhw*v.attributes.uv.x;
      numerator_dx.y+=wx[i]*v.rhw*v.attributes.uv.y;
      numerator_dy.x+=wy[i]*v.rhw*v.attributes.uv.x;
      numerator_dy.y+=wy[i]*v.rhw*v.attributes.uv.y;
    }
    for(int y=y0;y<=y1;++y) {
      for(int x=x0;x<=x1;++x) {
        const float px=x+.5f,py=y+.5f;
        const float e0=edge(v1,v2,px,py),e1=edge(v2,v0,px,py),e2=edge(v0,v1,px,py);
        if(e0<0.f || (e0==0.f && !inclusive0) || e1<0.f || (e1==0.f && !inclusive1) ||
           e2<0.f || (e2==0.f && !inclusive2)) continue;
        const float w0=e0*inv_area,w1=e1*inv_area,w2=e2*inv_area;
        // NDC depth is affine in screen space; do NOT divide this by interpolated 1/w.
        const float z=w0*v0.z+w1*v1.z+w2*v2.z;
        const std::size_t index=static_cast<std::size_t>(y)*m_width+x;
        if(!std::isfinite(z) || z < -1.00001f || z > 1.00001f || z>=m_depth[index]) continue;
        const float rhw=w0*v0.rhw+w1*v1.rhw+w2*v2.rhw;
        if(rhw<=0.f || !std::isfinite(rhw)) continue;
        const float a=w0*v0.rhw/rhw,b=w1*v1.rhw/rhw,c=w2*v2.rhw/rhw;
        const auto& p0=v0.attributes; const auto& p1=v1.attributes; const auto& p2=v2.attributes;
        ClipVertex pixel{};
        pixel.world=p0.world*a+p1.world*b+p2.world*c;
        pixel.normal=p0.normal*a+p1.normal*b+p2.normal*c;
        pixel.color=p0.color*a+p1.color*b+p2.color*c;
        pixel.uv={p0.uv.x*a+p1.uv.x*b+p2.uv.x*c,p0.uv.y*a+p1.uv.y*b+p2.uv.y*c};
        pixel.opacity=p0.opacity*a+p1.opacity*b+p2.opacity*c;
        const TextureFootprint footprint=perspective_texture_footprint(pixel.uv,numerator_dx,numerator_dy,
            rhw,inverse_w_dx,inverse_w_dy);
        const Vec2 uv{pixel.uv.x+m_time*material.uv_scroll_u,pixel.uv.y+m_time*material.uv_scroll_v};
        Vec4 texture{1,1,1,1};
        if(material.textures) {
          texture=sample_material_texture(material.textures->base_color,material.textures->base_color_mips,uv,true,footprint);
        } else if(material.texture!=TextureSlot::Water) {
          const int slot=static_cast<int>(material.texture);
          if(slot>0 && slot<static_cast<int>(TextureSlot::Count)) {
            const Vec3 sampled=sample_texture(m_slot_images[static_cast<std::size_t>(slot)],uv,true);
            texture={sampled.x,sampled.y,sampled.z,1};
          }
        }
        texture=apply_material_color_detail(material.color_detail.get(),texture,uv,footprint);
        const float alpha=cl01(pixel.opacity*material.opacity*texture.w);
        if(!std::isfinite(alpha)) continue;
        if(material.alpha_cutoff>=0.f && alpha<material.alpha_cutoff) continue;
        if(material.alpha_blend && alpha<=0.f) continue;
        Vec3 color=shade(pixel,frame,material,{texture.x,texture.y,texture.z},uv,footprint,front_facing);
        if(!finite(color)) continue;
        if(material.alpha_blend) color=color*alpha+m_linear[index]*(1.f-alpha);
        else m_depth[index]=z; // Alpha masks discard before writing depth; blends never write it.
        m_linear[index]=color;
        color=tonemap_gamma(color);
        const auto r=static_cast<std::uint32_t>(cl01(color.x)*255.f+.5f);
        const auto g=static_cast<std::uint32_t>(cl01(color.y)*255.f+.5f);
        const auto b8=static_cast<std::uint32_t>(cl01(color.z)*255.f+.5f);
        m_color[index]=(255u<<24)|(r<<16)|(g<<8)|b8;
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
  std::vector<Vec3> m_linear;
  std::vector<TransparentTriangle> m_transparent;
  std::vector<ClipVertex> m_vertices;
  std::unordered_set<std::uint64_t> m_seen_meshes;
  RenderStatistics m_stats;
  std::chrono::steady_clock::time_point m_frame_start;
  std::vector<float> m_depth;
  std::vector<Image> m_slot_images;
  std::vector<Image> m_normal_images;
};

}  // namespace

std::unique_ptr<IRenderBackend> create_software_backend() {
  return std::make_unique<SoftBackend>();
}

}  // namespace fury
