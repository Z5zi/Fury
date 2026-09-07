#include "coastal_scene.hpp"
#include <cmath>
#include <map>
#include <limits>
#include <stdexcept>
#include "fury/log.hpp"

using namespace fury;
namespace {
constexpr float pi=3.14159265359f;
Mesh cylinder(float radius,float height,unsigned sides=20) {
  Mesh m;
  for(unsigned i=0;i<=sides;++i) {
    const float a=float(i)/float(sides)*2*pi; const Vec3 n{std::cos(a),0,std::sin(a)};
    m.vertices.push_back({{n.x*radius,0,n.z*radius},n,{1,1,1},{float(i)/sides,0}});
    m.vertices.push_back({{n.x*radius,height,n.z*radius},n,{1,1,1},{float(i)/sides,height}});
    if(i<sides) { const unsigned v=i*2; for(unsigned j:{v,v+1,v+2,v+2,v+1,v+3}) m.indices.push_back(j); }
  }
  for(unsigned cap=0;cap<2;++cap) {
    const unsigned base=unsigned(m.vertices.size()); const float y=cap ? height : 0;
    const Vec3 n{0,cap ? 1.f : -1.f,0};
    m.vertices.push_back({{0,y,0},n,{1,1,1},{.5,.5}});
    for(unsigned i=0;i<=sides;++i) {
      const float a=float(i)/sides*2*pi; const float x=std::cos(a),z=std::sin(a);
      m.vertices.push_back({{radius*x,y,radius*z},n,{1,1,1},{x*.5f+.5f,z*.5f+.5f}});
      if(i<sides) {
        m.indices.push_back(base); m.indices.push_back(base+1+i+(cap ? 1:0));
        m.indices.push_back(base+1+i+(cap ? 0:1));
      }
    }
  }
  return m;
}
Mesh sphere(float radius,unsigned sides=32,unsigned rings=16) {
  Mesh m;
  for(unsigned y=0;y<=rings;++y) for(unsigned x=0;x<=sides;++x) {
    const float u=float(x)/sides,v=float(y)/rings;
    const Vec3 n{std::sin(v*pi)*std::cos(u*2*pi),std::cos(v*pi),std::sin(v*pi)*std::sin(u*2*pi)};
    m.vertices.push_back({n*radius,n,{1,1,1},{u,v}});
    if(x<sides && y<rings) {
      const unsigned a=y*(sides+1)+x,b=a+sides+1;
      for(unsigned i:{a,b,a+1,a+1,b,b+1}) m.indices.push_back(i);
    }
  }
  return m;
}
Mesh torus(float radius,float tube,unsigned segments=48) {
  Mesh m;
  constexpr unsigned ring=12;
  for(unsigned a=0;a<=segments;++a) for(unsigned b=0;b<=ring;++b) {
    float u=float(a)/segments*2*pi,v=float(b)/ring*2*pi;
    Vec3 n{std::cos(u)*std::cos(v),std::sin(u)*std::cos(v),std::sin(v)};
    Vec3 p{std::cos(u)*(radius+tube*std::cos(v)),std::sin(u)*(radius+tube*std::cos(v)),tube*std::sin(v)};
    m.vertices.push_back({p,n,{1,1,1},{float(a)/segments,float(b)/ring}});
    if(a<segments && b<ring) { unsigned x=a*(ring+1)+b,y=x+ring+1;
      for(unsigned i:{x,x+1,y,y,x+1,y+1}) m.indices.push_back(i); }
  }
  return m;
}
Mesh palm_fronds() {
  Mesh m;
  for(unsigned branch=0;branch<12;++branch) {
    float angle=branch*2*pi/12; Vec3 axis{std::cos(angle),0,std::sin(angle)},side{-axis.z,0,axis.x};
    for(unsigned leaf=1;leaf<24;++leaf) {
      float t=float(leaf)/24; Vec3 center=axis*(t*3.8f)+Vec3{0,1.1f*std::sin(t*pi)-t*.8f,0};
      float span=(1-t)*.9f+.12f;
      for(int sign:{-1,1}) {
        Vec3 tip=center+side*(span*float(sign))+axis*.25f-Vec3{0,.14f+t*.25f,0};
        Vec3 start=center-axis*.10f,end=center+axis*.13f;
        Vec3 n=normalize(cross(tip-start,end-start)); if(n.y<0)n=-n;
        unsigned base=unsigned(m.vertices.size());
        Vec3 color{.22f+.08f*t,.38f+.09f*t,.08f};
        m.vertices.push_back({start,n,color,{0,0}}); m.vertices.push_back({tip,n,color,{.5f,1}});
        m.vertices.push_back({end,n,color,{1,0}});
        m.indices.insert(m.indices.end(),{base,base+1,base+2});
      }
    }
  }
  return m;
}
}

