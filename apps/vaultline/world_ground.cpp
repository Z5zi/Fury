#include "world_ground.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
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

constexpr const char* kMarker = "WorldGround.Version1";
constexpr float kEpsilon = 0.0001f;
struct Rect { float x0, x1, z0, z1; };
bool valid(Rect r) { return r.x1-r.x0>kEpsilon && r.z1-r.z0>kEpsilon; }
bool overlaps(Rect a, Rect b) {
  return a.x0<b.x1-kEpsilon && a.x1>b.x0+kEpsilon &&
      a.z0<b.z1-kEpsilon && a.z1>b.z0+kEpsilon;
}
Rect expanded(Rect r, float d) { return {r.x0-d,r.x1+d,r.z0-d,r.z1+d}; }
std::vector<Rect> subtract(const std::vector<Rect>& pieces, Rect cut) {
  std::vector<Rect> result;
  for (Rect p : pieces) {
    if (!overlaps(p,cut)) { result.push_back(p); continue; }
    const float x0=std::max(p.x0,cut.x0), x1=std::min(p.x1,cut.x1);
    const float z0=std::max(p.z0,cut.z0), z1=std::min(p.z1,cut.z1);
    for (Rect q : {Rect{p.x0,x0,p.z0,p.z1}, Rect{x1,p.x1,p.z0,p.z1},
                   Rect{x0,x1,p.z0,z0}, Rect{x0,x1,z1,p.z1}})
      if (valid(q)) result.push_back(q);
  }
  return result;
}
std::vector<Rect> clipped(Rect r, const std::vector<Rect>& cuts) {
  std::vector<Rect> out{r};
  for (Rect cut : cuts) out=subtract(out,cut);
  return out;
}

void quad(Mesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n, Vec3 color,
          float uv_scale=0.5f) {
  const auto base=static_cast<std::uint32_t>(m.vertices.size());
  for (Vec3 p : {a,b,c,d}) {
    fury::Vertex v; v.position=p; v.normal=n; v.color=color;
    v.uv=std::abs(n.y)>.5f ? fury::Vec2{p.x*uv_scale,p.z*uv_scale} :
        fury::Vec2{(std::abs(n.x)>.5f?p.z:p.x)*uv_scale,p.y*uv_scale};
    m.vertices.push_back(v);
  }
  // Position order differs between XZ paving and vertical retaining faces.
  // Keep geometric facing consistent with the explicit shading normal.
  if(fury::dot(fury::cross(b-a,c-a),n)<0.f) {
    for(std::uint32_t i:{0u,2u,1u,0u,3u,2u}) m.indices.push_back(base+i);
  } else {
    for(std::uint32_t i:{0u,1u,2u,0u,2u,3u}) m.indices.push_back(base+i);
  }
}
void plane(Mesh& m, Rect r, float y, Vec3 color, float uv_scale=.5f) {
  if (!valid(r)) return;
  quad(m,{r.x0,y,r.z0},{r.x1,y,r.z0},{r.x1,y,r.z1},{r.x0,y,r.z1},
       {0,1,0},color,uv_scale);
}
void box(Mesh& m, Rect r, float low, float high, Vec3 color) {
  plane(m,r,high,color);
  quad(m,{r.x0,low,r.z0},{r.x1,low,r.z0},{r.x1,high,r.z0},{r.x0,high,r.z0},
       {0,0,-1},color*.80f);
  quad(m,{r.x1,low,r.z1},{r.x0,low,r.z1},{r.x0,high,r.z1},{r.x1,high,r.z1},
       {0,0,1},color*.87f);
  quad(m,{r.x0,low,r.z1},{r.x0,low,r.z0},{r.x0,high,r.z0},{r.x0,high,r.z1},
       {-1,0,0},color*.84f);
  quad(m,{r.x1,low,r.z0},{r.x1,low,r.z1},{r.x1,high,r.z1},{r.x1,high,r.z0},
       {1,0,0},color*.9f);
}

