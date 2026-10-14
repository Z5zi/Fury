#pragma once
#include "character_hale_shirt.hpp"
#include <tuple>

namespace fury::hale_vest_detail {
using namespace character_detail;
struct Shell {
  std::vector<P> nodes;
  std::vector<std::array<unsigned, 3>> triangles;
  std::vector<Vec3> thickness_directions;
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
      if (dot(n,n)<1e-20f) throw std::runtime_error("Hale vest has a degenerate face");
      for (auto i:t) normals[i]+=n;
      for(int k=0;k<3;++k) {
        unsigned a=t[k],c=t[(k+1)%3]; auto key=std::minmax(a,c);
        if(++counts[key]>2) throw std::runtime_error("Hale vest branches");
        boundary[key]={a,c};
      }
    }
    const unsigned first=b.m.bind_mesh.vertices.size(),count=nodes.size();
    for (unsigned i=0;i<count;++i) {
      const float n2=dot(normals[i],normals[i]);
      if(n2<1e-20f) throw std::runtime_error("Hale vest has an unused or singular node");
      normals[i]=normals[i]*(1/std::sqrt(n2)); b.vertex(nodes[i]);
      if(thickness_directions.size()==count) normals[i]=thickness_directions[i];
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
  if(!found)throw std::runtime_error("Hale equipment lacks vest support");
  return result;
}
inline CharacterInfluence mix_weights(CharacterInfluence a,CharacterInfluence b,float t) {
  std::array<float,character_bone_count> w{};
  auto add=[&](CharacterInfluence p,float k){w[p.first]+=p.first_weight*k;w[p.second]+=(1-p.first_weight-p.third_weight)*k;w[p.third]+=p.third_weight*k;};add(a,1-t);add(b,t);
  std::array<unsigned,character_bone_count> order{};for(unsigned k=0;k<order.size();++k)order[k]=k;
  std::stable_sort(order.begin(),order.end(),[&](unsigned x,unsigned z){return w[x]>w[z];});float sum=w[order[0]]+w[order[1]]+w[order[2]],first=w[order[0]]/sum,third=w[order[2]]/sum;
  return {std::uint8_t(order[0]),std::uint8_t(order[1]),first,std::uint8_t(order[2]),std::min(third,1-first)};
}

}

