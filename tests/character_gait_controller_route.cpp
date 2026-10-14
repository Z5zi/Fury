// Read-only production Camera + collision + animation replay. No render/gameplay
// callback is replaced, and no claim is made that this is a full Vaultline run.
#include "fury/camera.hpp"
#include "fury/character_animation.hpp"
#include "fury/character_model.hpp"
#include "fury/character_profile.hpp"
#include "fury/collision.hpp"
#include "character_gait_trace_inputs.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
int main(int argc,char**argv){
 if(argc!=7)return 2;
 std::ifstream file(argv[1]);unsigned n{};file>>n;std::vector<fury::Aabb> solids;
 for(unsigned i=0;i<n;++i){fury::Vec3 lo,hi;file>>lo.x>>lo.y>>lo.z>>hi.x>>hi.y>>hi.z;solids.push_back({(lo+hi)*.5f,(hi-lo)*.5f});}
 std::ofstream states(argv[2]),positions(argv[3]),metrics(argv[4]),controls(argv[6]);controls<<std::setprecision(17);states<<std::setprecision(17);positions<<std::setprecision(17);metrics<<std::setprecision(17);
 const int style=std::getenv("FURY_ROUTE_STYLE")?std::atoi(std::getenv("FURY_ROUTE_STYLE")):0;const float facing_offset=style==1?3.14159265359f:style==2?1.57079632679f:style==3?-1.57079632679f:0.f;
 fury::Camera camera;camera.position={0,1.7f,12};camera.yaw=-1.5707963f;camera.pitch=-.08f;camera.fly_mode=false;camera.move_speed=9;camera.snap_look();
 const auto profile=fury::make_character_profile(1.75f,fury::CharacterRole::Player,"PlayerBody",fury::CharacterLod::Near);
 fury::CharacterAnimationState state;state.profile=profile.animation_profile;state.sample.seed=profile.model.seed;state.sample.idle_time=double(state.sample.seed%1024)/91.;
 std::ifstream route(argv[5]);unsigned waypoint_count{};route>>waypoint_count;std::vector<fury::Vec3> waypoints(waypoint_count);for(auto&q:waypoints){route>>q.x>>q.z;q.y=1.7f;}unsigned waypoint=0;fury::Vec3 target=waypoints[waypoint];float move=0;const float dt=1.f/60;float minimum=100;
 metrics<<"frame,x,z,dx,dz,crouching,speed,remaining\n";
 for(unsigned frame=0;frame<750;++frame){
  auto previous=camera.position;if(waypoint+1<waypoints.size()&&std::hypot(target.x-previous.x,target.z-previous.z)<.1f){target=waypoints[++waypoint];}float remaining=std::hypot(target.x-previous.x,target.z-previous.z);minimum=std::min(minimum,remaining);
  fury::InputState input{};input.key_ctrl=true;
  if(frame<650){input.mouse_dx=std::remainder(std::atan2(target.z-previous.z,target.x-previous.x)+facing_offset-camera.yaw_target,6.28318530718f)/camera.mouse_sensitivity;if(frame>=45&&remaining>.035f&&std::fabs(std::remainder(camera.yaw_target-camera.yaw,6.28318530718f))<.12f){if(style==1)input.key_s=true;else if(style==2)input.key_a=true;else if(style==3)input.key_d=true;else input.key_w=true;}}
  controls<<frame<<' '<<input.key_w<<' '<<input.key_ctrl<<' '<<input.mouse_dx<<'\n';
  camera.update(input,dt);auto desired=camera.position;camera.position=fury::resolve_player_collision(camera.position,.45f,solids,1.7f);camera.position.y=1.7f;
  float dx=camera.position.x-desired.x,dz=camera.position.z-desired.z,c2=dx*dx+dz*dz;if(c2>1e-8f){float inv=1/std::sqrt(c2),nx=dx*inv,nz=dz*inv,into=camera.velocity.x*nx+camera.velocity.z*nz;if(into<0){camera.velocity.x-=nx*into;camera.velocity.z-=nz*into;}}
  float speed=std::hypot(camera.velocity.x,camera.velocity.z);if(speed>.2f)move=1;else move=std::max(0.f,move-dt*4);
  fury::CharacterAnimationInput anim;anim.delta_time=dt;anim.distance_delta=frame?std::min(1.f,std::hypot(camera.position.x-previous.x,camera.position.z-previous.z)):0;anim.travel_speed=speed;anim.move_weight=move;anim.yaw=1.57079632679f-camera.yaw;anim.crouch_weight=1;
  fury::advance_character_animation(state,anim,1.75f,profile.animation_profile);auto&s=state.sample;
  states<<frame<<" 1 "<<s.phase_cycles<<' '<<s.idle_time<<' '<<s.speed<<' '<<s.stride_length<<' '<<s.move_weight<<' '<<s.turn_rate<<' '<<s.crouch_weight<<' '<<s.attention_yaw<<' '<<s.attention_pitch<<' '<<s.attention_weight<<' '<<s.talk_weight<<' '<<s.seed<<' '<<state.previous_yaw<<' '<<state.initialized<<' '<<state.gait_started<<' '<<state.support_fraction;write_gait_extension(states,state);states<<'\n';
  positions<<frame<<' '<<camera.position.x<<" .875 "<<camera.position.z<<' '<<anim.yaw<<'\n';metrics<<frame<<','<<camera.position.x<<','<<camera.position.z<<','<<dx<<','<<dz<<','<<camera.crouching<<','<<speed<<','<<remaining<<'\n';
 }
 std::cout<<"minimum_waypoint_distance_m="<<minimum<<" final_waypoint="<<waypoint<<" final_x="<<camera.position.x<<" final_z="<<camera.position.z<<'\n';
 return file&&states&&positions&&metrics?0:3;
}
