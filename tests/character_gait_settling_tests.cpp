#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace fury;
float maximum_knee_flexion=0.f,maximum_ankle_flexion=0.f;
void require(bool ok,const std::string& message) {if(!ok)throw std::runtime_error(message);}
Vec3 at(const CharacterModel&m,const CharacterPose&p,CharacterBone b) {
  return transform_point(p.skin_matrices[bone_index(b)],m.rig.joints[bone_index(b)].bind_position);
}
float joint_delta(const CharacterModel&m,const CharacterPose&a,const CharacterPose&b) {
  float result=0;
  for(unsigned i=0;i<character_bone_count;++i)
    result=std::max(result,length(transform_point(a.skin_matrices[i],m.rig.joints[i].bind_position)-transform_point(b.skin_matrices[i],m.rig.joints[i].bind_position)));
  return result;
}
std::array<float,2> ground(const CharacterModel&m,const CharacterPose&p) {
  for(bool right:{false,true}) {
    const auto hb=right?CharacterBone::RightThigh:CharacterBone::LeftThigh;
    const auto kb=right?CharacterBone::RightShin:CharacterBone::LeftShin;
    const auto fb=right?CharacterBone::RightFoot:CharacterBone::LeftFoot;
    const auto hip=at(m,p,hb),knee=at(m,p,kb),ankle=at(m,p,fb);
    const auto up=normalize(knee-ankle),upper=normalize(hip-knee);
    const auto forward=normalize(transform_direction(p.skin_matrices[bone_index(fb)],{0,0,1}));
    maximum_knee_flexion=std::max(maximum_knee_flexion,std::acos(std::clamp(dot(up,upper),-1.f,1.f))*180.f/3.14159265359f);
    maximum_ankle_flexion=std::max(maximum_ankle_flexion,std::fabs(std::asin(std::clamp(dot(up,forward),-1.f,1.f))*180.f/3.14159265359f));
    require(std::fabs(length(knee-hip)-length(m.rig.joints[bone_index(kb)].bind_position-m.rig.joints[bone_index(hb)].bind_position))<m.rig.height*.0001f,"Thigh length changed during recovery");
    require(std::fabs(length(ankle-knee)-length(m.rig.joints[bone_index(fb)].bind_position-m.rig.joints[bone_index(kb)].bind_position))<m.rig.height*.0001f,"Calf length changed during recovery");
  }
  auto mesh=m.bind_mesh;apply_character_pose(m,p,mesh);
  std::array<float,2> result{100,100};
  for(unsigned v=0;v<mesh.vertices.size();++v) {
    if(mesh.vertices[v].position.y<-.5f*m.rig.height-m.rig.height*.0001f)
      throw std::runtime_error("Actual weighted mesh penetrated the flat ground: vertex="+std::to_string(v)+" y/h="+std::to_string(mesh.vertices[v].position.y/m.rig.height)+" influence="+std::to_string(m.influences[v].first)+"/"+std::to_string(m.influences[v].second)+" weight="+std::to_string(m.influences[v].first_weight));
    if(m.bind_mesh.vertices[v].position.y<-.495f*m.rig.height)
      result[m.bind_mesh.vertices[v].position.x>0]=std::min(result[m.bind_mesh.vertices[v].position.x>0],mesh.vertices[v].position.y+.5f*m.rig.height);
  }
  return result;
}
CharacterAnimationState stopped(float h,double phase) {
  CharacterAnimationState s;s.initialized=s.gait_started=true;
  s.sample.phase_cycles=phase;s.sample.speed=0;s.sample.move_weight=0;
  s.sample.stride_length=character_stride_length(h,19.8f);
  s.support_fraction=character_stance_fraction(h,19.8f);
  return s;
}
int main() {
  try {
    unsigned poses=0,transitions=0;float maximum_joint_step=0,maximum_grounded_drift=0;
    for(auto identity:{"CrewRook","CrewSparrow","PlayerBody"})
      for(float h:{1.35f,1.75f,2.1f})for(auto lod:{CharacterLod::Near,CharacterLod::Far}) {
        auto role=std::string(identity)=="CrewRook"?CharacterRole::CrewTech:std::string(identity)=="CrewSparrow"?CharacterRole::CrewScout:CharacterRole::Player;
        auto model=make_character_profile(h,role,identity,lod).model;
        for(int phase=0;phase<16;++phase) {
          auto state=stopped(h,phase/16.);state.sample.seed=model.seed;
          auto previous=sample_character_animation(model.rig,state);
          auto previous_ground=ground(model,previous);
          const auto initial_phase=state.sample.phase_cycles;
          CharacterAnimationInput input;input.delta_time=1.f/60;input.crouch_weight=1;input.turn_rate=0;
          for(int frame=0;frame<90;++frame) {
            advance_character_animation(state,input,h);
            const auto pose=sample_character_animation(model.rig,state);
            const auto clear=ground(model,pose);
            const float delta=joint_delta(model,previous,pose);
            maximum_joint_step=std::max(maximum_joint_step,delta/h);
            require(delta<h*.08f,std::string(identity)+" height="+std::to_string(h)+" lod="+std::to_string(int(lod))+" phase="+std::to_string(phase)+" frame="+std::to_string(frame)+" delta/h="+std::to_string(delta/h)+" Stopped crouch recovery exceeded the 8%-height per-60Hz-frame continuity guardrail");
            require(state.sample.phase_cycles==initial_phase,"Recovery steps advanced the distance-driven gait clock without actor travel");
            require(std::min(clear[0],clear[1])<h*.0002f,"Both support soles floated during a stationary recovery step");
            for(unsigned side=0;side<2;++side)if(clear[side]<h*.0001f&&previous_ground[side]<h*.0001f) {
              const auto foot=side?CharacterBone::RightFoot:CharacterBone::LeftFoot;
              auto a=at(model,previous,foot),b=at(model,pose,foot);
              const float drift=std::hypot(a.x-b.x,a.z-b.z);
              maximum_grounded_drift=std::max(maximum_grounded_drift,drift);
              require(drift<h*.0001f,"A grounded sole skated while the actor was stationary");
            }
            previous=pose;previous_ground=clear;++poses;
          }
          require(!state.settling_feet,"Stopped crouch did not finish its finite recovery");
          for(auto foot:{CharacterBone::LeftFoot,CharacterBone::RightFoot})
            require(std::fabs(at(model,previous,foot).z-model.rig.joints[bone_index(foot)].bind_position.z)<h*.0251f,"Settled crouch retained a wide longitudinal stance");
          CharacterAnimationSample upright;upright.seed=model.seed;upright.idle_time=state.sample.idle_time;
          const auto standing=sample_character_animation(model.rig,upright);
          require(at(model,standing,CharacterBone::Head).y-at(model,previous,CharacterBone::Head).y>h*.25f,"Settled crouch failed the existing full-depth presentation target");
          input.crouch_weight=0;
          for(int frame=0;frame<90;++frame) {
            advance_character_animation(state,input,h);
            auto pose=sample_character_animation(model.rig,state);ground(model,pose);
            for(auto foot:{CharacterBone::LeftFoot,CharacterBone::RightFoot})
              require(length(at(model,pose,foot)-at(model,previous,foot))<h*.0001f,"Crouch release moved a settled planted foot");
            previous=pose;++poses;
          }
          ++transitions;
        }
      }
    std::cout<<"settling_transitions="<<transitions<<" weighted_mesh_poses="<<poses<<" maximum_joint_step_over_height="<<maximum_joint_step<<" grounded_drift_m="<<maximum_grounded_drift<<" maximum_knee_flexion_degrees="<<maximum_knee_flexion<<" maximum_ankle_flexion_degrees="<<maximum_ankle_flexion<<'\n';
    return 0;
  }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
