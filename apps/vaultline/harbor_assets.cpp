#include "harbor_assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace harbor {
namespace {

using fury::Entity;
using fury::Log;
using fury::Material;
using fury::Mesh;
using fury::Vec3;

const HarborAssetDesc kAssets[] = {
    {"bank_interior_kit", "harbor_metro/hm_bank_interior_kit_v2.glb", nullptr,
     "harbor_metro/hm_bank_interior_kit_v2.obj"},
    {"bank_vault_door", "harbor_metro/hm_bank_vault_door_v2.glb", nullptr,
     "harbor_metro/hm_bank_vault_door_v2.obj"},
    {"bank_teller_counter", "harbor_metro/hm_bank_teller_counter_v2.glb", nullptr,
     "harbor_metro/hm_bank_teller_counter_v2.obj"},
    {"bank_security_desk", "harbor_metro/hm_bank_security_desk_v2.glb", nullptr,
     "harbor_metro/hm_bank_security_desk_v2.obj"},
    {"bank_deposit_boxes", "harbor_metro/hm_bank_deposit_boxes_v2.glb", nullptr,
     "harbor_metro/hm_bank_deposit_boxes_v2.obj"},
    {"bank_queue_poles", "harbor_metro/hm_bank_queue_poles_v2.glb", nullptr,
     "harbor_metro/hm_bank_queue_poles_v2.obj"},
    {"bank_stanchion", "harbor_metro/hm_bank_stanchion_v2.glb", nullptr,
     "harbor_metro/hm_bank_stanchion_v2.obj"},
    {"bank_lobby_chair", "harbor_metro/hm_bank_lobby_chair_v2.glb", nullptr,
     "harbor_metro/hm_bank_lobby_chair_v2.obj"},
    {"bank_camera_dome", "harbor_metro/hm_bank_camera_dome_v2.glb", nullptr,
     "harbor_metro/hm_bank_camera_dome_v2.obj"},
    {"bank_alarm_panel", "harbor_metro/hm_bank_alarm_panel_v2.glb", nullptr,
     "harbor_metro/hm_bank_alarm_panel_v2.obj"},
    {"bank_access_panel", "harbor_metro/hm_bank_access_panel_v2.glb", nullptr,
     "harbor_metro/hm_bank_access_panel_v2.obj"},
    {"bank_badge_scanner", "harbor_metro/hm_bank_badge_scanner_v2.glb", nullptr,
     "harbor_metro/hm_bank_badge_scanner_v2.obj"},
    {"bank_card_reader", "harbor_metro/hm_bank_card_reader_v2.glb", nullptr,
     "harbor_metro/hm_bank_card_reader_v2.obj"},
    {"bank_motion_sensor", "harbor_metro/hm_bank_motion_sensor_v2.glb", nullptr,
     "harbor_metro/hm_bank_motion_sensor_v2.obj"},
    {"bank_security_cabinet", "harbor_metro/hm_bank_security_cabinet_v2.glb",
     nullptr, "harbor_metro/hm_bank_security_cabinet_v2.obj"},
    {"bank_annex", "harbor_metro/hm_bank_annex_v2.glb", nullptr,
     "harbor_metro/hm_bank_annex_v2.obj"},
    {"bank_trim_kit", "harbor_metro/hm_bank_trim_kit_v2.glb", nullptr,
     "harbor_metro/hm_bank_trim_kit_v2.obj"},
    {"street_props_kit", "harbor_metro/hm_street_props_kit_v2.glb", nullptr,
     "harbor_metro/hm_street_props_kit_v2.obj"},
    {"prop_atm", "harbor_metro/hm_prop_atm_v2.glb", nullptr,
     "harbor_metro/hm_prop_atm_v2.obj"},
    {"prop_barrier_set", "harbor_metro/hm_prop_barrier_set_v2.glb", nullptr,
     "harbor_metro/hm_prop_barrier_set_v2.obj"},
    {"prop_bench", "harbor_metro/hm_prop_bench_v2.glb", nullptr,
     "harbor_metro/hm_prop_bench_v2.obj"},
    {"prop_bike_rack", "harbor_metro/hm_prop_bike_rack_v2.glb", nullptr,
     "harbor_metro/hm_prop_bike_rack_v2.obj"},
    {"prop_bollard", "harbor_metro/hm_prop_bollard_v2.glb", nullptr,
     "harbor_metro/hm_prop_bollard_v2.obj"},
    {"prop_drain_grate", "harbor_metro/hm_prop_drain_grate_v2.glb", nullptr,
     "harbor_metro/hm_prop_drain_grate_v2.obj"},
    {"prop_hydrant", "harbor_metro/hm_prop_hydrant_v2.glb", nullptr,
     "harbor_metro/hm_prop_hydrant_v2.obj"},
    {"prop_manhole", "harbor_metro/hm_prop_manhole_v2.glb", nullptr,
     "harbor_metro/hm_prop_manhole_v2.obj"},
    {"prop_newsbox", "harbor_metro/hm_prop_newsbox_v2.glb", nullptr,
     "harbor_metro/hm_prop_newsbox_v2.obj"},
    {"prop_parking_meter", "harbor_metro/hm_prop_parking_meter_v2.glb", nullptr,
     "harbor_metro/hm_prop_parking_meter_v2.obj"},
    {"prop_planter", "harbor_metro/hm_prop_planter_v2.glb", nullptr,
     "harbor_metro/hm_prop_planter_v2.obj"},
    {"prop_sign_post", "harbor_metro/hm_prop_sign_post_v2.glb", nullptr,
     "harbor_metro/hm_prop_sign_post_v2.obj"},
    {"prop_trash_bin", "harbor_metro/hm_prop_trash_bin_v2.glb", nullptr,
     "harbor_metro/hm_prop_trash_bin_v2.obj"},
    {"prop_utility_cabinet", "harbor_metro/hm_prop_utility_cabinet_v2.glb",
     nullptr, "harbor_metro/hm_prop_utility_cabinet_v2.obj"},
    {"hmpd_cruiser", "harbor_metro/hmpd_cruiser_v12b.glb",
     "harbor_metro/hmpd_cruiser_v12b_lod1.glb",
     "harbor_metro/hmpd_cruiser_v12b.obj"},
    {"civ_sedan", "harbor_metro/hm_civ_sedan_v3.glb",
     "harbor_metro/hm_civ_sedan_v3_lod1.glb", "harbor_metro/hm_civ_sedan_v3.obj"},
    {"civ_hatch", "harbor_metro/hm_civ_hatch_v3.glb",
     "harbor_metro/hm_civ_hatch_v3_lod1.glb", "harbor_metro/hm_civ_hatch_v3.obj"},
    {"civ_van", "harbor_metro/hm_civ_van_v3.glb",
     "harbor_metro/hm_civ_van_v3_lod1.glb", "harbor_metro/hm_civ_van_v3.obj"},
    {"buildings_kit", "harbor_metro/hm_buildings_kit_v10.glb",
     "harbor_metro/hm_buildings_kit_v10_lod1.glb",
     "harbor_metro/hm_buildings_kit_v10.obj"},
};

bool is_helper_prim(const std::string& name) {
  return name.find("ground_walk") != std::string::npos ||
         name.find("ground_curb") != std::string::npos ||
         name.find("Shadow") != std::string::npos ||
         name.find("SaltRing") != std::string::npos ||
         name.find("Scuff") != std::string::npos ||
         name.find("xmem") != std::string::npos;
}

Mesh merge_gltf_filtered(const fury::GltfAsset& asset, Material& out_mat,
                         bool& got_mat) {
  Mesh out;
  got_mat = false;
  for (const auto& prim : asset.primitives) {
    if (is_helper_prim(prim.name) || !prim.mesh) {
      continue;
    }
    if (!got_mat) {
      out_mat = prim.material;
      got_mat = true;
    }
    const auto base = static_cast<std::uint32_t>(out.vertices.size());
    out.vertices.reserve(out.vertices.size() + prim.mesh->vertices.size());
    for (const auto& v : prim.mesh->vertices) {
      fury::Vertex nv = v;
      nv.position = fury::transform_point(prim.transform, v.position);
      nv.normal =
          fury::normalize(fury::transform_direction(prim.transform, v.normal));
      out.vertices.push_back(nv);
    }
    for (std::uint32_t idx : prim.mesh->indices) {
      out.indices.push_back(base + idx);
    }
  }
  return out;
}

bool load_gltf_relative(const char* relative, fury::GltfAsset& out,
                        std::string& error) {
  std::string path;
  if (!resolve_mesh_path(relative, path)) {
    error = "path not found";
    return false;
  }
  return fury::load_gltf(path, out, error);
}

LoadedHarborMesh load_merged_from_desc(fury::Scene& scene,
                                       const HarborAssetDesc& desc,
                                       Mesh fallback, const char* log_label) {
  LoadedHarborMesh result;
  result.material.albedo = {1.f, 1.f, 1.f};
  result.material.roughness = 0.45f;
  result.material.metallic = 0.25f;

  const char* label = log_label ? log_label : desc.name;

  fury::GltfAsset asset;
  std::string error;
  if (desc.glb && load_gltf_relative(desc.glb, asset, error)) {
    bool got_mat = false;
    Mesh merged = merge_gltf_filtered(asset, result.material, got_mat);
    if (!merged.vertices.empty()) {
      result.mesh = scene.add_mesh(std::move(merged));
      result.from_asset = true;
      Log::info(std::string("GLB loaded: ") + desc.glb + " (" + label + ")");
    }
  } else if (desc.glb) {
    Log::warn(std::string("WARNING missing ") + label + " glb (" + desc.glb +
              "): " + error + " — trying OBJ / fallback");
  }

  if (!result.mesh && desc.obj) {
    Mesh loaded;
    if (fury::load_obj_asset(desc.obj, loaded)) {
      result.mesh = scene.add_mesh(std::move(loaded));
      result.from_asset = true;
      Log::info(std::string("OBJ loaded: ") + desc.obj + " (" + label + ")");
    }
  }

  if (!result.mesh) {
    Log::warn(std::string("WARNING missing ") + label +
              " — Using fallback");
    result.mesh = scene.add_mesh(std::move(fallback));
    result.used_fallback = true;
  }

  if (desc.lod_glb) {
    fury::GltfAsset lod_asset;
    std::string lod_err;
    if (load_gltf_relative(desc.lod_glb, lod_asset, lod_err)) {
      Material lod_mat;
      bool got = false;
      Mesh lod_merged = merge_gltf_filtered(lod_asset, lod_mat, got);
      if (!lod_merged.vertices.empty()) {
        result.lod_mesh = scene.add_mesh(std::move(lod_merged));
      }
    }
  }

  return result;
}

HarborPrimSet load_prims_from_desc(fury::Scene& scene,
                                   const HarborAssetDesc& desc, Mesh fallback,
                                   const char* log_label) {
  HarborPrimSet set;
  const char* label = log_label ? log_label : desc.name;

  fury::GltfAsset asset;
  std::string error;
  if (desc.glb && load_gltf_relative(desc.glb, asset, error)) {
    for (const auto& prim : asset.primitives) {
      if (is_helper_prim(prim.name) || !prim.mesh) {
        continue;
      }
      Mesh local = *prim.mesh;
      for (auto& v : local.vertices) {
        v.position = fury::transform_point(prim.transform, v.position);
        v.normal =
            fury::normalize(fury::transform_direction(prim.transform, v.normal));
      }
      HarborPrimPart part;
      part.mesh = scene.add_mesh(std::move(local));
      part.material = prim.material;
      set.parts.push_back(std::move(part));
    }
    if (!set.parts.empty()) {
      set.from_asset = true;
      Log::info(std::string("GLB prims loaded: ") + desc.glb + " (" + label +
                ", " + std::to_string(set.parts.size()) + " parts)");
      return set;
    }
  } else if (desc.glb) {
    Log::warn(std::string("WARNING missing ") + label + " glb — trying OBJ");
  }

  // Fall back to merged single mesh wrapped as one part.
  LoadedHarborMesh merged =
      load_merged_from_desc(scene, desc, std::move(fallback), label);
  HarborPrimPart part;
  part.mesh = merged.mesh;
  part.material = merged.material;
  set.parts.push_back(part);
  set.from_asset = merged.from_asset;
  set.used_fallback = merged.used_fallback;
  return set;
}

// Simple cache so traffic/patrol share one mesh.
std::unordered_map<std::string, LoadedHarborMesh> g_merged_cache;

}  // namespace

