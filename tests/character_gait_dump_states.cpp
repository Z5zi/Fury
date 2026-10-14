#include "character_gait_trace_inputs.hpp"
#include <iomanip>
#include <iostream>
int main(int argc,char**argv) {
  if(argc<2)return 2;
  // Input-only recordings have no asset metadata. Require a separate explicit
  // profile selection instead of inferring it from the historical Player seed.
  auto profile=fury::CharacterAnimationProfile::Generic;
  if(argc>3) {
    const std::string requested=argv[3];
    if(requested=="equipped_crouch_experimental") profile=fury::CharacterAnimationProfile::EquippedCrouchExperimental;
    else if(requested!="generic") throw std::runtime_error("Unknown requested animation profile");
  }
  std::cout<<std::setprecision(17);
  for(const auto& f:replay_gait_inputs(argv[1],argc>2?std::stof(argv[2]):1.75f,profile)) {
    const auto&st=f.state;const auto&s=st.sample;
    std::cout<<f.frame<<' '<<f.visible<<' '<<s.phase_cycles<<' '<<s.idle_time<<' '<<s.speed<<' '<<s.stride_length<<' '<<s.move_weight<<' '<<s.turn_rate<<' '<<s.crouch_weight<<' '<<s.attention_yaw<<' '<<s.attention_pitch<<' '<<s.attention_weight<<' '<<s.talk_weight<<' '<<s.seed<<' '<<st.previous_yaw<<' '<<st.initialized<<' '<<st.gait_started<<' '<<st.support_fraction;
    write_gait_extension(std::cout,st);std::cout<<'\n';
  }
}
