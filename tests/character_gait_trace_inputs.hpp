#pragma once
#include "fury/character_animation.hpp"
#include "fury/character_animation_trace.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

inline void write_gait_extension(std::ostream& out,const fury::CharacterAnimationState& s) {
  fury::diagnostics::write_character_animation_extension(out,s);
}
inline unsigned read_gait_extension(std::istream& in,fury::CharacterAnimationState& s) {
  return fury::diagnostics::read_character_animation_extension(in,s);
}

struct GaitTraceState {
  int frame{},visible{};
  fury::CharacterAnimationState state;
};
inline std::vector<GaitTraceState> replay_gait_inputs(const char* path,float height,
    fury::CharacterAnimationProfile profile=fury::CharacterAnimationProfile::Generic) {
  std::ifstream file(path);std::string line;
  fury::CharacterAnimationState state;
  state.profile=profile;
  state.sample.seed=1917480204u;
  state.sample.idle_time=2.9450549450549453;
  std::vector<GaitTraceState> result;
  while(std::getline(file,line)) {
    int frame{},visible{};fury::CharacterAnimationInput input;
    std::istringstream in(line);
    in>>frame>>visible>>input.delta_time>>input.distance_delta>>input.travel_speed>>input.move_weight>>input.yaw>>input.crouch_weight;
    if(!in)throw std::runtime_error("Incomplete recorded gameplay input");
    if(visible)fury::advance_character_animation(state,input,height,profile);
    result.push_back({frame,visible,state});
  }
  return result;
}
