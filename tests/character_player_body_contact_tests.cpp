#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
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
  // Actor-owned regions: 0 shirt, 1 sling/pouch/chest accent, 2 complete
  // source head/neck, 3/4 hand-forearm, 5/6 trousers, 7/8 footwear and socks.
  std::vector<std::pair<unsigned,int>> classify(const CharacterModel&m){
    std::vector<std::pair<unsigned,int>> out;
    for(unsigned t=0;t<m.bind_mesh.indices.size();t+=3){
      const auto &v=m.bind_mesh.vertices[m.bind_mesh.indices[t]];
      const int s=surface(v.uv);
      Vec3 c{};
      for(int k=0;k<3;++k)c+=m.bind_mesh.vertices[m.bind_mesh.indices[t+k]].position*(1/(3*m.rig.height));
      int r=-1;
      if(s==int(CharacterSurface::Face)||(s==int(CharacterSurface::Skin)&&c.y>.29f))r=2;
      else if(s==int(CharacterSurface::Skin)&&c.y<.2f)r=c.x<0?3:4;
      else if(s==int(CharacterSurface::Pants))r=c.x<0?5:6;
      else if((s==int(CharacterSurface::Leather)||s==int(CharacterSurface::Rubber)||s==int(CharacterSurface::Shirt))&&c.y<-.40f)r=c.x<0?7:8;
      else if(s==int(CharacterSurface::Leather)&&c.y>.05f)r=1;
      else if(s==int(CharacterSurface::Shirt)&&c.y>-.05f)r=0;
      if(r>=0)out.push_back({
        t,r
      });
    }
    return out;
  }
  bool pair_allowed(int a,int b){
    if(a>b)std::swap(a,b);
    if(a==0)return b==1||b==2||b==3||b==4||b==5||b==6;
    if(a==1)return b==2||b==3||b==4||b==5||b==6;
    if(a==3||a==4)return b==5||b==6;
    if(a==5)return b==7;
    if(a==6)return b==8;
    return false;
  }
  std::vector<Tri> triangles(const Mesh &mesh, float h,
  const std::vector<std::pair<unsigned, int>> &groups,
  const Mesh *bind = nullptr) {
    std::vector<Tri> out;
    out.reserve(groups.size());
    for (auto entry : groups) {
      Tri t{
        {}, {}, entry.first, entry.second
      };
      for (int i = 0; i < 3; ++i) {
        t.p[i] = mesh.vertices[mesh.indices[entry.first + i]].position * (1 / h);
        t.box.add(t.p[i]);
      }
      const auto &source = bind ? *bind : mesh;
      for (int i = 0; i < 3; ++i)
      t.original_y +=
      source.vertices[source.indices[entry.first + i]].position.y / (3 * h);
      out.push_back(t);
    }
    return out;
  }
  std::string audit(const std::vector<Tri> &ts) {
    Tree tree(ts);
    for (const auto &t : ts) {
      std::string failure;
      tree.visit(t, [&](const Tri &other) {
        if (!failure.empty() || other.offset <= t.offset ||
        !(pair_allowed(t.region, other.region) ||
        (t.region >= 5 && t.region <= 6 && other.region >= 5 && other.region <= 6 && t.original_y < -.065f &&
        t.original_y > -.40f && other.original_y < -.065f &&
        other.original_y > -.40f)))
        return;
        for (auto a : t.p)
        for (auto b : other.p)
        if (length(a - b) < 1e-6f)
        return; // Shared physical seam/edge is intentional contact
        if (crossing(t, other))
        failure = "regions " + std::to_string(t.region) + "/" +
        std::to_string(other.region) + " triangles " +
        std::to_string(t.offset / 3) + "/" +
        std::to_string(other.offset / 3);
      });
      if (!failure.empty())
      return failure;
    }
    return {};
  }
  void negative_controls(const std::vector<Tri>&ts){
    std::array<const Tri*,9> chosen{};
    for(const auto&t:ts)if(!chosen[t.region])chosen[t.region]=&t;
    for(auto p:chosen)require(p,"All actual contact regions represented");
    unsigned count=0;
    for(auto pair:{
      std::pair<int,int>{
        0,1
      },{
        0,2
      },{
        0,3
      },{
        0,4
      },{
        0,5
      },{
        0,6
      },{
        1,2
      },{
        1,3
      },{
        1,4
      },{
        3,5
      },{
        4,6
      },{
        5,7
      },{
        6,8
      }
    }){
      auto a=*chosen[pair.first],b=*chosen[pair.second];
      const Vec3 center=(a.p[0]+a.p[1]+a.p[2])*(1.f/3),n=normalize(cross(a.p[1]-a.p[0],a.p[2]-a.p[0])),u=normalize(a.p[1]-a.p[0]);
      const float d=std::min(length(a.p[1]-a.p[0]),length(a.p[2]-a.p[0]))*.15f;
      b.p={
        center+n*d,center-n*d+u*d*.1f,center+n*d+u*d*.2f
      };
      b.box={};
      for(auto p:b.p)b.box.add(p);
      if(a.offset>b.offset)std::swap(a.offset,b.offset);
      require(!audit({
        a,b
      }).empty(),"Injected real crossing rejected");
      ++count;
    }
    std::cout<<count<<" injected actual-triangle contact controls passed\n";
  }
} // namespace
int main(int argc,char**argv){
  bool quick=false,stress=false;
  for(int i=1;i<argc;++i){
    quick|=std::string(argv[i])=="--quick";
    stress|=std::string(argv[i])=="--stress";
  }int failures=0;
  for(float h:{
    1.35f,1.75f,2.10f
  }){
    if(quick&&h!=1.75f)continue;
    for(auto lod:{
      fury::CharacterLod::Near,fury::CharacterLod::Far
    }){
      auto p=fury::make_character_profile(h,fury::CharacterRole::Player,"PlayerBody",lod);
      auto&m=p.model;
      auto groups=classify(m);
      auto posed=m.bind_mesh;
      std::string context=std::string(lod==fury::CharacterLod::Near?"Near":"Far")+" height="+std::to_string(h);
      auto check=[&](std::string phase){
        auto ts=triangles(posed,h,groups,&m.bind_mesh);
        auto fail=audit(ts);
        if(!fail.empty()){
          ++failures;
          std::cerr<<context<<" "<<phase<<" FAILED "<<fail<<'\n';
        }
      };
      try{
        negative_controls(triangles(posed,h,groups,&m.bind_mesh));
      }catch(const std::exception&e){
        std::cerr<<context<<" controls "<<e.what()<<'\n';
        return 2;
      }
      check("bind");
      for(int mode=0;mode<(stress?6:5);++mode)for(int frame=0;frame<32;frame+=quick?8:2){
        fury::CharacterAnimationSample s;
        s.seed=m.seed;
        s.phase_cycles=frame/32.;
        s.idle_time=frame/8.;
        s.speed=mode==1?h*.85f:mode==2?h*2.6f:mode==5?h*.55f:0;
        s.move_weight=mode==1||mode==2||mode==5?1:0;
        s.crouch_weight=mode>=4?1:0;
        s.talk_weight=mode==3?1:0;
        s.attention_weight=mode==3?1:0;
        s.attention_yaw=mode==3?(frame%4?.9f:-.9f):0;
        s.attention_pitch=mode==3?(frame%4?.4f:-.4f):0;
        fury::apply_character_pose(m,fury::sample_character_animation(m.rig,s),posed);
        check("mode="+std::to_string(mode)+" phase="+std::to_string(frame));
      }
    }
  }
  std::cout<<"scope="<<(stress?"strict including full crouch-walk":"ordinary including static crouch")<<" failures="<<failures<<'\n';
  return failures?1:0;
}
