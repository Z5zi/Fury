#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_animation_trace.hpp"
#include "../apps/vaultline/npc_presentation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <string>

namespace fury { namespace animation_reference {
void advance_character_animation(CharacterAnimationState&, const CharacterAnimationInput&, float);
CharacterPose sample_character_animation(const CharacterRig&, const CharacterAnimationSample&);
CharacterPose sample_character_animation(const CharacterRig&, const CharacterAnimationState&);
} namespace animation_detail { namespace equipped_crouch {
void advance_character_animation(CharacterAnimationState&, const CharacterAnimationInput&, float);
CharacterPose sample_character_animation(const CharacterRig&, const CharacterAnimationSample&);
CharacterPose sample_character_animation(const CharacterRig&, const CharacterAnimationState&);
}}}
namespace {
using namespace fury;
using P = CharacterAnimationProfile;
std::size_t checks{};
void require(bool value, const char* message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
template<class T> bool bits(const T& a, const T& b) {
  return std::memcmp(&a, &b, sizeof(T)) == 0;
}
void same_sample(const CharacterAnimationSample& a, const CharacterAnimationSample& b) {
#define SAME(field) require(bits(a.field,b.field), "Sample mismatch: " #field)
  SAME(phase_cycles); SAME(idle_time); SAME(speed); SAME(stride_length); SAME(move_weight);
  SAME(turn_rate); SAME(attention_yaw); SAME(attention_pitch); SAME(attention_weight);
  SAME(talk_weight); SAME(crouch_weight); SAME(seed);
#undef SAME
}
void same_state(const CharacterAnimationState& a, const CharacterAnimationState& b) {
  same_sample(a.sample,b.sample);
#define SAME(field) require(bits(a.field,b.field), "State mismatch: " #field)
  SAME(previous_yaw); SAME(initialized); SAME(gait_started); SAME(support_fraction);
  SAME(settling_feet); SAME(settled_feet); SAME(settling_index); SAME(settling_first);
  SAME(settling_time); SAME(settling_z); SAME(settling_start_z); SAME(settling_start_lift);
  SAME(settling_start_pitch); SAME(settling_lift); SAME(settling_pitch);
  SAME(recovery_time); SAME(recovering_step); SAME(recovery_foot); SAME(recovery_z);
  SAME(recovery_lift); SAME(recovery_pitch); SAME(recovery_root_z);
#undef SAME
}
void same_pose(const CharacterPose& a, const CharacterPose& b) {
  for (std::size_t bone=0; bone<character_bone_count; ++bone)
    for(int row=0;row<4;++row) for(int col=0;col<4;++col)
      require(bits(a.skin_matrices[bone].at(col,row),b.skin_matrices[bone].at(col,row)),
              "Pose matrix differs at bit level");
}
CharacterAnimationInput input_at(int frame) {
  CharacterAnimationInput input;
  input.delta_time = (frame % 71 == 0) ? .25f : 1.f/60.f;
  const int stage = (frame / 47) % 9;
  input.travel_speed = stage == 0 || stage == 4 || stage == 7 ? 0.f :
                       stage == 1 ? .22f : stage == 2 ? 1.8f : stage == 3 ? 5.5f : 1.15f;
  input.distance_delta = input.travel_speed*input.delta_time;
  input.move_weight = input.travel_speed > 0.f ? 1.f : 0.f;
  input.crouch_weight = stage >= 3 && stage <= 7 ? 1.f : .13f;
  input.yaw = float(frame)*.017f;
  input.turn_rate = frame % 2 ? std::numeric_limits<float>::quiet_NaN() : .5f;
  input.attention_weight = .7f;
  input.attention_yaw = .3f*std::sin(frame*.06f);
  input.attention_pitch = .2f*std::cos(frame*.03f);
  input.talk_weight = frame % 13 ? .6f : 0.f;
  if(frame % 137 == 0) input.delta_time = 0.f;
  if(frame % 139 == 0) input.delta_time = -1.f;
  if(frame % 149 == 0) input.delta_time = std::numeric_limits<float>::quiet_NaN();
  if(frame % 151 == 0) input.delta_time = std::numeric_limits<float>::infinity();
  if(frame % 157 == 0) input.distance_delta = 1000.f;
  return input;
}
void trace_contract() {
  for(auto policy:{P::Generic,P::EquippedCrouchExperimental}) {
    CharacterAnimationState source;source.profile=policy;source.sample.seed=901u;
    for(int frame=0;frame<330;++frame) {
      advance_character_animation(source,input_at(frame),1.8f,policy);
      std::ostringstream out;diagnostics::write_character_animation_extension(out,source);
      CharacterAnimationState restored=source;
      restored.profile=P::Generic;restored.recovery_root_z=100.f;
      std::istringstream in(out.str());
      require(diagnostics::read_character_animation_extension(in,restored)==4,"Trace omitted v4");
      same_state(source,restored);require(restored.profile==policy,"Trace lost bound profile");
      std::istringstream split(out.str());std::vector<std::string> tokens;std::string token;
      while(split>>token) tokens.push_back(token);
      for(unsigned version:{2u,3u}) {
        std::ostringstream old;old<<version;
        for(std::size_t i=1;i<tokens.size()-(version==2?2:1);++i) old<<' '<<tokens[i];
        restored=source;std::istringstream archived(old.str());
        require(diagnostics::read_character_animation_extension(archived,restored)==version,"Archived version lost");
        require(restored.profile==P::Generic,"Archive inferred opt-in policy without metadata");
        require(version!=2 || restored.recovery_root_z==0.f,"v2 retained stale recovery root");
      }
      std::ostringstream bad;bad<<4;
      for(std::size_t i=1;i+1<tokens.size();++i) bad<<' '<<tokens[i];
      bad<<" 255";
      bool rejected=false;restored=source;std::istringstream unknown(bad.str());
      try {diagnostics::read_character_animation_extension(unknown,restored);}
      catch(const std::runtime_error&) {rejected=true;}
      require(rejected,"Trace accepted unknown profile");same_state(source,restored);
      require(restored.profile==policy,"Rejected trace mutated profile");
    }
  }
}
void generic_equivalence() {
  for (float height : {.4f,1.63f,1.8f,2.1f})
    for (auto seed : {0u,1917480204u,0xffffffffu,character_seed("CrewRook"),character_seed("GhostLoop")}) {
      const auto model = make_character_model(height,CharacterRole::CrewTech,seed,CharacterLod::Far);
      CharacterAnimationState expected, legacy, selected;
      expected.sample.seed=legacy.sample.seed=selected.sample.seed=seed;
      expected.sample.idle_time=legacy.sample.idle_time=selected.sample.idle_time=double(seed%1024u)/91.;
      for(int frame=0;frame<850;++frame) {
        const auto input=input_at(frame);
        animation_reference::advance_character_animation(expected,input,height);
        advance_character_animation(legacy,input,height);
        advance_character_animation(selected,input,height,P::Generic);
        require(legacy.profile==P::Generic && selected.profile==P::Generic,"Generic selection changed");
        same_state(expected,legacy);same_state(expected,selected);
        same_pose(animation_reference::sample_character_animation(model.rig,expected),
                  sample_character_animation(model.rig,selected));
        same_pose(animation_reference::sample_character_animation(model.rig,expected.sample),
                  sample_character_animation(model.rig,selected.sample,P::Generic));
        same_pose(sample_character_animation(model.rig,expected.sample),
                  sample_character_animation(model.rig,selected.sample,P::Generic));
      }
    }
}
void explicit_dispatch() {
  const auto model=make_character_model(1.8f,CharacterRole::Commuter,991u,CharacterLod::Far);
  for(auto seed : {0u,1u,1917480204u,0xffffffffu}) {
    CharacterAnimationState direct,selected;
    direct.sample.seed=selected.sample.seed=seed;
    selected.profile=P::EquippedCrouchExperimental;
    for(int frame=0;frame<650;++frame) {
      const auto input=input_at(frame);
      animation_detail::equipped_crouch::advance_character_animation(direct,input,model.rig.height);
      if(frame%2) advance_character_animation(selected,input,model.rig.height,P::EquippedCrouchExperimental);
      else advance_character_animation(selected,input,model.rig.height);
      same_state(direct,selected);
      require(selected.profile==P::EquippedCrouchExperimental,"Height-only advance lost bound profile");
      same_pose(animation_detail::equipped_crouch::sample_character_animation(model.rig,direct),
                sample_character_animation(model.rig,selected));
      same_pose(animation_detail::equipped_crouch::sample_character_animation(model.rig,direct.sample),
                sample_character_animation(model.rig,selected.sample,P::EquippedCrouchExperimental));
    }
  }
  CharacterAnimationSample sample;sample.crouch_weight=1.f;sample.seed=1917480204u;
  const auto generic=sample_character_animation(model.rig,sample);
  const auto equipped=sample_character_animation(model.rig,sample,P::EquippedCrouchExperimental);
  require(!bits(generic.skin_matrices,equipped.skin_matrices),"Experimental dispatch never changed numerical branch");
  same_pose(generic,sample_character_animation(model.rig,sample,static_cast<P>(255)));
  CharacterAnimationState state;state.sample.seed=991u;state.sample.idle_time=4.;
  auto before=state;CharacterAnimationInput invalid;invalid.delta_time=-1.f;
  advance_character_animation(state,invalid,1.8f,P::EquippedCrouchExperimental);
  same_state(before,state);require(state.profile==P::Generic,"Invalid dt changed policy");
  auto input=input_at(1);advance_character_animation(state,input,1.8f,P::EquippedCrouchExperimental);
  require(state.profile==P::EquippedCrouchExperimental && state.sample.seed==991u,"Explicit binding failed");
  advance_character_animation(state,input,1.8f,P::Generic);
  require(state.profile==P::Generic && state.sample.seed==991u && state.recovery_root_z==0.f,"Policy reset failed");
}
struct Asset {const char* id;CharacterRole role;};
const std::array<Asset,18> assets{{
 {"NpcCivA",CharacterRole::Commuter},{"NpcCivB",CharacterRole::Market},
 {"NpcCivC",CharacterRole::Commuter},{"NpcBankCust",CharacterRole::Commuter},
 {"NpcTeller",CharacterRole::BankStaff},{"NpcCivAsh",CharacterRole::Market},
 {"NpcCivD",CharacterRole::Dock},{"NpcCivE",CharacterRole::Dock},
 {"NpcGuard",CharacterRole::Security},{"NpcAlleyHmpd",CharacterRole::Security},
 {"NpcExtraGuard",CharacterRole::Security},{"NpcFence",CharacterRole::Fence},
 {"NpcDeskGuard",CharacterRole::Security},{"CrewRook",CharacterRole::CrewTech},
 {"CrewSparrow",CharacterRole::CrewScout},{"NpcEnforcer",CharacterRole::Enforcer},
 {"GhostLoop",CharacterRole::Ghost},{"PlayerBody",CharacterRole::Player}}};
P player_policy() {
#if FURY_ENABLE_EXPERIMENTAL_EQUIPPED_CROUCH
  return P::EquippedCrouchExperimental;
#else
  return P::Generic;
#endif
}
void factory_contract() {
  for(const auto& asset:assets) for(auto lod:{CharacterLod::Near,CharacterLod::Far}) {
    const auto p=make_character_profile(1.8f,asset.role,asset.id,lod);
    const P expected=asset.role==CharacterRole::Player ? player_policy() : P::Generic;
    require(p.animation_profile==expected,"Factory motion metadata mismatch");
    if(expected==P::Generic) {
      CharacterAnimationState state;state.sample.seed=p.model.seed;
      for(int frame=0;frame<450;++frame) {
        advance_character_animation(state,input_at(frame),1.8f,p.animation_profile);
        if(frame%47==0)
          same_pose(animation_reference::sample_character_animation(p.model.rig,state),
                    sample_character_animation(p.model.rig,state));
      }
    }
  }
  for(const auto& asset:std::array<Asset,4>{{{"UnrecognizedPlayer",CharacterRole::Player},
      {"PlayerBody",CharacterRole::CrewTech},{"CrewRook",CharacterRole::Player},
      {"GhostLoop",CharacterRole::Player}}}) {
    const auto p=make_character_profile(1.8f,asset.role,asset.id,CharacterLod::Far);
    require(p.animation_profile==P::Generic && !p.individualized,"Unknown/mismatched asset opted in");
  }
}
void same_mesh(const Mesh& a,const Mesh& b) {
  require(a.indices==b.indices && a.vertices.size()==b.vertices.size(),"Runtime topology changed");
  for(std::size_t i=0;i<a.vertices.size();++i) {
    require(bits(a.vertices[i].position,b.vertices[i].position),"Runtime position differs from selected pose");
    require(bits(a.vertices[i].normal,b.vertices[i].normal),"Runtime normal differs from selected pose");
  }
}
void runtime_contract() {
  for(const auto& asset:std::array<Asset,4>{{{"PlayerBody",CharacterRole::Player},
      {"CrewRook",CharacterRole::CrewTech},{"CrewSparrow",CharacterRole::CrewScout},
      {"GhostLoop",CharacterRole::Ghost}}}) {
    Scene scene;Entity entity;entity.name=asset.id;
    vaultline::NpcPresentation presentation;
    presentation.install(scene,entity,1.8f,asset.role,{});
    auto* near_handle=entity.mesh;auto* far_handle=entity.lod_mesh;
    const auto near=make_character_profile(1.8f,asset.role,asset.id,CharacterLod::Near);
    const auto far=make_character_profile(1.8f,asset.role,asset.id,CharacterLod::Far);
    CharacterAnimationState state;
    require(presentation.animation_state_for_review(asset.id,state),"Missing runtime state");
    require(state.profile==near.animation_profile,"Install lost profile metadata");
    auto expected_near=near.model.bind_mesh,expected_far=far.model.bind_mesh;
    apply_character_pose(near.model,sample_character_animation(near.model.rig,state),expected_near);
    apply_character_pose(far.model,sample_character_animation(far.model.rig,state),expected_far);
    same_mesh(*entity.mesh,expected_near);same_mesh(*entity.lod_mesh,expected_far);
    CharacterAnimationState expected_state=state;
    vaultline::NpcMotionSample motion;motion.position={0,.9f,0};
    double previous_travel=0.;
    for(int frame=0;frame<190;++frame) {
      const auto input=input_at(frame);motion.actual_speed=input.travel_speed;
      motion.move_weight=input.move_weight;motion.crouch=input.crouch_weight;
      motion.yaw=input.yaw;motion.travel_distance+=input.distance_delta;
      motion.visible=frame%31!=0;
      CharacterAnimationInput expected_input;
      expected_input.delta_time=1.f/60.f;
      expected_input.travel_speed=motion.actual_speed;
      expected_input.move_weight=motion.move_weight;
      expected_input.crouch_weight=motion.crouch;
      expected_input.yaw=motion.yaw;
      expected_input.turn_rate=motion.turn_rate;
      expected_input.distance_delta=frame==0 ? 0.f : float(std::clamp(motion.travel_distance-previous_travel,0.,10.));
      previous_travel=motion.travel_distance;
      advance_character_animation(expected_state,expected_input,1.8f,near.animation_profile);
      presentation.advance(asset.id,motion,1.f/60.f,{0,.9f,near.lod_distance},near.lod_distance);
      presentation.animation_state_for_review(asset.id,state);
      same_state(expected_state,state);
    }
    require(presentation.animation_state_for_review(asset.id,state),"Missing advanced state");
    require(state.profile==near.animation_profile,"Advance lost profile metadata");
    const auto frozen=state;
    // Skinning caches sub-micrometre positions. Start both refresh paths from
    // the same submitted mesh so different update histories do not confound
    // exact profile-dispatch verification.
    expected_near=*entity.mesh;expected_far=*entity.lod_mesh;
    require(presentation.refresh_for_review(asset.id,near.lod_distance,near.lod_distance),"Review refresh failed");
    CharacterAnimationState after;presentation.animation_state_for_review(asset.id,after);
    same_state(frozen,after);require(after.profile==frozen.profile,"Refresh changed profile");
    apply_character_pose(near.model,sample_character_animation(near.model.rig,state),expected_near);
    apply_character_pose(far.model,sample_character_animation(near.model.rig,state),expected_far);
    same_mesh(*entity.mesh,expected_near);same_mesh(*entity.lod_mesh,expected_far);
    presentation.install(scene,entity,1.8f,asset.role,{});
    presentation.animation_state_for_review(asset.id,state);
    require(state.profile==near.animation_profile && !state.initialized,"Reinstall lost profile/reset contract");
    require(entity.mesh==near_handle && entity.lod_mesh==far_handle,"Reinstall replaced geometry handles");
  }
  Scene scene;Entity entity;entity.name="PlayerBody";
  vaultline::NpcPresentation legacy(true,false);
  legacy.install(scene,entity,1.8f,CharacterRole::Player,{});
  CharacterAnimationState state;legacy.animation_state_for_review(entity.name,state);
  require(state.profile==P::Generic,"Legacy runtime opted in");
}
}
int main() {
  try {
    generic_equivalence();explicit_dispatch();trace_contract();factory_contract();runtime_contract();
    std::cout<<"Profile dispatch: "<<checks<<" checks passed; Player factory = "
             <<character_animation_profile_name(player_policy())<<"\n";
    return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<" after "<<checks<<" checks\n";return 1;}
}
