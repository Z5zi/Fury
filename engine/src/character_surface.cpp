#include "fury/character_surface.hpp"
#include "fury/player_hair.hpp"
#include "fury/generated/mira_anatomy.hpp"
#include "fury/generated/player_body_hair.hpp"
#include "fury/generated/player_body_hair_detail.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fury {
namespace {
constexpr float tau = 6.2831853071795864769f;
constexpr std::array<CharacterSurfaceRegion, 16> regions{{
  {0,0,1024,64}, {1024,0,512},
  {1024,512,256}, {1280,512,256}, {1024,768,256}, {1280,768,256},
  {0,1024,256}, {256,1024,256}, {512,1024,256}, {768,1024,256},
  {1024,1024,256}, {1280,1024,256}, {0,1280,256},
  {256,1280,256}, {512,1280,256}, {768,1280,256}
}};
static_assert(regions.size() == static_cast<unsigned>(CharacterSurface::Count),
              "Every character surface needs a stable atlas rectangle");

struct FaceChart {
  Vec2 source_min,source_max;
  Vec2 origin; // Reference 448px interior, measured between texel centers.
  float scale;
};
// Bounds are the union of the pinned Mira/Jon source selections. Their five
// rectangles are disjoint; every source triangle lies wholly within one. Keep
// the canonical generated UV streams and their source indices unchanged.
constexpr std::array<FaceChart,5> face_charts{{
  {{.641249001f,.148696005f},{.993515015f,.817633986f},{8,8},1.44f},
  {{.757633984f,.0118629998f},{.923740029f,.136439994f},{288,8},1.f},
  {{.215131998f,.840623021f},{.545929015f,.973855972f},{288,112},1.f},
  {{.0118089998f,.547390997f},{.0768219978f,.632857978f},{288,240},1.f},
  {{.0861430019f,.547390997f},{.151155993f,.632857978f},{288,368},1.f}
}};
int source_face_chart(Vec2 uv) {
  constexpr float epsilon=1e-7f;
  for(unsigned i=0;i<face_charts.size();++i) {
    const auto& chart=face_charts[i];
    if(uv.x>=chart.source_min.x-epsilon && uv.x<=chart.source_max.x+epsilon &&
       uv.y>=chart.source_min.y-epsilon && uv.y<=chart.source_max.y+epsilon)
      return int(i);
  }
  return -1;
}
Vec2 packed_face_uv(Vec2 uv,int chart) {
  if(chart<0) return uv;
  const auto& c=face_charts[std::size_t(chart)];
  return {(uv.x-c.source_min.x)*c.scale+c.origin.x/447.f,
          (uv.y-c.source_min.y)*c.scale+c.origin.y/447.f};
}
Vec2 source_face_uv(Vec2 uv,int chart) {
  const auto& c=face_charts[std::size_t(chart)];
  return {(uv.x-c.origin.x/447.f)/c.scale+c.source_min.x,
          (uv.y-c.origin.y/447.f)/c.scale+c.source_min.y};
}
// Internal cells share the material atlas's 64px mip grid. At level six the
// head occupies nine columns; each other chart owns seven columns/four rows.
// Every used UV has at least half a final-level texel of padding in its cell.
struct FaceCell { int x0,y0,x1,y1; };
FaceCell face_cell(int chart) {
  constexpr int gutter=64,interior=896;
  if(chart==0) return {0,0,576-gutter,interior};
  return {576-gutter,std::max(0,(chart-1)*256-gutter),interior,
          std::min(interior,chart*256-gutter)};
}
int pixel_face_chart(int x,int y) {
  return x<512?0:1+(y+64)/256;
}

float clamp01(float value) { return std::clamp(value,0.f,1.f); }
float lerp(float a,float b,float t) { return a+(b-a)*t; }
float smooth(float x) { return x*x*x*(x*(x*6.f-15.f)+10.f); }
int wrap(int x,int period) { return (x%period+period)%period; }
std::uint32_t hash(std::uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
  x *= 0x846ca68bu; return x^(x >> 16);
}
float lattice(int x,int y,int nx,int ny,std::uint32_t seed) {
  const auto key=std::uint32_t(wrap(x,nx))*0x9e3779b9u ^
                 std::uint32_t(wrap(y,ny))*0x85ebca6bu ^ seed;
  return float(hash(key)&0xffffu)/65535.f;
}
// Periodic smooth fields contribute low-amplitude material irregularity, never
// a high-contrast noise overlay or a directional illumination term.
float noise(float u,float v,int nx,int ny,std::uint32_t seed) {
  const float x=u*float(nx),y=v*float(ny);
  const int ix=int(std::floor(x)),iy=int(std::floor(y));
  const float a=smooth(x-float(ix)),b=smooth(y-float(iy));
  return lerp(lerp(lattice(ix,iy,nx,ny,seed),lattice(ix+1,iy,nx,ny,seed),a),
              lerp(lattice(ix,iy+1,nx,ny,seed),lattice(ix+1,iy+1,nx,ny,seed),a),b)-.5f;
}
struct SurfaceSample {
  Vec3 linear_color{.96f,.96f,.96f};
  float height{0.f}; // metres, used only to derive restrained tangent normals
  float roughness{.7f};
  float metallic{0.f};
  float alpha{1.f};
};
SurfaceSample hair_crop(float u,float v) {
  // Original dense short strokes, staggered in independently varied cells.
  // Their tapered ends interrupt long bands; no alpha gaps expose the scalp.
  // U is periodic, including jitter and bend, for the opaque wrapping cap.
  constexpr int columns=57,rows=13;
  const float x=u*columns,y=v*rows;
  const int ix=int(std::floor(x)),iy=int(std::floor(y));
  float fiber=0.f;
  for(int row=iy-1;row<=iy+1;++row) for(int column=ix-1;column<=ix+1;++column) {
    const float jitter=lattice(column,row,columns,rows,0x37a9u)-.5f;
    const float variation=lattice(column,row,columns,rows,0x8d13u);
    const float along=(y-float(row)-.5f)/(.42f+.13f*variation);
    if(std::abs(along)>=1.f) continue;
    const float stagger=.5f*float(wrap(row,rows)%2);
    const float across=x-float(column)-.5f-stagger-.22f*jitter-
        (.16f+.12f*jitter)*along-.10f*jitter*(1.f-along*along);
    const float width=.28f+.08f*variation;
    const float cross_section=smooth(clamp01(1.f-std::abs(across)/width));
    const float taper=smooth(clamp01((1.f-std::abs(along))/.35f));
    fiber=std::max(fiber,cross_section*taper);
  }
  const float grain=noise(u,v,43,37,0x54b7u);
  const float value=.928f+.032f*fiber+.012f*grain;
  // Very shallow relief and a matte dielectric lobe reduce the plastic cap
  // response. This uses the shared isotropic shaders, with no baked lighting.
  return {{value,value,value},.000014f*fiber+.000002f*grain,
          .648f-.027f*fiber+.016f*grain,0.f};
}
SurfaceSample hair_strands(float u,float v) {
  // Original longitudinal filaments: each has one continuous centerline and
  // one connected root-to-tip support. Variation changes curvature, thickness
  // and tip length; it never punches random holes through the strand.
  float alpha=0.f;
  for(int strand=0;strand<18;++strand) {
    const float id=float(strand);
    const float root=.025f+.0125f*(1.f+std::sin(id*.63f));
    const float tip=.82f+.065f*(1.f+std::sin(id*1.9f));
    if(v<=root || v>=tip) continue;
    const float t=(v-root)/(tip-root);
    const float bend=std::sin(tau*.5f*t);
    const float center=.075f+(id+.5f)*(.85f/18.f)+
        .022f*bend*(.6f+.4f*std::sin(id*.83f))+
        .006f*std::sin(tau*t+id*.7f)*bend;
    const float width=.019f*(.88f+.12f*std::sin(id*1.7f))*(1.f-.26f*t)*
        smooth(clamp01(t/.055f))*smooth(clamp01((1.f-t)/.18f));
    const float coverage=smooth(clamp01((width-std::abs(u-center))/.0038f+.5f))*
        smooth(clamp01(width/.0038f));
    alpha=std::max(alpha,coverage);
  }
  // Reflectance and shallow strand relief carry no painted lighting. Their
  // values continue under transparent margins, avoiding dark filtered halos.
  const float bend=.12f*std::sin(tau*.7f*v);
  const float fiber=std::sin(tau*(18.f*u-bend));
  const float fine=std::sin(tau*(53.f*u-.08f*std::sin(tau*v)));
  const float grain=noise(u,v,41,4,0x6725u);
  const float value=.939f+.015f*fiber+.006f*fine+.009f*grain;
  return {{value,value,value},.000028f*fiber+.000006f*fine,
          .39f+.011f*fiber+.012f*grain,0.f,alpha};
}
SurfaceSample hair_short_strands(float u,float v) {
  // Keep connected, tapered root-to-tip coverage, but fit the response to
  // 4-6mm wide, 20-45mm long boundary cards around a matte short-crop cap.
  // Seven-micrometre relief avoids the steep normals of rescaled long cards.
  // This remains an isotropic dielectric; all highlights come from lighting.
  auto result=hair_strands(u,v);
  const float bend=.12f*std::sin(tau*.7f*v);
  const float fiber=std::sin(tau*(18.f*u-bend));
  const float fine=std::sin(tau*(53.f*u-.08f*std::sin(tau*v)));
  const float grain=noise(u,v,41,4,0x6725u);
  const float value=.936f+.012f*fiber+.004f*fine+.007f*grain;
  result.linear_color={value,value,value};
  result.height=.000007f*fiber+.0000005f*fine;
  result.roughness=.65f+.011f*fiber+.012f*grain;
  return result;
}
SurfaceSample skin(float u,float v) {
  const float broad=noise(u,v,7,9,0x147bu);
  const float fine=noise(u,v,61,67,0x7539u);
  SurfaceSample s;
  const float value=.971f+.016f*broad+.006f*fine;
  s.linear_color={value,value-.006f,value-.009f};
  s.height=.000007f*fine+.000002f*noise(u,v,113,109,0x312bu);
  s.roughness=.458f+.033f*broad+.026f*fine;
  return s;
}
struct FaceTexel { Vec3 position; bool mapped{false}; };
std::vector<FaceTexel> face_positions(int size) {
  // Interpolate geometry through the same per-chart affine map used by mesh
  // vertices. Its input is the unmodified source per-corner UV stream.
  std::vector<FaceTexel> lookup(std::size_t(size)*size);
  const auto& positions=mira_anatomy::positions;
  const auto& uvs=mira_anatomy::uv_coordinates;
  const auto& indices=mira_anatomy::indices;
  const auto& corners=mira_anatomy::uv_indices;
  auto edge=[](Vec2 a,Vec2 b,Vec2 p) { return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x); };
  for(std::size_t i=0;i<indices.size();i+=3) {
    const int chart=source_face_chart(uvs[corners[i]]);
    if(chart<0 || source_face_chart(uvs[corners[i+1]])!=chart ||
       source_face_chart(uvs[corners[i+2]])!=chart)
      throw std::logic_error("Anatomical triangle crosses unsupported source UV charts");
    const Vec2 a=packed_face_uv(uvs[corners[i]],chart),
               b=packed_face_uv(uvs[corners[i+1]],chart),
               c=packed_face_uv(uvs[corners[i+2]],chart);
    const float area=edge(a,b,c);
    if(std::abs(area)<1e-12f) continue;
    const int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})*(size-1))));
    const int x1=std::min(size-1,int(std::ceil(std::max({a.x,b.x,c.x})*(size-1))));
    const int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})*(size-1))));
    const int y1=std::min(size-1,int(std::ceil(std::max({a.y,b.y,c.y})*(size-1))));
    for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
      const Vec2 p{float(x)/float(size-1),float(y)/float(size-1)};
      const float wa=edge(b,c,p)/area,wb=edge(c,a,p)/area,wc=1.f-wa-wb;
      if(wa<-.00001f || wb<-.00001f || wc<-.00001f) continue;
      if(pixel_face_chart(x,y)!=chart)
        throw std::logic_error("Anatomical UV escaped its isolated face cell");
      auto& texel=lookup[std::size_t(y)*size+x];
      texel.position=positions[indices[i]]*wa+positions[indices[i+1]]*wb+positions[indices[i+2]]*wc;
      texel.mapped=true;
    }
  }
  // Extend geometry-space pigment throughout each chart's own mip-aligned
  // cell. No lookup can cross a cell boundary, including at coarse mip levels.
  // Height fields are evaluated separately through the inverse affine map,
  // so this padding never creates a false normal-map ridge at a chart edge.
  std::vector<int> queue; queue.reserve(lookup.size());
  for(std::size_t i=0;i<lookup.size();++i) if(lookup[i].mapped) {
    queue.push_back(int(i));
  }
  for(std::size_t head=0;head<queue.size();++head) {
    const int current=queue[head],x=current%size,y=current/size;
    const auto cell=face_cell(pixel_face_chart(x,y));
    const std::array<int,4> next{{x>0?current-1:-1,x+1<size?current+1:-1,
                                y>0?current-size:-1,y+1<size?current+size:-1}};
    for(int target:next) if(target>=0 && !lookup[std::size_t(target)].mapped &&
        target%size>=cell.x0 && target%size<cell.x1 &&
        target/size>=cell.y0 && target/size<cell.y1) {
      lookup[std::size_t(target)]=lookup[std::size_t(current)];
      queue.push_back(target);
    }
  }
  return lookup;
}
void face_pigment(SurfaceSample& sample,Vec3 p) {
  // Geometry-space masks stay correctly placed across rotated/separate charts.
  // Z gating prevents the back of the head from inheriting facial pigment.
  const float front=smooth(clamp01((p.z-.045f)/.030f));
  auto spot=[&](float x,float y,float rx,float ry) {
    const float dx=(p.x-x)/rx,dy=(p.y-y)/ry;
    return front*std::exp(-.5f*(dx*dx+dy*dy));
  };
  const float cheeks=spot(-.026f,.412f,.010f,.010f)+spot(.026f,.412f,.010f,.010f);
  const float lips=spot(0,.391f,.012f,.0040f);
  const float nose=spot(0,.408f,.0050f,.0070f);
  const float sockets=spot(-.01766f,.42767f,.008f,.004f)+spot(.01766f,.42767f,.008f,.004f);
  sample.linear_color.x*=1.f-.055f*lips-.009f*sockets;
  sample.linear_color.y*=1.f-.037f*cheeks-.15f*lips-.018f*nose-.012f*sockets;
  sample.linear_color.z*=1.f-.024f*cheeks-.12f*lips-.012f*nose-.012f*sockets;
  sample.roughness-=.048f*nose+.024f*spot(0,.454f,.018f,.011f)+.029f*lips;
}
SurfaceSample cloth(float u,float v,CharacterSurface kind) {
  const bool jacket=kind==CharacterSurface::Jacket;
  const bool shirt=kind==CharacterSurface::Shirt;
  const float count=jacket?86.f:(shirt?41.f:45.f);
  const float irregular=noise(u,v,13,17,jacket?0xa92bu:0xf31du);
  const float warp=std::sin(tau*(u*count+.14f*std::sin(tau*v*5.f)));
  const float weft=std::sin(tau*(v*(count+5.f)+.11f*std::sin(tau*u*4.f)));
  const float twill=std::sin(tau*(u*(jacket?51.f:31.f)+v*(jacket?47.f:29.f)));
  const float fiber=noise(u,v,jacket?173:89,jacket?149:83,0x286fu);
  const float yarn=.55f*warp+.35f*weft+.10f*twill;
  const float value=.943f+.027f*irregular+.010f*yarn+.005f*fiber;
  SurfaceSample s;
  s.linear_color={value,value,value};
  s.height=(jacket?.000075f:.000055f)*yarn+.000016f*fiber;
  s.roughness=(jacket?.79f:(shirt?.86f:.82f))+.040f*irregular+.015f*yarn;
  return s;
}
SurfaceSample sample(CharacterSurface surface,float u,float v) {
  switch(surface) {
    case CharacterSurface::Face:
    case CharacterSurface::Skin: return skin(u,v);
    case CharacterSurface::Jacket:
    case CharacterSurface::Shirt:
    case CharacterSurface::Pants: return cloth(u,v,surface);
    case CharacterSurface::Hair: {
      // Shallow, gently curved strand bundles. Highlights come from the renderer.
      const float bend=.18f*std::sin(tau*v)+.07f*std::sin(tau*(3.f*v+u));
      const float strand=std::sin(tau*(29.f*u+bend));
      const float fine=std::sin(tau*(67.f*u+.16f*std::sin(tau*v*2.f)));
      const float grain=noise(u,v,37,7,0x1be3u);
      const float value=.924f+.037f*strand+.009f*fine+.020f*grain;
      return {{value,value,value},.000065f*strand+.000012f*fine,
              .39f+.038f*grain+.013f*strand,0.f};
    }
    case CharacterSurface::HairStrands: return hair_strands(u,v);
    case CharacterSurface::HairCrop: return hair_crop(u,v);
    case CharacterSurface::HairShortStrands: return hair_short_strands(u,v);
    case CharacterSurface::Leather: {
      const float grain=noise(u,v,43,47,0xcb17u);
      const float broad=noise(u,v,11,13,0xa045u);
      const float crease=std::pow(std::max(0.f,-grain-.12f),2.f);
      const float value=.946f+.023f*broad+.022f*grain-.030f*crease;
      return {{value,value,value},.000090f*grain-.000060f*crease,
              .552f+.055f*grain+.018f*broad,0.f};
    }
    case CharacterSurface::Eyes: {
      // Geometry/vertex tint distinguish sclera, iris and pupil. No painted
      // catchlight or built-in pupil that could fight their authored placement.
      const float edge=clamp01((std::hypot(u-.5f,v-.5f)-.24f)*1.6f);
      return {{.995f,.991f-.015f*edge,.983f-.011f*edge},0.f,.165f+.026f*edge,0.f};
    }
    case CharacterSurface::Rubber: {
      const float grain=noise(u,v,53,59,0xd857u);
      const float value=.953f+.021f*grain;
      return {{value,value,value},.000021f*grain,.88f+.042f*grain,0.f};
    }
    case CharacterSurface::Metal: {
      const float grain=noise(u,v,5,67,0x1357u);
      const float brushing=std::sin(tau*(61.f*v+.05f*std::sin(tau*u*2.f)));
      const float value=.976f+.009f*grain+.004f*brushing;
      return {{value,value,value},.0000009f*brushing,.29f+.045f*grain+.008f*brushing,.94f};
    }
    default: throw std::out_of_range("Invalid character surface");
  }
}
Vec2 physical_span(CharacterSurface surface) {
  switch(surface) {
    case CharacterSurface::Face: return {.38f,.24f};
    case CharacterSurface::Jacket: return {.75f,.70f};
    case CharacterSurface::Hair: return {.36f,.22f};
    case CharacterSurface::HairStrands: return {.08f,.20f};
    case CharacterSurface::HairShortStrands: return {.006f,.045f};
    // A crop tile covers one wrapping cap sector, not the full circumference.
    case CharacterSurface::HairCrop: return {.08f,.16f};
    case CharacterSurface::Skin: return {.18f,.27f};
    case CharacterSurface::Shirt: return {.70f,.60f};
    case CharacterSurface::Pants: return {.30f,.75f};
    case CharacterSurface::Leather: return {.22f,.28f};
    case CharacterSurface::Eyes: return {.026f,.016f};
    case CharacterSurface::Rubber: return {.16f,.30f};
    case CharacterSurface::Metal: return {.045f,.045f};
    default: throw std::out_of_range("Invalid character surface");
  }
}
std::uint8_t byte(float value) { return std::uint8_t(clamp01(value)*255.f+.5f); }
float srgb(float linear) {
  linear=clamp01(linear);
  return linear<=.0031308f?linear*12.92f:1.055f*std::pow(linear,1.f/2.4f)-.055f;
}
RgbaImage allocate(std::array<std::uint8_t,4> background) {
  constexpr auto size=std::size_t(character_surface_atlas_size);
  RgbaImage result{character_surface_atlas_size,character_surface_atlas_size,
                   std::vector<std::uint8_t>(size*size*4)};
  for(std::size_t i=0;i<result.pixels.size();i+=4)
    std::copy(background.begin(),background.end(),result.pixels.begin()+i);
  return result;
}
std::size_t offset(int x,int y) {
  return (std::size_t(y)*character_surface_atlas_size+std::size_t(x))*4;
}
void extrude(RgbaImage& image,const CharacterSurfaceRegion& region,bool periodic_u) {
  const int lo=region.gutter,hi=region.size-region.gutter-1;
  for(int y=0;y<region.size;++y) for(int x=0;x<region.size;++x) {
    if(x>=lo && x<=hi && y>=lo && y<=hi) continue;
    // U repeats around lofts, so its gutter continues the opposite edge instead
    // of duplicating a constant stripe. V ends (chin/crown, cuffs/hems) clamp.
    // Preserve the explicit duplicate U=1 endpoint when only V needs padding.
    const int sx=!periodic_u?std::clamp(x,lo,hi):((x>=lo && x<=hi)?x:lo+wrap(x-lo,hi-lo));
    const auto source=offset(region.x+sx,region.y+std::clamp(y,lo,hi));
    const auto target=offset(region.x+x,region.y+y);
    std::copy_n(image.pixels.data()+source,4,image.pixels.data()+target);
  }
}
void make_region(MaterialTextures& textures,CharacterSurface surface) {
  const auto& region=character_surface_region(surface);
  const int n=region.size-2*region.gutter;
  const bool face=surface==CharacterSurface::Face;
  const bool periodic_u=!face && surface!=CharacterSurface::HairStrands &&
                        surface!=CharacterSurface::HairShortStrands;
  const auto anatomy=face?face_positions(n):std::vector<FaceTexel>{};
  std::vector<float> heights(std::size_t(n)*n);
  for(int y=0;y<n;++y) for(int x=0;x<n;++x) {
    // Duplicate periodic U endpoints exactly, including their quantized normal.
    // Skin, cloth and hair fields repeat; the anatomical Face chart clamps.
    const float u=x==n-1 && periodic_u?0.f:float(x)/float(n-1),v=float(y)/float(n-1);
    // Skin grain remains in canonical source UVs, preserving its anatomical
    // frequency when the atlas gains pixels. Positive uniform chart scaling
    // also preserves the authored tangent directions and handedness.
    const Vec2 field_uv=face?source_face_uv({u,v},pixel_face_chart(x,y)):Vec2{u,v};
    auto s=sample(surface,field_uv.x,field_uv.y);
    if(face && anatomy[std::size_t(y)*n+x].mapped)
      face_pigment(s,anatomy[std::size_t(y)*n+x].position);
    heights[std::size_t(y)*n+x]=s.height;
    const auto out=offset(region.x+region.gutter+x,region.y+region.gutter+y);
    auto* color=textures.base_color.pixels.data()+out;
    color[0]=byte(srgb(s.linear_color.x));
    color[1]=byte(srgb(s.linear_color.y));
    color[2]=byte(srgb(s.linear_color.z));
    color[3]=byte(s.alpha);
    auto* orm=textures.metallic_roughness.pixels.data()+out;
    orm[0]=255; orm[1]=byte(s.roughness); orm[2]=byte(s.metallic);
  }
  const Vec2 span=physical_span(surface);
  auto h=[&](int x,int y) {
    return heights[std::size_t(std::clamp(y,0,n-1))*n+(periodic_u?wrap(x,n-1):std::clamp(x,0,n-1))];
  };
  for(int y=0;y<n;++y) for(int x=0;x<n;++x) {
    float dx{},dy{};
    if(face) {
      const int chart=pixel_face_chart(x,y);
      const auto cell=face_cell(chart);
      const int left=std::max(x-1,cell.x0),right=std::min(x+1,cell.x1-1),
                top=std::max(y-1,cell.y0),bottom=std::min(y+1,cell.y1-1);
      const float source_derivative_scale=float(n-1)*face_charts[std::size_t(chart)].scale;
      dx=(h(right,y)-h(left,y))*source_derivative_scale/(float(right-left)*span.x);
      dy=(h(x,bottom)-h(x,top))*source_derivative_scale/(float(bottom-top)*span.y);
    } else {
      // These operations intentionally match the original nine material tiles
      // exactly; only their mip-aligned atlas origins change.
      const float x_divisor=(!periodic_u && (x==0 || x==n-1))?span.x:2.f*span.x;
      dx=(h(x+1,y)-h(x-1,y))*float(n-1)/x_divisor;
      const float divisor=(y==0 || y==n-1)?span.y:2.f*span.y;
      dy=(h(x,y+1)-h(x,y-1))*float(n-1)/divisor;
    }
    const float inv=1.f/std::sqrt(dx*dx+dy*dy+1.f);
    auto* normal=textures.normal.pixels.data()+
      offset(region.x+region.gutter+x,region.y+region.gutter+y);
    normal[0]=byte(.5f-.5f*dx*inv);
    normal[1]=byte(.5f-.5f*dy*inv);
    normal[2]=byte(.5f+.5f*inv);
  }
  extrude(textures.base_color,region,periodic_u);
  extrude(textures.normal,region,periodic_u);
  extrude(textures.metallic_roughness,region,periodic_u);
}
void finish(RgbaImage& image,std::vector<RgbaImage>& mips,TextureEncoding encoding) {
  auto levels=build_mip_chain(std::move(image),encoding);
  image=std::move(levels.front());
  mips.reserve(character_surface_max_mip);
  for(int level=1;level<=character_surface_max_mip;++level)
    mips.push_back(std::move(levels[std::size_t(level)]));
}
void preserve_gathered_coverage(MaterialTextures& textures,CharacterSurface surface) {
  const auto& region=character_surface_region(surface);
  auto coverage=[&](const RgbaImage& map,int level,float scale) {
    int covered=0,total=0;
    for(int y=region.y>>level;y<(region.y+region.size)>>level;++y)
      for(int x=region.x>>level;x<(region.x+region.size)>>level;++x) {
        covered+=map.pixels[(std::size_t(y)*map.width+x)*4+3]*scale>=102.f;
        ++total;
      }
    return float(covered)/float(total);
  };
  const float target=coverage(textures.base_color,0,1);
  for(unsigned i=0;i<textures.base_color_mips.size();++i) {
    auto& map=textures.base_color_mips[i]; const int level=int(i)+1;
    float lo=0,hi=8;
    for(int step=0;step<18;++step) {
      const float mid=(lo+hi)*.5f;
      if(coverage(map,level,mid)<target)lo=mid;else hi=mid;
    }
    // Choose the closer discrete coverage at coarse levels.
    const float scale=std::abs(coverage(map,level,lo)-target)<std::abs(coverage(map,level,hi)-target)?lo:hi;
    for(int y=region.y>>level;y<(region.y+region.size)>>level;++y)
      for(int x=region.x>>level;x<(region.x+region.size)>>level;++x) {
        auto& a=map.pixels[(std::size_t(y)*map.width+x)*4+3];
        a=std::uint8_t(std::clamp(float(a)*scale,0.f,255.f)+.5f);
      }
  }
}
template<std::size_t N>
void source_hair_field(MaterialTextures& textures,CharacterSurface surface,const unsigned char (&png)[N],float roughness) {
  RgbaImage decoded;
  if(!decode_rgba_image(png,N,decoded)||decoded.width!=192||decoded.height!=192)
    throw std::runtime_error("Invalid embedded source-groom field");
  const auto&r=character_surface_region(surface);
  for(int y=0;y<192;++y)for(int x=0;x<192;++x){
    const auto source=(std::size_t(y)*192+x)*4,target=offset(r.x+r.gutter+x,r.y+r.gutter+y);
    std::copy_n(decoded.pixels.data()+source,4,textures.base_color.pixels.data()+target);
    auto*n=textures.normal.pixels.data()+target;n[0]=128;n[1]=128;n[2]=255;n[3]=255;
    auto*m=textures.metallic_roughness.pixels.data()+target;m[0]=255;m[1]=byte(roughness);m[2]=0;m[3]=255;
  }
  extrude(textures.base_color,r,false);extrude(textures.normal,r,false);extrude(textures.metallic_roughness,r,false);
}
std::shared_ptr<const MaterialTextures> generate() {
  auto textures=std::make_shared<MaterialTextures>();
  textures->source="fury:original-character-surfaces/v6;face=source-uv-uniform-repack-1.44;hair=longitudinal-cutout;hair-crop=opaque-short-fibers;hair-short-strands=matte-short-cutout;atlas=1536;face-tile=1024;gutter=64/32;max-lod=6;license=MIT";
  textures->base_color=allocate({255,255,255,255});
  textures->normal=allocate({128,128,255,255});
  textures->metallic_roughness=allocate({255,255,0,255});
  // Keep every baseline atlas byte, including unused cells, unchanged when off.
  for(unsigned i=0;i<unsigned(CharacterSurface::HairSwept);++i)
    make_region(*textures,CharacterSurface(i));
  if(player_source_hair_enabled()) {
    source_hair_field(*textures,CharacterSurface::HairSwept,player_body_hair::left_png,.60f);
    source_hair_field(*textures,CharacterSurface::HairBundle,player_body_hair::right_png,.60f);
    source_hair_field(*textures,CharacterSurface::HairSourceFold,player_body_hair::fold_png,.57f);
    textures->source+=player_short_tail_enabled()?";player-source-groom=CC0-ponytail01-fitted-scalp-short-tied-tail;pose-dependent-bend;source-tint-neutralized":";player-source-groom=CC0-ponytail01-fitted-scalp-original-fold;source-tint-neutralized";
  }
  finish(textures->base_color,textures->base_color_mips,TextureEncoding::SRGB);
  finish(textures->normal,textures->normal_mips,TextureEncoding::Normal);
  finish(textures->metallic_roughness,textures->metallic_roughness_mips,TextureEncoding::Linear);
  if(player_source_hair_enabled()) {
    preserve_gathered_coverage(*textures,CharacterSurface::HairBundle);
    preserve_gathered_coverage(*textures,CharacterSurface::HairSwept);
  }
  return textures;
}
}  // namespace

