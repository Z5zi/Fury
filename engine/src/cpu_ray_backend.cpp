// Portable, deterministic CPU ray/path tracing. Geometry BVHs are shared by
// instances; only the small top-level hierarchy changes when objects move.
#include "fury/renderer.hpp"
#include "fury/log.hpp"
#include "fury/texture.hpp"
#include "fury/material_sampling.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace fury {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr float epsilon=1e-4f;
constexpr float infinity=std::numeric_limits<float>::infinity();
float component(Vec3 v,unsigned axis) { return axis==0 ? v.x : axis==1 ? v.y : v.z; }
Vec3 product(Vec3 a,Vec3 b) { return {a.x*b.x,a.y*b.y,a.z*b.z}; }
Vec3 mix(Vec3 a,Vec3 b,float t) { return a*(1-t)+b*t; }
float maximum(Vec3 v) { return std::max({v.x,v.y,v.z}); }
bool finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
float saturate(float x) { return std::clamp(x,0.f,1.f); }
Vec3 reflect(Vec3 incident,Vec3 normal) { return incident-normal*(2*dot(incident,normal)); }
std::uint64_t hash_bytes(std::uint64_t h,const void* bytes,std::size_t size) {
  const auto* p=static_cast<const unsigned char*>(bytes);
  for(std::size_t i=0;i<size;++i) { h^=p[i]; h*=1099511628211ull; } return h;
}
template<class T> void hash_value(std::uint64_t& h,const T& value) { h=hash_bytes(h,&value,sizeof(value)); }
struct Random {
  std::uint32_t state;
  float next() {
    state=state*747796405u+2891336453u;
    const auto word=((state>>((state>>28u)+4u))^state)*277803737u;
    return float((word>>22u)^word)*0x1p-32f;
  }
};
// Cone width is a full diameter at the origin; spread is diameter growth per
// world-space unit. Intersection distances stay unchanged by this metadata.
struct Ray { Vec3 origin,direction; float width{},spread{}; };
struct Bounds {
  Vec3 lo{infinity,infinity,infinity},hi{-infinity,-infinity,-infinity};
  void add(Vec3 p) {
    lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
    hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
  }
  void add(const Bounds& b) { add(b.lo); add(b.hi); }
  Vec3 center() const { return (lo+hi)*.5f; }
  bool hit(const Ray& ray,float limit,float& entry) const {
    float start=epsilon,end=limit;
    for(unsigned axis=0;axis<3;++axis) {
      const float origin=component(ray.origin,axis),dir=component(ray.direction,axis);
      const float a=component(lo,axis),b=component(hi,axis);
      if(std::fabs(dir)<1e-20f) { if(origin<a || origin>b) return false; }
      else {
        float near=(a-origin)/dir,far=(b-origin)/dir;
        if(near>far) std::swap(near,far);
        start=std::max(start,near); end=std::min(end,far);
        if(end<start) return false;
      }
    }
    entry=start; return true;
  }
};
struct Node { Bounds box; unsigned first{},count{},left{},right{}; };
struct Bvh {
  std::vector<Node> nodes;
  std::vector<unsigned> order;
  void build(const std::vector<Bounds>& boxes) {
    nodes.clear(); order.resize(boxes.size()); std::iota(order.begin(),order.end(),0u);
    if(!boxes.empty()) split(boxes,0,unsigned(boxes.size()));
  }
  unsigned split(const std::vector<Bounds>& boxes,unsigned begin,unsigned end) {
    const unsigned index=unsigned(nodes.size()); nodes.emplace_back();
    Bounds bounds,centers;
    for(unsigned i=begin;i<end;++i) { bounds.add(boxes[order[i]]); centers.add(boxes[order[i]].center()); }
    nodes[index].box=bounds;
    if(end-begin<=4) { nodes[index].first=begin; nodes[index].count=end-begin; return index; }
    const Vec3 span=centers.hi-centers.lo;
    const unsigned axis=span.x>span.y ? (span.x>span.z ? 0:2) : (span.y>span.z ? 1:2);
    const unsigned mid=begin+(end-begin)/2;
    std::nth_element(order.begin()+begin,order.begin()+mid,order.begin()+end,[&](unsigned a,unsigned b) {
      const float ac=component(boxes[a].center(),axis),bc=component(boxes[b].center(),axis);
      return ac==bc ? a<b : ac<bc;
    });
    const auto left=split(boxes,begin,mid),right=split(boxes,mid,end);
    nodes[index].left=left; nodes[index].right=right; return index;
  }
  template<class Visit> void visit(const Ray& ray,float& limit,Visit&& visitor) const {
    if(nodes.empty()) return;
    // Median splits cap depth to <= 32 for a 32-bit primitive count.
    std::array<unsigned,64> stack{}; unsigned size=1;
    while(size) {
      const auto& node=nodes[stack[--size]]; float entry{};
      if(!node.box.hit(ray,limit,entry)) continue;
      if(node.count) { for(unsigned i=0;i<node.count;++i) visitor(order[node.first+i],limit); }
      else {
        float a{},b{};
        const bool ah=nodes[node.left].box.hit(ray,limit,a),bh=nodes[node.right].box.hit(ray,limit,b);
        if(ah&&bh) { stack[size++]=a<b ? node.right:node.left; stack[size++]=a<b ? node.left:node.right; }
        else if(ah) stack[size++]=node.left;
        else if(bh) stack[size++]=node.right;
      }
    }
  }
};
struct Triangle { Vertex v[3]; Vec3 edge1,edge2; };
struct Geometry {
  std::uint64_t revision{},last_used{};
  std::size_t vertex_count{},index_count{};
  std::vector<Triangle> triangles;
  Bvh bvh;
};
struct Instance {
  Geometry* geometry{};
  Mat4 model,inverse_model,normal_matrix;
  Material material;
  Bounds box;
};
struct Hit { float t{infinity},u{},v{}; unsigned instance{},triangle{}; bool found{false}; };
struct Surface {
  Vec3 point,normal,geometric,base,emission;
  float roughness{},metallic{},transmission{},ior{},alpha{1};
  bool front{true};
};
struct Sample { Vec3 radiance,normal; float depth{infinity}; };
struct MaterialCoordinates {
  Vec2 uv;
  MaterialTangentFrame frame;
  TextureFootprint footprint;
  bool valid_frame{};
};
MaterialCoordinates coordinates(const Instance& instance,const Triangle& t,
    float u,float v,Vec3 point,const Ray& ray,float distance,float time) {
  const auto& m=instance.material;
  const Vec3 e1=transform_direction(instance.model,t.edge1),e2=transform_direction(instance.model,t.edge2);
  const bool world_uv=m.world_uv_scale>0 && std::isfinite(m.world_uv_scale);
  Vec2 duv1{t.v[1].uv.x-t.v[0].uv.x,t.v[1].uv.y-t.v[0].uv.y};
  Vec2 duv2{t.v[2].uv.x-t.v[0].uv.x,t.v[2].uv.y-t.v[0].uv.y};
  MaterialCoordinates result;
  if(world_uv) {
    const Vec3 geometric=cross(e1,e2);
    const auto projection=world_planar_projection(point,geometric,m.world_uv_scale);
    result.uv=projection.uv; result.frame=projection.frame; result.valid_frame=true;
    // Projection is affine: project edge vectors directly to avoid cancellation
    // when instances are far from the origin.
    duv1=world_planar_projection(e1,geometric,m.world_uv_scale).uv;
    duv2=world_planar_projection(e2,geometric,m.world_uv_scale).uv;
  } else {
    const float w=1-u-v;
    result.uv={t.v[0].uv.x*w+t.v[1].uv.x*u+t.v[2].uv.x*v,
               t.v[0].uv.y*w+t.v[1].uv.y*u+t.v[2].uv.y*v};
    const float det=duv1.x*duv2.y-duv2.x*duv1.y;
    if(std::fabs(det)>1e-8f) {
      result.frame={(e1*duv2.y-e2*duv1.y)*(1/det),(e2*duv1.x-e1*duv2.x)*(1/det)};
      result.valid_frame=true;
    }
  }
  result.uv.x+=m.uv_scroll_u*time; result.uv.y+=m.uv_scroll_v*time;
  const bool mips=(m.textures && (!m.textures->base_color_mips.empty() || !m.textures->normal_mips.empty() ||
      !m.textures->metallic_roughness_mips.empty() || !m.textures->emissive_mips.empty())) ||
      (m.color_detail && !m.color_detail->color_mips.empty());
  if(mips) result.footprint=ray_cone_texture_footprint(e1,e2,duv1,duv2,ray.direction,
      ray.width+std::max(0.f,distance)*ray.spread);
  return result;
}
Vec3 cosine_direction(Vec3 normal,Random& rng) {
  const float r=std::sqrt(rng.next()),angle=2*pi*rng.next();
  const Vec3 tangent=normalize(cross(std::fabs(normal.y)<.99f ? Vec3{0,1,0}:Vec3{1,0,0},normal));
  return normalize(tangent*(r*std::cos(angle))+cross(normal,tangent)*(r*std::sin(angle))+normal*std::sqrt(std::max(0.f,1-r*r)));
}
Vec3 fresnel(Vec3 f0,float cosine) { return f0+(Vec3{1,1,1}-f0)*std::pow(1-saturate(cosine),5.f); }
float ggx_d(float ndoth,float roughness) {
  const float alpha=roughness*roughness,a2=alpha*alpha,d=ndoth*ndoth*(a2-1)+1;
  return a2/(pi*d*d);
}
float ggx_g1(float ndot,float roughness) {
  const float a=roughness*roughness;
  return 2*ndot/(ndot+std::sqrt(a*a+(1-a*a)*ndot*ndot)+1e-7f);
}
Vec3 surface_reflectance(const Surface& s) {
  const float ratio=(s.ior-1.f)/(s.ior+1.f),dielectric=ratio*ratio;
  return mix({dielectric,dielectric,dielectric},s.base,s.metallic);
}
Vec3 brdf(const Surface& s,Vec3 view,Vec3 light) {
  const float nv=std::max(0.f,dot(s.normal,view)),nl=std::max(0.f,dot(s.normal,light));
  if(nv<=0||nl<=0) return {};
  const Vec3 h=normalize(view+light),f=fresnel(surface_reflectance(s),dot(view,h));
  const float d=ggx_d(std::max(0.f,dot(s.normal,h)),s.roughness),g=ggx_g1(nv,s.roughness)*ggx_g1(nl,s.roughness);
  return (product(Vec3{1,1,1}-f,s.base)*((1-s.metallic)/pi)+f*(d*g/(4*nv*nl+1e-7f)))*(1-s.transmission);
}

