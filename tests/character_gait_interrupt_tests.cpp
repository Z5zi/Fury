#define main settling_acceptance_main
#include "character_gait_settling_tests.cpp"
#undef main

int main() {
  try {
    unsigned scenarios=0,poses=0;float worst_restart=0;
    for(auto identity:{"CrewRook","CrewSparrow","PlayerBody"})
      for(float h:{1.35f,1.75f,2.1f})for(auto lod:{CharacterLod::Near,CharacterLod::Far}) {
        auto role=std::string(identity)=="CrewRook"?CharacterRole::CrewTech:std::string(identity)=="CrewSparrow"?CharacterRole::CrewScout:CharacterRole::Player;
        auto model=make_character_profile(h,role,identity,lod).model;
        for(double phase:{.125,.375,.625,.875})for(int cut:{5,14,25})for(float speed:{1.2f,6.5f,9.f,19.8f}) {
          const std::string context=std::string(identity)+" h="+std::to_string(h)+" lod="+std::to_string(int(lod))+" phase="+std::to_string(phase)+" cut="+std::to_string(cut)+" speed="+std::to_string(speed);
          auto state=stopped(h,phase);state.sample.seed=model.seed;
          CharacterAnimationInput input;input.delta_time=1.f/60;input.crouch_weight=1;input.turn_rate=0;
          for(int frame=0;frame<cut;++frame)advance_character_animation(state,input,h);
          auto previous=sample_character_animation(model.rig,state);ground(model,previous);
          input.travel_speed=speed;input.move_weight=1;input.distance_delta=speed*input.delta_time;
          for(int frame=0;frame<36;++frame) {
            advance_character_animation(state,input,h);
            auto pose=sample_character_animation(model.rig,state);
            try{ground(model,pose);}catch(const std::exception&e){throw std::runtime_error(context+" frame="+std::to_string(frame)+" "+e.what());}
            if(frame==0) {
              float difference=0;
              for(unsigned b=0;b<character_bone_count;++b) {
                const Vec3 before=transform_point(previous.skin_matrices[b],model.rig.joints[b].bind_position);
                const Vec3 after=transform_point(pose.skin_matrices[b],model.rig.joints[b].bind_position)+Vec3{0,0,input.distance_delta};
                difference=std::max(difference,length(after-before));
              }
              worst_restart=std::max(worst_restart,difference/h);
              require(difference<h*.12f,context+" interrupted recovery snapped a world-space joint; delta/h="+std::to_string(difference/h));
            }
            previous=pose;++poses;
          }
          require(!state.settling_feet&&!state.recovering_step,context+" interrupted recovery did not rejoin the gait");
          ++scenarios;
        }
      }
    std::cout<<"interrupted_scenarios="<<scenarios<<" weighted_poses="<<poses<<" maximum_first_world_joint_delta_over_height="<<worst_restart<<" maximum_knee_flexion_degrees="<<maximum_knee_flexion<<" maximum_ankle_flexion_degrees="<<maximum_ankle_flexion<<'\n';
  }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
