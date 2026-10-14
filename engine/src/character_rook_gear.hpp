#pragma once
#include "character_rook_shirt.hpp"
#include <tuple>

namespace fury::rook_gear_detail {
using namespace character_detail;
using LegRanges = std::array<std::pair<std::size_t, std::size_t>, 2>;

// Rook's shell is authored on the actual sewn shirt, with separate cut lines,
// hanging ease and folded thickness. All fitting stays in the normalized rig.
struct Patch {
  std::vector<P> nodes;
  std::vector<std::array<unsigned, 3>> faces;
  std::vector<Vec3> extrusion;
};
inline CharacterInfluence mix_weights(const CharacterInfluence &a,
                                      const CharacterInfluence &c, float t) {
  std::array<float, character_bone_count> values{};
  auto add = [&](CharacterInfluence w, float amount) {
    values[w.first] += w.first_weight * amount;
    values[w.second] += (1 - w.first_weight - w.third_weight) * amount;
    values[w.third] += w.third_weight * amount;
  };
  add(a, 1 - t);
  add(c, t);
  std::array<unsigned, character_bone_count> order{};
  for (unsigned i = 0; i < order.size(); ++i)
    order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](unsigned i, unsigned j) {
    return values[i] > values[j];
  });
  float total = values[order[0]] + values[order[1]] + values[order[2]];
  if (total < .99999f)
    throw std::runtime_error(
        "Rook fitting needs more than three influences: retained=" +
        std::to_string(total));
  const float first = values[order[0]] / total;
  const float third = std::min(1 - first, values[order[2]] / total);
  return {std::uint8_t(order[0]), std::uint8_t(order[1]), first,
          std::uint8_t(order[2]), third};
}
inline std::vector<Vec3> patch_normals(const Patch &p) {
  std::vector<Vec3> n(p.nodes.size());
  for (auto t : p.faces) {
    auto normal = cross(p.nodes[t[1]].p - p.nodes[t[0]].p,
                        p.nodes[t[2]].p - p.nodes[t[0]].p);
    if (dot(normal, normal) <= 1e-20f)
      throw std::runtime_error("Degenerate Rook garment triangle");
    for (auto v : t)
      n[v] += normal;
  }
  for (auto &v : n)
    if (dot(v, v) > 1e-20f)
      v = normalize(v);
  return n;
}
inline void emit_shell(Sculpt &b, const Patch &p, float thickness, S surface, bool crease_faces=false) {
  const auto normals = patch_normals(p);
  const auto &extrusion = p.extrusion.empty() ? normals : p.extrusion;
  std::map<std::pair<unsigned, unsigned>, unsigned> edge_count;
  std::map<std::pair<unsigned, unsigned>, std::pair<unsigned, unsigned>>
      oriented;
  for (auto t : p.faces)
    for (int k = 0; k < 3; ++k) {
      const unsigned a = t[k], c = t[(k + 1) % 3];
      const auto edge = std::minmax(a, c);
      if (++edge_count[edge] > 2)
        throw std::runtime_error("Branched Rook garment shell");
      oriented[edge] = {a, c};
    }
  std::array<std::vector<std::vector<unsigned>>, 2> groups;
  for (auto &v : groups)
    v.resize(p.nodes.size());
  std::map<std::tuple<unsigned, int, int>, unsigned> corners;
  unsigned facet=0;
  auto emit = [&](unsigned source, int layer, int chart) {
    const auto key = std::make_tuple(source, layer, chart+(crease_faces?int(facet)*2:0));
    auto found = corners.find(key);
    if (found != corners.end())
      return found->second;
    P q = p.nodes[source];
    if (layer) {
      q.p -= extrusion[source] * thickness;
      q.c = q.c * .93f;
    }
    float u = q.uv.x;
    if (chart)
      u += 1;
    // A positive uniform angular mapping, split at the back center. Fitting
    // data stays local; no triangle spans the atlas gutter at that seam.
    q.uv =
        character_surface_uv(surface, .04f + .44f * u, .025f + .95f * q.uv.y);
    unsigned v = b.vertex(q);
    if(!crease_faces)groups[layer][source].push_back(v);
    corners.emplace(key, v);
    return v;
  };
  for (auto t : p.faces) {
    ++facet;
    float low = 2, high = -1;
    for (auto i : t) {
      low = std::min(low, p.nodes[i].uv.x);
      high = std::max(high, p.nodes[i].uv.x);
    }
    unsigned outer[3], inner[3];
    for (int k = 0; k < 3; ++k) {
      int chart = high - low > .5f && p.nodes[t[k]].uv.x < .5f ? 1 : 0;
      outer[k] = emit(t[k], 0, chart);
      inner[k] = emit(t[k], 1, chart);
    }
    b.tri(outer[0], outer[1], outer[2]);
    b.tri(inner[0], inner[2], inner[1]);
  }
  for (const auto &entry : edge_count)
    if (entry.second == 1) {
      auto edge = oriented.at(entry.first);
      P a = p.nodes[edge.first], c = p.nodes[edge.second], ia = a, ic = c;
      ia.p -= extrusion[edge.first] * thickness;
      ic.p -= extrusion[edge.second] * thickness;
      for (P *q : {&a, &c, &ia, &ic}) {
        q->c = q->c * .80f;
        q->uv = character_surface_uv(surface, .1f, .1f);
      }
      const auto va = b.vertex(a), vc = b.vertex(c), vi = b.vertex(ia),
                 vj = b.vertex(ic);
      b.tri(va, vi, vj);
      if(crease_faces) b.tri(b.vertex(a),b.vertex(ic),vc);
      else b.tri(va, vj, vc);
    }
  for (auto &layer : groups)
    for (auto &group : layer)
      if (group.size() > 1)
        b.normal_groups.push_back(std::move(group));
}

