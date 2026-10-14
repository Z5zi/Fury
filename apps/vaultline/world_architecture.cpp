#include "world_architecture.hpp"

#include <fury/scene.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace vaultline {
namespace {
using fury::Entity;
using fury::Material;
using fury::Mesh;
using fury::Scene;
using fury::TextureSlot;
using fury::Vec3;
const Vec3 white{1.f, 1.f, 1.f};

enum class District { Metro, Ridge, Ashcourt, NorthQuay, None };

bool numbered(const std::string& name, const char* prefix) {
  const std::string p(prefix);
  return name.size() > p.size() && name.compare(0, p.size(), p) == 0 &&
      std::all_of(name.begin() + static_cast<std::ptrdiff_t>(p.size()), name.end(),
                  [](unsigned char c) { return c >= '0' && c <= '9'; });
}
District district(const std::string& name) {
  if (numbered(name, "Bldg")) return District::Metro;
  if (numbered(name, "RidgeBldg")) return District::Ridge;
  if (numbered(name, "AshShop")) return District::Ashcourt;
  if (numbered(name, "NQWarehouse")) return District::NorthQuay;
  return District::None;
}
unsigned stable_id(const std::string& name) {
  unsigned result = 2166136261u;
  for (unsigned char c : name) result = (result ^ c) * 16777619u;
  return result;
}

struct Bounds { Vec3 lo, hi; };
bool bounds(const Mesh* mesh, Bounds& b) {
  if (!mesh || mesh->vertices.empty()) return false;
  b.lo = {1e20f, 1e20f, 1e20f}; b.hi = {-1e20f, -1e20f, -1e20f};
  for (const auto& vertex : mesh->vertices) {
    const auto& p = vertex.position;
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return false;
    b.lo = {std::min(b.lo.x,p.x),std::min(b.lo.y,p.y),std::min(b.lo.z,p.z)};
    b.hi = {std::max(b.hi.x,p.x),std::max(b.hi.y,p.y),std::max(b.hi.z,p.z)};
  }
  return true;
}

void quad(Mesh& mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 color = white) {
  Vec3 normal = fury::normalize(fury::cross(b-a,c-a));
  const auto offset = static_cast<std::uint32_t>(mesh.vertices.size());
  const float u = fury::length(b-a), v = fury::length(d-a);
  mesh.vertices.push_back({a,normal,color,{0,0}});
  mesh.vertices.push_back({b,normal,color,{u,0}});
  mesh.vertices.push_back({c,normal,color,{u,v}});
  mesh.vertices.push_back({d,normal,color,{0,v}});
  mesh.indices.insert(mesh.indices.end(),{offset,offset+1,offset+2,offset,offset+2,offset+3});
}
void triangle(Mesh& mesh, Vec3 a, Vec3 b, Vec3 c) {
  const auto offset = static_cast<std::uint32_t>(mesh.vertices.size());
  Vec3 n = fury::normalize(fury::cross(b-a,c-a));
  mesh.vertices.push_back({a,n,white,{0,0}});
  mesh.vertices.push_back({b,n,white,{fury::length(b-a),0}});
  mesh.vertices.push_back({c,n,white,{0,fury::length(c-a)}});
  mesh.indices.insert(mesh.indices.end(),{offset,offset+1,offset+2});
}
void box(Mesh& mesh, Vec3 center, Vec3 size, Vec3 color = white) {
  auto part = fury::make_box(size,color);
  const auto offset = static_cast<std::uint32_t>(mesh.vertices.size());
  for (auto v : part.vertices) { v.position += center; mesh.vertices.push_back(v); }
  for (auto i : part.indices) mesh.indices.push_back(offset+i);
}

Material surface(Vec3 color, TextureSlot type, float roughness, float metallic = 0.f) {
  Material m;
  m.albedo = color; m.texture = type; m.roughness = roughness; m.metallic = metallic;
  m.world_uv_scale = type == TextureSlot::Brick ? .52f : .7f;
  return m;
}
struct Palette { Material body, stone, frame, roof, glass, occupied, accent; };
Palette palette(District d, unsigned id) {
  Palette p;
  const std::array<Vec3,4> city{{{.57f,.51f,.43f},{.45f,.32f,.25f},
                              {.47f,.50f,.50f},{.41f,.43f,.40f}}};
  const std::array<Vec3,3> market{{{.59f,.42f,.29f},{.51f,.47f,.39f},{.43f,.47f,.40f}}};
  p.body = surface(city[id%city.size()],id%3 == 1 ? TextureSlot::Brick : TextureSlot::Concrete,.78f);
  if (d == District::Ridge) p.body = surface({.40f,.43f,.40f},TextureSlot::Brick,.82f);
  if (d == District::Ashcourt) p.body = surface(market[id%market.size()],TextureSlot::Brick,.83f);
  if (d == District::NorthQuay) p.body = surface(id%2 ? Vec3{.43f,.45f,.42f} : Vec3{.37f,.43f,.47f},TextureSlot::Metal,.63f,.24f);
  p.stone = surface({.64f,.61f,.54f},TextureSlot::Concrete,.74f);
  p.frame = surface({.19f,.22f,.22f},TextureSlot::Metal,.43f,.45f);
  p.roof = surface({.25f,.29f,.29f},TextureSlot::Metal,.66f,.3f);
  if (d == District::Ashcourt) p.roof = surface(id%2 ? Vec3{.37f,.27f,.22f} : Vec3{.29f,.33f,.34f},TextureSlot::Concrete,.83f);
  p.glass = surface({.12f,.21f,.25f},TextureSlot::Glass,.24f,.15f);
  p.glass.world_uv_scale=0; p.glass.emissive=0.f;
  p.occupied = p.glass; p.occupied.albedo={.42f,.38f,.27f}; p.occupied.emissive=.08f;
  p.accent = surface(id%2 ? Vec3{.22f,.32f,.31f} : Vec3{.40f,.27f,.22f},TextureSlot::Metal,.64f,.12f);
  return p;
}

// Each face has a right-handed local facade frame. y is shell-local height;
// positive depth points outdoors, so the glazing can sit behind the masonry.
struct Face {
  Vec3 center, right, normal;
  float width;
  Vec3 point(float u, float y, float depth) const {
    return center + right*u + Vec3{0,y,0} + normal*depth;
  }
};
std::array<Face,4> faces(Bounds b) {
  const float cx=(b.lo.x+b.hi.x)*.5f, cz=(b.lo.z+b.hi.z)*.5f;
  return {{{{cx,0,b.hi.z},{1,0,0},{0,0,1},b.hi.x-b.lo.x},
           {{b.hi.x,0,cz},{0,0,-1},{1,0,0},b.hi.z-b.lo.z},
           {{cx,0,b.lo.z},{-1,0,0},{0,0,-1},b.hi.x-b.lo.x},
           {{b.lo.x,0,cz},{0,0,1},{-1,0,0},b.hi.z-b.lo.z}}};
}
void panel(Mesh& mesh, const Face& f, float u0, float u1, float y0, float y1,
           float depth=0.f, Vec3 color=white) {
  if (u1-u0 < .0001f || y1-y0 < .0001f) return;
  quad(mesh,f.point(u0,y0,depth),f.point(u1,y0,depth),
       f.point(u1,y1,depth),f.point(u0,y1,depth),color);
}
void face_box(Mesh& mesh,const Face& f,float u,float y,float depth,
              float width,float height,float thickness) {
  const auto c=f.point(u,y,depth);
  box(mesh,c,{std::fabs(f.right.x)*width+std::fabs(f.normal.x)*thickness,
              height,std::fabs(f.right.z)*width+std::fabs(f.normal.z)*thickness});
}

struct Batch {
  Mesh shell, stone, frame, roof, glass, occupied, accent, detail;
  Mesh stone_lod;
  std::size_t windows{};
};

void window(Batch& b,const Face& f,float u,float y,float w,float h,bool lit,
            bool industrial) {
  const float x0=u-w*.5f,x1=u+w*.5f,y0=y-h*.5f,y1=y+h*.5f;
  constexpr float inset=-.16f, front=.065f, frame=.085f;
  // Four actual reveal faces bridge the cutout to recessed, opaque glazing.
  quad(b.stone,f.point(x0,y0,0),f.point(x0,y0,inset),f.point(x0,y1,inset),f.point(x0,y1,0));
  quad(b.stone,f.point(x1,y0,inset),f.point(x1,y0,0),f.point(x1,y1,0),f.point(x1,y1,inset));
  quad(b.stone,f.point(x0,y0,inset),f.point(x0,y0,0),f.point(x1,y0,0),f.point(x1,y0,inset));
  quad(b.stone,f.point(x0,y1,0),f.point(x0,y1,inset),f.point(x1,y1,inset),f.point(x1,y1,0));
  // Broad lintels and dark painted frames establish real window scale.
  panel(b.frame,f,x0,x0+frame,y0,y1,front);
  panel(b.frame,f,x1-frame,x1,y0,y1,front);
  panel(b.frame,f,x0+frame,x1-frame,y0,y0+frame,front);
  panel(b.frame,f,x0+frame,x1-frame,y1-frame,y1,front);
  panel(b.glass,f,x0,x1,y0,y1,inset);
  if (lit) {
    // A small occupied pane makes night streets varied without luminous walls.
    panel(b.occupied,f,x0+frame,x1-frame,y0+frame,y1-frame,inset+.006f);
  }
  // Projecting sill: top, lip, underside only. Tiny end faces are invisible
  // at play distance and do not justify extra triangles on a city-wide pass.
  const float sw=.12f;
  quad(b.stone,f.point(x0-sw,y0-.08f,0),f.point(x0-sw,y0-.08f,.21f),f.point(x1+sw,y0-.08f,.21f),f.point(x1+sw,y0-.08f,0));
  panel(b.stone,f,x0-sw,x1+sw,y0-.16f,y0-.08f,.21f);
  quad(b.stone,f.point(x0-sw,y0-.16f,.21f),f.point(x0-sw,y0-.16f,0),f.point(x1+sw,y0-.16f,0),f.point(x1+sw,y0-.16f,.21f));
  panel(b.detail,f,u-.025f,u+.025f,y0+frame,y1-frame,front+.006f);
  if (industrial || h>1.5f) panel(b.detail,f,x0+frame,x1-frame,y-.025f,y+.025f,front+.008f);
  ++b.windows;
}

void facade(Batch& b,const Face& f,Bounds bounds_,District district_,unsigned id,int face_index) {
  const float bottom=bounds_.lo.y,top=bounds_.hi.y,h=top-bottom;
  const bool industrial=district_==District::Ridge || district_==District::NorthQuay;
  const bool shop=district_==District::Ashcourt || (district_==District::Metro && h<10.f);
  const float pitch=industrial ? 3.9f : 3.15f;
  const int columns=std::clamp(static_cast<int>((f.width-.7f)/pitch),2,4);
  const float bay=(f.width-.7f)/columns;
  const float window_width=std::min(industrial ? 2.20f : 1.42f,bay-.65f);
  const float ground_height=industrial ? 3.45f : 3.0f;
  const int upper_rows=std::max(industrial ? 1 : 0,static_cast<int>((h-ground_height-.30f)/2.75f));
  float lower=bottom;
  const int first_row=industrial ? 1 : 0;
  // Continuous masonry strips between rows, separated by discrete window holes.
  for (int row=first_row; row<=upper_rows; ++row) {
    const float cy=bottom+(row==0 ? 1.70f : ground_height+1.05f+(row-1)*2.75f);
    const float wh=row==0 && shop ? 1.90f : industrial ? 1.55f : 1.65f;
    if(cy+wh*.5f>top-.40f) continue;
    const float y0=cy-wh*.5f,y1=cy+wh*.5f;
    panel(b.shell,f,-f.width*.5f,f.width*.5f,lower,y0);
    float left=-f.width*.5f;
    for(int col=0;col<columns;++col) {
      const float u=-f.width*.5f+.35f+bay*(col+.5f);
      const float ww=row==0 && shop ? std::min(2.05f,bay-.5f) : window_width;
      panel(b.shell,f,left,u-ww*.5f,y0,y1);
      const bool lit=((id+static_cast<unsigned>(col*13+row*17+face_index*5))%7)==0;
      window(b,f,u,cy,ww,wh,lit,industrial);
      left=u+ww*.5f;
    }
    panel(b.shell,f,left,f.width*.5f,y0,y1);
    lower=y1;
  }
  panel(b.shell,f,-f.width*.5f,f.width*.5f,lower,top);

  // A raised foundation, floor belt, and real corner pilasters give each mass
  // readable structure; these never project into a traversable building.
  face_box(b.stone,f,0,bottom+.23f,.055f,f.width,.46f,.11f);
  face_box(b.stone,f,0,top-.20f,.08f,f.width+.20f,.20f,.22f);
  face_box(b.stone_lod,f,0,top-.20f,.08f,f.width+.20f,.20f,.22f);
  if (!industrial) face_box(b.stone,f,0,bottom+ground_height,.06f,f.width,.18f,.14f);
  for(float sign : {-1.f,1.f})
    face_box(b.stone,f,sign*(f.width*.5f-.16f),(top+bottom)*.5f,.035f,.25f,h,.09f);
  if(industrial) {
    const int piers=std::max(2,columns);
    for(int i=0;i<=piers;++i) {
      const float u=-f.width*.5f+.28f+(f.width-.56f)*i/piers;
      face_box(b.frame,f,u,bottom+h*.5f,.08f,.12f,h,.18f);
    }
    // Closed loading doors on non-enterable scenery only. The depot and loft
    // use a separate routine that never draws across their working portals.
    if(face_index==0 || face_index==2) {
      const float dw=std::min(3.3f,f.width*.42f),dh=2.7f;
      face_box(b.frame,f,0,bottom+dh*.5f,.035f,dw,dh,.06f);
      for(int i=1;i<9;++i) panel(b.detail,f,-dw*.5f,dw*.5f,bottom+i*.30f,bottom+i*.30f+.028f,.073f);
      face_box(b.stone,f,0,bottom+2.88f,.20f,dw+.45f,.17f,.40f);
    }
  } else if(shop && face_index%2==0) {
    // Deep shop fascias and shallow canvas-toned canopies are high enough for
    // pedestrians and stay close to a shell whose entire footprint is solid.
    const float fascia=bottom+2.95f;
    face_box(b.accent,f,0,fascia,.12f,f.width-.7f,.34f,.20f);
    const float aw=f.width*.72f;
    quad(b.accent,f.point(-aw*.5f,bottom+2.78f,.18f),f.point(-aw*.5f,bottom+2.53f,.80f),
         f.point(aw*.5f,bottom+2.53f,.80f),f.point(aw*.5f,bottom+2.78f,.18f));
    panel(b.accent,f,-aw*.5f,aw*.5f,bottom+2.43f,bottom+2.53f,.80f);
    for(int i=0;i<columns;++i) {
      const float u=-f.width*.5f+.35f+bay*(i+.5f);
      face_box(b.detail,f,u,bottom+1.42f,.17f,.035f,.32f,.04f);
    }
  }
}

void pitched_roof(Mesh& roof, Bounds b, float rise, float overhang) {
  const float x0=b.lo.x-overhang,x1=b.hi.x+overhang;
  const float z0=b.lo.z-overhang,z1=b.hi.z+overhang,zm=(z0+z1)*.5f,y=b.hi.y+.15f;
  quad(roof,{x0,y,z0},{x0,y+rise,zm},{x1,y+rise,zm},{x1,y,z0});
  quad(roof,{x0,y+rise,zm},{x0,y,z1},{x1,y,z1},{x1,y+rise,zm});
  triangle(roof,{x0,y,z1},{x0,y+rise,zm},{x0,y,z0});
  triangle(roof,{x1,y,z0},{x1,y+rise,zm},{x1,y,z1});
}
void roof(Batch& b, Bounds s, District d, unsigned id) {
  const Vec3 size=s.hi-s.lo;
  const float cx=(s.lo.x+s.hi.x)*.5f,cz=(s.lo.z+s.hi.z)*.5f,y=s.hi.y;
  // Close the structural top even if small trim is culled at range.
  quad(b.shell,{s.lo.x,y,s.lo.z},{s.lo.x,y,s.hi.z},{s.hi.x,y,s.hi.z},{s.hi.x,y,s.lo.z});
  if(d!=District::Metro)
    box(b.roof,{cx,y+.07f,cz},{size.x+.48f,.16f,size.z+.48f});
  if(d==District::Ridge || d==District::Ashcourt) {
    const float rise=d==District::Ashcourt ? .85f+float(id%3)*.25f : 1.3f;
    pitched_roof(b.roof,s,rise,.24f);
    box(b.roof,{cx,y+rise+.20f,cz},{size.x+.6f,.12f,.18f});
    if(d==District::Ridge) {
      // Standing seams are real narrow strips following both roof pitches.
      const int seams=std::max(3,static_cast<int>(size.x/1.6f));
      for(int i=1;i<seams;++i) {
        const float x=s.lo.x+size.x*i/seams;
        for(float sign : {-1.f,1.f}) {
          const float edge=cz+sign*(size.z*.5f+.24f);
          const Vec3 a{x-.025f,y+.177f,edge},c{x+.025f,y+rise+.177f,cz};
          if(sign<0) quad(b.detail,a,{x-.025f,c.y,cz},c,{x+.025f,a.y,edge});
          else quad(b.detail,{x-.025f,c.y,cz},a,{x+.025f,a.y,edge},c);
        }
      }
      box(b.frame,{cx,y+rise+.45f,cz},{size.x*.40f,.55f,.8f});
      box(b.roof,{cx,y+rise+.76f,cz},{size.x*.45f,.12f,1.05f});
      for(float sign : {-1.f,1.f}) {
        const Face vent{{cx,0,cz+sign*.405f},{sign,0,0},{0,0,sign},size.x*.4f};
        for(int i=0;i<3;++i) panel(b.detail,vent,-size.x*.19f,size.x*.19f,y+rise+.25f+i*.14f,y+rise+.29f+i*.14f,.01f);
      }
    } else {
      const float chimney_x=cx+size.x*.27f,chimney_z=cz-size.z*.19f;
      box(b.stone,{chimney_x,y+1.20f,chimney_z},{.65f,1.65f,.70f});
      box(b.roof,{chimney_x,y+2.05f,chimney_z},{.85f,.16f,.9f});
    }
  } else if(d==District::NorthQuay) {
    // Sawtooth roof, with northlight glazing on each steep tooth face.
    const int teeth=3;
    const float step=size.z/teeth;
    for(int tooth=0;tooth<teeth;++tooth) {
      const float z0=s.lo.z+tooth*step,z1=z0+step;
      quad(b.roof,{s.lo.x-.15f,y+.15f,z0},{s.lo.x-.15f,y+1.45f,z1-.28f},
                  {s.hi.x+.15f,y+1.45f,z1-.28f},{s.hi.x+.15f,y+.15f,z0});
      quad(b.glass,{s.lo.x,y+.15f,z1},{s.hi.x,y+.15f,z1},
                   {s.hi.x,y+1.45f,z1-.28f},{s.lo.x,y+1.45f,z1-.28f});
      triangle(b.roof,{s.lo.x,y+.15f,z1},{s.lo.x,y+1.45f,z1-.28f},{s.lo.x,y+.15f,z0});
      triangle(b.roof,{s.hi.x,y+.15f,z0},{s.hi.x,y+1.45f,z1-.28f},{s.hi.x,y+.15f,z1});
      box(b.frame,{cx,y+1.47f,z1-.28f},{size.x+.4f,.12f,.16f});
      for(int i=1;i<5;++i) box(b.detail,{s.lo.x+size.x*i/5.f,y+.83f,z1-.14f},{.06f,1.3f,.31f});
    }
  } else {
    for(const auto& f : faces(s)) {
      face_box(b.stone,f,0,y+.20f,-.03f,f.width+.17f,.40f,.22f);
      face_box(b.roof,f,0,y+.44f,-.03f,f.width+.32f,.10f,.36f);
    }
    const bool tower=size.y>10.f;
    if(tower) {
      // Setback service crown changes the skyline, not just its texture.
      box(b.stone,{cx,y+.75f,cz},{size.x*.53f,1.5f,size.z*.50f});
      box(b.roof,{cx,y+1.56f,cz},{size.x*.56f,.16f,size.z*.53f});
      box(b.frame,{cx+size.x*.08f,y+1.99f,cz},{1.20f,.7f,.95f});
    }
    const float px=cx-size.x*.26f,pz=cz-size.z*.25f,py=y+.45f;
    box(b.frame,{px,py,pz},{1.1f,.8f,1.35f});
    box(b.roof,{px,py+.44f,pz},{1.25f,.10f,1.48f});
    for(int i=0;i<4;++i) box(b.detail,{px-.40f+i*.26f,py+.51f,pz},{.10f,.045f,1.17f});
    if(id%3==0) {
      box(b.stone,{cx+size.x*.27f,y+.6f,cz+size.z*.22f},{.64f,1.2f,.7f});
      box(b.roof,{cx+size.x*.27f,y+1.25f,cz+size.z*.22f},{.84f,.13f,.90f});
    }
  }
}

void add_batch(Scene& scene,const Entity& source,const char* suffix,Mesh mesh,
               const Material& material,bool detail,WorldArchitectureStats& stats,
               const char* tag=nullptr,Mesh lod={}) {
  if(mesh.indices.empty()) return;
  Entity part;
  part.name="Architecture."+source.name+"."+suffix;
  part.transform=source.transform;
  part.mesh=scene.add_mesh(std::move(mesh));
  part.material=material; part.detail=detail;
  if(tag) part.tag=tag;
  if(!lod.indices.empty()) part.lod_mesh=scene.add_mesh(std::move(lod));
  stats.triangles+=part.mesh->indices.size()/3;
  ++stats.added_entities;
  scene.add_entity(std::move(part));
}

void hide_strips(Scene& scene,const Entity& source,Bounds b,WorldArchitectureStats& stats) {
  // Legacy strip ownership is implicit. Match exact face positions, height and
  // orientation in the original world builder instead of blanket name removal.
  if(source.transform.rotation_euler.x!=0 || source.transform.rotation_euler.y!=0 ||
     source.transform.rotation_euler.z!=0) return;
  const auto p=source.transform.position,s=source.transform.scale;
  const float cx=p.x+(b.lo.x+b.hi.x)*.5f*s.x,cz=p.z+(b.lo.z+b.hi.z)*.5f*s.z;
  for(auto& e : scene.entities()) {
    if(!e.visible || e.solid || e.material.textures || e.tag!="window") continue;
    const auto w=e.transform.position;
    if(w.y<=p.y+b.lo.y*s.y || w.y>=p.y+b.hi.y*s.y) continue;
    const bool z=numbered(e.name,"WinZ") && std::fabs(w.x-cx)<.02f &&
                 std::fabs(w.z-(p.z+b.hi.z*s.z+.06f))<.025f;
    const bool x=numbered(e.name,"WinX") && std::fabs(w.z-cz)<.02f &&
                 std::fabs(w.x-(p.x+b.hi.x*s.x+.06f))<.025f;
    if(z || x) { e.visible=false; ++stats.hidden_window_strips; }
  }
}

bool protected_material(const Material& m) {
  return bool(m.textures) || m.texture==TextureSlot::Glass || m.texture==TextureSlot::Water ||
         m.transmission>0.f || m.alpha_blend || m.alpha_cutoff>=0.f || m.opacity<1.f || m.emissive>0.f;
}

void landmark(Scene& scene,const char* roof_name,const char* left_name,
              const char* right_name,bool depot,WorldArchitectureStats& stats) {
  const Entity* original=scene.find_by_name(roof_name);
  if(!original || !original->mesh || scene.find_by_name("Architecture."+std::string(roof_name)+".Roof")) return;
  const Entity source=*original;
  Bounds s;
  if(!bounds(source.mesh,s)) return;
  const float width=s.hi.x-s.lo.x,depth=s.hi.z-s.lo.z,top=s.hi.y;
  if(width<3.f || depth<3.f) return;
  auto p=palette(depot?District::NorthQuay:District::Ashcourt,stable_id(roof_name));
  Mesh roof_,stone,frame,detail;
  // All roof work is above the existing roof. Ground-level exterior posts are
  // attached only to the two explicit solid front-wall sections, never the gap.
  if(depot) {
    Bounds raised=s; raised.lo.y=raised.hi.y=top;
    pitched_roof(roof_,raised,.95f,.08f);
    box(frame,{0,top+1.16f,0},{width*.52f,.4f,.75f});
    box(roof_,{0,top+1.42f,0},{width*.57f,.12f,1.f});
    for(int i=0;i<5;++i) box(detail,{-width*.21f+i*width*.105f,top+1.16f,.40f},{.05f,.32f,.03f});
  } else {
    for(const auto& f:faces(s)) {
      face_box(stone,f,0,top+.18f,-.08f,f.width,.36f,.20f);
      face_box(roof_,f,0,top+.39f,-.08f,f.width+.10f,.09f,.30f);
    }
    box(stone,{-width*.30f,top+.65f,-depth*.2f},{.8f,1.30f,.80f});
    box(roof_,{-width*.30f,top+1.35f,-depth*.2f},{1.f,.12f,1.f});
  }
  // Wall-relative coordinates are converted to the roof's local frame. Current
  // enterable landmarks are axis-aligned and unit-scaled; otherwise skip posts.
  if(source.transform.rotation_euler.x==0 && source.transform.rotation_euler.y==0 &&
     source.transform.rotation_euler.z==0 && source.transform.scale.x==1 &&
     source.transform.scale.y==1 && source.transform.scale.z==1) {
    for(const char* name : {left_name,right_name}) {
      const auto* wall=scene.find_by_name(name);
      Bounds wb;
      if(!wall || !bounds(wall->mesh,wb) || wall->transform.rotation_euler.y!=0 ||
         wall->transform.scale.x!=1 || wall->transform.scale.y!=1 || wall->transform.scale.z!=1) continue;
      const Vec3 delta=wall->transform.position-source.transform.position;
      const float z=delta.z+wb.hi.z+.07f,wy=delta.y+(wb.lo.y+wb.hi.y)*.5f;
      for(float side : {-1.f,1.f}) {
        const float x=delta.x+(wb.lo.x+wb.hi.x)*.5f+side*((wb.hi.x-wb.lo.x)*.5f-.18f);
        box(stone,{x,wy,z},{.22f,wb.hi.y-wb.lo.y,.16f});
      }
      // High front-wall lintel accents stay wholly within each wall section.
      box(frame,{delta.x,delta.y+wb.hi.y-.55f,z+.025f},{wb.hi.x-wb.lo.x-.45f,.18f,.14f});
    }
  }
  add_batch(scene,source,"Roof",std::move(roof_),p.roof,false,stats);
  add_batch(scene,source,"Stone",std::move(stone),p.stone,false,stats);
  add_batch(scene,source,"Metal",std::move(frame),p.frame,false,stats);
  add_batch(scene,source,"Fine",std::move(detail),p.frame,true,stats);
  ++stats.landmark_exteriors;
}
}  // namespace

