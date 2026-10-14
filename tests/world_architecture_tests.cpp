#include "../apps/vaultline/world_architecture.hpp"

#include <fury/scene.hpp>
#include <fury/texture.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
void require(bool condition,const char* message) {
  if(!condition) throw std::runtime_error(message);
}
bool same(Vec3 a,Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
bool near(float a,float b,float epsilon=.0001f) { return std::fabs(a-b)<epsilon; }

Entity shell(Scene& scene,const std::string& name,Vec3 size,Vec3 position) {
  Entity e;
  e.name=name; e.mesh=scene.add_mesh(make_box(size,{.4f,.4f,.4f}));
  e.transform.position=position; e.solid=true;
  e.collider=Aabb::from_center_size({0,0,0},size);
  e.material.texture=TextureSlot::Concrete;
  scene.add_entity(e);
  return e;
}
void strip(Scene& scene,const Entity& body,bool x,const std::string& name) {
  Entity w;
  w.name=name; w.tag="window";
  w.mesh=scene.add_mesh(make_box({1,1,1},{1,1,1}));
  w.material.texture=TextureSlot::Glass;
  w.transform.position=body.transform.position;
  w.transform.position.y=1.6f;
  if(x) { w.transform.position.x+=body.collider.half_extents.x+.06f; w.transform.scale={.08f,.35f,body.collider.half_extents.z*1.44f}; }
  else { w.transform.position.z+=body.collider.half_extents.z+.06f; w.transform.scale={body.collider.half_extents.x*1.44f,.35f,.08f}; }
  scene.add_entity(std::move(w));
}
void roof(Scene& scene,const char* name,Vec3 position,Vec3 size) {
  Entity e;
  e.name=name;e.transform.position=position;e.mesh=scene.add_mesh(make_box(size,{.4f,.4f,.4f}));
  scene.add_entity(std::move(e));
}
void check_geometry(const Mesh& mesh) {
  require(!mesh.vertices.empty() && !mesh.indices.empty(),"Generated batches contain geometry");
  require(mesh.indices.size()%3==0,"All mesh indices form triangles");
  for(const auto& v:mesh.vertices) {
    require(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z),"Finite positions");
    require(std::isfinite(v.uv.x)&&std::isfinite(v.uv.y),"Finite texture coordinates");
    require(near(length(v.normal),1.f,.003f),"Normals are unit length");
  }
  for(std::size_t i=0;i<mesh.indices.size();i+=3) {
    const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
    require(a<mesh.vertices.size()&&b<mesh.vertices.size()&&c<mesh.vertices.size(),"Mesh indices remain in bounds");
    const auto cross_=cross(mesh.vertices[b].position-mesh.vertices[a].position,
                             mesh.vertices[c].position-mesh.vertices[a].position);
    require(length(cross_)>.0000001f,"No degenerate generated triangles");
    require(dot(normalize(cross_),mesh.vertices[a].normal)>.995f,"Winding agrees with vertex normals");
  }
}

