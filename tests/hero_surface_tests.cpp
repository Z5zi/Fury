#include "../apps/vaultline/harbor_assets.hpp"
#include "../apps/renderlab/coastal_scene.hpp"
#include "fury/surface_detail.hpp"
#include "test_files.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <tuple>

using namespace fury;
namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
bool same(Vec3 a, Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
bool close(Vec3 a, Vec3 b) { return length(a-b)<1e-5f; }
bool source_equal(const Material& a, const Material& b) {
  return same(a.albedo,b.albedo) && a.metallic==b.metallic &&
      a.roughness==b.roughness && a.emissive==b.emissive &&
      same(a.emissive_color,b.emissive_color) && a.texture==b.texture &&
      a.textures==b.textures && a.uv_scroll_u==b.uv_scroll_u &&
      a.uv_scroll_v==b.uv_scroll_v && a.wetness==b.wetness &&
      a.transmission==b.transmission && a.index_of_refraction==b.index_of_refraction &&
      a.opacity==b.opacity && a.alpha_cutoff==b.alpha_cutoff &&
      a.normal_scale==b.normal_scale && a.double_sided==b.double_sided &&
      a.alpha_blend==b.alpha_blend;
}
void test_selection_and_guards() {
  Material original;
  original.albedo={.37f,.42f,.59f}; original.metallic=.42f; original.roughness=.68f;
  original.emissive_color={.2f,.3f,.4f}; original.wetness=.15f;
  original.normal_scale=.9f; original.index_of_refraction=1.67f;
  original.uv_scroll_u=.07f; original.uv_scroll_v=-.08f; original.double_sided=false;
  original.textures=std::make_shared<MaterialTextures>();
  struct Pick { const char* asset; const char* name; TextureSlot expected; };
  const Pick picks[] = {
    {"storefront","hm_storefront_v10_shell",TextureSlot::Brick},
    {"storefront","hm_storefront_v10_fpil_0",TextureSlot::Concrete},
    {"storefront","hm_storefront_v10_fshop_0_sill",TextureSlot::Concrete},
    {"storefront","hm_storefront_v10_roof",TextureSlot::Concrete},
    {"storefront","hm_storefront_v10_fdoor",TextureSlot::Wood},
    {"storefront","hm_storefront_v10_alley_door",TextureSlot::BarrelMetal},
    {"storefront","hm_storefront_v10_fshop_0_frm",TextureSlot::BarrelMetal},
    {"storefront","hm_storefront_v10_pushplate",TextureSlot::Metal},
    {"storefront","hm_storefront_v10_fshop_0_glass",TextureSlot::None},
    {"storefront","hm_storefront_v10_sign",TextureSlot::None},
    {"storefront","hm_storefront_v10_sign_band",TextureSlot::None},
    {"storefront","hm_storefront_v10_awning",TextureSlot::None},
    {"storefront","hm_storefront_v10_v9_sill_1",TextureSlot::None},
    {"storefront","hm_storefront_v10_fshop_0_shadow",TextureSlot::None},
    {"bank_vault_door","VD_Door",TextureSlot::Metal},
    {"bank_vault_door","VD_DialGlass",TextureSlot::None},
    {"bank_vault_door","VD_Grease",TextureSlot::None},
    {"bank_vault_door","VD_Plaque",TextureSlot::None},
    {"bank_vault_door","VD_Frame",TextureSlot::None},
    {"bank_deposit_boxes","DB_Door_0",TextureSlot::Metal},
    {"bank_deposit_boxes","DB_Num_0",TextureSlot::None},
    {"bank_teller_counter","TC_Kick",TextureSlot::Metal},
    {"bank_teller_counter","TC_Top",TextureSlot::None},
    {"bank_teller_counter","TC_Scr_0",TextureSlot::None},
    {"bank_interior_kit","KIT_WallBack",TextureSlot::Concrete},
    {"bank_interior_kit","KIT_VentSlat_0_0",TextureSlot::Metal},
    {"bank_interior_kit","KIT_Floor",TextureSlot::None},
    {"bank_interior_kit","KIT_MM_Logo",TextureSlot::None},
    {"bank_interior_kit","KIT_LobbyLight_0",TextureSlot::None},
    {"bank_lobby_chair","LC_Seat",TextureSlot::None},
    {"civ_sedan","VD_Door",TextureSlot::None},
    {"prop_bench","Bench_Seat_0",TextureSlot::Wood},
    {"prop_bench","Bench_Leg_-1",TextureSlot::Concrete},
    {"prop_bench","Bench_Rail",TextureSlot::BarrelMetal},
    {"prop_bench","hm_prop_bench_v2_AssetPlate",TextureSlot::None},
    {"prop_bench","Bench_LegDirt_-1",TextureSlot::None},
  };
  for (const auto& pick:picks) {
    const auto result=harbor::hero_surface_material(pick.asset,pick.name,original);
    require(result.detail_texture==pick.expected,"Named part has the intended selective detail profile");
    require(result.world_uv_scale==surface_detail_uv_per_meter(pick.expected),"Hero texture scale is physical, never fitted to object bounds");
    require(result.detail_use_mesh_uvs==(std::string(pick.asset)=="prop_bench" && pick.expected==TextureSlot::Wood) && result.detail_rotation==0,
            "Only audited bench wood requests its asset-local longitudinal UVs");
    require(source_equal(result,original),"Decoration retains every authored factor and the original texture identity");
  }
  auto imported=original; imported.emissive=1.f; imported.emissive_color={0,0,0};
  require(harbor::hero_surface_material("bank_vault_door","VD_Door",imported).detail_texture==TextureSlot::Metal,
          "glTF's default unit emission strength with a black factor is non-emissive and receives detail");
  imported.textures.reset();
  require(harbor::hero_surface_material("bank_vault_door","VD_Door",imported).detail_texture==TextureSlot::None,
          "Legacy albedo-based emission stays excluded even with a black emissive_color field");
  for (unsigned map=0;map<4;++map) {
    auto textures=std::make_shared<MaterialTextures>();
    RgbaImage* images[]={&textures->base_color,&textures->normal,&textures->metallic_roughness,&textures->emissive};
    *images[map]={1,1,{128,128,255,255}};
    auto material=original; material.textures=textures;
    auto result=harbor::hero_surface_material("bank_vault_door","VD_Door",material);
    require(result.detail_texture==TextureSlot::None && result.world_uv_scale==0 && source_equal(result,material),
            "Any authored map, including an emissive-only map, protects the entire source set and its UVs");
  }
  for (unsigned guard=0;guard<11;++guard) {
    auto material=original;
    switch(guard) {
      case 0: material.emissive=.01f; break;
      case 1: material.transmission=.01f; break;
      case 2: material.opacity=.99f; break;
      case 3: material.alpha_blend=true; break;
      case 4: material.alpha_cutoff=.5f; break;
      case 5: material.texture=TextureSlot::Glass; break;
      case 6: material.texture=TextureSlot::Water; break;
      case 7: material.world_uv_scale=.25f; break;
      case 8: material.detail_texture=TextureSlot::Asphalt; break;
      case 9: material.detail_rotation=2; break;
      case 10: material.detail_use_mesh_uvs=true; break;
    }
    const auto result=harbor::hero_surface_material("bank_vault_door","VD_Door",material);
    require(result.detail_texture==material.detail_texture && result.world_uv_scale==material.world_uv_scale &&
            result.detail_rotation==material.detail_rotation && result.detail_use_mesh_uvs==material.detail_use_mesh_uvs && source_equal(result,material),
            "Transparency, emission, legacy slots and preassigned UV/profile choices are excluded");
  }
}

void test_real_bank_parts() {
  const char* assets[]={"bank_vault_door","bank_deposit_boxes","bank_teller_counter",
                       "bank_security_desk","bank_trim_kit","bank_interior_kit"};
  for (const auto* name:assets) {
    const auto* desc=harbor::find_asset(name); std::string path,error; GltfAsset source;
    require(desc && harbor::resolve_mesh_path(desc->glb,path) && load_gltf(path,source,error),"Bank hero source loads");
    Scene scene;
    const bool kit=std::string(name)=="bank_interior_kit";
    const auto parts=harbor::load_harbor_prims(scene,name,{},nullptr,kit ? "KIT_" : nullptr);
    require(parts.from_asset && !parts.used_fallback,"Real bank hero uses GLB primitives");
    std::size_t index=0,profiled=0,untouched=0;
    for (const auto& primitive:source.primitives) {
      if (kit && primitive.name.rfind("KIT_",0)!=0) continue;
      if (primitive.name.find("Shadow")!=std::string::npos) continue;
      const auto& part=parts.parts.at(index++);
      const auto expected=harbor::hero_surface_material(name,primitive.name,primitive.material);
      // Different loads own different empty texture markers; compare all factors
      // with that identity normalized, then inspect the original marker contents.
      auto material=part.material; material.textures=primitive.material.textures;
      require(source_equal(material,primitive.material),"Real bank source factors survive placement");
      require(part.material.textures && !part.material.textures->base_color.valid(),"Profile assignment does not rewrite source maps");
      require(part.material.detail_texture==expected.detail_texture && part.material.world_uv_scale==expected.world_uv_scale &&
              part.material.detail_rotation==expected.detail_rotation && part.material.detail_use_mesh_uvs==expected.detail_use_mesh_uvs,
              "Actual bank parts receive the selected profiles before placement");
      require(part.mesh->vertices.size()==primitive.mesh->vertices.size() && part.mesh->indices==primitive.mesh->indices,
              "Bank profile selection does not add, remove or reorder geometry");
      for (std::size_t v=0;v<part.mesh->vertices.size();++v) {
        require(close(part.mesh->vertices[v].position,transform_point(primitive.transform,primitive.mesh->vertices[v].position)),
                "Real hero vertex positions remain exactly the authored transformed geometry");
        require(part.mesh->vertices[v].uv.x==primitive.mesh->vertices[v].uv.x && part.mesh->vertices[v].uv.y==primitive.mesh->vertices[v].uv.y,
                "Original vertex UVs remain unchanged for disabling detail");
      }
      if (part.material.detail_texture==TextureSlot::None) ++untouched; else ++profiled;
    }
    require(index==parts.parts.size() && profiled>0 && untouched>0,"Each real hero is selectively dressed, never blanket textured");
    std::cout<<name<<": "<<profiled<<" profiled, "<<untouched<<" untouched primitives\n";
  }
}

void test_bench_uvs() {
  std::string path,error; GltfAsset source;
  require(harbor::resolve_mesh_path(harbor::find_asset("prop_bench")->glb,path) && load_gltf(path,source,error),"Bench source loads for longitudinal UV validation");
  Scene scene; const auto parts=harbor::load_harbor_prims(scene,"prop_bench",{});
  std::size_t part_index=0,wood_parts=0,changed_uvs=0;
  unsigned seat_faces=0,back_faces=0,end_faces=0;
  for(const auto& primitive:source.primitives) {
    if(primitive.name.find("Shadow")!=std::string::npos || primitive.name.find("SaltRing")!=std::string::npos) continue;
    const auto& part=parts.parts.at(part_index++);
    const auto& mesh=*part.mesh;
    const bool wood=part.material.detail_texture==TextureSlot::Wood;
    require(part.material.detail_use_mesh_uvs==wood,"Only actual timber slats use baked detail UVs");
    require(mesh.indices.size()==primitive.mesh->indices.size(),"Wood UV seams do not add or remove triangles");
    Mat4 inverse_world;
    require(inverse(primitive.transform,inverse_world),"Bench source transform is invertible");
    const auto normal_matrix=transpose(inverse_world);
    for(std::size_t i=0;i<mesh.indices.size();++i) {
      const auto& actual=mesh.vertices[mesh.indices[i]];
      const auto& original=primitive.mesh->vertices[primitive.mesh->indices[i]];
      require(close(actual.position,transform_point(primitive.transform,original.position)) &&
              close(actual.normal,normalize(transform_direction(normal_matrix,original.normal))) &&
              same(actual.color,original.color) && actual.opacity==original.opacity,
              "Runtime UV seams preserve each triangle corner's position, normal, color and opacity");
      if(!wood) require(actual.uv.x==original.uv.x && actual.uv.y==original.uv.y,"Unselected bench maps and source UVs stay untouched");
      else if(actual.uv.x!=original.uv.x || actual.uv.y!=original.uv.y) ++changed_uvs;
    }
    if(!wood) continue;
    ++wood_parts;
    require(part.material.world_uv_scale==.5f && part.material.detail_rotation==0,"Bench grain uses unrotated two-metre data in UV0");
    for(float yaw:{0.f,1.57079632679f}) {
      const auto placement=rotate_y(yaw);
      const auto long_axis=transform_direction(placement,{1,0,0});
      const auto end_axis=transform_direction(placement,{0,1,0});
      for(std::size_t i=0;i<mesh.indices.size();i+=3) {
        const auto& a=mesh.vertices[mesh.indices[i]];
        const auto& b=mesh.vertices[mesh.indices[i+1]];
        const auto& c=mesh.vertices[mesh.indices[i+2]];
        const Vec3 e1=transform_direction(placement,b.position-a.position);
        const Vec3 e2=transform_direction(placement,c.position-a.position);
        const Vec3 area=cross(e1,e2); const float area2=dot(area,area);
        require(area2>0,"Every remapped source triangle remains nondegenerate");
        const Vec3 n=normalize(area);
        Vec3 expected_v=long_axis-n*dot(long_axis,n);
        const bool end=dot(expected_v,expected_v)<.01f;
        if(end) expected_v=end_axis-n*dot(end_axis,n);
        expected_v=normalize(expected_v);
        const Vec3 expected_u=normalize(cross(expected_v,n));
        const Vec3 reciprocal1=cross(e2,area)*(1/area2);
        const Vec3 reciprocal2=cross(area,e1)*(1/area2);
        const Vec3 gu=reciprocal1*(b.uv.x-a.uv.x)+reciprocal2*(c.uv.x-a.uv.x);
        const Vec3 gv=reciprocal1*(b.uv.y-a.uv.y)+reciprocal2*(c.uv.y-a.uv.y);
        require(std::isfinite(gu.x) && std::isfinite(gu.y) && std::isfinite(gu.z) &&
                std::isfinite(gv.x) && std::isfinite(gv.y) && std::isfinite(gv.z),"Bench UV gradients stay finite on bevels and cut ends");
        require(length(gu-expected_u*.5f)<.006f && length(gv-expected_v*.5f)<.006f,
                "Bench seat/back grain follows local X at both placement yaws with uniform half-tile-per-metre density");
        if(yaw==0) {
          if(end) ++end_faces;
          if(std::fabs(n.y)>.99f && primitive.name.rfind("Bench_Seat_",0)==0) ++seat_faces;
          if(std::fabs(n.z)>.99f && primitive.name.rfind("Bench_Back_",0)==0) ++back_faces;
        }
      }
    }
  }
  require(wood_parts==9 && changed_uvs>0 && seat_faces>0 && back_faces>0 && end_faces>0,
          "Orientation/density checks cover the real five seat slats, four back slats and their end faces");
}

void test_storefront_and_bench() {
  Scene scene; Entity shell;
  shell.name="Bldg3"; shell.mesh=scene.add_mesh(make_box({11,8,8},{.5f,.5f,.5f}));
  shell.transform.position={-20,4,22}; shell.solid=true;
  shell.collider=Aabb::from_center_size({0,0,0},{11,8,8});
  scene.add_entity(shell); const auto before=scene.collect_solids();
  require(harbor::replace_storefront_shell(scene,"Bldg3"),"Real storefront is fitted into gameplay shell");
  const auto after=scene.collect_solids();
  require(before.size()==after.size() && same(before[0].center,after[0].center) && same(before[0].half_extents,after[0].half_extents),
          "Storefront surface integration preserves the exact collision volume");
  std::set<TextureSlot> profiles; unsigned untouched=0; std::size_t triangles=0;
  for (const auto& entity:scene.entities()) {
    if (!entity.mesh) continue;
    require(!entity.solid && same(entity.transform.position,shell.transform.position),"Storefront dressing adds no colliders or root pose changes");
    triangles+=entity.mesh->indices.size()/3;
    if(entity.material.detail_texture==TextureSlot::None) ++untouched;
    else profiles.insert(entity.material.detail_texture);
    for (const auto& vertex:entity.mesh->vertices)
      require(std::fabs(vertex.position.x)<=5.5001f && std::fabs(vertex.position.z)<=4.0001f,"Hero geometry remains inside the original footprint");
  }
  require(profiles==std::set<TextureSlot>{TextureSlot::Brick,TextureSlot::Concrete,TextureSlot::Wood,TextureSlot::Metal,TextureSlot::BarrelMetal} && untouched>0,
          "Placed storefront contains all five actual surface families alongside untouched source surfaces");
  require(triangles==28372,"Dressed storefront keeps the audited fitted source triangle count");
  const auto entities=scene.entities().size();
  require(harbor::replace_storefront_shell(scene,"Bldg3") && scene.entities().size()==entities,"Repeated storefront calls do not duplicate dressed entities");
  std::cout<<"Storefront: "<<entities-1<<" groups, "<<triangles<<" triangles, five surface profiles\n";

  Scene baseline; const auto merged=harbor::load_harbor_mesh(baseline,"prop_bench",{});
  Scene street; harbor::spawn_meridian_block(street);
  for (const char* name:{"BlkBenchA","BlkBenchB"}) {
    const auto* root=street.find_by_name(name);
    require(root && root->solid && !root->mesh && root->detail,"Bench retains its original named collision root and detail culling flag");
    require(close(root->collider.center,{0,.45f,0}) && close(root->collider.half_extents,{1.05f,.45f,.45f}),"Bench collider keeps its exact original center and dimensions");
    std::size_t indices=0; std::set<TextureSlot> bench_profiles;
    using Position=std::tuple<float,float,float>;
    std::multiset<Position> old_positions,new_positions;
    for(auto index:merged.mesh->indices) { const auto& v=merged.mesh->vertices[index]; old_positions.emplace(v.position.x,v.position.y,v.position.z); }
    for(const auto& entity:street.entities()) {
      if(entity.name.rfind(std::string(name)+"_Surface_",0)!=0) continue;
      require(!entity.solid && same(entity.transform.position,root->transform.position) && same(entity.transform.rotation_euler,root->transform.rotation_euler),
              "Grouped bench visual retains root placement and creates no duplicate collider");
      indices+=entity.mesh->indices.size(); bench_profiles.insert(entity.material.detail_texture);
      for(auto index:entity.mesh->indices) { const auto& v=entity.mesh->vertices[index]; new_positions.emplace(v.position.x,v.position.y,v.position.z); }
      if(entity.material.detail_texture==TextureSlot::Wood) {
        require(entity.material.detail_use_mesh_uvs,"Actual grouped bench draw selects its baked longitudinal UVs");
        const auto model=entity.transform.matrix();
        const auto x=transform_direction(model,{1,0,0}),y=transform_direction(model,{0,1,0});
        for(std::size_t i=0;i<entity.mesh->indices.size();i+=3) {
          const auto& a=entity.mesh->vertices[entity.mesh->indices[i]];
          const auto& b=entity.mesh->vertices[entity.mesh->indices[i+1]];
          const auto& c=entity.mesh->vertices[entity.mesh->indices[i+2]];
          const Vec3 e1=transform_direction(model,b.position-a.position);
          const Vec3 e2=transform_direction(model,c.position-a.position);
          const Vec3 n=normalize(cross(e1,e2));
          Vec3 v=x-n*dot(x,n);
          if(dot(v,v)<.01f) v=y-n*dot(y,n);
          v=normalize(v); const Vec3 u=normalize(cross(v,n));
          require(std::fabs((b.uv.x-a.uv.x)-dot(e1,u)*.5f)<1e-5f &&
                  std::fabs((b.uv.y-a.uv.y)-dot(e1,v)*.5f)<1e-5f &&
                  std::fabs((c.uv.x-a.uv.x)-dot(e2,u)*.5f)<1e-5f &&
                  std::fabs((c.uv.y-a.uv.y)-dot(e2,v)*.5f)<1e-5f,
                  "Both actual gameplay bench placements retain longitudinal grain and metre-based density after grouping");
        }
      }
    }
    require(indices==merged.mesh->indices.size() && old_positions==new_positions,"Bench grouping/UV seams preserve every baked triangle corner and triangle count");
    require(bench_profiles==std::set<TextureSlot>{TextureSlot::None,TextureSlot::Wood,TextureSlot::Concrete,TextureSlot::BarrelMetal},
            "Real gameplay benches retain distinct wood, concrete, coated metal and protected plate/wear materials");
  }
}

void write_u32(std::ostream& out, std::uint32_t value) {
  for(unsigned i=0;i<4;++i) out.put(static_cast<char>((value>>(i*8))&255));
}
void test_before_grouping(const std::filesystem::path& fixture) {
  const auto dir=fixture/"assets/meshes/harbor_metro"; std::filesystem::create_directories(dir);
  std::string json=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.4,0.5,0.6,1],"metallicFactor":0.2,"roughnessFactor":0.7}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],"nodes":[{"mesh":0,"name":"Bench_Seat_0"},{"mesh":0,"name":"Bench_Leg_-1"},{"mesh":0,"name":"Bench_Arm_1"},{"mesh":0,"name":"hm_prop_bench_v2_AssetPlate"},{"mesh":0,"name":"Bench_Back_0"}],"scenes":[{"nodes":[0,1,2,3,4]}],"scene":0})";
  while(json.size()%4) json+=' ';
  const float data[]={0,0,0,1,0,0,0,1,0};
  {
    std::ofstream out(dir/"hm_prop_bench_v2.glb",std::ios::binary);
    write_u32(out,0x46546c67); write_u32(out,2); write_u32(out,std::uint32_t(28+json.size()+sizeof(data)));
    write_u32(out,std::uint32_t(json.size())); write_u32(out,0x4e4f534a); out<<json;
    write_u32(out,sizeof(data)); write_u32(out,0x004e4942); out.write(reinterpret_cast<const char*>(data),sizeof(data));
  }
  std::filesystem::current_path(fixture);
  Scene scene; const auto groups=harbor::load_harbor_material_groups(scene,"prop_bench",{});
  require(groups.from_asset && groups.parts.size()==4,"Identical source factors split by semantic profile before grouping");
  require(groups.parts[0].material.detail_texture==TextureSlot::Wood && groups.parts[0].mesh->vertices.size()==6,
          "Compatible wood parts still batch together in deterministic source order");
  std::set<TextureSlot> profiles;
  for(const auto& part:groups.parts) {
    profiles.insert(part.material.detail_texture);
    if(part.material.detail_texture==TextureSlot::None)
      require(part.mesh->vertices.size()==3,"The original asset plate is isolated from all profiled geometry");
  }
  require(profiles==std::set<TextureSlot>{TextureSlot::Wood,TextureSlot::Concrete,TextureSlot::BarrelMetal,TextureSlot::None},
          "Same-colored branding never inherits neighbouring surface detail");
  Scene again; const auto repeated=harbor::load_harbor_material_groups(again,"prop_bench",{});
  for(std::size_t i=0;i<groups.parts.size();++i)
    require(groups.parts[i].material.detail_texture==repeated.parts[i].material.detail_texture &&
            groups.parts[i].mesh->indices==repeated.parts[i].mesh->indices,"Repeated semantic grouping has stable first-seen ordering");
}