class CpuRayBackend final : public IRenderBackend {
 public:
  ~CpuRayBackend() override { destroy(); }
  bool create(SDL_Window* window,int width,int height) override;
  void destroy() override { m_window=nullptr; m_cache.clear(); m_instances.clear(); m_tlas={}; m_tlas_hash=0; m_color.clear(); m_sum.clear(); }
  void begin_frame(const Color& clear) override {
    m_clear=clear; m_instances.clear(); m_rendered=false; m_frame_hash=14695981039346656037ull;
    m_stats.triangle_count=0; m_stats.instance_count=0; m_stats.rays_traced=0;
  }
  void set_view_proj(const Mat4& view,const Mat4& projection) override { m_vp=projection*view; }
  void set_camera_position(const Vec3& pos) override { m_camera=pos; }
  void set_lighting(const Lighting& lighting) override { m_lighting=lighting; }
  void set_time(float seconds) override { m_time=seconds; }
  void set_object_id(std::uint64_t id) override { m_object=id; }
  void draw_mesh(const Mesh& mesh,const Mat4& model,const Material& material) override;
  void upload_mesh(Mesh& mesh) override { mesh.gpu_dirty=false; }
  void draw_hud_rect(float x,float y,float width,float height,const Color& color) override;
  void end_frame() override;
  void resize(int width,int height) override;
  RenderBackendKind kind() const override { return RenderBackendKind::CpuRayTracing; }
  const char* name() const override { return "CPU BVH ray/path tracer"; }
  bool configure(const RenderSettings& settings) override;
  RenderSettings settings() const override { return m_settings; }
  RenderStatistics statistics() const override { return m_stats; }
  void reset_history() override { m_accumulated=0; m_previous_hash=0; }
  bool read_rgb_framebuffer(std::vector<std::uint8_t>& rgb,int& width,int& height) override;
 private:
  Geometry& geometry(const Mesh& mesh);
  Hit intersect(const Ray& ray,float limit=infinity) const;
  Surface surface(const Hit& hit,const Ray& ray) const;
  float opacity(const Instance& instance,const Triangle& triangle,float u,float v,
                const Ray& ray,float distance) const;
  Vec3 environment(Vec3 direction) const;
  Sample trace(Ray ray,Random& rng,std::uint64_t& rays,float primary_limit) const;
  void render();
  SDL_Window* m_window{};
  int m_width{},m_height{};
  unsigned m_threads{1},m_accumulated{};
  std::uint64_t m_object{},m_frame_hash{},m_previous_hash{},m_tlas_hash{};
  bool m_rendered{};
  float m_time{};
  Vec3 m_camera;
  Mat4 m_vp=Mat4::identity();
  Lighting m_lighting;
  Color m_clear;
  RenderSettings m_settings;
  RenderStatistics m_stats;
  std::unordered_map<std::uint64_t,std::unique_ptr<Geometry>> m_cache;
  std::vector<Instance> m_instances;
  Bvh m_tlas;
  std::array<RgbaImage,std::size_t(TextureSlot::Count)> m_slots,m_normals;
  std::vector<std::uint32_t> m_color;
  std::vector<Vec3> m_sum,m_frame,m_normal;
  std::vector<float> m_depth;
};

