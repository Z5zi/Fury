#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

using namespace fury;
static Vec3 joint(const CharacterRig& rig,const CharacterPose& pose,CharacterBone b) {
  return transform_point(pose.skin_matrices[bone_index(b)],rig.joints[bone_index(b)].bind_position);
}
int main(int argc,char**argv) {
  const float speed=argc>1?std::stof(argv[1]):6.5f;
  const float crouch=argc>2?std::stof(argv[2]):0.f;
  const float height=argc>3?std::stof(argv[3]):1.7f;
  auto model=make_character_model(height,CharacterRole::CrewTech,44);
  std::cout<<"phase,side,hip_y,hip_z,knee_y,knee_z,ankle_y,ankle_z,shin_degrees,knee_degrees,sole_min_y\n";
  for(int i=0;i<256;++i) {
    CharacterAnimationSample s;
    s.speed=speed;s.move_weight=1;s.crouch_weight=crouch;s.phase_cycles=i/256.;
    const auto pose=sample_character_animation(model.rig,s);
    Mesh mesh=model.bind_mesh;
    apply_character_pose(model,pose,mesh);
    for(bool left:{true,false}) {
      auto hb=left?CharacterBone::LeftThigh:CharacterBone::RightThigh;
      auto kb=left?CharacterBone::LeftShin:CharacterBone::RightShin;
      auto ab=left?CharacterBone::LeftFoot:CharacterBone::RightFoot;
      auto h=joint(model.rig,pose,hb),k=joint(model.rig,pose,kb),a=joint(model.rig,pose,ab);
      auto upper=normalize(k-h),lower=normalize(a-k);
      float knee=std::acos(std::clamp(dot(upper,lower),-1.f,1.f))*180/3.14159265f;
      float shin=std::atan2(k.z-a.z,k.y-a.y)*180/3.14159265f;
      float min_y=100;
      for(unsigned v=0;v<mesh.vertices.size();++v)
        if(model.influences[v].first==bone_index(ab))min_y=std::min(min_y,mesh.vertices[v].position.y+height*.5f);
      std::cout<<std::setprecision(9)<<s.phase_cycles<<','<<left<<','<<h.y<<','<<h.z<<','<<k.y<<','<<k.z<<','<<a.y<<','<<a.z<<','<<shin<<','<<knee<<','<<min_y<<'\n';
    }
  }
}
