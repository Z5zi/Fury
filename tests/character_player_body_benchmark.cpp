#include <fury/character_profile.hpp>
#include <fury/character_animation.hpp>
#include <fury/character_surface.hpp>
#include <fury/texture.hpp>
#include <fury/render_settings.hpp>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
using namespace fury;
using Clock=std::chrono::steady_clock;
int main(){
  std::cout<<std::setprecision(9)<<"{\"source_fingerprint\":\""<<build_source_fingerprint()<<"\",\"cpu_affinity\":[8],\"samples\":[";
  bool first=true;
  for(bool individual:{
    false,true
  })for(auto lod:{
    CharacterLod::Near,CharacterLod::Far
  }){
    auto start=Clock::now();
    CharacterProfile p;
    if(individual)p=make_character_profile(1.75f,CharacterRole::Player,"PlayerBody",lod);
    else p.model=make_character_model(1.75f,CharacterRole::Player,character_seed("PlayerBody"),lod);
    const auto construction=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    auto&m=p.model;
    Mesh mesh=m.bind_mesh;
    std::vector<double> times;
    for(int k=0;k<272;++k){
      CharacterAnimationState s{};
      s.sample.seed=m.seed;
      s.sample.phase_cycles=k/32.;
      s.sample.idle_time=k/30.;
      s.sample.speed=18.1216506958f;
      s.sample.stride_length=2.11525511742f;
      s.sample.move_weight=.997856676579f;
      s.gait_started=true;
      s.support_fraction=.469999998808f;
      const auto pose=sample_character_animation(m.rig,s);
      start=Clock::now();
      apply_character_pose(m,pose,mesh);
      double t=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
      if(k>=16)times.push_back(t);
    }
    std::sort(times.begin(),times.end());
    double mean=0;
    for(auto t:times)mean+=t;
    mean/=times.size();
    if(!first)std::cout<<',';
    first=false;
    std::cout<<"{\"individual\":"<<(individual?"true":"false")<<",\"lod\":\""<<(lod==CharacterLod::Near?"near":"far")<<"\",\"vertices\":"<<m.bind_mesh.vertices.size()<<",\"triangles\":"<<m.bind_mesh.indices.size()/3<<",\"construction_ms_cold_or_shared_cache\":"<<construction<<",\"pose_samples\":"<<times.size()<<",\"pose_mean_ms\":"<<mean<<",\"pose_median_ms\":"<<times[times.size()/2]<<",\"pose_p95_ms\":"<<times[std::size_t(times.size()*.95)]<<'}';
  }
  const auto tex=character_surface_textures();
  std::size_t bytes=0;
  for(const auto* im:{
    &tex->base_color,&tex->normal,&tex->metallic_roughness,&tex->emissive
  })bytes+=im->pixels.size();
  for(const auto* ms:{
    &tex->base_color_mips,&tex->normal_mips,&tex->metallic_roughness_mips,&tex->emissive_mips
  })for(auto&i:*ms)bytes+=i.pixels.size();
  std::cout<<"],\"shared_texture_bytes_with_mips\":"<<bytes<<",\"new_actor_texture_allocation_bytes\":0,\"simulation_sample\":\"exact production frame99 smoothed sprint state, phase varied\"}\n";
}