bool CpuRayBackend::create(SDL_Window* window,int width,int height) {
  destroy(); if(!window) return false; m_window=window;
  if(!SDL_GetWindowSurface(window)) { Log::error(std::string("CPU presentation: ")+SDL_GetError()); destroy(); return false; }
  m_threads=std::min(4u,std::max(1u,std::thread::hardware_concurrency()));
  if(const char* requested=std::getenv("FURY_CPU_THREADS")) {
    char* end{}; const long n=std::strtol(requested,&end,10);
    if(end==requested || *end || n<1 || n>64) { Log::error("FURY_CPU_THREADS must be 1..64"); destroy(); return false; }
    m_threads=unsigned(n);
  }
  if(const char* mode=std::getenv("FURY_TRACE_MODE")) {
    if(std::strcmp(mode,"ray")==0) m_settings.trace_mode=TraceMode::RayTraced;
    else if(std::strcmp(mode,"path")!=0) { Log::error("FURY_TRACE_MODE must be ray or path"); destroy(); return false; }
  }
  auto rgba=[](const Image& image) {
    RgbaImage out; out.width=image.width; out.height=image.height; out.pixels.reserve(image.rgb.size()/3*4);
    for(std::size_t i=0;i+2<image.rgb.size();i+=3) { out.pixels.insert(out.pixels.end(),{image.rgb[i],image.rgb[i+1],image.rgb[i+2],255}); }
    return out;
  };
  for(unsigned i=1;i<unsigned(TextureSlot::Count);++i) {
    Image image; resolve_texture_pixels(TextureSlot(i),64,image); m_slots[i]=rgba(image);
    if(texture_slot_has_normal(TextureSlot(i))) { resolve_normal_pixels(TextureSlot(i),64,image); m_normals[i]=rgba(image); }
  }
  m_stats={}; m_stats.software_ray_tracing=true; m_stats.cpu_threads=m_threads;
  m_stats.adapter="Portable CPU / "+std::to_string(m_threads)+" workers";
  resize(width,height);
  Log::info("Renderer backend: CPU BVH ray/path tracer; software surface presentation; "+std::to_string(m_threads)+" workers");
  return true;
}
void CpuRayBackend::resize(int width,int height) {
  width=std::clamp(width,1,7680); height=std::clamp(height,1,4320);
  if(width==m_width&&height==m_height&&!m_color.empty()) return;
  m_width=width; m_height=height; const std::size_t count=std::size_t(width)*height;
  m_color.assign(count,0); m_sum.assign(count,{}); m_frame.assign(count,{}); m_normal.assign(count,{}); m_depth.assign(count,infinity);
  m_stats.render_width=m_stats.output_width=unsigned(width); m_stats.render_height=m_stats.output_height=unsigned(height);
  reset_history(); m_rendered=false;
}
bool CpuRayBackend::configure(const RenderSettings& settings) {
  if(settings.upscaler!=Upscaler::Native || settings.samples_per_pixel<1 || settings.samples_per_pixel>64 ||
     settings.max_bounces<1 || settings.max_bounces>16 || !std::isfinite(settings.exposure) || settings.exposure<=0 || settings.exposure>64 ||
     settings.debug_view==RenderDebugView::Motion || settings.debug_layer) return false;
  const bool changed=settings.trace_mode!=m_settings.trace_mode || settings.samples_per_pixel!=m_settings.samples_per_pixel ||
      settings.max_bounces!=m_settings.max_bounces || settings.exposure!=m_settings.exposure || settings.denoise!=m_settings.denoise ||
      settings.accumulate!=m_settings.accumulate || settings.debug_view!=m_settings.debug_view;
  m_settings=settings; if(changed) reset_history(); return true;
}
Geometry& CpuRayBackend::geometry(const Mesh& mesh) {
  auto& item=m_cache[mesh.geometry_identity]; if(!item) item=std::make_unique<Geometry>();
  auto& g=*item; g.last_used=m_stats.frame_index;
  if(g.revision==mesh.geometry_revision && g.vertex_count==mesh.vertices.size() && g.index_count==mesh.indices.size() && !g.bvh.nodes.empty()) return g;
  g.revision=mesh.geometry_revision; g.vertex_count=mesh.vertices.size(); g.index_count=mesh.indices.size(); g.triangles.clear();
  std::vector<Bounds> boxes; boxes.reserve(mesh.indices.size()/3);
  for(std::size_t i=0;i+2<mesh.indices.size();i+=3) {
    Triangle t; bool valid=true;
    for(unsigned k=0;k<3;++k) {
      if(mesh.indices[i+k]>=mesh.vertices.size()) { valid=false; break; }
      t.v[k]=mesh.vertices[mesh.indices[i+k]]; if(!finite(t.v[k].position)) valid=false;
    }
    if(!valid) { ++m_stats.validation_errors; continue; }
    t.edge1=t.v[1].position-t.v[0].position; t.edge2=t.v[2].position-t.v[0].position;
    if(length(cross(t.edge1,t.edge2))<1e-12f) continue;
    Bounds box; for(const auto& v:t.v) box.add(v.position); boxes.push_back(box); g.triangles.push_back(t);
  }
  g.bvh.build(boxes); ++m_stats.blas_builds; return g;
}
void CpuRayBackend::draw_mesh(const Mesh& mesh,const Mat4& model,const Material& material) {
  if(m_rendered) { ++m_stats.validation_errors; return; }
  Instance instance; if(!inverse(model,instance.inverse_model)) { ++m_stats.validation_errors; return; }
  instance.model=model; instance.normal_matrix=transpose(instance.inverse_model); instance.material=material;
  instance.geometry=&geometry(mesh); if(instance.geometry->bvh.nodes.empty()) return;
  const Bounds local=instance.geometry->bvh.nodes[0].box;
  for(unsigned i=0;i<8;++i) instance.box.add(transform_point(model,{i&1 ? local.hi.x:local.lo.x,i&2 ? local.hi.y:local.lo.y,i&4 ? local.hi.z:local.lo.z}));
  m_stats.triangle_count+=instance.geometry->triangles.size(); ++m_stats.instance_count;
  hash_value(m_frame_hash,mesh.geometry_identity); hash_value(m_frame_hash,mesh.geometry_revision); hash_value(m_frame_hash,model); hash_value(m_frame_hash,m_object);
  // Hash semantic fields, never struct padding or shared_ptr bookkeeping.
  hash_value(m_frame_hash,material.albedo); hash_value(m_frame_hash,material.metallic); hash_value(m_frame_hash,material.roughness);
  hash_value(m_frame_hash,material.emissive); hash_value(m_frame_hash,material.emissive_color); hash_value(m_frame_hash,material.opacity);
  hash_value(m_frame_hash,material.transmission); hash_value(m_frame_hash,material.index_of_refraction); hash_value(m_frame_hash,material.alpha_cutoff);
  hash_value(m_frame_hash,material.normal_scale); hash_value(m_frame_hash,material.double_sided); hash_value(m_frame_hash,material.alpha_blend);
  hash_value(m_frame_hash,material.texture); hash_value(m_frame_hash,material.wetness);
  hash_value(m_frame_hash,material.detail_texture); hash_value(m_frame_hash,material.world_uv_scale);
  hash_value(m_frame_hash,material.uv_scroll_u); hash_value(m_frame_hash,material.uv_scroll_v);
  const auto* maps=material.textures.get(); hash_value(m_frame_hash,maps);
  // Keep the absent-override hash (and deterministic path noise) unchanged.
  if(material.color_detail) { const auto* detail=material.color_detail.get(); hash_value(m_frame_hash,detail); }
  if(material.texture==TextureSlot::Water || material.uv_scroll_u!=0 || material.uv_scroll_v!=0) hash_value(m_frame_hash,m_time);
  m_instances.push_back(std::move(instance));
}
float CpuRayBackend::opacity(const Instance& instance,const Triangle& t,float u,float v,
                             const Ray& ray,float distance) const {
  const auto& m=instance.material; const float w=1-u-v;
  float alpha=m.opacity*(t.v[0].opacity*w+t.v[1].opacity*u+t.v[2].opacity*v);
  if((m.textures&&m.textures->base_color.valid()) || m.color_detail) {
    const auto c=coordinates(instance,t,u,v,ray.origin+ray.direction*distance,ray,distance,m_time);
    const auto base=m.textures ? sample_material_texture(m.textures->base_color,
        m.textures->base_color_mips,c.uv,false,c.footprint):Vec4{1,1,1,1};
    alpha*=apply_material_color_detail(m.color_detail.get(),base,c.uv,c.footprint,false).w;
  }
  return saturate(alpha);
}
Hit CpuRayBackend::intersect(const Ray& ray,float limit) const {
  Hit closest; closest.t=limit;
  m_tlas.visit(ray,closest.t,[&](unsigned index,float& world_limit) {
    const auto& instance=m_instances[index];
    const Ray local{transform_point(instance.inverse_model,ray.origin),transform_direction(instance.inverse_model,ray.direction)};
    instance.geometry->bvh.visit(local,world_limit,[&](unsigned triangle,float& triangle_limit) {
      const auto& t=instance.geometry->triangles[triangle];
      const Vec3 p=cross(local.direction,t.edge2); const float determinant=dot(t.edge1,p);
      if(std::fabs(determinant)<1e-12f) return;
      // Inverse-transpose preserves authored facing when a mirrored node reverses winding.
      if(!instance.material.double_sided && instance.material.transmission<=0 && dot(transform_direction(instance.normal_matrix,cross(t.edge1,t.edge2)),ray.direction)>=0) return;
      const float inverse_d=1/determinant; const Vec3 d=local.origin-t.v[0].position;
      const float u=dot(d,p)*inverse_d; if(u<0||u>1) return;
      const Vec3 q=cross(d,t.edge1); const float v=dot(local.direction,q)*inverse_d; if(v<0||u+v>1) return;
      const float distance=dot(t.edge2,q)*inverse_d; if(distance<epsilon||distance>=triangle_limit) return;
      if(instance.material.alpha_cutoff>=0 && opacity(instance,t,u,v,ray,distance)<instance.material.alpha_cutoff) return;
      closest={distance,u,v,index,triangle,true}; triangle_limit=distance;
    });
  });
  return closest;
}
Surface CpuRayBackend::surface(const Hit& hit,const Ray& ray) const {
  const auto& instance=m_instances[hit.instance]; const auto& m=instance.material; const auto& t=instance.geometry->triangles[hit.triangle];
  const float w=1-hit.u-hit.v;
  const auto interpolate=[&](Vec3 a,Vec3 b,Vec3 c) { return a*w+b*hit.u+c*hit.v; };
  Surface s; s.point=ray.origin+ray.direction*hit.t;
  s.geometric=normalize(transform_direction(instance.normal_matrix,cross(t.edge1,t.edge2)));
  s.front=dot(s.geometric,ray.direction)<0;
  if(!s.front) s.geometric=-s.geometric;
  s.normal=normalize(transform_direction(instance.normal_matrix,interpolate(t.v[0].normal,t.v[1].normal,t.v[2].normal)));
  if(dot(s.normal,s.geometric)<0) s.normal=-s.normal;
  if(length(s.normal)<.5f) s.normal=s.geometric;
  s.base=product(m.albedo,interpolate(t.v[0].color,t.v[1].color,t.v[2].color));
  s.metallic=saturate(m.metallic); s.roughness=std::clamp(m.roughness,.045f,1.f);
  s.transmission=saturate(m.transmission); s.ior=std::clamp(m.index_of_refraction,1.0001f,3.f);
  s.alpha=m.alpha_blend ? opacity(instance,t,hit.u,hit.v,ray,hit.t):1.f;
  s.emission=m.emissive_color*std::max(0.f,m.emissive);
  if(!m.textures) s.emission=product(s.emission,s.base);
  const auto c=coordinates(instance,t,hit.u,hit.v,s.point,ray,hit.t,m_time);
  const RgbaImage* normal_map=nullptr;
  const std::vector<RgbaImage>* normal_mips=nullptr;
  static const std::vector<RgbaImage> no_mips;
  if(m.textures) {
    const auto& maps=*m.textures;
    const auto base=apply_material_color_detail(m.color_detail.get(),
        sample_material_texture(maps.base_color,maps.base_color_mips,c.uv,true,c.footprint),c.uv,c.footprint);
    s.base=product(s.base,{base.x,base.y,base.z});
    const auto mr=sample_material_texture(maps.metallic_roughness,maps.metallic_roughness_mips,c.uv,false,c.footprint);
    s.metallic*=mr.z; s.roughness=std::clamp(s.roughness*mr.y,.045f,1.f);
    const auto emission=sample_material_texture(maps.emissive,maps.emissive_mips,c.uv,true,c.footprint);
    s.emission=product(s.emission,{emission.x,emission.y,emission.z});
    if(maps.normal.valid()) { normal_map=&maps.normal; normal_mips=&maps.normal_mips; }
  } else if(unsigned(m.texture)<m_slots.size() && m.texture!=TextureSlot::None) {
    const auto base=apply_material_color_detail(m.color_detail.get(),
        sample_material_texture(m_slots[unsigned(m.texture)],no_mips,c.uv,true),c.uv,c.footprint);
    s.base=product(s.base,{base.x,base.y,base.z});
    if(m_normals[unsigned(m.texture)].valid()) { normal_map=&m_normals[unsigned(m.texture)]; normal_mips=&no_mips; }
  } else if(m.color_detail) {
    const auto base=apply_material_color_detail(m.color_detail.get(),{1,1,1,1},c.uv,c.footprint);
    s.base=product(s.base,{base.x,base.y,base.z});
  }
  if(normal_map && c.valid_frame) {
    const auto map=sample_material_texture(*normal_map,*normal_mips,c.uv,false,c.footprint);
    const auto basis=orthonormalize_material_frame(s.normal,c.frame);
    s.normal=normalize(basis.tangent*((map.x*2-1)*m.normal_scale)+
        basis.bitangent*((map.y*2-1)*m.normal_scale)+s.normal*(map.z*2-1));
    if(dot(s.normal,s.geometric)<0) s.normal=-s.normal;
  }
  if(m.texture==TextureSlot::Water && !(m.textures && m.textures->normal.valid())) {
    s.normal=normalize(s.normal+Vec3{.12f*std::sin(s.point.x*.8f+m_time),0,.09f*std::cos(s.point.z*.7f+m_time)});
  }
  if(m.wetness>0) { s.roughness=std::max(.045f,s.roughness*(1-.75f*saturate(m.wetness))); s.base=s.base*(1-.2f*saturate(m.wetness)); }
  s.base={saturate(s.base.x),saturate(s.base.y),saturate(s.base.z)};
  return s;
}
Vec3 CpuRayBackend::environment(Vec3 direction) const {
  const float sky=saturate(direction.y*.5f+.5f);
  return mix(product(m_lighting.ambient,{.35f,.32f,.28f}),m_lighting.ambient*2.f,sky);
}
Sample CpuRayBackend::trace(Ray ray,Random& rng,std::uint64_t& rays,float primary_limit) const {
  Sample output; Vec3 throughput{1,1,1},radiance{}; unsigned transparent=0;
  for(unsigned bounce=0;bounce<m_settings.max_bounces;) {
    ++rays; const Hit hit=intersect(ray,bounce==0 ? primary_limit:infinity);
    if(!hit.found) { radiance+=product(throughput,environment(ray.direction)); break; }
    const Surface s=surface(hit,ray);
    if(s.alpha<1 && rng.next()>s.alpha && transparent++<64) {
      ray.width+=(hit.t+epsilon*4)*ray.spread;
      ray.origin=s.point+ray.direction*epsilon*4; primary_limit-=hit.t+epsilon*4; continue;
    }
    if(bounce==0) { output.normal=s.normal; output.depth=length(s.point-m_camera); }
    const Vec3 view=-ray.direction;
    radiance+=product(throughput,s.emission);
    Vec3 direct{};
    auto illuminate=[&](Vec3 direction,Vec3 energy,float distance) {
      const float nl=std::max(0.f,dot(s.normal,direction)); if(nl<=0) return;
      Vec3 visibility{1,1,1};
      Ray shadow{s.point+s.geometric*epsilon*4,direction,ray.width+hit.t*ray.spread,0};
      float remaining=distance;
      for(unsigned layer=0;layer<64;++layer) {
        ++rays; const auto obstruction=intersect(shadow,remaining);
        if(!obstruction.found) break;
        const auto blocker=surface(obstruction,shadow);
        const Vec3 pass=mix({1,1,1},blocker.base,.2f)*blocker.transmission;
        visibility=product(visibility,mix({1,1,1},pass,blocker.alpha));
        if(maximum(visibility)<1e-4f) return;
        shadow.origin=blocker.point+direction*epsilon*4;
        remaining-=obstruction.t+epsilon*4;
        if(remaining<=epsilon) break;
        if(layer==63) return; // bounded traversal of pathological transparent stacks
      }
      direct+=product(brdf(s,view,direction),product(energy,visibility))*nl;
    };
    if(m_lighting.sun_intensity>0) illuminate(normalize(-m_lighting.sun_direction),m_lighting.sun_color*m_lighting.sun_intensity,infinity);
    for(int i=0;i<std::clamp(m_lighting.point_light_count,0,Lighting::kMaxPointLights);++i) {
      const auto& light=m_lighting.point_lights[i]; const Vec3 delta=light.position-s.point; const float distance=length(delta);
      if(distance<epsilon||distance>=light.radius) continue;
      const float edge=1-distance/std::max(.01f,light.radius);
      illuminate(delta*(1/distance),light.color*(light.intensity*edge*edge/(1+distance*distance)),distance-epsilon*8);
    }
    if(m_settings.debug_view!=RenderDebugView::Indirect || bounce>0) radiance+=product(throughput,direct);
    if(m_settings.debug_view==RenderDebugView::Direct) break;
    if(m_settings.trace_mode==TraceMode::RayTraced) {
      radiance+=product(throughput,product(s.base,m_lighting.ambient))*((1-s.metallic)*(1-s.transmission));
      // Glossy dielectrics (water, varnish, glazing) also reflect geometry.
      // Keep the bounded ray-mode approximation for rough diffuse surfaces;
      // path mode continues sampling their full diffuse/specular transport.
      if(s.metallic<.5f && s.transmission<.01f && s.roughness>.35f) break;
    }
    Vec3 next; float scatter_spread=0;
    if(s.transmission>.01f && rng.next()<s.transmission) {
      const float eta=s.front ? 1/s.ior:s.ior,cosine=saturate(dot(s.normal,view));
      const float f0=(s.ior-1)/(s.ior+1),f=f0*f0+(1-f0*f0)*std::pow(1-cosine,5.f);
      const float k=1-eta*eta*(1-cosine*cosine);
      if(k<0||rng.next()<f) next=reflect(ray.direction,s.normal);
      else { next=ray.direction*eta+s.normal*(eta*cosine-std::sqrt(k)); throughput=product(throughput,mix({1,1,1},s.base,.2f)); }
    } else if(m_settings.trace_mode==TraceMode::RayTraced) {
      scatter_spread=.25f*s.roughness*s.roughness;
      next=reflect(ray.direction,s.normal); throughput=product(throughput,fresnel(surface_reflectance(s),dot(s.normal,view)));
    } else {
      const float spec_probability=std::clamp(.25f+.5f*s.metallic,.25f,.75f);
      if(rng.next()<spec_probability) {
        scatter_spread=.5f*s.roughness*s.roughness;
        const float a=s.roughness*s.roughness,phi=2*pi*rng.next(),u=rng.next();
        const float c=std::sqrt((1-u)/(1+(a*a-1)*u)),sine=std::sqrt(std::max(0.f,1-c*c));
        const Vec3 tangent=normalize(cross(std::fabs(s.normal.y)<.99f ? Vec3{0,1,0}:Vec3{1,0,0},s.normal));
        const Vec3 half=normalize(tangent*(sine*std::cos(phi))+cross(s.normal,tangent)*(sine*std::sin(phi))+s.normal*c);
        next=reflect(ray.direction,half);
      } else { next=cosine_direction(s.normal,rng); scatter_spread=.5f; }
      const float nl=dot(s.normal,next); if(nl<=0 || dot(s.geometric,next)<=0) break;
      const Vec3 half=normalize(view+next);
      const float pdf_spec=ggx_d(std::max(0.f,dot(s.normal,half)),s.roughness)*std::max(0.f,dot(s.normal,half))/(4*std::max(1e-6f,dot(view,half)));
      const float pdf=spec_probability*pdf_spec+(1-spec_probability)*nl/pi;
      throughput=product(throughput,brdf(s,view,next))*(nl/(std::max(1e-7f,pdf)*std::max(1e-7f,1-s.transmission)));
    }
    if(!finite(throughput)) break;
    // Russian roulette keeps the estimator unbiased while bounding long paths.
    if(bounce>=2) { const float survival=std::clamp(maximum(throughput),.05f,.95f); if(rng.next()>survival) break; throughput=throughput*(1/survival); }
    const float sign=dot(next,s.geometric)>=0 ? 1.f:-1.f;
    // A practical cone approximation, not exact ray differentials: keep the
    // incoming pixel footprint and widen by the rough specular/diffuse lobe.
    // Smooth transmission preserves spread, avoiding blanket material blur.
    ray={s.point+s.geometric*(epsilon*4*sign),normalize(next),
         ray.width+hit.t*ray.spread,ray.spread+scatter_spread}; ++bounce;
  }
  if(m_settings.debug_view==RenderDebugView::Normals) output.radiance=output.normal*.5f+Vec3{.5f,.5f,.5f};
  else if(m_settings.debug_view==RenderDebugView::Depth) { const float d=std::isfinite(output.depth) ? 1/(1+output.depth*.08f):0; output.radiance={d,d,d}; }
  else {
    if(std::isfinite(output.depth) && m_lighting.fog_end>m_lighting.fog_start) {
      const float fog=saturate((output.depth-m_lighting.fog_start)/(m_lighting.fog_end-m_lighting.fog_start));
      radiance=mix(radiance,m_lighting.fog_color,fog);
    }
    output.radiance=finite(radiance) ? radiance:Vec3{};
  }
  return output;
}
void CpuRayBackend::render() {
  if(m_rendered || !m_window) return;
  const auto start=std::chrono::steady_clock::now(); m_rendered=true;
  Mat4 inverse_vp;
  if(!inverse(m_vp,inverse_vp)) { ++m_stats.validation_errors; return; }
  std::vector<Bounds> boxes; boxes.reserve(m_instances.size());
  std::unordered_set<const Geometry*> unique;
  m_stats.unique_triangle_count=0;
  for(const auto& instance:m_instances) {
    boxes.push_back(instance.box);
    if(unique.insert(instance.geometry).second) m_stats.unique_triangle_count+=instance.geometry->triangles.size();
  }
  // BLASes persist across frames; TLAS includes all submitted objects, including
  // off-screen shadow casters and reflected geometry. Never frustum-cull rays.
  std::uint64_t hierarchy_hash=14695981039346656037ull;
  const auto instance_count=boxes.size(); hash_value(hierarchy_hash,instance_count);
  for(const auto& box:boxes) { hash_value(hierarchy_hash,box.lo); hash_value(hierarchy_hash,box.hi); }
  if(hierarchy_hash!=m_tlas_hash) { m_tlas.build(boxes); ++m_stats.tlas_builds; m_tlas_hash=hierarchy_hash; }
  hash_value(m_frame_hash,m_vp); hash_value(m_frame_hash,m_camera);
  hash_value(m_frame_hash,m_lighting.sun_direction); hash_value(m_frame_hash,m_lighting.sun_color);
  hash_value(m_frame_hash,m_lighting.sun_intensity); hash_value(m_frame_hash,m_lighting.ambient);
  hash_value(m_frame_hash,m_lighting.fog_start); hash_value(m_frame_hash,m_lighting.fog_end); hash_value(m_frame_hash,m_lighting.fog_color);
  const int lights=std::clamp(m_lighting.point_light_count,0,Lighting::kMaxPointLights); hash_value(m_frame_hash,lights);
  for(int i=0;i<lights;++i) {
    const auto& l=m_lighting.point_lights[i]; hash_value(m_frame_hash,l.position); hash_value(m_frame_hash,l.color);
    hash_value(m_frame_hash,l.intensity); hash_value(m_frame_hash,l.radius);
  }
  if(m_frame_hash!=m_previous_hash || !m_settings.accumulate || m_accumulated>=65536) m_accumulated=0;
  m_previous_hash=m_frame_hash;
  if(!m_accumulated) std::fill(m_sum.begin(),m_sum.end(),Vec3{});
  std::atomic<unsigned> next_row{0}; std::atomic<std::uint64_t> total_rays{0};
  auto worker=[&]() {
    std::uint64_t rays=0;
    for(unsigned row=next_row.fetch_add(4);row<unsigned(m_height);row=next_row.fetch_add(4)) {
      for(unsigned y=row;y<std::min(row+4,unsigned(m_height));++y) for(unsigned x=0;x<unsigned(m_width);++x) {
        const std::size_t index=std::size_t(y)*m_width+x; Vec3 color{},normal{}; float depth=infinity;
        for(unsigned sample=0;sample<m_settings.samples_per_pixel;++sample) {
          const std::uint32_t sequence=m_accumulated*m_settings.samples_per_pixel+sample;
          Random rng{std::uint32_t(index)*2654435761u+sequence*2246822519u+0x9e3779b9u};
          const float px=(float(x)+rng.next())/m_width*2-1,py=1-(float(y)+rng.next())/m_height*2;
          Vec4 near=mul(inverse_vp,{px,py,-1,1}),far=mul(inverse_vp,{px,py,1,1});
          const Vec3 origin{near.x/near.w,near.y/near.w,near.z/near.w};
          const Vec3 target{far.x/far.w,far.y/far.w,far.z/far.w};
          const Vec3 direction=normalize(target-origin);
          // Adjacent unprojected pixel rays provide a projection-aware primary
          // cone. Orthographic views get a constant width and zero spread;
          // perspective views grow with distance beyond the near plane.
          auto adjacent=[&](float dx,float dy,Vec3& near_delta,Vec3& direction_delta) {
            const Vec4 a=mul(inverse_vp,{px+dx,py+dy,-1,1}),b=mul(inverse_vp,{px+dx,py+dy,1,1});
            const Vec3 o{a.x/a.w,a.y/a.w,a.z/a.w},target2{b.x/b.w,b.y/b.w,b.z/b.w};
            near_delta=o-origin; direction_delta=normalize(target2-o)-direction;
          };
          Vec3 near_x,near_y,direction_x,direction_y;
          adjacent(2.f/m_width,0,near_x,direction_x); adjacent(0,2.f/m_height,near_y,direction_y);
          const float width=std::max(length(near_x),length(near_y));
          const float spread=std::max(length(direction_x),length(direction_y));
          const auto result=trace({origin,direction,width,spread},rng,rays,length(target-origin));
          color+=result.radiance; normal+=result.normal; depth=std::min(depth,result.depth);
        }
        color=color*(1.f/m_settings.samples_per_pixel); m_sum[index]+=color;
        m_frame[index]=m_sum[index]*(1.f/(m_accumulated+1));
        m_normal[index]=normalize(normal); m_depth[index]=depth;
      }
    }
    total_rays.fetch_add(rays,std::memory_order_relaxed);
  };
  std::vector<std::thread> workers; workers.reserve(m_threads-1);
  for(unsigned i=1;i<m_threads;++i) workers.emplace_back(worker);
  worker(); for(auto& thread:workers) thread.join();
  m_stats.rays_traced=total_rays.load();
  const bool debug=m_settings.debug_view==RenderDebugView::Normals || m_settings.debug_view==RenderDebugView::Depth;
  for(int y=0;y<m_height;++y) for(int x=0;x<m_width;++x) {
    const std::size_t index=std::size_t(y)*m_width+x; Vec3 color=m_frame[index];
    // Small spatial edge-aware filter; it never feeds back into accumulation.
    // Depth and normal gates prevent smoothing across silhouettes/material edges.
    if(m_settings.denoise && !debug && m_settings.trace_mode==TraceMode::PathTraced) {
      Vec3 sum=color; float weights=1;
      for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        if((!dx&&!dy)||x+dx<0||y+dy<0||x+dx>=m_width||y+dy>=m_height) continue;
        const auto j=std::size_t(y+dy)*m_width+x+dx;
        if(std::isfinite(m_depth[index])!=std::isfinite(m_depth[j])) continue;
        if(std::isfinite(m_depth[index]) && (std::fabs(m_depth[index]-m_depth[j])>.015f*std::max(1.f,m_depth[index]) || dot(m_normal[index],m_normal[j])<.92f)) continue;
        const Vec3 diff=m_frame[j]-color;
        const float weight=.35f*std::exp(-dot(diff,diff)*4);
        sum+=m_frame[j]*weight; weights+=weight;
      }
      color=sum*(1/weights);
    }
    if(!debug) {
      auto tone=[&](float c) {
        c=std::max(0.f,c*m_settings.exposure);
        // ACES fitted display transform, followed by exact sRGB encoding.
        c=saturate((c*(2.51f*c+.03f))/(c*(2.43f*c+.59f)+.14f));
        return c<=.0031308f ? 12.92f*c:1.055f*std::pow(c,1/2.4f)-.055f;
      };
      color={tone(color.x),tone(color.y),tone(color.z)};
    }
    m_color[index]=0xff000000u | (unsigned(saturate(color.x)*255+.5f)<<16) |
                    (unsigned(saturate(color.y)*255+.5f)<<8) | unsigned(saturate(color.z)*255+.5f);
  }
  ++m_accumulated; ++m_stats.frame_index; m_stats.accumulated_frames=m_accumulated;
  m_stats.cpu_frame_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  // Scene transitions cannot retain unused meshes indefinitely.
  for(auto it=m_cache.begin();it!=m_cache.end();) {
    if(m_stats.frame_index-it->second->last_used>120) it=m_cache.erase(it); else ++it;
  }
}
void CpuRayBackend::draw_hud_rect(float x,float y,float width,float height,const Color& color) {
  render(); if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)) return;
  const int x0=int(std::clamp(std::floor(x),0.f,float(m_width))),y0=int(std::clamp(std::floor(y),0.f,float(m_height)));
  const int x1=int(std::clamp(std::ceil(x+width),0.f,float(m_width))),y1=int(std::clamp(std::ceil(y+height),0.f,float(m_height)));
  for(int py=y0;py<y1;++py) for(int px=x0;px<x1;++px) {
    auto& p=m_color[std::size_t(py)*m_width+px]; const unsigned alpha=color.a,inv=255-alpha;
    const unsigned r=(((p>>16)&255)*inv+color.r*alpha+127)/255;
    const unsigned g=(((p>>8)&255)*inv+color.g*alpha+127)/255;
    const unsigned b=((p&255)*inv+color.b*alpha+127)/255;
    p=0xff000000u|(r<<16)|(g<<8)|b;
  }
}
void CpuRayBackend::end_frame() {
  render(); if(!m_window) return;
  SDL_Surface* surface=SDL_GetWindowSurface(m_window);
  if(!surface) { ++m_stats.validation_errors; return; }
  SDL_Surface* source=SDL_CreateRGBSurfaceWithFormatFrom(m_color.data(),m_width,m_height,32,m_width*4,SDL_PIXELFORMAT_ARGB8888);
  if(!source) { ++m_stats.validation_errors; return; }
  if(SDL_BlitScaled(source,nullptr,surface,nullptr)!=0 || SDL_UpdateWindowSurface(m_window)!=0) ++m_stats.validation_errors;
  SDL_FreeSurface(source);
}
bool CpuRayBackend::read_rgb_framebuffer(std::vector<std::uint8_t>& rgb,int& width,int& height) {
  render(); width=m_width; height=m_height; rgb.resize(std::size_t(width)*height*3);
  for(std::size_t i=0;i<m_color.size();++i) { rgb[i*3]=std::uint8_t(m_color[i]>>16); rgb[i*3+1]=std::uint8_t(m_color[i]>>8); rgb[i*3+2]=std::uint8_t(m_color[i]); }
  return !rgb.empty();
}
} // namespace
std::unique_ptr<IRenderBackend> create_cpu_ray_backend() { return std::make_unique<CpuRayBackend>(); }
} // namespace fury