struct FitPoint {
  P vertex;
  Vec3 normal;
};
inline FitPoint lerp_fit(const FitPoint &a, const FitPoint &c, float t) {
  FitPoint q = a;
  q.vertex.p = a.vertex.p * (1 - t) + c.vertex.p * t;
  q.vertex.w = mix_weights(a.vertex.w, c.vertex.w, t);
  q.normal = normalize(a.normal * (1 - t) + c.normal * t);
  return q;
}
inline P surface_sample(const Patch &patch, float x, float y, bool front,
                        float lift) {
  bool found = false;
  float best = front ? -10.f : 10.f;
  P result{};
  for (auto tri : patch.faces) {
    const P &a = patch.nodes[tri[0]], &b = patch.nodes[tri[1]],
            &c = patch.nodes[tri[2]];
    Vec3 e = b.p - a.p, f = c.p - a.p;
    float det = e.x * f.y - e.y * f.x;
    if (std::fabs(det) < 1e-12f)
      continue;
    float u = ((x - a.p.x) * f.y - (y - a.p.y) * f.x) / det,
          v = (e.x * (y - a.p.y) - e.y * (x - a.p.x)) / det;
    if (u < -1e-6f || v < -1e-6f || u + v > 1.000001f)
      continue;
    float z = a.p.z + u * e.z + v * f.z;
    if (front ? z <= best : z >= best)
      continue;
    found = true;
    best = z;
    result = a;
    result.p = {x, y, z + (front ? lift : -lift)};
    u = std::max(0.f, u);
    v = std::max(0.f, v);
    float sum = u + v;
    result.w = sum > 1e-7f ? mix_weights(a.w, mix_weights(b.w, c.w, v / sum),
                                         std::min(1.f, sum))
                           : a.w;
  }
  if (!found)
    throw std::runtime_error("Rook trim lacks actual vest support at " +
                             std::to_string(x) + "," + std::to_string(y));
  return result;
}
} // namespace fury::rook_gear_detail


