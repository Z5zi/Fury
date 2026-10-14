#include "world_life.hpp"

#include <fury/gltf.hpp>
#include <fury/texture.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>

namespace vaultline {
namespace {
using fury::Aabb;
using fury::Entity;
using fury::Mesh;
using fury::Vec3;
constexpr float pi = 3.14159265358979323846f;
constexpr const char* marker_name = "WorldLife.Applied.v1";
const Vec3 bark{.29f, .22f, .14f};
const Vec3 foliage{.25f, .39f, .17f};
const Vec3 steel{.47f, .49f, .48f};
const Vec3 dark{.14f, .17f, .17f};
const Vec3 wood{.57f, .44f, .29f};

struct Bounds { Vec3 lo, hi; };
Bounds bounds(const Mesh& mesh) {
  Bounds b{{1e9f,1e9f,1e9f},{-1e9f,-1e9f,-1e9f}};
  for (const auto& v : mesh.vertices) {
    b.lo.x=std::min(b.lo.x,v.position.x); b.hi.x=std::max(b.hi.x,v.position.x);
    b.lo.y=std::min(b.lo.y,v.position.y); b.hi.y=std::max(b.hi.y,v.position.y);
    b.lo.z=std::min(b.lo.z,v.position.z); b.hi.z=std::max(b.hi.z,v.position.z);
  }
  return b;
}
void triangle(Mesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 color) {
  const Vec3 n=fury::normalize(fury::cross(b-a,c-a));
  const auto base=static_cast<std::uint32_t>(m.vertices.size());
  m.vertices.push_back({a,n,color,{0,0}});
  m.vertices.push_back({b,n,color,{1,0}});
  m.vertices.push_back({c,n,color,{.5f,1}});
  m.indices.insert(m.indices.end(),{base,base+1,base+2});
}
void quad(Mesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 color) {
  triangle(m,a,b,c,color); triangle(m,a,c,d,color);
}
void append(Mesh& dst,const Mesh& src,Vec3 p={}) {
  const auto base=static_cast<std::uint32_t>(dst.vertices.size());
  for (auto v:src.vertices) { v.position+=p; dst.vertices.push_back(v); }
  for (auto i:src.indices) dst.indices.push_back(base+i);
}
void box(Mesh& m,Vec3 p,Vec3 size,Vec3 color) {
  append(m,fury::make_box(size,color),p);
}
// Tapered branch/prism with a proper normal on every facet. Caps are optional
// only at attached ends; foliage, pots and utility silhouettes are opaque.
void tube(Mesh& m,Vec3 a,Vec3 b,float ra,float rb,Vec3 color,int sides=6,bool caps=true) {
  const Vec3 axis=fury::normalize(b-a);
  const Vec3 u=fury::normalize(fury::cross(axis,std::fabs(axis.y)>.9f?Vec3{1,0,0}:Vec3{0,1,0}));
  const Vec3 v=fury::cross(axis,u);
  for (int i=0;i<sides;++i) {
    const float t=2*pi*float(i)/float(sides), t1=2*pi*float(i+1)/float(sides);
    const Vec3 r=u*std::cos(t)+v*std::sin(t), s=u*std::cos(t1)+v*std::sin(t1);
    quad(m,a+r*ra,a+s*ra,b+s*rb,b+r*rb,color*(.92f+.08f*std::cos(t)));
    if(caps) { triangle(m,a,a+s*ra,a+r*ra,color); triangle(m,b,b+r*rb,b+s*rb,color); }
  }
}
void fit(Mesh& m,Bounds target) {
  const auto src=bounds(m);
  const Vec3 ss=src.hi-src.lo, ts=target.hi-target.lo;
  const Vec3 scale{ts.x/ss.x,ts.y/ss.y,ts.z/ss.z};
  for(auto& v:m.vertices) {
    v.position={target.lo.x+(v.position.x-src.lo.x)*scale.x,
                target.lo.y+(v.position.y-src.lo.y)*scale.y,
                target.lo.z+(v.position.z-src.lo.z)*scale.z};
    v.normal=fury::normalize({v.normal.x/scale.x,v.normal.y/scale.y,v.normal.z/scale.z});
  }
}
float noise(std::uint32_t& s) { s=s*1664525u+1013904223u; return float(s>>8)*(1.f/16777216.f); }

// A folded lanceolate leaf has a real silhouette and two tilted upper facets.
// It does not need an alpha mask, transparent sorting or view-facing billboards.
void leaf(Mesh& m,Vec3 root,Vec3 dir,float len,float width,Vec3 color,bool two_sided=false) {
  const auto begin=m.indices.size();
  dir=fury::normalize(dir);
  const Vec3 side=fury::normalize(fury::cross(dir,{0,1,0}))*width;
  const Vec3 tip=root+dir*len;
  const Vec3 mid=root+dir*(len*.46f), ridge=mid+Vec3{0,width*.24f,0};
  triangle(m,root,mid+side,ridge,color*.87f);
  triangle(m,root,ridge,mid-side,color);
  triangle(m,mid+side,tip,ridge,color);
  triangle(m,ridge,tip,mid-side,color*1.09f);
  if(two_sided) {
    const auto end=m.indices.size();
    for(auto i=begin;i<end;i+=3) {
      const auto a=m.vertices[m.indices[i]],b=m.vertices[m.indices[i+1]],c=m.vertices[m.indices[i+2]];
      triangle(m,a.position,c.position,b.position,a.color*.86f);
    }
  }
}
// A sparse hierarchy of bent trunks, outward limbs and small leaf sprays gives
// the canopy holes and irregular edges. LOD thins sprays, not trunk geometry.
void plant(Mesh& m,Vec3 p,float height,float radius,std::uint32_t seed,bool low,bool dry=false,bool two_sided_leaves=false) {
  const int branches=height>1.6f?9:7;
  const Vec3 bend{radius*.12f,height*.61f,-radius*.10f};
  tube(m,p,p+bend,height*.034f,height*.020f,bark,low?4:6);
  tube(m,p+bend,p+Vec3{radius*.08f,height*.96f,0},height*.020f,height*.005f,bark,low?4:5);
  for(int i=0;i<branches;++i) {
    const float angle=float(i)*2.39996323f+noise(seed)*.32f;
    const float spread=radius*(.66f+noise(seed)*.34f);
    const float level=.38f+.051f*float(i);
    const Vec3 start=p+Vec3{bend.x*level,height*level,bend.z*level};
    const Vec3 end=p+Vec3{std::cos(angle)*spread,height*(level+.19f),std::sin(angle)*spread};
    tube(m,start,end,height*.014f,height*.004f,bark,low?3:5,false);
    // Two tapered secondary twigs carry six alternating leaves each. Every
    // leaf root lies on a branch; no floating leaf cards surround the crown.
    for(int sprig=0;sprig<2;++sprig) {
      const float a=angle+(sprig==0?-.62f:.62f)+noise(seed)*.18f;
      const Vec3 tip=end+Vec3{std::cos(a)*radius*.46f,height*.065f,std::sin(a)*radius*.46f};
      tube(m,end,tip,height*.003f,height*.001f,bark,3,false);
      for(int j=0;j<6;++j) {
        const float along=.2f+.16f*float(j);
        const Vec3 root=end+(tip-end)*along;
        const float la=a+(j%2==0?-.9f:.9f)+noise(seed)*.25f;
        const Vec3 dir{std::cos(la),.18f+noise(seed)*.50f,std::sin(la)};
        const Vec3 color=(dry?Vec3{.38f,.39f,.22f}:foliage)*(.72f+noise(seed)*.50f);
        const float l=radius*(low?.62f:.42f);
        if(!low||j%3==0) leaf(m,root,dir,l,l*(low?.31f:.27f),color,two_sided_leaves);
      }
    }
  }
}
void reeds(Mesh& m,Vec3 p,std::uint32_t seed,bool low) {
  for(int i=0;i<(low?7:18);++i) {
    const float a=noise(seed)*2*pi, r=std::sqrt(noise(seed))*.56f;
    const float h=.65f+noise(seed)*.75f;
    const Vec3 base=p+Vec3{std::cos(a)*r,0,std::sin(a)*r};
    const Vec3 tip=base+Vec3{std::cos(a)*.24f,h,std::sin(a)*.24f};
    // Four-sided stem and bent blades keep reeds visible edge-on in both renderers.
    tube(m,base,tip,.014f,.008f,{.43f,.44f,.24f},3,false);
    const int count=low?1:2;
    for(int j=0;j<count;++j)
      leaf(m,base+(tip-base)*(.32f+float(j)*.21f),{std::cos(a+j*2.4f),.75f,std::sin(a+j*2.4f)},.46f,.034f,{.33f,.40f,.20f});
    if(i%3==0) tube(m,tip,tip+Vec3{0,.16f,0},.032f,.026f,{.36f,.28f,.17f},low?3:4);
  }
}
void pot(Mesh& m,Vec3 p,float radius,float h,bool low,Vec3 color={.48f,.40f,.30f}) {
  tube(m,p+Vec3{0,.035f,0},p+Vec3{0,h,0},radius*.79f,radius,color,low?6:10,false);
  tube(m,p+Vec3{0,h*.88f,0},p+Vec3{0,h,0},radius*1.035f,radius*1.035f,color*1.1f,low?6:10,false);
  tube(m,p+Vec3{0,h*.87f,0},p+Vec3{0,h*.90f,0},radius*.92f,radius*.92f,{.16f,.14f,.095f},low?6:10);
}
Mesh planter(bool low) {
  Mesh m;
  // Four separate wall slabs expose a recessed soil well instead of a solid top.
  box(m,{0,-.04f,-.70f},{1.6f,.62f,.2f},{.44f,.39f,.32f});
  box(m,{0,-.04f,.70f},{1.6f,.62f,.2f},{.44f,.39f,.32f});
  box(m,{-.70f,-.04f,0},{.2f,.62f,1.2f},{.44f,.39f,.32f});
  box(m,{.70f,-.04f,0},{.2f,.62f,1.2f},{.44f,.39f,.32f});
  box(m,{0,.16f,0},{1.22f,.07f,1.22f},{.16f,.14f,.10f});
  if(!low) for(float s:{-1.f,1.f}) {
    box(m,{0,.31f,.70f*s},{1.6f,.08f,.20f},{.58f,.51f,.41f});
    box(m,{.70f*s,.31f,0},{.20f,.08f,1.2f},{.58f,.51f,.41f});
  }
  return m;
}
Mesh hydrant(bool low) {
  Mesh m; const int n=low?4:6;
  tube(m,{0,-.45f,0},{0,-.34f,0},.19f,.19f,steel,n);
  tube(m,{0,-.34f,0},{0,.26f,0},.125f,.125f,steel,n,false);
  tube(m,{0,.26f,0},{0,.41f,0},.15f,.06f,steel,n);
  tube(m,{-.20f,.05f,0},{.20f,.05f,0},.095f,.095f,steel,low?3:5);
  if(!low) {
    tube(m,{0,.06f,-.15f},{0,.06f,-.07f},.075f,.075f,dark,5);
    box(m,{0,.43f,0},{.07f,.04f,.07f},dark);
  }
  return m;
}
Mesh bin(bool low) {
  Mesh m; const int n=low?5:8;
  tube(m,{0,-.5f,0},{0,.38f,0},.29f,.32f,{.32f,.35f,.30f},n,false);
  tube(m,{0,.36f,0},{0,.5f,0},.35f,.31f,dark,low?5:6);
  if(!low) for(int i=0;i<4;++i) {
    const float a=float(i)*pi*.5f;
    tube(m,{std::cos(a)*.304f,-.38f,std::sin(a)*.304f},
           {std::cos(a)*.328f,.28f,std::sin(a)*.328f},.012f,.012f,steel,3,false);
  }
  return m;
}
Mesh bench(bool low) {
  Mesh m;
  for(float x:{-.78f,.78f}) {
    box(m,{x,-.07f,0},{.1f,.32f,.58f},dark);
    box(m,{x,-.18f,0},{.35f,.08f,.65f},dark);
  }
  const int count=low?2:4;
  for(int i=0;i<count;++i)
    box(m,{0,.16f,-.35f+(float(i)+.5f)*.7f/count},{2.2f,.13f,.7f/count-.025f},wood*(.91f+.04f*i));
  return m;
}
Mesh bollard(bool low) {
  Mesh m;
  tube(m,{0,-.5f,0},{0,.40f,0},.15f,.15f,steel,low?5:8);
  tube(m,{0,.40f,0},{0,.50f,0},.15f,.075f,steel,low?5:8);
  if(!low) tube(m,{0,.20f,0},{0,.29f,0},.155f,.155f,{.75f,.68f,.40f},8,false);
  return m;
}
Mesh dumpster(bool low) {
  Mesh m;
  box(m,{0,-.04f,0},{2.12f,1.13f,1.28f},{.29f,.42f,.31f});
  box(m,{0,.60f,0},{2.2f,.20f,1.4f},dark);
  if(!low) {
    box(m,{0,.73f,0},{.05f,.05f,1.4f},steel);
    for(float x:{-.96f,.96f}) {
      box(m,{x,.04f,.66f},{.07f,.9f,.065f},steel);
      tube(m,{x,-.7f,-.40f},{x,-.60f,-.40f},.10f,.10f,dark,6);
      tube(m,{x,-.7f,.40f},{x,-.60f,.40f},.10f,.10f,dark,6);
    }
    box(m,{0,.32f,.72f},{.40f,.07f,.04f},steel);
  }
  return m;
}
Mesh aircon(bool low) {
  Mesh m;
  box(m,{0,0,0},{1.8f,1.1f,1.4f},{.66f,.69f,.68f});
  box(m,{0,.025f,.71f},{1.42f,.74f,.035f},dark);
  if(!low) {
    for(int i=0;i<5;++i) {
      const float y=-.27f+float(i)*.145f;
      // Only front and upper sloped louver faces are exposed; hidden backs
      // intersect the original housing and do not need raster/trace triangles.
      quad(m,{-.71f,y-.02f,.77f},{.71f,y-.02f,.77f},{.71f,y+.02f,.77f},{-.71f,y+.02f,.77f},steel);
      quad(m,{-.71f,y+.02f,.77f},{.71f,y+.02f,.77f},{.71f,y+.045f,.71f},{-.71f,y+.045f,.71f},steel*.84f);
    }
    box(m,{.73f,.02f,.74f},{.10f,.80f,.06f},steel);
  }
  return m;
}
Mesh fan(bool low) {
  Mesh m; const int n=low?5:8;
  tube(m,{0,-.10f,0},{0,.09f,0},.49f,.49f,dark,n,false);
  for(int i=0;i<n;++i) {
    const float a=2*pi*float(i)/n,b=2*pi*float(i+1)/n;
    triangle(m,{0,.075f,0},{std::cos(b)*.49f,.075f,std::sin(b)*.49f},
             {std::cos(a)*.49f,.075f,std::sin(a)*.49f},dark);
  }
  if(!low) {
    tube(m,{0,.09f,0},{0,.14f,0},.10f,.10f,steel,5);
    for(int i=0;i<4;++i) {
      const float a=float(i)*pi*.5f;
      const Vec3 d{std::cos(a),0,std::sin(a)},s{-std::sin(a),0,std::cos(a)};
      quad(m,{0,.10f,0},d*.42f+s*.12f+Vec3{0,.11f,0},d*.45f-s*.06f+Vec3{0,.11f,0},d*.12f-s*.12f+Vec3{0,.10f,0},steel);
    }
  }
  return m;
}
Mesh container(bool low) {
  Mesh m;
  // Preserve the source paint as the original material tint; the old source
  // multiplied paint twice. Neutral vertex metal makes existing colors readable.
  box(m,{0,0,0},{2.34f,2.13f,1.94f},{.81f,.83f,.81f});
  for(float x:{-1.15f,1.15f}) box(m,{x,0,0},{.10f,2.2f,2.f},{.48f,.51f,.50f});
  for(float y:{-1.045f,1.045f}) box(m,{0,y,0},{2.4f,.11f,2.f},{.64f,.67f,.65f});
  box(m,{0,0,1.f},{.035f,2.02f,.035f},dark);
  if(!low) {
    // Four side ribs per side + paired lock bars + hinges, all in one mesh.
    for(float x:{-1.188f,1.188f}) for(int i=0;i<4;++i) {
      const float z=-.68f+i*.45f, outside=x+(x<0?-.0125f:.0125f);
      const Vec3 a{x,-.975f,z-.055f},b{outside,-.975f,z},c{x,-.975f,z+.055f};
      const Vec3 up{0,1.95f,0};
      quad(m,a,b,b+up,a+up,{.70f,.72f,.70f});
      quad(m,b,c,c+up,b+up,{.75f,.77f,.75f});
      triangle(m,a,c,b,steel); triangle(m,a+up,b+up,c+up,steel);
    }
    for(float x:{-.55f,.55f}) {
      box(m,{x,0,1.025f},{.045f,1.92f,.045f},{.53f,.58f,.57f});
      box(m,{x+.09f,-.15f,1.05f},{.22f,.045f,.06f},dark);
    }
  }
  return m;
}
Mesh crane_mast(bool low) {
  Mesh m;
  const Vec3 paint{.89f,.84f,.66f};
  for(float x:{-.70f,.70f}) for(float z:{-.70f,.70f})
    box(m,{x,0,z},{.20f,18.f,.20f},paint);
  const int count=low?5:9;
  for(int i=0;i<count;++i) {
    const float y=-9.f+18.f*float(i)/count, next=-9.f+18.f*float(i+1)/count;
    for(float s:{-1.f,1.f}) {
      tube(m,{-.70f,y,.70f*s},{.70f,next,.70f*s},.06f,.06f,paint,3,false);
      tube(m,{.70f*s,y,-.70f},{.70f*s,next,.70f},.06f,.06f,paint,3,false);
    }
    if(!low) {
      box(m,{0,y,-.7f},{1.4f,.11f,.11f},paint);
      box(m,{0,y,.7f},{1.4f,.11f,.11f},paint);
    }
  }
  box(m,{0,-8.83f,0},{1.6f,.34f,1.6f},dark);
  return m;
}
Mesh crane_boom(bool low) {
  Mesh m; const Vec3 paint{.91f,.85f,.68f};
  for(float y:{-.50f,.50f}) for(float z:{-.60f,.60f})
    box(m,{0,y,z},{14.f,.18f,.18f},paint);
  const int count=low?6:12;
  for(int i=0;i<count;++i) {
    const float x=-6.9f+13.8f*float(i)/count, nx=-6.9f+13.8f*float(i+1)/count;
    for(float z:{-.6f,.6f}) tube(m,{x,-.5f,z},{nx,.5f,z},.065f,.065f,paint,3,false);
    if(!low) tube(m,{x,-.5f,-.6f},{nx,-.5f,.6f},.045f,.045f,paint,3,false);
  }
  return m;
}
Mesh market(bool low) {
  Mesh m;
  box(m,{0,.49f,0},{3.6f,.12f,1.4f},wood*1.10f);
  for(float x:{-1.58f,1.58f}) box(m,{x,-.05f,0},{.14f,1.f,1.2f},wood*.78f);
  const int planks=low?3:7;
  for(int i=0;i<planks;++i) box(m,{-1.7f+(i+.5f)*3.4f/planks,-.04f,.65f},
      {3.4f/planks-.025f,.85f,.12f},wood*(.80f+.035f*i));
  if(!low) for(int i=0;i<3;++i) {
    const float x=-1.13f+i*1.1f;
    box(m,{x,.63f,0},{.9f,.12f,1.08f},wood*.75f);
    for(float z:{-.49f,.49f}) box(m,{x,.75f,z},{.9f,.22f,.07f},wood);
    for(float s:{-.42f,.42f}) box(m,{x+s,.75f,0},{.07f,.22f,.91f},wood);
    for(int j=0;j<4;++j) tube(m,{x-.28f+float(j%2)*.5f,.72f,-.2f+float(j/2)*.4f},
      {x-.28f+float(j%2)*.5f,.91f,-.2f+float(j/2)*.4f},.14f,.08f,
      i==0?Vec3{.68f,.28f,.15f}:(i==1?Vec3{.47f,.57f,.22f}:Vec3{.78f,.65f,.25f}),5);
  }
  return m;
}
Mesh awning(bool low) {
  Mesh m;
  // Segmented, pitched canvas with a scalloped valance instead of a flat box.
  const int count=low?4:10;
  for(int i=0;i<count;++i) {
    const float x=-2.25f+4.5f*float(i)/count,nx=-2.25f+4.5f*float(i+1)/count;
    const Vec3 color=i%2==0?Vec3{.93f,.86f,.72f}:Vec3{.47f,.47f,.43f};
    quad(m,{x,-.01f,-1.6f},{x,.16f,0},{nx,.16f,0},{nx,-.01f,-1.6f},color);
    quad(m,{x,.16f,0},{x,-.01f,1.6f},{nx,-.01f,1.6f},{nx,.16f,0},color);
    quad(m,{x,-.01f,1.6f},{x,-.19f,1.6f},{nx,-.19f,1.6f},{nx,-.01f,1.6f},color*.8f);
    triangle(m,{x,-.19f,1.6f},{(x+nx)*.5f,-.25f,1.6f},{nx,-.19f,1.6f},color*.8f);
  }
  return m;
}

using Builder = Mesh (*)(bool);
struct Kind { const char* key; Builder build; int category; };
Kind kind(const std::string& n) {
  if(n=="PlanterA"||n=="PlanterB") return {"planter",planter,0};
  if(n=="Hydrant") return {"hydrant",hydrant,0};
  if(n=="MidTrash"||n=="TrashCanA"||n=="TrashCanB"||n=="TrashCanC"||n=="TrashCanD"||n=="TrashCanE") return {"bin",bin,0};
  if(n=="StreetBenchA"||n=="StreetBenchB"||n=="StreetBenchC"||n=="StreetBenchD") return {"bench",bench,0};
  if(n=="Bollard") return {"bollard",bollard,0};
  if(n=="Dumpster"||n=="Dumpster2") return {"dumpster",dumpster,0};
  if(n=="RooftopAC") return {"aircon",aircon,0};
  if(n=="RooftopACFan") return {"fan",fan,0};
  if(n=="NQContainer") return {"container",container,1};
  if(n=="NQCraneMast0"||n=="NQCraneMast1") return {"crane_mast",crane_mast,2};
  if(n=="NQCraneBoom0"||n=="NQCraneBoom1") return {"crane_boom",crane_boom,2};
  if(n=="MarketStallA"||n=="MarketStallB") return {"market",market,3};
  if(n=="StallAwningA"||n=="StallAwningB") return {"awning",awning,3};
  return {nullptr,nullptr,0};
}
struct MeshPair { Mesh* near; Mesh* far; };
MeshPair add_pair(fury::Scene& scene,Mesh near,Mesh far) {
  return {scene.add_mesh(std::move(near)),scene.add_mesh(std::move(far))};
}
void count_mesh(WorldLifeStats& stats,MeshPair p) {
  stats.near_triangles+=p.near->indices.size()/3;
  if(p.far) stats.lod_triangles+=p.far->indices.size()/3;
}

// Merged Harbor props have lost node labels, so a color heuristic is insufficient.
// Re-import only the named authored foliage primitives, fingerprint each baked
// triangle (position + color), and require a one-to-one match of all 3,600 faces.
// Non-foliage vertices/indices, maps, branding and materials are copied unchanged.
using Fingerprint = std::array<std::int64_t,18>;
Fingerprint fingerprint(const fury::Vertex& a,const fury::Vertex& b,const fury::Vertex& c) {
  std::array<std::array<std::int64_t,6>,3> points;
  const fury::Vertex* vertices[]={&a,&b,&c};
  for(int i=0;i<3;++i) {
    const auto& v=*vertices[i];
    const float values[]={v.position.x,v.position.y,v.position.z,v.color.x,v.color.y,v.color.z};
    for(int j=0;j<6;++j) points[i][j]=std::llround(double(values[j])*100000.0);
  }
  std::sort(points.begin(),points.end());
  Fingerprint result{};
  for(int i=0;i<3;++i) for(int j=0;j<6;++j) result[i*6+j]=points[i][j];
  return result;
}
bool authored_foliage(const Entity& e,Mesh& near,Mesh& far) {
  const bool kit=e.name=="BlkStreetKit";
  if(e.material.textures) {
    const auto& t=*e.material.textures;
    if(t.base_color.valid()||t.normal.valid()||t.metallic_roughness.valid()||t.emissive.valid()) return false;
  }
  const std::string relative=std::string("assets/meshes/harbor_metro/")+
    (kit?"hm_street_props_kit_v2.glb":"hm_prop_planter_v2.glb");
  fury::GltfAsset source; std::string error; bool loaded=false;
  for(const char* prefix:{"","../","../../","../../../"}) {
    const std::string path=std::string(prefix)+relative;
    std::error_code ec;
    if(std::filesystem::is_regular_file(path,ec)&&fury::load_gltf(path,source,error)) { loaded=true; break; }
  }
  if(!loaded) return false;
  std::map<Fingerprint,unsigned> expected;
  std::size_t primitive_count=0;
  for(const auto& primitive:source.primitives) {
    bool selected=false;
    for(int n=0;n<5;++n) if(primitive.name=="Planter_Foliage_"+std::to_string(n)) selected=true;
    if(!selected) continue;
    if(!primitive.mesh||primitive.mesh->indices.size()!=720*3) return false;
    ++primitive_count;
    for(std::size_t i=0;i<primitive.mesh->indices.size();i+=3) {
      fury::Vertex v[3];
      for(int j=0;j<3;++j) {
        v[j]=primitive.mesh->vertices[primitive.mesh->indices[i+j]];
        v[j].position=fury::transform_point(primitive.transform,v[j].position);
        v[j].color={v[j].color.x*primitive.material.albedo.x,v[j].color.y*primitive.material.albedo.y,v[j].color.z*primitive.material.albedo.z};
      }
      ++expected[fingerprint(v[0],v[1],v[2])];
    }
  }
  if(primitive_count!=5||expected.size()!=3600) return false;
  std::vector<std::uint32_t> retained;
  retained.reserve(e.mesh->indices.size());
  std::size_t removed=0;
  for(std::size_t i=0;i<e.mesh->indices.size();i+=3) {
    const auto a=e.mesh->indices[i],b=e.mesh->indices[i+1],c=e.mesh->indices[i+2];
    const auto found=expected.find(fingerprint(e.mesh->vertices[a],e.mesh->vertices[b],e.mesh->vertices[c]));
    if(found!=expected.end()&&found->second>0) { --found->second; ++removed; }
    else retained.insert(retained.end(),{a,b,c});
  }
  if(removed!=3600) return false;
  near=*e.mesh; near.indices=std::move(retained); far=near;
  const Vec3 root=kit?Vec3{5.7f,.56f,-1.35f}:Vec3{0,.56f,0};
  plant(near,root,.88f,.31f,67,false,false,!e.material.double_sided);
  plant(far,root,.88f,.31f,67,true,false,!e.material.double_sided);
  return true;
}

struct Patch { Vec3 p; float h; float r; int species; }; // 0 tree, 1 shrub, 2 reed
const Patch harbor_patch[]={{{-28.f,.04f,-56.f},3.3f,.86f,0},{{-57.f,.04f,43.f},3.7f,1.f,0}};
const Patch ridge_patch[]={{{74.f,.06f,-9.f},3.2f,.90f,0},{{117.f,.06f,24.f},2.8f,.90f,0},
  {{79.f,.03f,25.4f},0,0,2},{{121.f,.03f,25.4f},0,0,2}};
const Patch ash_patch[]={{{-107.f,.06f,24.f},3.2f,.9f,0},{{-108.f,.06f,58.f},3.6f,.96f,0},
  {{-106.4f,.06f,25.2f},.65f,.35f,1},{{-107.3f,.06f,56.7f},.65f,.38f,1}};
const Patch depot_patch[]={{{75.f,.04f,-57.f},1.1f,.55f,1},{{49.f,.04f,-60.f},.8f,.50f,1},
  {{75.3f,.04f,-55.5f},.62f,.40f,1}};
const Patch loft_patch[]={{{48.2f,.04f,58.7f},3.2f,.82f,0},{{34.7f,.04f,59.5f},2.8f,.76f,0}};
const Patch quay_patch[]={{{34.f,.05f,114.8f},.95f,.52f,1},{{-15.f,.05f,115.2f},.85f,.48f,1},
  {{-11.f,.03f,115.4f},0,0,2},{{35.f,.03f,115.4f},0,0,2}};
struct District { const char* anchor; const char* name; const Patch* patches; std::size_t count; };
const District districts[]={
  {"BankPlaza","Harbor",harbor_patch,2},
  {"RidgePlaza","Ridge",ridge_patch,4},
  {"AshcourtPlaza","Ashcourt",ash_patch,4},
  {"DepotYard","Depot",depot_patch,3},
  {"LoftFloor","Loft",loft_patch,2},
  {"NorthQuayPlaza","NorthQuay",quay_patch,4}
};
} // namespace

WorldLifeStats upgrade_world_life(fury::Scene& scene) {
  WorldLifeStats stats;
  if(scene.find_by_name(marker_name)) { stats.already_applied=true; return stats; }
  std::map<std::string,MeshPair> cache;
  // No add_entity inside this loop: replacing mesh pointers never invalidates
  // entity references, and no shared input meshes or materials are modified.
  for(auto& e:scene.entities()) {
    if(!e.mesh||e.mesh->vertices.empty()) continue;
    if(e.name=="BlkPlanterA"||e.name=="BlkPlanterB"||e.name=="BlkStreetKit") {
      Mesh near,far;
      if(!authored_foliage(e,near,far)) { ++stats.skipped_authored_planters; continue; }
      // Existing authored detail props deliberately vanish at mid-distance.
      // A full-shell kit LOD would retain ~96k unrelated triangles that used to
      // be culled. Preserve that behavior instead of expanding distant work.
      const bool culled=e.detail&&!e.lod_mesh;
      const MeshPair p{scene.add_mesh(std::move(near)),culled?nullptr:scene.add_mesh(std::move(far))};
      stats.replaced_triangles+=e.mesh->indices.size()/3;
      e.mesh=p.near; e.lod_mesh=p.far;
      ++stats.replaced_entities; ++stats.authored_planters;
      ++stats.plant_instances; ++stats.district_plants[0];
      stats.removed_foliage_triangles+=3600;
      count_mesh(stats,p);
      continue;
    }
    if(e.material.textures) continue;
    const bool shrub=e.name=="PlanterShrubA"||e.name=="PlanterShrubB";
    const bool loft=e.name=="LoftPlant";
    MeshPair p{};
    if(shrub||loft) {
      const std::string key=loft?"loft_plant":"planter_shrub";
      auto found=cache.find(key);
      if(found==cache.end()) {
        Mesh near,far;
        for(bool low:{false,true}) {
          Mesh& m=low?far:near;
          if(loft) {
            pot(m,{0,-.55f,0},.24f,.32f,low);
            plant(m,{0,-.25f,0},.91f,.25f,139,low);
          } else plant(m,{0,-.46f,0},1.65f,.50f,51,low);
        }
        found=cache.emplace(key,add_pair(scene,std::move(near),std::move(far))).first;
      }
      p=found->second;
      ++stats.plant_instances; ++stats.district_plants[loft?4:0];
    } else {
      const Kind k=kind(e.name);
      if(!k.build) continue;
      const Bounds b=bounds(*e.mesh);
      if(b.hi.x-b.lo.x<1e-5f||b.hi.y-b.lo.y<1e-5f||b.hi.z-b.lo.z<1e-5f) continue;
      std::string key=k.key;
      for(float f:{b.lo.x,b.lo.y,b.lo.z,b.hi.x,b.hi.y,b.hi.z}) key+="/"+std::to_string(f);
      auto found=cache.find(key);
      if(found==cache.end()) {
        Mesh near=k.build(false),far=k.build(true);
        // Market baskets remain above the existing countertop; the footprint
        // remains strictly inside the existing stall, rather than a new obstacle.
        Bounds target=b;
        if(k.build==market) target.hi.y+=.32f;
        fit(near,target); fit(far,b);
        found=cache.emplace(key,add_pair(scene,std::move(near),std::move(far))).first;
      }
      p=found->second;
      if(k.category==0) ++stats.utility_props;
      else if(k.category==1) ++stats.containers;
      else if(k.category==2) ++stats.crane_parts;
      else ++stats.market_parts;
    }
    stats.replaced_triangles+=e.mesh->indices.size()/3;
    e.mesh=p.near; e.lod_mesh=p.far;
    ++stats.replaced_entities; count_mesh(stats,p);
  }
  for(std::size_t i=0;i<6;++i) {
    const auto& d=districts[i];
    if(!scene.find_by_name(d.anchor)) continue;
    // Spatial cells keep the renderer's distance-based LOD local. A single
    // Harbor-wide batch would switch two trees 100 metres apart together.
    struct Cell { Vec3 origin; Mesh near,far; std::size_t plants{}; };
    std::map<std::pair<int,int>,Cell> cells;
    for(std::size_t j=0;j<d.count;++j) {
      const auto& p=d.patches[j];
      const int cx=int(std::floor(p.p.x/16.f)),cz=int(std::floor(p.p.z/16.f));
      auto& cell=cells[{cx,cz}]; cell.origin={cx*16.f+8.f,0,cz*16.f+8.f};
      ++cell.plants;
      const Vec3 local=p.p-cell.origin;
      const auto seed=std::uint32_t(173+i*71+j*17);
      for(bool low:{false,true}) {
        Mesh& m=low?cell.far:cell.near;
        if(p.species==2) reeds(m,local,seed,low);
        else {
          const float pot_height=p.species==0?.40f:.17f;
          // New trees use compact planters; industrial scrub uses low soil rims.
          pot(m,local,p.species==0?.58f:p.r*.85f,pot_height,low,
              i==2?Vec3{.57f,.36f,.25f}:Vec3{.43f,.45f,.40f});
          plant(m,local+Vec3{0,pot_height*.9f,0},p.h,p.r,seed,low,i==3||i==5);
        }
      }
    }
    for(auto& entry:cells) {
      auto& cell=entry.second;
      const MeshPair p=add_pair(scene,std::move(cell.near),std::move(cell.far));
      Entity e;
      e.name=std::string("WorldLife.")+d.name+"."+std::to_string(entry.first.first)+"."+std::to_string(entry.first.second);
      e.mesh=p.near; e.lod_mesh=p.far;
      e.transform.position=cell.origin;
      e.material.roughness=.88f;
      e.material.double_sided=true;
      const Bounds b=bounds(*p.near),far_b=bounds(*p.far);
      const Vec3 lo{std::min(b.lo.x,far_b.lo.x),std::min(b.lo.y,far_b.lo.y),std::min(b.lo.z,far_b.lo.z)};
      const Vec3 hi{std::max(b.hi.x,far_b.hi.x),std::max(b.hi.y,far_b.hi.y),std::max(b.hi.z,far_b.hi.z)};
      // Non-solid conservative render bounds include both canopy LODs.
      e.collider=Aabb::from_center_size((lo+hi)*.5f,hi-lo);
      scene.add_entity(std::move(e));
      ++stats.added_batches; stats.plant_instances+=cell.plants; stats.district_plants[i]+=cell.plants;
      count_mesh(stats,p);
    }
  }
  Entity marker; marker.name=marker_name; marker.visible=false;
  scene.add_entity(std::move(marker));
  return stats;
}
} // namespace vaultline
