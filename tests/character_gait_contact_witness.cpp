#define main original_contact_main
#include "character_player_body_contact_tests.cpp"
#undef main
#include "character_gait_trace_inputs.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
int main(int argc,char**argv){
 if(argc!=7)return 2;int desired=std::stoi(argv[3]);std::ifstream states(argv[1]),placements(argv[2]);CharacterAnimationState state{};Vec3 center{};float yaw{};std::string line;bool found=false;
 while(std::getline(states,line)){int f,v,other;std::istringstream in(line);auto&s=state.sample;in>>f>>v>>s.phase_cycles>>s.idle_time>>s.speed>>s.stride_length>>s.move_weight>>s.turn_rate>>s.crouch_weight>>s.attention_yaw>>s.attention_pitch>>s.attention_weight>>s.talk_weight>>s.seed>>state.previous_yaw>>state.initialized>>state.gait_started>>state.support_fraction;read_gait_extension(in,state);placements>>other>>center.x>>center.y>>center.z>>yaw;if(f==desired){found=true;break;}}
 if(!found)return 3;unsigned entity_id=std::stoul(argv[5]),count{};std::ifstream geo(argv[4]);geo>>count;std::vector<Tri> world;
 for(unsigned i=0;i<count;++i){Tri t{};geo>>t.region>>t.offset;for(auto&p:t.p){geo>>p.x>>p.y>>p.z;t.box.add(p);}if(unsigned(t.region)==entity_id)world.push_back(t);}
 Tree tree(world);auto profile=make_character_profile(1.75f,CharacterRole::Player,"PlayerBody",CharacterLod::Near);auto&m=profile.model;auto mesh=m.bind_mesh;apply_character_pose(m,sample_character_animation(m.rig,state),mesh);auto model=translate(center)*rotate_y(yaw);
 auto groups=classify(m);std::map<unsigned,int> regions;for(auto&g:groups)regions[g.first]=g.second;std::map<int,unsigned> counts;std::ofstream out(argv[6]);out<<std::setprecision(10)<<"{\"frame\":"<<desired<<",\"entity\":"<<entity_id<<",\"first_witness\":";bool emitted=false;unsigned contacts=0;
 for(unsigned i=0;i+2<mesh.indices.size();i+=3){Tri t{};t.offset=i;for(unsigned k=0;k<3;++k){t.p[k]=transform_point(model,mesh.vertices[mesh.indices[i+k]].position);t.box.add(t.p[k]);}tree.visit(t,[&](const Tri&w){if(!crossing(t,w))return;++contacts;++counts[regions.count(i)?regions[i]:-1];if(emitted)return;
 for(unsigned direction=0;direction<2;++direction){const auto&a=direction?w:t;const auto&b=direction?t:w;for(unsigned k=0;k<3;++k)if(segment(a.p[k],a.p[(k+1)%3],b)){auto p=a.p[k],q=a.p[(k+1)%3],dir=q-p,e=b.p[1]-b.p[0],f=b.p[2]-b.p[0],n=cross(dir,f),x=p-b.p[0],z=cross(x,e),plane=normalize(cross(e,f));float d=dot(e,n);out<<"{\"body_triangle\":"<<i/3<<",\"world_triangle\":"<<w.offset<<",\"body_region\":"<<(regions.count(i)?regions[i]:-1)<<",\"edge_source\":"<<std::quoted(direction?"world":"body")<<",\"edge\":"<<k<<",\"u\":"<<dot(x,n)/d<<",\"v\":"<<dot(dir,z)/d<<",\"t\":"<<dot(f,z)/d<<",\"signed_endpoint_distance_m\":["<<dot(p-b.p[0],plane)<<','<<dot(q-b.p[0],plane)<<"],\"body_vertices_world\":[";for(unsigned j=0;j<3;++j){if(j)out<<',';auto v=t.p[j];out<<'['<<v.x<<','<<v.y<<','<<v.z<<']';}out<<"],\"world_vertices\":[";for(unsigned j=0;j<3;++j){if(j)out<<',';auto v=w.p[j];out<<'['<<v.x<<','<<v.y<<','<<v.z<<']';}out<<"]}";emitted=true;return;}}});}
 if(!emitted)out<<"null";out<<",\"crossing_pairs\":"<<contacts<<",\"body_region_counts\":{";bool first=true;for(auto[a,b]:counts){if(!first)out<<',';first=false;out<<std::quoted(std::to_string(a))<<':'<<b;}out<<"},\"metric_note\":\"Pair counts and segment-plane witnesses are not signed penetration depth\"}\n";return out?0:4;
}