enum Surface { Stone, WarmStone, Asphalt, WhitePaint, YellowPaint, Joint, Wood, Metal, Count };
const char* const kSurfaceNames[]={"Stone","WarmStone","Asphalt","WhitePaint","YellowPaint","Joint","Wood","Metal"};
const char* const kDistrictNames[]={"Metro","Ridge","Ashcourt","Depot","Loft","NorthQuay"};
Material material(Surface s) {
  Material m; m.roughness=.86f;
  switch(s) {
    case Stone: case WarmStone: m.texture=TextureSlot::Concrete; m.world_uv_scale=.6f; break;
    case Asphalt: m.texture=TextureSlot::Asphalt; m.world_uv_scale=.33f; break;
    case Wood: m.texture=TextureSlot::Wood; m.detail_texture=TextureSlot::Wood;
      m.detail_use_mesh_uvs=true; m.roughness=.79f; break;
    case Metal: m.texture=TextureSlot::Metal; m.metallic=.72f; m.roughness=.65f; break;
    default: break;
  }
  return m;
}
struct Batch {
  Mesh mesh;
  int district;
  Surface surface;
  bool detail;
};
struct Builder {
  WorldGroundStats& stats;
  std::vector<Batch> batches;
  std::vector<Rect> obstacles;
  std::vector<Rect> water;
  Mesh& mesh(int district, Surface surface, bool detail=false) {
    for (auto& b:batches)
      if(b.district==district && b.surface==surface && b.detail==detail) return b.mesh;
    batches.push_back({{},district,surface,detail}); return batches.back().mesh;
  }
  void feature(int district) { ++stats.district_features[static_cast<std::size_t>(district)]; }
  void patch(int district, Surface s, Rect r, float y, Vec3 color,
             bool detail=false, bool avoid_buildings=true) {
    auto pieces=clipped(r,water);
    if(avoid_buildings) for(Rect o:obstacles) pieces=subtract(pieces,o);
    for(Rect p:pieces) plane(mesh(district,s,detail),p,y,color);
    if(!pieces.empty()) feature(district);
  }
  void stripe(int district, Rect r, float y, bool yellow=false) {
    auto pieces=clipped(r,obstacles);
    for(Rect p:pieces) for(Rect q:clipped(p,water)) {
      plane(mesh(district,yellow?YellowPaint:WhitePaint,true),q,y,
            yellow?Vec3{.70f,.53f,.19f}:Vec3{.71f,.70f,.63f});
      ++stats.road_markings;
    }
    feature(district);
  }
  void curb(int district, Rect r, float top=.15f) {
    auto pieces=clipped(r,obstacles);
    for(Rect p:pieces) for(Rect q:clipped(p,water)) {
      box(mesh(district,Stone),q,.015f,top,{.51f,.51f,.46f}); ++stats.curb_segments;
    }
  }
  void emit(Scene& scene) {
    for(auto& b:batches) {
      if(b.mesh.indices.empty()) continue;
      Vec3 lo{1e6f,1e6f,1e6f}, hi{-1e6f,-1e6f,-1e6f};
      for(const auto& v:b.mesh.vertices) {
        lo.x=std::min(lo.x,v.position.x); lo.y=std::min(lo.y,v.position.y); lo.z=std::min(lo.z,v.position.z);
        hi.x=std::max(hi.x,v.position.x); hi.y=std::max(hi.y,v.position.y); hi.z=std::max(hi.z,v.position.z);
      }
      const Vec3 center=(lo+hi)*.5f;
      for(auto& v:b.mesh.vertices) v.position=v.position-center;
      Entity e; e.name=std::string("WorldGround.")+kDistrictNames[b.district]+"."+
          kSurfaceNames[b.surface]+(b.detail?".Detail":".Structure");
      e.transform.position=center; e.material=material(b.surface); e.detail=b.detail;
      e.tag=b.surface==Asphalt?"asphalt":"world_ground";
      stats.added_triangles+=b.mesh.indices.size()/3;
      e.mesh=scene.add_mesh(std::move(b.mesh)); scene.add_entity(std::move(e));
      ++stats.added_entities;
    }
  }
};