const HarborAssetDesc* find_asset(const char* name) {
  if (!name) {
    return nullptr;
  }
  for (const auto& a : kAssets) {
    if (std::strcmp(a.name, name) == 0) {
      return &a;
    }
  }
  return nullptr;
}

const HarborAssetDesc* all_assets(std::size_t& out_count) {
  out_count = sizeof(kAssets) / sizeof(kAssets[0]);
  return kAssets;
}

bool resolve_mesh_path(const char* relative, std::string& out_path) {
  if (!relative || !relative[0]) {
    return false;
  }
  static const char* kPrefixes[] = {
      "assets/meshes/",
      "../assets/meshes/",
      "../../assets/meshes/",
      "../../../assets/meshes/",
      "./",
  };
  for (const char* prefix : kPrefixes) {
    const std::string path = std::string(prefix) + relative;
    // Probe via ifstream through load_gltf / filesystem — use fopen.
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
      std::fclose(f);
      out_path = path;
      return true;
    }
  }
  return false;
}

LoadedHarborMesh load_harbor_mesh(fury::Scene& scene, const char* asset_name,
                                  Mesh fallback, const char* log_label) {
  const auto it = g_merged_cache.find(asset_name);
  if (it != g_merged_cache.end()) {
    return it->second;
  }
  const HarborAssetDesc* desc = find_asset(asset_name);
  LoadedHarborMesh loaded;
  if (!desc) {
    Log::warn(std::string("WARNING unknown Harbor asset '") + asset_name +
              "' — Using fallback");
    loaded.mesh = scene.add_mesh(std::move(fallback));
    loaded.used_fallback = true;
  } else {
    loaded = load_merged_from_desc(scene, *desc, std::move(fallback),
                                   log_label ? log_label : asset_name);
  }
  g_merged_cache[asset_name] = loaded;
  return loaded;
}