bool player_source_hair_enabled() { return player_hair_configuration().source; }
bool player_short_tail_enabled() { return player_hair_configuration().short_tail; }
std::shared_ptr<const MaterialColorDetail> make_player_hair_color_detail() {
  static const int resolution=player_hair_configuration().detail_resolution;
  if(!resolution)return {};
  if(!player_source_hair_enabled())throw std::invalid_argument("Player hair detail requires FURY_PLAYER_SOURCE_HAIR=1");
  static const std::shared_ptr<const MaterialColorDetail> result=[] {
    auto out=std::make_shared<MaterialColorDetail>();
    const auto* data=resolution==512?player_body_hair_detail::png_512:player_body_hair_detail::png_1024;
    const auto bytes=resolution==512?sizeof(player_body_hair_detail::png_512):sizeof(player_body_hair_detail::png_1024);
    RgbaImage image;
    if(!decode_rgba_image(data,bytes,image)||image.width!=resolution||image.height!=resolution)
      throw std::runtime_error("Invalid embedded Player hair detail");
    auto chain=build_mip_chain(std::move(image),TextureEncoding::SRGB);
    out->color=std::move(chain.front());
    for(std::size_t i=1;i<chain.size();++i)out->color_mips.push_back(std::move(chain[i]));
    out->region_count=2;
    for(unsigned i=0;i<2;++i){
      auto& region=out->regions[i];
      const auto surface=i?CharacterSurface::HairBundle:CharacterSurface::HairSwept;
      region.source_min=character_surface_uv(surface,0,0);
      region.source_max=character_surface_uv(surface,1,1);
      // Mesh endpoints are content texel centers; include the upper endpoint
      // within the helper's half-open window without crossing its atlas gutter.
      region.source_max.x+=1e-7f;region.source_max.y+=1e-7f;
      region.detail_min={i*.5f,0};region.detail_max={(i+1)*.5f,1};
    }
    // Keep the bun's separately cropped 192px field. Mapping it back into the
    // full source image would reduce its effective horizontal density to80px.
    return out;
  }();
  return result;
}
const CharacterSurfaceRegion& character_surface_region(CharacterSurface surface) {
  const auto index=static_cast<unsigned>(surface);
  if(index>=regions.size()) throw std::out_of_range("Invalid character surface");
  return regions[index];
}
Vec2 character_surface_uv(CharacterSurface surface,float u,float v) {
  const auto& region=character_surface_region(surface);
  if(!std::isfinite(u) || !std::isfinite(v))
    throw std::invalid_argument("Character surface UVs must be finite");
  Vec2 uv{clamp01(u),clamp01(v)};
  if(surface==CharacterSurface::Face) uv=packed_face_uv(uv,source_face_chart(uv));
  const float span=float(region.size-2*region.gutter-1);
  const float x=float(region.x+region.gutter)+.5f+uv.x*span;
  const float y=float(region.y+region.gutter)+.5f+uv.y*span;
  return {x/character_surface_atlas_size,y/character_surface_atlas_size};
}
std::shared_ptr<const MaterialTextures> character_surface_textures() {
  static const auto textures=generate();
  return textures;
}
Material make_character_surface_material() {
  Material material;
  material.albedo={1,1,1}; material.metallic=1.f; material.roughness=1.f;
  material.normal_scale=1.f; material.world_uv_scale=0.f;
  material.textures=character_surface_textures();
  return material;
}

}  // namespace fury