bool numbered(const std::string& name, const char* prefix) {
  const std::string p=prefix;
  if(name.size()<=p.size() || name.compare(0,p.size(),p)!=0) return false;
  return std::all_of(name.begin()+static_cast<std::ptrdiff_t>(p.size()),name.end(),
                     [](char c){return c>='0'&&c<='9';});
}
Rect footprint(const Entity& e) {
  const auto c=e.transform.position+e.collider.center;
  // Scene::collect_solids intentionally ignores yaw; use exactly its XZ domain.
  const float x=std::abs(e.collider.half_extents.x*e.transform.scale.x);
  const float z=std::abs(e.collider.half_extents.z*e.transform.scale.z);
  return {c.x-x,c.x+x,c.z-z,c.z+z};
}
int building_district(const Entity& e) {
  if(numbered(e.name,"Bldg")) return 0;
  if(numbered(e.name,"RidgeBldg")) return 1;
  if(numbered(e.name,"AshShop")) return 2;
  if(numbered(e.name,"NQWarehouse")) return 5;
  return -1;
}

void frontage(Builder& b, Rect core, int district, bool wide=false) {
  const Rect out=expanded(core,wide?2.8f:2.1f);
  const Surface s=district==2?WarmStone:Stone;
  const Vec3 color=district==2?Vec3{.48f,.42f,.34f}:Vec3{.43f,.44f,.42f};
  for(Rect p:subtract({out},core)) b.patch(district,s,p,.095f,color);
  // Long stone edging rather than a fake city-wide lattice. Every frontage has
  // a 4.4 m flush opening on both approaches; entrances and missions stay open.
  const float cx=(core.x0+core.x1)*.5f, cz=(core.z0+core.z1)*.5f;
  for(float z:{out.z0,out.z1}) {
    b.curb(district,{out.x0,cx-2.2f,z-.09f,z+.09f});
    b.curb(district,{cx+2.2f,out.x1,z-.09f,z+.09f});
  }
  for(float x:{out.x0,out.x1}) {
    b.curb(district,{x-.09f,x+.09f,out.z0,cz-2.2f});
    b.curb(district,{x-.09f,x+.09f,cz+2.2f,out.z1});
  }
  // Sparse, correctly scaled slab joints confined to the actual walk surface.
  for(float x=out.x0+1.7f;x<out.x1;x+=2.2f) {
    b.patch(district,Joint,{x,x+.025f,out.z0,core.z0},.099f,{.24f,.25f,.23f},true);
    b.patch(district,Joint,{x,x+.025f,core.z1,out.z1},.099f,{.24f,.25f,.23f},true);
  }
  ++b.stats.frontage_walks;
}

void road_dashes(Builder& b,int d,Rect r,float y,bool along_x,bool yellow=false) {
  if(along_x) {
    const float z=(r.z0+r.z1)*.5f;
    for(float x=r.x0+1.f;x+2.5f<r.x1;x+=6.f) b.stripe(d,{x,x+2.5f,z-.065f,z+.065f},y,yellow);
  } else {
    const float x=(r.x0+r.x1)*.5f;
    for(float z=r.z0+1.f;z+2.5f<r.z1;z+=6.f) b.stripe(d,{x-.065f,x+.065f,z,z+2.5f},y,yellow);
  }
}