HarborPrimSet load_harbor_prims(fury::Scene& scene, const char* asset_name,
                                Mesh fallback, const char* log_label) {
  const HarborAssetDesc* desc = find_asset(asset_name);
  if (!desc) {
    HarborPrimSet set;
    HarborPrimPart part;
    part.mesh = scene.add_mesh(std::move(fallback));
    part.material.albedo = {0.7f, 0.7f, 0.72f};
    set.parts.push_back(part);
    set.used_fallback = true;
    Log::warn(std::string("WARNING unknown Harbor asset '") + asset_name +
              "' — Using fallback");
    return set;
  }
  return load_prims_from_desc(scene, *desc, std::move(fallback),
                              log_label ? log_label : asset_name);
}

void place_merged(fury::Scene& scene, const LoadedHarborMesh& loaded,
                  const char* entity_name, const Vec3& pos, float yaw,
                  bool solid, const Vec3& collider_size, bool detail,
                  const char* tag, const Vec3& scale) {
  Entity e;
  e.name = entity_name;
  e.mesh = loaded.mesh;
  e.lod_mesh = loaded.lod_mesh;
  e.transform.position = pos;
  e.transform.rotation_euler = {0.f, yaw, 0.f};
  e.transform.scale = scale;
  e.material = loaded.material;
  e.detail = detail;
  if (tag) {
    e.tag = tag;
  }
  if (solid) {
    e.solid = true;
    e.collider = fury::Aabb::from_center_size({0.f, collider_size.y * 0.5f, 0.f},
                                              collider_size);
  }
  scene.add_entity(std::move(e));
}

