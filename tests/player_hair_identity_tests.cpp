#include "fury/player_hair.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <array>
#include <stdexcept>
using namespace fury;
struct Hash {
  std::uint64_t value=14695981039346656037ull;
  void bytes(const void* p,std::size_t n) {auto b=static_cast<const unsigned char*>(p);while(n--)value=(value^*b++)*1099511628211ull;}
  template<class T> void add(T v){bytes(&v,sizeof(v));}
  void vec(Vec3 v){add(v.x);add(v.y);add(v.z);}
  void image(const RgbaImage& image){add(image.width);add(image.height);bytes(image.pixels.data(),image.pixels.size());}
};
int main(int argc,char**argv) {try {
  const std::string mode=argc>1?argv[1]:"legacy";
  if(mode!="legacy") {
    const auto preference=mode=="original"?PlayerHairPreference::Original:mode=="balanced-low"?PlayerHairPreference::Balanced:PlayerHairPreference::Auto;
    configure_player_hair(resolve_player_hair(preference,mode=="auto-low"||mode=="balanced-low"?QualityLevel::Low:QualityLevel::Med,
      RenderBackendKind::Software,mode!="disabled",true,player_hair_environment()));
  }
  std::array<const PlayerHairConfig*,16> addresses{};std::array<std::thread,16> threads;
  for(unsigned i=0;i<threads.size();++i) threads[i]=std::thread([&,i]{addresses[i]=&player_hair_configuration();});
  for(auto& thread:threads)thread.join();
  for(auto address:addresses)if(address!=addresses[0])throw std::runtime_error("Concurrent lazy config identity differs");
  Hash geometry,atlas,detail;
  for(auto lod:{CharacterLod::Near,CharacterLod::Far}) {
    const auto profile=make_character_profile(1.75f,CharacterRole::Player,"PlayerBody",lod);
    const auto& model=profile.model;
    for(auto& vertex:model.bind_mesh.vertices){geometry.vec(vertex.position);geometry.vec(vertex.normal);geometry.add(vertex.uv.x);geometry.add(vertex.uv.y);geometry.vec(vertex.color);geometry.add(vertex.opacity);}
    for(auto index:model.bind_mesh.indices)geometry.add(index);
    for(auto w:model.influences){geometry.add(w.first);geometry.add(w.second);geometry.add(w.first_weight);geometry.add(w.third);geometry.add(w.third_weight);}
    for(auto& r:model.deformed_normal_regions){geometry.add(r.vertex_begin);geometry.add(r.vertex_count);for(auto v:r.member_offsets)geometry.add(v);for(auto v:r.members)geometry.add(v);for(auto v:r.triangle_offsets)geometry.add(v);for(auto v:r.triangles)geometry.add(v);for(auto a:r.edge_attachments){geometry.add(a.vertex);geometry.add(a.first);geometry.add(a.second);geometry.add(a.fraction);}}
    if(lod==CharacterLod::Near) {
      const auto& t=*profile.material.textures;
      atlas.image(t.base_color);atlas.image(t.normal);atlas.image(t.metallic_roughness);
      for(auto& i:t.base_color_mips)atlas.image(i);for(auto& i:t.normal_mips)atlas.image(i);for(auto& i:t.metallic_roughness_mips)atlas.image(i);
      if(profile.material.color_detail) {const auto& d=*profile.material.color_detail;detail.image(d.color);for(auto& i:d.color_mips)detail.image(i);detail.add(d.region_count);for(auto&r:d.regions){detail.add(r.source_min.x);detail.add(r.source_min.y);detail.add(r.source_max.x);detail.add(r.source_max.y);detail.add(r.detail_min.x);detail.add(r.detail_min.y);detail.add(r.detail_max.x);detail.add(r.detail_max.y);}}
    }
  }
  std::cout<<"IDENTITY "<<geometry.value<<' '<<atlas.value<<' '<<detail.value<<'\n';return 0;
} catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;} }