// Emit only the union's perimeter, never the internal edges of its rectangle
// tessellation. This is important around foundations protected from the cutout.
void waterfront(Builder& b) {
  for(const Rect& r:b.water) for(int side=0;side<4;++side) {
    const bool xside=side<2;
    const float fixed=side==0?r.x0:side==1?r.x1:side==2?r.z0:r.z1;
    if((xside && (fixed<=-160.f+kEpsilon||fixed>=160.f-kEpsilon)) ||
       (!xside && (fixed<=-130.f+kEpsilon||fixed>=130.f-kEpsilon))) continue;
    std::vector<std::pair<float,float>> spans{{xside?r.z0:r.x0,xside?r.z1:r.x1}};
    for(const Rect& q:b.water) {
      const float opposing=side==0?q.x1:side==1?q.x0:side==2?q.z1:q.z0;
      if(std::abs(opposing-fixed)>kEpsilon) continue;
      const float q0=xside?q.z0:q.x0, q1=xside?q.z1:q.x1;
      std::vector<std::pair<float,float>> next;
      for(auto p:spans) {
        if(q1<=p.first+kEpsilon || q0>=p.second-kEpsilon) { next.push_back(p); continue; }
        if(q0>p.first+kEpsilon) next.emplace_back(p.first,q0);
        if(q1<p.second-kEpsilon) next.emplace_back(q1,p.second);
      }
      spans=std::move(next);
    }
    for(auto p:spans) {
      const float center=(p.first+p.second)*.5f;
      const float x=xside?fixed:center, z=xside?center:fixed;
      const int d=x>74.f?1:z>100.f?5:x>23.f?4:0;
      const float dir=(side==0||side==2)?-1.f:1.f;
      const float outer=fixed+dir*.42f;
      Rect edge=xside?Rect{std::min(fixed,outer),std::max(fixed,outer),p.first,p.second}:
                       Rect{p.first,p.second,std::min(fixed,outer),std::max(fixed,outer)};
      box(b.mesh(d,Stone),edge,-2.1f,.12f,{.44f,.46f,.43f});
      // A dark tide band is real geometry on the retaining face, not a water
      // overlay that would hide the animated normal/fresnel shader.
      const float face=fixed-dir*.003f;
      if(xside) quad(b.mesh(d,Joint,true),{face,-.7f,p.first},{face,-.7f,p.second},
          {face,-.43f,p.second},{face,-.43f,p.first},{-dir,0,0},{.16f,.22f,.19f});
      else quad(b.mesh(d,Joint,true),{p.first,-.7f,face},{p.second,-.7f,face},
          {p.second,-.43f,face},{p.first,-.43f,face},{0,0,-dir},{.16f,.22f,.19f});
      ++b.stats.waterfront_edges; b.feature(d);
    }
  }
}

void pier(Builder& b,int d,Rect deck,float y) {
  // Planks run across each deck; the longitudinal grain uses mesh UVs.
  int i=0;
  for(float x=deck.x0;x<deck.x1-.03f;x+=.42f,++i) {
    const float t=static_cast<float>((i*17)%7)/6.f;
    plane(b.mesh(d,Wood),{x,std::min(x+.405f,deck.x1),deck.z0,deck.z1},y,
          {.36f+t*.07f,.29f+t*.05f,.20f+t*.035f},.3f);
    ++b.stats.pier_boards;
  }
  // Pilings are below/outboard of the existing deck, never standing in its path.
  for(float x=deck.x0+1.f;x<deck.x1;x+=5.8f) for(float z:{deck.z0+.18f,deck.z1-.18f}) {
    box(b.mesh(d,Wood),{x-.20f,x+.20f,z-.20f,z+.20f},-2.1f,y-.1f,{.25f,.23f,.19f});
    box(b.mesh(d,Metal,true),{x-.205f,x+.205f,z-.205f,z+.205f},-.05f,.1f,{.22f,.25f,.24f});
  }
  b.feature(d);
}
}  // namespace

