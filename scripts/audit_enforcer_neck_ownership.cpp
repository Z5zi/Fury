// Private diagnostic: preserve the old global-Y assertion as a failed witness.
// The region ownership check uses actual canonical source UV data independently
// of the runtime radial neck-weight formula.
#define main enforcer_test_entry
#include "../tests/character_enforcer_tests.cpp"
#undef main
#include <iomanip>
int main() {
  auto p=make_character_profile(1.95f,CharacterRole::Enforcer,"NpcEnforcer",CharacterLod::Near);
  const auto&m=p.model;
  source_head(m);
  const auto protected_vertices=protected_head_runtime_vertices(m);
  unsigned cursor=0,failed=0;
  std::set<unsigned> source_failed;
  std::cout<<"source_index,original_obj_vertex,x,y,z,u,v,runtime_vertex,head_weight\n"<<std::setprecision(10);
  for(unsigned t=0;t<m.bind_mesh.indices.size();t+=3) {
    if(surface(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv)!=int(CharacterSurface::Face))continue;
    for(int k=0;k<3;++k,++cursor) {
      const auto index=enforcer_anatomy::indices[cursor],runtime=m.bind_mesh.indices[t+k];
      const auto q=enforcer_anatomy::positions[index];const auto w=m.influences[runtime];
      // Exact previous global-Y condition and influence requirement.
      if(q.y>=enforcer_anatomy::rigid_head_begin_y &&
          !(w.first==bone_index(CharacterBone::Head)&&w.first_weight==1.f&&w.third_weight==0.f)) {
        const auto uv=enforcer_anatomy::uv_coordinates[enforcer_anatomy::uv_indices[cursor]];
        require(uv.x>=.2151319f&&uv.x<=.5459291f&&uv.y>=.8406229f&&uv.y<=.9738561f,"An old-cutoff failure belongs outside the true source neck chart");
        require(!protected_vertices.count(runtime),"Protected jaw, mouth, socket or face vertex changed");
        ++failed;
        if(source_failed.insert(index).second)
          std::cout<<index<<','<<enforcer_anatomy::source_vertex_indices[index]<<','<<q.x<<','<<q.y<<','<<q.z<<','<<uv.x<<','<<uv.y<<','<<runtime<<','<<w.first_weight<<'\n';
      }
    }
  }
  std::cerr<<"FAIL retained old global-Y assertion: "<<source_failed.size()<<" true neck vertices / "<<failed<<" source corners over-constrained; zero protected head/face vertices.\n";
  require(failed>0,"Old cutoff has no reproduced witness");
  return 1;
}
