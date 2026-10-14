// Additive diagnostic only. Reuses the unchanged authored world constructors.
// The renamed application entry is discarded by the linker's section GC.
#define main original_vaultline_application_main
#include "../apps/vaultline/main.cpp"
#undef main
#include <iostream>
int main(int argc,char**argv) {
  if(argc!=2)return 2;
  fury::Scene scene;build_harbor_metro(scene);
  vaultline::upgrade_playable_world(scene,true);
  std::ofstream out(argv[1]);out<<std::setprecision(10);
  out<<"{\"kind\":\"authored-playable-world-static-clearance\",\"limits\":\"Static geometry after world-art upgrade; no runtime vehicles, actors, or dynamic states\",\"entities\":[";
  bool first=true;unsigned index=0;
  for(const auto&e:scene.entities()) {
    if(!e.mesh){++index;continue;}
    if(!first)out<<',';first=false;
    const auto mat=e.transform.matrix();
    fury::Vec3 lo{1e9,1e9,1e9},hi{-1e9,-1e9,-1e9};
    auto emit=[&](fury::Vec3 p){out<<'['<<p.x<<','<<p.y<<','<<p.z<<']';};
    for(const auto&v:e.mesh->vertices){auto p=fury::transform_point(mat,v.position);lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
    out<<"{\"index\":"<<index++<<",\"name\":"<<std::quoted(e.name)<<",\"visible\":"<<e.visible<<",\"solid\":"<<e.solid<<",\"bounds\":[";emit(lo);out<<',';emit(hi);out<<"],\"band_triangles\":[";
    bool first_triangle=true;
    for(unsigned i=0;i+2<e.mesh->indices.size();i+=3){
      fury::Vec3 p[3];float miny=1e9,maxy=-1e9;
      for(unsigned k=0;k<3;++k){p[k]=fury::transform_point(mat,e.mesh->vertices[e.mesh->indices[i+k]].position);miny=std::min(miny,p[k].y);maxy=std::max(maxy,p[k].y);}
      // Includes any triangle passing through the complete observed upright/crouch height band.
      if(maxy<.65f||miny>2.05f)continue;
      if(!first_triangle)out<<',';first_triangle=false;out<<'[';for(unsigned k=0;k<3;++k){if(k)out<<',';emit(p[k]);}out<<']';
    }
    out<<"]}";
  }
  out<<"],\"solids\":[";first=true;
  for(const auto&box:scene.collect_solids()){if(!first)out<<',';first=false;out<<'[';out<<box.center.x-box.half_extents.x<<','<<box.center.y-box.half_extents.y<<','<<box.center.z-box.half_extents.z<<','<<box.center.x+box.half_extents.x<<','<<box.center.y+box.half_extents.y<<','<<box.center.z+box.half_extents.z<<']';}
  out<<"]}\n";return out?0:1;
}
