#include "fury/material_sampling.hpp"
#include "fury/renderer.hpp"
#include "fury/gl_loader.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Exercise the real production shadow path even on llvmpipe, without changing
// its normal performance policy. Only the test executable wraps this SDL lookup.
#ifdef FURY_TEST_WRAP_GL
static bool force_shadow_path=false;
static const fury::gl::GLubyte* (*real_get_string)(fury::gl::GLenum)=nullptr;
extern "C" void* __real_SDL_GL_GetProcAddress(const char*);
static const fury::gl::GLubyte* test_get_string(fury::gl::GLenum name) {
  if(force_shadow_path && name==fury::gl::GL_RENDERER)
    return reinterpret_cast<const fury::gl::GLubyte*>("Fury test shadow-path override");
  return real_get_string(name);
}
extern "C" void* __wrap_SDL_GL_GetProcAddress(const char* name) {
  void* result=__real_SDL_GL_GetProcAddress(name);
  if(std::strcmp(name,"glGetString")==0) {
    real_get_string=reinterpret_cast<decltype(real_get_string)>(result);
    return reinterpret_cast<void*>(&test_get_string);
  }
  return result;
}
#endif
using namespace fury;
namespace {
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
bool equal(Vec4 a,Vec4 b) { return std::memcmp(&a,&b,sizeof(a))==0; }
RgbaImage solid(int w,int h,int r,int g,int b,int a=255) {
  RgbaImage result{w,h,std::vector<std::uint8_t>(std::size_t(w)*h*4)};
  for(std::size_t i=0;i<result.pixels.size();i+=4) {
    result.pixels[i]=std::uint8_t(r);result.pixels[i+1]=std::uint8_t(g);
    result.pixels[i+2]=std::uint8_t(b);result.pixels[i+3]=std::uint8_t(a);
  }
  return result;
}
std::shared_ptr<MaterialColorDetail> detail_image(int alpha=255) {
  auto result=std::make_shared<MaterialColorDetail>();
  result->color=solid(64,64,0,255,0,alpha);
  result->color_mips=build_mip_chain(result->color,TextureEncoding::SRGB);
  result->color_mips.erase(result->color_mips.begin());
  result->regions[0]={{.25f,.25f},{.75f,.75f},{0,0},{1,1},false};
  result->region_count=1;
  return result;
}
void sampling_contract() {
  const Vec4 base{.1234567f,.3456789f,.8123456f,.9876543f};
  auto detail=detail_image(32);
  require(equal(apply_material_color_detail(nullptr,base,{.5f,.5f}),base),"Null detail is bit-preserving");
  for(Vec2 uv:{Vec2{.1f,.5f},Vec2{.75f,.5f},Vec2{.5f,.75f},Vec2{.5f,0}})
    require(equal(apply_material_color_detail(detail.get(),base,uv),base),"Outside half-open windows is bit-preserving");
  const auto selected=apply_material_color_detail(detail.get(),base,{.5f,.5f});
  require(selected.x==0 && selected.y==1 && std::abs(selected.w-32/255.f)<1e-6f,"Color and linear alpha are replaced");
  require(equal(apply_material_color_detail(detail.get(),base,{-1.5f,2.5f}),selected),"Window selection repeats positive and negative UVs");
  detail->regions[0].preserve_base_alpha=true;
  require(apply_material_color_detail(detail.get(),base,{.5f,.5f}).w==base.w,"Opaque-region alpha preservation is exact");
  detail->regions[1]=detail->regions[0];detail->regions[1].preserve_base_alpha=false;detail->region_count=2;
  require(apply_material_color_detail(detail.get(),base,{.5f,.5f}).w==base.w,"First valid overlapping window wins");
  detail->regions[0].source_max.x=std::numeric_limits<float>::quiet_NaN();
  require(apply_material_color_detail(detail.get(),base,{.5f,.5f}).w==selected.w,"Invalid region skips safely to the next valid region");
  detail->region_count=9999;detail->regions[1].source_max.x=.2f;
  require(equal(apply_material_color_detail(detail.get(),base,{.5f,.5f}),base),"Region count is bounded and reversed/empty regions are ignored");
  detail->color={};
  require(equal(apply_material_color_detail(detail.get(),base,{.5f,.5f}),base),"Malformed detail image is a no-op");

  detail=detail_image();detail->color=solid(64,64,255,0,0);
  detail->color_mips={solid(32,32,0,255,0),solid(16,16,0,0,255)};
  detail->regions[0]={{.25f,.25f},{.375f,.375f},{0,0},{1,1},false};
  const auto mip=apply_material_color_detail(detail.get(),base,{.3f,.3f},{{1/256.f,0},{0,1/256.f}});
  require(mip.x==0 && mip.y==1 && mip.z==0,"Window remaps derivatives before computing true detail-image LOD");
  detail->regions[0].detail_max={.5f,.25f};
  const auto level0=apply_material_color_detail(detail.get(),base,{.3f,.3f},{{1/256.f,0},{0,1/256.f}});
  require(level0.x==1 && level0.y==0,"Anisotropic rectangular transform scales each derivative component");
  detail->color=solid(2,1,128,128,128);detail->color_mips.clear();
  const auto srgb=apply_material_color_detail(detail.get(),base,{.3f,.3f});
  require(srgb.x>.215f && srgb.x<.217f,"Detail RGB is decoded from sRGB");
  detail->color=solid(1024,1024,128,128,128);
  auto chain=build_mip_chain(detail->color,TextureEncoding::SRGB);
  std::size_t bytes=0;for(const auto& image:chain) bytes+=image.pixels.size();
  require(bytes==5592404,"One 1024-square RGBA full chain is exactly 5,592,404 bytes");
  detail->color_mips=std::move(chain);detail->color_mips.erase(detail->color_mips.begin());
  require(detail->pixel_bytes()==5592404,"Detail reports retained level-zero plus mip payload exactly");
  std::cout<<"Color-detail sampler and memory contract passed\n";
}
Mesh quad() {
  Mesh m;m.vertices={{{-1,-1,0},{0,0,1},{1,1,1},{0,1}},{{1,-1,0},{0,0,1},{1,1,1},{1,1}},
                     {{1,1,0},{0,0,1},{1,1,1},{1,0}},{{-1,1,0},{0,0,1},{1,1,1},{0,0}}};
  m.indices={0,1,2,0,2,3};return m;
}
struct Fixture {
  static constexpr int width=96,height=64;
  SDL_Window* window{};std::unique_ptr<IRenderBackend> renderer;
  Mesh plane=quad();Lighting lighting;bool gl_mode{},ray_mode{};
  explicit Fixture(const std::string& mode):gl_mode(mode=="gl"),ray_mode(mode=="ray") {
    window=SDL_CreateWindow("Color detail tests",0,0,width,height,SDL_WINDOW_HIDDEN|(gl_mode?SDL_WINDOW_OPENGL:0));
    require(window!=nullptr,"Create test window");
    renderer=gl_mode?create_gl_backend():(ray_mode?create_cpu_ray_backend():create_software_backend());
#ifdef FURY_TEST_WRAP_GL
    force_shadow_path=gl_mode;
#endif
    require(renderer->create(window,width,height),"Create renderer and shaders");
#ifdef FURY_TEST_WRAP_GL
    force_shadow_path=false;
#endif
    if(ray_mode) {
      auto settings=renderer->settings();settings.denoise=false;settings.accumulate=false;
      settings.samples_per_pixel=4;settings.max_bounces=2;settings.trace_mode=TraceMode::RayTraced;
      require(renderer->configure(settings),"Configure deterministic CPU ray mode");
    }
    lighting.ambient={1,1,1};lighting.sun_intensity=0;lighting.ao_strength=0;
    lighting.fog_start=100;lighting.fog_end=200;lighting.enable_shadows=false;
    lighting.enable_reflections=false;lighting.enable_bloom=false;
    if(gl_mode) std::cout<<"Actual GL driver: "<<reinterpret_cast<const char*>(gl::GetString(gl::GL_RENDERER))<<'\n';
  }
  ~Fixture() {renderer.reset();SDL_DestroyWindow(window);}
  void reset() {
    renderer->set_lighting(lighting);renderer->begin_frame({0,0,0,255});
    renderer->set_view_proj(look_at({0,0,4},{0,0,0},{0,1,0}),perspective(radians(50),1.5f,.1f,20));
    renderer->set_camera_position({0,0,4});renderer->set_time(0);
  }
  void uv(Vec2 value) {for(auto& v:plane.vertices)v.uv=value;plane.mark_dirty();}
  std::vector<std::uint8_t> image() {
    int w{},h{};std::vector<std::uint8_t> result;
    require(renderer->read_rgb_framebuffer(result,w,h),"Read renderer framebuffer");
    require(w==width && h==height,"Expected framebuffer dimensions");return result;
  }
  std::array<int,3> pixel(const std::vector<std::uint8_t>& image,int x=48,int y=32) {
    const auto i=(y*width+x)*3;return {image[i],image[i+1],image[i+2]};
  }
  std::vector<std::uint8_t> draw(const Material& front,bool background=false) {
    reset();renderer->draw_mesh(plane,Mat4::identity(),front);
    if(background) {
      Material blue;blue.albedo={0,0,0};blue.textures=std::make_shared<MaterialTextures>();
      blue.emissive=1;blue.emissive_color={0,0,1};
      renderer->draw_mesh(plane,translate({0,0,-.1f}),blue);
    }
    return image();
  }
};
void rendering_contract(Fixture& f) {
  auto atlas=std::make_shared<MaterialTextures>();atlas->base_color=solid(8,8,255,0,0);
  const auto original=atlas->base_color.pixels;
  Material material;material.textures=atlas;
  auto detail=detail_image();f.uv({.1f,.5f});
  const auto baseline=f.draw(material);material.color_detail=detail;
  const auto outside=f.draw(material);
  if(outside!=baseline) {auto a=f.pixel(baseline),b=f.pixel(outside);std::cerr<<"Outside baseline "<<a[0]<<","<<a[1]<<","<<a[2]<<" detail "<<b[0]<<","<<b[1]<<","<<b[2]<<"\n";}
  require(outside==baseline,"All pixels outside the source window retain exact baseline output");
  f.uv({.5f,.5f});auto color=f.pixel(f.draw(material));
  require(color[1]>color[0]+70 && color[1]>color[2]+70,"Detail changes only selected base color");
  require(material.textures.get()==atlas.get() && atlas->base_color.pixels==original,"Shared atlas identity and bytes remain untouched");
  material.color_detail.reset();color=f.pixel(f.draw(material));
  require(color[0]>color[1]+70,"Absent detail restores original atlas after a detailed draw");

  detail=detail_image(0);material.color_detail=detail;material.alpha_cutoff=.5f;
  color=f.pixel(f.draw(material,true));
  require(color[2]>color[1]+70,"Detail alpha cutout reveals rear geometry instead of writing depth/hit visibility");
  detail=std::make_shared<MaterialColorDetail>(*detail);detail->regions[0].preserve_base_alpha=true;
  material.color_detail=detail;color=f.pixel(f.draw(material,true));
  require(color[1]>color[2]+70,"Opaque detail window preserves original atlas alpha");
  detail=std::make_shared<MaterialColorDetail>(*detail);detail->regions[0].preserve_base_alpha=false;
  material.color_detail=detail;material.alpha_cutoff=-1;
  color=f.pixel(f.draw(material,true));require(color[1]>color[2]+70,"Opaque mode still ignores detail alpha");
  material.alpha_blend=true;color=f.pixel(f.draw(material,true));
  require(color[2]>color[1]+70,"Transparent detail blends/skips consistently with original material alpha semantics");
  material.alpha_blend=false;material.alpha_cutoff=.5f;material.opacity=.4f;
  detail=detail_image();material.color_detail=detail;color=f.pixel(f.draw(material,true));
  require(color[2]>color[1]+70,"Detail alpha still multiplies material opacity");

  // Large remap from a tiny source UV window must choose minified detail mips.
  material.opacity=1;material.alpha_cutoff=-1;
  detail=detail_image();detail->color=solid(1024,1024,255,0,0);
  detail->color_mips.clear();
  for(int n=512;n>=1;n/=2)detail->color_mips.push_back(solid(n,n,0,255,0));
  detail->regions[0]={{.25f,.25f},{.26f,.26f},{0,0},{1,1},false};
  material.color_detail=detail;
  f.plane=quad();for(auto& v:f.plane.vertices){v.uv.x=.25f+v.uv.x*.01f;v.uv.y=.25f+v.uv.y*.01f;}
  f.plane.mark_dirty();color=f.pixel(f.draw(material));
  require(color[1]>color[0]+70,"Real raster/ray footprints select remapped high-resolution mips");
  if(f.ray_mode) {
    auto settings=f.renderer->settings();settings.trace_mode=TraceMode::PathTraced;
    require(f.renderer->configure(settings),"Configure path tracing");
    color=f.pixel(f.draw(material));require(color[1]>color[0]+40,"CPU path tracing also samples scoped detail");
    settings.trace_mode=TraceMode::RayTraced;settings.accumulate=true;
    require(f.renderer->configure(settings),"Configure accumulation");
    f.draw(material);f.draw(material);
    require(f.renderer->statistics().accumulated_frames==2,"Immutable detailed material accumulates");
    material.color_detail=std::make_shared<MaterialColorDetail>(*detail);f.draw(material);
    require(f.renderer->statistics().accumulated_frames==1,"Replacing immutable detail invalidates CPU history");
  }
  if(f.gl_mode) {
    auto get_level=reinterpret_cast<void(*)(gl::GLenum,gl::GLint,gl::GLenum,gl::GLint*)>(SDL_GL_GetProcAddress("glGetTexLevelParameteriv"));
    gl::ActiveTexture(gl::GL_TEXTURE0+6);gl::GLint uploaded_width{};
    get_level(gl::GL_TEXTURE_2D,0,0x1000,&uploaded_width);
    require(uploaded_width==1024,"Detail uploads independently at 1024 resolution");
    gl::ActiveTexture(gl::GL_TEXTURE0);get_level(gl::GL_TEXTURE_2D,0,0x1000,&uploaded_width);
    require(uploaded_width==8,"Detail upload never clones or enlarges original atlas");
    auto get_error=reinterpret_cast<gl::GLenum(*)()>(SDL_GL_GetProcAddress("glGetError"));
    require(get_error && get_error()==0,"Detail rendering leaves no GL errors");
  }
  std::cout<<(f.gl_mode?"OpenGL":(f.ray_mode?"CPU ray/path":"Software raster"))<<" scoped detail color/alpha/LOD checks passed\n";
}
void cpu_shadow_visibility(Fixture& f) {
  f.plane=quad();f.uv({.5f,.5f});f.lighting.ambient={.04f,.04f,.04f};
  f.lighting.sun_direction={-.6f,0,-1};f.lighting.sun_intensity=3;
  Material receiver;receiver.albedo={.7f,.7f,.7f};receiver.roughness=.9f;
  Material blocker;blocker.alpha_cutoff=.5f;blocker.roughness=.9f;
  auto maps=std::make_shared<MaterialTextures>();maps->base_color=solid(1,1,255,255,255);blocker.textures=maps;
  auto detail=detail_image(0);blocker.color_detail=detail;
  auto draw=[&](bool cast) {
    f.reset();f.renderer->draw_mesh(f.plane,scale({2,2,1}),receiver);
    if(cast)f.renderer->draw_mesh(f.plane,translate({.65f,0,1})*scale({.3f,.7f,1}),blocker);
    return f.pixel(f.image());
  };
  const auto baseline=draw(false),clear=draw(true);
  require(std::abs(baseline[0]-clear[0])<3,"CPU detail alpha removes occlusion from secondary shadow rays");
  detail=std::make_shared<MaterialColorDetail>(*detail);detail->regions[0].preserve_base_alpha=true;blocker.color_detail=detail;
  const auto opaque=draw(true);
  require(opaque[0]+30<baseline[0],"CPU preserved atlas alpha restores shadow-ray occlusion");
  std::cout<<"CPU secondary shadow-ray detail alpha checks passed\n";
}
void shadow_contract(Fixture& f) {
  f.lighting.enable_shadows=true;f.lighting.sun_direction={0,0,-1};f.lighting.shadow_map_size=256;
  f.renderer->set_shadow_map_size(256);f.plane=quad();f.uv({.5f,.5f});
  auto atlas=std::make_shared<MaterialTextures>();atlas->base_color=solid(1,1,255,0,0);
  Material material;material.textures=atlas;material.alpha_cutoff=.5f;
  auto detail=detail_image(0);material.color_detail=detail;
  auto depth=[&]() {
    f.reset();require(f.renderer->begin_shadow_pass(),"Production shadow pass enabled by test-only driver-name shim");
    f.renderer->draw_mesh(f.plane,scale({20,20,1}),material);
    const int resolution=f.renderer->shadow_map_size();
    std::vector<float> result(std::size_t(resolution)*resolution);
    gl::ReadPixels(0,0,resolution,resolution,gl::GL_DEPTH_COMPONENT,gl::GL_FLOAT,result.data());
    f.renderer->end_shadow_pass();return result;
  };
  auto discarded=depth();
  require(std::all_of(discarded.begin(),discarded.end(),[](float d){return d==1;}),"Detail alpha fully discards shadow depth");
  detail=std::make_shared<MaterialColorDetail>(*detail);detail->regions[0].preserve_base_alpha=true;material.color_detail=detail;
  auto preserved=depth();
  require(*std::min_element(preserved.begin(),preserved.end())<.99f,"Alpha-preserving window writes opaque shadow depth");
  material.color_detail.reset();require(depth()==preserved,"Opaque alpha-preserving detail matches baseline shadow exactly");
  detail=detail_image(0);material.color_detail=detail;f.uv({.1f,.5f});
  require(depth()==preserved,"Outside source window preserves baseline shadow depth exactly");
  // Minified shadow footprint must use mapped derivatives rather than atlas UVs.
  detail=detail_image();detail->color=solid(1024,1024,0,255,0,0);detail->color_mips.clear();
  for(int n=512;n>=1;n/=2)detail->color_mips.push_back(solid(n,n,0,255,0,255));
  detail->regions[0]={{.25f,.25f},{.26f,.26f},{0,0},{1,1},false};material.color_detail=detail;
  f.plane=quad();for(auto& v:f.plane.vertices){v.uv.x=.25f+v.uv.x*.01f;v.uv.y=.25f+v.uv.y*.01f;}f.plane.mark_dirty();
  preserved=depth();
  require(*std::min_element(preserved.begin(),preserved.end())<.99f,"Shadow alpha chooses minified remapped detail mip");
  std::cout<<"Production OpenGL shadow alpha/window/preservation/LOD checks passed (test-only software-driver override)\n";
}
}
int main(int argc,char** argv) {
  const bool gl_mode=argc>1 && std::string(argv[1])=="--gl";
  if(!gl_mode)SDL_setenv("SDL_VIDEODRIVER","dummy",1);
  if(SDL_Init(SDL_INIT_VIDEO)!=0){std::cerr<<SDL_GetError()<<'\n';return gl_mode?77:1;}
  int result=0;
  try {
    if(gl_mode) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
      SDL_Window* probe_window=SDL_CreateWindow("GL capability probe",0,0,32,32,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);
      SDL_GLContext probe=probe_window?SDL_GL_CreateContext(probe_window):nullptr;
      if(!probe) {
        std::cout<<"SKIP: GL unavailable: "<<SDL_GetError()<<'\n';
        if(probe_window)SDL_DestroyWindow(probe_window);SDL_Quit();return 77;
      }
      SDL_GL_DeleteContext(probe);SDL_DestroyWindow(probe_window);
      Fixture f("gl");rendering_contract(f);
#ifdef FURY_TEST_WRAP_GL
      shadow_contract(f);
#endif
    } else {sampling_contract();{Fixture f("soft");rendering_contract(f);}{Fixture f("ray");rendering_contract(f);cpu_shadow_visibility(f);}}
  }catch(const std::exception& e){std::cerr<<"Color detail: "<<e.what()<<'\n';result=1;}
  SDL_Quit();return result;
}
