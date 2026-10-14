// Regression for an observed bank-entry route, not a global collision certificate.
#define main original_player_self_contact_main
#include "character_player_body_contact_tests.cpp"
#undef main
#include "character_gait_trace_inputs.hpp"
#include "fixtures/character_gait_queue.hpp"
#include <sstream>
#include <iomanip>

void crossing_witness(const Tri&a,const Tri&b) {
  for(unsigned direction=0;direction<2;++direction) {
    const auto&source=direction?b:a;const auto&target=direction?a:b;
    for(unsigned k=0;k<3;++k)if(segment(source.p[k],source.p[(k+1)%3],target)) {
      Vec3 p=source.p[k],q=source.p[(k+1)%3],dir=q-p,e=target.p[1]-target.p[0],f=target.p[2]-target.p[0],n=cross(dir,f),x=p-target.p[0],z=cross(x,e),plane=normalize(cross(e,f));
      float d=dot(e,n),u=dot(x,n)/d,v=dot(dir,z)/d,t=dot(f,z)/d;
      std::cout<<std::setprecision(10)<<" witness body_triangle="<<a.offset/3<<" world_entity="<<b.region<<" world_triangle="<<b.offset<<" edge="<<k<<" direction="<<direction<<" barycentric_u="<<u<<" barycentric_v="<<v<<" segment_t="<<t<<" endpoint_signed_distances_m="<<dot(p-target.p[0],plane)<<','<<dot(q-target.p[0],plane)<<'\n';return;
    }
  }
}
int main() {
  try {
    std::vector<Tri> world;
    for(const auto&f:gait_queue_fixture::faces){Tri t{};t.region=f.entity;t.offset=f.triangle;t.p=f.p;for(auto p:t.p)t.box.add(p);world.push_back(t);}
    Tree tree(world);unsigned tested=0,negative_hits=0,total_crossings=0;
    for(auto lod:{CharacterLod::Near,CharacterLod::Far}) {
      auto profile=make_character_profile(1.75f,CharacterRole::Player,"PlayerBody",lod);auto&m=profile.model;auto mesh=m.bind_mesh;
      for(unsigned sample=0;sample<4;++sample) {
        int frame{},visible{};CharacterAnimationState state{};auto&s=state.sample;std::istringstream in(gait_queue_fixture::states[sample]);
        in>>frame>>visible>>s.phase_cycles>>s.idle_time>>s.speed>>s.stride_length>>s.move_weight>>s.turn_rate>>s.crouch_weight>>s.attention_yaw>>s.attention_pitch>>s.attention_weight>>s.talk_weight>>s.seed>>state.previous_yaw>>state.initialized>>state.gait_started>>state.support_fraction;read_gait_extension(in,state);
        int placement_frame{};Vec3 center{};float yaw{};std::istringstream placement(gait_queue_fixture::placements[sample]);placement>>placement_frame>>center.x>>center.y>>center.z>>yaw;
        require(bool(in)&&bool(placement)&&frame==placement_frame,"Malformed recorded queue witness");
        apply_character_pose(m,sample_character_animation(m.rig,state),mesh);
        for(unsigned control=0;control<2;++control) {
          auto position=center;if(control)position.z+=.15f;
          auto model=translate(position)*rotate_y(yaw);unsigned contacts=0;
          for(unsigned i=0;i+2<mesh.indices.size();i+=3) {
            bool upper=false;for(unsigned k=0;k<3;++k)upper|=m.influences[mesh.indices[i+k]].first<=bone_index(CharacterBone::Head);if(!upper)continue;
            Tri t{};t.offset=i;for(unsigned k=0;k<3;++k){t.p[k]=transform_point(model,mesh.vertices[mesh.indices[i+k]].position);t.box.add(t.p[k]);}
            tree.visit(t,[&](const Tri&w){if(crossing(t,w)){if(!control&&contacts==0){std::cout<<"FAIL queue frame="<<frame<<" lod="<<unsigned(lod);crossing_witness(t,w);}++contacts;}});
          }
          if(control)negative_hits+=contacts;
          else {std::cout<<"queue frame="<<frame<<" lod="<<unsigned(lod)<<" crossing_pairs="<<contacts<<'\n';total_crossings+=contacts;++tested;}
        }
      }
    }
    require(negative_hits>0,"Queue collision negative control did not exercise the detector");
    require(total_crossings==0,"Moving crouch introduced upper-body crossings on the recorded clear queue passage");
    std::cout<<"Recorded visible non-solid queue witness passed: "<<tested<<" weighted Near/Far poses; negative_control_pairs="<<negative_hits<<"; pair counts are not penetration depth\n";return 0;
  }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
