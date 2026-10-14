#include "fury/renderer.hpp"
#include "fury/gl_loader.hpp"
#include "fury/texture.hpp"

#include <SDL.h>

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace fury;
namespace {
constexpr int size = 64;
void require(bool result, const char* message) {
  if (!result) throw std::runtime_error(message);
}
int display(float value) { return int(std::pow(value/(1+value),1.f/2.2f)*255+.5f); }
float linear(int byte) {
  const float value=byte/255.f;
  return value<=.04045f ? value/12.92f : std::pow((value+.055f)/1.055f,2.4f);
}
RgbaImage solid(int r,int g,int b,int a=255) {
  return {1,1,{std::uint8_t(r),std::uint8_t(g),std::uint8_t(b),std::uint8_t(a)}};
}
Mesh quad() {
  Mesh mesh;
  mesh.vertices={{{-.9f,-.9f,0},{0,0,1},{1,1,1},{0,1}},
                 {{ .9f,-.9f,0},{0,0,1},{1,1,1},{1,1}},
                 {{ .9f, .9f,0},{0,0,1},{1,1,1},{1,0}},
                 {{-.9f, .9f,0},{0,0,1},{1,1,1},{0,0}}};
  mesh.indices={0,1,2,0,2,3};
  return mesh;
}
struct Fixture {
  SDL_Window* window{};
  std::unique_ptr<IRenderBackend> backend;
  Lighting light;
  Mesh mesh=quad();
  Fixture(SDL_Window* input) : window(input),backend(create_gl_backend()) {
    require(backend->create(window,size,size),"GL backend/shaders must initialize after a successful context probe");
    light.ambient={1,1,1}; light.sun_intensity=0; light.ao_strength=0;
    light.fog_start=100; light.fog_end=200; light.point_light_count=0;
    light.enable_bloom=false; light.enable_shadows=false; light.enable_reflections=false;
    reset();
  }
  void reset() {
    backend->set_view_proj(Mat4::identity(),Mat4::identity());
    backend->set_camera_position({0,0,3}); backend->set_lighting(light); backend->set_time(0);
    backend->begin_frame({0,0,0,255});
  }
  void draw(const Material& material,const Mat4& model=Mat4::identity()) {
    backend->draw_mesh(mesh,model,material);
  }
  std::vector<std::uint8_t> image() {
    int w=0,h=0; std::vector<std::uint8_t> pixels;
    require(backend->read_rgb_framebuffer(pixels,w,h),"Read GL framebuffer");
    require(w==size && h==size,"GL framebuffer size"); return pixels;
  }
  std::array<int,3> pixel(int x=32,int y=32) {
    const auto pixels=image(); const auto i=std::size_t(y*size+x)*3;
    return {pixels[i],pixels[i+1],pixels[i+2]};
  }
};
void color_and_emission(Fixture& f) {
  Material material;
  auto maps=std::make_shared<MaterialTextures>(); maps->base_color=solid(128,64,32);
  material.textures=maps;
  f.reset(); f.draw(material); const auto color=f.pixel();
  for(int c=0;c<3;++c)
    require(std::abs(color[c]-display(linear(maps->base_color.pixels[c])))<=2,
            "Albedo channels must be decoded from sRGB before lighting");

  maps=std::make_shared<MaterialTextures>(); maps->base_color=solid(0,0,0);
  maps->emissive=solid(128,64,0); material.textures=maps;
  material.emissive=2; material.emissive_color={2,.5f,1};
  f.reset(); f.draw(material); const auto emission=f.pixel();
  require(std::abs(emission[0]-display(linear(128)*4))<=2 &&
          std::abs(emission[1]-display(linear(64)))<=2 && emission[2]==0,
          "Imported emissive map is sRGB and independent of black base color");
  // Cache hits and absent imported channels may never reuse the previous draw's maps.
  material={}; f.reset(); f.draw(material);
  require(std::abs(f.pixel()[0]-display(1))<=2,"Absent imported maps restore white legacy fallback");
}
void packed_surface_and_normals(Fixture& f) {
  f.light.ambient={0,0,0}; f.light.sun_direction={0,0,-1};
  f.light.sun_color={1,1,1}; f.light.sun_intensity=1;
  Material material; material.albedo={0,0,0}; material.roughness=1;
  auto maps=std::make_shared<MaterialTextures>(); maps->metallic_roughness=solid(255,128,0);
  material.textures=maps; f.reset(); f.draw(material); const int rough=f.pixel()[0];
  const float expected=.04f*(1-(128/255.f)*.85f);
  require(std::abs(rough-display(expected))<=3,"Roughness uses linear green channel");
  maps=std::make_shared<MaterialTextures>(); maps->metallic_roughness=solid(0,128,0);
  material.textures=maps; f.reset(); f.draw(material);
  require(std::abs(rough-f.pixel()[0])<=1,"Packed surface red channel is not roughness or metallic");

  material.albedo={.5f,.5f,.5f}; material.metallic=1;
  maps=std::make_shared<MaterialTextures>(); maps->metallic_roughness=solid(0,255,128);
  material.textures=maps; f.reset(); f.draw(material);
  const float metal=128/255.f, spec=.04f*(1-metal)+.5f*metal;
  require(std::abs(f.pixel()[0]-display(.5f*(1-.9f*metal)+spec*.15f))<=3,
          "Metallic uses linear blue channel");

  f.light.sun_direction={-1,0,0}; material={}; material.roughness=1;
  maps=std::make_shared<MaterialTextures>(); maps->normal=solid(255,128,128);
  material.textures=maps; f.reset(); f.draw(material); const int tilted=f.pixel()[0];
  material.normal_scale=0; f.reset(); f.draw(material);
  require(tilted>150 && f.pixel()[0]<20,"Normal-map XY scale changes real lighting");
  // Mirrored mesh U must reverse the tangent-space X lobe.
  material.normal_scale=1;
  for(auto& vertex:f.mesh.vertices) vertex.uv.x=1-vertex.uv.x;
  f.mesh.mark_dirty(); f.reset(); f.draw(material);
  require(f.pixel()[0]<10,"Mirrored imported UVs preserve tangent handedness");
  for(auto& vertex:f.mesh.vertices) vertex.uv.x=1-vertex.uv.x;
  f.mesh.mark_dirty();
  f.light.ambient={1,1,1}; f.light.sun_intensity=0;
}
void alpha_and_winding(Fixture& f) {
  auto maps=std::make_shared<MaterialTextures>(); maps->base_color=solid(255,255,255,128);
  Material material; material.textures=maps; material.alpha_cutoff=.6f;
  f.reset(); f.draw(material); require(f.pixel()[0]==0,"Alpha mask discards below cutoff");
  material.alpha_cutoff=.4f;
  f.reset(); f.draw(material); require(f.pixel()[0]>180,"Alpha mask retains above cutoff");
  material.opacity=.5f;
  f.reset(); f.draw(material); require(f.pixel()[0]==0,"Material opacity multiplies texture alpha");
  material.opacity=1;
  for(auto& vertex:f.mesh.vertices) vertex.opacity=.5f;
  f.mesh.mark_dirty(); f.reset(); f.draw(material);
  require(f.pixel()[0]==0,"Vertex opacity participates in alpha masks");
  for(auto& vertex:f.mesh.vertices) vertex.opacity=1;
  f.mesh.mark_dirty();
  material.alpha_cutoff=-1; material.alpha_blend=true;
  f.reset(); f.draw(material);
  require(std::abs(f.pixel()[0]-display(1)*128/255.f)<=2,"Alpha blend preserves linear alpha coverage");
  Material opaque; opaque.albedo={0,1,0};
  f.reset(); f.draw(material); f.draw(opaque,translate({0,0,.1f}));
  const auto composite=f.pixel();
  require(composite[0]>90 && composite[1]>180 && composite[2]>90,
          "Opaque background remains visible through deferred blended surfaces");
  // Opaque mode intentionally ignores texture alpha, as glTF specifies.
  material.alpha_blend=false; material.double_sided=false;
  f.reset(); f.draw(material,scale({-1,1,1}));
  require(f.pixel()[0]>180,"Mirrored single-sided geometry remains visible");
  f.backend->draw_hud_rect(0,0,8,8,{255,0,0,255});
  require(f.pixel(4,4)[0]==255 && f.pixel(4,4)[1]==0,
          "HUD remains visible after mirrored single-sided draws");
}
void transparency_order_and_lifetime(Fixture& f) {
  Material red; red.albedo={1,0,0}; red.alpha_blend=true; red.opacity=.5f;
  Material blue=red; blue.albedo={0,0,1};
  Material green; green.albedo={0,1,0};
  f.reset(); f.draw(red); f.draw(green,translate({0,0,.2f}));
  const auto red_first=f.image(); const auto center=f.pixel();
  f.reset(); f.draw(green,translate({0,0,.2f})); f.draw(red);
  require(f.image()==red_first,"Transparent foreground is independent of opaque submission order");
  require(std::abs(center[0]-93)<=2 && std::abs(center[1]-93)<=2 && center[2]==0,
          "Fifty-percent red blends over the opaque green background");
  require(f.image()==red_first,"Repeated framebuffer readback does not replay alpha blending");

  f.reset(); f.draw(red); f.draw(blue,translate({0,0,.1f})); f.draw(green,translate({0,0,.2f}));
  const auto near_first=f.image(); const auto layers=f.pixel();
  f.reset(); f.draw(green,translate({0,0,.2f})); f.draw(blue,translate({0,0,.1f})); f.draw(red);
  require(f.image()==near_first,"Transparent layers sort back-to-front independently of submission order");
  require(std::abs(layers[0]-93)<=2 && std::abs(layers[1]-46)<=2 && std::abs(layers[2]-46)<=2,
          "Depth-sorted transparent layers use the expected source-over composition");
  f.reset(); f.draw(red); f.draw(green,translate({0,0,-.1f}));
  require(f.pixel()[0]==0 && f.pixel()[1]>180,"Opaque foreground occludes queued transparency");

  f.reset(); f.draw(red); f.draw(green,translate({0,0,.2f}));
  f.backend->draw_hud_rect(28,28,8,8,{0,0,255,255});
  require(f.pixel()==std::array<int,3>{0,0,255},"HUD flushes transparent geometry before overlay drawing");
  const auto beside_hud=f.pixel(20,20);
  require(std::abs(beside_hud[0]-93)<=2 && std::abs(beside_hud[1]-93)<=2,
          "HUD flush preserves transparent world composition outside the overlay");

  f.reset();
  {
    Mesh temporary=quad();
    auto maps=std::make_shared<MaterialTextures>(); maps->base_color=solid(255,0,0,128);
    Material transient; transient.textures=maps; transient.alpha_blend=true;
    f.backend->draw_mesh(temporary,Mat4::identity(),transient);
    // The original mesh and its GL upload can change or disappear before flush.
    for(auto& vertex:temporary.vertices) vertex.position.x+=10;
    temporary.mark_dirty(); f.backend->upload_mesh(temporary);
    gl::DeleteVertexArrays(1,&temporary.gpu_vao);
    gl::DeleteBuffers(1,&temporary.gpu_vbo); gl::DeleteBuffers(1,&temporary.gpu_ibo);
    temporary.vertices.clear(); temporary.indices.clear();
  }
  f.draw(green,translate({0,0,.2f}));
  const auto retained=f.pixel();
  require(std::abs(retained[0]-93)<=2 && std::abs(retained[1]-93)<=2,
          "Queued draw owns original geometry and texture lifetimes across mutation/destruction");

  f.reset(); f.draw(red);
  f.backend->set_view_proj(translate({4,0,0}),Mat4::identity());
  Lighting dark=f.light; dark.ambient={0,0,0}; f.backend->set_lighting(dark);
  require(std::abs(f.pixel()[0]-93)<=2,"Queued draw retains its submitted camera and lighting state");

  f.reset(); f.draw(red); f.reset(); f.draw(green);
  require(f.pixel()[0]==0 && f.pixel()[1]>180,"New frame discards unfinished transparent submissions");
  f.reset(); f.draw(red);
  // An occlusion query observes the actual draw at presentation, without a
  // framebuffer readback accidentally flushing the pending pass first.
  auto gen_queries=reinterpret_cast<void(*)(gl::GLsizei,gl::GLuint*)>(SDL_GL_GetProcAddress("glGenQueries"));
  auto begin_query=reinterpret_cast<void(*)(gl::GLenum,gl::GLuint)>(SDL_GL_GetProcAddress("glBeginQuery"));
  auto end_query=reinterpret_cast<void(*)(gl::GLenum)>(SDL_GL_GetProcAddress("glEndQuery"));
  auto get_query=reinterpret_cast<void(*)(gl::GLuint,gl::GLenum,gl::GLuint*)>(SDL_GL_GetProcAddress("glGetQueryObjectuiv"));
  auto delete_queries=reinterpret_cast<void(*)(gl::GLsizei,const gl::GLuint*)>(SDL_GL_GetProcAddress("glDeleteQueries"));
  require(gen_queries && begin_query && end_query && get_query && delete_queries,"GL occlusion-query entrypoints");
  gl::GLuint query=0,samples=0; gen_queries(1,&query);
  begin_query(0x8914 /* GL_SAMPLES_PASSED */,query); f.backend->end_frame(); end_query(0x8914);
  get_query(query,0x8866 /* GL_QUERY_RESULT */,&samples); delete_queries(1,&query);
  require(samples>0,"Presentation flushes pending transparent geometry before swapping");
  f.reset(); f.draw(red); f.backend->resize(size,size);
  require(std::abs(f.pixel()[0]-93)<=2,"Resize flushes queued geometry while its original viewport is valid");
  f.reset(); f.draw(red);
  f.backend->destroy(); require(f.backend->create(f.window,size,size),"Recreate GL context with discarded pending draws");
  f.mesh=quad(); f.reset(); f.draw(green);
  require(f.pixel()[0]==0 && f.pixel()[1]>180,"Destroy/recreate cannot replay old queued draws or stale handles");
}
void world_projection(Fixture& f) {
  auto maps=std::make_shared<MaterialTextures>();
  maps->base_color={8,8,std::vector<std::uint8_t>(8*8*4,255)};
  for(int y=0;y<8;++y) for(int x=0;x<8;++x) {
    const int i=(y*8+x)*4; maps->base_color.pixels[i]=std::uint8_t(16+x*31);
    maps->base_color.pixels[i+1]=std::uint8_t(16+y*31); maps->base_color.pixels[i+2]=0;
  }
  Material material; material.textures=maps; material.world_uv_scale=1;
  f.reset(); f.draw(material); const auto expected=f.image();
  for(auto& vertex:f.mesh.vertices) vertex.uv={19.3f,-7.1f};
  f.mesh.mark_dirty(); f.reset(); f.draw(material);
  require(f.image()==expected,"World projection ignores unrelated mesh UVs");
  f.reset(); f.draw(material,scale({-1,1,1}));
  require(f.image()==expected,"World projection retains world orientation under mirrored transforms");
  // Align Y and X world planes with the screen without changing projected UVs.
  const Mat4 transforms[]={rotate_x(radians(90)),rotate_y(radians(-90))};
  for(const auto& model:transforms) {
    Mat4 view; require(inverse(model,view),"Invert plane-test model");
    f.reset(); f.backend->set_view_proj(view,Mat4::identity()); f.draw(material,model);
    const auto color=f.pixel(43,21);
    // Compare expected texels numerically instead of relying on triangle derivatives.
    const Vec3 world=transform_point(model,{(43.5f/size*2-1),(1-21.5f/size*2),0});
    const Vec3 n=transform_direction(model,{0,0,1});
    float u,v;
    if(std::fabs(n.y)>.9f) {u=world.x;v=world.z;}
    else {u=world.z;v=-world.y;}
    auto sample=[](float coordinate) {
      const float p=(coordinate-std::floor(coordinate))*8-.5f;
      const int lo=int(std::floor(p)); const float t=p-std::floor(p);
      const int a=(lo%8+8)%8,b=(a+1)%8;
      return linear(16+a*31)*(1-t)+linear(16+b*31)*t;
    };
    require(std::abs(color[0]-display(sample(u)))<=3 && std::abs(color[1]-display(sample(v)))<=3,
            "Dominant world-normal Y/X projections use expected axis and sign");
  }
  f.mesh=quad();
}
void mip_and_cache(Fixture& f) {
  auto maps=std::make_shared<MaterialTextures>();
  maps->base_color={64,64,std::vector<std::uint8_t>(64*64*4,255)};
  for(int y=0;y<64;++y) for(int x=0;x<64;++x)
    for(int c=0;c<3;++c) maps->base_color.pixels[(y*64+x)*4+c]=((x+y)&1)?255:0;
  Material material; material.textures=maps;
  for(auto& vertex:f.mesh.vertices) { vertex.uv.x*=256; vertex.uv.y*=256; }
  f.mesh.mark_dirty(); f.reset(); f.draw(material);
  require(std::abs(f.pixel()[0]-display(.5f))<=3,"Minified color mips preserve linear-light mean");
  // Verify uploaded normal mips, rather than merely the CPU generator.
  auto normal_maps=std::make_shared<MaterialTextures>();
  normal_maps->normal={2,1,{255,128,128,255,128,128,255,255}};
  material.textures=normal_maps; f.reset(); f.draw(material);
  auto get_texture=reinterpret_cast<void(*)(gl::GLenum,gl::GLint,gl::GLenum,gl::GLenum,void*)>(
      SDL_GL_GetProcAddress("glGetTexImage"));
  auto get_level=reinterpret_cast<void(*)(gl::GLenum,gl::GLint,gl::GLenum,gl::GLint*)>(
      SDL_GL_GetProcAddress("glGetTexLevelParameteriv"));
  auto is_texture=reinterpret_cast<gl::GLboolean(*)(gl::GLuint)>(SDL_GL_GetProcAddress("glIsTexture"));
  require(get_texture && get_level && is_texture,"GL 3.3 texture inspection entrypoints");
  gl::ActiveTexture(gl::GL_TEXTURE3);
  std::array<std::uint8_t,4> normal_mip{};
  get_texture(gl::GL_TEXTURE_2D,1,gl::GL_RGBA,gl::GL_UNSIGNED_BYTE,normal_mip.data());
  require(normal_mip[0]>210 && normal_mip[2]>210,"Uploaded normal mip is renormalized");
  gl::ActiveTexture(gl::GL_TEXTURE0);
  auto large_maps=std::make_shared<MaterialTextures>();
  large_maps->base_color={4096,1,std::vector<std::uint8_t>(4096*4,255)};
  material.textures=large_maps; f.reset(); f.draw(material);
  gl::GLint uploaded_width=0;
  get_level(gl::GL_TEXTURE_2D,0,0x1000 /* GL_TEXTURE_WIDTH */,&uploaded_width);
  gl::GLint driver_limit=0; gl::GetIntegerv(gl::GL_MAX_TEXTURE_SIZE,&driver_limit);
  int expected_width=4096;
  while(expected_width>2048 || expected_width>driver_limit) expected_width/=2;
  require(uploaded_width==expected_width,"Large maps obey bounded upload resolution and driver limits");
  std::vector<std::shared_ptr<MaterialTextures>> owners;
  gl::GLuint oldest_texture=0;
  for(int i=0;i<70;++i) {
    auto input=std::make_shared<MaterialTextures>(); input->base_color=solid(i*3,0,0);
    material.textures=input; owners.push_back(input); f.reset(); f.draw(material);
    if(i==0) {
      gl::GLint texture=0; gl::GetIntegerv(0x8069 /* GL_TEXTURE_BINDING_2D */,&texture);
      oldest_texture=gl::GLuint(texture);
    }
    require(std::abs(f.pixel()[0]-display(linear(i*3)))<=2,"Bounded cache texture identity remains correct");
  }
  require(oldest_texture!=0,"Record the first cached GL texture");
  if(is_texture(oldest_texture)) {
    // Drivers may recycle deleted names for a later map; the first black
    // material must nevertheless be gone rather than retained past the limit.
    gl::BindTexture(gl::GL_TEXTURE_2D,oldest_texture);
    std::array<std::uint8_t,4> recycled{};
    get_texture(gl::GL_TEXTURE_2D,0,gl::GL_RGBA,gl::GL_UNSIGNED_BYTE,recycled.data());
    require(recycled[0] || recycled[1] || recycled[2],"64-set cache evicts the oldest GPU texture");
  }
  material.textures=owners.front(); f.reset(); f.draw(material);
  require(f.pixel()[0]==0,"Evicted material texture set reloads correctly");
  // Expired owners must not keep stale images if an allocator reuses addresses.
  owners.clear(); material.textures.reset();
  for(int i=0;i<12;++i) {
    auto input=std::make_shared<MaterialTextures>(); input->base_color=solid(0,i*20,0);
    material.textures=input; f.reset(); f.draw(material);
    require(std::abs(f.pixel()[1]-display(linear(i*20)))<=2,"Expired texture owners cannot alias stale cache entries");
  }
}
}  // namespace