void CoastalScene::create(const std::string& pier_path,const std::string& tree_path) {
  meshes.clear(); instances.clear();
  auto mesh=[&](Mesh value) { meshes.push_back(std::make_unique<Mesh>(std::move(value))); return meshes.back().get(); };
  auto add=[&](Mesh* shape,const Vec3& p,const Vec3& size,Material material,float yaw=0) {
    instances.push_back({shape,translate(p)*rotate_y(yaw)*scale(size),material});
  };
  auto* box=mesh(make_box({1,1,1},{1,1,1}));
  auto* post=mesh(cylinder(1,1));
  auto* ball=mesh(sphere(1));
  deformation_mesh=ball; undeformed_vertices=ball->vertices;
  auto* ring=mesh(torus(.55f,.115f));
  auto* leaves=mesh(palm_fronds());
  Material wood; wood.albedo={.58f,.39f,.22f}; wood.roughness=.72f; wood.texture=TextureSlot::Wood;
  Material metal; metal.albedo={.34f,.38f,.4f}; metal.metallic=.95f; metal.roughness=.24f;
  Material yellow; yellow.albedo={.65f,.43f,.13f}; yellow.roughness=.8f;
  Material white; white.albedo={.76f,.77f,.71f}; white.roughness=.35f;
  Material green; green.albedo={.35f,.46f,.18f}; green.roughness=.8f;
  Material earth; earth.albedo={.2f,.16f,.095f}; earth.roughness=.95f;
  Material water; water.albedo={.05f,.16f,.13f}; water.texture=TextureSlot::Water; water.roughness=.065f;
  water.transmission=.86f; water.index_of_refraction=1.333f;
  auto* sea=mesh(make_plane(4000,4000,{1,1,1},1));
  add(sea,{0,-.35f,0},{1,1,1},water);
  add(box,{0,-3,0},{4000,1,4000},earth);
  add(box,{0,-.35f,-16},{65,1,19},earth);
  // Individually spaced planks, beams, caps and mooring posts cast real shadows.
  for(int i=-46;i<=46;++i) {
    Material plank=wood; float tint=.88f+.12f*float((i*i+7)%9)/8;
    plank.albedo=plank.albedo*tint;
    add(box,{float(i)*.3f,.55f,0},{.282f,.16f,3.6f},plank);
  }
  for(float z:{-1.4f,1.4f}) add(box,{0,.24f,z},{28,.32f,.24f},wood);
  for(int x=-13;x<=13;x+=3) for(float z:{-1.55f,1.55f}) {
    add(post,{float(x),-1.4f,z},{.18f,3.25f,.18f},wood);
    add(post,{float(x),1.85f,z},{.22f,.08f,.22f},metal);
    for(int wrap=0;wrap<4;++wrap) {
      Material rope; rope.albedo={.28f,.34f,.17f}; rope.roughness=.9f;
      instances.push_back({ring,translate({float(x),1.25f+wrap*.045f,z})*rotate_x(pi*.5f)*scale({.34f,.34f,.34f}),rope});
    }
  }
  for(float y:{1.05f,1.65f}) add(box,{0,y,-1.55f},{28,.085f,.085f},wood);
  const auto procedural_pier_end=instances.size();
  // Weatherboard cabin, roof slats, glazing, porch and warm fixtures.
  add(box,{3,2.35f,-7.5f},{6,4.6f,4.6f},yellow);
  for(int i=0;i<25;++i) {
    Material siding=yellow; siding.albedo=siding.albedo*(.88f+.12f*float(i%3)/2);
    add(box,{3,.2f+i*.18f,-5.17f},{6.08f,.145f,.06f},siding);
  }
  Material roof; roof.albedo={.15f,.17f,.16f}; roof.metallic=.65f; roof.roughness=.55f;
  for(int i=0;i<22;++i) {
    const float x=-.4f+i*.32f;
    instances.push_back({box,translate({x,4.85f,-6.2f})*rotate_x(.28f)*scale({.30f,.13f,3.2f}),roof});
    instances.push_back({box,translate({x,4.85f,-8.8f})*rotate_x(-.28f)*scale({.30f,.13f,3.2f}),roof});
  }
  Material glass; glass.albedo={.55f,.68f,.70f}; glass.roughness=.055f; glass.transmission=.93f;
  for(float x:{1.1f,4.9f}) {
    add(box,{x,2.7f,-5.1f},{1.3f,1.8f,.09f},glass);
    for(float dx:{-.72f,.72f}) add(box,{x+dx,2.7f,-5.02f},{.1f,2,.15f},white);
    for(float y:{1.7f,3.7f}) add(box,{x,y,-5.02f},{1.54f,.1f,.15f},white);
    add(box,{x,2.7f,-5.0f},{.07f,1.8f,.12f},white);
  }
  add(box,{3,1.6f,-5.05f},{1.05f,2.9f,.18f},wood);
  add(ball,{3.35f,1.55f,-4.9f},{.07f,.07f,.07f},metal);
  for(int step=0;step<5;++step) add(box,{3,.13f*step,-2.1f-.5f*step},{2,.15f,.55f},wood);
  Material light; light.albedo={1,.52f,.14f}; light.emissive=6;
  for(float x:{-.1f,6.1f}) {
    add(post,{x,.3f,-4.5f},{.06f,3.2f,.06f},metal);
    add(ball,{x,3.55f,-4.5f},{.14f,.21f,.14f},light);
  }
  // Boat hull assembled from curved rings with smooth normals and a separate interior.
  Mesh hull;
  constexpr unsigned rows=8,sides=64;
  for(unsigned row=0;row<=rows;++row) for(unsigned i=0;i<=sides;++i) {
    float t=float(row)/rows,a=float(i)/sides*2*pi;
    float width=.25f+1.12f*t,length=.8f+2.6f*t;
    Vec3 p{std::cos(a)*width,-.35f+t*1.1f,std::sin(a)*length};
    Vec3 n=normalize({std::cos(a)/width,-.5f,std::sin(a)/length});
    hull.vertices.push_back({p,n,{1,1,1},{float(i)/sides,t}});
    if(row<rows && i<sides) { unsigned b=row*(sides+1)+i,c=b+sides+1;
      hull.indices.insert(hull.indices.end(),{b,b+1,c,c,b+1,c+1}); }
  }
  auto* boat=mesh(std::move(hull));
  add(boat,{-6,-.15f,5.5f},{1,1,1},white,.27f);
  for(int seat=0;seat<3;++seat) add(box,{-6,.36f,4.1f+seat*1.25f},{2.1f,.13f,.38f},wood,.27f);
  for(float x:{-7.05f,-4.95f}) for(float z:{4.8f,7.f}) add(post,{x,.3f,z},{.032f,2,.032f},metal);
  Material canvas; canvas.albedo={.07f,.095f,.11f}; canvas.roughness=.85f;
  add(box,{-6,2.35f,5.9f},{2.65f,.1f,2.9f},canvas,.1f);
  add(box,{-6,.45f,8.7f},{.55f,1.3f,.7f},metal,.27f);
  Material lifebuoy; lifebuoy.albedo={.83f,.19f,.035f}; lifebuoy.roughness=.56f;
  for(float x:{-10.f,8.f}) instances.push_back({ring,translate({x,1.3f,-1.43f}),lifebuoy});
  // Palm crowns consist of individual leaf triangles, rather than opaque billboards.
  for(unsigned i=0;i<(tree_path.empty() ? 13u : 0u);++i) {
    float x=-23+float(i)*3.8f,z=-11-float((i*7)%5)*2.6f,height=7.8f+float(i%4)*.8f;
    for(unsigned segment=0;segment<12;++segment) {
      float t=float(segment)/12;
      add(post,{x+t*t*.85f,t*height,z},{.25f*(1-t*.45f),height/12+.02f,.25f*(1-t*.45f)},wood);
    }
    add(leaves,{x+.85f,height,z},{1,1,1},green,float(i)*.7f);
  }
  // Mooring bollards, stacked cargo, jars and metallic material probes.
  for(unsigned i=0;i<7;++i) {
    float x=8+float(i%3)*.9f,z=-.4f-float(i/3)*.75f;
    add(box,{x,.98f,z},{.7f,.65f,.65f},wood,float(i)*.12f);
  }
  for(unsigned i=0;i<5;++i) {
    Material finish; finish.albedo={.7f,.42f,.17f}; finish.metallic=float(i)/4;
    finish.roughness=.09f+float(i)*.17f;
    add(ball,{-2.4f+float(i)*.85f,1.03f,.25f},{.32f,.32f,.32f},finish);
  }
  if(!pier_path.empty()) {
    std::string error;
    if(!load_gltf(pier_path,pier_asset,error)) throw std::runtime_error("Coastal pier import: "+error);
    std::vector<const GltfPrimitive*> section;
    const float huge=(std::numeric_limits<float>::max)();
    Vec3 low{huge,huge,huge},high{-huge,-huge,-huge};
    std::map<int,double> horizontal_area;
    for(const auto& primitive:pier_asset.primitives) {
      if(primitive.name!="modular_wooden_pier_section_01") continue;
      section.push_back(&primitive);
      for(const auto& vertex:primitive.mesh->vertices) {
        const Vec3 p=transform_point(primitive.transform,vertex.position);
        low={(std::min)(low.x,p.x),(std::min)(low.y,p.y),(std::min)(low.z,p.z)};
        high={(std::max)(high.x,p.x),(std::max)(high.y,p.y),(std::max)(high.z,p.z)};
      }
      const auto& mesh=*primitive.mesh;
      for(std::size_t i=0;i<mesh.indices.size();i+=3) {
        const Vec3 a=transform_point(primitive.transform,mesh.vertices[mesh.indices[i]].position);
        const Vec3 b=transform_point(primitive.transform,mesh.vertices[mesh.indices[i+1]].position);
        const Vec3 c=transform_point(primitive.transform,mesh.vertices[mesh.indices[i+2]].position);
        const auto normal=cross(b-a,c-a); const float area=length(normal);
        if(area>1e-6f && std::fabs(normal.y)/area>.94f)
          horizontal_area[int(std::round((a.y+b.y+c.y)/3*20))]+=area;
      }
    }
    if(section.empty() || horizontal_area.empty()) throw std::runtime_error("Pier asset has no section_01 deck");
    int deck_height=0; double best_area=0;
    for(const auto& entry:horizontal_area) if(entry.second>=best_area*.98) { best_area=entry.second; deck_height=entry.first; }
    const Vec3 center=(low+high)*.5f;
    const float deck_y=float(deck_height)/20;
    Log::info("Imported coastal pier deck: "+std::to_string(deck_y)+"m; source section "+
              std::to_string(high.x-low.x)+" x "+std::to_string(high.z-low.z)+"m");
    instances.erase(instances.begin()+3,instances.begin()+std::ptrdiff_t(procedural_pier_end));
    const float spacing=high.z-low.z-.03f;
    const float depth_scale=3.6f/(high.x-low.x);
    for(int i=-4;i<=4;++i) {
      const Mat4 placement=translate({i*spacing,.55f,0})*scale({1,1,depth_scale})*
                           rotate_y(pi*.5f)*translate({-center.x,-deck_y,-center.z});
      for(const auto* primitive:section)
        instances.push_back({primitive->mesh.get(),placement*primitive->transform,primitive->material});
    }
  }
  if(!tree_path.empty()) {
    std::string error;
    if(!load_gltf(tree_path,tree_asset,error)) throw std::runtime_error("Coastal tree import: "+error);
    const auto center=(tree_asset.bounds_min+tree_asset.bounds_max)*.5f;
    const float height=tree_asset.bounds_max.y-tree_asset.bounds_min.y;
    if(height<=.01f) throw std::runtime_error("Invalid tree asset height");
    for(int i=-2;i<=2;++i) {
      const float scale_factor=(10.f+float((i*i)%3))/height;
      const Mat4 placement=translate({float(i)*10,.15f,-12.f-float(i*i)*1.7f})*
        rotate_y(float(i)*1.8f)*scale({scale_factor,scale_factor,scale_factor})*
        translate({-center.x,-tree_asset.bounds_min.y,-center.z});
      for(const auto& primitive:tree_asset.primitives)
        instances.push_back({primitive.mesh.get(),placement*primitive.transform,primitive.material});
    }
  }
}

void CoastalScene::draw(Renderer& renderer,float animation_time) {
  for(std::size_t i=0;i<instances.size();++i) {
    const auto& instance=instances[i]; Mat4 model=instance.model;
    if(animation_time!=0 && i+1==instances.size()) model=translate({std::sin(animation_time)*.6f,0,0})*model;
    renderer.draw_mesh(*instance.mesh,model,instance.material,i+1);
  }
}
void CoastalScene::deform(float time) {
  if(!deformation_mesh) return;
  const float stretch=1+.18f*std::sin(time*3);
  deformation_mesh->vertices=undeformed_vertices;
  for(auto& vertex:deformation_mesh->vertices) {
    vertex.position.x*=stretch;
    vertex.normal=normalize({vertex.normal.x/stretch,vertex.normal.y,vertex.normal.z});
  }
  deformation_mesh->mark_dirty();
}
