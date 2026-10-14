#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/render_settings.hpp"
#include "fury/texture.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
using namespace fury;
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point a){return std::chrono::duration<double,std::milli>(Clock::now()-a).count();}
double quantile(std::vector<double> a,double q){std::sort(a.begin(),a.end());return a.at(std::min(a.size()-1,std::size_t(q*(a.size()-1))));}
int main(){
 auto begin=Clock::now();const auto textures=character_surface_textures();const double atlas_ms=ms(begin);std::size_t bytes=textures->base_color.pixels.size()+textures->normal.pixels.size()+textures->metallic_roughness.pixels.size()+textures->emissive.pixels.size();for(const auto* maps:{&textures->base_color_mips,&textures->normal_mips,&textures->metallic_roughness_mips,&textures->emissive_mips})for(const auto& map:*maps)bytes+=map.pixels.size();
 std::cout<<"{\"source_fingerprint\":\""<<build_source_fingerprint()<<"\",\"clock\":\"steady_clock\",\"texture_first_request_ms\":"<<atlas_ms<<",\"shared_texture_resident_pixel_bytes\":"<<bytes<<",\"rows\":[";bool first=true;
 for(bool original:{true,false})for(auto lod:{CharacterLod::Near,CharacterLod::Far}){
  const auto make=[&](){return original?make_character_model(1.8f,CharacterRole::Ghost,character_seed("GhostLoop"),lod):make_character_profile(1.8f,CharacterRole::Ghost,"GhostLoop",lod).model;};std::vector<double> build_times;CharacterModel model;
  for(int i=0;i<6;++i){auto t=Clock::now();model=make();if(i)build_times.push_back(ms(t));}
  auto mesh=model.bind_mesh;const auto identity=mesh.geometry_identity;const auto vc=mesh.vertices.capacity(),ic=mesh.indices.capacity();const auto* vertices=mesh.vertices.data();const auto* indices=mesh.indices.data();
  for(int mode=0;mode<4;++mode){std::vector<double> times;for(int frame=0;frame<280;++frame){CharacterAnimationSample s;s.seed=model.seed;s.phase_cycles=frame/60.;s.idle_time=frame/30.;s.move_weight=mode==1||mode==2?1:0;s.speed=mode==2?5.04f:1.44f;s.attention_weight=s.talk_weight=mode==3?1:0;s.attention_yaw=.7f;s.attention_pitch=-.2f;const auto pose=sample_character_animation(model.rig,s);auto t=Clock::now();apply_character_pose(model,pose,mesh);if(frame>=40)times.push_back(ms(t));}
   if(mesh.geometry_identity!=identity||mesh.vertices.capacity()!=vc||mesh.indices.capacity()!=ic||mesh.vertices.data()!=vertices||mesh.indices.data()!=indices)return 2;
   if(!first)std::cout<<',';first=false;std::cout<<"{\"model\":\""<<(original?"original_generic":"individual_GhostLoop")<<"\",\"lod\":\""<<(lod==CharacterLod::Near?"near":"far")<<"\",\"mode\":\""<<(mode==0?"idle":mode==1?"walk":mode==2?"run":"attention_talk")<<"\",\"samples\":240,\"pose_median_ms\":"<<quantile(times,.5)<<",\"pose_p95_ms\":"<<quantile(times,.95)<<",\"warm_build_median_ms\":"<<quantile(build_times,.5)<<",\"vertices\":"<<mesh.vertices.size()<<",\"triangles\":"<<mesh.indices.size()/3<<",\"bind_mesh_live_bytes\":"<<mesh.vertices.size()*sizeof(Vertex)+mesh.indices.size()*sizeof(std::uint32_t)<<",\"influence_live_bytes\":"<<model.influences.size()*sizeof(CharacterInfluence)<<",\"posed_mesh_live_bytes\":"<<mesh.vertices.size()*sizeof(Vertex)+mesh.indices.size()*sizeof(std::uint32_t)<<",\"pose_cache_identity_and_storage_stable\":true}";
  }
 }
 std::cout<<"],\"scope\":\"One actor, warm preallocated apply_character_pose only; excludes renderer/world and pose solver. Texture bytes are shared once across all actors. No live FPS claim.\"}\n";
}
