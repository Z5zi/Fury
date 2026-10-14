#pragma once

#include "fury/character_animation.hpp"
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace fury { namespace diagnostics {
/// Diagnostic state extension, not the multiplayer wire protocol. Version 4
/// appends recovery_root_z and an explicit numeric motion-profile selection to
/// the version-2 fields. Archived v2 and prototype v3 traces load as Generic;
/// neither can certify that a production runtime used the opt-in policy.
inline void write_character_animation_extension(std::ostream& out,
                                                const CharacterAnimationState& s) {
  const auto precision=out.precision();
  out<<std::setprecision(17)<<" 4 "<<s.settling_feet<<' '<<s.settled_feet<<' '
     <<s.settling_index<<' '<<s.settling_first<<' '<<s.settling_time;
  for(const auto* a:{&s.settling_z,&s.settling_start_z,&s.settling_start_lift,
                    &s.settling_start_pitch,&s.settling_lift,&s.settling_pitch})
    for(float x:*a) out<<' '<<x;
  out<<' '<<s.recovery_time<<' '<<s.recovering_step<<' '<<s.recovery_foot;
  for(const auto* a:{&s.recovery_z,&s.recovery_lift,&s.recovery_pitch})
    for(float x:*a) out<<' '<<x;
  out<<' '<<s.recovery_root_z<<' '<<unsigned(s.profile);
  out.precision(precision);
}
inline unsigned read_character_animation_extension(std::istream& in,
                                                   CharacterAnimationState& state) {
  CharacterAnimationState s;
  s.sample=state.sample;
  s.previous_yaw=state.previous_yaw;
  s.initialized=state.initialized;
  s.gait_started=state.gait_started;
  s.support_fraction=state.support_fraction;
  unsigned version{};
  if(!(in>>version)) {
    if(!in.eof()) throw std::runtime_error("Invalid diagnostic state version");
    in.clear();state=s;return 0;
  }
  if(version!=2 && version!=3 && version!=4)
    throw std::runtime_error("Unsupported diagnostic state version");
  in>>s.settling_feet>>s.settled_feet>>s.settling_index>>s.settling_first>>s.settling_time;
  for(auto* a:{&s.settling_z,&s.settling_start_z,&s.settling_start_lift,
               &s.settling_start_pitch,&s.settling_lift,&s.settling_pitch})
    for(float& x:*a) in>>x;
  in>>s.recovery_time>>s.recovering_step>>s.recovery_foot;
  for(auto* a:{&s.recovery_z,&s.recovery_lift,&s.recovery_pitch})
    for(float& x:*a) in>>x;
  if(version>=3) in>>s.recovery_root_z;
  if(version==4) {
    unsigned profile{};in>>profile;
    if(profile>unsigned(CharacterAnimationProfile::EquippedCrouchExperimental))
      throw std::runtime_error("Unknown diagnostic animation profile");
    s.profile=static_cast<CharacterAnimationProfile>(profile);
  }
  if(!in) throw std::runtime_error("Incomplete diagnostic foot/profile state");
  state=s;
  return version;
}
}} // namespace fury::diagnostics
