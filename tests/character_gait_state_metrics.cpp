#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#ifndef GAIT_BASELINE_ABI
#include "character_gait_trace_inputs.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace fury;
Vec3 position(const CharacterModel& m,const CharacterPose& p,CharacterBone b) {
  return transform_point(p.skin_matrices[bone_index(b)],m.rig.joints[bone_index(b)].bind_position);
}
int main(int argc,char**argv) {
  if(argc<2)return 2;
  const float h=argc>2?std::stof(argv[2]):1.75f;
  const bool far=argc>3&&std::string(argv[3])=="far";
  auto profile=make_character_profile(h,CharacterRole::Player,"PlayerBody",far?CharacterLod::Far:CharacterLod::Near);
  const auto&m=profile.model;
  std::ifstream file(argv[1]);std::string line;
  std::cout<<std::setprecision(10)<<"frame,crouch,move,speed,head_y,pelvis_y,pelvis_z,max_joint_delta,ground_min,left_ankle_y,left_ankle_z,left_shin_degrees,left_sole_min,right_ankle_y,right_ankle_z,right_shin_degrees,right_sole_min,body_max_y,head_max_y,torso_max_y,head_min_x,head_max_x,head_min_z,head_max_z\n";
  CharacterPose previous;bool has_previous=false;
  while(std::getline(file,line)) {
    int frame,visible;CharacterAnimationState state{};auto&s=state.sample;
    std::istringstream in(line);
    in>>frame>>visible>>s.phase_cycles>>s.idle_time>>s.speed>>s.stride_length>>s.move_weight>>s.turn_rate>>s.crouch_weight>>s.attention_yaw>>s.attention_pitch>>s.attention_weight>>s.talk_weight>>s.seed>>state.previous_yaw>>state.initialized>>state.gait_started>>state.support_fraction;
    if(!in)return 3;
#ifndef GAIT_BASELINE_ABI
    read_gait_extension(in,state);
#endif
    const auto pose=sample_character_animation(m.rig,state);
    auto mesh=m.bind_mesh;apply_character_pose(m,pose,mesh);
    float delta=0,ground=100;float soles[2]{100,100};
    float body_max=-100,head_max=-100,torso_max=-100;
    Vec3 head_lo{100,100,100},head_hi{-100,-100,-100};
    for(unsigned b=0;b<character_bone_count;++b)if(has_previous)
      delta=std::max(delta,length(transform_point(pose.skin_matrices[b],m.rig.joints[b].bind_position)-transform_point(previous.skin_matrices[b],m.rig.joints[b].bind_position)));
    for(unsigned v=0;v<mesh.vertices.size();++v) {
      ground=std::min(ground,mesh.vertices[v].position.y+h*.5f);
      body_max=std::max(body_max,mesh.vertices[v].position.y+h*.5f);
      const auto bone=m.influences[v].first;
      if(bone==bone_index(CharacterBone::Head)) {
        const auto q=mesh.vertices[v].position;
        head_max=std::max(head_max,q.y+h*.5f);
        head_lo={std::min(head_lo.x,q.x),std::min(head_lo.y,q.y),std::min(head_lo.z,q.z)};
        head_hi={std::max(head_hi.x,q.x),std::max(head_hi.y,q.y),std::max(head_hi.z,q.z)};
      }
      if(bone==bone_index(CharacterBone::Pelvis)||bone==bone_index(CharacterBone::Spine)||bone==bone_index(CharacterBone::Chest))
        torso_max=std::max(torso_max,mesh.vertices[v].position.y+h*.5f);
      if(m.bind_mesh.vertices[v].position.y<-.495f*h)
        soles[m.bind_mesh.vertices[v].position.x>0]=std::min(soles[m.bind_mesh.vertices[v].position.x>0],mesh.vertices[v].position.y+h*.5f);
    }
    auto head=position(m,pose,CharacterBone::Head),pelvis=position(m,pose,CharacterBone::Pelvis);
    std::cout<<frame<<','<<s.crouch_weight<<','<<s.move_weight<<','<<s.speed<<','<<head.y<<','<<pelvis.y<<','<<pelvis.z<<','<<delta<<','<<ground;
    for(bool right:{false,true}) {
      auto ankle=position(m,pose,right?CharacterBone::RightFoot:CharacterBone::LeftFoot);
      auto knee=position(m,pose,right?CharacterBone::RightShin:CharacterBone::LeftShin);
      std::cout<<','<<ankle.y<<','<<ankle.z<<','<<std::atan2(knee.z-ankle.z,knee.y-ankle.y)*180/3.14159265359<<','<<soles[right];
    }
    std::cout<<','<<body_max<<','<<head_max<<','<<torso_max<<','<<head_lo.x<<','<<head_hi.x<<','<<head_lo.z<<','<<head_hi.z<<'\n';previous=pose;has_previous=true;
  }
}
