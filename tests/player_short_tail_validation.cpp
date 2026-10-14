#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/material_sampling.hpp"
#include "fury/generated/player_body_short_tail.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>
namespace {
  using namespace fury;
  void require(bool c, const std::string &m) {
    if (!c)
    throw std::runtime_error(m);
  }
  int surface(Vec2 uv) {
    for (unsigned i = 0; i < unsigned(CharacterSurface::Count); ++i) {
      const auto lo = character_surface_uv(CharacterSurface(i), 0, 0),
      hi = character_surface_uv(CharacterSurface(i), 1, 1);
      if (uv.x >= lo.x - 1e-7f && uv.x <= hi.x + 1e-7f && uv.y >= lo.y - 1e-7f &&
      uv.y <= hi.y + 1e-7f)
      return int(i);
    }
    return -1;
  }
  struct Box {
    Vec3 lo{
      100, 100, 100
    }, hi{
      -100, -100, -100
    };
    void add(Vec3 p) {
      lo = {
        std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)
      };
      hi = {
        std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)
      };
    }
    void add(const Box &b) {
      add(b.lo);
      add(b.hi);
    }
    bool overlap(const Box &b) const {
      return lo.x <= b.hi.x && b.lo.x <= hi.x && lo.y <= b.hi.y &&
      b.lo.y <= hi.y && lo.z <= b.hi.z && b.lo.z <= hi.z;
    }
  };
  struct Tri {
    std::array<Vec3, 3> p;
    Box box;
    unsigned offset;
    int region;
    float original_y{};
  };
  bool segment(Vec3 p, Vec3 q, const Tri &t) {
    Vec3 dir = q - p, e = t.p[1] - t.p[0], f = t.p[2] - t.p[0], n = cross(dir, f);
    const Vec3 plane = normalize(cross(e, f));
    const float dp = dot(p - t.p[0], plane), dq = dot(q - t.p[0], plane);
    // Rigidly transformed coplanar cut edges must not turn into crossings due
    // to roundoff. Require a real passage through both sides of the plane.
    if (!((dp > 1e-7f && dq < -1e-7f) || (dq > 1e-7f && dp < -1e-7f)))
    return false;
    float d = dot(e, n);
    if (std::fabs(d) < 1e-13f)
    return false;
    Vec3 x = p - t.p[0], z = cross(x, e);
    float u = dot(x, n) / d, v = dot(dir, z) / d, w = dot(f, z) / d;
    constexpr float eps = 2e-5f;
    return u > eps && v > eps && u + v < 1 - eps && w > eps && w < 1 - eps;
  }
  bool crossing(const Tri &a, const Tri &b) {
    if (!a.box.overlap(b.box))
    return false;
    for (int k = 0; k < 3; ++k)
    if (segment(a.p[k], a.p[(k + 1) % 3], b) ||
    segment(b.p[k], b.p[(k + 1) % 3], a))
    return true;
    return false;
  }
  struct Node {
    Box box;
    unsigned begin{}, end{};
    int left{
      -1
    }, right{
      -1
    };
  };
  struct Tree {
    const std::vector<Tri> &triangles;
    std::vector<unsigned> order;
    std::vector<Node> nodes;
    explicit Tree(const std::vector<Tri> &t) : triangles(t) {
      order.resize(t.size());
      std::iota(order.begin(), order.end(), 0);
      nodes.reserve(t.size() * 2);
      build(0, order.size());
    }
    int build(unsigned a, unsigned b) {
      int id = nodes.size();
      nodes.push_back({});
      nodes[id].begin = a;
      nodes[id].end = b;
      for (unsigned i = a; i < b; ++i)
      nodes[id].box.add(triangles[order[i]].box);
      if (b - a <= 8)
      return id;
      Vec3 extent = nodes[id].box.hi - nodes[id].box.lo;
      int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2)
      : (extent.y > extent.z ? 1 : 2);
      auto pos = [&](unsigned i) {
        auto c = triangles[i].box.lo + triangles[i].box.hi;
        return axis == 0 ? c.x : axis == 1 ? c.y : c.z;
      };
      unsigned m = (a + b) / 2;
      std::nth_element(order.begin() + a, order.begin() + m, order.begin() + b,
      [&](unsigned i, unsigned j) {
        return pos(i) < pos(j);
      });
      nodes[id].left = build(a, m);
      nodes[id].right = build(m, b);
      return id;
    }
    template <class F> void visit(const Tri &t, F fn, int id = 0) const {
      const auto &n = nodes[id];
      if (!n.box.overlap(t.box))
      return;
      if (n.left < 0) {
        for (unsigned i = n.begin; i < n.end; ++i)
        fn(triangles[order[i]]);
      } else {
        visit(t, fn, n.left);
        visit(t, fn, n.right);
      }
    }
  };

  float sqdist(Vec3 a,Vec3 b){const auto d=a-b;return dot(d,d);}
  // Closest point to a filled triangle, covering its face, all edges and corners.
  float point_triangle(Vec3 p,const Tri&t){
    const auto a=t.p[0],b=t.p[1],c=t.p[2],ab=b-a,ac=c-a,ap=p-a;
    float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return sqdist(p,a);
    const auto bp=p-b;float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return sqdist(p,b);
    float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return sqdist(p,a+ab*(d1/(d1-d3)));
    const auto cp=p-c;float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return sqdist(p,c);
    float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return sqdist(p,a+ac*(d2/(d2-d6)));
    float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return sqdist(p,b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6))));
    float inv=1/(va+vb+vc);return sqdist(p,a+ab*(vb*inv)+ac*(vc*inv));
  }
  float segment_distance(Vec3 p,Vec3 q,Vec3 r,Vec3 s){
    auto d1=q-p,d2=s-r,v=p-r;float a=dot(d1,d1),e=dot(d2,d2),f=dot(d2,v),x=0,y=0;
    if(a<=1e-20f&&e<=1e-20f)return sqdist(p,r);
    if(a<=1e-20f)y=std::clamp(f/e,0.f,1.f);
    else {float c=dot(d1,v);if(e<=1e-20f)x=std::clamp(-c/a,0.f,1.f);
      else {float b=dot(d1,d2),den=a*e-b*b;if(den>1e-20f)x=std::clamp((b*f-c*e)/den,0.f,1.f);y=(b*x+f)/e;
        if(y<0){y=0;x=std::clamp(-c/a,0.f,1.f);}else if(y>1){y=1;x=std::clamp((b-c)/a,0.f,1.f);}}}
    return sqdist(p+d1*x,r+d2*y);
  }
  float distance(const Tri&a,const Tri&b){
    if(crossing(a,b))return 0;
    float best=1e3f;
    for(int k=0;k<3;++k){best=std::min({best,point_triangle(a.p[k],b),point_triangle(b.p[k],a)});
      for(int j=0;j<3;++j)best=std::min(best,segment_distance(a.p[k],a.p[(k+1)%3],b.p[j],b.p[(j+1)%3]));}
    return best;
  }
  Tri triangle(const Mesh&m,unsigned offset,float h){Tri t{};t.offset=offset;for(int k=0;k<3;++k){t.p[k]=m.vertices[m.indices[offset+k]].position*(1/h);t.box.add(t.p[k]);}return t;}
  CharacterInfluence tail_weight(float y,float strength) {
    float t=std::clamp((.435f-y)/.070f,0.f,1.f);t=t*t*(3-2*t);
    return {std::uint8_t(CharacterBone::Head),std::uint8_t(CharacterBone::Neck),1-strength*t,0,0};
  }
  std::set<std::pair<unsigned,unsigned>> self_crossings(const std::vector<Tri>&ts) {
    Tree tree(ts);std::set<std::pair<unsigned,unsigned>> result;
    for(const auto&a:ts)tree.visit(a,[&](const Tri&b){
      if(b.offset<=a.offset||a.region!=b.region)return;
      for(auto p:a.p)for(auto q:b.p)if(length(p-q)<1e-6f)return;
      if(crossing(a,b))result.emplace(a.offset,b.offset);
    });return result;
  }
  void crossing_alpha(const Mesh&mesh,const Tri&a,const Tri&b,float h) {
    auto textures=character_surface_textures();auto detail=make_player_hair_color_detail();
    auto alpha=[&](unsigned t,std::array<float,3> weights){Vec2 uv{};for(int k=0;k<3;++k){const auto p=mesh.vertices[mesh.indices[t+k]].uv;uv.x+=p.x*weights[k];uv.y+=p.y*weights[k];}
      const auto sampled=sample_material_texture_lod(textures->base_color,textures->base_color_mips,uv,false,0);
      return apply_material_color_detail(detail.get(),sampled,uv,{},false).w;
    };
    for(int side=0;side<2;++side){const auto&x=side?b:a;const auto&y=side?a:b;
      for(int k=0;k<3;++k){const auto p=x.p[k],q=x.p[(k+1)%3];if(!segment(p,q,y))continue;
        const auto dir=q-p,e=y.p[1]-y.p[0],f=y.p[2]-y.p[0],n=cross(dir,f),off=p-y.p[0],r=cross(off,e);const float det=dot(e,n);
        const float u=dot(off,n)/det,v=dot(dir,r)/det,w=dot(f,r)/det;std::array<float,3> wx{};wx[k]=1-w;wx[(k+1)%3]=w;
        const auto point=(p+dir*w)*h;const float ax=alpha(x.offset,wx),ay=alpha(y.offset,{1-u-v,u,v});
        std::cout<<"OVERLAP_SAMPLE triangles="<<a.offset/3<<","<<b.offset/3<<" point_m="<<point.x<<","<<point.y<<","<<point.z<<" alpha_lod0="<<ax<<","<<ay<<" both_alpha_effective="<<(ax>=.4f&&ay>=.4f)<<"\n";return;
      }
    }
    throw std::runtime_error("Crossing must have an inspectable source-alpha sample");
  }
  float crossing_depth(const Tri&a,const Tri&b) {
    float result=0;
    for(int side=0;side<2;++side){const auto&x=side?a:b;const auto&y=side?b:a;
      auto n=normalize(cross(y.p[1]-y.p[0],y.p[2]-y.p[0]));float low=100,high=-100;
      for(auto p:x.p){float d=dot(p-y.p[0],n);low=std::min(low,d);high=std::max(high,d);}
      if(low<0&&high>0)result=std::max(result,std::min(-low,high));
    }return result;
  }
  template<class V,class I,class C> void verify(const CharacterModel&m,const V&vertices,const I&indices,const C&component_ids){
    const float h=m.rig.height;unsigned base=unsigned(m.bind_mesh.vertices.size());
    for(unsigned i=0;i+vertices.size()<=m.bind_mesh.vertices.size();++i){
      if(length(m.bind_mesh.vertices[i].position-vertices[0].p*h)>h*1e-7f)continue;
      bool found=true;for(unsigned j=0;j<vertices.size();++j)if(length(m.bind_mesh.vertices[i+j].position-vertices[j].p*h)>h*1e-7f){found=false;break;}
      if(found){base=i;break;}
    }
    require(base<m.bind_mesh.vertices.size(),"Complete generated tail has an exact contiguous runtime mapping");
    for(unsigned j=0;j<vertices.size();++j){const auto&w=m.influences[base+j];require(w.first==bone_index(CharacterBone::Head)&&w.first_weight>=.35f&&w.first_weight<=1&&w.third_weight==0,"Every source tail vertex is attached to Head with bounded Neck tip blending");}
    unsigned offset=unsigned(m.bind_mesh.indices.size());
    for(unsigned i=0;i+indices.size()<=m.bind_mesh.indices.size();i+=3){if(m.bind_mesh.indices[i]!=indices[0]+base)continue;bool found=true;for(unsigned j=0;j<indices.size();++j)if(m.bind_mesh.indices[i+j]!=indices[j]+base){found=false;break;}if(found){offset=i;break;}}
    require(offset<m.bind_mesh.indices.size(),"All generated tail faces are present in runtime topology");
    std::vector<unsigned> garment,head;
    for(unsigned t=0;t<m.bind_mesh.indices.size();t+=3){const auto&s=m.bind_mesh.vertices[m.bind_mesh.indices[t]];int mat=surface(s.uv);float top=-100;for(int k=0;k<3;++k)top=std::max(top,m.bind_mesh.vertices[m.bind_mesh.indices[t+k]].position.y/h);
      if(top>.05f&&(mat==int(CharacterSurface::Shirt)||mat==int(CharacterSurface::Leather)))garment.push_back(t);
      if((t<offset||t>=offset+indices.size())&&top>.30f&&(mat==int(CharacterSurface::Face)||mat==int(CharacterSurface::Skin)||mat==int(CharacterSurface::HairSwept)||mat==int(CharacterSurface::HairBundle)))head.push_back(t);}
    require(garment.size()>100,"Actual upper shirt, collar, sling and pouch are all included");
    std::vector<Tri> bind_tail;for(unsigned i=0;i<indices.size();i+=3){auto t=triangle(m.bind_mesh,offset+i,h);t.region=component_ids[i/3];bind_tail.push_back(t);}
    const auto bind_crossings=self_crossings(bind_tail);
    std::map<std::array<float,3>,std::set<unsigned>> touching;
    for(const auto&t:bind_tail)for(auto p:t.p)touching[{p.x,p.y,p.z}].insert(t.offset);
    std::map<unsigned,std::set<unsigned>> adjacent;
    for(const auto&t:bind_tail)for(auto p:t.p){const auto&linked=touching[{p.x,p.y,p.z}];adjacent[t.offset].insert(linked.begin(),linked.end());}
    float original_crossing_depth=0;
    for(auto pair:bind_crossings)original_crossing_depth=std::max(original_crossing_depth,crossing_depth(bind_tail[(pair.first-offset)/3],bind_tail[(pair.second-offset)/3]));
    CharacterModel probe;probe.rig=m.rig;probe.dual_quaternion_skinning=m.dual_quaternion_skinning;
    constexpr float epsilon=.0001f;
    for(const auto&v:vertices)for(int axis=0;axis<3;++axis)for(int side:{-1,1}){
      auto p=v.p; if(axis==0)p.x+=side*epsilon;else if(axis==1)p.y+=side*epsilon;else p.z+=side*epsilon;
      probe.bind_mesh.vertices.push_back({p*h,{0,1,0},{1,1,1},{0,0}});probe.influences.push_back(tail_weight(p.y,m.lod==CharacterLod::Near?.65f:.40f));
    }
    auto posed_probe=probe.bind_mesh;
    auto mesh=m.bind_mesh;float minimum=100,head_minimum=100,rigid_error=0,min_jacobian=100,min_area_ratio=100,max_edge_error=0;
    unsigned new_self_crossings=0,self_crossing_samples=0,migrated_crossing_observations=0;float posed_crossing_depth=0;std::set<std::pair<unsigned,unsigned>> changed_crossing_pairs;unsigned samples=0,crossings=0;int min_mode=-1,min_frame=-1;
    for(int path=0;path<3;++path)for(int mode=0;mode<7;++mode)for(int frame=0;frame<48;frame+=std::getenv("FURY_HAIR_QA_QUICK")?8:1){
      CharacterAnimationSample s;s.seed=m.seed;s.phase_cycles=frame/48.;s.idle_time=frame/12.;
      s.speed=mode==1?h*.85f:mode==2?h*2.6f:mode==5?h*.55f:0;s.move_weight=(mode==1||mode==2||mode==5)?1:0;
      s.crouch_weight=mode>=4?1:0;s.talk_weight=mode==3?1:0;
      s.attention_weight=1;s.attention_yaw=path==1?.9f:path==2?-.9f:.9f*std::sin(frame*6.2831853f/48);s.attention_pitch=.4f*std::cos(frame*6.2831853f/48);s.turn_rate=mode==6?4.f*std::sin(frame*6.2831853f/48):0;
      auto pose=sample_character_animation(m.rig,s);apply_character_pose(m,pose,mesh);
      const auto revision=mesh.geometry_revision;
      require(!apply_character_pose(m,pose,mesh)&&mesh.geometry_revision==revision,"Edge attachment and normal rebuild preserve exact repeated-pose holds");
      for(const auto& r:m.deformed_normal_regions)for(const auto&e:r.edge_attachments){
        const auto expected=mesh.vertices[e.first].position*(1-e.fraction)+mesh.vertices[e.second].position*e.fraction;
        max_edge_error=std::max(max_edge_error,length(mesh.vertices[e.vertex].position-expected)/h);
        require(mesh.vertices[e.vertex].uv.x==m.bind_mesh.vertices[e.vertex].uv.x&&mesh.vertices[e.vertex].uv.y==m.bind_mesh.vertices[e.vertex].uv.y,"Source UV split coordinates remain exact");
      }
      apply_character_pose(probe,pose,posed_probe);
      for(unsigned i=0;i<vertices.size();++i){Vec3 columns[3];for(int axis=0;axis<3;++axis){auto q=i*6+axis*2;columns[axis]=(posed_probe.vertices[q+1].position-posed_probe.vertices[q].position)*(1/(2*epsilon*h));}
        min_jacobian=std::min(min_jacobian,dot(columns[0],cross(columns[1],columns[2])));}
      if(h==1.75f&&(mode==3||mode==4)&&frame%8==0){
        std::vector<Tri> tail;for(unsigned i=0;i<indices.size();i+=3){auto t=triangle(mesh,offset+i,h);t.region=component_ids[i/3];tail.push_back(t);}
        for(auto pair:self_crossings(tail)){
          posed_crossing_depth=std::max(posed_crossing_depth,crossing_depth(tail[(pair.first-offset)/3],tail[(pair.second-offset)/3]));
          if(!bind_crossings.count(pair)){
            const bool first_pair=changed_crossing_pairs.insert(pair).second;if(first_pair)crossing_alpha(mesh,tail[(pair.first-offset)/3],tail[(pair.second-offset)/3],h);bool migrated=false;
            for(auto source:bind_crossings)if((adjacent[pair.first].count(source.first)&&adjacent[pair.second].count(source.second))||(adjacent[pair.first].count(source.second)&&adjacent[pair.second].count(source.first))){migrated=true;break;}
            if(migrated)++migrated_crossing_observations;
            else {++new_self_crossings;if(new_self_crossings<3)std::cerr<<"NEW_SELF_REGION path="<<path<<" mode="<<mode<<" frame="<<frame<<" triangles="<<pair.first/3<<","<<pair.second/3<<"\n";}
          }
        }++self_crossing_samples;
      }
      for(unsigned i=0;i<vertices.size();++i)if(vertices[i].p.y>=.445f)rigid_error=std::max(rigid_error,length(mesh.vertices[base+i].position-transform_point(pose.skin_matrices[bone_index(CharacterBone::Head)],m.bind_mesh.vertices[base+i].position))/h);
      std::vector<Tri> cloth,skull;for(auto t:garment)cloth.push_back(triangle(mesh,t,h));for(auto t:head)skull.push_back(triangle(mesh,t,h));Tree tree(cloth),head_tree(skull);float sample_min=.25f*.25f,sample_head=.25f*.25f;
      for(unsigned i=0;i<indices.size();i+=3){auto hair=triangle(mesh,offset+i,h);auto query=hair;float expand=std::sqrt(sample_min);query.box.lo-=Vec3{expand,expand,expand};query.box.hi+=Vec3{expand,expand,expand};
        const auto ia=mesh.indices[offset+i],ib=mesh.indices[offset+i+1],ic=mesh.indices[offset+i+2];
        const auto &a=mesh.vertices[ia],&b=mesh.vertices[ib],&c=mesh.vertices[ic];
        const auto ba=m.bind_mesh.vertices[ia].position,bb=m.bind_mesh.vertices[ib].position,bc=m.bind_mesh.vertices[ic].position;
        min_area_ratio=std::min(min_area_ratio,length(cross(b.position-a.position,c.position-a.position))/length(cross(bb-ba,bc-ba)));
        require(std::fabs(length(a.normal)-1)<.0003f&&std::fabs(length(b.normal)-1)<.0003f&&std::fabs(length(c.normal)-1)<.0003f,"Deformed tail normals remain unit length");
        if(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)<=0){std::cerr<<"WINDING path="<<path<<" mode="<<mode<<" frame="<<frame<<" min_jacobian="<<min_jacobian<<" min_area_ratio="<<min_area_ratio<<" local_tail_triangle="<<i/3<<" vertices="<<indices[i]<<","<<indices[i+1]<<","<<indices[i+2]<<" y="<<vertices[indices[i]].p.y<<","<<vertices[indices[i+1]].p.y<<","<<vertices[indices[i+2]].p.y<<"\n";throw std::runtime_error("Pose-dependent tail bending preserves face winding");}
        if(std::max({vertices[indices[i]].p.y,vertices[indices[i+1]].p.y,vertices[indices[i+2]].p.y})<.425f){
          auto q=hair;float radius=std::sqrt(sample_head);q.box.lo-=Vec3{radius,radius,radius};q.box.hi+=Vec3{radius,radius,radius};
          head_tree.visit(q,[&](const Tri&other){float d=distance(hair,other);sample_head=std::min(sample_head,d);});
        }
        tree.visit(query,[&](const Tri&other){float d=distance(hair,other);if(d==0){if(!crossings){std::cerr<<"FIRST_CROSS mode="<<mode<<" frame="<<frame<<" yaw="<<s.attention_yaw<<" pitch="<<s.attention_pitch<<" hair="<<hair.offset/3<<" garment="<<other.offset/3<<"\n";for(auto p:hair.p)std::cerr<<"hair "<<p.x<<","<<p.y<<","<<p.z<<"\n";for(auto p:other.p)std::cerr<<"cloth "<<p.x<<","<<p.y<<","<<p.z<<"\n";}++crossings;}sample_min=std::min(sample_min,d);});}
      head_minimum=std::min(head_minimum,std::sqrt(sample_head));float separation=std::sqrt(sample_min);if(separation<minimum){minimum=separation;min_mode=mode;min_frame=frame;}++samples;
    }
    std::cout<<(m.lod==CharacterLod::Near?"Near":"Far")<<" height="<<h<<" samples="<<samples<<" tail_faces="<<indices.size()/3<<" garment_faces="<<garment.size()<<" minimum_clearance_mm="<<minimum*h*1000<<" free_tail_head_clearance_mm="<<head_minimum*h*1000<<" min_mode="<<min_mode<<" min_frame="<<min_frame<<" rigid_error_per_height="<<rigid_error<<" crossing_pairs="<<crossings<<" max_parent_edge_error_per_height="<<max_edge_error<<" minimum_deformer_jacobian="<<min_jacobian<<" minimum_area_ratio="<<min_area_ratio<<" source_self_crossings="<<bind_crossings.size()<<" self_crossing_samples="<<self_crossing_samples<<" changed_crossing_pairs="<<changed_crossing_pairs.size()<<" migrated_existing_fold_observations="<<migrated_crossing_observations<<" new_self_crossing_regions="<<new_self_crossings<<" source_crossing_depth_mm="<<original_crossing_depth*h*1000<<" posed_crossing_depth_mm="<<posed_crossing_depth*h*1000<<std::endl;
    require(max_edge_error<1e-6f,"Atlas cut corners stay on their actual posed parent edge");require(min_jacobian>.10f&&min_area_ratio>.10f,"Normal rebuilding does not mask local geometric inversion or collapse");require(new_self_crossings==0,"No new self crossing regions beyond one-ring neighborhoods of existing connected-source folds");require(posed_crossing_depth<=original_crossing_depth*1.5f+.0005f,"Existing source fold overlaps do not grow substantially deeper");require(head_minimum>.0005f,"Free tail does not penetrate actual head, neck or scalp layers");require(crossings==0&&minimum>.003f,"Tail stays clear of every actual upper garment triangle");require(rigid_error<1e-6f,"Tie/root follows the actual Head transform without detachment");
  }
  void controls(){
    Tri a{{Vec3{0,0,0},Vec3{1,0,0},Vec3{0,1,0}},{},0,0};for(auto p:a.p)a.box.add(p);
    Tri b{{Vec3{.2f,.2f,-1},Vec3{.2f,.2f,1},Vec3{.5f,.5f,1}},{},3,1};for(auto p:b.p)b.box.add(p);
    require(crossing(a,b)&&distance(a,b)==0,"Injected crossing is rejected");
    auto c=a;for(auto&p:c.p)p.z+=.03f;c.box={};for(auto p:c.p)c.box.add(p);
    require(std::fabs(std::sqrt(distance(a,c))-.03f)<1e-6f,"Known parallel-plane gap is measured correctly");
    require(std::fabs(segment_distance({0,0,0},{1,0,0},{.5f,-1,.02f},{.5f,1,.02f})-.0004f)<1e-7f,"Edge-interior closest distance control");
  }
}
int main(){try{
  require(fury::player_short_tail_enabled(),"This scoped test requires short-tail opt-in");controls();
  const char* lod_filter=std::getenv("FURY_HAIR_QA_LOD");
  require(!lod_filter||std::string(lod_filter)=="near"||std::string(lod_filter)=="far","QA LOD filter must be near or far");
  for(float h:{1.35f,1.75f,2.10f})for(auto lod:{fury::CharacterLod::Near,fury::CharacterLod::Far}){
    if(lod_filter&&((std::string(lod_filter)=="near")!=(lod==fury::CharacterLod::Near)))continue;
    auto profile=fury::make_character_profile(h,fury::CharacterRole::Player,"PlayerBody",lod);
    if(lod==fury::CharacterLod::Near)verify(profile.model,fury::player_body_short_tail::vertices,fury::player_body_short_tail::indices,fury::player_body_short_tail::triangle_components);
    else verify(profile.model,fury::player_body_short_tail::far_vertices,fury::player_body_short_tail::far_indices,fury::player_body_short_tail::far_triangle_components);
  }return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
