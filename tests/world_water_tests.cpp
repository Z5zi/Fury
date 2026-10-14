#include "../apps/vaultline/world_water.hpp"

#include <fury/material_sampling.hpp>
#include <fury/renderer.hpp>

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
bool same(Vec3 a,Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
bool same(Vec2 a,Vec2 b) { return a.x==b.x && a.y==b.y; }
bool same(const Transform& a,const Transform& b) {
  return same(a.position,b.position) && same(a.rotation_euler,b.rotation_euler) && same(a.scale,b.scale);
}
bool same_image(const RgbaImage& a,const RgbaImage& b) {
  return a.width==b.width && a.height==b.height && a.pixels==b.pixels;
}
void check_chain(const RgbaImage& image,const std::vector<RgbaImage>& chain,
                 TextureEncoding encoding) {
  require(image.valid() && image.width==256 && image.height==256,"256px base map invalid");
  require(chain.size()==8,"complete additional mip chain required");
  const auto reference=build_mip_chain(image,encoding);
  int dim=128;
  for(std::size_t i=0;i<chain.size();++i) {
    require(chain[i].valid() && chain[i].width==dim && chain[i].height==dim,
            "mips must begin at level 1, not duplicate level 0");
    require(same_image(chain[i],reference[i+1]),"mip channel encoding/filtering mismatch");
    dim=std::max(1,dim/2);
  }
}
void check_normal(const RgbaImage& image) {
  for(std::size_t i=0;i<image.pixels.size();i+=4) {
    Vec3 n{image.pixels[i]/127.5f-1,image.pixels[i+1]/127.5f-1,image.pixels[i+2]/127.5f-1};
    require(std::abs(length(n)-1.f)<.011f,"normal not unit length within byte quantization");
    require(n.z>.98f && std::abs(n.x)<.16f && std::abs(n.y)<.16f,"water slopes are exaggerated");
    require(image.pixels[i+3]==255,"normal alpha must be opaque");
  }
}
void test_maps() {
  const auto a=vaultline::make_world_water_textures(),b=vaultline::make_world_water_textures();
  require(a.get()!=b.get(),"determinism test must generate independent maps");
  require(same_image(a->base_color,b->base_color) && same_image(a->normal,b->normal) &&
          same_image(a->metallic_roughness,b->metallic_roughness),"map generation is nondeterministic");
  check_chain(a->base_color,a->base_color_mips,TextureEncoding::SRGB);
  check_chain(a->normal,a->normal_mips,TextureEncoding::Normal);
  check_chain(a->metallic_roughness,a->metallic_roughness_mips,TextureEncoding::Linear);
  check_normal(a->normal); for(const auto& mip:a->normal_mips) check_normal(mip);
  require(!a->emissive.valid() && a->emissive_mips.empty(),"water should not emit light");
  double normal_energy=0; std::size_t bytes=0;
  const std::array<const RgbaImage*,3> images{{&a->base_color,&a->normal,&a->metallic_roughness}};
  for(const auto* image:images) bytes+=image->pixels.size();
  for(const auto* chain:{&a->base_color_mips,&a->normal_mips,&a->metallic_roughness_mips})
    for(const auto& mip:*chain) bytes+=mip.pixels.size();
  require(bytes==1048572 && bytes<2*1024*1024,"shared-map memory budget exceeded");
  for(std::size_t i=0;i<a->base_color.pixels.size();i+=4) {
    const auto& c=a->base_color.pixels; const auto& s=a->metallic_roughness.pixels;
    require(c[i]>=54 && c[i]<=58 && c[i+1]>=73 && c[i+1]<=77 && c[i+2]>=75 && c[i+2]<=79,
            "base must remain low-contrast restrained dark teal gray");
    require(c[i+3]==255 && s[i+3]==255,"water is opaque");
    require(s[i]==255 && s[i+1]>=62 && s[i+1]<=73 && s[i+2]==0,
            "surface map must be linear R=unused G=roughness B=nonmetal");
    const double nx=a->normal.pixels[i]/127.5-1,nz=a->normal.pixels[i+1]/127.5-1;
    normal_energy+=nx*nx+nz*nz;
  }
  require(normal_energy/(256*256)>.0005,"wave normals have become flat");
  // Repeat boundary must be as smooth as adjacent interior samples, rather
  // than a discontinuous square tile. This includes the narrowest waves.
  for(int j=0;j<256;++j) for(int c=0;c<3;++c) {
    const auto& p=a->normal.pixels;
    require(std::abs(int(p[(j*256)*4+c])-int(p[(j*256+255)*4+c]))<=5,"normal U seam");
    require(std::abs(int(p[j*4+c])-int(p[(255*256+j)*4+c]))<=5,"normal V seam");
  }
  for(const Vec2 uv:std::array<Vec2,3>{{{.231f,.797f},{-.132f,1.733f},{4.247f,-3.125f}}}) {
    const auto p=sample_material_texture_lod(a->normal,a->normal_mips,uv,false,1.5f);
    const auto q=sample_material_texture_lod(a->normal,a->normal_mips,{uv.x+3,uv.y-2},false,1.5f);
    require(std::abs(p.x-q.x)+std::abs(p.y-q.y)+std::abs(p.z-q.z)<.0001f,
            "negative/repeated UV filtering mismatch");
  }
  std::cout<<"Water maps: deterministic 256px, 8 mips, "<<bytes<<" shared bytes, RMS slope "
           <<std::sqrt(normal_energy/(256*256))<<'\n';
}

void add_water(Scene& scene,const char* name,Vec3 position,Vec3 color,float width,float depth,float uv) {
  Entity e; e.name=name; e.tag="original_gameplay_tag"; e.transform.position=position;
  e.mesh=scene.add_mesh(make_plane(width,depth,color,uv)); e.material.texture=TextureSlot::Water;
  e.material.albedo={.7f,.92f,1.12f}; e.material.roughness=.18f; e.material.metallic=.45f;
  e.material.uv_scroll_u=.045f; e.material.uv_scroll_v=.028f;
  e.collider=Aabb::from_center_size({.3f,0,.7f},{width,.2f,depth});
  scene.add_entity(std::move(e));
}
void test_scene() {
  Scene scene;
  add_water(scene,"HarborWater",{20,-.35f,56},{.15f,.35f,.55f},90,36,10);
  add_water(scene,"RidgeWater",{103,-.4f,38},{.12f,.32f,.52f},50,28,8);
  add_water(scene,"NorthQuayWater",{14,-.4f,124},{.12f,.32f,.52f},56,22,8);
  add_water(scene,"OtherWater",{30,-.4f,-56},{.2f,.3f,.5f},7,9,2);
  // Preserve real behavior flags, not only default non-solid fixtures.
  scene.entities()[1].solid=true; scene.entities()[1].visible=false; scene.entities()[1].detail=true;
  scene.entities()[1].transform.rotation_euler={.01f,.13f,0};
  scene.entities()[1].transform.scale={1.2f,.9f,.8f};
  scene.entities()[1].lod_mesh=scene.entities()[3].mesh;
  const auto originals=scene.entities(); const auto solids=scene.collect_solids();
  std::vector<Mesh> meshes; for(const auto& m:scene.meshes()) meshes.push_back(*m);
  const auto stats=vaultline::upgrade_world_water(scene);
  require(stats.applied && !stats.already_applied && stats.updated_surfaces==3,"all exact water surfaces required");
  require(stats.harbor_surfaces==1 && stats.ridge_surfaces==1 && stats.north_quay_surfaces==1,
          "three named surface counts required");
  require(stats.shared_map_bytes==1048572 && stats.added_triangles==0,"map/geometry accounting");
  require(scene.entities().size()==4 && scene.meshes().size()==4,"water may not add entities/meshes");
  require(scene.collect_solids().size()==solids.size(),"solid count changed");
  for(std::size_t i=0;i<originals.size();++i) {
    const auto& a=originals[i]; const auto& b=scene.entities()[i];
    require(a.name==b.name && a.tag==b.tag && a.mesh==b.mesh && a.lod_mesh==b.lod_mesh,"original identity/mesh changed");
    require(same(a.transform,b.transform) && a.visible==b.visible && a.solid==b.solid && a.detail==b.detail,
            "original transform or gameplay flag changed");
    require(same(a.collider.center,b.collider.center) && same(a.collider.half_extents,b.collider.half_extents),
            "original collider changed");
    require(b.material.texture==TextureSlot::Water,"Water slot/reflection routing lost");
    require(meshes[i].indices==b.mesh->indices && meshes[i].vertices.size()==b.mesh->vertices.size(),"water geometry changed");
    for(std::size_t j=0;j<meshes[i].vertices.size();++j) {
      const auto& v=meshes[i].vertices[j]; const auto& w=b.mesh->vertices[j];
      require(same(v.position,w.position) && same(v.normal,w.normal) && same(v.color,w.color) && same(v.uv,w.uv) &&
              v.opacity==w.opacity,"water vertex data was changed");
    }
    if(i==3) {
      require(!b.material.textures && b.material.uv_scroll_u==a.material.uv_scroll_u &&
              same(b.material.albedo,a.material.albedo),"other Water-slot materials were modified");
      continue;
    }
    const auto& m=b.material;
    require(m.textures==scene.entities()[0].material.textures,"water maps are not shared");
    require(m.world_uv_scale==1.f/16 && m.normal_scale==1.f && m.metallic==0 && m.roughness==1,
            "physical map/material factors incorrect");
    const auto color=b.mesh->vertices.front().color;
    require(std::abs(color.x*m.albedo.x-1)<1e-6f && std::abs(color.y*m.albedo.y-1)<1e-6f &&
            std::abs(color.z*m.albedo.z-1)<1e-6f,"legacy vertex blue tint not compensated");
    require(m.transmission==0 && m.opacity==1 && !m.alpha_blend && m.index_of_refraction==1.333f,
            "water opacity/dielectric factors incorrect");
    require(std::abs(m.uv_scroll_u/m.world_uv_scale-.12f)<1e-6f &&
            std::abs(m.uv_scroll_v/m.world_uv_scale-.045f)<1e-6f,"physical scroll speed incorrect");
    const Vec2 uv{.317f,.419f};
    const auto n0=sample_material_texture(m.textures->normal,m.textures->normal_mips,uv,false);
    const auto n1=sample_material_texture(m.textures->normal,m.textures->normal_mips,
        {uv.x+m.uv_scroll_u*2,uv.y+m.uv_scroll_v*2},false);
    require(std::abs(n0.x-n1.x)+std::abs(n0.y-n1.y)>.001f,"normal map animation has no effect");
  }
  const auto again=vaultline::upgrade_world_water(scene);
  require(again.already_applied && !again.applied && again.updated_surfaces==0 && again.shared_map_bytes==stats.shared_map_bytes,
          "second apply must be a true no-op");
  require(scene.entities()[0].material.textures==scene.entities()[2].material.textures,"second apply broke sharing");
}

void test_scope() {
  Scene empty; const auto no_water=vaultline::upgrade_world_water(empty);
  require(!no_water.applied && !no_water.already_applied && !no_water.shared_map_bytes,"empty scene allocated water");
  Scene source; add_water(source,"HarborWater",{},{.2f,.3f,.4f},10,10,1);
  auto maps=std::make_shared<MaterialTextures>(); maps->source="gltf://original-water";
  maps->base_color={1,1,{70,80,90,255}}; source.entities()[0].material.textures=maps;
  add_water(source,"RidgeWater",{},{.2f,.3f,.4f},10,10,1); source.entities()[1].material.texture=TextureSlot::Metal;
  add_water(source,"NorthQuayWater",{},{.2f,.3f,.4f},10,10,1); source.entities()[2].mesh=nullptr;
  const auto result=vaultline::upgrade_world_water(source);
  require(!result.applied && result.preserved_source_materials==1,"authored source maps must not be overwritten");
  require(source.entities()[0].material.textures==maps && source.entities()[0].material.roughness==.18f,
          "source map/factor regression");
  require(!source.entities()[1].material.textures && source.entities()[1].material.texture==TextureSlot::Metal,
          "name alone may not replace non-water material");
  // A partial scene may gain a district later without duplicating or replacing
  // the earlier map set, and without a scene-wide sentinel blocking progress.
  Scene partial; add_water(partial,"HarborWater",{},{.15f,.35f,.55f},90,36,10);
  require(vaultline::upgrade_world_water(partial).updated_surfaces==1,"partial scene first apply");
  const auto first=partial.entities()[0].material.textures;
  add_water(partial,"NorthQuayWater",{},{.12f,.32f,.52f},56,22,8);
  std::rotate(partial.entities().begin(),partial.entities().begin()+1,partial.entities().end());
  require(vaultline::upgrade_world_water(partial).updated_surfaces==1,"partial scene second apply");
  require(partial.entities()[0].material.textures==first,"late district must reuse maps regardless of entity order");
}

void test_cpu_ray() {
  SDL_setenv("SDL_VIDEODRIVER","dummy",1); SDL_setenv("FURY_CPU_THREADS","2",1);
  require(SDL_Init(SDL_INIT_VIDEO)==0,"CPU water test SDL initialization");
  SDL_Window* window=SDL_CreateWindow("CPU mapped water",0,0,64,64,SDL_WINDOW_HIDDEN);
  require(window!=nullptr,"CPU water test dummy window");
  try {
    auto backend=create_cpu_ray_backend(); require(backend && backend->create(window,64,64),"CPU water backend");
    auto settings=backend->settings(); settings.denoise=false; settings.samples_per_pixel=2;
    settings.max_bounces=2; settings.trace_mode=TraceMode::RayTraced;
    settings.debug_view=RenderDebugView::Normals;
    require(backend->configure(settings),"CPU water configuration");
    Mesh plane=make_plane(18,18,{1,1,1},10);
    const auto& a=plane.vertices[plane.indices[0]];
    const auto& b=plane.vertices[plane.indices[1]];
    const auto& c=plane.vertices[plane.indices[2]];
    require(dot(cross(b.position-a.position,c.position-a.position),a.normal)>0,
            "horizontal plane winding must agree with upward vertex normals");
    Lighting light; light.ambient={.1f,.1f,.1f}; light.sun_direction={-.2f,-1,.1f};
    light.sun_intensity=1; light.fog_start=100; light.fog_end=200;
    const Vec3 eye{1,8,5}; const auto view=look_at(eye,{0,0,0},{0,1,0});
    const auto projection=perspective(radians(50),1,.1f,30);
    Material material; material.textures=vaultline::make_world_water_textures();
    material.roughness=1; material.world_uv_scale=1.f/16; material.index_of_refraction=1.333f;
    material.uv_scroll_u=.12f/16; material.uv_scroll_v=.045f/16;
    auto render=[&](const Material& m) {
      backend->reset_history(); backend->begin_frame({0,0,0,255});
      backend->set_view_proj(view,projection); backend->set_camera_position(eye);
      backend->set_lighting(light); backend->set_time(2.75f); backend->set_object_id(1);
      backend->draw_mesh(plane,Mat4::identity(),m);
      std::vector<std::uint8_t> image; int w=0,h=0;
      require(backend->read_rgb_framebuffer(image,w,h) && w==64 && h==64,"CPU mapped water readback");
      return image;
    };
    const auto reference_normals=render(material); material.texture=TextureSlot::Water;
    require(render(material)==reference_normals,"CPU mapped water must not add legacy wave normals");
    settings.debug_view=RenderDebugView::Beauty; require(backend->configure(settings),"CPU water beauty configuration");
    const auto water=render(material); material.texture=TextureSlot::None;
    require(render(material)==water,"CPU mapped water must retain authored optical/material factors");
    std::cout<<"CPU water: horizontal winding and mapped-water normal/beauty equivalence passed\n";
  } catch(...) { SDL_DestroyWindow(window); SDL_Quit(); throw; }
  SDL_DestroyWindow(window); SDL_Quit();
}

int test_gl() {
  // GL entry points are loaded through SDL. CMake's optional OpenGL package
  // flag does not prove whether this actual runtime can create a context.
  if(SDL_Init(SDL_INIT_VIDEO)!=0) { std::cout<<"SKIP: SDL video: "<<SDL_GetError()<<'\n'; return 77; }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_Window* window=SDL_CreateWindow("World water GL checks",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  if(!window) { std::cout<<"SKIP: GL window: "<<SDL_GetError()<<'\n'; SDL_Quit(); return 77; }
  SDL_GLContext probe=SDL_GL_CreateContext(window);
  if(!probe) { std::cout<<"SKIP: GL context: "<<SDL_GetError()<<'\n'; SDL_DestroyWindow(window); SDL_Quit(); return 77; }
  SDL_GL_DeleteContext(probe);
  int result=0;
  try {
    auto backend=create_gl_backend();
    if(!backend) {
      std::cout<<"SKIP: GL backend unavailable\n"; SDL_DestroyWindow(window); SDL_Quit(); return 77;
    }
    require(backend->create(window,64,64),"GL water shader initialization failed after successful context probe");
    Lighting light; light.ambient={1,1,1}; light.sun_intensity=0; light.ao_strength=0;
    light.fog_start=100; light.fog_end=200; light.point_light_count=0;
    light.enable_bloom=false; light.enable_shadows=false; light.enable_reflections=false;
    Mesh quad;
    quad.vertices={{{-.9f,-.9f,0},{0,0,1},{1,1,1},{0,10}},{{.9f,-.9f,0},{0,0,1},{1,1,1},{10,10}},
                   {{.9f,.9f,0},{0,0,1},{1,1,1},{10,0}},{{-.9f,.9f,0},{0,0,1},{1,1,1},{0,0}}};
    quad.indices={0,1,2,0,2,3};
    const Mesh* surface=&quad;
    Mat4 view=Mat4::identity(),projection=Mat4::identity();
    Vec3 eye{0,0,3};
    auto render=[&](const Material& material) {
      backend->set_view_proj(view,projection); backend->set_camera_position(eye);
      backend->set_lighting(light); backend->set_time(0); backend->begin_frame({0,0,0,255});
      backend->draw_mesh(*surface,Mat4::identity(),material);
      std::vector<std::uint8_t> image; int w=0,h=0;
      require(backend->read_rgb_framebuffer(image,w,h) && w==64 && h==64,"GL water readback failed");
      return image;
    };
    Material material; material.textures=vaultline::make_world_water_textures(); material.roughness=1;
    const auto untagged=render(material); material.texture=TextureSlot::Water;
    require(render(material)==untagged,"mapped water acquired legacy UV-edge foam/tint");
    // Arbitrary source maps also retain their albedo when normal data exists.
    auto source=std::make_shared<MaterialTextures>(); source->base_color={1,1,{31,73,48,255}};
    source->normal={1,1,{128,128,255,255}}; material.textures=source;
    material.texture=TextureSlot::None; const auto authored=render(material);
    material.texture=TextureSlot::Water; require(render(material)==authored,"authored water map regression");
    // The old fallback continues to behave as before when it has no normal map.
    auto fallback=std::make_shared<MaterialTextures>(); fallback->base_color=source->base_color;
    material.textures=fallback; material.texture=TextureSlot::None; const auto plain=render(material);
    material.texture=TextureSlot::Water; require(render(material)!=plain,"legacy water path was unintentionally removed");
    // Use the real horizontal helper and a camera/sun above it. Ambient-only
    // front-facing screen quads cannot catch mismatched plane winding/normals.
    Mesh horizontal=make_plane(1.8f,1.8f,{1,1,1},10.f); surface=&horizontal;
    eye={0,3,0}; view=look_at(eye,{0,0,0},{0,0,-1});
    projection=orthographic(-1,1,-1,1,.1f,10.f);
    light.ambient={0,0,0}; light.sun_direction={0,-1,0}; light.sun_color={1,1,1}; light.sun_intensity=1;
    material.textures=vaultline::make_world_water_textures(); material.world_uv_scale=1.f/16;
    const auto above=render(material); const auto center=static_cast<std::size_t>((32*64+32)*3);
    require(above[center]>35 && above[center+1]>45 && above[center+2]>45,
            "horizontal make_plane water must face and receive sunlight from above");
    material.texture=TextureSlot::None;
    require(render(material)==above,"horizontal mapped water acquired legacy lighting/foam");
    std::cout<<"GL water: generated/source maps bypass legacy foam/tint; fallback retained; horizontal plane lit from above\n";
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
  SDL_DestroyWindow(window); SDL_Quit(); return result;
}
}  // namespace

int main(int argc,char** argv) {
  if(argc>1 && std::string(argv[1])=="--gl") return test_gl();
  try { test_maps(); test_scene(); test_scope(); test_cpu_ray(); std::cout<<"World water checks passed\n"; return 0; }
  catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