int main(int, char**) {
  // Optional runtime test: GL is never required for a CPU-only machine/build.
  if(SDL_Init(SDL_INIT_VIDEO)!=0) { std::cout<<"SKIP: SDL video unavailable: "<<SDL_GetError()<<'\n'; return 77; }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_Window* window=SDL_CreateWindow("GL material tests",0,0,size,size,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  if(!window) { std::cout<<"SKIP: GL window unavailable: "<<SDL_GetError()<<'\n'; SDL_Quit(); return 77; }
  SDL_GLContext probe=SDL_GL_CreateContext(window);
  if(!probe) { std::cout<<"SKIP: GL context unavailable: "<<SDL_GetError()<<'\n'; SDL_DestroyWindow(window); SDL_Quit(); return 77; }
  SDL_GL_DeleteContext(probe);
  int result=0;
  try {
    Fixture f(window);
    std::cout<<"GL material driver: "<<reinterpret_cast<const char*>(gl::GetString(gl::GL_RENDERER))<<'\n';
    color_and_emission(f); packed_surface_and_normals(f); alpha_and_winding(f);
    transparency_order_and_lifetime(f); world_projection(f); mip_and_cache(f);
    auto get_error=reinterpret_cast<gl::GLenum(*)()>(SDL_GL_GetProcAddress("glGetError"));
    require(get_error && get_error()==0,"GL material operations leave no API errors");
    std::cout<<"GL material color/normal/metalrough/emission/alpha/transparency-lifetime/world-UV/mip/cache checks passed\n";
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
  SDL_DestroyWindow(window); SDL_Quit(); return result;
}
