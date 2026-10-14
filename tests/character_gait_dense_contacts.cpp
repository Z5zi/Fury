#define main original_actor_contact_main
#if GAIT_ACTOR == 0
#include "character_rook_contact_tests.cpp"
#elif GAIT_ACTOR == 1
#include "character_sparrow_contact_tests.cpp"
#else
#include "character_player_body_contact_tests.cpp"
#endif
#undef main
#include <iomanip>

int main() {
  const char* identity=GAIT_ACTOR==0?"CrewRook":GAIT_ACTOR==1?"CrewSparrow":"PlayerBody";
  const auto role=GAIT_ACTOR==0?fury::CharacterRole::CrewTech:GAIT_ACTOR==1?fury::CharacterRole::CrewScout:fury::CharacterRole::Player;
  unsigned samples=0,failures=0;
  for(float height:{1.35f,1.75f,2.1f})for(auto lod:{fury::CharacterLod::Near,fury::CharacterLod::Far}) {
    auto profile=fury::make_character_profile(height,role,identity,lod);
    const auto&m=profile.model;const auto groups=classify(m);auto mesh=m.bind_mesh;
    negative_controls(triangles(mesh,height,groups,&m.bind_mesh));
    auto check=[&](const fury::CharacterPose& pose,const std::string& scope) {
      fury::apply_character_pose(m,pose,mesh);
      const auto failure=audit(triangles(mesh,height,groups,&m.bind_mesh));
      if(!failure.empty()) {
        ++failures;std::cerr<<identity<<" h="<<height<<" lod="<<int(lod)<<' '<<scope<<' '<<failure<<'\n';
      }
      ++samples;
    };
    for(float speed:{6.5f,9.f,19.8f})for(unsigned frame=0;frame<256;++frame) {
      fury::CharacterAnimationSample s;s.seed=m.seed;s.speed=speed;s.move_weight=1;s.phase_cycles=frame/256.;
      check(fury::sample_character_animation(m.rig,s),"upright speed="+std::to_string(speed)+" phase="+std::to_string(frame));
    }
    for(unsigned frame=0;frame<256;++frame) {
      fury::CharacterAnimationSample s;s.seed=m.seed;s.speed=height*.55f;s.move_weight=1;s.crouch_weight=1;s.phase_cycles=frame/256.;
      check(fury::sample_character_animation(m.rig,s),"crouch-walk phase="+std::to_string(frame));
    }
    for(unsigned phase=0;phase<16;++phase) {
      fury::CharacterAnimationState state;state.initialized=state.gait_started=true;
      state.sample.seed=m.seed;state.sample.phase_cycles=phase/16.;
      state.sample.stride_length=fury::character_stride_length(height,19.8f);
      state.support_fraction=fury::character_stance_fraction(height,19.8f);
      fury::CharacterAnimationInput input;input.delta_time=1.f/60;input.crouch_weight=1;input.turn_rate=0;
      for(unsigned frame=0;frame<60;++frame) {
        fury::advance_character_animation(state,input,height);
        if(frame%2==0)check(fury::sample_character_animation(m.rig,state),"settle phase="+std::to_string(phase)+" frame="+std::to_string(frame));
      }
    }
    std::cout<<identity<<" height="<<height<<" lod="<<int(lod)<<" completed cumulative_samples="<<samples<<" cumulative_failures="<<failures<<'\n';
  }
  std::cout<<"actor="<<identity<<" total_samples="<<samples<<" failures="<<failures<<'\n';
  return failures?1:0;
}