bool inside_triangle(Vec3 p,Vec3 a,Vec3 b,Vec3 c) {
  const Vec3 v0=b-a,v1=c-a,v2=p-a;
  const float d00=dot(v0,v0),d01=dot(v0,v1),d11=dot(v1,v1),d20=dot(v2,v0),d21=dot(v2,v1);
  const float den=d00*d11-d01*d01;
  if(std::fabs(den)<1e-10f) return false;
  const float v=(d11*d20-d01*d21)/den,w=(d00*d21-d01*d20)/den;
  return v>=-.0001f && w>=-.0001f && v+w<=1.0001f;
}
void check_cutout(const Entity& source,const Entity& glass) {
  // The first glazing quad is a facade bay, before roof northlights. Its centre
  // must be behind the old exterior plane, with no masonry triangle over it.
  const auto& pane=glass.mesh->vertices;
  const Vec3 middle=(pane[0].position+pane[2].position)*.5f;
  const Vec3 normal=pane[0].normal;
  require(near(normal.z,1.f),"First bay faces the positive-Z frontage");
  const Vec3 opening=middle+normal*.16f;
  require(near(opening.z,source.collider.half_extents.z),"Glazing is physically recessed 16 cm");
  for(std::size_t i=0;i<source.mesh->indices.size();i+=3) {
    const auto& a=source.mesh->vertices[source.mesh->indices[i]];
    const auto& b=source.mesh->vertices[source.mesh->indices[i+1]];
    const auto& c=source.mesh->vertices[source.mesh->indices[i+2]];
    if(dot(a.normal,normal)>.999f && std::fabs(dot(a.position-opening,normal))<.001f)
      require(!inside_triangle(opening,a.position,b.position,c.position),"Masonry leaves a genuine window cutout");
  }
  require(length(pane[1].position-pane[0].position)>=1.2f &&
          length(pane[1].position-pane[0].position)<=2.21f,"Individual window widths have plausible metre scale");
  require(length(pane[3].position-pane[0].position)>=1.4f &&
          length(pane[3].position-pane[0].position)<=1.91f,"Individual window heights have plausible metre scale");
}

