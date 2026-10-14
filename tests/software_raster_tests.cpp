#include "fury/renderer.hpp"
#include "fury/texture.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace fury;
namespace {
constexpr int size=64;
void require(bool condition,const char* message) {
  if(!condition) throw std::runtime_error(message);
}
int display(float linear) {
  return static_cast<int>(std::pow(linear/(1.f+linear),1.f/2.2f)*255.f+.5f);
}
float linear(std::uint8_t value) {
  const float c=value/255.f;
  return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);
}
Mesh triangle(Vec3 a={-.8f,-.8f,0},Vec3 b={.8f,-.8f,0},Vec3 c={0,.8f,0}) {
  Mesh mesh;
  mesh.vertices={ {a,{0,0,1},{1,1,1},{0,0}},
                  {b,{0,0,1},{1,1,1},{1,0}},
                  {c,{0,0,1},{1,1,1},{.5f,1}} };
  mesh.indices={0,1,2}; return mesh;
}
Mesh quad(float z) {
  Mesh mesh;
  mesh.vertices={{{-.75f,-.75f,z},{0,0,1},{1,1,1},{0,1}},
                 {{ .75f,-.75f,z},{0,0,1},{1,1,1},{1,1}},
                 {{ .75f, .75f,z},{0,0,1},{1,1,1},{1,0}},
                 {{-.75f, .75f,z},{0,0,1},{1,1,1},{0,0}}};
  mesh.indices={0,1,2,0,2,3}; return mesh;
}
struct Fixture {
  SDL_Window* window{};
  std::unique_ptr<IRenderBackend> backend;
  Lighting light;
  Fixture() {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    require(SDL_Init(SDL_INIT_VIDEO)==0,"Initialize SDL dummy video driver");
    window=SDL_CreateWindow("CPU raster tests",0,0,size,size,SDL_WINDOW_HIDDEN);
    require(window!=nullptr,"Create hidden software-rendering window");
    backend=create_software_backend();
    require(backend && backend->create(window,size,size),"Create CPU software backend");
    SDL_RendererInfo info{};
    require(SDL_GetRendererInfo(SDL_GetRenderer(window),&info)==0,"Inspect presentation renderer");
    require((info.flags&SDL_RENDERER_SOFTWARE)!=0 && (info.flags&SDL_RENDERER_ACCELERATED)==0,
            "Presentation must be genuinely CPU software-rendered");
    light.ambient={1,1,1}; light.sun_intensity=0; light.ao_strength=0;
    light.fog_start=light.fog_end=0; light.enable_bloom=false;
    light.enable_reflections=false; light.point_light_count=0;
    reset();
  }
  ~Fixture() { backend.reset(); if(window) SDL_DestroyWindow(window); SDL_Quit(); }
  void reset() {
    backend->begin_frame({0,0,0,255});
    backend->set_lighting(light);
    backend->set_view_proj(Mat4::identity(),Mat4::identity());
    backend->set_camera_position({0,0,3}); backend->set_time(0);
  }
  std::vector<std::uint8_t> read() {
    std::vector<std::uint8_t> result; int w=0,h=0;
    require(backend->read_rgb_framebuffer(result,w,h),"Read CPU framebuffer");
    require(w==size && h==size,"Framebuffer dimensions"); return result;
  }
  std::array<int,3> pixel(int x=32,int y=32) {
    const auto image=read(); const auto offset=(y*size+x)*3;
    return {image[offset],image[offset+1],image[offset+2]};
  }
  int coverage() {
    const auto image=read(); int result=0;
    for(std::size_t i=0;i<image.size();i+=3) if(image[i] || image[i+1] || image[i+2]) ++result;
    return result;
  }
  void draw(const Mesh& mesh,const Material& material={},const Mat4& model=Mat4::identity()) {
    backend->draw_mesh(mesh,model,material);
  }
};
void clipping_tests(Fixture& f) {
  // Every homogeneous plane must reject fully outside geometry, and retain
  // the visible portion of a crossing triangle (including near and far).
  for(int plane=0;plane<6;++plane) {
    auto coordinate=[&](Vec3& v,float value) {
      if(plane<2) v.x=value; else if(plane<4) v.y=value; else v.z=value;
    };
    const float outside=(plane%2==0)?-2.f:2.f;
    Mesh mesh=triangle();
    for(auto& v:mesh.vertices) {
      const float original=plane<2?v.position.x:(plane<4?v.position.y:v.position.z);
      coordinate(v.position,original+outside);
    }
    f.reset(); f.draw(mesh);
    require(f.coverage()==0,"Fully outside triangles rejected at every clip plane");
    mesh=triangle(); coordinate(mesh.vertices[0].position,outside);
    f.reset(); f.draw(mesh);
    require(f.coverage()>50,"Crossing triangle retained at every clip plane");
  }
  f.reset(); f.backend->set_view_proj(Mat4::identity(),perspective(radians(90),1,.1f,10));
  f.backend->set_camera_position({0,0,0});
  f.draw(triangle({-.8f,-.6f,-1},{.8f,-.6f,-1},{0,.8f,.2f}));
  require(f.coverage()>100,"Triangle with one vertex behind camera is homogeneously clipped, not dropped");
  for(float z:{.2f,-.05f,-11.f}) {
    f.reset(); f.backend->set_view_proj(Mat4::identity(),perspective(radians(90),1,.1f,10));
    f.draw(triangle({-.8f,-.6f,z},{.8f,-.6f,z},{0,.8f,z}));
    require(f.coverage()==0,"Behind camera, before near, and beyond far are rejected");
  }
  // Clipping must interpolate all varyings at newly created edge vertices.
  Mesh crossing=triangle({-.8f,-.8f,-2},{.8f,-.8f,0},{0,.8f,0});
  crossing.vertices[0].color={1,0,0}; crossing.vertices[1].color={0,1,0};
  crossing.vertices[2].color={0,0,1};
  Mesh clipped;
  clipped.vertices={{{-.4f,0,-1},{0,0,1},{.5f,0,.5f},{.25f,.5f}},
                    {{0,-.8f,-1},{0,0,1},{.5f,.5f,0},{.5f,0}},
                    crossing.vertices[1],crossing.vertices[2]};
  clipped.indices={0,1,2,0,2,3};
  f.reset(); f.draw(crossing); const auto automatic=f.read();
  f.reset(); f.draw(clipped); const auto explicit_clip=f.read();
  for(std::size_t i=0;i<automatic.size();++i)
    require(std::abs(int(automatic[i])-int(explicit_clip[i]))<=1,
            "Homogeneous intersections interpolate vertex varyings consistently");
  f.reset(); f.draw(triangle({-10000,-10000,0},{10000,-10000,0},{0,10000,0}));
  require(f.coverage()==size*size,"Very large triangles clip safely to the entire viewport");
}
void depth_tests(Fixture& f) {
  Material red; red.albedo={1,0,0}; Material blue; blue.albedo={0,0,1};
  const Mesh near=quad(-.5f),far=quad(.5f);
  for(bool reverse:{false,true}) {
    f.reset();
    if(reverse) { f.draw(near,red); f.draw(far,blue); }
    else { f.draw(far,blue); f.draw(near,red); }
    const auto color=f.pixel();
    require(color[0]>180 && color[2]==0,"Opaque depth is independent of submission order");
  }
  // Match the same triangle under a perspective projection against a plane
  // at z=-1.8. At this pixel, affine NDC z places the triangle in front.
  f.reset(); f.backend->set_view_proj(Mat4::identity(),perspective(radians(90),1,.1f,10));
  f.backend->set_camera_position({0,0,0});
  f.draw(triangle({-1.8f,-1.8f,-1.8f},{1.8f,-1.8f,-1.8f},{0,1.8f,-1.8f}),blue);
  f.draw(triangle({-.75f,-.75f,-1},{3,-3,-4},{0,1.5f,-2}),red);
  const auto color=f.pixel(24,40);
  require(color[0]>180 && color[2]==0,"Perspective depth interpolates NDC z without a second 1/w divide");
}
void texture_tests(Fixture& f) {
  auto maps=std::make_shared<MaterialTextures>();
  maps->base_color={4,1,{0,0,0,255,80,80,80,255,160,160,160,255,240,240,240,255}};
  Material material; material.textures=maps;
  Mesh mesh=triangle({-.75f,-.75f,-1},{3,-3,-4},{0,1.5f,-2});
  mesh.vertices[0].uv={.125f,.5f}; mesh.vertices[1].uv={.875f,.5f}; mesh.vertices[2].uv={.5f,.5f};
  f.reset(); f.backend->set_view_proj(Mat4::identity(),perspective(radians(90),1,.1f,10));
  f.backend->set_camera_position({0,0,0}); f.draw(mesh,material);
  const float w2=(56.f-40.5f)/48.f;
  const float w1=(32.5f-8.f-w2*24.f)/48.f, w0=1.f-w1-w2;
  const float u=(w0*.125f+w1*.875f/4.f+w2*.5f/2.f)/(w0+w1/4.f+w2/2.f);
  const float texel=u*4.f-.5f;
  const int left=static_cast<int>(std::floor(texel));
  const float amount=texel-left;
  const int expected=display(linear(static_cast<std::uint8_t>(left*80))*(1.f-amount)+
                             linear(static_cast<std::uint8_t>((left+1)*80))*amount);
  require(std::abs(f.pixel(32,40)[0]-expected)<=1,"Per-pixel texture UVs are perspective correct and filtered in linear light");
  require(std::abs(f.pixel(32,40)[0]-f.pixel(20,40)[0])>12,"Texture detail survives between vertices");

  maps->base_color={2,1,{0,0,0,255,255,255,255,255}};
  mesh=quad(0); for(auto& v:mesh.vertices) v.uv={.5f,.5f};
  f.reset(); f.draw(mesh,material);
  require(std::abs(f.pixel()[0]-display(.5f))<=1,"sRGB bilinear filtering averages linear-light texels");
  for(auto& v:mesh.vertices) v.uv={-.75f,.5f};
  f.reset(); f.draw(mesh,material);
  require(f.pixel()[0]==0,"Negative UV repeat wraps correctly");
  material.uv_scroll_u=.5f; f.reset(); f.backend->set_time(1.f); f.draw(mesh,material);
  require(f.pixel()[0]==display(1.f),"UV animation affects per-pixel texture sampling");
}
void alpha_tests(Fixture& f) {
  auto maps=std::make_shared<MaterialTextures>();
  maps->base_color={1,1,{255,255,255,0}};
  Material mask; mask.textures=maps; mask.albedo={1,0,0}; mask.alpha_cutoff=.5f;
  Material blue; blue.albedo={0,0,1};
  f.reset(); f.draw(quad(-.5f),mask); f.draw(quad(.5f),blue);
  require(f.pixel()[0]==0 && f.pixel()[2]>180,"Discarded alpha-mask texels do not occlude background");
  maps->base_color.pixels[3]=255; mask.opacity=.6f;
  Mesh mesh=quad(-.5f); for(auto& v:mesh.vertices) v.opacity=.5f;
  f.reset(); f.draw(mesh,mask); f.draw(quad(.5f),blue);
  require(f.pixel()[2]>180,"Mask opacity combines vertex, material, and texture alpha");
  mask.alpha_cutoff=-1; mask.opacity=0;
  f.reset(); f.draw(mesh,mask); f.draw(quad(.5f),blue);
  require(f.pixel()[0]>180 && f.pixel()[2]==0,"OPAQUE mode ignores alpha as required by glTF");

  Material red; red.albedo={1,0,0}; red.alpha_blend=true; red.opacity=.5f;
  for(bool reverse:{false,true}) {
    f.reset();
    if(reverse) { f.draw(quad(-.5f),red); f.draw(quad(.5f),blue); }
    else { f.draw(quad(.5f),blue); f.draw(quad(-.5f),red); }
    const auto color=f.pixel();
    require(std::abs(color[0]-display(.5f))<=1 && std::abs(color[2]-display(.5f))<=1,
            "Transparent surface resolves after opaque draws and blends in linear light");
    require(f.pixel(31,32)==f.pixel(30,32),"Transparent shared diagonal is shaded once without a seam");
  }
  Material green=red; green.albedo={0,1,0};
  std::vector<std::uint8_t> reference;
  for(bool reverse:{false,true}) {
    f.reset();
    if(reverse) { f.draw(quad(.4f),red); f.draw(quad(-.4f),green); }
    else { f.draw(quad(-.4f),green); f.draw(quad(.4f),red); }
    auto image=f.read(); if(reference.empty()) reference=image;
    require(image==reference,"Transparent triangles sort back-to-front independent of mesh submission");
    const auto color=f.pixel();
    require(std::abs(color[0]-display(.25f))<=1 && std::abs(color[1]-display(.5f))<=1,
            "Transparent layering follows the same near/far ordering as the depth buffer");
  }
  f.reset(); f.draw(quad(.5f),red); f.draw(quad(-.5f),blue);
  require(f.pixel()[0]==0 && f.pixel()[2]>180,"Transparent surfaces respect opaque depth");
  f.reset(); f.draw(quad(0),red); f.backend->draw_hud_rect(24,24,16,16,{0,255,0,255});
  require(f.pixel()==std::array<int,3>{0,255,0},"HUD appears after deferred transparency");
}
void coverage_tests(Fixture& f) {
  Material single; single.double_sided=false;
  f.reset(); f.draw(quad(0),single); const auto front=f.read();
  f.reset(); f.draw(quad(0),single,scale({-1,1,1}));
  require(f.read()==front,"Mirrored single-sided node preserves authored front face");
  Material transparent; transparent.alpha_blend=true; transparent.opacity=.5f;
  for(int angle=0;angle<16;++angle) {
    const Mat4 model=translate({.015625f,.015625f,0})*rotate_z(angle*.071f)*scale({.83f,.71f,1});
    const std::array<std::array<std::uint32_t,6>,4> orders={{{0,1,2,0,2,3},
                                                          {2,0,1,3,0,2},
                                                          {2,1,0,3,2,0},
                                                          {0,2,3,0,1,2}}};
    std::vector<std::uint8_t> reference;
    for(const auto& order:orders) {
      Mesh mesh=quad(0); mesh.indices.assign(order.begin(),order.end());
      f.reset(); f.draw(mesh,transparent,model); const auto image=f.read();
      if(reference.empty()) reference=image;
      for(std::size_t i=0;i<image.size();++i) {
        require(std::abs(int(image[i])-int(reference[i]))<=1,
                "Rotated quad coverage is invariant under triangle index permutations");
        require(image[i]==0 || std::abs(int(image[i])-display(.5f))<=1,
                "Each transparent shared-edge sample is shaded exactly once");
      }
    }
  }
}
void shading_tests(Fixture& f) {
  const auto ambient=f.light;
  f.light.ambient={0,0,0}; f.light.sun_intensity=3; f.light.sun_color={1,1,1};
  const Mat4 transform=scale({2,.5f,1});
  Mesh original=triangle({-.3f,-.8f,0},{.3f,-.8f,0},{0,.8f,0});
  for(auto& v:original.vertices) v.normal=normalize({1,1,1});
  Mat4 inverse_transform; require(inverse(transform,inverse_transform),"Normal test transform invertible");
  Mesh transformed=original;
  for(auto& v:transformed.vertices) {
    v.position=transform_point(transform,v.position);
    v.normal=transform_direction(transpose(inverse_transform),v.normal);
  }
  f.light.sun_direction=-normalize(transformed.vertices[0].normal);
  f.reset(); f.draw(original,{},transform); const auto actual=f.read();
  f.reset(); f.draw(transformed); require(actual==f.read(),"Inverse-transpose normals preserve lighting under nonuniform scale");

  f.light.sun_direction={-1,0,-1};
  auto maps=std::make_shared<MaterialTextures>();
  maps->normal={1,1,{255,128,128,255}};
  Material material; material.textures=maps; material.normal_scale=1;
  Mesh mesh=quad(0);
  f.reset(); f.draw(mesh,material); const auto normal_x=f.pixel();
  for(auto& v:mesh.vertices) v.uv.x=1.f-v.uv.x;
  f.reset(); f.draw(mesh,material); const auto mirrored=f.pixel();
  require(normal_x[0]>mirrored[0]+60,"Normal map uses UV-derived tangent frame and mirrored UV handedness");
  material.normal_scale=0;
  f.reset(); f.draw(mesh,material); require(f.pixel()[0]>mirrored[0]+30,"normal_scale zero restores geometric normal");

  f.light.ambient={0,0,0}; f.light.sun_intensity=0;
  material.albedo={0,0,0}; material.emissive=1; material.emissive_color={.5f,1,.25f};
  maps->emissive={1,1,{255,128,0,255}};
  f.reset(); f.draw(mesh,material);
  const auto emission=f.pixel();
  require(std::abs(emission[0]-display(.5f))<=1 && std::abs(emission[1]-display(linear(128)))<=1 && emission[2]==0,
          "Imported emissive factor and sRGB map are independent of base color");

  f.light=ambient; maps->normal={}; maps->emissive={}; material.emissive=0;
  material.albedo={1,1,1}; material.metallic=1;
  maps->metallic_roughness={1,1,{0,255,0,255}};
  f.reset(); f.draw(mesh,material); const int dielectric=f.pixel()[0];
  maps->metallic_roughness.pixels[2]=255;
  f.reset(); f.draw(mesh,material);
  require(dielectric>f.pixel()[0]+50,"Metallic factor uses blue channel of imported metallic-roughness map");
  f.light.ambient={0,0,0}; f.light.sun_direction={0,0,-1}; f.light.sun_intensity=1;
  material.roughness=1; maps->metallic_roughness.pixels[1]=255;
  f.reset(); f.draw(mesh,material); const int rough=f.pixel()[0];
  maps->metallic_roughness.pixels[1]=64;
  f.reset(); f.draw(mesh,material);
  require(f.pixel()[0]>rough+60,"Roughness factor uses green channel of imported metallic-roughness map");
  f.light=ambient;
}
void normal_facing_tests(Fixture& f) {
  const auto saved_light=f.light;
  f.light.ambient={.04f,.04f,.04f}; f.light.sun_intensity=3;
  f.light.sun_color={1,1,1};
  // A concave smooth surface can face the camera geometrically while its
  // interpolated normal points away. Flipping that normal lights the wrong side.
  for(bool clipped:{false,true}) for(bool mirrored:{false,true}) for(bool blended:{false,true}) {
    Mesh front=triangle({clipped?-1.6f:-.6f,-.8f,clipped?1.6f:.6f},
                        {.6f,-.8f,-.6f},{0,.8f,0});
    front.vertices[0].normal=normalize({1,-.12f,-.3f});
    front.vertices[1].normal=normalize({1,.12f,-.4f});
    front.vertices[2].normal=normalize({1,0,-.35f});
    const Vec3 geometric=normalize(cross(front.vertices[1].position-front.vertices[0].position,
                                        front.vertices[2].position-front.vertices[0].position));
    for(const auto& v:front.vertices)
      require(dot(geometric,v.normal)>0,"Concave smooth normals remain in the geometric front hemisphere");
    const float reflection=mirrored?-1.f:1.f;
    const Mat4 model=scale({reflection,1,1});
    f.light.sun_direction={reflection,0,-1};
    Material single; single.double_sided=false;
    single.alpha_blend=blended; single.opacity=blended?.5f:1.f;
    Material both=single; both.double_sided=true;
    f.reset(); f.draw(front,single,model); const auto authored=f.read();
    const auto dark=f.pixel();
    require(std::abs(dark[0]-display(.04f*single.opacity))<=1,
            "Front-facing concave test pixel receives ambient light only");
    f.reset(); f.draw(front,both,model);
    require(f.read()==authored,
            "Double-sided front faces preserve smooth normals through clipping, mirroring and transparency");

    Mesh back=front; std::swap(back.indices[1],back.indices[2]);
    f.reset(); f.draw(back,single,model);
    require(f.coverage()==0,"Single-sided geometric back faces are culled regardless of smooth normals");
    f.reset(); f.draw(back,both,model); const auto reversed=f.read();
    require(f.pixel()[0]>dark[0]+80,"Double-sided geometric back faces reverse their smooth normals");
    // Independently authored reversed normals on the original front face give
    // the expected back-face lighting, without asking shade to choose a side.
    for(auto& v:front.vertices) v.normal=-v.normal;
    f.reset(); f.draw(front,single,model);
    const auto expected=f.read();
    for(std::size_t i=0;i<expected.size();++i)
      require(std::abs(int(reversed[i])-int(expected[i]))<=1,
              "Reversed winding lighting matches explicitly reversed authored normals");
  }
  // Perspective clipping can split a triangle that crosses the eye/near plane
  // into a visible fan. Every resulting front must keep the same shading side.
  Mesh crossing=triangle({-.8f,-.6f,-1},{.8f,-.6f,-1},{0,.8f,.2f});
  for(auto& v:crossing.vertices) v.normal=normalize({0,-1,-.3f});
  f.light.sun_direction={0,-1,-.3f};
  Material single; single.double_sided=false;
  Material both=single; both.double_sided=true;
  auto draw_perspective=[&](const Mesh& mesh,const Material& material) {
    f.reset(); f.backend->set_view_proj(Mat4::identity(),perspective(radians(90),1,.1f,10));
    f.backend->set_camera_position({0,0,0}); f.draw(mesh,material);
  };
  draw_perspective(crossing,single); const auto authored=f.read();
  require(f.coverage()>100,"Eye-plane-crossing front face retains visible clipped geometry");
  draw_perspective(crossing,both);
  require(f.read()==authored,"Perspective near-plane clipping preserves geometric front-face shading");
  std::swap(crossing.indices[1],crossing.indices[2]);
  draw_perspective(crossing,single);
  require(f.coverage()==0,"Perspective-clipped single-sided back face remains culled");
  f.light=saved_light;
}
void robustness_tests(Fixture& f) {
  f.reset(); Mesh invalid=triangle(); invalid.indices[0]=999999;
  f.draw(invalid); require(f.coverage()==0,"Out-of-range mesh indices are rejected safely");
  invalid=triangle(); invalid.vertices[0].position.x=std::numeric_limits<float>::quiet_NaN();
  f.draw(invalid); require(f.coverage()==0,"Nonfinite vertex positions do not corrupt raster bounds");
  f.draw(triangle({0,0,0},{0,0,0},{0,0,0}));
  require(f.coverage()==0,"Degenerate triangles are skipped");
  Material one_sided; one_sided.double_sided=false;
  Mesh mesh=triangle(); f.draw(mesh,one_sided); require(f.coverage()>100,"CCW face is front-facing");
  f.reset(); std::swap(mesh.indices[1],mesh.indices[2]); f.draw(mesh,one_sided);
  require(f.coverage()==0,"Single-sided reversed winding is culled");
  one_sided.double_sided=true; f.draw(mesh,one_sided);
  require(f.coverage()>100,"Double-sided material retains reversed winding");
  f.reset();
  f.backend->draw_hud_rect(std::numeric_limits<float>::quiet_NaN(),0,8,8,{255,0,0,255});
  require(f.coverage()==0,"Nonfinite HUD coordinates are ignored safely");
  f.backend->draw_hud_rect(-std::numeric_limits<float>::max(),0,
                           std::numeric_limits<float>::max(),8,{255,0,0,255});
  require(f.coverage()==0,"Off-screen extreme HUD coordinates clamp before integer conversion");
  Material invalid_material; invalid_material.alpha_blend=true;
  invalid_material.opacity=std::numeric_limits<float>::quiet_NaN();
  f.draw(quad(0),invalid_material);
  require(f.coverage()==0,"Nonfinite blended alpha cannot corrupt the framebuffer");
  // Statistics count shared geometry once and report actual CPU presentation.
  f.reset(); const auto shared=quad(0);
  f.draw(shared); f.draw(shared,{},translate({.1f,0,.1f}));
  f.backend->end_frame(); // Exercise the actual CPU upload/present path under dummy video.
  const auto stats=f.backend->statistics();
  require(stats.triangle_count==4 && stats.unique_triangle_count==2 && stats.instance_count==2,
          "Raster statistics distinguish submitted instances from shared geometry");
  require(stats.render_width==size && stats.render_height==size && stats.output_width==size && stats.output_height==size,
          "Raster statistics report actual framebuffer dimensions");
  require(stats.cpu_threads==1 && !stats.hardware_ray_tracing && !stats.software_ray_tracing &&
          stats.cpu_frame_ms>=0 && !stats.adapter.empty(),"Raster statistics describe the CPU backend honestly");
}
}
int main(int argc, char** argv) {
  (void)argc; (void)argv;
  try {
    Fixture fixture;
    clipping_tests(fixture); depth_tests(fixture); texture_tests(fixture);
    alpha_tests(fixture); coverage_tests(fixture); shading_tests(fixture);
    normal_facing_tests(fixture); robustness_tests(fixture);
    std::cout<<"CPU software raster clipping, depth, materials, alpha, normal and presentation checks passed\n";
    return 0;
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
