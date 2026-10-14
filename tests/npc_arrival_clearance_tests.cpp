// Production-model diagnostic, not an ordinary mission/navigation replay.
// Drives the real locomotion and presentation cadence; it never refreshes poses
// for review. Native views use the gameplay standing eye, FOV and near plane.
#include "../apps/vaultline/npc_presentation.hpp"
#include "../apps/vaultline/npc_roster.hpp"
#include <fury/application.hpp>
#include <fury/camera.hpp>
#include <fury/renderer.hpp>
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace fury;
namespace {
void require(bool value, const char* reason) {
  if (!value) throw std::runtime_error(reason);
}
struct Variant { const char* id; float height, speed; NpcKind kind; };
const Variant variants[] = {
  {"NpcGuard",1.85f,3.5f,NpcKind::Guard},
  {"NpcDeskGuard",1.82f,3.4f,NpcKind::Guard},
  {"NpcAlleyHmpd",1.86f,3.8f,NpcKind::Guard},
  {"NpcExtraGuard",1.82f,4.f,NpcKind::Guard},
  {"NpcEnforcer",1.95f,5.6f,NpcKind::Enforcer},
};

float minimum_depth(const Entity& entity, const Camera& camera) {
  float result=std::numeric_limits<float>::infinity();
  const auto model=entity.transform.matrix();
  for(const auto& vertex:entity.mesh->vertices) {
    const auto world=transform_point(model,vertex.position);
    const float depth=dot(world-camera.position,camera.forward());
    require(std::isfinite(depth),"Nonfinite animated mesh depth");
    result=std::min(result,depth);
  }
  return result;
}

struct NativeView {
  SDL_Window* window{};
  std::unique_ptr<IRenderBackend> renderer;
  Mesh floor=make_plane(24.f,24.f,{.15f,.18f,.22f});
  Lighting light;
  NativeView() {
    require(SDL_Init(SDL_INIT_VIDEO)==0,"SDL video initialization failed");
    window=SDL_CreateWindow("Threat arrival model diagnostic",0,0,640,360,SDL_WINDOW_HIDDEN);
    require(window!=nullptr,"SDL diagnostic window failed");
    renderer=create_software_backend();
    require(renderer&&renderer->create(window,640,360),"Native software renderer failed");
    require(renderer->kind()==RenderBackendKind::Software,"Unexpected backend fallback");
    light.ambient={.62f,.65f,.70f};light.sun_direction={-.4f,-.8f,-.5f};
    light.sun_color={1.f,.95f,.88f};light.sun_intensity=.7f;
    light.ao_strength=0.f;light.fog_start=100.f;light.fog_end=200.f;
    light.enable_shadows=light.enable_reflections=light.enable_bloom=false;
    light.point_light_count=0;
  }
  ~NativeView() { renderer.reset();if(window)SDL_DestroyWindow(window);SDL_Quit(); }
  void draw(const Entity& actor,const Camera& camera,const std::filesystem::path& path) {
    renderer->begin_frame({40,53,66,255});
    renderer->set_view_proj(camera.view_matrix(),camera.projection_matrix(640.f/360.f));
    renderer->set_camera_position(camera.position);renderer->set_lighting(light);renderer->set_time(0.f);
    Material ground;ground.albedo={1,1,1};ground.roughness=1.f;
    renderer->set_object_id(1);renderer->draw_mesh(floor,Mat4::identity(),ground);
    renderer->set_object_id(2);renderer->draw_mesh(*actor.mesh,actor.transform.matrix(),actor.material);
    std::vector<std::uint8_t> rgb;int width{},height{};
    require(renderer->read_rgb_framebuffer(rgb,width,height)&&width==640&&height==360,
            "Native RGB readback failed");
    if(!path.empty()) {
      std::ofstream file(path,std::ios::binary);
      file<<"P6\n640 360\n255\n";
      file.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());
      require(bool(file),"Native diagnostic capture write failed");
    }
    renderer->end_frame();
  }
};
}

