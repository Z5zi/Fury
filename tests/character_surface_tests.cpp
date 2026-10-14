#include "fury/character_surface.hpp"
#include "fury/material_sampling.hpp"
#include "fury/generated/mira_anatomy.hpp"
#include "fury/generated/jon_anatomy.hpp"
#include "fury/generated/tessa_anatomy.hpp"
#include "fury/generated/owen_anatomy.hpp"
#include "fury/generated/lia_anatomy.hpp"
#include "fury/generated/hale_anatomy.hpp"

#if __has_include(<SDL.h>) && !defined(FURY_CHARACTER_SURFACE_NO_GL_TESTS)
#define FURY_CHARACTER_SURFACE_GL_TESTS 1
#include "fury/gl_loader.hpp"
#include "fury/renderer.hpp"
#include <SDL.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
constexpr std::array<CharacterSurface,13> surfaces{{
  CharacterSurface::Face,CharacterSurface::Jacket,CharacterSurface::Hair,
  CharacterSurface::Skin,CharacterSurface::Shirt,CharacterSurface::Pants,
  CharacterSurface::Leather,CharacterSurface::Eyes,CharacterSurface::Rubber,
  CharacterSurface::Metal,CharacterSurface::HairStrands,CharacterSurface::HairCrop,CharacterSurface::HairShortStrands
}};
constexpr std::array<const char*,13> names{{
  "face","jacket","hair","skin","shirt","pants","leather","eyes","rubber","metal","hair strands","hair crop","short hair strands"
}};
void require(bool condition,const char* message) {
  if(!condition) throw std::runtime_error(message);
}
bool close(float a,float b,float epsilon=1e-6f) { return std::abs(a-b)<=epsilon; }
bool close(Vec4 a,Vec4 b,float epsilon=1e-6f) {
  return close(a.x,b.x,epsilon) && close(a.y,b.y,epsilon) &&
         close(a.z,b.z,epsilon) && close(a.w,b.w,epsilon);
}
Vec4 sample(const RgbaImage& image,const std::vector<RgbaImage>& mips,
            CharacterSurface surface,float u,float v,bool srgb,float lod=0) {
  return sample_material_texture_lod(image,mips,character_surface_uv(surface,u,v),srgb,lod);
}
void layout_and_api() {
  require(character_surface_atlas_size==1536,"Shared character atlas is exactly 1536px square");
  require(unsigned(CharacterSurface::HairStrands)==10 && unsigned(CharacterSurface::HairCrop)==11 &&
          unsigned(CharacterSurface::HairShortStrands)==12 && unsigned(CharacterSurface::Count)==16,
          "HairShortStrands is appended at index twelve without changing any existing material index");
  constexpr int size=character_surface_atlas_size;
  constexpr std::array<CharacterSurfaceRegion,13> expected{{
    {0,0,1024,64},{1024,0,512,32},{1024,512,256,32},{1280,512,256,32},
    {1024,768,256,32},{1280,768,256,32},{0,1024,256,32},{256,1024,256,32},
    {512,1024,256,32},{768,1024,256,32},{1024,1024,256,32},{1280,1024,256,32},{0,1280,256,32}
  }};
  std::vector<unsigned> coverage(std::size_t(size)*size,0);
  for(unsigned i=0;i<surfaces.size();++i) {
    const auto& r=character_surface_region(surfaces[i]);
    require(r.x==expected[i].x && r.y==expected[i].y && r.size==expected[i].size && r.gutter==expected[i].gutter,
            "Stable layout appends the 256px HairShortStrands tile after all twelve unchanged material regions");
    require(r.x>=0 && r.y>=0 && r.x+r.size<=size && r.y+r.size<=size,"Every stable region lies inside the 1536px atlas");
    for(int y=r.y;y<r.y+r.size;++y) for(int x=r.x;x<r.x+r.size;++x)
      ++coverage[std::size_t(y)*size+x];
    const auto a=character_surface_uv(surfaces[i],0,0),b=character_surface_uv(surfaces[i],1,1);
    require(a.x>float(r.x+r.gutter)/size && a.y>float(r.y+r.gutter)/size &&
            b.x<float(r.x+r.size-r.gutter)/size && b.y<float(r.y+r.size-r.gutter)/size,
            "Local UV endpoints remain strictly inside the content's edge texels");
    const auto outside=character_surface_uv(surfaces[i],-40,900);
    require(close(outside.x,a.x) && close(outside.y,b.y),"Finite out-of-range UVs clamp inside the selected material");
    // Face is piecewise affine over its five source charts. A global cap average
    // can cross chart classifications; actual source triangles are checked below.
    if(surfaces[i]==CharacterSurface::Face) continue;
    const auto middle=character_surface_uv(surfaces[i],.5f,.5f);
    require(close(middle.x,(a.x+b.x)*.5f) && close(middle.y,(a.y+b.y)*.5f),
            "UV helper is affine within a surface so interpolation and tangent frames stay valid");
    Vec2 cap{};
    for(int vertex=0;vertex<=24;++vertex) {
      const auto uv=character_surface_uv(surfaces[i],float(vertex)/24.f,.73f);
      cap.x+=uv.x/25.f; cap.y+=uv.y/25.f;
    }
    require(cap.x>=a.x && cap.x<=b.x && cap.y>=a.y && cap.y<=b.y &&
            close(cap.x,middle.x) && close(cap.y,character_surface_uv(surfaces[i],.5f,.73f).y),
            "A loft cap's averaged ring UV stays in the same region and matches the local-coordinate average");
  }
  require(std::all_of(coverage.begin(),coverage.end(),[](unsigned n) { return n<=1; }) &&
          std::count(coverage.begin(),coverage.end(),1u)==2031616,
          "Material tiles never overlap and occupy exactly 2031616 texels; unused atlas space is intentional");
  bool bad_enum=false,bad_uv=false;
  try { character_surface_region(CharacterSurface::Count); } catch(const std::out_of_range&) { bad_enum=true; }
  try { character_surface_uv(CharacterSurface::Face,std::numeric_limits<float>::infinity(),0); }
  catch(const std::invalid_argument&) { bad_uv=true; }
  require(bad_enum && bad_uv,"Invalid regions and nonfinite UVs fail explicitly");
}
struct FaceChart {
  const char* name;
  Vec2 low,high,origin;
  float scale;
  int x,y,width,height;
};
// These independent bounds come from connected components of both generated
// source-UV streams. Jon contributes the wider neck; all other charts coincide.
constexpr std::array<FaceChart,5> face_charts{{
  {"main",{.641249001f,.148696005f},{.993515015f,.817633986f},{8,8},1.44f,0,0,576,1024},
  {"mouth",{.757633984f,.0118629998f},{.923740029f,.136439994f},{288,8},1.f,576,0,448,256},
  {"neck",{.215131998f,.840623021f},{.545929015f,.973855972f},{288,112},1.f,576,256,448,256},
  {"left socket",{.0118089998f,.547390997f},{.0768219978f,.632857978f},{288,240},1.f,576,512,448,256},
  {"right socket",{.0861430019f,.547390997f},{.151155993f,.632857978f},{288,368},1.f,576,768,448,256}
}};
struct FaceTriangle {
  std::array<Vec2,3> uv;
  std::array<Vec3,3> position;
  unsigned chart;
};
using FaceTriangles=std::vector<FaceTriangle>;
template<class Positions,class Uvs,class Indices>
void append_face_triangles(FaceTriangles& result,const Positions& positions,const Uvs& uvs,
                           const Indices& indices,const Indices& corners) {
  require(indices.size()==corners.size() && indices.size()%3==0,"Source face position and UV triangle streams agree");
  std::vector<std::size_t> parent(uvs.size());
  for(std::size_t i=0;i<parent.size();++i) parent[i]=i;
  auto root=[&](std::size_t i) {
    while(parent[i]!=i) { parent[i]=parent[parent[i]]; i=parent[i]; }
    return i;
  };
  for(std::size_t i=0;i<corners.size();i+=3) {
    parent[root(corners[i+1])]=root(corners[i]);
    parent[root(corners[i+2])]=root(corners[i]);
  }
  std::vector<Vec2> lows(uvs.size(),{2,2}),highs(uvs.size(),{-1,-1});
  for(auto corner:corners) {
    const auto id=root(corner); const auto uv=uvs[corner];
    lows[id].x=std::min(lows[id].x,uv.x); lows[id].y=std::min(lows[id].y,uv.y);
    highs[id].x=std::max(highs[id].x,uv.x); highs[id].y=std::max(highs[id].y,uv.y);
  }
  std::vector<unsigned> chart_for_root(uvs.size(),unsigned(face_charts.size()));
  std::array<unsigned,5> components{};
  for(std::size_t id=0;id<uvs.size();++id) if(highs[id].x>=0) {
    unsigned matches=0;
    for(unsigned chart=0;chart<face_charts.size();++chart) {
      const auto& bounds=face_charts[chart];
      if(lows[id].x>=bounds.low.x-1e-7f && lows[id].y>=bounds.low.y-1e-7f &&
         highs[id].x<=bounds.high.x+1e-7f && highs[id].y<=bounds.high.y+1e-7f) {
        chart_for_root[id]=chart; ++components[chart]; ++matches;
      }
    }
    require(matches==1,"Each connected source UV component fits exactly one independent chart bound");
  }
  require(std::all_of(components.begin(),components.end(),[](unsigned n) { return n==1; }),
          "Every generated head contains precisely five disjoint connected face charts");
  for(std::size_t i=0;i<corners.size();i+=3) {
    FaceTriangle triangle{}; triangle.chart=chart_for_root[root(corners[i])];
    for(unsigned j=0;j<3;++j) {
      require(root(corners[i+j])==root(corners[i]),"No source triangle spans disconnected face charts");
      triangle.uv[j]=uvs[corners[i+j]];
      triangle.position[j]=positions[indices[i+j]];
    }
    result.push_back(triangle);
  }
}
const FaceTriangles& face_triangles() {
  static const FaceTriangles triangles=[] {
    FaceTriangles result;
    append_face_triangles(result,mira_anatomy::positions,mira_anatomy::uv_coordinates,
                          mira_anatomy::indices,mira_anatomy::uv_indices);
    append_face_triangles(result,jon_anatomy::positions,jon_anatomy::uv_coordinates,
                          jon_anatomy::indices,jon_anatomy::uv_indices);
    append_face_triangles(result,tessa_anatomy::positions,tessa_anatomy::uv_coordinates,
                          tessa_anatomy::indices,tessa_anatomy::uv_indices);
    append_face_triangles(result,owen_anatomy::positions,owen_anatomy::uv_coordinates,
                          owen_anatomy::indices,owen_anatomy::uv_indices);
    append_face_triangles(result,lia_anatomy::positions,lia_anatomy::uv_coordinates,
                          lia_anatomy::indices,lia_anatomy::uv_indices);
    append_face_triangles(result,hale_anatomy::positions,hale_anatomy::uv_coordinates,
                          hale_anatomy::indices,hale_anatomy::uv_indices);
    return result;
  }();
  return triangles;
}
constexpr std::array<std::array<float,3>,10> face_weights{{
  {{1,0,0}},{{0,1,0}},{{0,0,1}},{{.5f,.5f,0}},{{0,.5f,.5f}},{{.5f,0,.5f}},
  {{1.f/3,1.f/3,1.f/3}},{{.1f,.2f,.7f}},{{.7f,.1f,.2f}},{{.2f,.7f,.1f}}
}};
Vec2 interpolate_uv(const std::array<Vec2,3>& uv,const std::array<float,3>& weights) {
  Vec2 result{};
  for(unsigned j=0;j<3;++j) { result.x+=uv[j].x*weights[j]; result.y+=uv[j].y*weights[j]; }
  return result;
}
double uv_area(Vec2 a,Vec2 b,Vec2 c) {
  return (double(b.x)-a.x)*(double(c.y)-a.y)-(double(b.y)-a.y)*(double(c.x)-a.x);
}
MaterialTangentFrame face_frame(const FaceTriangle& triangle,const std::array<Vec2,3>& uv) {
  const auto e1=triangle.position[1]-triangle.position[0],e2=triangle.position[2]-triangle.position[0];
  const float inverse=1.f/float(uv_area(uv[0],uv[1],uv[2]));
  return {normalize((e1*(uv[2].y-uv[0].y)-e2*(uv[1].y-uv[0].y))*inverse),
          normalize((e2*(uv[1].x-uv[0].x)-e1*(uv[2].x-uv[0].x))*inverse)};
}
void face_chart_mapping() {
  std::array<Vec2,5> lows,highs;
  lows.fill({2,2}); highs.fill({-1,-1});
  std::array<unsigned,5> counts{};
  for(const auto& triangle:face_triangles()) {
    const auto& chart=face_charts[triangle.chart]; ++counts[triangle.chart];
    std::array<Vec2,3> packed;
    for(unsigned j=0;j<3;++j) {
      const auto uv=triangle.uv[j];
      auto& low=lows[triangle.chart]; auto& high=highs[triangle.chart];
      low.x=std::min(low.x,uv.x); low.y=std::min(low.y,uv.y);
      high.x=std::max(high.x,uv.x); high.y=std::max(high.y,uv.y);
      packed[j]=character_surface_uv(CharacterSurface::Face,uv.x,uv.y);
      const Vec2 pixels{packed[j].x*1536,packed[j].y*1536};
      const Vec2 expected{64.5f+895.f*((uv.x-chart.low.x)*chart.scale+chart.origin.x/447.f),
                          64.5f+895.f*((uv.y-chart.low.y)*chart.scale+chart.origin.y/447.f)};
      require(close(pixels.x,expected.x,.0002f) && close(pixels.y,expected.y,.0002f),
              "Every Mira and Jon source corner receives its chart's expected uniform affine transform");
      require(pixels.x>=chart.x+32 && pixels.x<=chart.x+chart.width-32 &&
              pixels.y>=chart.y+32 && pixels.y<=chart.y+chart.height-32,
              "Every used UV stays at least 32 source pixels inside its own chart cell");
    }
    require(uv_area(triangle.uv[0],triangle.uv[1],triangle.uv[2])*
            uv_area(packed[0],packed[1],packed[2])>0,
            "Packing preserves the signed area of every source face triangle");
    const auto before=face_frame(triangle,triangle.uv),after=face_frame(triangle,packed);
    require(dot(before.tangent,after.tangent)>.999f && dot(before.bitangent,after.bitangent)>.999f,
            "Uniform positive chart scaling preserves anatomical tangent and bitangent orientation");
    for(unsigned j=0;j<3;++j) {
      const unsigned k=(j+1)%3;
      const double source_length=std::hypot(double(triangle.uv[k].x)-triangle.uv[j].x,
                                            double(triangle.uv[k].y)-triangle.uv[j].y);
      const double texel_length=1536*std::hypot(double(packed[k].x)-packed[j].x,double(packed[k].y)-packed[j].y);
      require(std::abs(texel_length-source_length*895*chart.scale)<.0003,
              "Every chart edge has isotropic texel density; main-chart linear density rises by 2.88322148x over 447px");
    }
    for(const auto& weights:face_weights) {
      const auto source=interpolate_uv(triangle.uv,weights),interpolated=interpolate_uv(packed,weights);
      const auto mapped=character_surface_uv(CharacterSurface::Face,source.x,source.y);
      require(close(interpolated.x,mapped.x,2e-7f) && close(interpolated.y,mapped.y,2e-7f),
              "All source triangle corners, edges and interior samples remain affine within one chart");
    }
  }
  for(unsigned i=0;i<face_charts.size();++i) {
    const auto& chart=face_charts[i];
    require(close(lows[i].x,chart.low.x,1e-8f) && close(lows[i].y,chart.low.y,1e-8f) &&
            close(highs[i].x,chart.high.x,1e-8f) && close(highs[i].y,chart.high.y,1e-8f),
            "Canonical chart bounds match the actual union of Mira and Jon source arrays");
    require(chart.x%64==0 && chart.y%64==0 && chart.width%64==0 && chart.height%64==0,
            "Every chart cell aligns to the full retained mip block size");
    for(const auto corner:std::array<Vec2,4>{{chart.low,chart.high,{chart.low.x,chart.high.y},{chart.high.x,chart.low.y}}}) {
      const auto mapped=character_surface_uv(CharacterSurface::Face,corner.x,corner.y);
      require(close(mapped.x,(64.5f+895*((corner.x-chart.low.x)*chart.scale+chart.origin.x/447))/1536) &&
              close(mapped.y,(64.5f+895*((corner.y-chart.low.y)*chart.scale+chart.origin.y/447))/1536),
              "Canonical chart classifier includes every bounding-box edge exactly");
    }
    const Vec2 middle{(chart.low.x+chart.high.x)*.5f,(chart.low.y+chart.high.y)*.5f};
    for(const auto outside:std::array<Vec2,4>{{{chart.low.x-.0001f,middle.y},{chart.high.x+.0001f,middle.y},
                                             {middle.x,chart.low.y-.0001f},{middle.x,chart.high.y+.0001f}}}) {
      const auto mapped=character_surface_uv(CharacterSurface::Face,outside.x,outside.y);
      require(close(mapped.x,(64.5f+895*outside.x)/1536) && close(mapped.y,(64.5f+895*outside.y)/1536),
              "Unknown points beyond the tiny bounding-box tolerance use the original identity fallback");
    }
    std::cout<<"Face chart "<<chart.name<<": "<<counts[i]<<" triangles, density "<<895*chart.scale/447<<"x\n";
  }
  for(const Vec2 uv:std::array<Vec2,4>{{{0,0},{1,1},{.25f,.25f},{.6f,.9f}}}) {
    const auto mapped=character_surface_uv(CharacterSurface::Face,uv.x,uv.y);
    require(close(mapped.x,(64.5f+895*uv.x)/1536) && close(mapped.y,(64.5f+895*uv.y)/1536),
            "Unknown finite Face UVs retain the clamped identity fallback and endpoint bounds");
  }
}
std::size_t validate_map(const RgbaImage& image,const std::vector<RgbaImage>& mips,bool normals,bool cutout=false) {
  require(image.valid() && image.width==character_surface_atlas_size && image.height==character_surface_atlas_size,
          "Every character PBR map is a complete 1536px RGBA image");
  require(mips.size()==6 && mips.back().width==24 && mips.back().height==24,
          "Mip chain is bounded at the last chart-safe 24px level");
  std::size_t bytes=0;
  for(unsigned level=0;level<=mips.size();++level) {
    const auto& map=level?mips[level-1]:image;
    require(map.valid() && map.width==(character_surface_atlas_size>>level) && map.height==(character_surface_atlas_size>>level),
            "Additional mip zero is half resolution; every supplied level has valid dimensions");
    bytes+=map.pixels.size();
    for(std::size_t p=0;p<map.pixels.size();p+=4) {
      const int x=int(p/4)%map.width,y=int(p/4)/map.width;
      bool strand_texel=false;
      for(auto surface:{CharacterSurface::HairStrands,CharacterSurface::HairShortStrands,
                        CharacterSurface::HairSwept,CharacterSurface::HairBundle,CharacterSurface::HairSourceFold}) {
        const auto& strands=character_surface_region(surface);
        strand_texel|=x>=(strands.x>>level) && x<((strands.x+strands.size)>>level) &&
                      y>=(strands.y>>level) && y<((strands.y+strands.size)>>level);
      }
      require((cutout && strand_texel) || map.pixels[p+3]==255,
              "Only the existing and appended hair-card base-color alpha masks may be transparent; all other tiles, gutters and both data maps stay opaque at every mip");
      if(normals) {
        const Vec3 n{map.pixels[p]/127.5f-1.f,map.pixels[p+1]/127.5f-1.f,map.pixels[p+2]/127.5f-1.f};
        require(std::isfinite(length(n)) && std::abs(length(n)-1.f)<.008f && n.z>.985f,
                "Normal map texels remain normalized, outward and physically restrained through minification");
      }
    }
  }
  return bytes;
}
void channels_and_physical_response(const MaterialTextures& t) {
  std::size_t bytes=validate_map(t.base_color,t.base_color_mips,false,true);
  bytes+=validate_map(t.normal,t.normal_mips,true);
  bytes+=validate_map(t.metallic_roughness,t.metallic_roughness_mips,false);
  require(bytes==37746432u,
          "Three RGBA8 maps with six retained mips use exactly 37746432 bytes and remain shared between characters");
  require(!t.emissive.valid() && t.emissive_mips.empty(),"Character surfaces never bake emission into their maps");
  constexpr std::array<float,13> rough_min{{.37f,.74f,.35f,.42f,.81f,.77f,.50f,.15f,.84f,.25f,.35f,.60f,.60f}};
  constexpr std::array<float,13> rough_max{{.51f,.84f,.44f,.50f,.91f,.87f,.61f,.20f,.92f,.33f,.44f,.67f,.68f}};
  for(unsigned i=0;i<surfaces.size();++i) {
    double rough_sum=0,color_sum=0,normal_energy=0;
    float min_rough=1,max_rough=0;
    for(int y=0;y<=48;++y) for(int x=0;x<=48;++x) {
      const float u=float(x)/48.f,v=float(y)/48.f;
      const auto orm=sample(t.metallic_roughness,t.metallic_roughness_mips,surfaces[i],u,v,false);
      require(close(orm.x,1) && close(orm.w,1),"ORM reserves fully unoccluded R and opaque alpha; no directional AO is painted");
      require(orm.y>=rough_min[i] && orm.y<=rough_max[i],"Each material samples within its documented physical roughness range");
      if(surfaces[i]==CharacterSurface::Metal)
        require(orm.z>.93f && orm.z<.95f,"Hardware uses metallic B, independently of its roughness G");
      else require(close(orm.z,0),"Skin, cloth, hair, leather, eyes and rubber remain dielectric");
      const auto color=sample(t.base_color,t.base_color_mips,surfaces[i],u,v,true);
      require(color.x>.80f && color.y>.79f && color.z>.80f && color.x<=1 && color.y<=1 && color.z<=1,
              "Linear base-color multipliers remain bounded and preserve authored vertex tint");
      const auto n=sample(t.normal,t.normal_mips,surfaces[i],u,v,false);
      normal_energy+=std::abs(n.x*2-1)+std::abs(n.y*2-1);
      rough_sum+=orm.y; color_sum+=(color.x+color.y+color.z)/3;
      min_rough=std::min(min_rough,orm.y); max_rough=std::max(max_rough,orm.y);
    }
    constexpr double count=49*49;
    if(surfaces[i]==CharacterSurface::Skin)
      require(color_sum/count>.95 && color_sum/count<.99,
              "Skin's sampled linear reflectance catches accidental double sRGB decoding or missing encoding");
    if(surfaces[i]==CharacterSurface::Jacket || surfaces[i]==CharacterSurface::Leather || surfaces[i]==CharacterSurface::Hair)
      require(normal_energy/count>.009 && max_rough-min_rough>.02f,
              "Cloth, leather and hair contain actual normal/roughness variation, not flat placeholder maps");
    if(surfaces[i]==CharacterSurface::HairStrands || surfaces[i]==CharacterSurface::HairShortStrands)
      require(color_sum/count>.89 && color_sum/count<.99,
              "HairStrands keeps a near-neutral mean multiplier so authored hair tint remains visible");
    std::cout<<names[i]<<": mean roughness "<<rough_sum/count<<", linear tint "<<color_sum/count<<'\n';
  }
}
// Captured from the preceding twelve-tile atlas before HairShortStrands was added.
// Each FNV-1a digest visits every local RGBA byte, map and retained mip, including gutters.
void previous_surface_bytes(const MaterialTextures& textures) {
  constexpr std::array<std::uint64_t,12> expected{{
    0xccec0c990f13341eull,0x06c046d2b20e68e3ull,0xb8f7e753e538e08eull,
    0xb6a59f21a5e6e185ull,0x28972394f7d7d236ull,0x00e7c4d8d2e17a93ull,
    0xd2399652431f650eull,0x323f907733a7678dull,0x35071f8757a1ea29ull,
    0x03d6561989ecba4cull,0xb62d862ce90f4b23ull,0x061f2806a2d6bc1cull
  }};
  struct Map { const RgbaImage* base; const std::vector<RgbaImage>* mips; };
  const std::array<Map,3> maps{{
    {&textures.base_color,&textures.base_color_mips}, {&textures.normal,&textures.normal_mips},
    {&textures.metallic_roughness,&textures.metallic_roughness_mips}
  }};
  std::size_t bytes=0;
  for(unsigned i=0;i<expected.size();++i) {
    const auto& r=character_surface_region(CharacterSurface(i));
    std::uint64_t digest=14695981039346656037ull;
    for(const auto& map:maps) for(unsigned level=0;level<=map.mips->size();++level) {
      const auto& image=level?(*map.mips)[level-1]:*map.base;
      for(int y=r.y>>level;y<(r.y+r.size)>>level;++y) {
        const auto p=(std::size_t(y)*image.width+(r.x>>level))*4;
        const auto size=std::size_t(r.size>>level)*4;
        for(std::size_t x=0;x<size;++x) digest=(digest^image.pixels[p+x])*1099511628211ull;
        bytes+=size;
      }
    }
    if(digest!=expected[i]) throw std::runtime_error(std::string("Previous atlas bytes changed: ")+names[i]);
  }
  require(bytes==31455360u,"Exact regression hashes cover every byte of all twelve preceding surfaces");
  std::cout<<"Previous twelve surfaces unchanged: "<<bytes<<" local map/mip bytes\n";
}
void anatomical_face_mapping(const MaterialTextures& t) {
  double lip_redness=0,chin_redness=0,nose_roughness=0,chin_roughness=0;
  unsigned lips=0,chin=0,nose=0;
  // Independent geometry-space selections inspect actual source-triangle UVs.
  // They would fail if the old U=.25/V=.25 cylindrical pigment survived.
  for(std::size_t i=0;i<mira_anatomy::indices.size();i+=3) {
    Vec3 p{}; Vec2 uv{};
    for(unsigned j=0;j<3;++j) {
      p+=mira_anatomy::positions[mira_anatomy::indices[i+j]]*(1.f/3.f);
      const auto corner=mira_anatomy::uv_coordinates[mira_anatomy::uv_indices[i+j]];
      uv.x+=corner.x/3.f; uv.y+=corner.y/3.f;
    }
    const auto color=sample(t.base_color,t.base_color_mips,CharacterSurface::Face,uv.x,uv.y,true);
    const auto orm=sample(t.metallic_roughness,t.metallic_roughness_mips,CharacterSurface::Face,uv.x,uv.y,false);
    if(std::abs(p.x)<.008f && p.y>.386f && p.y<.396f && p.z>.085f) {
      lip_redness+=color.x/color.y; ++lips;
    }
    if(std::abs(p.x)<.012f && p.y>.362f && p.y<.376f && p.z>.06f) {
      chin_redness+=color.x/color.y; chin_roughness+=orm.y; ++chin;
    }
    if(std::abs(p.x)<.004f && p.y>.403f && p.y<.412f && p.z>.09f) {
      nose_roughness+=orm.y; ++nose;
    }
  }
  require(lips>10 && chin>10 && nose>10,"Anatomical feature tests sample real surface triangles, not internal joint markers");
  require(lip_redness/lips>chin_redness/chin+.05,
          "Actual anatomical lip UVs receive restrained red pigment while the chin retains neutral skin");
  require(nose_roughness/nose<chin_roughness/chin-.02,
          "Anatomical nose UVs receive the subtle smoother response at the correct source-chart position");
  const auto unused=sample(t.base_color,t.base_color_mips,CharacterSurface::Face,.25f,.25f,true);
  require(unused.x/unused.y<1.025f,"Unused face-chart space is neutral skin, not the former cylindrical painted mouth");
  std::vector<Vec4> first(mira_anatomy::positions.size());
  std::vector<bool> seen(first.size(),false);
  float seam_difference=0;
  for(std::size_t i=0;i<mira_anatomy::indices.size();++i) {
    const auto vertex=mira_anatomy::indices[i];
    const auto uv=mira_anatomy::uv_coordinates[mira_anatomy::uv_indices[i]];
    const auto color=sample(t.base_color,t.base_color_mips,CharacterSurface::Face,uv.x,uv.y,true);
    if(seen[vertex]) seam_difference=std::max({seam_difference,std::abs(color.x-first[vertex].x),
        std::abs(color.y-first[vertex].y),std::abs(color.z-first[vertex].z)});
    else { first[vertex]=color; seen[vertex]=true; }
  }
  require(seam_difference<.055f,"Separate UV corners of the same anatomical vertex remain free of conspicuous pigment seams");
  std::cout<<"Anatomical face: lip/chin redness "<<lip_redness/lips<<'/'<<chin_redness/chin
           <<", nose/chin roughness "<<nose_roughness/nose<<'/'<<chin_roughness/chin
           <<", maximum chart-seam tint delta "<<seam_difference<<'\n';
}
void bounded_mips(RgbaImage& base,std::vector<RgbaImage>& mips,TextureEncoding encoding,bool bounded) {
  auto chain=build_mip_chain(base,encoding);
  mips.clear();
  const auto count=bounded?std::size_t(character_surface_max_mip):chain.size()-1;
  for(std::size_t level=1;level<=count;++level) mips.push_back(std::move(chain[level]));
}
void tile_map_isolation(const MaterialTextures& textures,CharacterSurface surface) {
  const auto& r=character_surface_region(surface);
  struct Map { const RgbaImage* base; const std::vector<RgbaImage>* mips;
               TextureEncoding encoding; std::array<std::uint8_t,4> hostile; };
  const std::array<Map,3> maps{{
    {&textures.base_color,&textures.base_color_mips,TextureEncoding::SRGB,{{255,0,255,0}}},
    {&textures.normal,&textures.normal_mips,TextureEncoding::Normal,{{255,128,128,255}}},
    {&textures.metallic_roughness,&textures.metallic_roughness_mips,TextureEncoding::Linear,{{0,0,255,255}}}
  }};
  for(const auto& map:maps) {
    RgbaImage hostile=*map.base;
    for(int y=0;y<hostile.height;++y) for(int x=0;x<hostile.width;++x) {
      if(x>=r.x && x<r.x+r.size && y>=r.y && y<r.y+r.size) continue;
      std::copy(map.hostile.begin(),map.hostile.end(),hostile.pixels.begin()+
                (std::size_t(y)*hostile.width+x)*4);
    }
    std::vector<RgbaImage> safe,unsafe;
    bounded_mips(hostile,safe,map.encoding,true);
    const bool srgb=map.encoding==TextureEncoding::SRGB;
    for(float lod:{0.f,.5f,1.7f,3.f,4.9f,5.5f,6.f,100.f})
      for(float u:{0.f,.001f,.5f,.999f,1.f}) for(float v:{0.f,.001f,.5f,.999f,1.f})
        require(close(sample(*map.base,*map.mips,surface,u,v,srgb,lod),
                      sample(hostile,safe,surface,u,v,srgb,lod)),
                "Selected tile color, alpha, normal and ORM stay isolated at fractional and extreme LODs");
    bounded_mips(hostile,unsafe,map.encoding,false);
    require(!close(sample(*map.base,*map.mips,surface,.5f,.5f,srgb,100),
                   sample(hostile,unsafe,surface,.5f,.5f,srgb,100),.1f),
            "Unsafe complete mips contaminate each tile map, proving the isolation fixture is sensitive");
  }
}
void hair_crop_field(const MaterialTextures& textures) {
  const auto surface=CharacterSurface::HairCrop;
  const auto& r=character_surface_region(surface);
  const int n=r.size-2*r.gutter;
  unsigned short_fibers=0,active_columns=0,longest_run=0;
  double normal_energy=0,across=0,along=0;
  float minimum=1,maximum=0;
  auto color=[&](int x,int y) {
    return sample(textures.base_color,textures.base_color_mips,surface,
                  float(x)/float(n-1),float(y)/float(n-1),true).x;
  };
  for(int x=0;x<n;++x) {
    unsigned run=0,column_fibers=0;
    for(int y=0;y<n;++y) {
      const float c=color(x,y);
      minimum=std::min(minimum,c); maximum=std::max(maximum,c);
      if(c>.948f) { ++run; ++short_fibers; ++column_fibers; }
      else run=0;
      longest_run=std::max(longest_run,run);
      if(x) across+=std::abs(c-color(x-1,y));
      if(y) along+=std::abs(c-color(x,y-1));
      const auto normal=sample(textures.normal,textures.normal_mips,surface,
                               float(x)/float(n-1),float(y)/float(n-1),false);
      normal_energy+=std::abs(normal.x*2-1)+std::abs(normal.y*2-1);
    }
    active_columns+=column_fibers>0;
  }
  const double coverage=double(short_fibers)/(n*n);
  std::cout<<"Hair crop: fiber coverage "<<coverage<<", active columns "<<active_columns
           <<", longest short run "<<longest_run<<", across/along variation "<<across/along
           <<", mean normal relief "<<normal_energy/(n*n)<<'\n';
  require(coverage>.05 && coverage<.45 && active_columns>unsigned(n*.9f),
          "Dense short hair detail covers the cap without becoming sparse or a flat fill");
  require(longest_run>2 && longest_run<24 && across>1.5*along,
          "The crop has short, directionally coherent fibers rather than long bands or isotropic noise");
  require(maximum-minimum>.025f && minimum>.91f && maximum<.98f &&
          normal_energy/(n*n)>.008 && normal_energy/(n*n)<.05,
          "Crop reflectance stays near-neutral with visible but shallow normal relief");
  tile_map_isolation(textures,surface);
}
void hair_short_strand_field(const MaterialTextures& textures) {
  const auto surface=CharacterSurface::HairShortStrands;
  const auto& r=character_surface_region(surface);
  const auto& original=character_surface_region(CharacterSurface::HairStrands);
  double slope=0,roughness=0; unsigned count=0;
  for(unsigned level=0;level<=textures.base_color_mips.size();++level) {
    const auto& color=level?textures.base_color_mips[level-1]:textures.base_color;
    const auto& normal=level?textures.normal_mips[level-1]:textures.normal;
    const auto& orm=level?textures.metallic_roughness_mips[level-1]:textures.metallic_roughness;
    const int size=r.size>>level;
    for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
      const auto p=(std::size_t((r.y>>level)+y)*color.width+(r.x>>level)+x)*4;
      const auto old=(std::size_t((original.y>>level)+y)*color.width+(original.x>>level)+x)*4;
      require(color.pixels[p+3]==color.pixels[old+3],
              "Short cards retain every connected long-card alpha texel, including gutters and all mips");
      require(orm.pixels[p]==255 && orm.pixels[p+2]==0 && orm.pixels[p+3]==255 &&
              orm.pixels[p+1]>=153 && orm.pixels[p+1]<=173 && normal.pixels[p+3]==255,
              "Short-card ORM stays matte, unoccluded and dielectric with opaque data channels at every mip");
      if(level==0 && x>=r.gutter && x<size-r.gutter && y>=r.gutter && y<size-r.gutter) {
        slope+=std::abs(normal.pixels[p]/127.5-1.); roughness+=orm.pixels[p+1]/255.; ++count;
      }
    }
  }
  require(slope/count>.06 && slope/count<.11 && roughness/count>.64 && roughness/count<.66,
          "Millimetre-scale short cards retain shallow directional normals and the intended matte lobe");
  std::cout<<"Short-card mean normal X slope "<<slope/count<<", roughness "<<roughness/count<<'\n';
  tile_map_isolation(textures,surface);
}
void hair_strand_mask(const MaterialTextures& textures,CharacterSurface surface) {
  const auto& region=character_surface_region(surface);
  const int n=region.size-2*region.gutter;
  require(n==192,"HairStrands reserves a 192px content interior with 32px gutters");
  auto alpha=[&](int x,int y) {
    const auto offset=(std::size_t(region.y+region.gutter+y)*textures.base_color.width+
                       region.x+region.gutter+x)*4;
    return int(textures.base_color.pixels[offset+3]);
  };
  std::vector<unsigned char> covered(std::size_t(n)*n),seen(covered.size());
  unsigned occupied=0,antialiased=0;
  double across=0,along=0,central=0,ends=0;
  unsigned central_count=0,end_count=0;
  for(int y=0;y<n;++y) for(int x=0;x<n;++x) {
    const int a=alpha(x,y);
    covered[std::size_t(y)*n+x]=a>=102;
    occupied+=a>=102; antialiased+=a>0 && a<255;
    if(x+1<n) across+=std::abs(a-alpha(x+1,y));
    if(y+1<n) along+=std::abs(a-alpha(x,y+1));
    if(y>=n/3 && y<2*n/3) { central+=a; ++central_count; }
    if(y<n/10 || y>=n-n/10) { ends+=a; ++end_count; }
    if(x==0 || x==n-1 || y==0 || y==n-1)
      require(a==0,"Hair card U margins and root/tip endpoints are fully transparent");
  }
  const double coverage=double(occupied)/(n*n);
  require(coverage>.10 && coverage<.90 && antialiased>unsigned(n),
          "HairStrands contains substantial visible strands, transparent gaps and antialiased edges");
  require(central/central_count>255*.15 && ends/end_count<central/central_count*.70,
          "Continuous strands taper in coverage at both longitudinal ends");
  require(across>0 && along<across*.35,
          "Alpha changes primarily across strands, rejecting random speckle or punched-out holes along their length");
  for(int y=-region.gutter;y<n+region.gutter;++y)
    for(int x=-region.gutter;x<n+region.gutter;++x)
      if(x<0 || x>=n || y<0 || y>=n)
        require(alpha(x,y)==0,"All HairStrands gutters extend transparent perimeter texels without wrapping an opaque edge");

  // Flood actual thresholded texels, independent of the procedural formula.
  // Small disconnected dashes and random dots cannot stand in for long fibers.
  unsigned long_pixels=0,long_components=0,curved_components=0;
  std::vector<int> queue;
  for(int start=0;start<n*n;++start) if(covered[std::size_t(start)] && !seen[std::size_t(start)]) {
    queue.clear(); queue.push_back(start); seen[std::size_t(start)]=1;
    int top=n,bottom=0;
    std::vector<double> row_x(n,0);
    std::vector<unsigned> row_count(n,0);
    for(std::size_t head=0;head<queue.size();++head) {
      const int x=queue[head]%n,y=queue[head]/n;
      top=std::min(top,y); bottom=std::max(bottom,y);
      row_x[std::size_t(y)]+=x; ++row_count[std::size_t(y)];
      for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        const int nx=x+dx,ny=y+dy;
        if(nx<0 || nx>=n || ny<0 || ny>=n) continue;
        const auto next=std::size_t(ny)*n+nx;
        if(covered[next] && !seen[next]) { seen[next]=1; queue.push_back(int(next)); }
      }
    }
    if(bottom-top>=n/2) {
      long_pixels+=unsigned(queue.size()); ++long_components;
      double left=n,right=0;
      for(int y=std::max(top,n/4);y<=std::min(bottom,3*n/4);++y) if(row_count[std::size_t(y)]) {
        const double center=row_x[std::size_t(y)]/row_count[std::size_t(y)];
        left=std::min(left,center); right=std::max(right,center);
      }
      curved_components+=right-left>.5;
    }
  }
  require(long_components>0 && long_pixels>=occupied*.95,
          "At least 95 percent of covered texels belong to connected fibers spanning half the card length");
  require(curved_components>0,"Longitudinal strand centers bend across V instead of forming rigid straight bars");
  // Every transparent gap must lead out to the perimeter. Enclosed transparent
  // islands would be holes punched into otherwise solid fibers.
  std::fill(seen.begin(),seen.end(),0); queue.clear();
  for(int y=0;y<n;++y) for(int x=0;x<n;++x)
    if(x==0 || x==n-1 || y==0 || y==n-1) {
      const auto p=std::size_t(y)*n+x;
      if(!covered[p] && !seen[p]) { seen[p]=1; queue.push_back(int(p)); }
    }
  for(std::size_t head=0;head<queue.size();++head) {
    const int x=queue[head]%n,y=queue[head]/n;
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
      const int nx=x+dx,ny=y+dy;
      if(nx<0 || nx>=n || ny<0 || ny>=n) continue;
      const auto next=std::size_t(ny)*n+nx;
      if(!covered[next] && !seen[next]) { seen[next]=1; queue.push_back(int(next)); }
    }
  }
  for(std::size_t p=0;p<covered.size();++p)
    require(covered[p] || seen[p],"Hair fibers contain no enclosed transparent pinholes at the production cutoff");

  double reference_mean=0;
  for(unsigned level=0;level<=textures.base_color_mips.size();++level) {
    const auto& map=level?textures.base_color_mips[level-1]:textures.base_color;
    const int x0=region.x>>level,y0=region.y>>level,size=region.size>>level;
    double total=0; int maximum=0;
    for(int y=y0;y<y0+size;++y) for(int x=x0;x<x0+size;++x) {
      const int a=map.pixels[(std::size_t(y)*map.width+x)*4+3];
      total+=a; maximum=std::max(maximum,a);
    }
    const double mean=total/(255.*size*size);
    if(level==0) reference_mean=mean;
    require(std::abs(mean-reference_mean)<.02,
            "Every retained alpha mip preserves tile-average coverage instead of becoming opaque or disappearing");
    require(maximum>=102 && mean>.05 && mean<.75,
            "Every retained HairStrands mip keeps meaningful cutout coverage at the production .4 threshold");
  }

  // An adversarial alpha-only change outside this tile must never affect it.
  RgbaImage hostile=textures.base_color;
  for(int y=0;y<hostile.height;++y) for(int x=0;x<hostile.width;++x)
    if(x<region.x || x>=region.x+region.size || y<region.y || y>=region.y+region.size)
      hostile.pixels[(std::size_t(y)*hostile.width+x)*4+3]=0;
  std::vector<RgbaImage> safe;
  bounded_mips(hostile,safe,TextureEncoding::SRGB,true);
  for(float lod:{0.f,.5f,1.f,2.5f,4.f,5.5f,6.f,100.f})
    for(float u:{0.f,.001f,.1f,.5f,.9f,.999f,1.f})
      for(float v:{0.f,.001f,.1f,.5f,.9f,.999f,1.f}) {
        const auto a=sample(textures.base_color,textures.base_color_mips,surface,u,v,true,lod);
        const auto b=sample(hostile,safe,surface,u,v,true,lod);
        require(close(a,b),"Hair alpha remains isolated from neighboring material alpha at fractional and extreme LODs");
      }
  std::vector<RgbaImage> unsafe;
  bounded_mips(hostile,unsafe,TextureEncoding::SRGB,false);
  const auto safe_center=sample(textures.base_color,textures.base_color_mips,surface,.5f,.5f,true,100);
  const auto unsafe_center=sample(hostile,unsafe,surface,.5f,.5f,true,100);
  require(safe_center.w-unsafe_center.w>.10,
          "Deliberately unbounded mips destroy alpha coverage, proving the neighboring-alpha control is sensitive");
  std::cout<<names[unsigned(surface)]<<" mask: coverage "<<coverage<<", long components "<<long_components
           <<", curved components "<<curved_components<<", longitudinal/across variation "<<along/across<<'\n';
}
void filtering_and_seams(const MaterialTextures& t) {
  for(auto surface:surfaces) {
    // Face is an anatomical chart atlas, not a wrapped cylindrical surface.
    if(surface==CharacterSurface::Face || surface==CharacterSurface::HairStrands ||
       surface==CharacterSurface::HairShortStrands) continue;
    for(float lod:{0.f,.5f,1.f,2.f,3.5f,4.f,5.f,6.f,30.f}) {
      for(float v:{0.f,.13f,.5f,.87f,1.f}) {
        const auto a=sample(t.base_color,t.base_color_mips,surface,0,v,true,lod);
        const auto b=sample(t.base_color,t.base_color_mips,surface,1,v,true,lod);
        if(!close(a,b,lod==0?.00001f:.025f))
          throw std::runtime_error(std::string("Wrapped tint seam: ")+names[unsigned(surface)]+" LOD "+
              std::to_string(lod)+" V "+std::to_string(v)+" R "+std::to_string(a.x)+"/"+std::to_string(b.x));
        const auto na=sample(t.normal,t.normal_mips,surface,0,v,false,lod);
        const auto nb=sample(t.normal,t.normal_mips,surface,1,v,false,lod);
        require(close(na,nb,lod==0?.00001f:.026f),"Wrapped normal maps have no exceptional seam at any retained LOD");
      }
    }
    const auto coarse=sample(t.metallic_roughness,t.metallic_roughness_mips,surface,.98f,.98f,false,6);
    const auto far=sample(t.metallic_roughness,t.metallic_roughness_mips,surface,.98f,.98f,false,1000);
    require(close(coarse,far),"Extreme distance clamps to the region-safe final mip instead of combining materials");
  }
  // A destructive neighboring-material fixture proves isolation independently of
  // the production image's gentle colors. Every unrelated rectangle turns vivid
  // magenta with metallic B=1; even fractional-LOD edge samples must not change.
  for(auto surface:surfaces) {
    const auto& r=character_surface_region(surface);
    RgbaImage sentinel=t.metallic_roughness;
    for(int y=0;y<character_surface_atlas_size;++y) for(int x=0;x<character_surface_atlas_size;++x) {
      if(x>=r.x && x<r.x+r.size && y>=r.y && y<r.y+r.size) continue;
      const auto p=(std::size_t(y)*character_surface_atlas_size+x)*4;
      sentinel.pixels[p]=0; sentinel.pixels[p+1]=0; sentinel.pixels[p+2]=255;
    }
    auto chain=build_mip_chain(sentinel,TextureEncoding::Linear);
    std::vector<RgbaImage> mips;
    for(int level=1;level<=6;++level) mips.push_back(std::move(chain[std::size_t(level)]));
    for(float lod:{0.f,.5f,1.7f,3.f,4.9f,5.5f,6.f,100.f})
      for(float u:{0.f,.001f,.5f,.999f,1.f}) for(float v:{0.f,.001f,.5f,.999f,1.f}) {
        const auto original=sample(t.metallic_roughness,t.metallic_roughness_mips,surface,u,v,false,lod);
        const auto modified=sample(sentinel,mips,surface,u,v,false,lod);
        require(close(original,modified),"Bilinear/trilinear samples never touch adjacent atlas materials, including corners at maximum LOD");
      }
  }
  // Read actual production minified texels: cloth remains rough dielectric;
  // its immediate metal neighbor remains conductive, even under huge footprints.
  const auto rubber=sample_material_texture(t.metallic_roughness,t.metallic_roughness_mips,
      character_surface_uv(CharacterSurface::Rubber,1,1),false,{{9,9},{-9,9}});
  const auto metal=sample_material_texture(t.metallic_roughness,t.metallic_roughness_mips,
      character_surface_uv(CharacterSurface::Metal,0,0),false,{{9,9},{-9,9}});
  require(rubber.y>.84f && rubber.z==0 && metal.y<.33f && metal.z>.93f,
          "Actual footprint-driven material samples preserve dielectric/conductor separation at extreme minification");
}
void face_chart_isolation(const MaterialTextures& textures) {
  struct Map { const char* name; const RgbaImage* base; const std::vector<RgbaImage>* mips;
               TextureEncoding encoding; std::array<std::uint8_t,4> hostile; };
  const std::array<Map,3> maps{{
    {"color",&textures.base_color,&textures.base_color_mips,TextureEncoding::SRGB,{{255,0,255,255}}},
    {"normal",&textures.normal,&textures.normal_mips,TextureEncoding::Normal,{{255,128,128,255}}},
    {"ORM",&textures.metallic_roughness,&textures.metallic_roughness_mips,TextureEncoding::Linear,{{0,0,255,255}}}
  }};
  constexpr std::array<float,14> lods{{0,.5f,1,1.5f,2,2.5f,3,3.5f,4,4.5f,5,5.5f,6,100}};
  for(unsigned chart_id=0;chart_id<face_charts.size();++chart_id) {
    const auto& chart=face_charts[chart_id];
    std::vector<Vec2> probes;
    for(const auto& triangle:face_triangles()) if(triangle.chart==chart_id)
      for(const auto& weights:face_weights) {
        // Interpolate the actual remapped source corners, exactly as raster and
        // ray backends do. A second per-fragment chart lookup could hide a seam.
        std::array<Vec2,3> packed;
        for(unsigned j=0;j<3;++j) packed[j]=character_surface_uv(CharacterSurface::Face,triangle.uv[j].x,triangle.uv[j].y);
        probes.push_back(interpolate_uv(packed,weights));
      }
    require(!probes.empty(),"Every face chart has real source triangles to probe");
    for(const auto& map:maps) {
      RgbaImage sentinel=*map.base;
      for(int y=0;y<sentinel.height;++y) for(int x=0;x<sentinel.width;++x) {
        if(x>=chart.x && x<chart.x+chart.width && y>=chart.y && y<chart.y+chart.height) continue;
        const auto offset=(std::size_t(y)*sentinel.width+x)*4;
        std::copy(map.hostile.begin(),map.hostile.end(),sentinel.pixels.begin()+offset);
      }
      auto chain=build_mip_chain(sentinel,map.encoding);
      std::vector<RgbaImage> safe,unsafe;
      for(std::size_t level=1;level<chain.size();++level) {
        if(level<=std::size_t(character_surface_max_mip)) safe.push_back(chain[level]);
        unsafe.push_back(std::move(chain[level]));
      }
      const bool srgb=map.encoding==TextureEncoding::SRGB;
      for(float lod:lods) for(const auto uv:probes) {
        const auto original=sample_material_texture_lod(*map.base,*map.mips,uv,srgb,lod);
        const auto protected_sample=sample_material_texture_lod(sentinel,safe,uv,srgb,lod);
        if(!close(original,protected_sample))
          throw std::runtime_error(std::string("Face chart isolation: ")+chart.name+" "+map.name+
              " at LOD "+std::to_string(lod)+" UV "+std::to_string(uv.x)+","+std::to_string(uv.y));
      }
      const auto original=sample_material_texture_lod(*map.base,*map.mips,probes.front(),srgb,100);
      const auto contaminated=sample_material_texture_lod(sentinel,unsafe,probes.front(),srgb,100);
      require(std::max({std::abs(original.x-contaminated.x),std::abs(original.y-contaminated.y),
                        std::abs(original.z-contaminated.z)})>.25f,
              "Unsafe full-mip controls visibly contaminate every chart and map, proving hostile fixtures are sensitive");
    }
    std::cout<<"Face chart isolation "<<chart.name<<": "<<probes.size()<<" real triangle/edge probes through all six mips\n";
  }
}
void material_contract(const std::shared_ptr<const MaterialTextures>& t) {
  const auto material=make_character_surface_material();
  require(material.textures==t && character_surface_textures()==t,"All character instances share one immutable atlas allocation");
  require(material.metallic==1 && material.roughness==1 && material.normal_scale==1,
          "Material factors preserve the atlas's independent roughness and metallic channels");
  require(material.albedo.x==1 && material.albedo.y==1 && material.albedo.z==1 &&
          material.world_uv_scale==0 && material.texture==TextureSlot::None,
          "Material uses the authored mesh UVs and vertex colors without projected legacy textures");
  require(material.emissive==0 && material.opacity==1 && !material.alpha_blend && material.alpha_cutoff<0,
          "The character material is opaque and nonemissive in every backend");
}
#if FURY_CHARACTER_SURFACE_GL_TESTS
constexpr int gl_size=64;
constexpr float gl_center_x=1.f/gl_size,gl_center_y=-1.f/gl_size;
using Pixel=std::array<int,3>;
int display(float value) { return int(std::pow(value/(1.f+value),1.f/2.2f)*255.f+.5f); }
Mesh uv_quad(Vec2 uv0,Vec2 uv1,float pixels) {
  const float r=pixels/gl_size;
  Mesh mesh;
  mesh.vertices={
    {{gl_center_x-r,gl_center_y-r,0},{0,0,1},{1,1,1},uv0},
    {{gl_center_x+r,gl_center_y-r,0},{0,0,1},{1,1,1},{uv1.x,uv0.y}},
    {{gl_center_x+r,gl_center_y+r,0},{0,0,1},{1,1,1},uv1},
    {{gl_center_x-r,gl_center_y+r,0},{0,0,1},{1,1,1},{uv0.x,uv1.y}}
  };
  mesh.indices={0,1,2,0,2,3};
  return mesh;
}
Mesh surface_quad(CharacterSurface surface,float pixels,bool constant_uv=false) {
  return uv_quad(character_surface_uv(surface,constant_uv?.5f:0.f,constant_uv?.5f:0.f),
                 character_surface_uv(surface,constant_uv?.5f:1.f,constant_uv?.5f:1.f),pixels);
}
void renderer_hair_crop_response(IRenderBackend& backend) {
  Lighting light;
  light.ambient={0,0,0}; light.sun_intensity=1; light.sun_color={1,1,1};
  light.sun_direction={0,0,-1}; light.point_light_count=0; light.ao_strength=0;
  light.enable_bloom=light.enable_reflections=light.enable_shadows=false;
  light.fog_start=100; light.fog_end=200;
  auto material=make_character_surface_material();
  material.albedo={0,0,0}; material.normal_scale=0;
  constexpr std::array<CharacterSurface,4> hair_surfaces{{CharacterSurface::Hair,CharacterSurface::HairCrop,
    CharacterSurface::HairStrands,CharacterSurface::HairShortStrands}};
  std::array<int,4> highlights{};
  for(unsigned i=0;i<highlights.size();++i) {
    backend.set_view_proj(look_at({0,0,3},{0,0,0},{0,1,0}),orthographic(-1,1,-1,1,.1f,10));
    backend.set_camera_position({0,0,3}); backend.set_lighting(light); backend.set_time(0);
    backend.begin_frame({0,0,0,255}); backend.set_object_id(1);
    backend.draw_mesh(surface_quad(hair_surfaces[i],40,true),
                      Mat4::identity(),material);
    int width=0,height=0; std::vector<std::uint8_t> image;
    require(backend.read_rgb_framebuffer(image,width,height) && width==gl_size && height==gl_size,
            "Read the actual renderer's short-crop material response");
    highlights[i]=image[(std::size_t(gl_size/2)*width+gl_size/2)*3];
  }
  require(highlights[1]>5 && highlights[0]>highlights[1]+5,
          "HairCrop roughness reduces the actual highlight while retaining dielectric specular response");
  require(highlights[3]>5 && highlights[2]>highlights[3]+5 &&
          std::abs(highlights[3]-highlights[1])<=5,
          "Short-card roughness reduces actual long-card highlights and matches the matte crop cap");
  std::cout<<backend.name()<<" Hair/crop/long-card/short-card highlight pixels "
           <<highlights[0]<<'/'<<highlights[1]<<'/'<<highlights[2]<<'/'<<highlights[3]<<'\n';
}
void renderer_hair_cutouts(IRenderBackend& backend,const MaterialTextures& textures,CharacterSurface surface) {
  Vec2 solid{},gap{}; bool found_solid=false,found_gap=false;
  for(int y=48;y<144;++y) for(int x=20;x<172;++x) {
    const float u=float(x)/191.f,v=float(y)/191.f;
    const auto color=sample(textures.base_color,textures.base_color_mips,surface,u,v,true);
    if(!found_solid && color.w>.98f) { solid={u,v}; found_solid=true; }
    if(!found_gap && color.w<.01f) { gap={u,v}; found_gap=true; }
  }
  require(found_solid && found_gap,"Actual generated HairStrands supplies opaque fiber and transparent interior-gap pixel probes");
  Lighting lighting;
  lighting.ambient={0,0,0}; lighting.sun_intensity=0; lighting.point_light_count=0;
  lighting.ao_strength=0; lighting.enable_bloom=lighting.enable_reflections=lighting.enable_shadows=false;
  lighting.fog_start=100; lighting.fog_end=200;
  auto foreground=make_character_surface_material();
  foreground.alpha_cutoff=.4f; foreground.normal_scale=0; foreground.albedo={0,0,0};
  foreground.emissive=1; foreground.emissive_color={1,0,0};
  Material background; background.albedo={0,0,0}; background.emissive=1;
  background.emissive_color={0,0,1}; background.textures=std::make_shared<MaterialTextures>();
  const auto rear=uv_quad({0,0},{0,0},60);
  auto draw=[&](const Mesh& mesh,const Material& material,bool reverse=false) {
    backend.set_view_proj(look_at({0,0,3},{0,0,0},{0,1,0}),orthographic(-1,1,-1,1,.1f,10));
    backend.set_camera_position({0,0,3}); backend.set_lighting(lighting); backend.set_time(0);
    // GL snapshots frame lighting when begin_frame uploads its uniforms.
    backend.begin_frame({0,0,0,255});
    auto front=[&] { backend.set_object_id(2); backend.draw_mesh(mesh,Mat4::identity(),material); };
    auto back=[&] { backend.set_object_id(1); backend.draw_mesh(rear,translate({0,0,-.5f}),background); };
    if(reverse) { back(); front(); } else { front(); back(); }
    int width=0,height=0; std::vector<std::uint8_t> image;
    require(backend.read_rgb_framebuffer(image,width,height) && width==gl_size && height==gl_size,
            "Read complete real-renderer cutout framebuffer");
    return image;
  };
  auto center=[](const std::vector<std::uint8_t>& image) {
    const auto p=(std::size_t(gl_size/2)*gl_size+gl_size/2)*3;
    return Pixel{image[p],image[p+1],image[p+2]};
  };
  // These probes keep the actual atlas texture and cutoff. The disabled-cutoff
  // control proves a blue result is alpha rejection, not missing/backfacing mesh.
  for(const auto uv:std::array<Vec2,6>{{{0,.5f},{1,.5f},{.5f,0},{.5f,1},gap,solid}}) {
    const auto atlas_uv=character_surface_uv(surface,uv.x,uv.y);
    const auto mesh=uv_quad(atlas_uv,atlas_uv,40);
    const bool visible=sample(textures.base_color,textures.base_color_mips,surface,uv.x,uv.y,true).w>=.4f;
    for(bool reverse:{false,true}) {
      const auto pixel=center(draw(mesh,foreground,reverse));
      if(!(visible?(pixel[0]>160 && pixel[2]<5):(pixel[2]>160 && pixel[0]<5)))
        throw std::runtime_error(std::string("Actual renderer hair cutout mismatch: ")+backend.name()+
            " UV "+std::to_string(uv.x)+","+std::to_string(uv.y)+
            " reverse "+std::to_string(reverse)+" expected visible "+std::to_string(visible)+
            " RGB "+std::to_string(pixel[0])+","+std::to_string(pixel[1])+","+std::to_string(pixel[2]));
    }
    auto opaque=foreground; opaque.alpha_cutoff=-1;
    const auto positive=center(draw(mesh,opaque));
    require(positive[0]>160 && positive[2]<5,
            "Disabling alpha cutoff fills the same real pixel, proving the HairStrands cutout fixture is sensitive");
  }
  // Existing opaque hair shares the cutout-enabled character material, so it
  // must remain visible even at its U/V endpoints.
  for(auto opaque_surface:{CharacterSurface::Hair,CharacterSurface::HairCrop}) {
    const auto opaque_uv=character_surface_uv(opaque_surface,0,0);
    const auto hair=center(draw(uv_quad(opaque_uv,opaque_uv,40),foreground));
    require(hair[0]>160 && hair[2]<5,"A shared .4 alpha cutoff preserves both opaque hair tiles");
  }
  auto red_pixels=[](const std::vector<std::uint8_t>& image) {
    unsigned count=0;
    for(std::size_t p=0;p<image.size();p+=3) count+=image[p]>160 && image[p+2]<5;
    return count;
  };
  const auto card=surface_quad(surface,56);
  const unsigned masked=red_pixels(draw(card,foreground));
  auto opaque=foreground; opaque.alpha_cutoff=-1;
  const unsigned filled=red_pixels(draw(card,opaque));
  require(masked>50 && filled>1000 && masked<filled*.90 && masked>filled*.10,
          "A full HairStrands card produces a nontrivial cutout silhouette under actual renderer texture footprints");
  std::cout<<backend.name()<<' '<<names[unsigned(surface)]<<" pixels: cutout "<<masked<<", opaque control "<<filled<<'\n';
}
void cpu_hair_cutouts(const MaterialTextures& textures) {
  SDL_setenv("SDL_VIDEODRIVER","dummy",1); SDL_setenv("FURY_CPU_THREADS","2",0);
  require(SDL_Init(SDL_INIT_VIDEO)==0,"Initialize dummy SDL video for CPU hair cutout regression");
  try {
    for(auto kind:{RenderBackendKind::Software,RenderBackendKind::CpuRayTracing}) {
      SDL_Window* window=SDL_CreateWindow("Hair strand CPU cutouts",0,0,gl_size,gl_size,SDL_WINDOW_HIDDEN);
      require(window!=nullptr,"Create hidden window for CPU hair cutout regression");
      auto backend=kind==RenderBackendKind::Software?create_software_backend():create_cpu_ray_backend();
      try {
        require(backend && backend->create(window,gl_size,gl_size) && backend->kind()==kind,
                "Create the actual requested CPU cutout backend without fallback");
        if(kind==RenderBackendKind::CpuRayTracing) {
          auto settings=backend->settings();
          settings.samples_per_pixel=1; settings.max_bounces=1; settings.denoise=false;
          settings.accumulate=false; settings.trace_mode=TraceMode::RayTraced;
          require(backend->configure(settings),"Configure deterministic CPU-ray hair cutout checks");
        }
        for(auto surface:{CharacterSurface::HairStrands,CharacterSurface::HairShortStrands})
          renderer_hair_cutouts(*backend,textures,surface);
        renderer_hair_crop_response(*backend);
      } catch(...) { backend.reset(); SDL_DestroyWindow(window); throw; }
      backend.reset(); SDL_DestroyWindow(window);
    }
  } catch(...) { SDL_Quit(); throw; }
  SDL_Quit();
}
Mesh face_chart_quad(unsigned chart,float pixels) {
  Vec2 low{1,1},high{0,0};
  for(const auto& triangle:face_triangles()) if(triangle.chart==chart)
    for(const auto source:triangle.uv) {
      const auto uv=character_surface_uv(CharacterSurface::Face,source.x,source.y);
      low.x=std::min(low.x,uv.x); low.y=std::min(low.y,uv.y);
      high.x=std::max(high.x,uv.x); high.y=std::max(high.y,uv.y);
    }
  require(low.x<high.x && low.y<high.y,"GL face chart range comes from actual remapped source corners");
  return uv_quad(low,high,pixels);
}
struct GlFixture {
  std::unique_ptr<IRenderBackend> backend;
  Lighting light;
  explicit GlFixture(SDL_Window* window):backend(create_gl_backend()) {
    require(backend && backend->create(window,gl_size,gl_size),"Actual GL backend initializes after a successful context probe");
    require(backend->kind()==RenderBackendKind::OpenGL,"Atlas GL test never silently falls back to another renderer");
    backend->set_msaa_samples(0);
    light.ambient={1,1,1}; light.sun_intensity=0; light.sun_color={1,1,1};
    light.sun_direction={0,0,-1}; light.ao_strength=0; light.point_light_count=0;
    light.fog_start=100; light.fog_end=200;
    light.enable_bloom=light.enable_reflections=light.enable_shadows=false;
  }
  Pixel draw(const Mesh& mesh,const Material& material) {
    backend->set_view_proj(Mat4::identity(),Mat4::identity());
    backend->set_camera_position({gl_center_x,gl_center_y,3});
    backend->set_lighting(light); backend->set_time(0);
    backend->begin_frame({0,0,0,255});
    backend->draw_mesh(mesh,Mat4::identity(),material);
    int w=0,h=0; std::vector<std::uint8_t> image;
    require(backend->read_rgb_framebuffer(image,w,h) && w==gl_size && h==gl_size,
            "Read a complete actual OpenGL RGB framebuffer");
    const auto p=(std::size_t(gl_size/2)*gl_size+gl_size/2)*3;
    return {image[p],image[p+1],image[p+2]};
  }
};
void gl_region_isolation(GlFixture& fixture,const std::shared_ptr<const MaterialTextures>& original) {
  auto get_parameter=reinterpret_cast<void(*)(gl::GLenum,gl::GLenum,gl::GLint*)>(
      SDL_GL_GetProcAddress("glGetTexParameteriv"));
  require(get_parameter!=nullptr,"Query the actual GL texture maximum level");
  for(auto surface:{CharacterSurface::Face,CharacterSurface::Hair,CharacterSurface::Skin,
                   CharacterSurface::Jacket,CharacterSurface::Eyes,CharacterSurface::Rubber,CharacterSurface::Metal,
                   CharacterSurface::HairStrands,CharacterSurface::HairCrop,CharacterSurface::HairShortStrands}) {
    const auto& r=character_surface_region(surface);
    auto hostile=std::make_shared<MaterialTextures>(*original);
    for(int y=0;y<character_surface_atlas_size;++y) for(int x=0;x<character_surface_atlas_size;++x) {
      if(x>=r.x && x<r.x+r.size && y>=r.y && y<r.y+r.size) continue;
      const auto p=(std::size_t(y)*character_surface_atlas_size+x)*4;
      hostile->base_color.pixels[p]=255; hostile->base_color.pixels[p+1]=0; hostile->base_color.pixels[p+2]=255;
      hostile->metallic_roughness.pixels[p+1]=0; hostile->metallic_roughness.pixels[p+2]=255;
    }
    bounded_mips(hostile->base_color,hostile->base_color_mips,TextureEncoding::SRGB,true);
    bounded_mips(hostile->metallic_roughness,hostile->metallic_roughness_mips,TextureEncoding::Linear,true);
    auto material=make_character_surface_material(); material.normal_scale=0;
    // Quarter-pixel geometry covers a known pixel center, while helper-fragment
    // derivatives request LOD >9. Ordinary distant cards can hide the bug by
    // never requesting levels where complete-atlas filtering crosses regions.
    auto tiny=surface_quad(surface,.25f);
    fixture.light.ambient={1,1,1}; fixture.light.sun_intensity=0;
    const auto expected=fixture.draw(tiny,material);
    if(expected[1]<=170) throw std::runtime_error("Subpixel fixture needs a real lit pixel: RGB "+
        std::to_string(expected[0])+","+std::to_string(expected[1])+","+std::to_string(expected[2]));
    material.textures=hostile;
    const auto protected_pixel=fixture.draw(tiny,material);
    for(int c=0;c<3;++c)
      require(std::abs(expected[c]-protected_pixel[c])<=1,
              "Actual GL pixels retain their surface color at severe minification despite hostile neighboring materials");
    for(int map:{0,3,4}) {
      gl::ActiveTexture(gl::GL_TEXTURE0+map);
      gl::GLint max_level=-1;
      get_parameter(gl::GL_TEXTURE_2D,gl::GL_TEXTURE_MAX_LEVEL,&max_level);
      require(max_level==6,"Actual uploaded color, normal and ORM textures retain their six-level atlas cap");
    }
    gl::ActiveTexture(gl::GL_TEXTURE0);
    fixture.light.ambient={0,0,0}; fixture.light.sun_intensity=1;
    material.albedo={0,0,0}; material.textures=original;
    const auto expected_response=fixture.draw(tiny,material);
    material.textures=hostile;
    const auto protected_response=fixture.draw(tiny,material);
    for(int c=0;c<3;++c)
      require(std::abs(expected_response[c]-protected_response[c])<=1,
              "Actual GL roughness/metallic response cannot receive neighboring ORM values at severe minification");

    // Positive control: deliberately unsafe full mips must visibly contaminate
    // the same pixel, proving the fixture would catch the original upload bug.
    auto unsafe=std::make_shared<MaterialTextures>(*hostile);
    bounded_mips(unsafe->base_color,unsafe->base_color_mips,TextureEncoding::SRGB,false);
    material.textures=unsafe; material.albedo={1,1,1};
    fixture.light.ambient={1,1,1}; fixture.light.sun_intensity=0;
    const auto contaminated=fixture.draw(tiny,material);
    require(expected[1]-contaminated[1]>20,
            "The full-chain control visibly bleeds neighboring regions, proving this GL pixel regression is sensitive");
    std::cout<<"GL minification "<<names[unsigned(surface)]<<": protected green "<<protected_pixel[1]
             <<", unsafe full-chain green "<<contaminated[1]<<'\n';
  }
}
void gl_face_chart_isolation(GlFixture& fixture,const std::shared_ptr<const MaterialTextures>& original) {
  for(unsigned chart_id=0;chart_id<face_charts.size();++chart_id) {
    const auto& chart=face_charts[chart_id];
    auto hostile=std::make_shared<MaterialTextures>(*original);
    for(int y=0;y<character_surface_atlas_size;++y) for(int x=0;x<character_surface_atlas_size;++x) {
      if(x>=chart.x && x<chart.x+chart.width && y>=chart.y && y<chart.y+chart.height) continue;
      const auto p=(std::size_t(y)*character_surface_atlas_size+x)*4;
      hostile->base_color.pixels[p]=255; hostile->base_color.pixels[p+1]=0; hostile->base_color.pixels[p+2]=255;
      hostile->metallic_roughness.pixels[p]=0; hostile->metallic_roughness.pixels[p+1]=0;
      hostile->metallic_roughness.pixels[p+2]=255;
    }
    bounded_mips(hostile->base_color,hostile->base_color_mips,TextureEncoding::SRGB,true);
    bounded_mips(hostile->metallic_roughness,hostile->metallic_roughness_mips,TextureEncoding::Linear,true);
    auto material=make_character_surface_material(); material.normal_scale=0;
    // The range uses actual source extrema, never fallback Face UV 0..1. A tiny
    // on-center primitive forces the final retained mip for even socket charts.
    auto tiny=face_chart_quad(chart_id,.125f);
    fixture.light.ambient={1,1,1}; fixture.light.sun_intensity=0;
    const auto expected=fixture.draw(tiny,material);
    require(expected[1]>170,"Every real-source face-chart quad shades the intended GL pixel");
    material.textures=hostile;
    const auto protected_color=fixture.draw(tiny,material);
    for(unsigned c=0;c<3;++c)
      require(std::abs(expected[c]-protected_color[c])<=1,
              "Actual GL color cannot bleed across anatomical chart cells at severe minification");
    fixture.light.ambient={0,0,0}; fixture.light.sun_intensity=1;
    material.albedo={0,0,0}; material.textures=original;
    const auto expected_response=fixture.draw(tiny,material);
    material.textures=hostile;
    const auto protected_response=fixture.draw(tiny,material);
    for(unsigned c=0;c<3;++c)
      require(std::abs(expected_response[c]-protected_response[c])<=1,
              "Actual GL roughness and metallic response stays inside its anatomical chart cell");

    auto unsafe=std::make_shared<MaterialTextures>(*hostile);
    bounded_mips(unsafe->base_color,unsafe->base_color_mips,TextureEncoding::SRGB,false);
    bounded_mips(unsafe->metallic_roughness,unsafe->metallic_roughness_mips,TextureEncoding::Linear,false);
    material.textures=unsafe; material.albedo={1,1,1};
    fixture.light.ambient={1,1,1}; fixture.light.sun_intensity=0;
    const auto contaminated_color=fixture.draw(tiny,material);
    require(expected[1]-contaminated_color[1]>20,
            "Unsafe complete GL mips visibly bleed every face chart's hostile color control");
    material.albedo={0,0,0};
    fixture.light.ambient={0,0,0}; fixture.light.sun_intensity=1;
    const auto contaminated_response=fixture.draw(tiny,material);
    require(expected_response[0]-contaminated_response[0]>3,
            "Unsafe complete GL mips visibly change every face chart's hostile ORM control");
    std::cout<<"GL face chart "<<chart.name<<": protected green "<<protected_color[1]
             <<", unsafe green "<<contaminated_color[1]<<", protected/unsafe response "
             <<protected_response[0]<<'/'<<contaminated_response[0]<<'\n';
  }
}
void gl_surface_response(GlFixture& fixture,const std::shared_ptr<const MaterialTextures>& textures) {
  fixture.light.ambient={0,0,0}; fixture.light.sun_intensity=1;
  std::array<int,3> response{};
  const std::array<CharacterSurface,3> dielectrics{{CharacterSurface::Eyes,CharacterSurface::Skin,CharacterSurface::Jacket}};
  for(unsigned i=0;i<dielectrics.size();++i) {
    auto quad=surface_quad(dielectrics[i],40,true);
    auto material=make_character_surface_material(); material.albedo={0,0,0}; material.normal_scale=0;
    const auto orm=sample(textures->metallic_roughness,textures->metallic_roughness_mips,dielectrics[i],.5f,.5f,false);
    response[i]=fixture.draw(quad,material)[0];
    require(std::abs(response[i]-display(.04f*(1.f-.85f*orm.y)))<=2,
            "Real eye/skin/cloth highlights use the atlas's linear G roughness and dielectric B channel");
  }
  require(response[0]>response[1]+5 && response[1]>response[2]+5,
          "Smooth eyes, skin and rough clothing remain visibly distinct in actual GL lighting");
  auto hair_material=make_character_surface_material();
  hair_material.albedo={0,0,0}; hair_material.normal_scale=0;
  const int long_hair=fixture.draw(surface_quad(CharacterSurface::Hair,40,true),hair_material)[0];
  const int crop=fixture.draw(surface_quad(CharacterSurface::HairCrop,40,true),hair_material)[0];
  require(long_hair>crop+5,
          "The short crop's matte roughness visibly reduces the actual GL highlight with unchanged dielectric specular");
  std::cout<<"GL hair/crop highlight pixels="<<long_hair<<'/'<<crop<<'\n';
  auto quad=surface_quad(CharacterSurface::Metal,40,true);
  auto material=make_character_surface_material(); material.normal_scale=0; material.albedo={.45f,.45f,.45f};
  const auto orm=sample(textures->metallic_roughness,textures->metallic_roughness_mips,CharacterSurface::Metal,.5f,.5f,false);
  const auto color=sample(textures->base_color,textures->base_color_mips,CharacterSurface::Metal,.5f,.5f,true);
  const float base=.45f*color.x;
  const float direct=base*(1.f-.9f*orm.z)+(.04f*(1.f-orm.z)+base*orm.z)*(1.f-.85f*orm.y);
  const int metal=fixture.draw(quad,material)[0];
  require(std::abs(metal-display(direct))<=2,"Hardware's real GL pixel combines sRGB-decoded color with linear metallic B");
  material.metallic=0;
  const int disabled=fixture.draw(quad,material)[0];
  require(disabled>metal+5,"Disabling the metallic factor visibly changes hardware, catching a zero-metallic integration regression");
  std::cout<<"GL highlight pixels eyes/skin/jacket="<<response[0]<<'/'<<response[1]<<'/'<<response[2]
           <<"; hardware="<<metal<<", metallic disabled="<<disabled<<'\n';
}
int gl_tests(const std::shared_ptr<const MaterialTextures>& textures) {
  if(SDL_Init(SDL_INIT_VIDEO)!=0) { std::cout<<"SKIP: SDL video unavailable: "<<SDL_GetError()<<'\n'; return 77; }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0); SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0);
  SDL_Window* window=SDL_CreateWindow("Character atlas GL tests",0,0,gl_size,gl_size,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  if(!window) { std::cout<<"SKIP: GL window unavailable: "<<SDL_GetError()<<'\n'; SDL_Quit(); return 77; }
  SDL_GLContext probe=SDL_GL_CreateContext(window);
  if(!probe) { std::cout<<"SKIP: GL context unavailable: "<<SDL_GetError()<<'\n'; SDL_DestroyWindow(window); SDL_Quit(); return 77; }
  SDL_GL_DeleteContext(probe);
  int result=0;
  try {
    GlFixture fixture(window);
    std::cout<<"Character atlas GL driver: "<<reinterpret_cast<const char*>(gl::GetString(gl::GL_RENDERER))<<'\n';
    gl_region_isolation(fixture,textures); gl_face_chart_isolation(fixture,textures); gl_surface_response(fixture,textures);
    for(auto surface:{CharacterSurface::HairStrands,CharacterSurface::HairShortStrands})
      renderer_hair_cutouts(*fixture.backend,*textures,surface);
    renderer_hair_crop_response(*fixture.backend);
    const auto error=reinterpret_cast<gl::GLenum(*)()>(SDL_GL_GetProcAddress("glGetError"));
    require(error && error()==0,"Character atlas GL regressions leave no graphics API errors");
    std::cout<<"Character atlas GL pixel tests passed\n";
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
  SDL_DestroyWindow(window); SDL_Quit();
  return result;
}
#else
void cpu_hair_cutouts(const MaterialTextures&) {}
int gl_tests(const std::shared_ptr<const MaterialTextures>&) {
  std::cout<<"SKIP: build with SDL headers and Fury::Engine for --gl pixel tests\n";
  return 77;
}
#endif
void write_ppm(const std::filesystem::path& path,const RgbaImage& image) {
  std::ofstream out(path,std::ios::binary);
  out<<"P6\n"<<image.width<<' '<<image.height<<"\n255\n";
  for(std::size_t p=0;p<image.pixels.size();p+=4)
    out.write(reinterpret_cast<const char*>(image.pixels.data()+p),3);
  require(bool(out),"Write reviewable atlas pixels");
}
}  // namespace

