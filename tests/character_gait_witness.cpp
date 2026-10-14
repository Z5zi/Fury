#define main original_contact_main
#include "character_rook_contact_tests.cpp"
#undef main
#include <iomanip>
int main() {
  unsigned failures=0;
  for(float h:{1.35f,1.75f,2.1f}) {
    auto p=fury::make_character_profile(h,fury::CharacterRole::CrewTech,"CrewRook",fury::CharacterLod::Near);
    auto&m=p.model;const auto groups=classify(m);auto mesh=m.bind_mesh;
    for(int frame=120;frame<=128;++frame) {
      fury::CharacterAnimationSample s;s.seed=m.seed;s.speed=h*.55f;s.move_weight=1;s.crouch_weight=1;s.phase_cycles=frame/256.;
      fury::apply_character_pose(m,fury::sample_character_animation(m.rig,s),mesh);
      const auto ts=triangles(mesh,h,groups,&m.bind_mesh);const auto first=audit(ts);
      float severity=0;unsigned pairs=0;Tree tree(ts);
      for(const auto&a:ts)if(a.region==3||a.region==4)tree.visit(a,[&](const Tri&b){if(b.region==1&&crossing(a,b)) {
        ++pairs;
        for(const auto*plane:{&a,&b}) {
          auto n=fury::normalize(fury::cross(plane->p[1]-plane->p[0],plane->p[2]-plane->p[0]));
          const auto&other=plane==&a?b:a;float low=0,high=0;
          for(auto q:other.p){float d=fury::dot(q-plane->p[0],n);low=std::min(low,d);high=std::max(high,d);}
          severity=std::max(severity,std::min(-low,high)*h);
        }
      }});
      if(pairs)++failures;
      std::cout<<std::setprecision(10)<<"height="<<h<<" phase="<<s.phase_cycles<<" forearm_vest_pairs="<<pairs<<" maximum_segment_plane_crossing_m="<<severity<<" first="<<std::quoted(first)<<'\n';
    }
  }
  return failures?1:0;
}