int main(int argc,char** argv) {
  try {
    const std::filesystem::path output=argc>1?argv[1]:"";
    if(!output.empty())std::filesystem::create_directories(output);
    NativeView native;
    std::cout<<std::setprecision(9);
    const float player_radius=AppConfig{}.player_radius;
    unsigned frames=0,negative_controls=0;
    for(const auto& variant:variants)for(float angle:{0.f,.8f}) {
      NpcAgent authored;
      bool from_roster=false;
      for(const auto& entry:vaultline::make_npc_roster())if(entry.agent.entity_name==variant.id) {
        authored=entry.agent;from_roster=true;
      }
      if(!from_roster) {
        // Rare-complication values from main.cpp's actual spawn blocks.
        authored.entity_name=variant.id;authored.kind=variant.kind;
        authored.height=variant.height;authored.chase_speed=variant.speed;
      }
      require(authored.height==variant.height&&authored.chase_speed==variant.speed&&
                  authored.kind==variant.kind,"Production variant identity/speed/height drift");
      authored.position={std::sin(angle)*8.f,authored.height*.5f,-std::cos(angle)*8.f};
      authored.yaw=-1.5707963f;authored.chasing=true;authored.chase_target={0,1.7f,0};
      NpcSystem npcs;npcs.add(authored);
      Scene scene;Entity body;body.name=variant.id;body.solid=false;
      auto& entity=scene.add_entity(std::move(body));
      vaultline::NpcPresentation presentation;
      presentation.install(scene,entity,authored.height,
          vaultline::npc_character_role(variant.id,authored.kind),{1,1,1});
      Camera camera;camera.position={0,camera.stand_eye_y,0};camera.fly_mode=false;
      camera.yaw=std::atan2(-std::cos(angle),std::sin(angle));camera.snap_look();
      require(camera.near_plane==.1f&&camera.fov_y_degrees==60.f&&camera.position.y==1.7f,
              "Diagnostic view must preserve gameplay standing-eye optics");
      float minimum=100.f;
      for(int frame=0;frame<720;++frame) {
        npcs.update(1.f/60.f,player_radius);
        const auto& agent=npcs.agents()[0];
        entity.transform.position=agent.position;entity.transform.rotation_euler.y=agent.yaw;
        presentation.advance(entity.name,{agent.position,agent.yaw,agent.actual_speed,
            agent.travel_distance,agent.move_weight,agent.turn_rate,0.f,true},
            1.f/60.f,camera.position,30.f);
        const float depth=minimum_depth(entity,camera);minimum=std::min(minimum,depth);++frames;
        require(depth>=camera.near_plane,
                "Configured summed-radius arrival fails animated native near-plane clearance");
        if(angle==0.f&&(frame==90||frame==359||frame==719)) {
          const auto name=std::string(variant.id)+"-frame-"+std::to_string(frame)+".ppm";
          native.draw(entity,camera,output.empty()?std::filesystem::path{}:output/name);
        }
      }
      const auto& settled=npcs.agents()[0];
      const float stop=player_radius+settled.radius;
      const float distance=locomotion_detail::distance_xz(settled.position,settled.chase_target);
      require(distance>=stop-.0001f&&distance<=stop+.0011f&&settled.actual_speed==0.f,
              "Production-model fixture did not reach and settle at configured arrival");
      const float settled_depth=minimum_depth(entity,camera);
      // Same actual settled animated mesh at the old root distance. Diagnostic
      // negative control only: this transform never enters product simulation.
      entity.transform.position.x=std::sin(angle)*.2f;
      entity.transform.position.z=-std::cos(angle)*.2f;
      const float old_depth=minimum_depth(entity,camera);
      require(old_depth<camera.near_plane,"20 cm negative control did not exercise near clipping");
      ++negative_controls;
      if(angle==0.f)native.draw(entity,camera,output.empty()?std::filesystem::path{}:
          output/(std::string(variant.id)+"-old-20cm-negative.ppm"));
      std::cout<<"variant="<<variant.id<<" angle="<<angle<<" speed="<<settled.chase_speed
               <<" radius="<<settled.radius<<" player_radius="<<player_radius<<" arrival="<<distance
               <<" minimum_depth="<<minimum<<" settled_depth="<<settled_depth
               <<" old_20cm_depth="<<old_depth<<'\n';
    }
    std::cout<<"PASS: "<<frames<<" animated production Near-mesh frames; "<<negative_controls
             <<" negative controls; no global collision or ordinary rare-spawn navigation claim\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