int main(int argc,char** argv) {
  try {
    layout_and_api();
    face_chart_mapping();
    const auto textures=character_surface_textures();
    channels_and_physical_response(*textures);
    previous_surface_bytes(*textures);
    hair_crop_field(*textures);
    hair_short_strand_field(*textures);
    hair_strand_mask(*textures,CharacterSurface::HairStrands);
    hair_strand_mask(*textures,CharacterSurface::HairShortStrands);
    anatomical_face_mapping(*textures);
    filtering_and_seams(*textures);
    face_chart_isolation(*textures);
    material_contract(textures);
    if(argc==2 && std::string(argv[1])=="--gl") return gl_tests(textures);
    cpu_hair_cutouts(*textures);
    if(argc==3 && std::string(argv[1])=="--dump") {
      const std::filesystem::path directory=argv[2];
      std::filesystem::create_directories(directory);
      write_ppm(directory/"character_base_color.ppm",textures->base_color);
      write_ppm(directory/"character_normal.ppm",textures->normal);
      write_ppm(directory/"character_orm.ppm",textures->metallic_roughness);
    } else if(argc!=1) throw std::runtime_error("Usage: character_surface_tests [--dump directory | --gl]");
    std::cout<<"Character surface atlas tests passed\n";
    return 0;
  } catch(const std::exception& error) {
    std::cerr<<"Character surface atlas test failed: "<<error.what()<<'\n';
    return 1;
  }
}
