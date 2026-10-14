#include "harbor_assets.hpp"
#include "fury/surface_detail.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string_view>
#include <unordered_map>

namespace harbor {
namespace {

using fury::Entity;
using fury::Log;
using fury::Material;
using fury::Mesh;
using fury::TextureSlot;
using fury::Vec3;

bool begins(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}
bool ends(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() &&
         text.substr(text.size() - suffix.size()) == suffix;
}
bool begins_any(std::string_view text,
                std::initializer_list<std::string_view> prefixes) {
  for (auto prefix : prefixes) if (begins(text, prefix)) return true;
  return false;
}

TextureSlot hero_profile(std::string_view asset, std::string_view name) {
  // Explicit source-name allowlists, checked against the shipped GLBs. Do not
  // infer surfaces from color/metallicity: decals and branding share factors.
  if (asset == "storefront" && begins(name, "hm_storefront_v10_")) {
    const auto part = name.substr(std::strlen("hm_storefront_v10_"));
    if (part == "shell" || begins_any(part, {"lpier_", "rpier_"}))
      return TextureSlot::Brick;
    if (part == "fdoor") return TextureSlot::Wood;
    const bool window = begins_any(part, {"fshop_", "fuw_", "rshop_", "ruw_", "lw_", "rw_"});
    if ((window && (ends(part, "_sill") || ends(part, "_lint"))) ||
        begins_any(part, {"c0_belt_", "c1_belt_", "fpil_", "fpilcap_", "fjamb_", "rpad_", "cap_"}) ||
        (begins(part, "corn_") && (ends(part, "_0") || ends(part, "_2") || ends(part, "_4"))) ||
        part == "fdoor_head" || part == "roof" || part == "frecess")
      return TextureSlot::Concrete;
    if ((window && (ends(part, "_frm") || ends(part, "_vm") || ends(part, "_hm"))) ||
        part == "alley_door" || part == "alley_frm" ||
        begins_any(part, {"arm_", "ladder_", "lrail_", "par_par_"}))
      return TextureSlot::BarrelMetal;
    if (part == "fhandle" || part == "pushplate" || part == "kick" ||
        part == "gutter" || part == "duct" ||
        begins_any(part, {"par_flash_", "hvac_", "stack_", "rpipe_", "down_", "shoe_", "rvent_", "rventcap_"}))
      return TextureSlot::Metal;
  } else if (asset == "bank_vault_door") {
    if (name == "VD_Door" || name == "VD_DoorEdge" || name == "VD_Threshold" ||
        name == "VD_WheelHub" || name == "VD_WheelMount" || name == "VD_WheelRim" ||
        begins_any(name, {"VD_FaceRing_", "VD_WheelArm_", "VD_HingePlate_", "VD_InnerSteel"}))
      return TextureSlot::Metal;
  } else if (asset == "bank_deposit_boxes") {
    if (name == "DB_Frame" || begins_any(name, {"DB_Door_", "DB_Div_", "DB_Rail_", "DB_Hinge_"}))
      return TextureSlot::Metal;
  } else if (asset == "bank_teller_counter") {
    if (name == "TC_Kick" || name == "TC_TrimB" || name == "TC_TrimF" ||
        name == "TC_GTop" || begins_any(name, {"TC_GPost_", "TC_Tray_", "TC_Handle_"}))
      return TextureSlot::Metal;
  } else if (asset == "bank_security_desk") {
    if (name == "SD_Top" || name == "SD_Riser" || begins_any(name, {"SD_TVent_", "SD_PedH_"}))
      return TextureSlot::Metal;
  } else if (asset == "bank_trim_kit") {
    if (name == "TR_Wainscot") return TextureSlot::Concrete;
    if (name == "TR_Baseboard" || name == "TR_Corner" || name == "TR_Cove")
      return TextureSlot::Metal;
  } else if (asset == "bank_interior_kit") {
    if (name == "KIT_WallBack" || name == "KIT_WallL" || name == "KIT_WallR")
      return TextureSlot::Concrete;
    if (name == "KIT_Ceil" || begins_any(name, {"KIT_BB_", "KIT_CeilAccess_", "KIT_Vent_", "KIT_VentSlat_"}))
      return TextureSlot::Metal;
  } else if (asset == "prop_bench") {
    if (begins_any(name, {"Bench_Seat_", "Bench_Back_"})) return TextureSlot::Wood;
    if (begins(name, "Bench_Leg_")) return TextureSlot::Concrete;
    if (name == "Bench_Rail" || name == "Bench_SeatRail" ||
        begins_any(name, {"Bench_Arm_", "Bench_ArmPost_", "Bench_EndCap_"}))
      return TextureSlot::BarrelMetal;
  }
  return TextureSlot::None;
}

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
    {"bank_annex", "harbor_metro/hm_bank_annex_v10.glb", nullptr,
     "harbor_metro/hm_bank_annex_v10.obj"},
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
    {"civ_sedan_ashcourt_apertures", "harbor_metro/hm_parked_sedan_aperture_v1.glb",
     "harbor_metro/hm_parked_sedan_aperture_v1_lod1.glb", nullptr},
    {"civ_hatch", "harbor_metro/hm_civ_hatch_v3.glb",
     "harbor_metro/hm_civ_hatch_v3_lod1.glb", "harbor_metro/hm_civ_hatch_v3.obj"},
    {"civ_van", "harbor_metro/hm_civ_van_v3.glb",
     "harbor_metro/hm_civ_van_v3_lod1.glb", "harbor_metro/hm_civ_van_v3.obj"},
    {"buildings_kit", "harbor_metro/hm_buildings_kit_v10.glb",
     "harbor_metro/hm_buildings_kit_v10_lod1.glb",
     "harbor_metro/hm_buildings_kit_v10.obj"},
};

bool is_helper_prim(const std::string& name) {
  // Keep KIT_/bank wear meshes (KIT_Scuff_*, VD grease) — only drop vehicle helpers.
  return name.find("ground_walk") != std::string::npos ||
         name.find("ground_curb") != std::string::npos ||
         name.find("Shadow") != std::string::npos ||
         name.find("SaltRing") != std::string::npos ||
         name.find("xmem") != std::string::npos ||
         name.find("StreetWalk") != std::string::npos;
}

bool name_starts_with(const std::string& name, const char* prefix) {
  if (!prefix || !prefix[0]) {
    return true;
  }
  const std::size_t n = std::strlen(prefix);
  return name.size() >= n && name.compare(0, n, prefix) == 0;
}

bool same_material(const Material& a, const Material& b) {
  // Compare every authored property. Quantizing a partial key used to merge
  // opaque/glass/masked surfaces and could silently discard material state.
  return a.albedo.x == b.albedo.x && a.albedo.y == b.albedo.y &&
         a.albedo.z == b.albedo.z && a.metallic == b.metallic &&
         a.roughness == b.roughness && a.emissive == b.emissive &&
         a.emissive_color.x == b.emissive_color.x &&
         a.emissive_color.y == b.emissive_color.y &&
         a.emissive_color.z == b.emissive_color.z &&
         a.texture == b.texture && a.textures == b.textures &&
         a.detail_texture == b.detail_texture && a.world_uv_scale == b.world_uv_scale &&
         a.detail_rotation == b.detail_rotation && a.detail_use_mesh_uvs == b.detail_use_mesh_uvs &&
         a.uv_scroll_u == b.uv_scroll_u && a.uv_scroll_v == b.uv_scroll_v &&
         a.wetness == b.wetness && a.transmission == b.transmission &&
         a.index_of_refraction == b.index_of_refraction &&
         a.opacity == b.opacity && a.alpha_cutoff == b.alpha_cutoff &&
         a.normal_scale == b.normal_scale && a.double_sided == b.double_sided &&
         a.alpha_blend == b.alpha_blend;
}

void append_baked(Mesh& out, const fury::GltfPrimitive& prim,
                  bool bake_albedo = false, float longitudinal_uv_scale = 0.f) {
  const auto base = static_cast<std::uint32_t>(out.vertices.size());
  fury::Mat4 inverse_world;
  const fury::Mat4 normal_matrix = fury::inverse(prim.transform, inverse_world)
      ? fury::transpose(inverse_world) : prim.transform;
  const auto& m = prim.transform;
  const Vec3 x{m.m[0], m.m[1], m.m[2]};
  const Vec3 y{m.m[4], m.m[5], m.m[6]};
  const Vec3 z{m.m[8], m.m[9], m.m[10]};
  const bool mirrored = fury::dot(x, fury::cross(y, z)) < 0.f;
  // Bench slats need an asset-local grain direction that follows each bench's
  // eventual yaw. Bake only their image-free runtime copies; source UVs/maps
  // remain untouched. Face-local seams duplicate corners without moving them.
  std::vector<fury::Vertex> longitudinal_vertices;
  auto& vertices = longitudinal_uv_scale > 0.f ? longitudinal_vertices : out.vertices;
  vertices.reserve(vertices.size() + prim.mesh->vertices.size());
  for (const auto& v : prim.mesh->vertices) {
    fury::Vertex nv = v;
    nv.position = fury::transform_point(prim.transform, v.position);
    nv.normal = fury::normalize(fury::transform_direction(normal_matrix, v.normal));
    if (bake_albedo) {
      nv.color.x *= prim.material.albedo.x;
      nv.color.y *= prim.material.albedo.y;
      nv.color.z *= prim.material.albedo.z;
    }
    vertices.push_back(nv);
  }
  for (std::size_t i = 0; i < prim.mesh->indices.size(); i += 3) {
    const std::uint32_t indices[] = {prim.mesh->indices[i],
      prim.mesh->indices[i + (mirrored ? 2 : 1)],
      prim.mesh->indices[i + (mirrored ? 1 : 2)]};
    if (longitudinal_uv_scale > 0.f) {
      const auto& a=vertices[indices[0]].position;
      const Vec3 n=fury::normalize(fury::cross(vertices[indices[1]].position-a,
                                              vertices[indices[2]].position-a));
      Vec3 v=Vec3{1,0,0}-n*n.x;
      // On cut ends, X is nearly normal to the face. Local Y is a stable,
      // in-plane fallback; unlike dividing by a tiny projection it cannot
      // collapse the UVs or amplify tiny normal changes along the bevel.
      if (fury::dot(v,v) < .01f) v=Vec3{0,1,0}-n*n.y;
      v=fury::normalize(v);
      const Vec3 u=fury::normalize(fury::cross(v,n));
      for (auto index:indices) {
        auto vertex=vertices[index];
        vertex.uv={fury::dot(vertex.position,u)*longitudinal_uv_scale,
                   fury::dot(vertex.position,v)*longitudinal_uv_scale};
        out.indices.push_back(static_cast<std::uint32_t>(out.vertices.size()));
        out.vertices.push_back(vertex);
      }
    } else {
      for (auto index:indices) out.indices.push_back(base + index);
    }
  }
}

Mesh merge_gltf_filtered(const fury::GltfAsset& asset, Material& out_mat,
                         bool& got_mat) {
  Mesh out;
  got_mat = false;
  for (const auto& prim : asset.primitives) {
    if (is_helper_prim(prim.name) || !prim.mesh) continue;
    if (!got_mat) {
      out_mat = prim.material;
      got_mat = true;
    }
    // The single-mesh traffic/prop API cannot represent multiple roughness or
    // transmission values. Preserve the authored base colors in vertex colors
    // instead of tinting the whole car with whichever material loads first.
    // For full PBR fidelity callers should use load_harbor_material_groups.
    append_baked(out, prim, true);
  }
  if (got_mat) out_mat.albedo = {1.f, 1.f, 1.f};
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
                                   const char* log_label,
                                   const char* keep_name_prefix) {
  HarborPrimSet set;
  const char* label = log_label ? log_label : desc.name;

  fury::GltfAsset asset;
  std::string error;
  if (desc.glb && load_gltf_relative(desc.glb, asset, error)) {
    for (const auto& prim : asset.primitives) {
      if (is_helper_prim(prim.name) || !prim.mesh) {
        continue;
      }
      if (!name_starts_with(prim.name, keep_name_prefix)) {
        continue;
      }
      HarborPrimPart part;
      part.material = hero_surface_material(desc.name, prim.name, prim.material);
      Mesh local;
      append_baked(local, prim, false,
                   part.material.detail_use_mesh_uvs ? part.material.world_uv_scale : 0.f);
      part.mesh = scene.add_mesh(std::move(local));
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

HarborPrimSet load_mat_groups_from_desc(fury::Scene& scene,
                                        const HarborAssetDesc& desc,
                                        Mesh fallback, const char* log_label) {
  HarborPrimSet set;
  const char* label = log_label ? log_label : desc.name;

  fury::GltfAsset asset;
  std::string error;
  if (desc.glb && load_gltf_relative(desc.glb, asset, error)) {
    struct Acc {
      Mesh mesh;
      Material material;
    };
    // Stable first-seen order makes captures deterministic and equality avoids
    // lossy float quantization/hash collisions in material grouping.
    std::vector<Acc> groups;
    groups.reserve(64);
    for (const auto& prim : asset.primitives) {
      if (is_helper_prim(prim.name) || !prim.mesh) continue;
      const auto material = hero_surface_material(desc.name, prim.name, prim.material);
      auto group = std::find_if(groups.begin(), groups.end(), [&](const Acc& acc) {
        return same_material(acc.material, material);
      });
      if (group == groups.end()) {
        groups.push_back({{}, material});
        group = groups.end() - 1;
      }
      append_baked(group->mesh, prim, false,
                   material.detail_use_mesh_uvs ? material.world_uv_scale : 0.f);
    }
    for (auto& group : groups) {
      if (group.mesh.vertices.empty()) continue;
      HarborPrimPart part;
      part.material = group.material;
      part.mesh = scene.add_mesh(std::move(group.mesh));
      set.parts.push_back(std::move(part));
    }
    if (!set.parts.empty()) {
      set.from_asset = true;
      Log::info(std::string("GLB material groups: ") + desc.glb + " (" + label +
                ", " + std::to_string(set.parts.size()) + " materials)");
      return set;
    }
  } else if (desc.glb) {
    Log::warn(std::string("WARNING missing ") + label +
              " glb — material-group fallback");
  }

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
struct CachedHarborMesh {
  LoadedHarborMesh loaded;
  std::uint64_t mesh_identity{}, lod_identity{};
};
std::unordered_map<std::string, CachedHarborMesh> g_merged_cache;

bool scene_owns(const fury::Scene& scene, const Mesh* mesh, std::uint64_t identity) {
  if (!mesh) return true;
  for (const auto& owned : scene.meshes()) {
    // Dereference only a currently owned object, never a cached raw pointer.
    if (owned.get() == mesh && owned->geometry_identity == identity) return true;
  }
  return false;
}

void place_emissive_box(fury::Scene& scene, const char* name, const Vec3& pos,
                        const Vec3& size, const Vec3& rgb, float emissive,
                        const char* tag) {
  Entity e;
  e.name = name;
  e.mesh = scene.add_mesh(fury::make_box(size, rgb));
  e.transform.position = pos;
  e.material.albedo = rgb;
  e.material.emissive = emissive;
  e.material.roughness = 0.85f;
  if (tag) {
    e.tag = tag;
  }
  e.detail = true;
  scene.add_entity(std::move(e));
}

void place_route_pad(fury::Scene& scene, const char* name, const Vec3& pos,
                     const Vec3& size, const Vec3& rgb, float emissive,
                     const char* tag) {
  Entity e;
  e.name = name;
  e.mesh = scene.add_mesh(fury::make_box(size, rgb));
  e.transform.position = pos;
  e.material.albedo = rgb;
  e.material.emissive = emissive;
  e.material.roughness = 0.92f;
  if (tag) {
    e.tag = tag;
  }
  e.detail = true;
  scene.add_entity(std::move(e));
}

}  // namespace

Material hero_surface_material(const char* asset_name,
                               const std::string& primitive_name,
                               const Material& authored) {
  Material result = authored;
  // glTF defaults emissive strength to one with a black emissive factor. Its
  // non-null texture marker selects independent emission instead of legacy
  // albedo-based emission, so strength alone cannot identify a glowing part.
  const bool emits = authored.emissive > 0.f && (!authored.textures ||
      authored.emissive_color.x > 0.f || authored.emissive_color.y > 0.f ||
      authored.emissive_color.z > 0.f);
  // An imported texture marker with no images is eligible. Even one authored
  // image keeps the complete original set/UVs, including emissive-only maps.
  if (!asset_name || authored.detail_texture != TextureSlot::None ||
      authored.detail_rotation != 0 || authored.detail_use_mesh_uvs ||
      authored.world_uv_scale != 0.f || authored.texture != TextureSlot::None ||
      emits || authored.transmission > 0.f ||
      authored.opacity < 1.f || authored.alpha_blend || authored.alpha_cutoff >= 0.f)
    return result;
  if (authored.textures && (authored.textures->base_color.valid() ||
      authored.textures->normal.valid() || authored.textures->metallic_roughness.valid() ||
      authored.textures->emissive.valid())) return result;
  const auto profile = hero_profile(asset_name, primitive_name);
  if (profile != TextureSlot::None) {
    result.detail_texture = profile;
    result.world_uv_scale = fury::surface_detail_uv_per_meter(profile);
    result.detail_use_mesh_uvs = std::strcmp(asset_name, "prop_bench") == 0 &&
                                profile == TextureSlot::Wood;
  }
  return result;
}

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
  const std::string key = asset_name ? asset_name : "";
  const auto it = g_merged_cache.find(key);
  if (it != g_merged_cache.end() &&
      scene_owns(scene, it->second.loaded.mesh, it->second.mesh_identity) &&
      scene_owns(scene, it->second.loaded.lod_mesh, it->second.lod_identity)) {
    return it->second.loaded;
  }
  const HarborAssetDesc* desc = find_asset(asset_name);
  LoadedHarborMesh loaded;
  if (!desc) {
    Log::warn(std::string("WARNING unknown Harbor asset '") + (asset_name ? asset_name : "") +
              "' — Using fallback");
    loaded.mesh = scene.add_mesh(std::move(fallback));
    loaded.used_fallback = true;
  } else {
    loaded = load_merged_from_desc(scene, *desc, std::move(fallback),
                                   log_label ? log_label : asset_name);
  }
  // Missing assets are not cached: a later lookup may run from a new asset root.
  if (loaded.from_asset) {
    g_merged_cache[key] = {loaded,
                         loaded.mesh ? loaded.mesh->geometry_identity : 0,
                         loaded.lod_mesh ? loaded.lod_mesh->geometry_identity : 0};
  }
  return loaded;
}

LoadedHarborMesh load_ashcourt_sedan(fury::Scene& scene, Mesh fallback,
                                    const char* log_label) {
  // This derivative belongs only to CivSedanBody. Traffic keeps the original
  // descriptor/cache. Commit both LODs together, including on load failure.
  const std::string key = "civ_sedan_ashcourt_apertures";
  const auto it = g_merged_cache.find(key);
  if (it != g_merged_cache.end() &&
      scene_owns(scene, it->second.loaded.mesh, it->second.mesh_identity) &&
      scene_owns(scene, it->second.loaded.lod_mesh, it->second.lod_identity)) {
    return it->second.loaded;
  }
  const auto* desc = find_asset(key.c_str());
  fury::GltfAsset near_asset, far_asset;
  std::string error;
  if (desc && load_gltf_relative(desc->glb, near_asset, error) &&
      load_gltf_relative(desc->lod_glb, far_asset, error)) {
    LoadedHarborMesh loaded;
    Material far_material;
    bool near_material_found = false, far_material_found = false;
    Mesh near_mesh = merge_gltf_filtered(near_asset, loaded.material, near_material_found);
    Mesh far_mesh = merge_gltf_filtered(far_asset, far_material, far_material_found);
    if (!near_mesh.vertices.empty() && !far_mesh.vertices.empty()) {
      loaded.mesh = scene.add_mesh(std::move(near_mesh));
      loaded.lod_mesh = scene.add_mesh(std::move(far_mesh));
      loaded.from_asset = true;
      g_merged_cache[key] = {loaded, loaded.mesh->geometry_identity,
                            loaded.lod_mesh->geometry_identity};
      Log::info("Ashcourt sedan aperture pair loaded");
      return loaded;
    }
  }
  Log::warn("Ashcourt sedan aperture pair unavailable; using original sedan pair");
  return load_harbor_mesh(scene, "civ_sedan", std::move(fallback), log_label);
}

HarborPrimSet load_harbor_prims(fury::Scene& scene, const char* asset_name,
                                Mesh fallback, const char* log_label,
                                const char* keep_name_prefix) {
  const HarborAssetDesc* desc = find_asset(asset_name);
  if (!desc) {
    HarborPrimSet set;
    HarborPrimPart part;
    part.mesh = scene.add_mesh(std::move(fallback));
    part.material.albedo = {0.7f, 0.7f, 0.72f};
    set.parts.push_back(part);
    set.used_fallback = true;
    Log::warn(std::string("WARNING unknown Harbor asset '") + (asset_name ? asset_name : "") +
              "' — Using fallback");
    return set;
  }
  return load_prims_from_desc(scene, *desc, std::move(fallback),
                              log_label ? log_label : asset_name,
                              keep_name_prefix);
}

HarborPrimSet load_harbor_material_groups(fury::Scene& scene,
                                          const char* asset_name, Mesh fallback,
                                          const char* log_label) {
  const HarborAssetDesc* desc = find_asset(asset_name);
  if (!desc) {
    HarborPrimSet set;
    HarborPrimPart part;
    part.mesh = scene.add_mesh(std::move(fallback));
    part.material.albedo = {0.55f, 0.55f, 0.58f};
    set.parts.push_back(part);
    set.used_fallback = true;
    Log::warn(std::string("WARNING unknown Harbor asset '") + (asset_name ? asset_name : "") +
              "' — Using fallback");
    return set;
  }
  return load_mat_groups_from_desc(scene, *desc, std::move(fallback),
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

bool replace_storefront_shell(fury::Scene& scene, const char* shell_name) {
  if (!shell_name || std::strncmp(shell_name, "Bldg", 4) != 0) return false;
  auto* shell = scene.find_by_name(shell_name);
  // This helper is deliberately limited to closed scenery boxes, never mission
  // shells, doors, tagged targets, or collision/portal changes.
  if (!shell || !shell->solid || !shell->tag.empty()) return false;
  const std::string prefix = std::string(shell_name) + "_Authored";
  if (scene.find_by_name(prefix + "_0")) return true;
  const auto target_transform = shell->transform;
  const auto collider = shell->collider;
  if (collider.half_extents.x <= 0 || collider.half_extents.y <= 0 ||
      collider.half_extents.z <= 0) return false;
  fury::GltfAsset asset;
  std::string error;
  if (!load_gltf_relative("harbor_metro/hm_storefront_v10.glb", asset, error)) {
    Log::warn("Authored storefront unavailable; keeping building box: " + error);
    return false;
  }
  auto bounds = [](const fury::GltfPrimitive& p, Vec3& lo, Vec3& hi) {
    lo = {1e30f, 1e30f, 1e30f}; hi = {-1e30f, -1e30f, -1e30f};
    for (const auto& vertex : p.mesh->vertices) {
      const auto v = fury::transform_point(p.transform, vertex.position);
      lo = {std::min(lo.x,v.x), std::min(lo.y,v.y), std::min(lo.z,v.z)};
      hi = {std::max(hi.x,v.x), std::max(hi.y,v.y), std::max(hi.z,v.z)};
    }
  };
  Vec3 source_lo{}, source_hi{};
  bool found_shell = false;
  for (const auto& p : asset.primitives) {
    if (p.mesh && p.name == "hm_storefront_v10_shell") {
      bounds(p, source_lo, source_hi); found_shell = true; break;
    }
  }
  if (!found_shell) return false;
  // The exported "individual" storefront includes meters/bins/signs up to 21m
  // away. Keep only facade/roof parts near the actual named masonry shell.
  std::vector<fury::GltfPrimitive> selected;
  Vec3 envelope_lo=source_lo, envelope_hi=source_hi;
  for (const auto& p : asset.primitives) {
    if (!p.mesh || is_helper_prim(p.name) ||
        p.name.find("_walk") != std::string::npos ||
        p.name.find("_curb") != std::string::npos) continue;
    Vec3 lo, hi; bounds(p,lo,hi);
    if (lo.x < source_lo.x-.4f || hi.x > source_hi.x+.4f ||
        lo.z < source_lo.z-.4f || hi.z > source_hi.z+.4f ||
        lo.y < source_lo.y-.05f) continue;
    selected.push_back(p);
    envelope_lo={std::min(envelope_lo.x,lo.x),std::min(envelope_lo.y,lo.y),std::min(envelope_lo.z,lo.z)};
    envelope_hi={std::max(envelope_hi.x,hi.x),std::max(envelope_hi.y,hi.y),std::max(envelope_hi.z,hi.z)};
  }
  if (selected.empty()) return false;
  // X/Z envelope stays entirely within the old footprint. Fit the structural
  // roof height, allowing authored parapets/roof equipment above it as before.
  const Vec3 size = envelope_hi-envelope_lo;
  const Vec3 fit_scale{2*collider.half_extents.x/size.x,
                       2*collider.half_extents.y/(source_hi.y-source_lo.y),
                       2*collider.half_extents.z/size.z};
  const Vec3 source_center{(envelope_lo.x+envelope_hi.x)*.5f,
                           (source_lo.y+source_hi.y)*.5f,
                           (envelope_lo.z+envelope_hi.z)*.5f};
  const auto fit=fury::translate(collider.center)*fury::scale(fit_scale)*
                 fury::translate(-source_center);
  struct Group { Mesh mesh; Material material; };
  std::vector<Group> groups;
  for (auto& p : selected) {
    p.transform=fit*p.transform;
    p.material=hero_surface_material("storefront", p.name, p.material);
    auto group=std::find_if(groups.begin(),groups.end(),[&](const Group& g) {
      return same_material(g.material,p.material);
    });
    if(group==groups.end()) { groups.push_back({{},p.material}); group=groups.end()-1; }
    append_baked(group->mesh,p);
  }
  // Remove only the old visual after source/fit validation. Collision collection
  // depends on solid/collider, never mesh/visibility. No duplicate solid is added.
  shell->mesh=nullptr;
  shell->lod_mesh=nullptr;
  std::size_t index=0;
  std::size_t triangle_count=0;
  for (auto& group : groups) {
    triangle_count+=group.mesh.indices.size()/3;
    Entity part;
    part.name=prefix+"_"+std::to_string(index++);
    part.mesh=scene.add_mesh(std::move(group.mesh));
    part.material=group.material;
    part.transform=target_transform;
    part.detail=false;
    scene.add_entity(std::move(part));
  }
  Log::info("Authored storefront: " + std::string(shell_name) + " (" +
            std::to_string(groups.size()) + " material groups, " +
            std::to_string(triangle_count) + " triangles; collider preserved)");
  return true;
}

void spawn_meridian_mutual(fury::Scene& scene) {
  // Bank shell origin matches build_meridian_mutual (cx=0, cz=-10).
  constexpr float bank_cx = 0.f;
  constexpr float bank_cz = -10.f;

  Log::info(kLogKitChoice);

  // Source of truth: modular heroes for gameplay readability.
  // Interior kit contributes ONLY KIT_* densifiers (floor/walls/lights/brochures/
  // vents/scuffs/signage) — never TC_/SD_/VD_/DB_ hero meshes stacked twice.
  {
    auto dens = load_harbor_prims(
        scene, "bank_interior_kit",
        fury::make_box({14.f, 0.2f, 12.f}, Vec3{0.75f, 0.78f, 0.82f}),
        "Meridian Mutual KIT densifiers", "KIT_");
    place_prims(scene, dens, "MMKitDens", {bank_cx, 0.f, bank_cz}, 0.f, true,
                "bank");
    if (dens.from_asset) {
      Log::info(kLogMeridianMutual);
    } else {
      Log::warn("WARNING Meridian Mutual kit densifiers missing — Using fallback");
    }
  }

  // Trim kit accents along lobby baseboards (small storytelling density).
  {
    auto trim = load_harbor_prims(
        scene, "bank_trim_kit",
        fury::make_box({2.f, 0.15f, 0.08f}, Vec3{0.55f, 0.58f, 0.62f}),
        "bank trim kit");
    place_prims(scene, trim, "MMTrimL", {bank_cx - 6.5f, 0.f, bank_cz + 1.5f},
                0.f, true);
    place_prims(scene, trim, "MMTrimR", {bank_cx + 6.5f, 0.f, bank_cz + 1.5f},
                0.f, true);
  }

  // Readable route props — modular heroes own teller / security / vault.
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
      // The inner seats used to cross Owen's unchanged lobby route. Keep their
      // visual and solid collider together, with the center aisle left open.
      const float chair_z = std::fabs(x) < 1.5f ? bank_cz + 6.05f : bank_cz + 5.1f;
      place_merged(scene, chair, n.c_str(), {x, 0.f, chair_z}, 3.1416f,
                   true, {0.7f, 0.85f, 0.7f}, true);
    }
  }
  // Extra stanchions guiding street→entrance flow.
  {
    int si = 0;
    for (float x : {-1.8f, 1.8f}) {
      auto st = load_harbor_mesh(
          scene, "bank_stanchion",
          fury::make_box({0.25f, 1.05f, 0.25f}, Vec3{0.75f, 0.72f, 0.55f}),
          "stanchion");
      const std::string n = "MMStanch" + std::to_string(si++);
      place_merged(scene, st, n.c_str(), {x, 0.f, bank_cz + 6.6f}, 0.f, true,
                   {0.3f, 1.05f, 0.3f}, true);
    }
  }

  // Vault corridor hero door + deposit boxes.
  {
    auto door = load_harbor_prims(
        scene, "bank_vault_door",
        fury::make_box({3.2f, 2.8f, 1.2f}, Vec3{0.95f, 0.72f, 0.18f}),
        "vault door");
    place_prims(scene, door, "MMVaultDoorVis", {bank_cx, 0.f, bank_cz - 5.2f},
                0.f, false, "vault_vis");
    Entity vault;
    vault.name = "VaultDoor";
    vault.tag = "vault";
    vault.mesh = scene.add_mesh(
        fury::make_box({3.2f, 2.8f, 1.0f}, Vec3{0.15f, 0.15f, 0.16f}));
    vault.transform.position = {bank_cx, 1.4f, bank_cz - 5.2f};
    vault.material.albedo = {0.2f, 0.2f, 0.22f};
    vault.material.metallic = 0.9f;
    vault.material.roughness = 0.25f;
    vault.visible = false;
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
    place_prims(scene, boxes, "MMDepositL",
                {bank_cx - 4.2f, 0.f, bank_cz - 6.0f}, 1.5708f, false);
    place_prims(scene, boxes, "MMDepositR",
                {bank_cx + 4.2f, 0.f, bank_cz - 6.0f}, -1.5708f, false);
  }

  // Security systems along restricted corridor + env storytelling clutter.
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
    auto badge = load_harbor_mesh(
        scene, "bank_badge_scanner",
        fury::make_box({0.22f, 0.35f, 0.12f}, Vec3{0.35f, 0.4f, 0.45f}),
        "badge scanner");
    place_merged(scene, badge, "MMBadgeScan",
                 {bank_cx - 3.2f, 1.25f, bank_cz - 1.6f}, 0.f, false, {}, true);
  }
  {
    auto card = load_harbor_mesh(
        scene, "bank_card_reader",
        fury::make_box({0.28f, 0.18f, 0.12f}, Vec3{0.3f, 0.32f, 0.35f}),
        "card reader");
    place_merged(scene, card, "MMCardReader",
                 {bank_cx + 1.4f, 1.15f, bank_cz + 2.4f}, 0.f, false, {}, true);
  }
  {
    auto motion = load_harbor_mesh(
        scene, "bank_motion_sensor",
        fury::make_box({0.2f, 0.12f, 0.2f}, Vec3{0.7f, 0.7f, 0.72f}),
        "motion sensor");
    place_merged(scene, motion, "MMMotion",
                 {bank_cx, 2.85f, bank_cz - 2.0f}, 0.f, false, {}, true);
  }
  {
    auto cab = load_harbor_mesh(
        scene, "bank_security_cabinet",
        fury::make_box({0.9f, 1.6f, 0.5f}, Vec3{0.3f, 0.32f, 0.36f}),
        "security cabinet");
    place_merged(scene, cab, "MMSecCab", {bank_cx + 6.8f, 0.f, bank_cz - 2.8f},
                 -1.5708f, true, {0.9f, 1.6f, 0.5f}, false);
  }

  // Tiny procedural clutter — papers / monitor stubs (no new Blender campaign).
  {
    place_emissive_box(scene, "MMPaperStackA",
                       {0.55f, 1.12f, bank_cz + 2.55f}, {0.28f, 0.04f, 0.22f},
                       {0.92f, 0.88f, 0.78f}, 0.05f, "clutter");
    place_emissive_box(scene, "MMPaperStackB",
                       {-5.1f, 1.22f, bank_cz + 0.35f}, {0.24f, 0.035f, 0.18f},
                       {0.9f, 0.86f, 0.76f}, 0.04f, "clutter");
    place_emissive_box(scene, "MMMonitorSec",
                       {-5.35f, 1.55f, bank_cz + 0.55f}, {0.42f, 0.32f, 0.08f},
                       {0.25f, 0.55f, 0.62f}, 0.85f, "lamp");
    place_emissive_box(scene, "MMMonitorTeller",
                       {-0.9f, 1.45f, bank_cz + 2.55f}, {0.38f, 0.28f, 0.07f},
                       {0.35f, 0.5f, 0.55f}, 0.55f, "lamp");
    // Service cart proxy in vault antechamber.
    place_emissive_box(scene, "MMServiceCart",
                       {bank_cx + 3.6f, 0.45f, bank_cz - 3.6f},
                       {0.7f, 0.9f, 0.45f}, {0.45f, 0.48f, 0.52f}, 0.02f,
                       "clutter");
    // Meridian Mutual wall plaque / signage cue at entrance.
    place_emissive_box(scene, "MMEntranceSign",
                       {0.f, 3.6f, bank_cz + 6.85f}, {2.4f, 0.55f, 0.12f},
                       {0.15f, 0.55f, 0.58f}, 0.45f, "signage");
  }

  // Mission lighting — warm lobby, cooler security, dramatic vault, alley escape.
  // Tagged "lamp" so night lamp_mul + dynamic point-light picker can use them.
  {
    // Lobby warm fills
    place_emissive_box(scene, "MMLampLobby0", {-3.5f, 3.6f, bank_cz + 3.5f},
                       {0.55f, 0.12f, 0.55f}, {1.f, 0.9f, 0.7f}, 1.6f, "lamp");
    place_emissive_box(scene, "MMLampLobby1", {3.5f, 3.6f, bank_cz + 3.5f},
                       {0.55f, 0.12f, 0.55f}, {1.f, 0.9f, 0.7f}, 1.6f, "lamp");
    place_emissive_box(scene, "MMLampLobby2", {0.f, 3.7f, bank_cz + 5.2f},
                       {0.7f, 0.1f, 0.7f}, {1.f, 0.92f, 0.75f}, 1.45f, "lamp");
    // Security cooler
    place_emissive_box(scene, "MMLampSec0", {-5.2f, 3.5f, bank_cz + 0.2f},
                       {0.45f, 0.1f, 0.45f}, {0.65f, 0.82f, 1.f}, 1.55f, "lamp");
    place_emissive_box(scene, "MMLampSec1", {-3.8f, 3.4f, bank_cz - 1.8f},
                       {0.4f, 0.1f, 0.4f}, {0.55f, 0.75f, 1.f}, 1.35f, "lamp");
    // Vault dramatic (gold rim + cool spill)
    place_emissive_box(scene, "MMLampVaultGold",
                       {bank_cx, 3.2f, bank_cz - 4.4f}, {0.55f, 0.12f, 0.35f},
                       {1.f, 0.78f, 0.35f}, 1.9f, "lamp");
    place_emissive_box(scene, "MMLampVaultCool",
                       {bank_cx, 2.6f, bank_cz - 6.2f}, {0.4f, 0.1f, 0.4f},
                       {0.55f, 0.7f, 1.05f}, 1.5f, "lamp");
    // Escape alley night-readable path
    place_emissive_box(scene, "MMLampAlley0", {10.5f, 3.4f, -14.5f},
                       {0.35f, 0.12f, 0.35f}, {0.95f, 0.85f, 0.55f}, 1.35f,
                       "lamp");
    place_emissive_box(scene, "MMLampAlley1", {12.f, 3.2f, -18.5f},
                       {0.35f, 0.12f, 0.35f}, {0.9f, 0.82f, 0.5f}, 1.45f,
                       "lamp");
    // Alarm accent beacon (existing heat flash hook)
    place_emissive_box(scene, "MMLampAlarmAccent",
                       {bank_cx + 7.0f, 2.8f, bank_cz - 1.0f},
                       {0.25f, 0.25f, 0.25f}, {1.f, 0.2f, 0.15f}, 0.35f,
                       "alarm_lamp");
    Log::info(kLogMissionLighting);
  }

  // Playable heist route markers: street → entrance → security → vault →
  // escape alley → getaway → HMPD approach cue.
  {
    struct Marker {
      const char* name;
      Vec3 pos;
      Vec3 size;
      Vec3 rgb;
      float em;
      const char* tag;
    };
    const Marker marks[] = {
        {"RouteStreet", {0.f, 0.06f, 2.5f}, {2.2f, 0.06f, 1.2f},
         {0.25f, 0.75f, 0.85f}, 0.35f, "route"},
        {"RouteEntrance", {0.f, 0.07f, bank_cz + 6.5f}, {2.0f, 0.06f, 1.0f},
         {0.3f, 0.85f, 0.7f}, 0.4f, "route"},
        {"RouteLobby", {0.f, 0.07f, bank_cz + 3.5f}, {1.6f, 0.05f, 0.9f},
         {0.95f, 0.85f, 0.45f}, 0.3f, "route"},
        {"RouteSecurity", {-4.2f, 0.07f, bank_cz + 0.3f}, {1.4f, 0.05f, 0.9f},
         {0.45f, 0.65f, 1.f}, 0.35f, "route"},
        {"RouteCorridor", {0.f, 0.07f, bank_cz - 2.2f}, {1.5f, 0.05f, 1.0f},
         {0.9f, 0.55f, 0.25f}, 0.4f, "route"},
        {"RouteVault", {0.f, 0.08f, bank_cz - 4.6f}, {2.0f, 0.06f, 1.1f},
         {1.f, 0.78f, 0.25f}, 0.55f, "route"},
        {"RouteEscapeSide", {8.5f, 0.07f, bank_cz - 2.5f}, {1.6f, 0.05f, 0.9f},
         {0.85f, 0.35f, 0.25f}, 0.4f, "route"},
        {"RouteEscapeAlley", {11.5f, 0.07f, -16.5f}, {1.8f, 0.05f, 1.2f},
         {0.9f, 0.4f, 0.2f}, 0.45f, "route"},
        {"RouteGetaway", {12.f, 0.08f, -20.f}, {2.4f, 0.06f, 1.6f},
         {0.35f, 1.f, 0.45f}, 0.5f, "route"},
        {"RouteHmpdCue", {16.5f, 0.07f, -12.f}, {1.8f, 0.05f, 1.2f},
         {0.25f, 0.4f, 0.95f}, 0.4f, "route"},
    };
    for (const auto& m : marks) {
      place_route_pad(scene, m.name, m.pos, m.size, m.rgb, m.em, m.tag);
    }
    // Restricted corridor floor stripe volume (gameplay-readable).
    place_route_pad(scene, "RouteRestrictStripe",
                    {-2.5f, 0.05f, bank_cz - 1.0f}, {5.5f, 0.04f, 0.35f},
                    {0.95f, 0.55f, 0.1f}, 0.25f, "route");
    Log::info(kLogHeistRoute);
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
  const PropPlace places[] = {
      {"prop_atm", "BlkAtmA", {-7.5f, 0.f, -2.2f}, 3.1416f, true, {1.0f, 1.6f, 0.75f}},
      {"prop_atm", "BlkAtmB", {-6.2f, 0.f, -2.2f}, 3.1416f, true, {1.0f, 1.6f, 0.75f}},
      {"prop_bench", "BlkBenchA", {4.5f, 0.f, -1.5f}, 0.f, true, {2.1f, 0.9f, 0.9f}},
      {"prop_bollard", "BlkBollardA", {-2.8f, 0.f, -2.5f}, 0.f, true, {0.35f, 1.0f, 0.35f}},
      {"prop_bollard", "BlkBollardB", {2.8f, 0.f, -2.5f}, 0.f, true, {0.35f, 1.0f, 0.35f}},
      {"prop_trash_bin", "BlkTrashA", {6.5f, 0.f, -1.8f}, 0.f, true, {0.7f, 1.1f, 0.7f}},
      {"prop_planter", "BlkPlanterA", {-9.0f, 0.f, -3.5f}, 0.f, true, {0.9f, 0.8f, 0.9f}},
      {"prop_utility_cabinet", "BlkUtilA", {11.5f, 0.f, -8.0f}, -1.5708f, true,
       {0.8f, 1.5f, 0.5f}},
      {"prop_barrier_set", "BlkBarrierA", {12.5f, 0.f, -14.0f}, 0.f, true,
       {2.0f, 1.2f, 0.6f}},
      {"bank_camera_dome", "BlkCamAlley", {11.0f, 2.6f, -12.0f}, 0.f, false, {}},
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
      {"prop_bench", "BlkBenchB", {-14.0f, 0.f, 8.0f}, 1.5708f, true, {2.1f, 0.9f, 0.9f}},
      {"prop_trash_bin", "BlkTrashB", {14.0f, 0.f, 5.0f}, 0.f, true, {0.7f, 1.1f, 0.7f}},
      {"prop_bollard", "BlkBollardC", {16.0f, 0.f, -6.0f}, 0.f, true, {0.35f, 1.0f, 0.35f}},
      {"prop_planter", "BlkPlanterB", {15.0f, 0.f, -2.0f}, 0.f, true, {0.9f, 0.8f, 0.9f}},
      // Escape-alley densify toward getaway
      {"prop_barrier_set", "BlkBarrierB", {14.5f, 0.f, -18.0f}, 1.5708f, true,
       {2.0f, 1.2f, 0.6f}},
      {"prop_trash_bin", "BlkTrashAlley", {10.2f, 0.f, -17.5f}, 0.f, true,
       {0.7f, 1.1f, 0.7f}},
      {"prop_bollard", "BlkBollardAlley", {9.5f, 0.f, -15.0f}, 0.f, true,
       {0.35f, 1.0f, 0.35f}},
  };

  bool any = false;
  for (const auto& p : places) {
    if (std::strcmp(p.asset, "prop_bench") == 0) {
      // Keep the named collision root at exactly the old pose/size. Separate
      // groups let timber, cast-concrete legs and coated frames shade correctly;
      // their tiny asset plates and preauthored wear keep their own material.
      const auto groups = load_harbor_material_groups(scene, p.asset,
          fury::make_box(p.col, {0.45f, 0.45f, 0.48f}), p.name);
      place_merged(scene, {}, p.name, p.pos, p.yaw, p.solid, p.col, true);
      const std::string visual_name = std::string(p.name) + "_Surface";
      place_prims(scene, groups, visual_name.c_str(), p.pos, p.yaw, true);
      any = any || groups.from_asset;
      continue;
    }
    auto loaded = load_harbor_mesh(
        scene, p.asset,
        fury::make_box(p.solid ? p.col : Vec3{0.5f, 0.5f, 0.5f},
                       Vec3{0.45f, 0.45f, 0.48f}),
        p.name);
    place_merged(scene, loaded, p.name, p.pos, p.yaw, p.solid, p.col, true);
    any = any || loaded.from_asset;
  }

  {
    auto kit = load_harbor_mesh(
        scene, "street_props_kit",
        fury::make_box({4.f, 1.f, 4.f}, Vec3{0.4f, 0.4f, 0.42f}), "street kit");
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
  const Vec3 pos{12.f, 0.f, -20.f};
  const float yaw = -1.5708f;

  const char* asset = vehicle_asset_name(kind);
  // Material groups preserve paint / glass / trim for the hero getaway.
  auto body = load_harbor_material_groups(
      scene, asset,
      fury::make_box({4.5f, 2.0f, 2.2f}, Vec3{0.14f, 0.16f, 0.18f}),
      kind == VehicleVisualType::CivVan ? "getaway van" : "getaway sedan");

  place_prims(scene, body, "MeridianGetaway", pos, yaw, false, "getaway");

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
