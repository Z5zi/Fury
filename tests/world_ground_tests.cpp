#include "../apps/vaultline/world_ground.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
bool same(Vec3 a,Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
bool same_pose(const Transform& a,const Transform& b) {
  return same(a.position,b.position) && same(a.rotation_euler,b.rotation_euler) && same(a.scale,b.scale);
}
void ground(Scene& s,const char* name,Vec3 p,float width,float depth,TextureSlot texture=TextureSlot::Concrete) {
  Entity e; e.name=name; e.tag=std::string(name)=="StreetGrid"?"asphalt":"original_tag";
  e.mesh=s.add_mesh(make_plane(width,depth,{.4f,.4f,.4f})); e.transform.position=p;
  e.material.texture=texture; e.material.uv_scroll_u=.05f; e.material.roughness=.18f;
  s.add_entity(std::move(e));
}
void solid(Scene& s,const std::string& name,Vec3 p,Vec3 size) {
  Entity e; e.name=name; e.tag="protected_gameplay"; e.transform.position=p;
  e.mesh=s.add_mesh(make_box(size,{.5f,.4f,.3f})); e.solid=true;
  e.collider=Aabb::from_center_size({},size); e.material.texture=TextureSlot::Brick;
  s.add_entity(std::move(e));
}

Scene fixture() {
  Scene s;
  ground(s,"StreetGrid",{},320,260,TextureSlot::Asphalt);
  for(float z:{-42.f,-28.f,0.f,28.f,42.f}) ground(s,"Sidewalk",{0,.03f,z},140,14);
  for(float x:{-42.f,-28.f,0.f,28.f,42.f}) ground(s,"SidewalkNS",{x,.04f,0},14,140);
  ground(s,"BankPlaza",{0,.05f,-8},32,24);
  ground(s,"RidgePlaza",{95,.06f,8},48,36);
  ground(s,"AshcourtPlaza",{-88,.06f,42},42,34);
  ground(s,"DepotYard",{58,.05f,-48},28,24);
  ground(s,"NorthQuayPlaza",{10,.05f,96},52,40);
  ground(s,"HarborWater",{20,-.35f,56},90,36,TextureSlot::Water);
  ground(s,"RidgeWater",{103,-.4f,38},50,28,TextureSlot::Water);
  ground(s,"NorthQuayWater",{14,-.4f,124},56,22,TextureSlot::Water);

  struct Spec { float x,z,w,h,d; };
  // Actual baseline shell footprints, not invented generic placeholders. Keep
  // these in sync if the original builders deliberately change their layout.
  const Spec metro[]={
    {-38,-6,8,7,9},{22,-6,12,14,10},{40,-10,10,11,12},{-20,22,11,8,8},
    {18,20,9,6,9},{-40,16,10,10,8},{38,18,11,13,9},{-22,-32,9,7,8},
    {20,-34,10,9,9},{0,36,14,5,8},{-36,-30,8,12,8},{48,6,9,8,10},
    {-50,4,8,9,9},{52,-28,10,15,8},{-48,-18,9,6,10},{8,-48,12,7,8},
    {-8,50,10,6,7},{30,48,8,9,8},{-30,40,9,11,9},{55,32,11,10,9},
    {-58,22,7,16,7},{62,-8,8,18,8},{-14,-55,9,4,10},{14,58,8,20,8},
    {-62,-36,10,5,9},{44,-48,7,13,7},{-44,52,12,8,7},{68,18,9,6,11}};
  const Spec ridge[]={{81,-2,10,8,9},{107,0,12,10,10},{105,20,9,6,8},
    {83,20,11,7,8},{117,10,8,12,8}};
  const Spec ash[]={{-100,34,9,6,8},{-78,32,10,7,9},{-76,52,8,5.5f,8},
    {-98,54,11,6.5f,8},{-68,44,7,8,7}};
  const Spec quay[]={{-6,90,14,9,12},{26,88,12,8,14},{-4,108,11,7,10},{28,106,10,10,9}};
  auto buildings=[&](const auto& specs,const char* prefix) {
    int i=0; for(auto v:specs) solid(s,std::string(prefix)+std::to_string(i++),
                                   {v.x,v.h*.5f,v.z},{v.w,v.h,v.d});
  };
  buildings(metro,"Bldg"); buildings(ridge,"RidgeBldg");
  buildings(ash,"AshShop"); buildings(quay,"NQWarehouse");
  // Composite heroes and pier supports exercise the protected foundation path.
  solid(s,"LoftWallN",{42,2.75f,47.5f},{11,5.5f,.7f});
  solid(s,"LoftWallW",{36.5f,2.75f,52},{.7f,5.5f,9});
  solid(s,"LoftWallE",{47.5f,2.75f,52},{.7f,5.5f,9});
  solid(s,"DepotWallN",{58,3.5f,-54},{16,7,1});
  solid(s,"RidgeBeacon",{113,2.75f,32},{1.2f,5.5f,1.2f});
  solid(s,"NQBridgePillar",{18,-1.4f,62},{1.3f,4.2f,1.3f});
  solid(s,"NorthQuaySealedContainer",{8,1.2f,112},{2.6f,2.4f,2.2f});
  return s;
}

float cross2(float ax,float az,float bx,float bz) { return ax*bz-az*bx; }
bool covers(const Entity& e,float x,float z) {
  if(!e.visible || !e.mesh) return false;
  const Mesh& m=*e.mesh;
  for(std::size_t i=0;i+2<m.indices.size();i+=3) {
    Vec3 p[3];
    for(int j=0;j<3;++j) {
      const auto v=m.vertices[m.indices[i+static_cast<std::size_t>(j)]].position;
      p[j]={v.x*e.transform.scale.x+e.transform.position.x,
            v.y*e.transform.scale.y+e.transform.position.y,
            v.z*e.transform.scale.z+e.transform.position.z};
    }
    if(std::abs(p[0].y-p[1].y)>.001f || std::abs(p[1].y-p[2].y)>.001f) continue;
    const float a=cross2(p[1].x-p[0].x,p[1].z-p[0].z,p[2].x-p[0].x,p[2].z-p[0].z);
    if(std::abs(a)<1e-7f) continue;
    const float u=cross2(x-p[0].x,z-p[0].z,p[2].x-p[0].x,p[2].z-p[0].z)/a;
    const float v=cross2(p[1].x-p[0].x,p[1].z-p[0].z,x-p[0].x,z-p[0].z)/a;
    if(u>=-.0001f && v>=-.0001f && u+v<=1.0001f) return true;
  }
  return false;
}
bool dry_surface(const Scene& s,float x,float z) {
  for(const Entity& e:s.entities()) {
    if(e.name=="StreetGrid" || e.name=="Sidewalk" || e.name=="SidewalkNS" ||
       e.name.find("Plaza")!=std::string::npos || e.name=="DepotYard" ||
       e.name.find("WorldGround.")==0) {
      if(covers(e,x,z)) return true;
    }
  }
  return false;
}
void test_full_scene() {
  Scene s=fixture(); const auto original=s.entities(); const auto solids=s.collect_solids();
  const auto stats=vaultline::upgrade_world_ground(s);
  require(stats.applied && !stats.already_applied,"first application did not run");
  require(stats.districts==6,"not all six districts have ground geometry");
  require(stats.replaced_surfaces==16,"unexpected baseline replacement coverage");
  require(stats.frontage_walks==46,"not all generic/hero building frontages were covered");
  require(stats.added_triangles+stats.terrain_triangles<=15000,"15k triangle budget exceeded");
  require(stats.added_entities<=75,"ground material batching regressed");
  require(stats.exposed_water_area>3000.f,"insufficient actual water aperture area");
  require(stats.water_aperture_area>stats.exposed_water_area+150.f,"pier occlusion missing from water accounting");
  require(stats.waterfront_edges>=20 && stats.pier_boards>175,"waterfront geometry missing");
  require(stats.road_markings>60,"contextual road markings missing");
  for(auto n:stats.district_features) require(n>10,"district lacks coherent feature coverage");
  require(s.collect_solids().size()==solids.size(),"solid count changed");
  for(std::size_t i=0;i<original.size();++i) {
    const Entity& a=original[i]; const Entity& b=s.entities()[i];
    require(a.name==b.name && a.tag==b.tag,"original entity identity/tag changed");
    require(same_pose(a.transform,b.transform),"original transform changed");
    require(a.solid==b.solid && a.visible==b.visible && a.detail==b.detail,"original behavior flags changed");
    require(same(a.collider.center,b.collider.center) && same(a.collider.half_extents,b.collider.half_extents),
            "original collider changed");
    if(a.solid) require(a.mesh==b.mesh && a.material.texture==b.material.texture,"solid visual unexpectedly replaced");
    if(a.material.texture==TextureSlot::Water) {
      require(a.mesh==b.mesh && a.material.texture==b.material.texture &&
              a.material.uv_scroll_u==b.material.uv_scroll_u &&
              a.material.roughness==b.material.roughness,"animated water was modified");
    }
  }
  std::size_t triangles=0;
  for(std::size_t i=original.size();i<s.entities().size();++i) {
    const Entity& e=s.entities()[i]; require(!e.solid,"new detail blocks gameplay");
    if(!e.mesh) continue;
    triangles+=e.mesh->indices.size()/3;
    for(auto index:e.mesh->indices) require(index<e.mesh->vertices.size(),"invalid mesh index");
    for(const auto& v:e.mesh->vertices)
      require(std::isfinite(v.position.x) && std::isfinite(v.position.y) && std::isfinite(v.position.z),"nonfinite geometry");
  }
  require(triangles==stats.added_triangles,"triangle accounting mismatch");
  const Entity& terrain=*s.find_by_name("StreetGrid");
  for(const Entity& e:original) {
    if(!e.solid || e.transform.position.y<0) continue;
    const float x=e.transform.position.x+e.collider.center.x;
    const float z=e.transform.position.z+e.collider.center.z;
    const float w=e.collider.half_extents.x*e.transform.scale.x;
    const float d=e.collider.half_extents.z*e.transform.scale.z;
    // Center, corners and edge midpoints ensure foundations aren't thin islands.
    for(float dx:{-w,0.f,w}) for(float dz:{-d,0.f,d})
      require(covers(terrain,x+dx,z+dz),"original solid foundation lost land support");
  }
  for(Vec3 hub:std::vector<Vec3>{{0,0,12},{92,0,8},{-86,0,48},{58,0,-40},{42,0,54},{18,0,88}})
    require(covers(terrain,hub.x,hub.z),"fast-travel hub is over water");
  for(float z=46;z<=94;z+=.5f) for(float x:{13.f,18.f,23.f})
    require(covers(terrain,x,z),"North Quay route was submerged");
  for(float x=23.5f;x<=49;x+=.5f)
    require(covers(terrain,x,58.8f),"loft promenade was severed");
  require(!covers(terrain,0,44) && dry_surface(s,0,44),"Harbor pier has no actual water underneath its deck");
  const Vec3 exposed[]={{-19,0,63},{-20,0,50},{58,0,67},{57,0,52},{121,0,43},{12,0,126}};
  for(Vec3 p:exposed) {
    require(!dry_surface(s,p.x,p.z),"a dry surface still occludes a water aperture");
    bool water=false;
    for(const Entity& e:s.entities()) if(e.material.texture==TextureSlot::Water && covers(e,p.x,p.z)) water=true;
    require(water,"aperture has no authored water underneath");
  }
  // Raster, ray and single-sided material tests must see the intended exterior
  // side. Default double-sided shading must not hide inverted generated faces.
  for(const auto& e:s.entities()) {
    if(!e.mesh || (e.name!="StreetGrid" && e.name.rfind("WorldGround.",0)!=0)) continue;
    const auto& mesh=*e.mesh;
    for(std::size_t i=0;i+2<mesh.indices.size();i+=3) {
      const auto& a=mesh.vertices[mesh.indices[i]];
      const auto& b=mesh.vertices[mesh.indices[i+1]];
      const auto& c=mesh.vertices[mesh.indices[i+2]];
      const auto geometric=normalize(cross(b.position-a.position,c.position-a.position));
      require(dot(geometric,a.normal)>.999f && dot(geometric,b.normal)>.999f && dot(geometric,c.normal)>.999f,
              "ground/curb/pier winding disagrees with exterior shading normals");
    }
  }
  const auto entity_count=s.entities().size(), mesh_count=s.meshes().size();
  const auto again=vaultline::upgrade_world_ground(s);
  require(again.already_applied && !again.applied && again.added_triangles==0,"second application isn't a no-op");
  require(s.entities().size()==entity_count && s.meshes().size()==mesh_count,"second application duplicated content");
  std::cout<<"World ground: districts="<<stats.districts<<" entities="<<stats.added_entities
           <<" triangles="<<stats.added_triangles<<" terrain="<<stats.terrain_triangles
           <<" walks="<<stats.frontage_walks<<" curbs="<<stats.curb_segments
           <<" markings="<<stats.road_markings<<" water_m2="<<stats.exposed_water_area
           <<" waterfront_edges="<<stats.waterfront_edges<<" boards="<<stats.pier_boards<<'\n';
}
void test_guards_and_protection() {
  Scene empty;
  require(!vaultline::upgrade_world_ground(empty).applied && empty.entities().empty(),"empty scene mutated");
  Scene s; ground(s,"StreetGrid",{},320,260);
  // A future building added inside a basin must remain supported without any
  // special-case name, including the engine's scaled collider convention.
  solid(s,"FutureShop",{-19,2,63},{3,4,3});
  Entity* e=s.find_by_name("FutureShop"); e->transform.scale={1.5f,1,1.3f};
  e->transform.rotation_euler={0,.7f,0}; e->visible=false;
  const Entity saved=*e;
  vaultline::upgrade_world_ground(s);
  require(same_pose(saved.transform,s.find_by_name("FutureShop")->transform),"future entity moved");
  for(float x:{-21.25f,-19.f,-16.75f}) for(float z:{61.05f,63.f,64.95f})
    require(covers(*s.find_by_name("StreetGrid"),x,z),"new scaled building lost dry support");
}

void test_crosswalk_paint_height() {
  Scene s=fixture();
  vaultline::upgrade_world_ground(s);
  const Entity* paint=s.find_by_name("WorldGround.Metro.WhitePaint.Detail");
  const Entity* asphalt=s.find_by_name("WorldGround.Metro.Asphalt.Structure");
  require(paint && paint->mesh && asphalt && asphalt->mesh,"Metro crossing surfaces missing");
  require(!paint->solid && paint->detail && paint->material.opacity==1.f &&
      !paint->material.alpha_blend && paint->material.texture==TextureSlot::None,
      "crosswalk must remain opaque non-solid paint geometry");
  for(const auto& v:asphalt->mesh->vertices)
    require(transform_point(asphalt->transform.matrix(),v.position).y==.02f,
            "Metro asphalt height changed");
  const auto& mesh=*paint->mesh;
  require(mesh.vertices.size()==172 && mesh.indices.size()==258,"Metro paint topology changed");
  const auto matrix=paint->transform.matrix();
  std::vector<bool> crossing(mesh.vertices.size(),false);
  std::size_t stripes=0;
  // Recognize complete authored quads. A rectangle-only vertex filter also
  // catches road-dash vertices from this shared white-paint batch.
  for(float z=3.8f;z<12.f;z+=.95f) {
    const Vec3 corners[]={{-1.8f,0,z},{1.8f,0,z},
                          {1.8f,0,z+.42f},{-1.8f,0,z+.42f}};
    std::size_t matches=0;
    for(std::size_t q=0;q<mesh.vertices.size();q+=4) {
      bool match=true;
      for(std::size_t k=0;k<4;++k) {
        const auto p=transform_point(matrix,mesh.vertices[q+k].position);
        match=match && std::abs(p.x-corners[k].x)<.00001f &&
            std::abs(p.z-corners[k].z)<.00001f;
      }
      if(!match) continue;
      ++matches;
      const unsigned order[]={0,2,1,0,3,2};
      for(std::size_t k=0;k<6;++k)
        require(mesh.indices[q/4*6+k]==q+order[k],"crosswalk winding changed");
      for(std::size_t k=0;k<4;++k) {
        const auto p=transform_point(matrix,mesh.vertices[q+k].position);
        require(p.y==.02f+.001f,"crosswalk paint must sit 1 mm above Metro asphalt");
        require(same(mesh.vertices[q+k].normal,{0,1,0}),"crosswalk normal changed");
        require(!crossing[q+k],"crosswalk stripe duplicated");
        crossing[q+k]=true;
      }
    }
    require(matches==1,"an authored crosswalk stripe is missing or duplicated");
    for(float x:{-1.8f,0.f,1.8f}) for(float sample_z:{z,z+.21f,z+.42f})
      require(covers(*asphalt,x,sample_z),"crosswalk has no asphalt underneath");
    ++stripes;
  }
  std::size_t road_vertices=0;
  for(std::size_t i=0;i<mesh.vertices.size();++i) if(!crossing[i]) {
    require(transform_point(matrix,mesh.vertices[i].position).y==.031f,
            "crosswalk correction changed unrelated road-marking height");
    ++road_vertices;
  }
  require(stripes==9 && std::count(crossing.begin(),crossing.end(),true)==36 &&
      road_vertices==136,"crosswalk correction must cover exactly nine quads / 36 vertices");
}
}  // namespace

int main() {
  try { test_full_scene(); test_guards_and_protection(); test_crosswalk_paint_height(); std::cout<<"world_ground_tests passed\n"; }
  catch(const std::exception& e) { std::cerr<<"world_ground_tests: "<<e.what()<<'\n'; return 1; }
}
