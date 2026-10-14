#include "fury/renderer.hpp"
#include "fury/texture.hpp"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace fury;
namespace {
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
struct Draw { Mesh* mesh; Mat4 transform; Material material; };
struct Fixture {
  SDL_Window* window{}; std::unique_ptr<IRenderBackend> renderer;
  Vec3 eye{0,0,4}; Mat4 view=look_at(eye,{0,0,0},{0,1,0});
  Mat4 projection=perspective(radians(50),1.5f,.1f,20);
  Lighting lighting;
  Fixture(unsigned threads=1) {
    SDL_setenv("FURY_CPU_THREADS",std::to_string(threads).c_str(),1);
    window=SDL_CreateWindow("CPU ray tests",0,0,96,64,SDL_WINDOW_HIDDEN);
    require(window!=nullptr,"dummy window"); renderer=create_cpu_ray_backend();
    require(renderer->create(window,96,64),"CPU backend create");
    auto settings=renderer->settings(); settings.denoise=false; settings.samples_per_pixel=4;
    settings.max_bounces=4; settings.trace_mode=TraceMode::RayTraced;
    require(renderer->configure(settings),"CPU configure");
    lighting.sun_intensity=0; lighting.ambient={.04f,.04f,.04f}; lighting.fog_start=100; lighting.fog_end=200;
  }
  ~Fixture() { renderer.reset(); SDL_DestroyWindow(window); }
  std::vector<std::uint8_t> draw(const std::vector<Draw>& objects,float time=0,bool present=false) {
    renderer->begin_frame({0,0,0,255}); renderer->set_view_proj(view,projection);
    renderer->set_camera_position(eye); renderer->set_lighting(lighting); renderer->set_time(time);
    for(std::size_t i=0;i<objects.size();++i) {
      renderer->set_object_id(i+1); renderer->draw_mesh(*objects[i].mesh,objects[i].transform,objects[i].material);
    }
    std::vector<std::uint8_t> rgb; int w{},h{};
    require(renderer->read_rgb_framebuffer(rgb,w,h),"CPU readback");
    require(w==96 && h==64 && rgb.size()==96*64*3,"readback dimensions");
    if(present) renderer->end_frame();
    return rgb;
  }
};
Mesh quad() {
  Mesh m; m.vertices={{{-1,-1,0},{0,0,1},{1,1,1},{0,1}},{{1,-1,0},{0,0,1},{1,1,1},{1,1}},
                     {{1,1,0},{0,0,1},{1,1,1},{1,0}},{{-1,1,0},{0,0,1},{1,1,1},{0,0}}};
  m.indices={0,1,2,0,2,3}; return m;
}
Material emission(Vec3 color) { Material m; m.albedo={0,0,0}; m.emissive=1; m.emissive_color=color; m.textures=std::make_shared<MaterialTextures>(); return m; }
Vec3 pixel(const std::vector<std::uint8_t>& image,unsigned x=48,unsigned y=32) {
  const auto index=(y*96+x)*3; return {float(image[index]),float(image[index+1]),float(image[index+2])};
}
void coverage_materials() {
  Fixture f; auto plane=quad(); const auto blue=emission({0,0,1}),green=emission({0,1,0});
  auto frame=f.draw({{&plane,Mat4::identity(),blue}},0,true);
  require(pixel(frame).z>220 && pixel(frame).x<5,"emissive map color");
  auto legacy=blue; legacy.textures.reset(); legacy.emissive_color={1,1,1}; legacy.albedo={1,0,0};
  require(pixel(f.draw({{&plane,Mat4::identity(),legacy}})).x>220,"legacy emission albedo tint");
  auto maps=std::make_shared<MaterialTextures>(); maps->base_color={2,1,{255,255,255,255,255,255,255,0}};
  auto mask=blue; mask.textures=maps; mask.alpha_cutoff=.5f;
  frame=f.draw({{&plane,translate({0,0,-.1f}),green},{&plane,Mat4::identity(),mask}});
  require(pixel(frame,36,32).z>200 && pixel(frame,60,32).y>200,"masked rays reveal rear geometry");
  auto blend=blue; blend.alpha_blend=true; blend.opacity=0;
  const auto behind=f.draw({{&plane,translate({0,0,-.1f}),green}});
  frame=f.draw({{&plane,translate({0,0,-.1f}),green},{&plane,Mat4::identity(),blend}});
  require(pixel(frame).y>220 && pixel(frame).z<5,"zero-alpha blend skipped");
  (void)behind;
  auto single=blue; single.double_sided=false;
  frame=f.draw({{&plane,rotate_y(radians(180)),single}});
  require(pixel(frame).z<100,"opaque backfaces culled");
  const auto mirrored=f.draw({{&plane,scale({-1,1,1}),single}});
  require(pixel(mirrored).z>220,"Mirrored single-sided node preserves authored front face");
  single.double_sided=true;
  require(pixel(f.draw({{&plane,rotate_y(radians(180)),single}})).z>220,"double-sided surfaces visible");
}
void depth_and_instances() {
  Fixture f; auto plane=quad(); auto red=emission({1,0,0}),green=emission({0,1,0});
  auto frame=f.draw({{&plane,translate({0,0,-1})*scale({2,2,1}),red},{&plane,scale({.7f,.7f,3}),green}});
  require(pixel(frame).y>220 && pixel(frame).x<5,"closest hit and nonuniform scale preserve world distance");
  const auto stats=f.renderer->statistics();
  require(stats.blas_builds==1 && stats.instance_count==2 && stats.triangle_count==4 && stats.unique_triangle_count==2,"instanced BLAS reuse");
  f.projection=perspective(radians(50),1.5f,.1f,5);
  const auto sky=f.draw({});
  frame=f.draw({{&plane,translate({0,0,-10})*scale({100,100,1}),red}});
  require(frame==sky,"primary far-plane clipping");
  frame=f.draw({{&plane,translate({0,0,3.95f})*scale({10,10,1}),red}});
  require(frame==sky,"primary near-plane clipping");
  auto clear=red; clear.alpha_blend=true; clear.opacity=0;
  frame=f.draw({{&plane,translate({0,0,-2})*scale({100,100,1}),red},
                {&plane,scale({100,100,1}),clear}});
  require(frame==sky,"alpha skipping preserves the remaining primary far-plane distance");
}
void tlas_order_invariance() {
  Fixture f; auto plane=quad(); std::vector<Draw> objects;
  for(int i=0;i<14;++i) {
    const float x=(i%4-1.5f)*.1f;
    const Mat4 transform=translate({x,0,-4.f+i*.5f})*
        scale({(i%2?-1.f:1.f)*(1.2f+i*.1f),.8f+i*.1f,.5f*(1+i%3)});
    objects.push_back({&plane,transform,emission(i==13?Vec3{0,1,0}:Vec3{1,0,0})});
  }
  const auto first=f.draw(objects);
  require(pixel(first).y>220 && pixel(first).x<5,"Multi-node TLAS selects the closest nonuniform mirrored instance");
  require(f.renderer->statistics().instance_count==14 && f.renderer->statistics().unique_triangle_count==2,
          "Multi-node TLAS retains shared BLAS geometry");
  std::reverse(objects.begin(),objects.end()); f.renderer->reset_history();
  require(f.draw(objects)==first,"BVH closest-hit image is independent of instance submission order");
}
void oblique_glass_exit() {
  Fixture f; f.eye={2,0,4}; f.view=look_at(f.eye,{0,0,0},{0,1,0});
  f.projection=orthographic(-.6f,.6f,-.4f,.4f,.1f,20);
  f.lighting.ambient={0,0,0};
  auto settings=f.renderer->settings(); settings.samples_per_pixel=32; settings.max_bounces=6;
  require(f.renderer->configure(settings),"Oblique slab configuration");
  auto slab=make_box({4,4,.5f},{1,1,1}),target=quad();
  Material glass; glass.transmission=1; glass.index_of_refraction=1.5f; glass.double_sided=false;
  // Snell's law through parallel faces restores the outgoing direction. A
  // renderer that culls the exit face instead keeps the refracted interior ray
  // and misses this narrow target by approximately 0.33 world units.
  const float sine=2.f/std::sqrt(20.f),inside_sine=sine/glass.index_of_refraction;
  const float inside_slope=inside_sine/std::sqrt(1.f-inside_sine*inside_sine);
  const float target_x=.125f-.5f*inside_slope-1.75f*.5f;
  const auto image=f.draw({{&slab,Mat4::identity(),glass},
                          {&target,translate({target_x,0,-2})*scale({.07f,.4f,1}),emission({0,1,0})}});
  require(pixel(image).y>180 && pixel(image).x<5,"Single-sided glass exit refraction restores the expected outgoing ray");
}
void history_and_validation() {
  Fixture f; auto plane=quad(); auto blue=emission({0,0,1});
  f.draw({{&plane,Mat4::identity(),blue}});
  f.draw({{&plane,Mat4::identity(),blue}},1);
  require(f.renderer->statistics().accumulated_frames==2,"static time does not reset history");
  blue.roughness=.3f; f.draw({{&plane,Mat4::identity(),blue}});
  require(f.renderer->statistics().accumulated_frames==1,"material edits reset accumulation");
  f.lighting.ambient.x=.1f; f.draw({{&plane,Mat4::identity(),blue}});
  require(f.renderer->statistics().accumulated_frames==1,"lighting edits reset accumulation");
  f.eye.x=.1f; f.view=look_at(f.eye,{0,0,0},{0,1,0}); f.draw({{&plane,Mat4::identity(),blue}});
  require(f.renderer->statistics().accumulated_frames==1,"camera edits reset accumulation");
  plane.vertices[0].position.x-=.1f; plane.mark_dirty(); f.draw({{&plane,Mat4::identity(),blue}});
  require(f.renderer->statistics().blas_builds==2 && f.renderer->statistics().accumulated_frames==1,"geometry revision rebuilds BLAS and history");
  blue.uv_scroll_u=.1f; f.draw({{&plane,Mat4::identity(),blue}},1); f.draw({{&plane,Mat4::identity(),blue}},2);
  require(f.renderer->statistics().accumulated_frames==1,"animated materials reset history");
  auto settings=f.renderer->settings(); settings.upscaler=Upscaler::FSR;
  require(!f.renderer->configure(settings),"no fake CPU FSR support"); settings=f.renderer->settings(); settings.samples_per_pixel=0;
  require(!f.renderer->configure(settings),"invalid sample count rejected"); settings=f.renderer->settings(); settings.exposure=std::numeric_limits<float>::quiet_NaN();
  require(!f.renderer->configure(settings),"nonfinite exposure rejected"); settings=f.renderer->settings(); settings.debug_view=RenderDebugView::Motion;
  require(!f.renderer->configure(settings),"unsupported motion view rejected");
  const auto stats=f.renderer->statistics();
  require(stats.software_ray_tracing && !stats.hardware_ray_tracing && stats.gpu_frame_ms==0 && stats.cpu_frame_ms>0 && stats.rays_traced>0,"honest CPU telemetry");
  f.renderer->reset_history(); f.draw({{&plane,Mat4::identity(),blue}});
  require(f.renderer->statistics().accumulated_frames==1,"explicit history reset");
  f.renderer->resize(80,48); f.renderer->begin_frame({}); f.renderer->set_view_proj(f.view,f.projection);
  std::vector<std::uint8_t> image; int width{},height{};
  require(f.renderer->read_rgb_framebuffer(image,width,height) && width==80 && height==48,"CPU resize readback");
}
void glossy_dielectric_reflections() {
  Fixture f; f.lighting.sun_intensity=0;f.lighting.ambient={0,0,0};
  auto plane=quad();
  Material glossy;glossy.albedo={0,0,0};glossy.metallic=0;glossy.transmission=0;
  glossy.roughness=.2f;glossy.index_of_refraction=1.5f;
  auto green=emission({0,1,0});green.emissive=4;
  const std::vector<Draw> objects{{&plane,Mat4::identity(),glossy},
      {&plane,translate({0,0,6})*scale({10,10,1}),green}};
  const auto reflected=f.draw(objects);
  require(pixel(reflected).y>60 && pixel(reflected).x<3 && pixel(reflected).z<3,
          "Opaque nonmetal glossy surface must reflect off-camera geometry");
  auto water=objects;water[0].material.index_of_refraction=1.333f;
  const auto water_reflection=f.draw(water);
  require(pixel(water_reflection).y>30 && pixel(water_reflection).y+10<pixel(reflected).y,
          "Dielectric reflection must honor material IOR, including water");
  auto matte=objects;matte[0].material.roughness=.9f;
  require(pixel(f.draw(matte)).y<3,"Bounded ray-mode diffuse surface remains direct/ambient only");
  auto direct_settings=f.renderer->settings();direct_settings.debug_view=RenderDebugView::Direct;
  require(f.renderer->configure(direct_settings),"Direct debug configuration");
  require(pixel(f.draw(objects)).y<3,"Reflections must not leak into direct-only debug view");
}
void shadows_and_path_determinism() {
  auto plane=quad(); Material matte; matte.albedo={.7f,.7f,.7f}; matte.roughness=.9f;
  Material clear=matte; clear.alpha_blend=true; clear.opacity=0;
  Fixture f; f.lighting.sun_direction={-.6f,0,-1}; f.lighting.sun_intensity=3;
  const auto base=f.draw({{&plane,scale({2,2,1}),matte}});
  // This blocker intersects the receiver's shadow ray but is off the primary view.
  const Mat4 blocker=translate({.65f,0,1})*scale({.3f,.7f,1});
  f.renderer->reset_history(); const auto transparent=f.draw({{&plane,scale({2,2,1}),matte},{&plane,blocker,clear}});
  require(length(pixel(base)-pixel(transparent))<2,"transparent geometry does not cast opaque shadows");
  auto opaque=clear; opaque.alpha_blend=false; opaque.opacity=1;
  const auto shadow=f.draw({{&plane,scale({2,2,1}),matte},{&plane,blocker,opaque}});
  require(pixel(shadow).x+30<pixel(base).x,"BVH shadow occlusion darkens receiver");
  auto box=make_box({1,1,1},{1,1,1}); Material metal; metal.albedo={.8f,.4f,.1f}; metal.metallic=.9f; metal.roughness=.18f;
  std::vector<Draw> scene{{&plane,translate({0,0,-1})*scale({3,3,1}),matte},{&box,rotate_y(.3f),metal}};
  auto settings=f.renderer->settings(); settings.trace_mode=TraceMode::PathTraced; settings.samples_per_pixel=8;
  require(f.renderer->configure(settings),"path configuration"); f.renderer->reset_history();
  const auto single=f.draw(scene); f.renderer->reset_history(); require(f.draw(scene)==single,"same seed reproducible across runs");
  Fixture parallel(4); parallel.lighting=f.lighting; require(parallel.renderer->configure(settings),"parallel configure");
  require(parallel.draw(scene)==single,"thread scheduling does not affect pixels");
  Material glass; glass.transmission=1; glass.index_of_refraction=1.5f; glass.double_sided=false;
  const auto glass_frame=f.draw({{&plane,translate({0,0,-1}),emission({0,1,0})},{&box,Mat4::identity(),glass}});
  require(pixel(glass_frame).y>150,"single-sided closed glass transmits through exit face");
}
}
int main(int argc,char** argv) {
  (void)argc; (void)argv;
  SDL_setenv("SDL_VIDEODRIVER","dummy",1);
  if(SDL_Init(SDL_INIT_VIDEO)!=0) return 1;
  try { coverage_materials(); depth_and_instances(); tlas_order_invariance(); oblique_glass_exit(); history_and_validation(); glossy_dielectric_reflections(); shadows_and_path_determinism(); }
  catch(const std::exception& e) { std::cerr<<"CPU ray test: "<<e.what()<<"\n"; SDL_Quit(); return 1; }
  SDL_Quit(); std::cout<<"CPU ray/path tracing regression tests passed\n"; return 0;
}
