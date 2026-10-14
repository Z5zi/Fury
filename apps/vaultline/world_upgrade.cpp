#include "world_upgrade.hpp"
#include "world_architecture.hpp"
#include "world_ground.hpp"
#include "world_life.hpp"
#include "world_water.hpp"
namespace vaultline {
WorldCoverage upgrade_playable_world(fury::Scene& scene,bool enabled) {
  WorldCoverage coverage;
  const char* names[]={"metro","ridge","ashcourt","depot","loft","quay"};
  for(const char* category:{"architecture","ground","life","water"})
    for(const char* name:names) coverage[category][name]=0;
  if(!enabled) return coverage;
  const auto ground=upgrade_world_ground(scene);
  const auto water=upgrade_world_water(scene);
  const auto architecture=upgrade_world_architecture(scene);
  const auto life=upgrade_world_life(scene);
  auto& a=coverage["architecture"];
  a["metro"]=architecture.metro_shells;a["ridge"]=architecture.ridge_shells;
  a["ashcourt"]=architecture.ashcourt_shells;a["quay"]=architecture.north_quay_shells;
  a["depot"]=scene.find_by_name("Architecture.DepotRoof.Roof")?1:0;
  a["loft"]=scene.find_by_name("Architecture.LoftRoof.Roof")?1:0;
  a["eligible_shells"]=architecture.eligible_shells;a["upgraded_shells"]=architecture.upgraded_shells;
  a["protected_shells"]=architecture.protected_shells;a["landmark_exteriors"]=architecture.landmark_exteriors;
  a["window_bays"]=architecture.window_bays;a["hidden_window_strips"]=architecture.hidden_window_strips;
  a["added_entities"]=architecture.added_entities;a["triangles"]=architecture.triangles;
  auto& g=coverage["ground"];
  for(unsigned i=0;i<6;++i) g[names[i]]=ground.district_features[i];
  g["replaced_surfaces"]=ground.replaced_surfaces;g["added_entities"]=ground.added_entities;
  g["added_triangles"]=ground.added_triangles;g["terrain_triangles"]=ground.terrain_triangles;
  g["frontage_walks"]=ground.frontage_walks;g["curb_segments"]=ground.curb_segments;
  g["road_markings"]=ground.road_markings;g["waterfront_edges"]=ground.waterfront_edges;
  g["pier_boards"]=ground.pier_boards;g["protected_solid_footprints"]=ground.protected_solid_footprints;
  g["water_apertures"]=ground.water_apertures;
  g["water_aperture_square_centimetres"]=static_cast<std::uint64_t>(ground.water_aperture_area*10000.f+.5f);
  g["exposed_water_square_centimetres"]=static_cast<std::uint64_t>(ground.exposed_water_area*10000.f+.5f);
  auto& l=coverage["life"];
  for(unsigned i=0;i<6;++i) l[names[i]]=life.district_plants[i];
  l["authored_planters"]=life.authored_planters;
  l["removed_foliage_triangles"]=life.removed_foliage_triangles;
  l["skipped_authored_planters"]=life.skipped_authored_planters;
  l["replaced_entities"]=life.replaced_entities;l["added_batches"]=life.added_batches;
  l["plant_instances"]=life.plant_instances;l["utility_props"]=life.utility_props;
  l["containers"]=life.containers;l["crane_parts"]=life.crane_parts;l["market_parts"]=life.market_parts;
  l["near_triangles"]=life.near_triangles;l["lod_triangles"]=life.lod_triangles;
  l["replaced_triangles"]=life.replaced_triangles;
  auto& w=coverage["water"];
  w["metro"]=water.harbor_surfaces;w["ridge"]=water.ridge_surfaces;w["quay"]=water.north_quay_surfaces;
  w["updated_surfaces"]=water.updated_surfaces;w["shared_map_bytes"]=water.shared_map_bytes;
  w["preserved_source_materials"]=water.preserved_source_materials;w["added_triangles"]=water.added_triangles;
  return coverage;
}
}
