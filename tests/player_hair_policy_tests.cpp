#include "fury/player_hair.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace fury;
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
int main() { try {
  unsigned combinations=0, env_combinations=0;
  for (auto backend : {RenderBackendKind::None,RenderBackendKind::Software,RenderBackendKind::CpuRayTracing,
                       RenderBackendKind::OpenGL,RenderBackendKind::Direct3D12})
  for (auto preference : {PlayerHairPreference::Auto,PlayerHairPreference::Original,PlayerHairPreference::Balanced})
  for (auto quality : {QualityLevel::Low,QualityLevel::Med,QualityLevel::High})
  for (bool detail : {false,true}) for (bool individual : {false,true}) {
    const auto config=resolve_player_hair(preference,quality,backend,detail,individual);
    const bool expected=detail && individual && player_hair_backend_supported(backend) &&
      (preference==PlayerHairPreference::Balanced || (preference==PlayerHairPreference::Auto && quality!=QualityLevel::Low));
    require(config.source==expected && config.short_tail==expected && config.detail_resolution==(expected?512:0),"Preset matrix");
    require(!config.environment_override,"Absent bundle is not an override");
    ++combinations;
  }
  for (const char* source : std::array<const char*,3>{nullptr,"0","1"})
  for (const char* tail : std::array<const char*,3>{nullptr,"0","1"})
  for (const char* detail : std::array<const char*,4>{nullptr,"0","512","1024"}) {
    const bool s=source && std::string(source)=="1", t=tail && std::string(tail)=="1";
    const int d=detail?std::stoi(detail):0;
    const bool any=source||tail||detail, valid=s||(!t&&!d);
    for (auto preference : {PlayerHairPreference::Auto,PlayerHairPreference::Original,PlayerHairPreference::Balanced}) {
      bool threw=false;
      try {
        const auto config=resolve_player_hair(preference,QualityLevel::Med,RenderBackendKind::Software,true,true,{source,tail,detail});
        require(valid,"Invalid dependency was accepted");
        if (any) require(config.source==s && config.short_tail==t && config.detail_resolution==d && config.environment_override,"Whole env bundle is authoritative");
      } catch (const std::invalid_argument&) { threw=true; }
      require(threw==!valid,"Legacy environment Cartesian rules"); ++env_combinations;
    }
  }
  for (const char* malformed : {"","true"," 1","1 ","01","-1","2","512x","1.0"})
  for (unsigned field=0;field<3;++field) for (bool enabled : {false,true}) {
    PlayerHairEnvironment env{"1","0","0"};
    if (field==0) env.source=malformed; if (field==1) env.tail=malformed; if (field==2) env.detail=malformed;
    bool threw=false;try { (void)resolve_player_hair(PlayerHairPreference::Auto,QualityLevel::Med,RenderBackendKind::Software,enabled,enabled,env); }
    catch(const std::invalid_argument&) { threw=true; }
    require(threw,"Malformed env must fail even behind disable gate");
  }
  const auto dx=resolve_player_hair(PlayerHairPreference::Auto,QualityLevel::Med,RenderBackendKind::Direct3D12,true,true);
  const auto fallback=resolve_player_hair(PlayerHairPreference::Auto,QualityLevel::Med,RenderBackendKind::Software,true,true);
  require(!dx.source && fallback.source,"Actual backend fallback is authoritative");
  require(cycle_player_hair_preference(PlayerHairPreference::Original,1,RenderBackendKind::Direct3D12)==PlayerHairPreference::Auto,"No verified DX12 Balanced choice");
  require(cycle_player_hair_preference(PlayerHairPreference::Auto,-1,RenderBackendKind::OpenGL)==PlayerHairPreference::Balanced,"Supported reverse navigation");
  configure_player_hair(fallback);
  require(player_hair_configuration().source && player_hair_configuration().short_tail,"Explicit freeze reaches model config");
  const auto pending=preview_player_hair(fallback,PlayerHairPreference::Original,QualityLevel::High,RenderBackendKind::Software);
  require(!same_player_hair_groom(pending,fallback),"Pending restart detected");
  require(same_player_hair_groom(preview_player_hair(fallback,PlayerHairPreference::Auto,QualityLevel::Med,RenderBackendKind::Software),fallback),"Revert clears pending");
  require(!same_player_hair_groom(preview_player_hair(fallback,PlayerHairPreference::Auto,QualityLevel::Low,RenderBackendKind::Software),fallback),"Auto quality Low needs restart");
  const auto forced_high=QualityLevel::High;
  require(same_player_hair_groom(preview_player_hair(fallback,PlayerHairPreference::Auto,QualityLevel::Low,RenderBackendKind::Software,&forced_high),fallback),"FURY_QUALITY wins at restart despite live Low");
  const auto env=resolve_player_hair(PlayerHairPreference::Auto,QualityLevel::Med,RenderBackendKind::Software,true,true,{"0",nullptr,nullptr});
  require(same_player_hair_groom(preview_player_hair(env,PlayerHairPreference::Balanced,QualityLevel::High,RenderBackendKind::Software),env),"Env override never merges menu");
  bool frozen=false;try {configure_player_hair(pending);}catch(const std::logic_error&){frozen=true;}
  require(frozen && player_hair_configuration().source,"No reconfiguration after freeze");
  std::cout<<combinations<<" preset/backend/disable cases; "<<env_combinations<<" env Cartesian cases; malformed, fallback, pending/revert, freeze passed\n";
  return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
