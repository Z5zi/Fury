#include "fury/material_sampling.hpp"
#include "fury/renderer.hpp"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace fury;
namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
bool close(float a,float b,float tolerance=1e-5f) { return std::fabs(a-b)<=tolerance; }
bool close(Vec2 a,Vec2 b,float tolerance=1e-5f) { return close(a.x,b.x,tolerance)&&close(a.y,b.y,tolerance); }
RgbaImage solid(int width,int height,std::uint8_t r,std::uint8_t g,std::uint8_t b,std::uint8_t alpha=255) {
  RgbaImage image{width,height,{}}; image.pixels.resize(std::size_t(width)*height*4);
  for(std::size_t i=0;i<image.pixels.size();i+=4) {
    image.pixels[i]=r; image.pixels[i+1]=g; image.pixels[i+2]=b; image.pixels[i+3]=alpha;
  }
  return image;
}
std::vector<RgbaImage> additional_mips(const RgbaImage& image,TextureEncoding encoding) {
  auto chain=build_mip_chain(image,encoding);
  if(!chain.empty()) chain.erase(chain.begin());
  return chain;
}
void sampler_channels() {
  const RgbaImage contrast{2,1,{0,0,0,0,255,255,255,255}};
  const auto linear_mips=additional_mips(contrast,TextureEncoding::SRGB);
  require(linear_mips.size()==1 && linear_mips[0].width==1,"Mip vector begins at additional half-size level one");
  const auto midpoint=sample_material_texture_lod(contrast,{}, {.5f,.5f},true,0);
  require(close(midpoint.x,.5f)&&close(midpoint.w,.5f),"Decode sRGB before bilinear interpolation; alpha stays linear");
  const auto mip=sample_material_texture_lod(contrast,linear_mips,{.25f,.5f},true,1);
  require(std::fabs(mip.x-.5f)<.004f && std::fabs(mip.w-.5f)<.003f,"Color mip averages linear light and alpha independently");
  const auto between=sample_material_texture_lod(contrast,linear_mips,{.25f,.5f},true,.5f);
  require(close(between.x,mip.x*.5f)&&close(between.w,mip.w*.5f),"Trilinear interpolation is also in linear light");
  const auto original=sample_material_texture_lod(contrast,{}, {.25f,.5f},true,20);
  require(close(original.x,0),"Empty mip chain preserves the original bilinear sample");
  const auto wrapped=sample_material_texture_lod(contrast,{}, {-.75f,3.5f},true,0);
  require(close(wrapped.x,0),"Negative UVs repeat at texel centers");
  const auto invalid=sample_material_texture_lod(contrast,{}, {std::numeric_limits<float>::quiet_NaN(),0},true,0);
  require(close(invalid.x,1),"Nonfinite coordinates return the documented white fallback");
  auto malformed=linear_mips; malformed[0].pixels.clear();
  require(close(sample_material_texture_lod(contrast,malformed,{.25f,.5f},true,5).x,0),"Malformed mip chain safely falls back to last valid level");
  malformed={solid(2,1,255,255,255)};
  require(close(sample_material_texture_lod(contrast,malformed,{.25f,.5f},true,5).x,0),"Duplicate level zero is not silently accepted as a mip");
  const RgbaImage mr{2,1,{32,0,255,255,96,255,0,255}};
  const auto data=sample_material_texture_lod(mr,additional_mips(mr,TextureEncoding::Linear),{.1f,.5f},false,8);
  require(std::fabs(data.x-64/255.f)<.004f&&std::fabs(data.y-.5f)<.004f&&std::fabs(data.z-.5f)<.004f,
          "Roughness G, metalness B and data R are filtered without color transfer");
  const RgbaImage normals{2,1,{230,128,204,255,25,128,204,255}};
  const auto normal=sample_material_texture_lod(normals,additional_mips(normals,TextureEncoding::Normal),{.1f,.5f},false,1);
  const Vec3 decoded=normalize({normal.x*2-1,normal.y*2-1,normal.z*2-1});
  require(std::fabs(decoded.x)<.015f&&decoded.z>.999f,"Opposing normal slopes minify to a renormalized flat normal");
  const auto infinity=sample_material_texture_lod(contrast,linear_mips,{.25f,.5f},true,std::numeric_limits<float>::infinity());
  require(close(infinity.x,mip.x),"Unbounded footprint safely clamps to last mip");
}
void world_projection_and_frames() {
  const Vec3 p{3,5,7};
  require(close(world_planar_projection(p,{0,1,0},2).uv,{6,14}),"Floor tiles use world X/Z in tiles per metre");
  require(close(world_planar_projection(p,{0,0,-1},2).uv,{6,-10}),"Z walls use world X/-Y independent of face sign");
  require(close(world_planar_projection(p,{-1,0,0},2).uv,{14,-10}),"X walls use world Z/-Y independent of face sign");
  require(close(world_planar_projection(p,{1,1,1},2).uv,{6,14}),"Dominant-axis ties choose Y consistently");
  for(Vec3 n:{Vec3{0,1,0},Vec3{0,-1,0},Vec3{0,0,1},Vec3{0,0,-1},Vec3{1,0,0},Vec3{-1,0,0},normalize(Vec3{.3f,.9f,.1f})}) {
    const auto projection=world_planar_projection(p,n,3);
    const auto frame=orthonormalize_material_frame(n,projection.frame);
    require(std::fabs(dot(frame.tangent,n))<1e-5f&&std::fabs(dot(frame.bitangent,n))<1e-5f,
            "Normal-map tangent frame is reorthogonalized against shading normal");
    require(dot(frame.tangent,projection.frame.tangent)>.9f && dot(frame.bitangent,projection.frame.bitangent)>.9f,
            "Normal-map U/V orientation survives negative normals and mirrored surfaces");
    require(close(length(frame.tangent),1)&&close(length(frame.bitangent),1),"Tangent frame has unit-length axes");
  }
  const Mat4 model=translate({11,4,-8})*rotate_y(.7f)*scale({-2,3,.5f});
  const Vec3 a=transform_point(model,{-1,0,0}),b=transform_point(model,{1,0,0});
  const Vec2 ua=world_planar_projection(a,{0,1,0},2).uv,ub=world_planar_projection(b,{0,1,0},2).uv;
  require(close(std::hypot(ub.x-ua.x,ub.y-ua.y),length(b-a)*2,1e-4f),
          "Translated, rotated, nonuniform and mirrored floors retain physical tile density");
}
void footprint_math() {
  const RgbaImage image=solid(512,256,0,0,0);
  require(close(material_texture_lod(image,{{1/512.f,0},{0,1/256.f}}),0),"One texel per pixel selects level zero");
  require(close(material_texture_lod(image,{{8/512.f,0},{0,8/256.f}}),3),"Eight texels per pixel selects level three");
  const Vec2 n{.8f,.3f},nx{.02f,-.005f},ny{-.003f,.04f};
  const float iw=.4f,ix=-.012f,iy=.005f;
  const Vec2 uv{n.x/iw,n.y/iw};
  const auto derivative=perspective_texture_footprint(uv,nx,ny,iw,ix,iy);
  auto at=[&](float x,float y) { return Vec2{(n.x+x*nx.x+y*ny.x)/(iw+x*ix+y*iy),
                                           (n.y+x*nx.y+y*ny.y)/(iw+x*ix+y*iy)}; };
  const float h=.002f; const auto xp=at(h,0),xm=at(-h,0),yp=at(0,h),ym=at(0,-h);
  require(close(derivative.dx,{(xp.x-xm.x)/(2*h),(xp.y-xm.y)/(2*h)},.0001f)&&
          close(derivative.dy,{(yp.x-ym.x)/(2*h),(yp.y-ym.y)/(2*h)},.0001f),
          "Perspective footprint differentiates the quotient instead of affine UVs");
  const auto head_on=ray_cone_texture_footprint({2,0,0},{0,3,0},{2,0},{0,3},{0,0,-1},.02f);
  const auto glancing=ray_cone_texture_footprint({2,0,0},{0,3,0},{2,0},{0,3},normalize(Vec3{.99f,0,-.1f}),.02f);
  require(material_texture_lod(image,glancing)>material_texture_lod(image,head_on)+3,
          "Ray footprint widens on grazing surfaces");
  const auto wider=ray_cone_texture_footprint({-4,0,0},{0,6,0},{-4,0},{0,6},{0,0,-1},.04f);
  require(close(material_texture_lod(image,wider),material_texture_lod(image,head_on)+1),
          "Cone growth changes LOD while physical scale survives nonuniform mirrored edges");
}
constexpr int width=96,height=64;
Mesh quad() {
  Mesh mesh;
  mesh.vertices={{{-1.45f,-.95f,0},{0,0,1},{1,1,1},{0,0}},{{1.45f,-.95f,0},{0,0,1},{1,1,1},{17.37f,0}},
                 {{1.45f,.95f,0},{0,0,1},{1,1,1},{17.37f,11.83f}},{{-1.45f,.95f,0},{0,0,1},{1,1,1},{0,11.83f}}};
  mesh.indices={0,1,2,0,2,3}; return mesh;
}
struct Fixture {
  SDL_Window* window{};
  std::unique_ptr<IRenderBackend> backend;
  Fixture(bool ray) {
    window=SDL_CreateWindow("Surface sampling",0,0,width,height,SDL_WINDOW_HIDDEN);
    require(window!=nullptr,"Create hidden SDL window");
    backend=ray?create_cpu_ray_backend():create_software_backend();
    require(backend->create(window,width,height),"Create CPU backend");
    if(ray) {
      auto settings=backend->settings(); settings.samples_per_pixel=1; settings.denoise=false;
      settings.accumulate=false; settings.trace_mode=TraceMode::RayTraced;
      require(backend->configure(settings),"Configure deterministic ray fixture");
    }
  }
  ~Fixture() { backend.reset(); SDL_DestroyWindow(window); }
  std::vector<std::uint8_t> draw(const Mesh& mesh,const Material& material,const Mat4& model=Mat4::identity(),bool perspective_view=false) {
    backend->begin_frame({0,0,0,255});
    backend->set_view_proj(look_at({0,0,4},{0,0,0},{0,1,0}),perspective_view?
        perspective(radians(40),1.5f,.1f,20):orthographic(-1.5f,1.5f,-1,1,.1f,20));
    backend->set_camera_position({0,0,4}); backend->set_time(0);
    Lighting lighting; lighting.sun_intensity=0; lighting.ambient={0,0,0}; lighting.ao_strength=0;
    lighting.enable_bloom=false; lighting.enable_reflections=false; lighting.fog_start=lighting.fog_end=0;
    backend->set_lighting(lighting); backend->draw_mesh(mesh,model,material);
    std::vector<std::uint8_t> image; int w=0,h=0;
    require(backend->read_rgb_framebuffer(image,w,h)&&w==width&&h==height,"Read CPU fixture framebuffer");
    return image;
  }
};
float variance(const std::vector<std::uint8_t>& image) {
  double sum=0,sum2=0,count=0;
  for(int y=20;y<height-20;++y) for(int x=28;x<width-28;++x) {
    const double v=image[(y*width+x)*3]; sum+=v; sum2+=v*v; ++count;
  }
  return float(sum2/count-(sum/count)*(sum/count));
}
void runtime_filtering(bool ray) {
  Fixture f(ray); auto mesh=quad();
  auto maps=std::make_shared<MaterialTextures>(); maps->emissive=solid(512,512,0,0,0);
  for(int y=0;y<512;++y) for(int x=0;x<512;++x) {
    const auto i=(y*512+x)*4; const auto value=std::uint8_t(((x/2+y/2)&1)?255:0);
    for(int c=0;c<3;++c) maps->emissive.pixels[i+c]=value;
  }
  Material material; material.albedo={0,0,0}; material.emissive=1; material.textures=maps;
  for(bool perspective_view:{false,true}) {
    const Mat4 model=perspective_view?rotate_y(.5f):Mat4::identity();
    maps->emissive_mips.clear();
    const auto aliased=f.draw(mesh,material,model,perspective_view);
    maps->emissive_mips=additional_mips(maps->emissive,TextureEncoding::SRGB);
    const auto filtered=f.draw(mesh,material,model,perspective_view);
    require(variance(aliased)>100,"Synthetic severe minification fixture exposes unresolved aliasing");
    require(variance(filtered)<1 && variance(filtered)<variance(aliased)*.01f,
            "Actual CPU renderer mip sampling suppresses severe minification aliasing");
  }
  // The same visible world surface must sample identically when transforms are
  // baked into vertices, and regardless of arbitrary authored UV coordinates.
  maps->emissive={2,2,{255,30,10,255,10,255,50,255,30,20,255,255,240,180,20,255}};
  maps->emissive_mips.clear(); material.world_uv_scale=2.75f;
  for(const Mat4& model:{translate({.17f,-.11f,0})*scale({.8f,.7f,1}),
                         translate({-.13f,.08f,-.2f})*rotate_y(.55f)*scale({-.8f,.7f,1})}) {
    auto baked=mesh; Mat4 inv; require(inverse(model,inv),"Invert fixture transform");
    for(auto& vertex:baked.vertices) {
      vertex.position=transform_point(model,vertex.position);
      vertex.normal=transform_direction(transpose(inv),vertex.normal);
      vertex.uv={193,-117};
    }
    const auto instanced=f.draw(mesh,material,model),world=f.draw(baked,material);
    double difference=0; for(std::size_t i=0;i<world.size();++i) difference+=std::abs(int(world[i])-int(instanced[i]));
    require(difference/world.size()<.05,"Runtime world mapping is invariant to baked transforms, translation, mirroring and authored UVs");
  }
  if(ray) {
    material.world_uv_scale=1; f.draw(mesh,material);
    auto settings=f.backend->settings(); settings.accumulate=true; require(f.backend->configure(settings),"Enable history test");
    f.draw(mesh,material); f.draw(mesh,material);
    require(f.backend->statistics().accumulated_frames==2,"Stable world mapping accumulates history");
    material.world_uv_scale=2; f.draw(mesh,material);
    require(f.backend->statistics().accumulated_frames==1,"World physical scale edits reset ray history");
    material.detail_texture=TextureSlot::Brick; f.draw(mesh,material);
    require(f.backend->statistics().accumulated_frames==1,"Detail profile edits reset ray history");
  }
}
}
int main(int argc,char** argv) {
  (void)argc; (void)argv;
  SDL_setenv("SDL_VIDEODRIVER","dummy",1); SDL_setenv("FURY_CPU_THREADS","2",1);
  if(SDL_Init(SDL_INIT_VIDEO)!=0) return 1;
  try { sampler_channels(); world_projection_and_frames(); footprint_math(); runtime_filtering(false); runtime_filtering(true); }
  catch(const std::exception& e) { std::cerr<<"Surface sampling: "<<e.what()<<'\n'; SDL_Quit(); return 1; }
  SDL_Quit(); std::cout<<"World surface projection and CPU mip filtering tests passed\n"; return 0;
}