namespace fury {
inline void make_hale_collar(character_detail::Sculpt &b,
                              const hale_shirt_detail::ShirtSupport &shirt) {
  using namespace character_detail;
  using namespace hale_vest_detail;
  std::vector<Vec3> normals(shirt.nodes.size());
  for(auto t:shirt.triangles) {
    const Vec3 n=cross(shirt.nodes[t[1]].p-shirt.nodes[t[0]].p,
                       shirt.nodes[t[2]].p-shirt.nodes[t[0]].p);
    for(auto i:t)normals[i]+=n;
  }
  for(auto &n:normals) {
    const float q=dot(n,n);if(q>1e-20f)n=n*(1/std::sqrt(q));
  }
  // Two original folded collar leaves follow the actual yoke triangles. This
  // preserves support under attention/head turns and supplies real cut edges.
  const std::array<Vec2,5> outline{{{.013f,.352f},{.043f,.349f},
                                  {.048f,.318f},{.026f,.299f},{.007f,.329f}}};
  for(float side:{-1.f,1.f}) {
    struct Sample {P p;Vec3 n;};
    Shell leaf;
    const auto add=[&](Sample q) {
      const Vec3 original=q.p.p;
      const float fold=.0016f+.0055f*g(original.y,.328f,.022f);
      q.p.p+=q.n*fold;
      q.p.c={.16f*.94f,.22f*.94f,.28f*.94f};
      q.p.uv=character_surface_uv(S::Shirt,
             (side*original.x-.007f)/.041f,(original.y-.299f)/.053f);
      for(unsigned i=0;i<leaf.nodes.size();++i)
        if(length(leaf.nodes[i].p-q.p.p)<1e-7f)return i;
      const auto result=leaf.point(q.p.p,q.p.c,q.p.w,q.p.uv);
      leaf.thickness_directions.push_back(q.n);
      return result;
    };
    for(auto t:shirt.triangles) {
      if(std::all_of(t.begin(),t.end(),[&](unsigned i){return shirt.nodes[i].p.z<=0;}))continue;
      std::vector<Sample> poly;
      for(auto i:t)poly.push_back({shirt.nodes[i],normals[i]});
      for(unsigned edge=0;edge<outline.size();++edge) {
        const Vec2 a=outline[edge],c=outline[(edge+1)%outline.size()];
        const auto distance=[&](Vec3 p){return -((c.x-a.x)*(p.y-a.y)-(c.y-a.y)*(side*p.x-a.x));};
        std::vector<Sample> next;
        for(std::size_t i=0;i<poly.size();++i) {
          const auto &a=poly[i],&c=poly[(i+1)%poly.size()];
          const float da=distance(a.p.p),dc=distance(c.p.p);
          if(da>=0)next.push_back(a);
          if((da>=0)!=(dc>=0)) {
            const float t=da/(da-dc);Sample q=a;
            q.p.p=a.p.p*(1-t)+c.p.p*t;
            q.p.w=mix_weights(a.p.w,c.p.w,t);
            q.n=normalize(a.n*(1-t)+c.n*t);next.push_back(q);
          }
        }
        poly=std::move(next);if(poly.size()<3)break;
      }
      if(poly.size()<3)continue;
      for(std::size_t i=1;i+1<poly.size();++i) {
        const Vec3 n=cross(poly[i].p.p-poly[0].p.p,poly[i+1].p.p-poly[0].p.p);
        if(dot(n,n)<1e-18f)continue;
        const auto a=add(poly[0]),c=add(poly[i]),d=add(poly[i+1]);
        if(a!=c&&a!=d&&c!=d)leaf.triangles.push_back({a,c,d});
      }
    }
    leaf.emit(b,.0009f);
  }
}
inline void make_hale_equipment(character_detail::Sculpt &b,
                                const hale_vest_detail::Shell &support) {
  using namespace character_detail;
  using namespace hale_vest_detail;
  const Vec3 dark{.055f,.065f,.075f};
  // Clip thin insignia/pocket panels from their real support triangles. The
  // barycentric positions and weights preserve the garment's deformation.
  const auto patch = [&](float cx,float cy,float hx,float hy,Vec3 color,
                         S material,float lift,bool shield) {
    std::vector<Vec3> normals(support.nodes.size());
    for(auto t:support.triangles) {
      Vec3 n=cross(support.nodes[t[1]].p-support.nodes[t[0]].p,
                   support.nodes[t[2]].p-support.nodes[t[0]].p);
      for(auto i:t)normals[i]+=n;
    }
    for(auto &n:normals) {
      const float d=dot(n,n);
      if(d>1e-20f)n=n*(1/std::sqrt(d));
    }
    struct CutPoint {P p;Vec3 n;};
    const std::array<std::function<float(Vec3)>,6> cuts{{
      [&](Vec3 p){return p.x-(cx-hx);},[&](Vec3 p){return cx+hx-p.x;},
      [&](Vec3 p){return p.y-(cy-hy);},[&](Vec3 p){return cy+hy-p.y;},
      [&](Vec3 p){return p.z;},
      [&](Vec3 p){return shield?(p.y-(cy-hy))*.7f+hx*.32f-std::fabs(p.x-cx):1.f;}
    }};
    for(auto triangle:support.triangles) {
      std::vector<CutPoint> poly;
      for(auto i:triangle)poly.push_back({support.nodes[i],normals[i]});
      for(const auto &cut:cuts) {
        std::vector<CutPoint> next;
        for(std::size_t i=0;i<poly.size();++i) {
          const auto &a=poly[i],&c=poly[(i+1)%poly.size()];
          const float da=cut(a.p.p),dc=cut(c.p.p);
          if(da>=0)next.push_back(a);
          if((da>=0)!=(dc>=0)) {
            const float t=da/(da-dc);CutPoint q=a;
            q.p.p=a.p.p*(1-t)+c.p.p*t;
            q.p.w=mix_weights(a.p.w,c.p.w,t);
            q.n=normalize(a.n*(1-t)+c.n*t);next.push_back(q);
          }
        }
        poly=std::move(next);if(poly.size()<3)break;
      }
      if(poly.size()<3)continue;
      std::vector<unsigned> ids;
      for(auto q:poly) {
        const float u=(q.p.p.x-cx+hx)/(2*hx),v=(q.p.p.y-cy+hy)/(2*hy);
        q.p.p+=q.n*lift;q.p.c=color;q.p.uv=character_surface_uv(material,u,v);
        ids.push_back(b.vertex(q.p));
      }
      for(std::size_t i=1;i+1<ids.size();++i) {
        const auto &a=b.m.bind_mesh.vertices[ids[0]],
                   &c=b.m.bind_mesh.vertices[ids[i]],
                   &d=b.m.bind_mesh.vertices[ids[i+1]];
        if(length(cross(c.position-a.position,d.position-a.position))>1e-12f*b.h*b.h)
          b.tri(ids[0],ids[i],ids[i+1]);
      }
    }
  };
  patch(-.044f,.226f,.013f,.016f,{.64f,.68f,.53f},S::Metal,.0015f,true);
  // Two low-relief front utility seams/pocket mouths retain the dark vest
  // language. Their supporting points are never guessed from an ellipse.
  for(float side:{-1.f,1.f}) {
    patch(side*.045f,.146f,.026f,.030f,{.074f,.108f,.150f},S::Jacket,.0010f,false);
    patch(side*.045f,.171f,.026f,.0020f,dark,S::Shirt,.0015f,false);
  }
  // Each side closure is a thin, closed webbing tab fitted to the actual
  // vest faces. The outer corner weights are barycentric, so shoulder/torso
  // bends cannot leave a rigid belt floating across a deforming garment.
  for (float side : {-1.f, 1.f}) {
    Shell tab;
    struct Sample { P p; Vec3 n; };
    std::vector<Vec3> normals(support.nodes.size());
    for (auto t : support.triangles) {
      const Vec3 n = cross(support.nodes[t[1]].p-support.nodes[t[0]].p,
                           support.nodes[t[2]].p-support.nodes[t[0]].p);
      for (auto i : t) normals[i] += n;
    }
    for (auto &n : normals) n = normalize(n);
    const auto add = [&](Sample q) {
      const Vec3 original = q.p.p;
      q.p.p += q.n * .0015f;
      q.p.c = {.046f,.063f,.081f};
      q.p.uv = character_surface_uv(S::Leather,
                      (original.z+.026f)/.057f,(original.y-.127f)/.022f);
      for (unsigned i=0;i<tab.nodes.size();++i)
        if (length(tab.nodes[i].p-q.p.p)<1e-7f) return i;
      const auto i=tab.point(q.p.p,q.p.c,q.p.w,q.p.uv);
      tab.thickness_directions.push_back(q.n);
      return i;
    };
    const std::array<std::function<float(Vec3)>,5> cuts{{
      [](Vec3 p){return p.y-.127f;},[](Vec3 p){return .149f-p.y;},
      [](Vec3 p){return p.z+.026f;},[](Vec3 p){return .031f-p.z;},
      [side](Vec3 p){return side*p.x-.085f;}
    }};
    for (auto t : support.triangles) {
      std::vector<Sample> poly;
      for(auto i:t) poly.push_back({support.nodes[i],normals[i]});
      for(const auto &cut:cuts) {
        std::vector<Sample> next;
        for(std::size_t i=0;i<poly.size();++i) {
          const auto &a=poly[i],&c=poly[(i+1)%poly.size()];
          const float da=cut(a.p.p),dc=cut(c.p.p);
          if(da>=0) next.push_back(a);
          if((da>=0)!=(dc>=0)) {
            const float t=da/(da-dc);
            Sample q=a;q.p.p=a.p.p*(1-t)+c.p.p*t;
            q.p.w=mix_weights(a.p.w,c.p.w,t);
            q.n=normalize(a.n*(1-t)+c.n*t);next.push_back(q);
          }
        }
        poly=std::move(next);if(poly.size()<3)break;
      }
      if(poly.size()<3)continue;
      for(std::size_t i=1;i+1<poly.size();++i) {
        const Vec3 n=cross(poly[i].p.p-poly[0].p.p,
                           poly[i+1].p.p-poly[0].p.p);
        if(dot(n,n)<1e-18f)continue;
        const auto a=add(poly[0]),c=add(poly[i]),d=add(poly[i+1]);
        if(a!=c&&a!=d&&c!=d)tab.triangles.push_back({a,c,d});
      }
    }
    tab.emit(b,.0008f);
  }
  const auto radio_weight=fit(support,.063f,.226f,true,0).w;
  const Vec3 radio_center=fit(support,.063f,.226f,true,.010f).p;
  const auto radio_first=b.m.influences.size();
  // A rounded hard radio case has a readable depth, top controls and recessed
  // grille. All corners share one fitted mount transform to keep it rigid.
  b.grid(b.near?20:12,b.near?8:4,[&](float u,float v) {
    const float a=u==1.f?0.f:2*pi*u,round=.85f+.15f*std::sin(pi*v);
    const float x=.011f*round*std::copysign(std::pow(std::fabs(std::cos(a)),.35f),std::cos(a));
    const float z=.008f*round*std::copysign(std::pow(std::fabs(std::sin(a)),.35f),std::sin(a));
    return P{radio_center+Vec3{x,lerp(-.022f,.022f,v),z},dark,wt(B::Chest),
             character_surface_uv(S::Rubber,u,v)};
  });
  for(int row=0;row<(b.near?5:3);++row) {
    const float y=radio_center.y-.009f+row*(b.near?.004f:.008f);
    b.curve({{radio_center.x-.007f,y,radio_center.z+.0081f},
             {radio_center.x+.007f,y,radio_center.z+.0081f}},
             .0007f,{.028f,.033f,.037f},S::Rubber,B::Chest,b.near?6:4);
  }
  b.tube({radio_center+Vec3{.006f,.019f,0},radio_center+Vec3{.006f,.027f,0},
           radio_center+Vec3{.007f,.067f,-.007f}},
           {.0022f,.0016f,.00065f},1.f,dark,S::Rubber,
           [](float){return wt(B::Chest);},b.near?8:5);
  b.ellipsoid(radio_center+Vec3{-.005f,.024f,0},{.0026f,.004f,.0026f},
              dark*.8f,S::Rubber,B::Chest,b.near?12:8,4);
  // A rigid case needs a fitted mounting frame, not just a center-depth
  // offset: the curved chest rises behind one edge of an axis-aligned box.
  // Construct the orthonormal frame from the actual vest support triangles.
  const Vec3 mount = fit(support,.063f,.226f,true,0).p;
  const Vec3 right = normalize(fit(support,.065f,.226f,true,0).p -
                               fit(support,.061f,.226f,true,0).p);
  const Vec3 up_hint = fit(support,.063f,.228f,true,0).p -
                       fit(support,.063f,.224f,true,0).p;
  const Vec3 outward = normalize(cross(right,up_hint));
  const Vec3 up = normalize(cross(outward,right));
  for(std::size_t i=radio_first;i<b.m.influences.size();++i) {
    const Vec3 local = b.m.bind_mesh.vertices[i].position*(1/b.h)-radio_center;
    b.m.bind_mesh.vertices[i].position =
        (mount + outward*.018f + right*local.x + up*local.y + outward*local.z)*b.h;
    b.m.influences[i]=radio_weight;
  }
  // A closed narrow mounting clip bridges the supported case to the garment;
  // lifting the case alone would leave a visibly unsupported floating box.
  Shell clip;
  for (Vec2 corner : std::array<Vec2,4>{{{-.002f,-.005f},{.002f,-.005f},
                                         {.002f,.005f},{-.002f,.005f}}}) {
    clip.point(mount + right*corner.x + up*corner.y + outward*.0105f,
               dark*.8f,radio_weight,character_surface_uv(S::Rubber,
                 (corner.x+.002f)/.004f,(corner.y+.005f)/.010f));
    clip.thickness_directions.push_back(outward);
  }
  clip.quad(0,1,2,3);
  clip.emit(b,.0090f);

  // The original belt and left hip pouch are fitted to Hale's narrower waist.
  b.grid(b.near?40:20,1,[&](float u,float v){
    const float a=u==1.f?0.f:2*pi*u;
    return P{{.0955f*std::cos(a),lerp(.048f,.066f,v),.0645f*std::sin(a)-.002f},
             dark,wt(B::Pelvis),character_surface_uv(S::Leather,u,v)};
  },true,false);
  b.ellipsoid({0,.057f,.0640f},{.0085f,.007f,.0018f},
              {.55f,.56f,.51f},S::Metal,B::Pelvis,b.near?16:8,5);
  b.grid(b.near?24:12,b.near?10:5,[&](float u,float v){
    const float a=u==1.f?0.f:2*pi*u,round=.88f+.12f*std::sin(pi*v);
    return P{{-.104f+.018f*round*std::copysign(std::pow(std::fabs(std::cos(a)),.30f),std::cos(a)),
               lerp(-.016f,.060f,v),.008f+.022f*round*std::copysign(std::pow(std::fabs(std::sin(a)),.35f),std::sin(a))},
             dark,wt(B::Pelvis),character_surface_uv(S::Leather,u,v)};
  });
  b.grid(b.near?12:6,4,[&](float u,float v){
    return P{{lerp(-.121f,-.087f,1-u),lerp(.041f,.061f,v),.0310f+.0017f*std::sin(pi*v)},
             dark*.77f,wt(B::Pelvis),character_surface_uv(S::Leather,u,v)};
  },false,false);
  b.ellipsoid({-.104f,.044f,.033f},{.0026f,.0026f,.0008f},
              {.38f,.41f,.40f},S::Metal,B::Pelvis,b.near?12:8,4);
}
inline void make_hale_vest(character_detail::Sculpt &b,const hale_shirt_detail::ShirtSupport &shirt) {
  using namespace character_detail;using namespace hale_vest_detail;
  const Vec3 vest{.08f,.12f,.165f};
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
    if(t<.0005f)return a;
    if(t>.9995f)return c;
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
    // A complete phase sweep reveals the rear underarm shirt sweep between
    // the coarse gait keys. Local ease accommodates that real moving layer,
    // preserving the fitted vest and all original source/rig inputs.
    const float rear_underarm = b.near ? .0040f *
        g(std::fabs(original.x), .073f, .028f) *
        g(original.y, .207f, .044f) *
        (1 - smooth((original.z + .030f) / .050f)) : 0.f;
    q.p.p+=q.n*((b.near?.0080f:.0105f)+hang*.3f+rear_underarm);q.p.c=vest;
    const float a=std::atan2(original.x,std::fabs(original.z));
    q.p.uv=character_surface_uv(S::Jacket,.5f+a/pi,smooth((original.y-.06f)/.30f));
    auto id=s.point(q.p.p,q.p.c,q.p.w,q.p.uv);s.thickness_directions.push_back(q.n);welded[key].push_back(id);source_positions.push_back(original);return id;
  };
  const std::array<std::function<float(Vec3)>,3> cuts{{
    [](Vec3 p){float hem=.080f+.007f*g(std::fabs(p.x),.097f,.020f);return p.y-hem;},
    [](Vec3 p){float front=smooth((p.z-.008f)/.050f);return .373f-(.077f*front+.041f*(1-front))*g(p.x,0,.035f)-p.y;},
    // A vest's armhole must clear the moving sleeve, not share its cut edge.
    // The enlarged opening leaves a real band of shirt visible around it.
    [](Vec3 p){return std::max(sq((p.y-.269f)/.090f)+sq((p.z-.003f)/.068f)-1.f,(.080f-std::fabs(p.x))/.010f);}
  }};
  // The shirt's sewn torso/yoke is the fitting surface. Hale's shallow curved
  // neck opening, protective front/back and armholes use its actual triangles
  // and influence correspondence. The lining follows the source support
  // normals, avoiding folded offsets on newly clipped narrow edge triangles.
  for(auto t:shirt.triangles) {
    std::vector<Sample> poly;for(auto i:t)poly.push_back({shirt.nodes[i],normals[i]});
    for(const auto &cut:cuts) {
      std::vector<Sample> next;
      for(std::size_t i=0;i<poly.size();++i) {
        const auto &a=poly[i],&c=poly[(i+1)%poly.size()];float da=cut(a.p.p),dc=cut(c.p.p);
        if(da>=0)next.push_back(a);
        if((da>=0)!=(dc>=0)) {
          // These garment cuts are curved signed-distance fields. Solve the
          // edge root instead of treating the field as a linear plane; the
          // latter left a visibly serrated armhole at coarse source spacing.
          float low=0.f,high=1.f;
          for(int iteration=0;iteration<20;++iteration) {
            const float middle=(low+high)*.5f;
            const float value=cut(a.p.p*(1-middle)+c.p.p*middle);
            if((value>=0)==(da>=0))low=middle;else high=middle;
          }
          next.push_back(blend(a,c,(low+high)*.5f));
        }
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
  s.emit(b,.0024f);
  make_hale_equipment(b,s);
}
}