WorldArchitectureStats upgrade_world_architecture(fury::Scene& scene) {
  WorldArchitectureStats stats;
  const std::size_t original_count=scene.entities().size();
  for(std::size_t index=0;index<original_count;++index) {
    const Entity original=scene.entities()[index];
    const District d=district(original.name);
    if(d==District::None) continue;
    ++stats.eligible_shells;
    if(original.name=="Bldg3" || !original.solid || !original.mesh || !original.visible ||
       (!original.tag.empty() && original.tag!="scenery" && original.tag!="building") ||
       protected_material(original.material)) { ++stats.protected_shells; continue; }
    if(scene.find_by_name("Architecture."+original.name+".Roof")) continue;
    Bounds s;
    if(!bounds(original.mesh,s)) continue;
    const Vec3 size=s.hi-s.lo;
    // Audited scenery boxes are metres across, not arbitrary custom imports.
    if(size.x<4.f || size.x>24.f || size.z<4.f || size.z>24.f || size.y<3.5f || size.y>30.f) continue;
    Batch batch;
    const unsigned id=stable_id(original.name);
    const auto p=palette(d,id);
    int face_index=0;
    for(const auto& f:faces(s)) facade(batch,f,s,d,id,face_index++);
    roof(batch,s,d,id);
    stats.window_bays+=batch.windows;
    // Keep every gameplay field on the original entity. Replacing just its
    // decorative mesh/material leaves physics and script name lookups intact.
    auto& shell=scene.entities()[index];
    shell.mesh=scene.add_mesh(std::move(batch.shell));
    shell.material=p.body;
    // The facade cutouts remain inexpensive at range. Keeping them prevents
    // depth-fighting between a solid LOD box and recessed far-window planes.
    shell.lod_mesh=nullptr;
    stats.triangles+=shell.mesh->indices.size()/3;
    hide_strips(scene,original,s,stats);
    add_batch(scene,original,"Roof",std::move(batch.roof),p.roof,false,stats);
    add_batch(scene,original,"Stone",std::move(batch.stone),p.stone,false,stats,nullptr,std::move(batch.stone_lod));
    add_batch(scene,original,"Frames",std::move(batch.frame),p.frame,false,stats);
    add_batch(scene,original,"Glass",std::move(batch.glass),p.glass,false,stats);
    add_batch(scene,original,"Occupied",std::move(batch.occupied),p.occupied,false,stats,"window");
    add_batch(scene,original,"Frontage",std::move(batch.accent),p.accent,false,stats);
    add_batch(scene,original,"Fine",std::move(batch.detail),p.frame,true,stats);
    ++stats.upgraded_shells;
    if(d==District::Metro) ++stats.metro_shells;
    if(d==District::Ridge) ++stats.ridge_shells;
    if(d==District::Ashcourt) ++stats.ashcourt_shells;
    if(d==District::NorthQuay) ++stats.north_quay_shells;
  }
  landmark(scene,"DepotRoof","DepotWallSL","DepotWallSR",true,stats);
  landmark(scene,"LoftRoof","LoftWallSL","LoftWallSR",false,stats);
  return stats;
}
}  // namespace vaultline
