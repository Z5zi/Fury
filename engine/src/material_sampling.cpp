#include "fury/material_sampling.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace fury {
namespace {
bool finite(Vec2 v) { return std::isfinite(v.x)&&std::isfinite(v.y); }
Vec4 bilinear(const RgbaImage& image,Vec2 uv,bool srgb) {
  static const std::array<float,256> linear=[] {
    std::array<float,256> values{};
    for(unsigned i=0;i<values.size();++i) {
      const float c=i/255.f;
      values[i]=c<=.04045f ? c/12.92f:std::pow((c+.055f)/1.055f,2.4f);
    }
    return values;
  }();
  const float x=(uv.x-std::floor(uv.x))*image.width-.5f;
  const float y=(uv.y-std::floor(uv.y))*image.height-.5f;
  const int ix=int(std::floor(x)),iy=int(std::floor(y));
  const float fx=x-ix,fy=y-iy;
  auto texel=[&](int px,int py) {
    px=(px+image.width)%image.width; py=(py+image.height)%image.height;
    const auto* p=image.pixels.data()+(std::size_t(py)*image.width+px)*4;
    return Vec4{srgb?linear[p[0]]:p[0]/255.f,srgb?linear[p[1]]:p[1]/255.f,
                srgb?linear[p[2]]:p[2]/255.f,p[3]/255.f};
  };
  const Vec4 a=texel(ix,iy),b=texel(ix+1,iy),c=texel(ix,iy+1),d=texel(ix+1,iy+1);
  auto lerp=[&](float aa,float bb,float cc,float dd) {
    return (aa*(1-fx)+bb*fx)*(1-fy)+(cc*(1-fx)+dd*fx)*fy;
  };
  return {lerp(a.x,b.x,c.x,d.x),lerp(a.y,b.y,c.y,d.y),
          lerp(a.z,b.z,c.z,d.z),lerp(a.w,b.w,c.w,d.w)};
}
}
MaterialProjection world_planar_projection(Vec3 p,Vec3 n,float scale) {
  const float x=std::fabs(n.x),y=std::fabs(n.y),z=std::fabs(n.z);
  if(y>=x && y>=z) return {{p.x*scale,p.z*scale},{{1,0,0},{0,0,1}}};
  if(z>=x) return {{p.x*scale,-p.y*scale},{{1,0,0},{0,-1,0}}};
  return {{p.z*scale,-p.y*scale},{{0,0,1},{0,-1,0}}};
}
MaterialTangentFrame orthonormalize_material_frame(Vec3 n,MaterialTangentFrame frame) {
  Vec3 t=frame.tangent-n*dot(n,frame.tangent);
  if(dot(t,t)<1e-12f) t=cross(frame.bitangent,n);
  if(dot(t,t)<1e-12f) t=cross(std::fabs(n.y)<.99f?Vec3{0,1,0}:Vec3{1,0,0},n);
  t=normalize(t);
  Vec3 b=normalize(cross(n,t));
  if(dot(b,frame.bitangent)<0) b=-b;
  return {t,b};
}
TextureFootprint perspective_texture_footprint(Vec2 uv,Vec2 nx,Vec2 ny,
    float iw,float ix,float iy) {
  if(!(iw>0) || !std::isfinite(iw)) return {};
  return {{(nx.x-uv.x*ix)/iw,(nx.y-uv.y*ix)/iw},
          {(ny.x-uv.x*iy)/iw,(ny.y-uv.y*iy)/iw}};
}
TextureFootprint ray_cone_texture_footprint(Vec3 e1,Vec3 e2,Vec2 uv1,Vec2 uv2,
    Vec3 direction,float diameter) {
  if(!(diameter>0)) return {};
  const Vec3 area=cross(e1,e2);
  const float area2=dot(area,area);
  if(!(area2>1e-20f)) return {};
  const Vec3 n=normalize(area);
  // World-space gradients of the barycentric UV interpolant. They also handle
  // nonuniform/mirrored transforms and arbitrary authored UV scale correctly.
  const Vec3 reciprocal1=cross(e2,area)*(1/area2);
  const Vec3 reciprocal2=cross(area,e1)*(1/area2);
  const Vec3 gu=reciprocal1*uv1.x+reciprocal2*uv2.x;
  const Vec3 gv=reciprocal1*uv1.y+reciprocal2*uv2.y;
  const Vec3 a=normalize(cross(std::fabs(direction.y)<.99f?Vec3{0,1,0}:Vec3{1,0,0},direction));
  const Vec3 b=cross(direction,a);
  // Cap only the near-parallel numerical singularity, not the ordinary grazing
  // footprint. A 1e-4 cosine already selects the final mip of practical maps.
  const float cosine=dot(n,direction);
  const float divisor=std::copysign(std::max(1e-4f,std::fabs(cosine)),cosine);
  const Vec3 dx=(a-direction*(dot(n,a)/divisor))*diameter;
  const Vec3 dy=(b-direction*(dot(n,b)/divisor))*diameter;
  return {{dot(gu,dx),dot(gv,dx)},{dot(gu,dy),dot(gv,dy)}};
}
float material_texture_lod(const RgbaImage& image,TextureFootprint f) {
  if(!finite(f.dx) || !finite(f.dy)) return std::numeric_limits<float>::infinity();
  // Double arithmetic avoids overflow for huge but finite tiling/derivatives.
  const double ux=double(f.dx.x)*image.width,vx=double(f.dx.y)*image.height;
  const double uy=double(f.dy.x)*image.width,vy=double(f.dy.y)*image.height;
  const double a=ux*ux+vx*vx,b=ux*uy+vx*vy,c=uy*uy+vy*vy;
  const double eigenvalue=.5*(a+c+std::hypot(a-c,2*b));
  return eigenvalue>1 ? float(.5*std::log2(eigenvalue)):0.f;
}
Vec4 sample_material_texture_lod(const RgbaImage& image,
    const std::vector<RgbaImage>& mips,Vec2 uv,bool srgb,float lod) {
  if(!image.valid() || !finite(uv)) return {1,1,1,1};
  if(mips.empty() || !(lod>0)) return bilinear(image,uv,srgb);
  unsigned levels=0;
  int width=image.width,height=image.height;
  for(const auto& mip:mips) {
    if(width==1 && height==1) break;
    width=std::max(1,width/2); height=std::max(1,height/2);
    if(!mip.valid() || mip.width!=width || mip.height!=height) break;
    ++levels;
  }
  if(!levels) return bilinear(image,uv,srgb);
  lod=std::min(lod,float(levels));
  const auto low=unsigned(std::floor(lod)),high=std::min(low+1,levels);
  const Vec4 a=bilinear(low?mips[low-1]:image,uv,srgb);
  if(low==high) return a;
  const Vec4 b=bilinear(mips[high-1],uv,srgb);
  const float t=lod-low,s=1-t;
  return {a.x*s+b.x*t,a.y*s+b.y*t,a.z*s+b.z*t,a.w*s+b.w*t};
}
Vec4 sample_material_texture(const RgbaImage& image,
    const std::vector<RgbaImage>& mips,Vec2 uv,bool srgb,TextureFootprint footprint) {
  return sample_material_texture_lod(image,mips,uv,srgb,
      mips.empty()?0.f:material_texture_lod(image,footprint));
}
bool material_color_detail_region_valid(const MaterialColorDetailRegion& r) {
  const auto bounds=[](Vec2 lo,Vec2 hi) {
    return finite(lo) && finite(hi) && lo.x>=0 && lo.y>=0 && hi.x<=1 && hi.y<=1 &&
        hi.x>lo.x && hi.y>lo.y;
  };
  return bounds(r.source_min,r.source_max) && bounds(r.detail_min,r.detail_max) &&
      std::isfinite((r.detail_max.x-r.detail_min.x)/(r.source_max.x-r.source_min.x)) &&
      std::isfinite((r.detail_max.y-r.detail_min.y)/(r.source_max.y-r.source_min.y));
}
Vec4 apply_material_color_detail(const MaterialColorDetail* detail,Vec4 base,
    Vec2 uv,TextureFootprint footprint,bool srgb) {
  if(!detail || !detail->color.valid() || !finite(uv)) return base;
  const Vec2 wrapped{uv.x-std::floor(uv.x),uv.y-std::floor(uv.y)};
  for(unsigned i=0;i<std::min(detail->region_count,MaterialColorDetail::kMaxRegions);++i) {
    const auto& r=detail->regions[i];
    if(!material_color_detail_region_valid(r) || wrapped.x<r.source_min.x ||
        wrapped.y<r.source_min.y || wrapped.x>=r.source_max.x || wrapped.y>=r.source_max.y) continue;
    const Vec2 scale{(r.detail_max.x-r.detail_min.x)/(r.source_max.x-r.source_min.x),
                     (r.detail_max.y-r.detail_min.y)/(r.source_max.y-r.source_min.y)};
    const Vec2 mapped{r.detail_min.x+(wrapped.x-r.source_min.x)*scale.x,
                      r.detail_min.y+(wrapped.y-r.source_min.y)*scale.y};
    const TextureFootprint remapped{{footprint.dx.x*scale.x,footprint.dx.y*scale.y},
                                    {footprint.dy.x*scale.x,footprint.dy.y*scale.y}};
    auto result=sample_material_texture(detail->color,detail->color_mips,mapped,srgb,remapped);
    if(r.preserve_base_alpha) result.w=base.w;
    return result;
  }
  return base;
}
} // namespace fury
