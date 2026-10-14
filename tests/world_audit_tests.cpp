#include "../apps/vaultline/world_audit.hpp"
#include "../apps/vaultline/world_views.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
fury::Entity instance(fury::Mesh* mesh,const char* name) { fury::Entity e;e.mesh=mesh;e.name=name;return e; }
}
int main() {
  try {
    const auto* street=vaultline::world_capture_view("metro-street");
    require(street!=nullptr,"Metro street capture view missing");
    // Existing GhostLoop box is centered at (8,.9,10), sized (.8,1.8,.8).
    require(std::fabs(street->eye.x-8.f)>.5f || std::fabs(street->eye.z-10.f)>.5f || street->eye.y>1.9f,
            "Street capture eye intersects the legacy ghost's top face");
    fury::Scene scene;
    auto* a=scene.add_mesh(fury::make_box({2,2,2},{1,0,0}));
    auto* b=scene.add_mesh(fury::make_box({2,2,2},{0,0,1}));
    for(const auto& e:{instance(a,"first-a"),instance(b,"first-b"),instance(a,"shared-a"),instance(b,"shared-b")}) scene.add_entity(e);
    const auto baseline=vaultline::snapshot_world(scene);
    require(baseline.counts.entities==4 && baseline.counts.triangles==48 && baseline.counts.unique_mesh_triangles==24,"scene counts mismatch");
    require(!baseline.counts.invalid_indices && !baseline.counts.nonfinite_vertices && !baseline.counts.nonfinite_instances,"valid fixture marked invalid");
    require(vaultline::snapshot_world(scene).geometry_fingerprint==baseline.geometry_fingerprint,"fingerprint is unstable");
    scene.entities()[2].mesh=b;scene.entities()[3].mesh=a;
    require(vaultline::snapshot_world(scene).geometry_fingerprint!=baseline.geometry_fingerprint,"shared-mesh binding swap missed");
    scene.entities()[2].mesh=a;scene.entities()[3].mesh=b;
    scene.entities()[0].visible=false;
    require(vaultline::snapshot_world(scene).geometry_fingerprint!=baseline.geometry_fingerprint,"visibility change missed");
    auto* lod=scene.add_mesh(*a);
    lod->indices[0]=static_cast<std::uint32_t>(lod->vertices.size()+1);
    scene.entities()[0].lod_mesh=lod;
    require(vaultline::snapshot_world(scene).counts.invalid_indices==1,"hidden LOD invalid index missed");
    lod->indices[0]=a->indices[0];
    lod->vertices[0].normal.x=std::numeric_limits<float>::quiet_NaN();
    require(vaultline::snapshot_world(scene).counts.nonfinite_vertices==1,"hidden LOD nonfinite vertex missed");
    lod->vertices[0].normal=a->vertices[0].normal;
    lod->indices[1]=lod->indices[0];
    const auto broken=vaultline::snapshot_world(scene);
    require(broken.counts.zero_area_triangles==1 && broken.counts.degenerate_triangles==1,"exact zero-area LOD missed");
    scene.entities()[1].transform.scale.x=std::numeric_limits<float>::infinity();
    require(vaultline::snapshot_world(scene).counts.nonfinite_instances==1,"nonfinite instance missed");
    std::cout<<"World audit counts, near/LOD diagnostics and per-instance fingerprints passed\n";
    return 0;
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