namespace fury::rook_gear_detail {
// Clip the original shirt triangles, retaining their support and interpolated
// influences. All attachment cuts are linear; crossing points are exact along
// the source triangle edges and shared physically across texture charts.
inline Patch cut_patch(const Patch &source,
                        const std::vector<std::function<float(Vec3)>> &cuts,
                        float lift, Vec3 color) {
  const auto normal = patch_normals(source);
  Patch result;
  std::vector<Vec3> originals;
  const auto keep = [&](FitPoint p) {
    for (unsigned i = 0; i < originals.size(); ++i)
      if (length(originals[i] - p.vertex.p) < 1e-7f)
        return i;
    const unsigned i = result.nodes.size();
    originals.push_back(p.vertex.p);
    p.vertex.p += p.normal * lift;
    p.vertex.c = color;
    p.vertex.uv = {.5f + std::atan2(p.vertex.p.x, -p.vertex.p.z)/(2*pi),
                   std::clamp((p.vertex.p.y-.045f)/.31f,0.f,1.f)};
    result.nodes.push_back(p.vertex);
    result.extrusion.push_back(p.normal);
    return i;
  };
  for (auto face : source.faces) {
    std::vector<FitPoint> poly;
    for (auto i : face) poly.push_back({source.nodes[i],normal[i]});
    for (const auto &cut : cuts) {
      std::vector<FitPoint> next;
      for (unsigned i=0; i<poly.size(); ++i) {
        auto a=poly[i], c=poly[(i+1)%poly.size()];
        float da=cut(a.vertex.p), dc=cut(c.vertex.p);
        if (da>=0) next.push_back(a);
        if ((da>=0)!=(dc>=0)) next.push_back(lerp_fit(a,c,da/(da-dc)));
      }
      poly=std::move(next);
      if (poly.size()<3) break;
    }
    if (poly.size()<3) continue;
    auto a=keep(poly[0]);
    for (unsigned i=1; i+1<poly.size(); ++i) {
      auto c=keep(poly[i]), d=keep(poly[i+1]);
      if (a!=c && a!=d && c!=d &&
          length(cross(result.nodes[c].p-result.nodes[a].p,
                       result.nodes[d].p-result.nodes[a].p))>1e-12f)
        result.faces.push_back({a,c,d});
    }
  }
  return result;
}
inline float rounded(float a) {
  return std::copysign(std::pow(std::fabs(a),.42f),a);
}
} // namespace fury::rook_gear_detail

