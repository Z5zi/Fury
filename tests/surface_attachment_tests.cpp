#include "fury/renderer.hpp"
#include "fury/texture.hpp"
#include <SDL.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace fury;
namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
std::vector<std::uint8_t> render(const Material& material,bool detail) {
  SDL_setenv("FURY_SURFACE_DETAIL",detail ? "1":"0",1);
  SDL_setenv("FURY_SURFACE_DETAIL_RES","128",1);
  SDL_Window* window=SDL_CreateWindow("Surface attachment",0,0,48,48,SDL_WINDOW_HIDDEN);
  require(window!=nullptr,"Create attachment fixture window");
  std::vector<std::uint8_t> result;
  {
    Renderer renderer;
    require(renderer.create(window,48,48,false,RenderBackendKind::Software),"Create CPU attachment renderer");
    Lighting light; light.sun_intensity=0; light.ambient={1,1,1}; light.ao_strength=0;
    light.fog_start=light.fog_end=0; light.enable_bloom=false; light.enable_reflections=false;
    renderer.set_lighting(light); renderer.set_camera_position({0,0,2});
    renderer.begin_frame({0,0,0,255}); renderer.set_view_proj(Mat4::identity(),Mat4::identity());
    Mesh mesh;
    mesh.vertices={{{-1,-1,0},{0,0,1},{1,1,1},{0,1}},{{1,-1,0},{0,0,1},{1,1,1},{1,1}},
                   {{1,1,0},{0,0,1},{1,1,1},{1,0}},{{-1,1,0},{0,0,1},{1,1,1},{0,0}}};
    mesh.indices={0,1,2,0,2,3};
    renderer.draw_mesh(mesh,Mat4::identity(),material,1);
    int width{},height{}; require(renderer.read_rgb_framebuffer(result,width,height),"Read attachment pixels");
    require(width==48&&height==48,"Attachment dimensions");
  }
  SDL_DestroyWindow(window); return result;
}
}
int main(int argc,char** argv) {
  (void)argc; (void)argv;
  SDL_setenv("SDL_VIDEODRIVER","dummy",1);
  if(SDL_Init(SDL_INIT_VIDEO)!=0) return 1;
  int result=0;
  try {
    auto imported=std::make_shared<MaterialTextures>(); imported->source="non-emitting imported factor fixture";
    Material material; material.albedo={.8f,.8f,.8f}; material.roughness=1;
    material.textures=imported; material.emissive=1; material.emissive_color={0,0,0};
    material.detail_texture=TextureSlot::Concrete;
    require(render(material,false)!=render(material,true),"Imported strength-one black emission still receives actual detail maps");
    require(material.textures==imported && material.world_uv_scale==0 && material.detail_rotation==0,
            "Renderer detail attachment never mutates the source material");
    auto colored=std::make_shared<MaterialTextures>(); colored->base_color={1,1,{0,255,0,255}};
    material.textures=colored;
    require(render(material,false)==render(material,true),"Authored maps are preserved despite a detail hint");
    material.textures=imported; material.emissive_color={0,.5f,1};
    require(render(material,false)==render(material,true),"Actual imported emission remains protected");
    material.emissive_color={0,0,0}; material.transmission=.8f;
    require(render(material,false)==render(material,true),"Glass remains protected");
    material.transmission=0; material.alpha_blend=true; material.opacity=.5f;
    require(render(material,false)==render(material,true),"Blended surfaces remain protected");
    material.alpha_blend=false; material.alpha_cutoff=.5f;
    require(render(material,false)==render(material,true),"Masked surfaces remain protected");
    material.alpha_cutoff=-1; material.opacity=1; material.detail_texture=TextureSlot::Wood;
    const auto grain=render(material,true); material.detail_rotation=1;
    require(render(material,true)!=grain,"Quarter-turn variants reach the actual renderer");
    material.detail_rotation=0; material.detail_use_mesh_uvs=true;
    require(render(material,true)!=grain,"Runtime-baked UV opt-in is respected");
    Material emissive_legacy; emissive_legacy.texture=TextureSlot::Wood; emissive_legacy.emissive=1;
    require(render(emissive_legacy,false)==render(emissive_legacy,true),"Legacy emission semantics are preserved");
    std::cout<<"Renderer surface-detail attachment, preservation, rotation and UV controls passed\n";
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
  SDL_Quit(); return result;
}
