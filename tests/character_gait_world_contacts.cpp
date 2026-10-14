// Additive source-mesh / authored-world diagnostic. Existing contact assertions
// and thresholds are reused without modification. This is sampled, not swept CCD.
#define main original_contact_test_main
#include "character_player_body_contact_tests.cpp"
#undef main
#include "character_gait_trace_inputs.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
int main(int argc,char**argv) {
  if(argc!=5)return 2;
  std::ifstream geometry(argv[1]);std::vector<Tri> world;unsigned count{};geometry>>count;
  for(unsigned i=0;i<count;++i){Tri t{};geometry>>t.region>>t.offset;for(auto&p:t.p){geometry>>p.x>>p.y>>p.z;t.box.add(p);}world.push_back(t);}
  if(!geometry)return 3;
  Tree tree(world);
  auto profile=make_character_profile(1.75f,CharacterRole::Player,"PlayerBody",CharacterLod::Near);
  auto&m=profile.model;auto mesh=m.bind_mesh;std::ifstream states(argv[2]),placement(argv[3]);std::ofstream out(argv[4]);
  out<<std::setprecision(10)<<"{\"kind\":\"actual-weighted-mesh-strict-triangle-crossings\",\"scope\":\"head/neck/chest/spine/pelvis against authored visible static world triangles in 0.65-2.05m band\",\"frames\":[";
  bool first=true;std::string line;unsigned frame_samples=0,pose_contacts=0;
  while(std::getline(states,line)) {
    int frame{},visible{};CharacterAnimationState state{};auto&s=state.sample;std::istringstream in(line);
    in>>frame>>visible>>s.phase_cycles>>s.idle_time>>s.speed>>s.stride_length>>s.move_weight>>s.turn_rate>>s.crouch_weight>>s.attention_yaw>>s.attention_pitch>>s.attention_weight>>s.talk_weight>>s.seed>>state.previous_yaw>>state.initialized>>state.gait_started>>state.support_fraction;read_gait_extension(in,state);
    int placement_frame{};Vec3 center{};float yaw{};placement>>placement_frame>>center.x>>center.y>>center.z>>yaw;if(!in||!placement||frame!=placement_frame)return 4;
    if(!visible||(!std::getenv("GAIT_ALL_FRAMES")&&(frame<110||frame>174)))continue;
    auto pose=sample_character_animation(m.rig,state);apply_character_pose(m,pose,mesh);
    const auto model=translate(center)*rotate_y(yaw);
    if(!first)out<<',';first=false;out<<"{\"frame\":"<<frame<<",\"pairs\":[";bool first_pair=true;unsigned contacts=0;
    for(unsigned i=0;i+2<mesh.indices.size();i+=3) {
      bool upper=false;for(unsigned k=0;k<3;++k)upper|=m.influences[mesh.indices[i+k]].first<=bone_index(CharacterBone::Head);
      if(!upper&&!std::getenv("GAIT_FULL_BODY"))continue;
      Tri t{};t.offset=i;for(unsigned k=0;k<3;++k){t.p[k]=transform_point(model,mesh.vertices[mesh.indices[i+k]].position);t.box.add(t.p[k]);}
      tree.visit(t,[&](const Tri&w){if(crossing(t,w)){if(!first_pair)out<<',';first_pair=false;out<<'['<<w.region<<','<<w.offset<<','<<i/3<<']';++contacts;}});
    }
    out<<"],\"count\":"<<contacts<<'}';++frame_samples;if(contacts)++pose_contacts;
  }
  out<<"],\"frame_samples\":"<<frame_samples<<",\"frames_with_contacts\":"<<pose_contacts<<"}\n";return out?0:5;
}