namespace fury {
inline void make_rook_gear(character_detail::Sculpt &b,
                           const rook_shirt_detail::ShirtSupport &shirt,
                           const rook_gear_detail::LegRanges &) {
  using namespace character_detail;
  using namespace rook_gear_detail;
  const Vec3 harness_color{.055f,.065f,.075f}, canvas{.29f,.24f,.16f};
  const Vec3 hardware{.48f,.50f,.47f};
  const Patch substrate{shirt.nodes,shirt.triangles,{}};
  const auto diagonal=[](Vec3 p) {return p.y+1.65f*p.x-.207f;};
  Patch harness=cut_patch(substrate,
    {[=](Vec3 p){return diagonal(p)+.021f;},
     [=](Vec3 p){return .021f-diagonal(p);}},b.near?.0031f:.0025f,harness_color);
  // The shirt is tucked under the waist. The harness instead rests outside
  // the belt/trouser envelope and transfers load into the rigid pelvis.
  for(unsigned i=0;i<harness.nodes.size();++i) {
    auto &p=harness.nodes[i];
    p.p+=harness.extrusion[i]*(.004f*smooth((.095f-p.p.y)/.019f)+
                               (b.near?.0005f:.0018f)*g(p.p.y,.309f,.036f));
    const float envelope=std::sqrt(sq(p.p.x/.094f)+sq((p.p.z+.002f)/.065f));
    const float ease=smooth((.092f-p.p.y)/.016f);
    if(envelope<1.045f && envelope>0) {
      const float scale=1+(1.045f/envelope-1)*ease;
      p.p.x*=scale;p.p.z=(p.p.z+.002f)*scale-.002f;
    }
    p.w=mix_weights(wt(B::Pelvis),p.w,smooth((p.p.y-.074f)/.031f));
  }
  emit_shell(b,harness,.0012f,S::Jacket);
  // Two subdued edge bindings run over the entire front, shoulder and back.
  // They are cuts of the supported harness, including the coarse Far triangles.
  if(b.near) for(float side:{-1.f,1.f}) {
    const float center=side*.0178f;
    auto edge=cut_patch(harness,
      {[=](Vec3 p){return diagonal(p)-center+.0011f;},
       [=](Vec3 p){return center+.0011f-diagonal(p);}},.00025f,
       harness_color*1.24f);
    // Sewn binding is a surface finish, with no second interior shell. Its
    // support follows the same triangles; the main strap owns visible depth.
    std::vector<unsigned> vertices;
    for(auto q:edge.nodes) {
      q.uv=character_surface_uv(S::Shirt,q.uv.x,q.uv.y);
      vertices.push_back(b.vertex(q));
    }
    for(auto face:edge.faces)
      b.tri(vertices[face[0]],vertices[face[1]],vertices[face[2]]);
  }
  // A compact sliding adjuster is fitted to the front webbing. Its closed
  // rounded frame and center bar have actual openings and a localized metal
  // material, with the same live chest attachment as their supporting strap.
  {
    const int rows=b.near?24:12, sides=b.near?6:4;
    std::vector<P> path;
    const Vec3 tangent=normalize(Vec3{-1.f/1.65f,1,0});
    const Vec3 across{tangent.y,-tangent.x,0};
    for(int i=0;i<rows;++i){
      float a=2*pi*float(i)/rows;
      const Vec3 xy=Vec3{-.031f,.258f,0}+across*(.0120f*rounded(std::cos(a)))+
                    tangent*(.0100f*rounded(std::sin(a)));
      P q=surface_sample(substrate,xy.x,xy.y,true,.0050f);
      q.c=hardware*.80f;path.push_back(q);
    }
    const unsigned base=b.m.bind_mesh.vertices.size();
    b.grid(sides,rows,[&](float u,float v){
      const int j=int(std::lround(v*rows))%rows;
      const Vec3 t=normalize(path[(j+1)%rows].p-path[(j+rows-1)%rows].p);
      const Vec3 n=normalize(Vec3{0,0,1}-t*t.z),w=cross(n,t);
      P q=path[j];q.p+=(n*std::cos(2*pi*u)+w*std::sin(2*pi*u))*.00065f;
      q.uv=character_surface_uv(S::Metal,u,v);return q;
    },true,false);
    for(int i=0;i<=sides;++i)b.normal_groups.push_back({base+unsigned(i),base+unsigned(rows*(sides+1)+i)});
    std::vector<Vec3> bar;std::vector<CharacterInfluence> weights;
    const int sections=b.near?4:2;
    for(int i=0;i<=sections;++i){
      Vec3 xy=Vec3{-.031f,.258f,0}+across*lerp(-.0116f,.0116f,float(i)/sections);
      P q=surface_sample(substrate,xy.x,xy.y,true,.0052f);
      bar.push_back(q.p);weights.push_back(q.w);
    }
    b.tube(bar,std::vector<float>(bar.size(),.00065f),1,hardware*.80f,S::Metal,
       [&](float v){return weights[std::min(sections,int(std::lround(v*sections)))];},b.near?6:4);
  }
  // Preserve the small warm chest equipment patch, with a folded welt and
  // restrained central canvas panel, fitted to the actual garment.
  for(int layer=0;layer<2;++layer) {
    const float half=layer?.0125f:.0150f;
    auto patch=cut_patch(substrate,
      {[=](Vec3 p){return p.x-.046f+half;},
       [=](Vec3 p){return .046f+half-p.x;},
       [=](Vec3 p){return p.y-(layer?.212f:.208f);},
       [=](Vec3 p){return (layer?.249f:.253f)-p.y;},
       [](Vec3 p){return p.z;}},layer?.0034f:.0022f,
       b.m.appearance.accent*(layer?.88f:.58f));
    emit_shell(b,patch,.0008f,S::Jacket);
  }
  // A real belt wraps the waist. The lower shirt is tucked into the belt's
  // top edge; the pouch's stitched loops continue over this support.
  b.grid(b.near?48:24,2,[&](float u,float v){
    float a=2*pi*u;
    return P{{.095f*std::cos(a),lerp(.048f,.069f,v),.066f*std::sin(a)-.002f},
              harness_color,wt(B::Pelvis),character_surface_uv(S::Leather,u,v)};
  },true,false);
  b.grid(b.near?48:24,1,[&](float u,float v){
    float a=2*pi*u,rx=lerp(.095f,.0932f,v),rz=lerp(.066f,.0642f,v);
    return P{{rx*std::cos(a),.069f,rz*std::sin(a)-.002f},
              harness_color*.83f,wt(B::Pelvis),character_surface_uv(S::Leather,u,v)};
  },true,false);
  // Rectangular buckle frame: the opening shows the belt instead of a metal
  // slab. Four closed bars have rounded corners and a retained center tongue.
  for (auto path:std::vector<std::vector<Vec3>>{
      {{-.014f,.052f,.066f},{-.014f,.065f,.066f}},
      {{ .014f,.052f,.066f},{ .014f,.065f,.066f}},
      {{-.014f,.052f,.066f},{ .014f,.052f,.066f}},
      {{-.014f,.065f,.066f},{ .014f,.065f,.066f}},
      {{0,.052f,.067f},{0,.064f,.067f}}})
    b.curve(path,.0011f,hardware,S::Metal,B::Pelvis,b.near?8:4);

  // The pouch is one open cup: outer canvas, returned mouth, inner lining,
  // and two distinct bottom skins. Its rounded rectangular plan retains the
  // original left-hip identity while giving the repair tools a usable opening.
  const int columns=b.near?32:12, rows=b.near?8:3;
  const float cx=-.117f,cz=.070f,bottom=-.029f,top=.066f;
  const auto section=[&](float a,float v,bool inner){
    if(a>=2*pi-1e-6f)a=0.f;
    float y=lerp(bottom,top,v)+(inner?.002f*(1-v):0.f);
    float rx=lerp(.024f,.026f,smooth(v)),rz=lerp(.023f,.029f,smooth(v));
    const float ease=.0015f*std::sin(pi*v)*std::sin(2*a+.8f);
    Vec3 p{cx+(rx+ease)*rounded(std::cos(a)),y,
            cz+(rz+ease)*rounded(std::sin(a))};
    // The inner gusset follows the outside of the hip rather than crossing
    // through the trouser volume. Lining shrinks toward the pouch interior.
    const float hip=-.098f*std::sqrt(std::max(0.f,1-sq((p.z+.002f)/.065f)))-.003f;
    p.x=std::min(p.x,hip);
    // A hanging lower gusset sits to the outside of the swinging upper thigh.
    // This is sewn bag volume, not an altered body or hidden collision filter.
    const float lower=smooth((.050f-y)/.060f);
    p.x=std::min(p.x,lerp(-.108f,-.111f,lower));
    if(inner){
      Vec3 radial{p.x-cx,0,p.z-cz};
      p-=normalize(radial)*.0016f;
    }
    return p;
  };
  Patch pouch_support;
  for(bool inner:{false,true}) {
    const auto begin=b.m.bind_mesh.indices.size();
    const unsigned base=b.m.bind_mesh.vertices.size();
    b.grid(columns,rows,[&](float u,float v){
      return P{section(2*pi*u,v,inner),canvas*(inner?.47f:1.f),wt(B::Pelvis),
                character_surface_uv(S::Jacket,u,v)};
    },true,false);
    if(!inner) {
      for(unsigned i=base;i<b.m.bind_mesh.vertices.size();++i) {
        const auto &v=b.m.bind_mesh.vertices[i];
        pouch_support.nodes.push_back({v.position*(1/b.h),canvas,wt(B::Pelvis),{}});
      }
      for(unsigned t=begin;t<b.m.bind_mesh.indices.size();t+=3)
        pouch_support.faces.push_back({b.m.bind_mesh.indices[t]-base,
          b.m.bind_mesh.indices[t+1]-base,b.m.bind_mesh.indices[t+2]-base});
    }
    if(inner) for(std::size_t i=begin;i<b.m.bind_mesh.indices.size();i+=3)
      std::swap(b.m.bind_mesh.indices[i+1],b.m.bind_mesh.indices[i+2]);
    const auto center=b.vertex({{cx,bottom+(inner?.002f:0.f),cz},
                                canvas*(inner?.47f:.83f),wt(B::Pelvis),
                                character_surface_uv(S::Jacket,.5f,.5f)});
    for(int i=0;i<columns;++i) {
      if(inner)b.tri(center,base+i+1,base+i);
      else b.tri(center,base+i,base+i+1);
    }
  }
  b.grid(columns,1,[&](float u,float v){
    auto a=section(2*pi*u,1,false),c=section(2*pi*u,1,true);
    return P{a*(1-v)+c*v,canvas*.72f,wt(B::Pelvis),
             character_surface_uv(S::Leather,u,v)};
  },true,false);
  // Sewn lower binding and side reinforcement follow the real canvas skin.
  // A flat binding survives the pinched gusset without a sharply folded tube.
  for(int k=0;k<3;++k) {
    b.grid(k==0?columns:1,k==0?1:(b.near?8:4),[&](float u,float v){
      const float a=k==0?2*pi*u:(k==1?.34f*pi:.66f*pi)+lerp(-.015f,.015f,u);
      const float t=k==0?lerp(.045f,.073f,v):v;
      Vec3 p=section(a,t,false);
      p+=normalize(Vec3{p.x-cx,0,p.z-cz})*.00035f;
      return P{p,canvas*.74f,wt(B::Pelvis),character_surface_uv(S::Leather,u,v)};
    },k==0,false);
  }
  // Two broad folded hangers turn over the actual belt and descend onto the
  // pouch. Filled leather ribbons replace an implausible free oval outline.
  for(float z:{.002f,.023f}) {
    const float anchor=-.095f*std::sqrt(1-sq((z+.002f)/.066f));
    const Vec3 radial=normalize(Vec3{anchor/sq(.095f),0,(z+.002f)/sq(.066f)});
    const Vec3 width=normalize(Vec3{-radial.z,0,radial.x});
    const std::array<Vec3,5> centers{{
      {-.112f,.043f,z+.052f}, {anchor-.002f,.068f,z+.012f},
      {anchor,.077f,z}, {anchor+.004f,.066f,z-.002f},
      {anchor+.003f,.050f,z}}};
    Patch hanger;
    for(unsigned row=0;row<centers.size();++row)
      for(int column=0;column<2;++column){
        Vec3 p=centers[row]+width*(column?.0055f:-.0055f);
        hanger.nodes.push_back({p,canvas*.63f,wt(B::Pelvis),
                                 {float(column),float(row)/4}});
      }
    for(unsigned row=0;row+1<centers.size();++row){
      unsigned a=row*2,c=a+2;
      hanger.faces.push_back({a,c,c+1});hanger.faces.push_back({a,c+1,a+1});
    }
    emit_shell(b,hanger,.0012f,S::Leather,true);
  }
  // Cut the slip pocket from the actual cup faces, retaining a supported
  // lower/side seam and a raised turned mouth. No free-standing rectangle
  // spans the rounded gussets or projects into the thigh.
  Patch slip=cut_patch(pouch_support,
    {[](Vec3 p){return p.x+.137f;},[](Vec3 p){return -.111f-p.x;},
     [](Vec3 p){return p.y+.018f;},[](Vec3 p){return .042f-p.y;},
     [](Vec3 p){return p.z-.076f;}},.0013f,canvas*.91f);
  for(unsigned i=0;i<slip.nodes.size();++i) {
    auto &p=slip.nodes[i];
    float u=std::clamp((p.p.x+.137f)/.026f,0.f,1.f);
    float v=std::clamp((p.p.y+.018f)/.060f,0.f,1.f);
    p.p+=slip.extrusion[i]*(.0032f*std::sin(pi*u)*smooth(v));
  }
  emit_shell(b,slip,.0008f,S::Jacket);
  // Original two neutral metal handles remain visible. Joined grips and shafts
  // terminate inside the lined cup; they are presentation props, never weapons.
  for(int tool=0;tool<2;++tool) {
    const float x=tool?-.118f:-.132f,z=.081f;
    b.curve({{x,.038f,z},{x,.087f,z}},.0023f,hardware*.85f,S::Metal,
             B::Pelvis,b.near?8:5);
    b.grid(b.near?12:6,3,[&](float u,float v){
      float a=2*pi*u,r=.0033f+.0005f*std::sin(pi*v);
      return P{{x+r*std::cos(a),lerp(.074f,.104f,v),z+r*std::sin(a)},
               hardware,wt(B::Pelvis),character_surface_uv(S::Metal,u,v)};
    });
    if(b.near)for(int ring=0;ring<3;++ring)
      b.grid(10,1,[&](float u,float v){
        float a=2*pi*u;
        return P{{x+.0039f*std::cos(a),.080f+ring*.007f+v*.0012f,
                  z+.0039f*std::sin(a)},hardware*.71f,wt(B::Pelvis),
                  character_surface_uv(S::Metal,u,v)};
      },true,false);
  }
}
} // namespace fury
