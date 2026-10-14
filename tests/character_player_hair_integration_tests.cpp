#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/material_sampling.hpp"
#include "fury/generated/player_body_short_tail.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace fury;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool inside(Vec2 uv,CharacterSurface surface) {
 const auto a=character_surface_uv(surface,0,0),b=character_surface_uv(surface,1,1);
 return uv.x>=a.x-1e-7f&&uv.x<=b.x+1e-7f&&uv.y>=a.y-1e-7f&&uv.y<=b.y+1e-7f;
}
void atlas(const MaterialTextures& t) {
 // Captured from the actual 13-region fury-quality baseline, including gutters
 // and all six retained mips. This is separate from prototype art acceptance.
 constexpr std::array<std::uint64_t,13> expected{{
  0xccec0c990f13341eull,0x06c046d2b20e68e3ull,0xb8f7e753e538e08eull,
  0xb6a59f21a5e6e185ull,0x28972394f7d7d236ull,0x00e7c4d8d2e17a93ull,
  0xd2399652431f650eull,0x323f907733a7678dull,0x35071f8757a1ea29ull,
  0x03d6561989ecba4cull,0xb62d862ce90f4b23ull,0x061f2806a2d6bc1cull,
  0xcd8a38c7ed2688cfull}};
 const RgbaImage* base[]{&t.base_color,&t.normal,&t.metallic_roughness};
 const std::vector<RgbaImage>* mips[]{&t.base_color_mips,&t.normal_mips,&t.metallic_roughness_mips};
 for(unsigned i=0;i<13;++i) {
  const auto&r=character_surface_region(CharacterSurface(i));std::uint64_t hash=14695981039346656037ull;
  for(int m=0;m<3;++m)for(unsigned l=0;l<=mips[m]->size();++l) {
   const auto&im=l?(*mips[m])[l-1]:*base[m];
   for(int y=r.y>>l;y<(r.y+r.size)>>l;++y)for(int x=r.x>>l;x<(r.x+r.size)>>l;++x)for(int c=0;c<4;++c)
    hash=(hash^im.pixels[(std::size_t(y)*im.width+x)*4+c])*1099511628211ull;
  }
  require(hash==expected[i],"A main atlas region, gutter or retained mip changed");
 }
 require(unsigned(CharacterSurface::HairSwept)==13&&unsigned(CharacterSurface::HairBundle)==14&&
         unsigned(CharacterSurface::HairSourceFold)==15&&unsigned(CharacterSurface::Count)==16,
         "Append only three source-hair cells after main's 13 stable IDs");
 std::vector<int> occupancy(1536*1536);
 for(unsigned i=0;i<16;++i){const auto&r=character_surface_region(CharacterSurface(i));
  if(i>=13)require(r.x==int(i-12)*256&&r.y==1280&&r.size==256&&r.gutter==32,"New cells occupy only named previously unused rectangles");
  for(int y=r.y;y<r.y+r.size;++y)for(int x=r.x;x<r.x+r.size;++x)require(++occupancy[y*1536+x]==1,"Atlas regions overlap");
 }
 std::size_t bytes=0;for(unsigned m=0;m<3;++m){bytes+=base[m]->pixels.size();for(auto&im:*mips[m])bytes+=im.pixels.size();}
 require(bytes==37746432u,"Hair must not grow the existing shared atlas allocation");
}
void mapping() {
 std::shared_ptr<const MaterialColorDetail> prior;
 for(auto lod:{CharacterLod::Near,CharacterLod::Far}){
  auto p=make_character_profile(1.75f,CharacterRole::Player,"PlayerBody",lod);const auto&m=p.model;
#if FURY_ENABLE_EXPERIMENTAL_EQUIPPED_CROUCH
  require(p.animation_profile==CharacterAnimationProfile::EquippedCrouchExperimental,"Hair lost the explicit experimental Player profile assignment");
#else
  require(p.animation_profile==CharacterAnimationProfile::Generic,"Default-off motion profile changed");
#endif
  require(m.deformed_normal_regions.size()==(player_short_tail_enabled()?1u:0u),"Scoped normal metadata must belong only to the enabled tail");
  unsigned scalp_tail=0;
  for(std::size_t t=0;t<m.bind_mesh.indices.size();t+=3) {
   const auto&a=m.bind_mesh.vertices[m.bind_mesh.indices[t]],&b=m.bind_mesh.vertices[m.bind_mesh.indices[t+1]],&c=m.bind_mesh.vertices[m.bind_mesh.indices[t+2]];
   CharacterSurface region=inside(a.uv,CharacterSurface::HairSwept)?CharacterSurface::HairSwept:CharacterSurface::HairBundle;
   if(!inside(a.uv,region))continue;
   ++scalp_tail;require(inside(b.uv,region)&&inside(c.uv,region),"Source triangles cross atlas cells");
   const Vec3 n=cross(b.position-a.position,c.position-a.position);
   require(length(n)>1e-10f&&dot(n,a.normal+b.normal+c.normal)>0,"Source hair winding disagrees with its bind normals");
  }
  const unsigned tail=player_short_tail_enabled()?(lod==CharacterLod::Near?player_body_short_tail::indices.size():player_body_short_tail::far_indices.size())/3:0;
  require(scalp_tail==(player_source_hair_enabled()?(lod==CharacterLod::Near?1688u:537u)+tail:0u),"Exact source scalp/tail face count differs");
  auto detail=p.material.color_detail;
  require(bool(detail)==bool(make_player_hair_color_detail()),"Factory detail plumbing differs");
  if(lod==CharacterLod::Near)prior=detail;else require(prior==detail,"Both LODs must share one immutable detail image");
  if(!detail)continue;
  require(detail->region_count==2,"Only the source scalp/tail UV windows may use detail");
  require((detail->color.width==512&&detail->pixel_bytes()==1398100u)||
          (detail->color.width==1024&&detail->pixel_bytes()==5592404u),"Unexpected detail texture memory");
  for(const auto&v:m.bind_mesh.vertices) {
   if(inside(v.uv,CharacterSurface::HairSwept)||inside(v.uv,CharacterSurface::HairBundle))continue;
   auto base=sample_material_texture(p.material.textures->base_color,p.material.textures->base_color_mips,v.uv,true);
   auto result=apply_material_color_detail(detail.get(),base,v.uv);
   require(std::memcmp(&base,&result,sizeof(base))==0,"Hair detail changed a non-hair material sample");
  }
 }
 auto generic=make_character_profile(1.75f,CharacterRole::Player,"unregistered-source-hair-control",CharacterLod::Near);
 require(!generic.material.color_detail&&generic.model.deformed_normal_regions.empty()&&generic.animation_profile==CharacterAnimationProfile::Generic,
         "Hair options leaked into generic characters");
}
}
int main(int argc,char**argv) {
 const bool invalid=argc==2&&std::string(argv[1])=="--expect-invalid";
 try {mapping();atlas(*fury::character_surface_textures());require(!invalid,"Invalid option was accepted");
  std::cout<<"Main atlas, hair scope, detail scope and profile metadata passed. Motion acceptance was not tested.\n";return 0;
 } catch(const std::invalid_argument&e){if(invalid)return 0;std::cerr<<e.what()<<'\n';return 1;}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
