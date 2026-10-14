#include "../apps/vaultline/harbor_assets.hpp"
#include "test_files.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace fury;
namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
bool owns(const Scene& scene, const Mesh* mesh) {
  for (const auto& item : scene.meshes()) if (item.get()==mesh) return true;
  return false;
}
void write_u32(std::ostream& out, std::uint32_t value) {
  for (unsigned i=0;i<4;++i) out.put(static_cast<char>((value>>(i*8))&255));
}
void write_fixture(const std::filesystem::path& path) {
  // A slanted triangle, nonuniform mirrored node, three factor materials.
  std::string json=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":72}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,0,0],"max":[0,1,1]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0,0,1]}},{"pbrMetallicRoughness":{"baseColorFactor":[0,1,0,1]}},{"pbrMetallicRoughness":{"baseColorFactor":[0,1,0,0.25]},"alphaMode":"BLEND"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"material":0},{"attributes":{"POSITION":0,"NORMAL":1},"material":1},{"attributes":{"POSITION":0,"NORMAL":1},"material":2}]}],"nodes":[{"mesh":0,"scale":[-2,3,4]}],"scenes":[{"nodes":[0]}],"scene":0})";
  while (json.size()%4) json+=' ';
  const float data[]={0,0,0,0,1,0,-1,0,1, .70710678f,0,.70710678f,.70710678f,0,.70710678f,.70710678f,0,.70710678f};
  std::ofstream out(path,std::ios::binary);
  write_u32(out,0x46546c67); write_u32(out,2); write_u32(out,std::uint32_t(12+8+json.size()+8+sizeof(data)));
  write_u32(out,std::uint32_t(json.size())); write_u32(out,0x4e4f534a); out<<json;
  write_u32(out,sizeof(data)); write_u32(out,0x004e4942); out.write(reinterpret_cast<const char*>(data),sizeof(data));
}
// Exercise public pair-loading behavior with copied assets in a disposable root.
// Four nested directories keep every resolver parent prefix inside the fixture.
void test_parked_sedan_pair(const std::filesystem::path& shipped,
                           const std::filesystem::path& fixture) {
  namespace fs = std::filesystem;
  const auto old_cwd = fs::current_path();
  const auto root = fixture / "sedan/one/two/three/work";
  const auto assets = root / "assets/meshes/harbor_metro";
  fs::create_directories(assets);
  const char* original[] = {"hm_civ_sedan_v3.glb", "hm_civ_sedan_v3_lod1.glb"};
  const char* derived[] = {"hm_parked_sedan_aperture_v1.glb",
                           "hm_parked_sedan_aperture_v1_lod1.glb"};
  auto restore = [&](const char* name) {
    fs::copy_file(shipped / name, assets / name, fs::copy_options::overwrite_existing);
  };
  for (const auto* name : original) restore(name);
  for (const auto* name : derived) restore(name);
  fs::current_path(root);
  try {
    Scene scene;
    auto traffic = harbor::load_harbor_mesh(scene, "civ_sedan", {});
    auto parked = harbor::load_ashcourt_sedan(scene, {});
    require(traffic.from_asset && parked.from_asset && !parked.used_fallback &&
            traffic.mesh && traffic.lod_mesh && parked.mesh && parked.lod_mesh,
            "Original and parked sedan load complete asset pairs");
    require(scene.meshes().size() == 4 && parked.mesh != traffic.mesh &&
            parked.lod_mesh != traffic.lod_mesh,
            "Parked sedan owns an isolated near/far pair, independent of traffic");
    const auto mesh_count = scene.meshes().size();
    auto repeated = harbor::load_ashcourt_sedan(scene, {});
    auto repeated_traffic = harbor::load_harbor_mesh(scene, "civ_sedan", {});
    require(repeated.mesh == parked.mesh && repeated.lod_mesh == parked.lod_mesh &&
            repeated.material.textures == parked.material.textures &&
            repeated_traffic.mesh == traffic.mesh && repeated_traffic.lod_mesh == traffic.lod_mesh &&
            repeated_traffic.material.textures == traffic.material.textures &&
            scene.meshes().size() == mesh_count,
            "Repeated parked and traffic loads retain distinct caches without allocations");
    Scene other;
    auto other_parked = harbor::load_ashcourt_sedan(other, {});
    require(other_parked.mesh != parked.mesh && other_parked.lod_mesh != parked.lod_mesh &&
            owns(other, other_parked.mesh) && owns(other, other_parked.lod_mesh),
            "Parked cache never returns meshes owned by another scene");
    const auto previous_identity = other_parked.mesh->geometry_identity;
    *other_parked.mesh = make_box({1, 1, 1}, {1, 0, 0});
    require(other_parked.mesh->geometry_identity != previous_identity,
            "Cache fixture reuses a mesh address with a fresh geometry identity");
    auto fresh = harbor::load_ashcourt_sedan(other, {});
    require(fresh.mesh != other_parked.mesh && fresh.lod_mesh != other_parked.lod_mesh &&
            owns(other, fresh.mesh) && owns(other, fresh.lod_mesh) && other.meshes().size() == 4,
            "Parked cache rejects a stale pair when an owned mesh identity changes");
    for (bool malformed : {false, true}) for (const auto* name : derived) {
      fs::remove(assets / name);
      if (malformed) {
        std::ofstream bad(assets / name, std::ios::binary);
        bad << "{invalid glTF";
      } else {
        std::string resolved;
        require(!harbor::resolve_mesh_path((std::string("harbor_metro/") + name).c_str(), resolved),
                "Missing derivative cannot resolve from any parent asset root");
      }
      Scene missing;
      auto standard = harbor::load_harbor_mesh(missing, "civ_sedan", {});
      require(standard.from_asset && standard.mesh && standard.lod_mesh,
              "Fallback fixture loads original near/far pair");
      for (int repeat = 0; repeat < 2; ++repeat) {
        auto failed_pair = harbor::load_ashcourt_sedan(missing, {});
        require(failed_pair.mesh == standard.mesh && failed_pair.lod_mesh == standard.lod_mesh &&
                failed_pair.material.textures == standard.material.textures &&
                failed_pair.from_asset && !failed_pair.used_fallback && missing.meshes().size() == 2,
                "Either missing/malformed derivative falls back atomically without partial mesh leakage");
      }
      restore(name);
      auto recovered = harbor::load_ashcourt_sedan(missing, {});
      require(recovered.from_asset && recovered.mesh != standard.mesh &&
              recovered.lod_mesh != standard.lod_mesh && owns(missing, recovered.mesh) &&
              owns(missing, recovered.lod_mesh) && missing.meshes().size() == 4,
              "Restored complete pair is retried instead of caching the fallback under its key");
      auto still_traffic = harbor::load_harbor_mesh(missing, "civ_sedan", {});
      require(still_traffic.mesh == standard.mesh && still_traffic.lod_mesh == standard.lod_mesh,
              "Derivative recovery leaves the original traffic cache intact");
    }
    fs::current_path(old_cwd);
  } catch (...) {
    fs::current_path(old_cwd);
    throw;
  }
}
}
int main() {
  const auto cwd=std::filesystem::current_path();
  const auto fixture=std::filesystem::temp_directory_path()/
    ("fury-asset-pipeline-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    const auto assets=cwd/"assets/meshes/harbor_metro";
    require(std::filesystem::is_directory(assets),"Run asset pipeline tests with repository root as working directory");
    std::size_t count=0; const auto* descriptors=harbor::all_assets(count);
    for (std::size_t i=0;i<count;++i) {
      for (const auto* name : {descriptors[i].glb,descriptors[i].lod_glb,descriptors[i].obj}) {
        if(name) require(std::filesystem::is_regular_file(cwd/"assets/meshes"/name),"Every Harbor descriptor resolves to a shipped file");
      }
    }
    std::size_t glbs=0, triangles=0;
    for (const auto& file : std::filesystem::directory_iterator(assets)) {
      if(file.path().extension()!=".glb") continue;
      GltfAsset asset; std::string error;
      require(load_gltf(file.path().u8string(),asset,error),error.c_str());
      for(const auto& primitive:asset.primitives) {
        require(!primitive.mesh->indices.empty(),"Runtime primitives contain renderable triangles");
        require(primitive.material.textures &&
                !primitive.material.textures->base_color.valid() &&
                !primitive.material.textures->normal.valid(),
                "Shipped Harbor GLBs have an imported marker but no texture image maps");
        for(std::size_t i=0;i<primitive.mesh->indices.size();i+=3) {
          const auto& a=primitive.mesh->vertices.at(primitive.mesh->indices[i]);
          const auto& b=primitive.mesh->vertices.at(primitive.mesh->indices[i+1]);
          const auto& c=primitive.mesh->vertices.at(primitive.mesh->indices[i+2]);
          const Vec3 area=cross(b.position-a.position,c.position-a.position);
          require(dot(area,area)>1e-20f,"Degenerate geometry is excluded from runtime triangles");
        }
        triangles+=primitive.mesh->indices.size()/3;
      }
      ++glbs;
    }
    require(glbs>=44,"Every shipped GLB audited through production importer");

    Scene building_scene;
    Entity box;
    box.name="Bldg3";
    box.mesh=building_scene.add_mesh(make_box({11,8,8},{.5f,.5f,.5f}));
    box.transform.position={-20,4,22};
    box.solid=true;
    box.collider=Aabb::from_center_size({0,0,0},{11,8,8});
    building_scene.add_entity(box);
    const auto solids_before=building_scene.collect_solids();
    require(harbor::replace_storefront_shell(building_scene,"Bldg3"),"Authored storefront replaces generic shell");
    const auto* retained=building_scene.find_by_name("Bldg3");
    require(retained && retained->solid && !retained->mesh,"Original shell remains collision-only");
    require(retained->transform.position.x==-20 && retained->transform.position.y==4 && retained->transform.position.z==22,"Original shell transform preserved");
    const auto solids_after=building_scene.collect_solids();
    require(solids_after.size()==1 && solids_before.size()==1,"No new storefront collider added");
    require(solids_after[0].center.x==solids_before[0].center.x &&
            solids_after[0].center.y==solids_before[0].center.y &&
            solids_after[0].center.z==solids_before[0].center.z &&
            solids_after[0].half_extents.x==solids_before[0].half_extents.x &&
            solids_after[0].half_extents.y==solids_before[0].half_extents.y &&
            solids_after[0].half_extents.z==solids_before[0].half_extents.z,"World collider unchanged");
    for (const auto& part:building_scene.entities()) {
      if(!part.mesh) continue;
      require(!part.solid && part.tag.empty(),"Authored parts are noninteractive scenery");
      for(const auto& v:part.mesh->vertices) {
        require(v.position.x>=-5.5001f && v.position.x<=5.5001f &&
                v.position.z>=-4.0001f && v.position.z<=4.0001f,"Off-footprint export helpers excluded");
      }
    }
    const auto entity_count=building_scene.entities().size();
    require(harbor::replace_storefront_shell(building_scene,"Bldg3") &&
            building_scene.entities().size()==entity_count,"Storefront replacement is idempotent");
    require(!harbor::replace_storefront_shell(building_scene,"MeridianDoor"),"Mission shells are outside replacement scope");

    test_parked_sedan_pair(assets, fixture);

    std::filesystem::create_directories(fixture/"assets/meshes/harbor_metro");
    write_fixture(fixture/"assets/meshes/harbor_metro/hm_bank_lobby_chair_v2.glb");
    std::filesystem::current_path(fixture);
    Scene missing_building_scene;
    missing_building_scene.add_entity(box);
    require(!harbor::replace_storefront_shell(missing_building_scene,"Bldg3") &&
            missing_building_scene.find_by_name("Bldg3")->mesh==box.mesh &&
            missing_building_scene.entities().size()==1,"Missing storefront preserves original box without partial changes");
    Scene scene;
    auto merged=harbor::load_harbor_mesh(scene,"bank_lobby_chair",{});
    require(merged.from_asset && merged.mesh->vertices.size()==9,"Synthetic GLB merged");
    require(merged.material.albedo.x==1 && merged.material.albedo.y==1,"Merged material is neutral");
    bool red=false, green=false;
    for(const auto& v:merged.mesh->vertices) {
      red|=v.color.x==1 && v.color.y==0;
      green|=v.color.x==0 && v.color.y==1;
      const auto expected=normalize(Vec3{-.5f,0,.25f});
      require(dot(v.normal,expected)>.9999f,"Baked normal uses inverse transpose");
    }
    require(red && green,"All authored albedo colors survive merging");
    const auto& mesh=*merged.mesh;
    auto area=cross(mesh.vertices[mesh.indices[1]].position-mesh.vertices[mesh.indices[0]].position,
                    mesh.vertices[mesh.indices[2]].position-mesh.vertices[mesh.indices[0]].position);
    require(dot(area,mesh.vertices[mesh.indices[0]].normal)>0,"Mirrored bake corrects winding");
    auto again=harbor::load_harbor_mesh(scene,"bank_lobby_chair",{});
    require(again.mesh==merged.mesh,"Same scene shares cached mesh");
    Scene another;
    auto other=harbor::load_harbor_mesh(another,"bank_lobby_chair",{});
    require(other.mesh!=merged.mesh && owns(another,other.mesh),"Cache never returns another scene's owned mesh");
    auto parts=harbor::load_harbor_material_groups(scene,"bank_lobby_chair",{});
    require(parts.parts.size()==3,"Opaque and blended materials remain distinct");
    require(parts.parts[0].material.textures &&
            parts.parts[0].material.textures==parts.parts[1].material.textures &&
            parts.parts[1].material.textures==parts.parts[2].material.textures,
            "Factor-only materials share an imported identity for correct emission and batching");
    require(parts.parts[2].material.alpha_blend && parts.parts[2].material.opacity==.25f,"Grouped alpha material retains properties and source order");
    auto prims=harbor::load_harbor_prims(scene,"bank_lobby_chair",{});
    require(prims.parts.size()==3,"Per-primitive path retained");
    require(dot(prims.parts[0].mesh->vertices[0].normal,normalize(Vec3{-.5f,0,.25f}))>.9999f,"Per-primitive bake uses inverse transpose");
    std::filesystem::current_path(cwd);
    remove_fury_fixture(fixture);
    std::cout<<"Asset pipeline: "<<glbs<<" GLBs, "<<triangles
             <<" runtime triangles; descriptor, color, alpha, normal, winding, cache, and parked pair fallback regressions passed\n";
    return 0;
  } catch(const std::exception& e) {
    std::filesystem::current_path(cwd);
    try { if(std::filesystem::exists(fixture)) remove_fury_fixture(fixture); } catch(...) {}
    std::cerr<<e.what()<<'\n'; return 1;
  }
}
