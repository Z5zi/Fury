#pragma once
#include "character_pax_shirt.hpp"
#include <tuple>

namespace fury::pax_vest_detail {
using namespace character_detail;
struct Shell {
  std::vector<P> nodes;
  std::vector<std::array<unsigned, 3>> triangles;
  unsigned point(Vec3 p, Vec3 c, CharacterInfluence w, Vec2 uv) {
    nodes.push_back({p,c,w,uv}); return unsigned(nodes.size()-1);
  }
  void quad(unsigned a,unsigned b,unsigned c,unsigned d) {
    triangles.push_back({a,b,c}); triangles.push_back({a,c,d});
  }
  void emit(Sculpt &b,float thickness) const {
    std::vector<Vec3> normals(nodes.size());
    std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,unsigned>> boundary;
    std::map<std::pair<unsigned,unsigned>,int> counts;
    for (auto t:triangles) {
      Vec3 n=cross(nodes[t[1]].p-nodes[t[0]].p,nodes[t[2]].p-nodes[t[0]].p);
      if (dot(n,n)<1e-20f) throw std::runtime_error("Pax vest has a degenerate face");
      for (auto i:t) normals[i]+=n;
      for(int k=0;k<3;++k) {
        unsigned a=t[k],c=t[(k+1)%3]; auto key=std::minmax(a,c);
        if(++counts[key]>2) throw std::runtime_error("Pax vest branches");
        boundary[key]={a,c};
      }
    }
    const unsigned first=b.m.bind_mesh.vertices.size(),count=nodes.size();
    for (unsigned i=0;i<count;++i) {
      const float n2=dot(normals[i],normals[i]);
      if(n2<1e-20f) throw std::runtime_error("Pax vest has an unused or singular node");
      normals[i]=normals[i]*(1/std::sqrt(n2)); b.vertex(nodes[i]);
    }
    for (unsigned i=0;i<count;++i) {P p=nodes[i];p.p-=normals[i]*thickness;p.c=p.c*.95f;b.vertex(p);}
    for(auto t:triangles) {b.tri(first+t[0],first+t[1],first+t[2]);b.tri(first+count+t[0],first+count+t[2],first+count+t[1]);}
    // The lining and outer cloth meet at actual cut edges, including the
    // complete armhole loops. Independent corners preserve edge normals.
    for(auto entry:counts) if(entry.second==1) {
      const auto e=boundary.at(entry.first); P a=nodes[e.first],c=nodes[e.second],ia=a,ic=c;
      ia.p-=normals[e.first]*thickness;ic.p-=normals[e.second]*thickness;
      a.c=a.c*.76f;c.c=c.c*.76f;ia.c=ia.c*.76f;ic.c=ic.c*.76f;
      auto q=b.vertex(a),r=b.vertex(ia),s=b.vertex(ic),t=b.vertex(c);
      b.tri(q,r,s);b.tri(q,s,t);
    }
  }
};
inline P fit(const Shell &s,float x,float y,bool front,float lift) {
  float best=front?-1.f:1.f; P result{};bool found=false;
  for(auto tri:s.triangles) {
    const P &a=s.nodes[tri[0]],&c=s.nodes[tri[1]],&d=s.nodes[tri[2]];
    const Vec3 e=c.p-a.p,f=d.p-a.p;
    const float det=e.x*f.y-e.y*f.x;if(std::fabs(det)<1e-12f)continue;
    const float u=((x-a.p.x)*f.y-(y-a.p.y)*f.x)/det;
    const float v=(e.x*(y-a.p.y)-e.y*(x-a.p.x))/det;
    if(u< -1e-5f||v< -1e-5f||u+v>1.00001f)continue;
    const float z=a.p.z+u*e.z+v*f.z;
    if((front&&z<=0)||(!front&&z>=0))continue;
    if((front&&z>best)||(!front&&z<best)) {
      best=z;found=true;result=a;result.p={x,y,z+(front?lift:-lift)};
      // All main vest vertices share the same chest/spine/pelvis scalar
      // field, sampled at this physical height. The tapes follow that field.
      std::array<float,character_bone_count> weights{};
      auto add=[&](CharacterInfluence w,float k){weights[w.first]+=w.first_weight*k;weights[w.second]+=(1-w.first_weight-w.third_weight)*k;weights[w.third]+=w.third_weight*k;};
      add(a.w,1-u-v);add(c.w,u);add(d.w,v);
      std::array<unsigned,character_bone_count> order{};for(unsigned k=0;k<order.size();++k)order[k]=k;
      std::stable_sort(order.begin(),order.end(),[&](unsigned aa,unsigned cc){return weights[aa]>weights[cc];});
      const float sum=weights[order[0]]+weights[order[1]]+weights[order[2]],first=weights[order[0]]/sum,third=weights[order[2]]/sum;
      result.w={std::uint8_t(order[0]),std::uint8_t(order[1]),first,std::uint8_t(order[2]),std::min(third,1-first)};
    }
  }
  if(!found)throw std::runtime_error("Pax tape lacks vest support");
  return result;
}
inline CharacterInfluence mix_weights(CharacterInfluence a,CharacterInfluence b,float t) {
  std::array<float,character_bone_count> w{};
  auto add=[&](CharacterInfluence p,float k){w[p.first]+=p.first_weight*k;w[p.second]+=(1-p.first_weight-p.third_weight)*k;w[p.third]+=p.third_weight*k;};add(a,1-t);add(b,t);
  std::array<unsigned,character_bone_count> order{};for(unsigned k=0;k<order.size();++k)order[k]=k;
  std::stable_sort(order.begin(),order.end(),[&](unsigned x,unsigned z){return w[x]>w[z];});float sum=w[order[0]]+w[order[1]]+w[order[2]],first=w[order[0]]/sum,third=w[order[2]]/sum;
  return {std::uint8_t(order[0]),std::uint8_t(order[1]),first,std::uint8_t(order[2]),std::min(third,1-first)};
}
inline Shell fitted_band(const Shell &source,bool front,float side) {
  struct Sample{P p;Vec3 normal;};
  std::vector<Vec3> n(source.nodes.size());
  for(auto t:source.triangles){Vec3 q=cross(source.nodes[t[1]].p-source.nodes[t[0]].p,source.nodes[t[2]].p-source.nodes[t[0]].p);for(auto i:t)n[i]+=q;}
  for(auto &q:n)q=normalize(q);
  const float low=side*.056f-.006f,high=side*.056f+.006f;
  std::array<std::function<float(Vec3)>,5>cuts{{[&](Vec3 p){return p.x-low;},[&](Vec3 p){return high-p.x;},[](Vec3 p){return p.y-.090f;},[](Vec3 p){return .293f-p.y;},[&](Vec3 p){return front?p.z:-p.z;}}};
  Shell band;std::vector<Vec3>original;
  auto node=[&](Sample q){for(unsigned i=0;i<original.size();++i)if(length(original[i]-q.p.p)<1e-6f)return i;original.push_back(q.p.p);q.p.uv=character_surface_uv(S::Shirt,(q.p.p.x-low)/(high-low),(q.p.p.y-.090f)/.203f);return band.point(q.p.p+q.normal*.0014f,{.70f,.75f,.65f},q.p.w,q.p.uv);};
  for(auto t:source.triangles) {
    std::vector<Sample> poly;for(auto i:t)poly.push_back({source.nodes[i],n[i]});
    for(const auto&cut:cuts) {
      std::vector<Sample>out;
      for(std::size_t k=0;k<poly.size();++k){const auto &a=poly[k],&c=poly[(k+1)%poly.size()];float da=cut(a.p.p),dc=cut(c.p.p);if(da>=0)out.push_back(a);if((da>=0)!=(dc>=0)){float t=da/(da-dc);Sample q=a;q.p.p=a.p.p*(1-t)+c.p.p*t;q.p.w=mix_weights(a.p.w,c.p.w,t);q.normal=normalize(a.normal*(1-t)+c.normal*t);out.push_back(q);}}
      poly=std::move(out);if(poly.size()<3)break;
    }
    bool changed=true;while(changed&&poly.size()>=3){changed=false;for(std::size_t k=0;k<poly.size();++k){Vec3 a=poly[(k+poly.size()-1)%poly.size()].p.p,c=poly[k].p.p,d=poly[(k+1)%poly.size()].p.p,e=d-a,q=cross(c-a,e);if(dot(e,e)>1e-16f&&dot(c-a,d-c)>=0&&dot(q,q)<sq(.00006f)*dot(e,e)){poly.erase(poly.begin()+std::ptrdiff_t(k));changed=true;break;}}}
    if(poly.size()<3)continue;
    unsigned a=node(poly[0]);for(std::size_t k=1;k+1<poly.size();++k){unsigned c=node(poly[k]),d=node(poly[k+1]);if(a!=c&&a!=d&&c!=d)band.triangles.push_back({a,c,d});}
  }
  return band;
}

}