void place_prims(fury::Scene& scene, const HarborPrimSet& set,
                 const char* name_prefix, const Vec3& pos, float yaw,
                 bool detail, const char* tag) {
  int i = 0;
  for (const auto& part : set.parts) {
    Entity e;
    e.name = std::string(name_prefix) + "_" + std::to_string(i++);
    e.mesh = part.mesh;
    e.transform.position = pos;
    e.transform.rotation_euler = {0.f, yaw, 0.f};
    e.material = part.material;
    e.detail = detail;
    if (tag) {
      e.tag = tag;
    }
    scene.add_entity(std::move(e));
  }
}

const char* vehicle_asset_name(VehicleVisualType kind) {
  switch (kind) {
    case VehicleVisualType::HmpdCruiser:
      return "hmpd_cruiser";
    case VehicleVisualType::CivSedan:
      return "civ_sedan";
    case VehicleVisualType::CivHatch:
      return "civ_hatch";
    case VehicleVisualType::CivVan:
      return "civ_van";
  }
  return "civ_sedan";
}

void spawn_meridian_mutual(fury::Scene& scene) {
  // Bank shell origin matches build_meridian_mutual (cx=0, cz=-10).
  constexpr float bank_cx = 0.f;
  constexpr float bank_cz = -10.f;

  // Interior kit fills lobby→corridor volume (asset-local origin at floor).
  {
    auto kit = load_harbor_prims(
        scene, "bank_interior_kit",
        fury::make_box({14.f, 3.2f, 12.f}, Vec3{0.75f, 0.78f, 0.82f}),
        "Meridian Mutual kit");
    place_prims(scene, kit, "MMKit", {bank_cx, 0.f, bank_cz}, 0.f, false,
                "bank");
    if (kit.from_asset) {
      Log::info(kLogMeridianMutual);
    } else {
      Log::warn("WARNING Meridian Mutual kit missing — Using fallback");
    }
  }

  // Readable route props (hero pieces on top of kit for clarity).
  {
    auto teller = load_harbor_prims(
        scene, "bank_teller_counter",
        fury::make_box({3.4f, 1.6f, 1.1f}, Vec3{0.22f, 0.25f, 0.30f}),
        "teller counter");
    place_prims(scene, teller, "MMTeller", {0.f, 0.f, bank_cz + 2.4f}, 0.f,
                false);
  }
  {
    auto desk = load_harbor_prims(
        scene, "bank_security_desk",
        fury::make_box({2.0f, 1.3f, 0.95f}, Vec3{0.25f, 0.28f, 0.32f}),
        "security desk");
    place_prims(scene, desk, "MMSecDesk", {-5.5f, 0.f, bank_cz + 0.5f},
                1.5708f, false);
  }
  {
    auto poles = load_harbor_prims(
        scene, "bank_queue_poles",
        fury::make_box({3.0f, 1.05f, 0.4f}, Vec3{0.75f, 0.72f, 0.55f}),
        "queue poles");
    place_prims(scene, poles, "MMQueue", {0.f, 0.f, bank_cz + 4.2f}, 0.f, true);
  }
  {
    int ci = 0;
    for (float x : {-2.5f, -1.0f, 1.0f, 2.5f}) {
      auto chair = load_harbor_mesh(
          scene, "bank_lobby_chair",
          fury::make_box({0.7f, 0.85f, 0.7f}, Vec3{0.35f, 0.22f, 0.18f}),
          "lobby chair");
      const std::string n = "MMChair" + std::to_string(ci++);
      place_merged(scene, chair, n.c_str(), {x, 0.f, bank_cz + 5.1f}, 3.1416f,
                   true, {0.7f, 0.85f, 0.7f}, true);
    }
  }

  // Vault corridor hero door + deposit boxes (gameplay vault tag on collider proxy).
  {
    auto door = load_harbor_prims(
        scene, "bank_vault_door",
        fury::make_box({3.2f, 2.8f, 1.2f}, Vec3{0.95f, 0.72f, 0.18f}),
        "vault door");
    place_prims(scene, door, "MMVaultDoorVis", {bank_cx, 0.f, bank_cz - 5.2f},
                0.f, false, "vault_vis");
    // Solid gameplay proxy (simple box collision — never use visual tris).
    Entity vault;
    vault.name = "VaultDoor";
    vault.tag = "vault";
    vault.mesh = scene.add_mesh(
        fury::make_box({3.2f, 2.8f, 1.0f}, Vec3{0.15f, 0.15f, 0.16f}));
    vault.transform.position = {bank_cx, 1.4f, bank_cz - 5.2f};
    vault.material.albedo = {0.2f, 0.2f, 0.22f};
    vault.material.metallic = 0.9f;
    vault.material.roughness = 0.25f;
    vault.visible = false;  // visual is MMVaultDoorVis_*; collider still solid
    vault.solid = true;
    vault.collider =
        fury::Aabb::from_center_size({0.f, 0.f, 0.f}, {3.2f, 2.8f, 1.2f});
    scene.add_entity(std::move(vault));
    if (door.from_asset) {
      Log::info(kLogVaultDoor);
    } else {
      Log::warn("WARNING missing vault door — Using fallback");
    }
  }
  {
    auto boxes = load_harbor_prims(
        scene, "bank_deposit_boxes",
        fury::make_box({1.8f, 1.7f, 0.4f}, Vec3{0.55f, 0.48f, 0.40f}),
        "deposit boxes");
    place_prims(scene, boxes, "MMDepositL", {bank_cx - 4.2f, 0.f, bank_cz - 6.0f},
                1.5708f, false);
    place_prims(scene, boxes, "MMDepositR", {bank_cx + 4.2f, 0.f, bank_cz - 6.0f},
                -1.5708f, false);
  }

  // Security systems along restricted corridor.
  {
    const Vec3 cam_pts[] = {{bank_cx - 6.5f, 2.8f, bank_cz + 4.5f},
                            {bank_cx + 6.5f, 2.8f, bank_cz + 4.5f},
                            {bank_cx, 2.9f, bank_cz - 3.2f}};
    int cam_i = 0;
    for (const Vec3& p : cam_pts) {
      auto cam = load_harbor_mesh(
          scene, "bank_camera_dome",
          fury::make_box({0.35f, 0.28f, 0.45f}, Vec3{0.15f, 0.16f, 0.18f}),
          "camera dome");
      const std::string n = "MMCam" + std::to_string(cam_i++);
      place_merged(scene, cam, n.c_str(), p, 0.f, false, {}, true, "camera");
    }
  }
  {
    auto alarm = load_harbor_mesh(
        scene, "bank_alarm_panel",
        fury::make_box({0.35f, 0.45f, 0.12f}, Vec3{0.85f, 0.2f, 0.15f}),
        "alarm panel");
    place_merged(scene, alarm, "MMAlarmPanel",
                 {bank_cx + 7.2f, 1.4f, bank_cz - 1.0f}, -1.5708f, true,
                 {0.4f, 0.5f, 0.2f}, false);
  }
  {
    auto access = load_harbor_mesh(
        scene, "bank_access_panel",
        fury::make_box({0.3f, 0.4f, 0.1f}, Vec3{0.4f, 0.45f, 0.5f}),
        "access panel");
    place_merged(scene, access, "MMAccessPanel",
                 {bank_cx - 3.5f, 1.3f, bank_cz - 2.4f}, 0.f, true,
                 {0.35f, 0.45f, 0.15f}, false);
  }
  {
    auto cab = load_harbor_mesh(
        scene, "bank_security_cabinet",
        fury::make_box({0.9f, 1.6f, 0.5f}, Vec3{0.3f, 0.32f, 0.36f}),
        "security cabinet");
    place_merged(scene, cab, "MMSecCab", {bank_cx + 6.8f, 0.f, bank_cz - 2.8f},
                 -1.5708f, true, {0.9f, 1.6f, 0.5f}, false);
  }

  // Alarm beacon retained for heat flash (name expected by existing logic).
  {
    Entity s;
    s.name = "MeridianSiren";
    s.tag = "siren";
    s.mesh = scene.add_mesh(
        fury::make_box({0.55f, 0.35f, 0.55f}, Vec3{0.95f, 0.15f, 0.12f}));
    s.transform.position = {bank_cx, 8.4f, bank_cz};
    s.material.albedo = {1.0f, 0.2f, 0.15f};
    s.material.emissive = 0.2f;
    s.material.roughness = 0.85f;
    scene.add_entity(std::move(s));
  }
}

