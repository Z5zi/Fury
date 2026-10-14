#include "fury/surface_detail.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace fury {
namespace detail {
// Kept separately testable for the height-gradient orientation regression.
// Internal implementation detail, not part of the public generator API.
RgbaImage rotate_surface_detail_quarter_turn(const RgbaImage& source, bool normal) {
  if(!source.valid()) return {};
  RgbaImage rotated{source.height,source.width,std::vector<std::uint8_t>(source.pixels.size())};
  for(int y=0;y<rotated.height;++y) for(int x=0;x<rotated.width;++x) {
    const auto from=(std::size_t(source.height-1-x)*source.width+y)*4;
    const auto to=(std::size_t(y)*rotated.width+x)*4;
    for(unsigned c=0;c<4;++c) rotated.pixels[to+c]=source.pixels[from+c];
    if(normal) {
      // H_new(u,v)=H_old(v,1-u), hence N_new.xy=(-N_old.y,N_old.x).
      rotated.pixels[to]=std::uint8_t(255u-source.pixels[from+1]);
      rotated.pixels[to+1]=source.pixels[from];
    }
  }
  return rotated;
}
}  // namespace detail
namespace {
constexpr unsigned kMasterSize = 512;
constexpr float kTau = 6.2831853071795864769f;

float mix(float a, float b, float t) { return a + (b - a) * t; }
float saturate(float x) { return std::clamp(x, 0.f, 1.f); }
float smooth(float a, float b, float x) {
  x = saturate((x - a) / (b - a));
  return x * x * (3.f - 2.f * x);
}
int wrap(int x, int period) { x %= period; return x < 0 ? x + period : x; }
float fract(float x) { return x - std::floor(x); }
float periodic_delta(float x) { return x - std::floor(x + .5f); }
std::uint32_t hash(std::uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du;
  x ^= x >> 15; x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
float random(int x, int y, int px, int py, std::uint32_t seed) {
  const auto key = std::uint32_t(wrap(x, px)) * 0x9e3779b9u ^
                   std::uint32_t(wrap(y, py)) * 0x85ebca6bu ^ seed;
  return float(hash(key) >> 8) * (1.f / 16777216.f);
}
// Periodic C2-interpolated value noise. All lattice periods are integers, so
// neither a color nor a height feature has a special seam at a tile boundary.
float noise(float u, float v, int px, int py, std::uint32_t seed) {
  const float x = u * float(px), y = v * float(py);
  const int ix = int(std::floor(x)), iy = int(std::floor(y));
  const auto fade = [](float t) { return t*t*t*(t*(t*6.f-15.f)+10.f); };
  const float tx = fade(fract(x)), ty = fade(fract(y));
  return mix(mix(random(ix, iy, px, py, seed), random(ix+1, iy, px, py, seed), tx),
             mix(random(ix, iy+1, px, py, seed), random(ix+1, iy+1, px, py, seed), tx), ty);
}
float cloud(float u, float v, std::uint32_t seed) {
  return .56f * noise(u,v,3,3,seed) + .28f * noise(u,v,9,9,seed+1) +
         .16f * noise(u,v,27,27,seed+2);
}
struct Cell {
  float distance{2.f}, identity{}, aspect{}, edge{};
};
// Jittered grains / air pores, rather than uncorrelated per-texel noise. Search
// a toroidal 3x3 neighbourhood so stone shapes continue across repeat edges.
Cell cell(float u, float v, int count, std::uint32_t seed) {
  const float x = u * float(count), y = v * float(count);
  const int ix = int(std::floor(x)), iy = int(std::floor(y));
  Cell result;
  float second=2.f;
  for (int dy=-1; dy<=1; ++dy) for (int dx=-1; dx<=1; ++dx) {
    const int cx=ix+dx, cy=iy+dy;
    const float ox=.18f+.64f*random(cx,cy,count,count,seed);
    const float oy=.18f+.64f*random(cx,cy,count,count,seed+1);
    const float identity=random(cx,cy,count,count,seed+2);
    const float ax=x-float(cx)-ox, ay=y-float(cy)-oy;
    const float d=ax*ax*(.8f+identity*.4f)+ay*ay*(1.2f-identity*.4f);
    if (d < result.distance) {
      second=result.distance;
      result={d,identity,random(cx,cy,count,count,seed+3),0.f};
    } else second=std::min(second,d);
  }
  result.distance=std::sqrt(result.distance);
  result.edge=std::sqrt(second)-result.distance;
  return result;
}
struct Sample {
  std::array<float,3> color;  // authored sRGB, no baked directional lighting
  float height;             // metres, used only for the normal field
  float roughness;
  float metallic;
};
Sample asphalt(float u, float v) {
  const float broad=cloud(u,v,101);
  const float fine=noise(u,v,192,192,113);
  const Cell grain=cell(u,v,100,121); // mean grain spacing 20 mm
  const float grain_mask=smooth(.025f,.18f+.17f*grain.aspect,grain.edge+(fine-.5f)*.14f);
  const float light_grain=smooth(.57f,.95f,grain.identity);
  const float mineral=grain_mask*(.020f+.11f*light_grain);
  const float binder=.37f+.045f*(broad-.5f)+.026f*(fine-.5f);
  // Very restrained weathering: connected fine fissures in isolated patches.
  const float ridge=std::abs(noise(u,v,7,7,136)-.49f);
  const float fissure=(1.f-smooth(.006f,.019f,ridge))*smooth(.62f,.8f,cloud(u,v,142));
  const float c=binder+mineral-.085f*fissure;
  return {{c*.97f,c*.99f,c},
          .0018f*grain_mask*(.35f+.65f*grain.identity)+.00032f*(fine-.5f)-.0014f*fissure,
          saturate(.94f-.12f*grain_mask*light_grain+.04f*(broad-.5f)+.05f*fissure),0.f};
}
Sample concrete(float u, float v) {
  const float broad=cloud(u,v,211), fine=noise(u,v,160,160,214);
  const Cell pores=cell(u,v,62,225);
  const float pore=(1.f-smooth(.055f,.19f,pores.distance))*smooth(.71f,.85f,pores.identity);
  const float aggregate=smooth(.53f,.83f,noise(u,v,110,110,231));
  const float trowel=std::sin(kTau*(v*9.f+.13f*noise(u,v,3,4,238)));
  const float c=.72f+.105f*(broad-.5f)+.035f*(fine-.5f)+.018f*trowel-
                .12f*pore-.017f*aggregate;
  return {{c*1.015f,c,c*.972f},
          .00052f*(fine-.5f)+.0003f*aggregate-.0019f*pore+.00012f*trowel,
          saturate(.91f+.08f*(broad-.5f)+.075f*pore-.025f*aggregate),0.f};
}
Sample brick(float u, float v) {
  // Eight 250 mm bricks by twenty-four 83 mm courses in a 2 m running-bond tile.
  const float row=v*24.f;
  const int iy=int(std::floor(row));
  const float col=u*8.f+float(wrap(iy,2))*.5f;
  const int ix=int(std::floor(col));
  const float dx=std::min(fract(col),1.f-fract(col))*.25f;
  const float dy=std::min(fract(row),1.f-fract(row))*(2.f/24.f);
  const float edge_wear=.0032f*(noise(u,v,103,103,307)-.5f);
  const float face=smooth(.004f,.010f,std::min(dx,dy)+edge_wear);
  const float id=random(ix,iy,8,24,311);
  const float warmth=.065f*(random(ix,iy,8,24,313)-.5f);
  const float grain=noise(u,v,145,145,318);
  const float worn=noise(u,v,39,39,321);
  const float chips=smooth(.69f,.87f,worn)*(1.f-smooth(.008f,.022f,std::min(dx,dy)));
  const float variation=(id-.5f)*.14f+(grain-.5f)*.06f+(worn-.5f)*.055f-.05f*chips;
  const float mortar=.61f+.045f*(grain-.5f)+.025f*(cloud(u,v,329)-.5f);
  return {{mix(mortar*1.015f,.67f+variation+warmth,face),
           mix(mortar,.43f+variation*.70f+warmth*.22f,face),
           mix(mortar*.96f,.32f+variation*.52f-warmth*.5f,face)},
          -.0045f*(1.f-face)+face*(.00075f*(grain-.5f)-.0014f*chips),
          mix(.98f,.85f+.10f*grain,face),0.f};
}
Sample wood(float u, float v) {
  // Continuous cut-wood grain. Real mesh slats/planks define the edges: no
  // synthetic board-end or lateral joints are superimposed across geometry.
  // Knot support fits within each 400 mm candidate zone, keeping its boundary
  // smooth without a per-zone color or height discontinuity.
  const float zone=u*5.f;
  const int ip=int(std::floor(zone));
  const float id=random(ip,0,5,1,410);
  const float local=fract(zone);
  const float knot_x=.35f+.30f*random(ip,0,5,1,421);
  const float knot_y=random(ip,0,5,1,422);
  const float kx=(local-knot_x)*.4f/.041f;
  const float ky=periodic_delta(v-knot_y)*2.f/.14f;
  const float kr=std::sqrt(kx*kx+ky*ky);
  const float knot=(1.f-smooth(.30f,1.8f,kr))*smooth(.53f,.68f,id);
  const float warp=1.8f*noise(u,v,5,3,429)+.55f*noise(u,v,15,7,430);
  const float rings=std::sin(kTau*(u*135.f+warp+knot*std::sin(kr*2.f)*2.f));
  const float fibers=.55f*noise(u,v,235,6,433)+.45f*noise(u,v,71,9,435);
  const float fine=noise(u,v,240,56,437);
  const float heart=(1.f-smooth(.08f,.42f,kr))*smooth(.53f,.68f,id);
  const float grain_ridge=std::pow(.5f+.5f*rings,8.f);
  const float tone=.72f+.14f*(noise(u,v,5,2,439)-.5f)+.08f*(cloud(u,v,441)-.5f)-
                   .017f*grain_ridge+.095f*(fibers-.5f)+.022f*(fine-.5f)-.10f*knot-.12f*heart;
  return {{tone,tone*.87f,tone*.70f},
          -.00012f*grain_ridge+.00045f*(fibers-.5f)-.00045f*heart,
          .84f+.09f*fibers+.05f*knot,0.f};
}
Sample metal(float u, float v) {
  // Brushed sheet, 1 m tile. Fine directional lay is geometric, not a baked
  // highlight, so it reacts to the renderer's light and view directions.
  const float brush=noise(u,v,5,230,511);
  const float rolled=noise(u,v,4,12,517);
  const float broad=cloud(u,v,523);
  const float scratch=smooth(.83f,.96f,noise(u,v,3,170,527))*smooth(.40f,.69f,broad);
  const float c=.78f+.038f*(rolled-.5f)+.038f*(brush-.5f)+.028f*(broad-.5f)+.035f*scratch;
  return {{c*.98f,c,c*1.015f},
          .000075f*(brush-.5f)+.000065f*(rolled-.5f)-.00009f*scratch,
          saturate(.74f+.18f*(brush-.5f)+.10f*(broad-.5f)+.06f*scratch),1.f};
}
Sample painted_metal(float u, float v) {
  const float broad=cloud(u,v,611), fine=noise(u,v,170,170,619);
  const float erosion=.52f*noise(u,v,15,15,623)+.30f*noise(u,v,47,47,624)+
                      .18f*noise(u,v,110,110,625);
  const float rust=smooth(.665f,.735f,erosion);
  const float chip=smooth(.735f,.80f,erosion);
  const float exposed=chip*smooth(.32f,.61f,noise(u,v,37,37,632));
  const float streak=noise(u,v,31,3,641);
  const float paint=.74f+.058f*(broad-.5f)+.025f*(fine-.5f)+.027f*(streak-.5f);
  const float oxide=.38f+.10f*fine;
  const float steel=.62f+.11f*fine;
  std::array<float,3> color{{mix(paint,oxide,rust),mix(paint*.98f,oxide*.58f,rust),
                           mix(paint*.955f,oxide*.30f,rust)}};
  for (float& c:color) c=mix(c,steel,exposed);
  return {color,.00022f*(fine-.5f)-.00065f*chip+.00030f*rust,
          mix(mix(.80f+.08f*(broad-.5f),.99f,rust),.65f,exposed),
          mix(.03f*rust,.97f,exposed)};
}
Sample sample(TextureSlot slot, float u, float v) {
  switch(slot) {
    case TextureSlot::Asphalt: return asphalt(u,v);
    case TextureSlot::Concrete: return concrete(u,v);
    case TextureSlot::Brick: return brick(u,v);
    case TextureSlot::Wood: return wood(u,v);
    case TextureSlot::Metal: return metal(u,v);
    case TextureSlot::BarrelMetal: return painted_metal(u,v);
    default: return {{{1.f,1.f,1.f}},0.f,1.f,0.f};
  }
}
const char* profile_name(TextureSlot slot) {
  switch(slot) {
    case TextureSlot::Asphalt: return "asphalt";
    case TextureSlot::Concrete: return "concrete";
    case TextureSlot::Brick: return "running-bond-brick";
    case TextureSlot::Wood: return "weathered-wood";
    case TextureSlot::Metal: return "brushed-steel";
    case TextureSlot::BarrelMetal: return "painted-steel";
    default: return "unsupported";
  }
}
std::uint8_t byte(float x) {
  return static_cast<std::uint8_t>(saturate(x)*255.f+.5f);
}
RgbaImage allocate(unsigned resolution) {
  return {int(resolution),int(resolution),std::vector<std::uint8_t>(std::size_t(resolution)*resolution*4,255)};
}
std::shared_ptr<const MaterialTextures> generate(TextureSlot slot, unsigned resolution, unsigned quarter_turns) {
  auto textures=std::make_shared<MaterialTextures>();
  textures->base_color=allocate(kMasterSize);
  textures->normal=allocate(kMasterSize);
  textures->metallic_roughness=allocate(kMasterSize);
  textures->source="fury:original-surface-detail/v1/"+std::string(profile_name(slot))+"/"+
                   std::to_string(resolution)+";rotation="+std::to_string(quarter_turns)+";license=MIT";
  std::vector<float> heights(std::size_t(kMasterSize)*kMasterSize);
  for(unsigned y=0;y<kMasterSize;++y) for(unsigned x=0;x<kMasterSize;++x) {
    const auto pixel=std::size_t(y)*kMasterSize+x, offset=pixel*4;
    const auto s=sample(slot,(float(x)+.5f)/float(kMasterSize),(float(y)+.5f)/float(kMasterSize));
    for(unsigned c=0;c<3;++c) textures->base_color.pixels[offset+c]=byte(s.color[c]);
    textures->metallic_roughness.pixels[offset+1]=byte(s.roughness);
    textures->metallic_roughness.pixels[offset+2]=byte(s.metallic);
    heights[pixel]=s.height;
  }
  const float derivative_scale=float(kMasterSize)*surface_detail_uv_per_meter(slot)*.5f;
  const auto h=[&](int x,int y) { return heights[std::size_t(wrap(y,int(kMasterSize)))*kMasterSize+wrap(x,int(kMasterSize))]; };
  for(unsigned y=0;y<kMasterSize;++y) for(unsigned x=0;x<kMasterSize;++x) {
    const float nx=(h(int(x)-1,int(y))-h(int(x)+1,int(y)))*derivative_scale;
    const float ny=(h(int(x),int(y)-1)-h(int(x),int(y)+1))*derivative_scale;
    const float inverse_length=1.f/std::sqrt(nx*nx+ny*ny+1.f);
    const auto offset=(std::size_t(y)*kMasterSize+x)*4;
    textures->normal.pixels[offset]=byte(nx*inverse_length*.5f+.5f);
    textures->normal.pixels[offset+1]=byte(ny*inverse_length*.5f+.5f);
    textures->normal.pixels[offset+2]=byte(inverse_length*.5f+.5f);
  }
  // Smaller settings and all runtime mips derive from the same physical master.
  // Additional-level index 0 is half-size; do not retain a duplicate level zero.
  const auto finish=[&](RgbaImage& image, std::vector<RgbaImage>& mips, TextureEncoding encoding) {
    auto chain=build_mip_chain(std::move(image),encoding);
    const std::size_t level=resolution==512?0:(resolution==256?1:2);
    image=std::move(chain[level]);
    mips.reserve(chain.size()-level-1);
    for(std::size_t i=level+1;i<chain.size();++i) mips.push_back(std::move(chain[i]));
  };
  finish(textures->base_color,textures->base_color_mips,TextureEncoding::SRGB);
  finish(textures->normal,textures->normal_mips,TextureEncoding::Normal);
  finish(textures->metallic_roughness,textures->metallic_roughness_mips,TextureEncoding::Linear);
  // Rotate existing levels rather than re-filtering quantized rotated normals;
  // every quality setting and mip stays the exact same physical surface.
  const auto rotate=[&](RgbaImage& image, std::vector<RgbaImage>& mips, bool normal) {
    for(unsigned turn=0;turn<quarter_turns;++turn) {
      image=detail::rotate_surface_detail_quarter_turn(image,normal);
      for(auto& mip:mips) mip=detail::rotate_surface_detail_quarter_turn(mip,normal);
    }
  };
  rotate(textures->base_color,textures->base_color_mips,false);
  rotate(textures->normal,textures->normal_mips,true);
  rotate(textures->metallic_roughness,textures->metallic_roughness_mips,false);
  return textures;
}
struct Cache {
  std::mutex mutex;
  unsigned resolution{}, quarter_turns{};
  std::array<std::shared_ptr<const MaterialTextures>,std::size_t(TextureSlot::Count)> profiles{};
};
}  // namespace

bool supports_surface_detail(TextureSlot slot) {
  switch(slot) {
    case TextureSlot::Asphalt: case TextureSlot::Concrete: case TextureSlot::Brick:
    case TextureSlot::Wood: case TextureSlot::Metal: case TextureSlot::BarrelMetal:
      return true;
    default: return false;
  }
}
float surface_detail_uv_per_meter(TextureSlot slot) {
  return !supports_surface_detail(slot)?0.f:(slot==TextureSlot::Metal?1.f:.5f);
}
std::shared_ptr<const MaterialTextures> surface_detail_textures(TextureSlot slot, unsigned resolution, unsigned quarter_turns) {
  if(resolution!=128 && resolution!=256 && resolution!=512)
    throw std::invalid_argument("Surface detail resolution must be 128, 256, or 512");
  if(quarter_turns>3) throw std::invalid_argument("Surface detail quarter-turn rotation must be 0, 1, 2, or 3");
  if(!supports_surface_detail(slot)) return nullptr;
  static Cache cache;
  std::lock_guard<std::mutex> lock(cache.mutex);
  if(cache.resolution!=resolution || cache.quarter_turns!=quarter_turns) {
    cache.profiles.fill(nullptr);
    cache.resolution=resolution;
    cache.quarter_turns=quarter_turns;
  }
  auto& result=cache.profiles[std::size_t(slot)];
  if(!result) result=generate(slot,resolution,quarter_turns);
  return result;
}

}  // namespace fury