void full_world() {
  Scene scene;
  std::vector<Entity> originals;
  // Dimensions match the full current city, not only a single showcase box.
  const Vec3 metro[]={{8,7,9},{12,14,10},{10,11,12},{11,8,8},{9,6,9},{10,10,8},{11,13,9},
    {9,7,8},{10,9,9},{14,5,8},{8,12,8},{9,8,10},{8,9,9},{10,15,8},{9,6,10},{12,7,8},
    {10,6,7},{8,9,8},{9,11,9},{11,10,9},{7,16,7},{8,18,8},{9,4,10},{8,20,8},{10,5,9},
    {7,13,7},{12,8,7},{9,6,11}};
  int idx=0;
  for(auto size:metro) {
    auto e=shell(scene,"Bldg"+std::to_string(idx),size,{idx*28.f,size.y*.5f,0});
    originals.push_back(e);
    if(idx!=3) { strip(scene,e,false,"WinZ"+std::to_string(idx*2)); strip(scene,e,true,"WinX"+std::to_string(idx*2+1)); }
    ++idx;
  }
  const Vec3 ridge[]={{10,8,9},{12,10,10},{9,6,8},{11,7,8},{8,12,8}};
  idx=0;for(auto size:ridge) { originals.push_back(shell(scene,"RidgeBldg"+std::to_string(idx),size,{idx*28.f,size.y*.5f,40})); ++idx; }
  const Vec3 ash[]={{9,6,8},{10,7,9},{8,5.5f,8},{11,6.5f,8},{7,8,7}};
  idx=0;for(auto size:ash) { originals.push_back(shell(scene,"AshShop"+std::to_string(idx),size,{idx*28.f,size.y*.5f,80})); ++idx; }
  const Vec3 north[]={{14,9,12},{12,8,14},{11,7,10},{10,10,9}};
  idx=0;for(auto size:north) { originals.push_back(shell(scene,"NQWarehouse"+std::to_string(idx),size,{idx*28.f,size.y*.5f,120})); ++idx; }
  // Real front-wall and roof layout of the enterable mission landmarks.
  originals.push_back(shell(scene,"DepotWallSL",{5.6f,7,1},{52.88f,3.5f,-42}));
  originals.push_back(shell(scene,"DepotWallSR",{5.6f,7,1},{63.12f,3.5f,-42}));
  originals.push_back(shell(scene,"LoftWallSL",{3.6f,5.5f,.7f},{38.8f,2.75f,56.5f}));
  originals.push_back(shell(scene,"LoftWallSR",{3.6f,5.5f,.7f},{45.2f,2.75f,56.5f}));
  roof(scene,"DepotRoof",{58,7.2f,-48},{16.6f,.45f,12.6f});
  roof(scene,"LoftRoof",{42,5.62f,52},{11.3f,.4f,9.3f});
  Entity trigger;trigger.name="HarborDepotCage";trigger.tag="vault";trigger.solid=false;scene.add_entity(trigger);
  const auto before_solids=scene.collect_solids();
  const auto before_entities=scene.entities().size();
  const auto authored=scene.find_by_name("Bldg3")->mesh;
  const auto stats=vaultline::upgrade_world_architecture(scene);
  require(stats.eligible_shells==42 && stats.upgraded_shells==41,"All 41 replaceable city shells are upgraded");
  require(stats.metro_shells==27 && stats.ridge_shells==5 && stats.ashcourt_shells==5 && stats.north_quay_shells==4,"Every district receives its full building coverage");
  require(stats.landmark_exteriors==2,"Both enterable landmarks receive roof and exterior trim");
  require(stats.hidden_window_strips==54,"Only the exact old Metro strips are hidden");
  require(stats.protected_shells==1 && scene.find_by_name("Bldg3")->mesh==authored,"Authored Bldg3 is untouched");
  require(stats.window_bays>600,"Windows cover the whole city, not a showcase facade");
  require(stats.triangles<60000,"Architecture stays below the 60k new-triangle budget");
  require(stats.added_entities<=41*7+8 && scene.entities().size()==before_entities+stats.added_entities,"Geometry is batched by building and material");
  const auto after_solids=scene.collect_solids();
  require(before_solids.size()==after_solids.size(),"No new solid obstacles are added");
  for(std::size_t i=0;i<before_solids.size();++i)
    require(same(before_solids[i].center,after_solids[i].center)&&same(before_solids[i].half_extents,after_solids[i].half_extents),"World collision is bit-for-bit unchanged");
  for(const auto& old:originals) {
    const auto* current=scene.find_by_name(old.name);
    require(current && current->solid==old.solid && current->tag==old.tag,"Original identity and gameplay fields survive");
    require(same(current->transform.position,old.transform.position)&&same(current->transform.scale,old.transform.scale)&&
            same(current->transform.rotation_euler,old.transform.rotation_euler),"Original transforms survive");
    require(same(current->collider.center,old.collider.center)&&same(current->collider.half_extents,old.collider.half_extents),"Original local collider survives");
    if(old.name=="Bldg3" || old.name.find("Wall")!=std::string::npos) continue;
    const auto* glazing=scene.find_by_name("Architecture."+old.name+".Glass");
    const auto* roof_=scene.find_by_name("Architecture."+old.name+".Roof");
    require(glazing && roof_,"Every upgraded shell has discrete windows and a roof batch");
    check_geometry(*current->mesh);
    check_cutout(*current,*glazing);
    float max_y=-100.f;for(const auto& v:roof_->mesh->vertices)max_y=std::max(max_y,v.position.y);
    require(max_y>old.collider.half_extents.y+.35f,"Roof geometry changes each building silhouette");
  }
  std::size_t occupied=0,detail_count=0;
  for(std::size_t i=before_entities;i<scene.entities().size();++i) {
    const auto& e=scene.entities()[i];
    require(!e.solid,"Architectural batches are non-solid");
    check_geometry(*e.mesh);if(e.lod_mesh)check_geometry(*e.lod_mesh);
    if(e.detail) ++detail_count;
    if(e.tag=="window") { ++occupied; require(e.name.find("Occupied")!=std::string::npos,"Only selected occupied panes participate in night emission"); }
    if(e.name.find("Architecture.DepotRoof.")==0 || e.name.find("Architecture.LoftRoof.")==0) {
      // No generated vertex enters the walkable front opening under head height.
      const bool depot=e.name.find("DepotRoof")!=std::string::npos;
      for(const auto& v:e.mesh->vertices) {
        const auto w=v.position+e.transform.position;
        const float center=depot?58.f:42.f,z=depot?-42.f:56.5f,half_gap=depot?2.2f:1.3f;
        require(!(std::fabs(w.x-center)<half_gap && std::fabs(w.z-z)<.7f && w.y<3.4f),"Mission front openings remain visually unobstructed");
      }
    }
  }
  std::cout<<"Occupancy batches: "<<occupied<<", fine-detail batches: "<<detail_count<<"\n";
  require(detail_count>=41 && occupied==41,"LOD trims and selective occupied panes cover the city");
  const auto entity_count=scene.entities().size(),mesh_count=scene.meshes().size();
  const auto again=vaultline::upgrade_world_architecture(scene);
  require(again.upgraded_shells==0 && again.landmark_exteriors==0 && again.added_entities==0 && again.triangles==0,"Upgrade is idempotent");
  require(scene.entities().size()==entity_count&&scene.meshes().size()==mesh_count,"Repeated application allocates no duplicate geometry");
  std::cout<<"Architecture: "<<stats.upgraded_shells<<" shells + "<<stats.landmark_exteriors
           <<" landmarks, "<<stats.window_bays<<" window bays, "<<stats.triangles
           <<" triangles, "<<stats.added_entities<<" added entities\n";
}

