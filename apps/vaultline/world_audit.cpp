#include "world_audit.hpp"
#include <cmath>
#include <cstring>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <unordered_set>
#include <unordered_map>

namespace vaultline {
namespace {
void hash_word(std::uint64_t& hash,std::uint32_t word) {
  for(unsigned shift=0;shift<32;shift+=8) { hash^=(word>>shift)&255u;hash*=1099511628211ull; }
}
void hash_float(std::uint64_t& hash,float value) { std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));hash_word(hash,bits); }
void hash_vec(std::uint64_t& hash,const fury::Vec3& value) { hash_float(hash,value.x);hash_float(hash,value.y);hash_float(hash,value.z); }
void hash_string(std::uint64_t& hash,const std::string& value) { hash_word(hash,static_cast<std::uint32_t>(value.size()));for(unsigned char c:value){hash^=c;hash*=1099511628211ull;} }
std::string hex(std::uint64_t value) { std::ostringstream s;s<<std::hex<<std::setfill('0')<<std::setw(16)<<value;return s.str(); }
bool finite(const fury::Vec3& v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
bool equal(const fury::Vec3& a,const fury::Vec3& b) { return a.x==b.x&&a.y==b.y&&a.z==b.z; }
bool same(const ProtectedWorldEntity& a,const ProtectedWorldEntity& b) {
  return a.name==b.name&&a.tag==b.tag&&a.solid==b.solid&&
    equal(a.transform.position,b.transform.position)&&equal(a.transform.rotation_euler,b.transform.rotation_euler)&&
    equal(a.transform.scale,b.transform.scale)&&equal(a.collider.center,b.collider.center)&&equal(a.collider.half_extents,b.collider.half_extents);
}
void str(std::ostream& out,const std::string& s) {
  out<<'"';
  for(unsigned char c:s) {
    if(c=='"'||c=='\\') out<<'\\'<<char(c);
    else if(c<32) { const char h[]="0123456789abcdef";out<<"\\u00"<<h[c>>4]<<h[c&15]; }
    else out<<char(c);
  }
  out<<'"';
}
void vec(std::ostream& out,const fury::Vec3& v) {
  out<<'[';bool first=true;
  for(float value:{v.x,v.y,v.z}) {
    if(!first)out<<',';
    first=false;
    if(std::isfinite(value))out<<value;else out<<"null";
  }
  out<<']';
}
void counts(std::ostream& out,const WorldSceneCounts& c) {
  out<<"{\"entities\":"<<c.entities<<",\"visible_entities\":"<<c.visible_entities
     <<",\"triangles\":"<<c.triangles<<",\"unique_mesh_triangles\":"<<c.unique_mesh_triangles
     <<",\"solids\":"<<c.solids<<",\"tagged_entities\":"<<c.tagged_entities
     <<",\"invalid_indices\":"<<c.invalid_indices<<",\"nonfinite_vertices\":"<<c.nonfinite_vertices
     <<",\"degenerate_triangles\":"<<c.degenerate_triangles<<",\"zero_area_triangles\":"<<c.zero_area_triangles<<",\"nonfinite_instances\":"<<c.nonfinite_instances<<'}';
}
}
WorldSceneSnapshot snapshot_world(const fury::Scene& scene) {
  WorldSceneSnapshot result;
  std::unordered_set<const fury::Mesh*> visible_meshes;
  std::unordered_map<const fury::Mesh*,std::uint32_t> mesh_ids;
  auto& hash=result.geometry_fingerprint;
  const auto inspect_mesh=[&](const fury::Mesh* mesh) {
    if(!mesh) { hash_word(hash,0);return; }
    const auto found=mesh_ids.find(mesh);
    if(found!=mesh_ids.end()) { hash_word(hash,found->second);return; }
    const auto id=static_cast<std::uint32_t>(mesh_ids.size()+1);
    mesh_ids.emplace(mesh,id);hash_word(hash,id);
    const auto& m=*mesh;
    hash_word(hash,static_cast<std::uint32_t>(m.vertices.size()));
    hash_word(hash,static_cast<std::uint32_t>(m.indices.size()));
    for(const auto& v:m.vertices) {
      hash_vec(hash,v.position);hash_vec(hash,v.normal);hash_vec(hash,v.color);
      hash_float(hash,v.uv.x);hash_float(hash,v.uv.y);hash_float(hash,v.opacity);
      if(!finite(v.position)||!finite(v.normal)||!finite(v.color)||!std::isfinite(v.uv.x)||!std::isfinite(v.uv.y)||!std::isfinite(v.opacity))
        ++result.counts.nonfinite_vertices;
    }
    for(auto index:m.indices) hash_word(hash,index);
    if(m.indices.size()%3) ++result.counts.invalid_indices;
    for(std::size_t i=0;i+2<m.indices.size();i+=3) {
      const auto a=m.indices[i],b=m.indices[i+1],c=m.indices[i+2];
      if(a>=m.vertices.size()||b>=m.vertices.size()||c>=m.vertices.size()) { ++result.counts.invalid_indices;continue; }
      const auto n=fury::cross(m.vertices[b].position-m.vertices[a].position,m.vertices[c].position-m.vertices[a].position);
      const float area_squared=fury::dot(n,n);
      if(area_squared<1e-14f) ++result.counts.degenerate_triangles;
      if(area_squared==0.f) ++result.counts.zero_area_triangles;
    }
  };
  for(const auto& e:scene.entities()) {
    result.protected_entities.push_back({e.name,e.tag,e.solid,e.transform,e.collider});
    ++result.counts.entities;
    hash_string(hash,e.name);hash_string(hash,e.tag);hash_word(hash,e.visible?1u:0u);hash_word(hash,e.solid?1u:0u);
    hash_word(hash,e.detail?1u:0u);
    hash_vec(hash,e.transform.position);hash_vec(hash,e.transform.rotation_euler);hash_vec(hash,e.transform.scale);
    hash_vec(hash,e.collider.center);hash_vec(hash,e.collider.half_extents);
    hash_vec(hash,e.material.albedo);hash_float(hash,e.material.roughness);hash_float(hash,e.material.metallic);
    hash_word(hash,static_cast<std::uint32_t>(e.material.texture));hash_word(hash,static_cast<std::uint32_t>(e.material.detail_texture));
    hash_word(hash,e.material.detail_rotation);hash_word(hash,e.material.detail_use_mesh_uvs?1u:0u);hash_float(hash,e.material.world_uv_scale);
    const auto& m=e.material;
    if(!finite(e.transform.position)||!finite(e.transform.rotation_euler)||!finite(e.transform.scale)||
       !finite(e.collider.center)||!finite(e.collider.half_extents)||!finite(m.albedo)||!finite(m.emissive_color)||
       !std::isfinite(m.metallic)||!std::isfinite(m.roughness)||!std::isfinite(m.emissive)||
       !std::isfinite(m.opacity)||!std::isfinite(m.transmission)||!std::isfinite(m.index_of_refraction)||
       !std::isfinite(m.normal_scale)||!std::isfinite(m.world_uv_scale)||!std::isfinite(m.uv_scroll_u)||!std::isfinite(m.uv_scroll_v))
      ++result.counts.nonfinite_instances;
    // Bindings are hashed on every instance, including hidden and far-LOD
    // meshes. Geometry is scanned once per unique pointer in traversal order.
    inspect_mesh(e.mesh);inspect_mesh(e.lod_mesh);
    if(e.solid) ++result.counts.solids;
    if(!e.tag.empty()) ++result.counts.tagged_entities;
    if(!e.visible||!e.mesh) continue;
    ++result.counts.visible_entities;
    result.counts.triangles+=e.mesh->indices.size()/3;
    if(visible_meshes.insert(e.mesh).second)
      result.counts.unique_mesh_triangles+=e.mesh->indices.size()/3;
  }
  return result;
}
bool write_world_audit(const std::string& path,const fury::Scene& scene,
                       const WorldSceneSnapshot& before,bool enabled,const WorldCoverage& coverage) {
  const auto after=snapshot_world(scene);
  bool preserved=after.protected_entities.size()>=before.protected_entities.size();
  for(std::size_t i=0;preserved&&i<before.protected_entities.size();++i)
    preserved=same(before.protected_entities[i],after.protected_entities[i]);
  std::uint64_t added_solids=0;
  for(std::size_t i=before.protected_entities.size();i<after.protected_entities.size();++i)
    if(after.protected_entities[i].solid) ++added_solids;
  std::ofstream out(path);if(!out) return false;
  out<<std::setprecision(9)<<"{\n\"schema\":1,\"enabled\":"<<(enabled?"true":"false")<<",\n\"before\":";
  counts(out,before.counts);out<<",\n\"after\":";counts(out,after.counts);
  out<<",\n\"before_geometry_fingerprint\":\""<<hex(before.geometry_fingerprint)<<"\",\"after_geometry_fingerprint\":\""<<hex(after.geometry_fingerprint)<<"\"";
  out<<",\n\"original_entities_preserved\":"<<(preserved?"true":"false")<<",\"added_solids\":"<<added_solids<<",\n\"coverage\":{";
  bool first=true;
  for(const auto& group:coverage) {
    if(!first)out<<',';
    first=false;str(out,group.first);out<<":{";bool item_first=true;
    for(const auto& item:group.second){if(!item_first)out<<',';item_first=false;str(out,item.first);out<<':'<<item.second;}
    out<<'}';
  }
  out<<"},\n\"protected_entities\":[\n";
  for(std::size_t i=0;i<before.protected_entities.size()&&i<after.protected_entities.size();++i) {
    if(i)out<<",\n";
    const auto& e=after.protected_entities[i];out<<"{\"name\":";str(out,e.name);out<<",\"tag\":";str(out,e.tag);
    out<<",\"solid\":"<<(e.solid?"true":"false")<<",\"position\":";vec(out,e.transform.position);
    out<<",\"rotation\":";vec(out,e.transform.rotation_euler);out<<",\"scale\":";vec(out,e.transform.scale);
    out<<",\"collider_center\":";vec(out,e.collider.center);out<<",\"collider_half_extents\":";vec(out,e.collider.half_extents);out<<'}';
  }
  out<<"\n]}\n";return out.good();
}
} // namespace vaultline
