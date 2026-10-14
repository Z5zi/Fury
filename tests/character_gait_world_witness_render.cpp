// Isolated actual-mesh diagnostic, not an in-game screenshot. Pose and props are
// unaltered; orange prop material and isolated lighting expose contact witnesses.
#include "fury/character_profile.hpp"
#include "fury/character_animation.hpp"
#include "fury/renderer.hpp"
#include "character_gait_trace_inputs.hpp"
#include <SDL.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
using namespace fury;
int main(int argc,char**argv) {
 if(argc!=7)return 2;int requested=std::stoi(argv[3]);
 CharacterAnimationState state{};Vec3 center;float yaw{};std::ifstream states(argv[1]),places(argv[2]);std::string line;bool found=false;
 while(std::getline(states,line)){int frame,visible,other;std::istringstream in(line);auto&s=state.sample;in>>frame>>visible>>s.phase_cycles>>s.idle_time>>s.speed>>s.stride_length>>s.move_weight>>s.turn_rate>>s.crouch_weight>>s.attention_yaw>>s.attention_pitch>>s.attention_weight>>s.talk_weight>>s.seed>>state.previous_yaw>>state.initialized>>state.gait_started>>state.support_fraction;read_gait_extension(in,state);places>>other>>center.x>>center.y>>center.z>>yaw;if(frame==requested){found=true;break;}}
 if(!found)return 3;std::set<unsigned> ids;std::istringstream idstream(argv[5]);unsigned id;while(idstream>>id){ids.insert(id);if(idstream.peek()==',')idstream.ignore();}
 Mesh props;std::ifstream geometry(argv[4]);unsigned count{};geometry>>count;
 for(unsigned i=0;i<count;++i){unsigned entity,tri;Vec3 p[3];geometry>>entity>>tri;for(auto&v:p)geometry>>v.x>>v.y>>v.z;if(!ids.count(entity))continue;auto normal=normalize(cross(p[1]-p[0],p[2]-p[0]));for(auto v:p){Vertex q;q.position=v;q.normal=normal;q.color={1.f,.32f,.03f};props.indices.push_back(props.vertices.size());props.vertices.push_back(q);}}
 auto profile=make_character_profile(1.75f,CharacterRole::Player,"PlayerBody",CharacterLod::Near);auto mesh=profile.model.bind_mesh;apply_character_pose(profile.model,sample_character_animation(profile.model.rig,state),mesh);
 if(SDL_Init(SDL_INIT_VIDEO))return 4;const int width=640,height=640;auto*window=SDL_CreateWindow("Fury exact geometry witness",0,0,width,height,SDL_WINDOW_HIDDEN);auto backend=create_software_backend();if(!window||!backend->create(window,width,height))return 5;
 Lighting light;light.ambient={.65f,.65f,.65f};light.sun_direction={-.4f,-.8f,-.5f};light.sun_intensity=.6f;light.ao_strength=0;light.enable_shadows=light.enable_reflections=light.enable_bloom=false;light.point_light_count=0;light.fog_start=100;light.fog_end=200;
 Material material;material.roughness=.85f;material.double_sided=true;auto ground=make_plane(3.f,3.f,{.20f,.22f,.24f});
 for(unsigned view=0;view<3;++view){Vec3 target=center+Vec3{0,-.025f,0};Vec3 eye=target+(view==0?Vec3{2,.5f,2}:view==1?Vec3{2,.35f,0}:Vec3{-2,.35f,0});backend->begin_frame({22,25,29,255});backend->set_view_proj(look_at(eye,target,{0,1,0}),orthographic(-.82f,.82f,-.82f,.82f,.1f,20));backend->set_camera_position(eye);backend->set_lighting(light);backend->set_time(0);backend->draw_mesh(ground,translate({center.x,0,center.z}),material);backend->draw_mesh(props,Mat4::identity(),material);backend->draw_mesh(mesh,translate(center)*rotate_y(yaw),profile.material);std::vector<std::uint8_t> rgb;int w,h;if(!backend->read_rgb_framebuffer(rgb,w,h))return 6;std::ofstream out(std::string(argv[6])+"-view"+std::to_string(view)+".ppm",std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";out.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());}
 std::cout<<"renderer="<<backend->name()<<" frame="<<requested<<" body_vertices="<<mesh.vertices.size()<<" isolated_prop_triangles="<<props.indices.size()/3<<" validation_errors="<<backend->statistics().validation_errors<<'\n';backend->destroy();SDL_DestroyWindow(window);SDL_Quit();return 0;
}