void protections_and_transforms() {
  Scene scene;
  auto scaled=shell(scene,"Bldg40",{10,8,9},{3,4,5});
  auto* e=scene.find_by_name("Bldg40");
  e->tag="scenery";e->transform.scale={1.1f,1.2f,.9f};e->transform.rotation_euler={0,.4f,0};
  e->collider.center={.25f,0,.1f};scaled=*e;
  shell(scene,"Bldg41",{8,8,8},{25,4,0});
  auto* authored=scene.find_by_name("Bldg41");
  authored->material.textures=std::make_shared<MaterialTextures>();
  const auto* protected_mesh=authored->mesh;
  shell(scene,"Bldg42",{8,8,8},{50,4,0});scene.find_by_name("Bldg42")->material.transmission=.6f;
  const auto name_trap=shell(scene,"Bldg43MissionDoor",{8,8,8},{75,4,0});
  shell(scene,"Bldg44",{8,8,8},{85,4,0}); scene.find_by_name("Bldg44")->tag="vault";
  const auto prop=shell(scene,"DepotGarageBay",{6.5f,4.2f,5},{69.5f,2.1f,-46});
  Entity orphan;orphan.name="WinZ999";orphan.visible=true;orphan.tag="window";scene.add_entity(orphan);
  const auto stats=vaultline::upgrade_world_architecture(scene);
  require(stats.upgraded_shells==1&&stats.protected_shells==3,"Authored maps, glass materials and mission tags are protected");
  require(scene.find_by_name("Bldg41")->mesh==protected_mesh,"Authored source mesh remains unchanged");
  require(scene.find_by_name(name_trap.name)->mesh==name_trap.mesh&&scene.find_by_name(prop.name)->mesh==prop.mesh,"Mission and near-prefix names are excluded");
  require(scene.find_by_name("WinZ999")->visible,"Unowned strips are not hidden");
  const auto* body=scene.find_by_name(scaled.name);
  require(body->tag==scaled.tag&&same(body->collider.center,scaled.collider.center),"Tags and collider offsets survive");
  for(const auto& part:scene.entities())if(part.name.find("Architecture.Bldg40.")==0)
    require(same(part.transform.position,scaled.transform.position)&&same(part.transform.rotation_euler,scaled.transform.rotation_euler)&&same(part.transform.scale,scaled.transform.scale),"All batches inherit the exact source pose");
  shell(scene,"AshShop12",{9,6,8},{100,3,0});
  const auto incremental=vaultline::upgrade_world_architecture(scene);
  require(incremental.upgraded_shells==1&&incremental.ashcourt_shells==1,"Newly added shells can be upgraded without duplicating old work");
}
}
int main() {
  try { full_world();protections_and_transforms(); }
  catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
  std::cout<<"World architecture coverage, collision, openings, normals, batching, budgets and idempotence passed\n";
  return 0;
}