void spawn_meridian_block(fury::Scene& scene) {
  struct PropPlace {
    const char* asset;
    const char* name;
    Vec3 pos;
    float yaw;
    bool solid;
    Vec3 col;
  };
  // Density: clusters every ~10–15 m around Meridian Mutual front / side alley.
  const PropPlace places[] = {
      // Bank entrance cluster
      {"prop_atm", "BlkAtmA", {-7.5f, 0.f, -2.2f}, 3.1416f, true, {1.0f, 1.6f, 0.75f}},
      {"prop_atm", "BlkAtmB", {-6.2f, 0.f, -2.2f}, 3.1416f, true, {1.0f, 1.6f, 0.75f}},
      {"prop_bench", "BlkBenchA", {4.5f, 0.f, -1.5f}, 0.f, true, {2.1f, 0.9f, 0.9f}},
      {"prop_bollard", "BlkBollardA", {-2.8f, 0.f, -2.5f}, 0.f, true, {0.35f, 1.0f, 0.35f}},
      {"prop_bollard", "BlkBollardB", {2.8f, 0.f, -2.5f}, 0.f, true, {0.35f, 1.0f, 0.35f}},
      {"prop_trash_bin", "BlkTrashA", {6.5f, 0.f, -1.8f}, 0.f, true, {0.7f, 1.1f, 0.7f}},
      {"prop_planter", "BlkPlanterA", {-9.0f, 0.f, -3.5f}, 0.f, true, {0.9f, 0.8f, 0.9f}},
      // Side alley (east) — utility + barrier storytelling
      {"prop_utility_cabinet", "BlkUtilA", {11.5f, 0.f, -8.0f}, -1.5708f, true,
       {0.8f, 1.5f, 0.5f}},
      {"prop_barrier_set", "BlkBarrierA", {12.5f, 0.f, -14.0f}, 0.f, true,
       {2.0f, 1.2f, 0.6f}},
      {"bank_camera_dome", "BlkCamAlley", {11.0f, 2.6f, -12.0f}, 0.f, false, {}},
      // West curb
      {"prop_hydrant", "BlkHydrant", {-11.0f, 0.f, -4.0f}, 0.f, true, {0.45f, 0.9f, 0.45f}},
      {"prop_parking_meter", "BlkMeterA", {-10.5f, 0.f, 2.0f}, 1.5708f, true,
       {0.3f, 1.3f, 0.3f}},
      {"prop_parking_meter", "BlkMeterB", {-10.5f, 0.f, 6.0f}, 1.5708f, true,
       {0.3f, 1.3f, 0.3f}},
      {"prop_newsbox", "BlkNews", {-9.5f, 0.f, 8.5f}, 3.1416f, true, {0.7f, 1.2f, 0.5f}},
      {"prop_bike_rack", "BlkBike", {8.5f, 0.f, 2.5f}, 0.f, true, {2.0f, 0.9f, 0.6f}},
      {"prop_sign_post", "BlkSign", {0.5f, 0.f, 4.0f}, 0.f, true, {0.25f, 2.5f, 0.25f}},
      {"prop_manhole", "BlkManhole", {3.0f, 0.02f, 6.0f}, 0.f, false, {}},
      {"prop_drain_grate", "BlkDrain", {-4.0f, 0.02f, 5.0f}, 0.f, false, {}},
      // Farther plaza density
      {"prop_bench", "BlkBenchB", {-14.0f, 0.f, 8.0f}, 1.5708f, true, {2.1f, 0.9f, 0.9f}},
      {"prop_trash_bin", "BlkTrashB", {14.0f, 0.f, 5.0f}, 0.f, true, {0.7f, 1.1f, 0.7f}},
      {"prop_bollard", "BlkBollardC", {16.0f, 0.f, -6.0f}, 0.f, true, {0.35f, 1.0f, 0.35f}},
      {"prop_planter", "BlkPlanterB", {15.0f, 0.f, -2.0f}, 0.f, true, {0.9f, 0.8f, 0.9f}},
  };

  bool any = false;
  for (const auto& p : places) {
    auto loaded = load_harbor_mesh(
        scene, p.asset,
        fury::make_box(p.solid ? p.col : Vec3{0.5f, 0.5f, 0.5f},
                       Vec3{0.45f, 0.45f, 0.48f}),
        p.name);
    place_merged(scene, loaded, p.name, p.pos, p.yaw, p.solid, p.col, true);
    any = any || loaded.from_asset;
  }

  // Optional full street kit accent near plaza (merged, detail).
  {
    auto kit = load_harbor_mesh(
        scene, "street_props_kit",
        fury::make_box({4.f, 1.f, 4.f}, Vec3{0.4f, 0.4f, 0.42f}), "street kit");
    // Offset so kit origin does not bury the entrance — east sidewalk accent.
    place_merged(scene, kit, "BlkStreetKit", {18.f, 0.f, 0.f}, 0.f, false, {},
                 true);
    if (kit.from_asset || any) {
      Log::info(kLogStreetKit);
    } else {
      Log::warn("WARNING street kit missing — Using fallback");
    }
  }
}