void test_coastal() {
  CoastalScene scene; scene.create();
  require(scene.instances.size()==494 && scene.meshes.size()==7,"Coastal surface dressing retains all original instances and meshes");
  unsigned siding=0,roof=0,metal=0,glass=0,emissive=0,white=0,canvas=0,probes=0;
  for(const auto& instance:scene.instances) {
    const auto& material=instance.material;
    require(instance.mesh && !instance.mesh->vertices.empty(),"Coastal instance geometry remains present");
    if(material.transmission>0 || material.emissive>0) {
      require(material.detail_texture==TextureSlot::None,"Coastal water, glass and lamps remain unprofiled");
      if(material.transmission>.9f) ++glass;
      if(material.emissive>0) ++emissive;
    }
    if(material.detail_texture==TextureSlot::Wood) {
      require(material.world_uv_scale==.5f && material.detail_rotation==1 && !material.detail_use_mesh_uvs,
              "Cabin weatherboard grain follows horizontal U on a two-metre world tile");
      ++siding;
    }
    if(material.detail_texture==TextureSlot::BarrelMetal) {
      require(material.metallic==.65f && material.roughness==.55f && material.world_uv_scale==.5f,"Roof preserves its original coated metal factors"); ++roof;
    }
    if(material.detail_texture==TextureSlot::Metal) {
      require(material.metallic==.95f && material.roughness==.24f && material.world_uv_scale==1.f,"Small steel hardware uses a one metre tile and original scalar finish"); ++metal;
    }
    if(same(material.albedo,{.76f,.77f,.71f})) {
      require(material.detail_texture==TextureSlot::None && material.detail_rotation==0,"White painted casings and smooth boat hull retain the source material"); ++white;
    }
    if(material.texture==TextureSlot::Wood)
      require(material.detail_rotation==0,"Deck and door legacy wood retain longitudinal V grain");
    if(same(material.albedo,{.07f,.095f,.11f})) { require(material.detail_texture==TextureSlot::None,"Canvas canopy is not assigned a hard-surface profile"); ++canvas; }
    if(same(material.albedo,{.7f,.42f,.17f})) { require(material.detail_texture==TextureSlot::None,"Material reference probes keep their pure scalar response"); ++probes; }
  }
  require(siding==26 && roof==44 && metal==26,"The actual cabin and hardware instances, not a detached fixture, receive their surface profiles");
  require(glass==2 && emissive==2 && white==11 && canvas==1 && probes==5,"Coastal exclusion coverage reaches real water/glass/painted trim/boat/canvas/probe instances");
}
}

int main() {
  const auto cwd=std::filesystem::current_path();
  const auto fixture=std::filesystem::temp_directory_path()/
      ("fury-hero-surfaces-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    test_selection_and_guards(); test_real_bank_parts(); test_bench_uvs(); test_storefront_and_bench(); test_coastal();
    test_before_grouping(fixture);
    std::filesystem::current_path(cwd); remove_fury_fixture(fixture);
    std::cout<<"Hero surfaces: selective runtime profiles, authored factors/maps, geometry and collision invariance passed\n";
    return 0;
  } catch(const std::exception& error) {
    std::filesystem::current_path(cwd);
    try { if(std::filesystem::exists(fixture)) remove_fury_fixture(fixture); } catch(...) {}
    std::cerr<<error.what()<<'\n'; return 1;
  }
}