WorldGroundStats upgrade_world_ground(Scene& scene) {
  WorldGroundStats stats;
  if(scene.find_by_name(kMarker)) { stats.already_applied=true; return stats; }
  Entity* original_ground=scene.find_by_name("StreetGrid");
  if(!original_ground || !original_ground->mesh || original_ground->solid) return stats;

  Builder b{stats,{},{},{}};
  // All rectangles lie inside an existing animated water plane. Harbor's water
  // originally also lies under the loft and several city blocks: those stay dry.
  const std::array<Rect,7> basin{{{-24,8,55,74},{-24,-14,42,55},
      {24,65,60,74},{49,65,38,60},{78,128,26,52},{-14,42,116,130},
      {-14,12.5f,40.7f,51}}};
  std::vector<Rect> dry{{12.5f,23.5f,46,94},{23.5f,49,56.5f,60},
      {-14,14,51,55}, // occupied headland behind the western pier
      {-14,40,38,40},{84,89,23,29}, // unobstructed harbor / ridge pier approaches
      {-2.5f,2.5f,9.5f,14.5f},{89.5f,94.5f,5.5f,10.5f},
      {-88.5f,-83.5f,45.5f,50.5f},{55.5f,60.5f,-42.5f,-37.5f},
      {39.5f,44.5f,51.5f,56.5f},{15.5f,20.5f,85.5f,90.5f}};
  struct Front { Rect rect; int district; };
  std::vector<Front> frontages;
  for(const Entity& e:scene.entities()) {
    if(!e.solid) continue;
    const Rect r=footprint(e);
    const float bottom=e.transform.position.y+e.collider.center.y-
                       std::abs(e.collider.half_extents.y*e.transform.scale.y);
    // Structural bridge piles descend below water and must not become islands.
    // Everything else meeting ground, including authored prop colliders, gets
    // support. Existing pier tops provide support for raised crates/beacons.
    if(bottom>=-.32f && bottom<.30f) {
      dry.push_back(expanded(r,.65f)); ++stats.protected_solid_footprints;
    }
    const int d=building_district(e);
    if(d>=0) { frontages.push_back({r,d}); b.obstacles.push_back(r); }
  }
  const Front hero_fronts[]={{{-9.6f,9.6f,-17.6f,-2.4f},0},
      {{-28.5f,-15.5f,2.5f,13.5f},0},{{49.5f,66.5f,-54.5f,-41.5f},3},
      {{36.1f,47.9f,47.1f,56.9f},4}};
  for(auto h:hero_fronts) {
    frontages.push_back(h); b.obstacles.push_back(h.rect); dry.push_back(expanded(h.rect,.8f));
  }
  for(Rect r:basin) {
    auto parts=clipped(r,dry); b.water.insert(b.water.end(),parts.begin(),parts.end());
  }
  stats.water_apertures=b.water.size();
  for(Rect r:b.water) stats.water_aperture_area+=(r.x1-r.x0)*(r.z1-r.z0);
  auto exposed=subtract(b.water,{-9,39,40,48});
  exposed=subtract(exposed,{87,115,26.5f,33.5f});
  for(Rect r:exposed) stats.exposed_water_area+=(r.x1-r.x0)*(r.z1-r.z0);

  // Replacement is visual only. Scene::collect_solids and the world's ground
  // movement clamp are unchanged. Separate rectangles expose the real water.
  Mesh land;
  for(Rect r:clipped({-160,160,-130,130},b.water))
    plane(land,r,0,{.255f,.265f,.255f},.22f);
  stats.terrain_triangles=land.indices.size()/3;
  original_ground->mesh=scene.add_mesh(std::move(land));
  original_ground->material=material(Asphalt);
  ++stats.replaced_surfaces;
  Mesh* empty=nullptr;
  for(Entity& e:scene.entities()) {
    if(e.solid) continue;
    if(e.name=="Sidewalk" || e.name=="SidewalkNS") {
      if(!empty) empty=scene.add_mesh(Mesh{});
      e.mesh=empty; ++stats.replaced_surfaces;
    } else if(e.name=="BankPlaza" || e.name=="RidgePlaza" ||
              e.name=="AshcourtPlaza" || e.name=="DepotYard" || e.name=="NorthQuayPlaza") {
      e.material=material(e.name=="AshcourtPlaza"?WarmStone:Stone);
      ++stats.replaced_surfaces;
    }
  }

  for(auto f:frontages) frontage(b,f.rect,f.district,f.district==4);
  // Harbor Metro: a legible bank forecourt, actual clear road segments and a
  // pedestrian crossing. Surface patches never paint across building interiors.
  b.patch(0,Stone,{-14.5f,14.5f,-2.4f,2.5f},.076f,{.47f,.47f,.43f});
  constexpr float metro_asphalt_y=.02f;
  b.patch(0,Asphalt,{-14,43,3.2f,12.5f},metro_asphalt_y,{.23f,.25f,.25f});
  road_dashes(b,0,{-12,41,3.2f,12.5f},.031f,true);
  road_dashes(b,0,{-54,58,-24,-18},.031f,true);
  road_dashes(b,0,{28.5f,33.5f,-18,27},.031f,false);
  // Keep the authored paint just above asphalt without a visibly raised edge.
  for(float z=3.8f;z<12;z+=.95f) b.stripe(0,{-1.8f,1.8f,z,z+.42f},metro_asphalt_y+.001f);
  // Crossing approaches are flat, clear curb breaks rather than steps.
  b.patch(0,Stone,{-2.2f,2.2f,.4f,3.1f},.075f,{.48f,.47f,.41f});
  b.patch(0,Stone,{-2.2f,2.2f,12.7f,15.4f},.075f,{.48f,.47f,.41f});

  // Existing raised connectors keep their authored heights and geometry.
  road_dashes(b,1,{53,87,2.5f,9.5f},.588f,true);
  road_dashes(b,2,{-78,-46,24,32},.389f,true);
  road_dashes(b,3,{23,49,-39.5f,-32.5f},.355f,true);
  road_dashes(b,5,{14,22,65,92},.389f,false);
  road_dashes(b,5,{13,23,51,65},.588f,false);
  for(float z:{3.1f,8.9f}) b.stripe(1,{53,87,z,z+.10f},.588f);

  // Ridge: a street between warehouses, bordered pedestrian apron, and proper
  // weathered timber decking along the waterfront instead of a featureless box.
  b.patch(1,Asphalt,{73,117,4.5f,11.5f},.078f,{.25f,.27f,.27f});
  road_dashes(b,1,{88,116,4.5f,11.5f},.089f,true);
  b.patch(1,Stone,{75,120,23.2f,26},.09f,{.46f,.47f,.43f});
  pier(b,1,{87,115,26.5f,33.5f},.464f);

  // Ashcourt's market has an open warm-stone pedestrian spine between stalls,
  // a contrasting perimeter band, and a flush crossing from the connector.
  b.patch(2,WarmStone,{-91.4f,-87.6f,25.5f,58.5f},.080f,{.53f,.46f,.36f});
  b.patch(2,WarmStone,{-108.7f,-67.3f,46.8f,49.4f},.082f,{.48f,.41f,.33f});
  for(float x=-91.1f;x<-87.6f;x+=.76f) b.stripe(2,{x,x+.37f,25,30.8f},.086f);
  // A perimeter paving band reads at district scale; slab joints stay near-field.
  for(Rect r:subtract({Rect{-109, -67,25,59}},Rect{-107.8f,-68.2f,26.2f,57.8f}))
    b.patch(2,WarmStone,r,.079f,{.37f,.33f,.28f});

  // Depot apron includes legible loading lanes, not parking stripes across the
  // hall or the armored cage. Main entrance x=58 remains completely clear.
  b.patch(3,Asphalt,{45,76,-41.2f,-34.7f},.072f,{.25f,.27f,.27f});
  for(float x:{47.f,50.2f,64.8f,68.f,71.2f,74.4f})
    b.stripe(3,{x,x+.10f,-40.4f,-36.2f},.084f,true);
  b.stripe(3,{45.8f,75,-35.2f,-35.07f},.084f,true);
  b.patch(3,Stone,{55.6f,60.4f,-41.4f,-36.1f},.085f,{.46f,.47f,.43f});

  // Loft's occupied peninsula and pedestrian promenade deliberately bridge the
  // gap to North Quay's road. Its floor and workbench remain untouched.
  b.patch(4,Stone,{23.5f,49,57.6f,60},.095f,{.47f,.47f,.42f});
  b.patch(4,Stone,{47.9f,49,38,57.6f},.095f,{.45f,.46f,.42f});
  for(float x=25;x<48;x+=2.3f)
    b.patch(4,Joint,{x,x+.027f,57.7f,59.7f},.100f,{.27f,.28f,.25f},true);
  pier(b,0,{-9,39,40,48},.514f);

  // North Quay: paved cargo spine, real waterfront edge beyond the sealed
  // container, and sparse service-lane lines that respect every warehouse.
  b.patch(5,Asphalt,{11,20,77,114.7f},.074f,{.26f,.28f,.28f});
  b.patch(5,Stone,{-14,36,114.8f,116},.078f,{.45f,.46f,.43f});
  road_dashes(b,5,{12,20,79,96},.087f,false,true);
  for(float x=-12;x<36;x+=5.5f) b.stripe(5,{x,x+2.5f,115.4f,115.53f},.090f,true);
  b.stripe(5,{13,13.12f,106,114},.087f,true);
  b.stripe(5,{19.4f,19.52f,106,114},.087f,true);

  waterfront(b);
  b.emit(scene);
  for(auto n:stats.district_features) if(n) ++stats.districts;
  Entity marker; marker.name=kMarker; marker.visible=false; marker.tag="world_ground";
  scene.add_entity(std::move(marker)); ++stats.added_entities;
  stats.applied=true;
  return stats;
}

}  // namespace vaultline
