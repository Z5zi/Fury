#include "../apps/vaultline/world_life.hpp"
#include "fury/gltf.hpp"
#include "fury/texture.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using fury::Vec3;
using fury::Entity;
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
bool same(Vec3 a,Vec3 b) { return a.x==b.x&&a.y==b.y&&a.z==b.z; }
void add(fury::Scene& s,const std::string& name,Vec3 size,int count=1) {
  auto* mesh=s.add_mesh(fury::make_box(size,{.4f,.5f,.6f}));
  for(int i=0;i<count;++i) {
    Entity e; e.name=name; e.mesh=mesh; e.tag="fixture_"+name;
    e.transform.position={float(i),.75f,-1.f}; e.material.roughness=.76f;
    e.solid=true; e.collider=fury::Aabb::from_center_size({.1f,.2f,.3f},size);
    s.add_entity(std::move(e));
  }
}
// Source-exact counts for the audited named primitive families in main.cpp.
fury::Scene fixture() {
  fury::Scene s;
  add(s,"Hydrant",{.4f,.9f,.4f},28); add(s,"MidTrash",{.65f,1.05f,.65f},28);
  for(const char* name:{"TrashCanA","TrashCanB","TrashCanC","TrashCanD","TrashCanE"}) add(s,name,{.7f,1.1f,.7f});
  for(const char* name:{"StreetBenchA","StreetBenchB","StreetBenchC","StreetBenchD"}) add(s,name,{2.2f,.45f,.7f});
  add(s,"Bollard",{.35f,1.f,.35f},4);
  add(s,"Dumpster",{2.2f,1.4f,1.4f}); add(s,"Dumpster2",{2.2f,1.4f,1.4f});
  add(s,"RooftopAC",{1.8f,1.1f,1.4f},40); add(s,"RooftopACFan",{.81f,.275f,.63f},40);
  add(s,"PlanterA",{1.6f,.7f,1.6f}); add(s,"PlanterB",{1.6f,.7f,1.6f});
  add(s,"PlanterShrubA",{1.2f,1.1f,1.2f}); add(s,"PlanterShrubB",{1.2f,1.1f,1.2f});
  add(s,"LoftPlant",{.5f,1.1f,.5f});
  add(s,"NQContainer",{2.4f,2.2f,2.f},30);
  for(const char* name:{"NQCraneMast0","NQCraneMast1"}) add(s,name,{1.6f,18.f,1.6f});
  for(const char* name:{"NQCraneBoom0","NQCraneBoom1"}) add(s,name,{14.f,1.2f,1.4f});
  add(s,"MarketStallA",{3.6f,1.1f,1.4f}); add(s,"MarketStallB",{3.6f,1.1f,1.4f});
  add(s,"StallAwningA",{4.5f,.18f,3.2f}); add(s,"StallAwningB",{4.5f,.18f,3.2f});
  for(const char* name:{"BankPlaza","RidgePlaza","AshcourtPlaza","DepotYard","LoftFloor","NorthQuayPlaza"}) {
    Entity e; e.name=name; s.add_entity(std::move(e));
  }
  // Negative selectors: authoring prefixes, protected interactable, vehicles,
  // characters, external images, and substrate objects must never be replaced.
  for(const char* name:{"BlkBench0","MeridianSign","NorthQuaySealedContainer","Civilian","TrafficSedan","DepotCashA"})
    add(s,name,{1.f,1.f,1.f});
  return s;
}
void validate_mesh(const fury::Mesh& m) {
  require(!m.vertices.empty()&&!m.indices.empty(),"Generated mesh is populated");
  require(m.indices.size()%3==0,"Complete triangles");
  for(const auto& v:m.vertices) {
    require(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z),"Positions finite");
    require(std::fabs(fury::length(v.normal)-1.f)<.0002f,"Unit normals");
    require(v.opacity==1.f,"Vegetation uses opaque geometry consistently");
    require(std::isfinite(v.uv.x)&&std::isfinite(v.uv.y),"UVs finite");
  }
  for(std::size_t i=0;i<m.indices.size();i+=3) {
    for(int j=0;j<3;++j) require(m.indices[i+j]<m.vertices.size(),"Indices in range");
    const auto& a=m.vertices[m.indices[i]],&b=m.vertices[m.indices[i+1]],&c=m.vertices[m.indices[i+2]];
    const auto cross=fury::cross(b.position-a.position,c.position-a.position);
    require(fury::length(cross)>1e-9f,"No degenerate triangles");
    require(fury::dot(fury::normalize(cross),a.normal)>.99f,"Normal agrees with winding");
  }
}
void validate_preservation(const Entity& before,const Entity& after) {
  require(before.name==after.name&&before.tag==after.tag,"Identity and gameplay tags preserved");
  require(before.solid==after.solid&&before.visible==after.visible&&before.detail==after.detail,"Original flags preserved");
  require(same(before.transform.position,after.transform.position)&&same(before.transform.scale,after.transform.scale)&&
          same(before.transform.rotation_euler,after.transform.rotation_euler),"All transforms preserved");
  require(same(before.collider.center,after.collider.center)&&same(before.collider.half_extents,after.collider.half_extents),"All original colliders preserved");
  const auto& a=before.material; const auto& b=after.material;
  require(same(a.albedo,b.albedo)&&a.roughness==b.roughness&&a.metallic==b.metallic&&
    a.emissive==b.emissive&&a.texture==b.texture&&a.detail_texture==b.detail_texture&&
    a.detail_rotation==b.detail_rotation&&a.detail_use_mesh_uvs==b.detail_use_mesh_uvs&&
    a.world_uv_scale==b.world_uv_scale&&a.uv_scroll_u==b.uv_scroll_u&&a.uv_scroll_v==b.uv_scroll_v&&
    a.wetness==b.wetness&&a.transmission==b.transmission&&a.index_of_refraction==b.index_of_refraction&&
    a.opacity==b.opacity&&a.alpha_cutoff==b.alpha_cutoff&&a.normal_scale==b.normal_scale&&
    a.double_sided==b.double_sided&&a.alpha_blend==b.alpha_blend&&same(a.emissive_color,b.emissive_color)&&
    a.textures==b.textures,"Every original material property and map pointer preserved");
}
std::uint64_t hash_scene(const fury::Scene& scene) {
  std::uint64_t hash=1469598103934665603ULL;
  const auto mix=[&hash](float f) { std::uint32_t u; std::memcpy(&u,&f,4); hash=(hash^u)*1099511628211ULL; };
  for(const auto& e:scene.entities()) for(const auto* mesh:{e.mesh,e.lod_mesh}) if(mesh) {
    for(const auto& v:mesh->vertices) {
      for(float f:{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.color.x,v.color.y,v.color.z}) mix(f);
    }
    for(auto i:mesh->indices) hash=(hash^i)*1099511628211ULL;
  }
  return hash;
}
void authored_preservation() {
  fury::Scene scene;
  std::vector<std::vector<std::uint32_t>> retained_indices;
  for(const char* name:{"BlkPlanterA","BlkPlanterB","BlkStreetKit"}) {
    const std::string file=std::string("assets/meshes/harbor_metro/")+
      (std::string(name)=="BlkStreetKit"?"hm_street_props_kit_v2.glb":"hm_prop_planter_v2.glb");
    fury::GltfAsset source; std::string error;
    require(fury::load_gltf(file,source,error),"Authored test loads shipped GLB from repository working directory");
    fury::Mesh mesh; std::vector<std::uint32_t> retained;
    for(const auto& p:source.primitives) {
      if(p.name.find("ground_walk")!=std::string::npos||p.name.find("ground_curb")!=std::string::npos||
         p.name.find("Shadow")!=std::string::npos||p.name.find("SaltRing")!=std::string::npos||
         p.name.find("xmem")!=std::string::npos||p.name.find("StreetWalk")!=std::string::npos) continue;
      const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
      fury::Mat4 inv; require(fury::inverse(p.transform,inv),"Invert authored node transform");
      const auto normals=fury::transpose(inv);
      for(auto v:p.mesh->vertices) {
        v.position=fury::transform_point(p.transform,v.position);
        v.normal=fury::normalize(fury::transform_direction(normals,v.normal));
        v.color={v.color.x*p.material.albedo.x,v.color.y*p.material.albedo.y,v.color.z*p.material.albedo.z};
        mesh.vertices.push_back(v);
      }
      for(auto idx:p.mesh->indices) {
        mesh.indices.push_back(base+idx);
        if(p.name.compare(0,16,"Planter_Foliage_")!=0) retained.push_back(base+idx);
      }
    }
    Entity e; e.name=name; e.mesh=scene.add_mesh(std::move(mesh));
    e.material=source.primitives[0].material; e.material.albedo={1,1,1};
    e.solid=true; e.detail=true; e.tag="protected_planter"; e.collider=fury::Aabb::from_center_size({0,.4f,0},{.9f,.8f,.9f});
    scene.add_entity(e); retained_indices.push_back(std::move(retained));
  }
  const auto original=scene.entities();
  const auto stats=vaultline::upgrade_world_life(scene);
  require(stats.authored_planters==3&&stats.removed_foliage_triangles==10800&&stats.skipped_authored_planters==0,"All fifteen authored sphere parts replaced through named-primitive fingerprint");
  for(std::size_t i=0;i<3;++i) {
    const auto& before=original[i]; const auto& after=scene.entities()[i];
    validate_preservation(before,after);
    require(after.mesh->vertices.size()>before.mesh->vertices.size(),"Retain source vertices then append new foliage");
    for(std::size_t j=0;j<before.mesh->vertices.size();++j) {
      const auto& a=before.mesh->vertices[j]; const auto& b=after.mesh->vertices[j];
      require(same(a.position,b.position)&&same(a.normal,b.normal)&&same(a.color,b.color)&&a.uv.x==b.uv.x&&a.uv.y==b.uv.y&&a.opacity==b.opacity,"Every authored source vertex including branding/UVs remains unchanged");
    }
    require(std::equal(retained_indices[i].begin(),retained_indices[i].end(),after.mesh->indices.begin()),"All and only named non-foliage triangles retain exact indices/order");
    require(!after.lod_mesh&&after.detail,"Authored kit retains original mid-distance culling, without an expensive full-shell far proxy");
    require(after.mesh->indices.size()<before.mesh->indices.size(),"Detailed foliage is cheaper than authored spheres");
  }
  // A changed sphere vertex invalidates the fingerprint. No broad green-color
  // filtering is allowed to accidentally match a revised prop or its branding.
  fury::Scene changed;
  Entity altered=original[0]; auto copy=*altered.mesh;
  for(auto& v:copy.vertices) if(v.position.y>.65f&&v.color.y>v.color.x*1.4f) { v.position.x+=.007f; break; }
  altered.mesh=changed.add_mesh(std::move(copy)); changed.add_entity(altered);
  auto bad=vaultline::upgrade_world_life(changed);
  require(bad.authored_planters==0&&bad.skipped_authored_planters==1&&changed.entities()[0].mesh==altered.mesh,"Unexpected authored geometry skips safely");
  fury::Scene mapped;
  altered=original[0]; auto maps=std::make_shared<fury::MaterialTextures>();
  maps->base_color={1,1,{255,255,255,255}}; altered.material.textures=maps;
  mapped.add_entity(altered);
  bad=vaultline::upgrade_world_life(mapped);
  require(bad.authored_planters==0&&bad.skipped_authored_planters==1&&mapped.entities()[0].material.textures==maps,"Image-mapped authored assets remain protected");
  std::cout<<"authored_planters: replaced="<<stats.authored_planters<<" sphere_triangles_removed="<<stats.removed_foliage_triangles
    <<" near="<<stats.near_triangles<<" lod="<<stats.lod_triangles<<" before="<<stats.replaced_triangles<<'\n';
}

} // namespace
int main() {
  try {
    auto s=fixture(); const auto original=s.entities(); const auto meshes=s.meshes().size();
    const auto solids=s.collect_solids();
    auto stats=vaultline::upgrade_world_life(s);
    for(std::size_t i=0;i<original.size();++i) {
      validate_preservation(original[i],s.entities()[i]);
      if(s.entities()[i].mesh!=original[i].mesh)
        require(s.entities()[i].lod_mesh->indices.size()<s.entities()[i].mesh->indices.size(),"Each replacement has a lower triangle LOD");
    }
    const auto after=s.collect_solids(); require(solids.size()==after.size(),"No added solid colliders");
    for(std::size_t i=0;i<after.size();++i) require(same(solids[i].center,after[i].center)&&same(solids[i].half_extents,after[i].half_extents),"World-space colliders unchanged");
    for(std::size_t i=meshes;i<s.meshes().size();++i) validate_mesh(*s.meshes()[i]);
    for(std::size_t i=original.size();i<s.entities().size();++i) {
      const auto& e=s.entities()[i]; require(!e.solid&&e.tag.empty(),"Added decoration has no gameplay influence");
      if(!e.mesh) continue;
      require(e.lod_mesh&&e.lod_mesh->indices.size()<e.mesh->indices.size(),"District vegetation reduces for distance");
      const Vec3 lo=e.collider.min(),hi=e.collider.max();
      for(const auto* mesh:{e.mesh,e.lod_mesh}) for(const auto& v:mesh->vertices) require(v.position.x>=lo.x-.001f&&v.position.x<=hi.x+.001f&&v.position.y>=lo.y-.001f&&v.position.y<=hi.y+.001f&&v.position.z>=lo.z-.001f&&v.position.z<=hi.z+.001f,"Render bounds contain full canopy");
    }
    const auto entity_count=s.entities().size(),mesh_count=s.meshes().size(); const auto hash=hash_scene(s);
    auto second=vaultline::upgrade_world_life(s);
    require(second.already_applied&&second.replaced_entities==0&&second.added_batches==0,"Idempotent repeat reports no work");
    require(s.entities().size()==entity_count&&s.meshes().size()==mesh_count&&hash_scene(s)==hash,"Idempotence leaves scene byte-stable");
    auto other=fixture(); vaultline::upgrade_world_life(other); require(hash_scene(other)==hash,"Independent builds are deterministic");
    for(const char* name:{"BlkBench0","MeridianSign","NorthQuaySealedContainer","Civilian","TrafficSedan","DepotCashA"}) {
      for(const auto& e:original) if(e.name==name) require(s.find_by_name(name)->mesh==e.mesh,"Protected entities keep authored geometry");
    }
    require(stats.containers==30&&stats.crane_parts==4&&stats.market_parts==4,"Industrial and market families covered");
    require(stats.plant_instances==22,"Three existing plus nineteen boundary plant groups");
    const std::array<std::size_t,6> plants{{4,4,4,3,3,4}};
    require(stats.district_plants==plants,"All six district planting counts are exact");
    std::cout<<"world_life: replaced="<<stats.replaced_entities<<" batches="<<stats.added_batches
      <<" plants="<<stats.plant_instances<<" utility="<<stats.utility_props
      <<" containers="<<stats.containers<<" crane_parts="<<stats.crane_parts<<" market_parts="<<stats.market_parts
      <<" near="<<stats.near_triangles<<" lod="<<stats.lod_triangles
      <<" replaced_triangles="<<stats.replaced_triangles<<" delta="<<stats.near_triangles-stats.replaced_triangles<<'\n';
    require(stats.near_triangles-stats.replaced_triangles<=30000,"Scene-wide triangle growth stays under 30k budget");
    require(stats.lod_triangles*2<stats.near_triangles,"LOD removes over half of triangles");
    authored_preservation();
    std::cout<<"world_life_tests: passed\n";
  } catch(const std::exception& ex) { std::cerr<<"world_life_tests: "<<ex.what()<<'\n'; return 1; }
}