Vec3 spawn_meridian_getaway(fury::Scene& scene, VehicleVisualType kind) {
  // East rear alley behind Meridian Mutual — readable from bank side exit.
  const Vec3 pos{12.f, 0.f, -20.f};
  const float yaw = -1.5708f;  // face street (east→west egress feel)

  const char* asset = vehicle_asset_name(kind);
  auto body = load_harbor_mesh(
      scene, asset,
      fury::make_box({4.5f, 2.0f, 2.2f}, Vec3{0.14f, 0.16f, 0.18f}),
      kind == VehicleVisualType::CivVan ? "getaway van" : "getaway sedan");

  Entity e;
  e.name = "MeridianGetaway";
  e.tag = "getaway";
  e.mesh = body.mesh;
  e.lod_mesh = body.lod_mesh;
  e.transform.position = pos;
  e.transform.rotation_euler = {0.f, yaw, 0.f};
  e.material = body.material;
  e.solid = false;
  scene.add_entity(std::move(e));

  // Extraction pad under / near getaway for heist escape_position.
  {
    Entity pad;
    pad.name = "ExtractionPad";
    pad.tag = "escape";
    pad.mesh = scene.add_mesh(
        fury::make_box({7.f, 0.25f, 5.f}, Vec3{0.18f, 0.70f, 0.28f}));
    pad.transform.position = {pos.x, 0.15f, pos.z};
    pad.material.albedo = {0.7f, 1.2f, 0.7f};
    pad.material.roughness = 0.9f;
    pad.material.emissive = 0.25f;
    scene.add_entity(std::move(pad));
  }

  if (body.from_asset) {
    Log::info(kLogGetaway);
  } else {
    Log::warn("WARNING getaway vehicle missing — Using fallback");
  }
  return pos;
}

}  // namespace harbor