namespace fury {
inline void make_pax_vest(character_detail::Sculpt &b,const pax_shirt_detail::ShirtSupport &shirt) {
  using namespace character_detail;using namespace pax_vest_detail;
  const Vec3 vest{.71f,.39f,.09f};
  Shell s;
  std::vector<Vec3> normals(shirt.nodes.size());
  for(auto t:shirt.triangles) {
    Vec3 n=cross(shirt.nodes[t[1]].p-shirt.nodes[t[0]].p,shirt.nodes[t[2]].p-shirt.nodes[t[0]].p);
    for(auto i:t)normals[i]+=n;
  }
  for(auto &n:normals) {float d=dot(n,n);if(d>1e-20f)n=n*(1/std::sqrt(d));}
  struct Sample {P p;Vec3 n;};
  auto blend=[](const Sample&a,const Sample&c,float t) {
    // Avoid micron-wide slivers when a clipping line nearly meets a source
    // corner; the shared edge parameter snaps identically on both faces.
    if(t<.025f)return a;
    if(t>.975f)return c;
    Sample q=a;q.p.p=a.p.p*(1-t)+c.p.p*t;q.n=normalize(a.n*(1-t)+c.n*t);
    std::array<float,character_bone_count>w{};
    auto add=[&](CharacterInfluence p,float k){w[p.first]+=p.first_weight*k;w[p.second]+=(1-p.first_weight-p.third_weight)*k;w[p.third]+=p.third_weight*k;};
    add(a.p.w,1-t);add(c.p.w,t);
    std::array<unsigned,character_bone_count> order{};for(unsigned k=0;k<order.size();++k)order[k]=k;
    std::stable_sort(order.begin(),order.end(),[&](unsigned x,unsigned z){return w[x]>w[z];});
    float sum=w[order[0]]+w[order[1]]+w[order[2]],first=w[order[0]]/sum,third=w[order[2]]/sum;
    q.p.w={std::uint8_t(order[0]),std::uint8_t(order[1]),first,std::uint8_t(order[2]),std::min(third,1-first)};
    return q;
  };
  std::map<std::tuple<long long,long long,long long>,std::vector<unsigned>> welded;
  std::vector<Vec3> source_positions;
  auto node=[&](Sample q) {
    const Vec3 original=q.p.p;
    auto key=std::make_tuple(std::llround(original.x*1e6f),std::llround(original.y*1e6f),std::llround(original.z*1e6f));
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z) {
      auto found=welded.find({std::get<0>(key)+x,std::get<1>(key)+y,std::get<2>(key)+z});
      if(found!=welded.end())for(auto i:found->second)if(length(source_positions[i]-original)<1e-6f)return i;
    }
    const float hang=.0018f*g(original.y,.085f,.024f)*(1+.35f*std::sin(original.x*75));
    q.p.p+=q.n*((b.near?.0058f:.0063f)+hang);q.p.c=vest;
    const float a=std::atan2(original.x,std::fabs(original.z));
    q.p.uv=character_surface_uv(S::Jacket,.5f+a/pi,smooth((original.y-.06f)/.30f));
    auto id=s.point(q.p.p,q.p.c,q.p.w,q.p.uv);welded[key].push_back(id);source_positions.push_back(original);return id;
  };
  const std::array<std::function<float(Vec3)>,4> cuts{{
    [](Vec3 p){float hem=.064f+.009f*g(std::fabs(p.x),.106f,.018f)+.011f*g(p.x,0,.020f)*smooth(p.z/.04f);return p.y-hem;},
    [](Vec3 p){float front=smooth((p.z-.008f)/.050f);return .351f-.078f*front+1.4f*std::fabs(p.x)*front-p.y;},
    [](Vec3 p){return p.z>0?std::fabs(p.x)-.0040f:1.f;},
    // A vest's armhole must clear the moving sleeve, not share its cut edge.
    // The enlarged opening leaves a real band of shirt visible around it.
    [](Vec3 p){return std::max(sq((p.y-.269f)/.098f)+sq((p.z-.003f)/.068f)-1.f,(.078f-std::fabs(p.x))/.010f);}
  }};
  // The shirt's actual sewn torso/yoke is the fitting surface. Cutting our
  // own V opening and curved hem from its continuous topology gives the vest
  // correct shoulder/armhole contact in both LODs, with the same underlying
  // influence correspondence. It has its own offset, lining and hanging ease.
  for(auto t:shirt.triangles) {
    std::vector<Sample> poly;for(auto i:t)poly.push_back({shirt.nodes[i],normals[i]});
    for(const auto &cut:cuts) {
      std::vector<Sample> next;
      for(std::size_t i=0;i<poly.size();++i) {
        const auto &a=poly[i],&c=poly[(i+1)%poly.size()];float da=cut(a.p.p),dc=cut(c.p.p);
        if(da>=0)next.push_back(a);
        if((da>=0)!=(dc>=0))next.push_back(blend(a,c,da/(da-dc)));
      }
      poly=std::move(next);if(poly.size()<3)break;
    }
    // Repeated clipping planes can leave virtually collinear boundary
    // corners. Remove points within 0.12 mm per metre of height of their
    // adjacent edge; this subpixel cloth cleanup prevents a tiny offset
    // lining triangle from folding while leaving anatomical data untouched.
    bool changed=true;
    while(changed&&poly.size()>=3) {
      changed=false;
      for(std::size_t k=0;k<poly.size();++k) {
        Vec3 a=poly[(k+poly.size()-1)%poly.size()].p.p,
             c=poly[k].p.p,d=poly[(k+1)%poly.size()].p.p;
        Vec3 edge=d-a;float length2=dot(edge,edge);
        const Vec3 cross_edge=cross(c-a,edge);
        if(length2>1e-16f&&dot(c-a,d-c)>=0&&dot(cross_edge,cross_edge)<sq(.00012f)*length2) {
          poly.erase(poly.begin()+std::ptrdiff_t(k));changed=true;break;
        }
      }
    }
    if(poly.size()<3)continue;
    unsigned a=node(poly[0]);for(std::size_t k=1;k+1<poly.size();++k){unsigned c=node(poly[k]),d=node(poly[k+1]);if(a!=c&&a!=d&&c!=d)s.triangles.push_back({a,c,d});}
  }
  s.emit(b,.0009f);
  // Each finite band follows the exact supporting triangle partition and
  // weights. An unrelated regular strip grid cut through curved cloth in gait.
  for(bool front:{true,false})for(float side:{-1.f,1.f})
    fitted_band(s,front,side).emit(b,.0004f);
  // A dark fabric underlap bridges the central opening. Its raised side rails
  // are distinct layers over the supporting shirt, not an unbacked slit.
  b.grid(2,b.near?14:6,[&](float u,float v){float y=lerp(.081f,.270f,v);P p=fit(s,.009f,y,true,.0002f);p.p.x=lerp(.0075f,-.0075f,u);p.c={.054f,.061f,.058f};p.uv=character_surface_uv(S::Rubber,u,v);return p;},false,false);
  for(float side:{-1.f,1.f}) {
    b.grid(1,b.near?14:6,[&](float u,float v){float y=lerp(.081f,.273f,v),x=side*.0030f+(.5f-u)*.0032f;P p=fit(s,x+side*.0045f,y,true,.0011f);p.c={.07f,.078f,.075f};p.uv=character_surface_uv(S::Rubber,u,v);return p;},false,false);
  }
  // Existing left cargo-thigh pocket, now a shallow gusseted pouch with a
  // turned flap. The thigh weights preserve its original attachment role.
  const Vec3 cloth=b.m.appearance.trousers;
  auto trouser_radius=[](float y){return .027f+.011f*g(y,-.34f,.073f)+.022f*smooth((y+.24f)/.19f);};
  auto cargo_point=[&](float u,float v,float extra){
    float a=lerp(1.78f,.12f,u),y=lerp(-.159f,-.091f,v),r=trouser_radius(y),
          ease=.0018f+.0040f*std::sin(pi*u)*std::sin(pi*v)+extra;
    return Vec3{-.056f-(r+ease)*std::cos(a),y,(r*1.10f+ease)*std::sin(a)-.002f};
  };
  for(bool flap:{false,true}) {
    Shell pocket;const int nx=b.near?12:6,ny=flap?(b.near?4:2):(b.near?8:4);
    for(int j=0;j<=ny;++j)for(int i=0;i<=nx;++i){float u=float(i)/nx,v=float(j)/ny;Vec3 p=cargo_point(u,flap?lerp(1.f,.76f,v):v,flap?.002f+.0018f*std::sin(pi*v):0.f);pocket.point(p,cloth*(flap?.78f:.84f),wt(B::LeftThigh),character_surface_uv(S::Pants,u,v));}
    for(int j=0;j<ny;++j)for(int i=0;i<nx;++i){unsigned a=j*(nx+1)+i,c=a+nx+1;if(flap)pocket.quad(a,a+1,c+1,c);else pocket.quad(a,c,c+1,a+1);}
    pocket.emit(b,.00085f);
  }
}
}
