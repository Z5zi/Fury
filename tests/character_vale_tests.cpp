#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/vale_anatomy.hpp"
#include "fury/generated/vale_hands.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
using namespace fury;
namespace {
void require(bool p, const std::string &message) {
  if (!p)
    throw std::runtime_error(message);
}
bool finite(Vec3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool same(Vec3 a, Vec3 b, float e = 1e-6f) { return length(a - b) <= e; }
int material(Vec2 uv) {
  for (int i = 0; i < int(CharacterSurface::Count); ++i) {
    auto lo = character_surface_uv(CharacterSurface(i), 0, 0),
         hi = character_surface_uv(CharacterSurface(i), 1, 1);
    if (uv.x >= lo.x - 1e-7f && uv.y >= lo.y - 1e-7f && uv.x <= hi.x + 1e-7f &&
        uv.y <= hi.y + 1e-7f)
      return i;
  }
  return -1;
}
struct Bounds {
  Vec3 lo{100, 100, 100}, hi{-100, -100, -100};
  void add(Vec3 p) {
    lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
    hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
  }
};
void geometry(const CharacterProfile &p) {
  auto &m = p.model;
  float h = m.rig.height;
  Bounds bounds;
  unsigned tiny = 0, reversed = 0;
  std::array<unsigned, int(CharacterSurface::Count)> counts{};
  require(p.individualized && m.seed == 0xf5cd87d6u &&
              m.role == CharacterRole::Security,
          "Vale identity");
  require(m.dual_quaternion_skinning && p.lod_distance == 12, "Vale skin/LOD");
  require(same(m.appearance.skin, {.90f, .69f, .52f}) &&
              same(m.appearance.hair, {.055f, .042f, .033f}) &&
              same(m.appearance.eyes, {.13f, .08f, .045f}) &&
              same(m.appearance.shoes, {.19f, .125f, .078f}),
          "Seeded palette unchanged");
  require(m.bind_mesh.vertices.size() == m.influences.size(),
          "Weights cover every vertex");
  for (unsigned i = 0; i < m.bind_mesh.vertices.size(); ++i) {
    const auto &v = m.bind_mesh.vertices[i];
    const auto w = m.influences[i];
    require(finite(v.position) && finite(v.normal) && finite(v.color),
            "Finite vertex");
    require(std::fabs(length(v.normal) - 1) < .0005f,
            "Unit normal at " + std::to_string(i));
    require(
        w.first < character_bone_count && w.second < character_bone_count &&
            w.third < character_bone_count && std::isfinite(w.first_weight) &&
            std::isfinite(w.third_weight) && w.first_weight >= 0 &&
            w.third_weight >= 0 && w.first_weight + w.third_weight <= 1.000001f,
        "Normalized influences");
    const int s = material(v.uv);
    require(s >= 0, "Material atlas chart");
    ++counts[s];
    bounds.add(v.position);
  }
  for (unsigned k = 0; k < m.bind_mesh.indices.size(); k += 3) {
    auto ia = m.bind_mesh.indices[k], ib = m.bind_mesh.indices[k + 1],
         ic = m.bind_mesh.indices[k + 2];
    require(std::max({ia, ib, ic}) < m.bind_mesh.vertices.size(),
            "Index bounds");
    const auto &a = m.bind_mesh.vertices[ia], &b = m.bind_mesh.vertices[ib],
               &c = m.bind_mesh.vertices[ic];
    const auto n = cross(b.position - a.position, c.position - a.position);
    tiny += dot(n, n) < 1e-20f;
    if(dot(n,a.normal+b.normal+c.normal)<=0){if(reversed<8)std::cout<<"Reversed triangle "<<k/3<<" surface "<<material(a.uv)<<" center "<<(a.position.x+b.position.x+c.position.x)/(3*h)<<","<<(a.position.y+b.position.y+c.position.y)/(3*h)<<","<<(a.position.z+b.position.z+c.position.z)/(3*h)<<'\n';++reversed;}
    require(material(a.uv) == material(b.uv) &&
                material(a.uv) == material(c.uv),
            "Atlas triangle region separation");
  }
  std::cout << (m.lod == CharacterLod::Near ? "Near" : "Far") << " vertices "
            << m.bind_mesh.vertices.size() << " triangles "
            << m.bind_mesh.indices.size() / 3 << " degenerate " << tiny
            << " reversed " << reversed << '\n';
  require(tiny == 0, "No degenerate triangles");
  require(reversed == 0, "Winding/normal agreement");
  require(std::fabs(bounds.lo.y / h + .5f) < 1e-5f &&
              std::fabs(bounds.hi.y / h - .5f) < 1e-5f,
          "Exact total height/sole");
  for (auto s : {CharacterSurface::Face, CharacterSurface::Skin,
                 CharacterSurface::Eyes, CharacterSurface::HairCrop,
                 CharacterSurface::Hair, CharacterSurface::Shirt,
                 CharacterSurface::Pants, CharacterSurface::Jacket,
                 CharacterSurface::Rubber, CharacterSurface::Leather})
    require(counts[int(s)] > 0, "Required real surface");
  require(m.bind_mesh.indices.size() / 3 <= (m.lod==CharacterLod::Near?40000:16000), "Explicit model budget");
}
void source(const CharacterModel &m) {
  const float h = m.rig.height;
  unsigned cursor = 0;
  std::vector<bool> seen(vale_anatomy::positions.size());
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    if (material(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) !=
        int(CharacterSurface::Face))
      continue;
    for (int k = 0; k < 3; ++k) {
      require(cursor < vale_anatomy::indices.size(), "No extra source face");
      const auto i = vale_anatomy::indices[cursor],
                 u = vale_anatomy::uv_indices[cursor];
      const auto &v = m.bind_mesh.vertices[m.bind_mesh.indices[t + k]];
      require(same(v.position, vale_anatomy::positions[i] * h, 1e-7f * h),
              "Every original anatomy position retained");
      const auto expected = character_surface_uv(
          CharacterSurface::Face, vale_anatomy::uv_coordinates[u].x,
          vale_anatomy::uv_coordinates[u].y);
      require(std::fabs(v.uv.x - expected.x) < 1e-7f &&
                  std::fabs(v.uv.y - expected.y) < 1e-7f,
              "Every source UV corner retained");
      if (vale_anatomy::positions[i].y >= .373f) {
        const auto w = m.influences[m.bind_mesh.indices[t + k]];
        require(w.first == bone_index(CharacterBone::Head) && w.first_weight == 1.f && w.third_weight == 0.f,
                "Face above lower-neck blend is rigid to Head, including mouth planes");
      }
      seen[i] = true;
      ++cursor;
    }
  }
  require(cursor == vale_anatomy::indices.size() &&
              std::all_of(seen.begin(), seen.end(), [](bool p) { return p; }),
          "Complete anatomical source surface");
  require(same(m.rig.joints[bone_index(CharacterBone::Head)].bind_position,
               vale_anatomy::head_center * h) &&
              same(m.rig.joints[bone_index(CharacterBone::Neck)].bind_position,
                   vale_anatomy::neck_center * h),
          "Measured anatomical rig pivots");
}
float projected_front(Vec3 p, const Mesh &mesh,
                      const std::vector<unsigned> &triangles) {
  float front = -100;
  for (unsigned t : triangles) {
    const auto a = mesh.vertices[mesh.indices[t]].position,
               b = mesh.vertices[mesh.indices[t + 1]].position,
               c = mesh.vertices[mesh.indices[t + 2]].position;
    const float det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(det) < 1e-12f)
      continue;
    float u = ((p.x - a.x) * (c.y - a.y) - (p.y - a.y) * (c.x - a.x)) / det,
          v = ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) / det;
    if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
      front = std::max(front, a.z + u * (b.z - a.z) + v * (c.z - a.z));
  }
  return front;
}
void eyes(const CharacterModel &m) {
  std::vector<unsigned> eye, face;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    int s = material(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv);
    if (s == int(CharacterSurface::Eyes))
      eye.push_back(t);
    if (s == int(CharacterSurface::Face))
      face.push_back(t);
  }
  float low = 1, high = -1, inside = 1, central = 1; unsigned lid_covered = 0;
  for (auto ring :
       {vale_anatomy::left_eye_opening, vale_anatomy::right_eye_opening}) {
    Vec3 center{};
    for (auto p : ring)
      center += p;
    center = center * (1.f / ring.size());
    for (auto r : ring)
      for (float t : {0.f, .25f, .5f, .75f, 1.f}) {
        const Vec3 p = (center * (1 - t) + r * t) * m.rig.height;
        float z = projected_front(p, m.bind_mesh, eye);
        require(z > -10, "Globe covers eye aperture");
        if(t<1){float d=(z-projected_front(p,m.bind_mesh,face))/m.rig.height;inside=std::min(inside,d);if(t<=.5f)central=std::min(central,d);if(d<=.0001f){++lid_covered;require(t>.5f,"Lid must not obscure the central half aperture");}}
        else {
          low = std::min(low, (z - p.z) / m.rig.height);
          high = std::max(high, (z - p.z) / m.rig.height);
        }
      }
  }
  std::cout << "Actual socket rim globe depth/h " << low << " .. " << high
            << '\n';
  std::cout << "Minimum aperture clearance/h " << inside << '\n';
  // The original rolled upper lid projects over two outer (75%) spokes.
  // Those samples are skin by construction, not uncovered aperture. Forcing
  // the globe in front would make it protrude through the intact lid. Require
  // the full central half-aperture to remain visible and record covered outer
  // samples, while independently bounding every actual socket-rim depth.
  std::cout<<"Central half-aperture clearance/h "<<central<<"; outer-spoke samples covered by the original projected lid fold "<<lid_covered<<'\n';
  require(central>.0001f,"Actual eye remains visible in central aperture");
  require(low > -.003f && high < .002f, "Actual eye fitting tolerance");
}
bool segment_triangle(Vec3 p, Vec3 q, Vec3 a, Vec3 b, Vec3 c, float h) {
  const Vec3 d = q - p, e = b - a, f = c - a, r = cross(d, f);
  float det = dot(e, r);
  if (std::fabs(det) < 1e-14f * h * h * h)
    return false;
  Vec3 v = p - a;
  float u = dot(v, r) / det;
  Vec3 s = cross(v, e);
  float w = dot(d, s) / det, t = dot(f, s) / det;
  return u > 1e-5f && w > 1e-5f && u + w < 1 - 1e-5f && t > 1e-5f &&
         t < 1 - 1e-5f;
}
struct Triangle {
  std::array<Vec3, 3> p;
  Bounds bounds;
  unsigned index;
};
std::vector<Triangle> select(const CharacterModel &m, const Mesh &mesh,
                             int surface, float ymin, float ymax) {
  std::vector<Triangle> r;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    if (material(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) != surface)
      continue;
    Bounds bind;
    Triangle q{};
    q.index = t / 3;
    for (int k = 0; k < 3; ++k) {
      auto i = mesh.indices[t + k];
      bind.add(m.bind_mesh.vertices[i].position * (1 / m.rig.height));
      q.p[k] = mesh.vertices[i].position;
      q.bounds.add(q.p[k]);
    }
    if (bind.hi.y >= ymin && bind.lo.y <= ymax)
      r.push_back(q);
  }
  return r;
}
unsigned intersections(const std::vector<Triangle> &a,
                       const std::vector<Triangle> &b, float h) {
  unsigned count = 0;
  for (const auto &x : a)
    for (const auto &y : b) {
      if (x.bounds.hi.x < y.bounds.lo.x || y.bounds.hi.x < x.bounds.lo.x ||
          x.bounds.hi.y < y.bounds.lo.y || y.bounds.hi.y < x.bounds.lo.y ||
          x.bounds.hi.z < y.bounds.lo.z || y.bounds.hi.z < x.bounds.lo.z)
        continue;
      bool hit = false;
      for (int k = 0; k < 3; ++k)
        hit |= segment_triangle(x.p[k], x.p[(k + 1) % 3], y.p[0], y.p[1],
                                y.p[2], h) ||
               segment_triangle(y.p[k], y.p[(k + 1) % 3], x.p[0], x.p[1],
                                x.p[2], h);
      if (hit) {
        if (count < 3)
          std::cout << " crossing " << x.index << ":" << y.index;
        ++count;
      }
    }
  return count;
}
void contacts(const CharacterModel &m, const Mesh &mesh,
              const std::string &label) {
  const int face = int(CharacterSurface::Face),
            hair = int(CharacterSurface::HairCrop),
            shirt = int(CharacterSurface::Shirt),
            vest = int(CharacterSurface::Jacket);
  auto f = select(m, mesh, face, .346f, 1), h = select(m, mesh, hair, 0, 1),
       c = select(m, mesh, shirt, .34f, 1),
       v = select(m, mesh, vest, .070f, .39f),
       s = select(m, mesh, shirt, .070f, .39f);
  unsigned fh = intersections(f, h, m.rig.height),
           fc = intersections(f, c, m.rig.height),
           vs = intersections(v, s, m.rig.height);
  std::cout << '\n'
            << label << " face/hair " << fh << " face/collar-cap " << fc
            << " vest/shirt " << vs << '\n';
  require(fh == 0, "No scalp/hair crossings: " + label);
  require(fc == 0, "No exposed neck/cap crossings: " + label);
  require(vs == 0, "No vest/shirt crossings: " + label);
}
// Derive the lip seam from the exact canonical charts, rather than a broad
// coordinate box or a helper joint that lives inside the mouth.
std::set<unsigned> lip_runtime_vertices(const CharacterModel& m) {
  std::set<unsigned> external, interior, source_lip, runtime;
  for (unsigned t = 0; t < vale_anatomy::indices.size(); t += 3) {
    const auto uv = vale_anatomy::uv_coordinates[vale_anatomy::uv_indices[t]];
    const bool head = uv.x > .64f && uv.y >= .14869f && uv.y <= .81764f;
    const bool mouth = uv.x > .75f && uv.x < .93f && uv.y < .13645f;
    for (unsigned k = 0; k < 3; ++k) {
      if (head) external.insert(vale_anatomy::indices[t + k]);
      if (mouth) interior.insert(vale_anatomy::indices[t + k]);
    }
  }
  std::set_intersection(external.begin(), external.end(), interior.begin(),
                        interior.end(), std::inserter(source_lip, source_lip.end()));
  require(source_lip.size() == 38, "Exact 38-source-vertex anatomical lip seam");
  unsigned cursor = 0;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    if (material(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) !=
        int(CharacterSurface::Face)) continue;
    for (unsigned k = 0; k < 3; ++k, ++cursor)
      if (source_lip.count(vale_anatomy::indices[cursor]))
        runtime.insert(m.bind_mesh.indices[t + k]);
  }
  require(runtime.size() >= source_lip.size(), "Every source lip corner reaches runtime");
  return runtime;
}
float lip_attention_error(const CharacterModel& m) {
  const auto lip = lip_runtime_vertices(m);
  auto mesh = m.bind_mesh;
  float maximum = 0;
  for (float yaw : {-.9f, 0.f, .9f})
    for (float pitch : {-.4f, 0.f, .4f}) {
      CharacterAnimationSample sample;
      sample.seed = m.seed; sample.phase_cycles = .125; sample.idle_time = .125;
      sample.talk_weight = 1; sample.attention_weight = 1;
      sample.attention_yaw = yaw; sample.attention_pitch = pitch;
      const auto pose = sample_character_animation(m.rig, sample);
      apply_character_pose(m, pose, mesh);
      for (const auto vertex : lip) {
        const auto expected = transform_point(pose.skin_matrices[bone_index(CharacterBone::Head)],
                                               m.bind_mesh.vertices[vertex].position);
        maximum = std::max(maximum, length(mesh.vertices[vertex].position - expected));
      }
    }
  return maximum / m.rig.height;
}
void lip_attention_regression(const CharacterModel& m) {
  const float error = lip_attention_error(m);
  std::cout << "Exact lip attention maximum rigid-Head error/h " << error << '\n';
  require(error < 2e-6f, "All source lip corners stay rigid to Head in nine attention samples");
  auto broken = m;
  for (const auto vertex : lip_runtime_vertices(broken)) {
    const float y = broken.bind_mesh.vertices[vertex].position.y / broken.rig.height;
    const float t = std::clamp((y - .365f) / .026f, 0.f, 1.f);
    broken.influences[vertex] = {std::uint8_t(CharacterBone::Head),
                                std::uint8_t(CharacterBone::Neck), t * t * (3 - 2 * t)};
  }
  const float old_error = lip_attention_error(broken);
  std::cout << "Old Vale lip-weight negative control rigid-Head error/h " << old_error << '\n';
  require(old_error > .001f, "Regression detects the original lip-shear weighting");
}
void hand_source_contract(const CharacterModel&m){
 using K=std::array<float,15>;std::map<K,unsigned> actual;
 auto key=[](const std::array<Vec3,3>&p,const std::array<Vec2,3>&uv){K k{};for(unsigned i=0;i<3;++i){k[i*5]=p[i].x;k[i*5+1]=p[i].y;k[i*5+2]=p[i].z;k[i*5+3]=uv[i].x;k[i*5+4]=uv[i].y;}return k;};
 for(unsigned t=0;t<m.bind_mesh.indices.size();t+=3){if(material(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv)!=int(CharacterSurface::Skin))continue;std::array<Vec3,3>p;std::array<Vec2,3>u;for(int k=0;k<3;++k){auto&v=m.bind_mesh.vertices[m.bind_mesh.indices[t+k]];p[k]=v.position;u[k]=v.uv;}++actual[key(p,u)];}
 unsigned checked=0;
 for(float side:{-1.f,1.f}){
  auto position=[&](unsigned i){Vec3 p=vale_hands::positions[i];p.x*=side;const float angle=side*1.05f,c=std::cos(angle),s=std::sin(angle);return (Vec3{c*p.x+s*p.z,p.y,-s*p.x+c*p.z}+Vec3{side*.181f,-.044f,.018f})*m.rig.height;};
  auto check=[&](const auto&indices,const auto&uv_indices){for(unsigned t=0;t<indices.size();t+=3){std::array<Vec3,3>p;std::array<Vec2,3>u;for(unsigned k=0;k<3;++k){unsigned j=side>0?k:(k==0?0:3-k);p[k]=position(indices[t+j]);auto uv=vale_hands::uv_coordinates[uv_indices[t+j]];u[k]=character_surface_uv(CharacterSurface::Skin,uv.x,uv.y);}auto it=actual.find(key(p,u));require(it!=actual.end()&&it->second>0,"Every anatomical hand triangle keeps its position, winding and UV corner");--it->second;++checked;}};
  if(m.lod==CharacterLod::Near)check(vale_hands::near_indices,vale_hands::near_uv_indices);else check(vale_hands::far_indices,vale_hands::far_uv_indices);
  for(auto id:vale_hands::wrist_boundary_indices){Vec3 p=position(id);unsigned copies=0;Vec3 first{};for(const auto&v:m.bind_mesh.vertices)if(same(v.position,p,1e-7f*m.rig.height)){if(!copies)first=v.normal;else require(same(v.normal,first,2e-5f),"Wrist geometric seam shares exact smooth normal");++copies;}require(copies>=2,"Every exact anatomical wrist point is sewn into the forearm");}
 }
 std::cout<<"Exact anatomical hand triangles checked "<<checked<<"; complete wrist rings retained\n";
}
void sewn_topology(const CharacterModel&m,CharacterSurface surface,int expected_components,int expected_euler,bool prefix_only){
  using Key=std::tuple<float,float,float>;std::map<Key,unsigned> points;std::map<std::pair<unsigned,unsigned>,unsigned> edges;std::map<unsigned,std::set<unsigned>> adjacent;unsigned triangles=0;
  bool started=false;
  for(unsigned t=0;t<m.bind_mesh.indices.size();t+=3){int s=material(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv);if(s!=int(surface)){if(prefix_only&&started)break;continue;}started=true;unsigned out[3];
    for(int k=0;k<3;++k){auto p=m.bind_mesh.vertices[m.bind_mesh.indices[t+k]].position;Key key{p.x,p.y,p.z};auto found=points.find(key);if(found==points.end())found=points.emplace(key,points.size()).first;out[k]=found->second;}
    require(out[0]!=out[1]&&out[1]!=out[2]&&out[0]!=out[2],"Physical garment triangle has distinct vertices");++triangles;
    for(int k=0;k<3;++k){auto a=out[k],b=out[(k+1)%3];++edges[std::minmax(a,b)];adjacent[a].insert(b);adjacent[b].insert(a);}
  }
  unsigned boundary=0,nonmanifold=0;std::map<unsigned,std::set<unsigned>> border;
  for(auto e:edges){if(e.second==1){++boundary;border[e.first.first].insert(e.first.second);border[e.first.second].insert(e.first.first);}nonmanifold+=e.second>2;}
  unsigned components=0;std::set<unsigned> unseen;for(auto p:points)unseen.insert(p.second);while(!unseen.empty()){++components;std::vector<unsigned>todo{*unseen.begin()};unseen.erase(todo[0]);while(!todo.empty()){unsigned i=todo.back();todo.pop_back();for(auto j:adjacent[i])if(unseen.erase(j))todo.push_back(j);}}
  const int euler=int(points.size())-int(edges.size())+int(triangles);std::cout<<"Physical surface "<<int(surface)<<" components "<<components<<" Euler "<<euler<<" boundary edges "<<boundary<<" nonmanifold "<<nonmanifold<<'\n';
  require(nonmanifold==0,"No nonmanifold sewn garment edges");require(int(components)==expected_components&&euler==expected_euler,"Connected sewn garment topology");for(auto p:border)require(p.second.size()==2,"Garment opening is a simple closed loop");
}
// Cap brim is the only Leather surface above the anatomical brow. It must
// form one closed physical shell despite separate shading/UV corner indices.
void closed_brim(const CharacterModel& m) {
  using Point = std::tuple<float, float, float>;
  std::map<Point, unsigned> vertices;
  std::map<std::pair<unsigned, unsigned>, std::pair<unsigned, int>> edges;
  std::map<unsigned, std::set<unsigned>> neighbors;
  unsigned triangles = 0;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    const auto& first = m.bind_mesh.vertices[m.bind_mesh.indices[t]];
    if (material(first.uv) != int(CharacterSurface::Leather) ||
        first.position.y < .40f * m.rig.height) continue;
    unsigned corners[3];
    for (unsigned k = 0; k < 3; ++k) {
      const auto p = m.bind_mesh.vertices[m.bind_mesh.indices[t + k]].position;
      const Point key{p.x, p.y, p.z};
      auto found = vertices.find(key);
      if (found == vertices.end()) found = vertices.emplace(key, vertices.size()).first;
      corners[k] = found->second;
    }
    ++triangles;
    for (unsigned k = 0; k < 3; ++k) {
      const unsigned a = corners[k], b = corners[(k+1)%3];
      auto& edge = edges[std::minmax(a, b)];
      ++edge.first; edge.second += a < b ? 1 : -1;
      neighbors[a].insert(b); neighbors[b].insert(a);
    }
  }
  require(triangles > 0, "Brim shell exists");
  for (const auto& edge : edges)
    require(edge.second.first == 2 && edge.second.second == 0,
            "Brim is exactly closed and consistently oriented, including the buried root");
  std::set<unsigned> unseen; for (const auto& vertex : vertices) unseen.insert(vertex.second);
  unsigned components = 0;
  while (!unseen.empty()) {
    ++components; std::vector<unsigned> pending{*unseen.begin()}; unseen.erase(pending[0]);
    while (!pending.empty()) {
      auto point = pending.back(); pending.pop_back();
      for (const auto neighbor : neighbors[point]) if (unseen.erase(neighbor)) pending.push_back(neighbor);
    }
  }
  require(components == 1 && int(vertices.size()) - int(edges.size()) + int(triangles) == 2,
          "Brim is one closed genus-zero physical component");
  std::cout << "Closed brim triangles " << triangles << "; exact paired physical edges " << edges.size() << '\n';
}
void extremity_contacts(const CharacterModel&m,const Mesh&mesh,const std::string&label){
  auto hands=select(m,mesh,int(CharacterSurface::Skin),-.22f,-.010f),shirt=select(m,mesh,int(CharacterSurface::Shirt),-.04f,.28f),pants=select(m,mesh,int(CharacterSurface::Pants),-.5f,.08f),boots=select(m,mesh,int(CharacterSurface::Leather),-.501f,-.40f);
  unsigned hs=intersections(hands,shirt,m.rig.height),hp=intersections(hands,pants,m.rig.height),bp=intersections(boots,pants,m.rig.height);
  auto thigh=select(m,mesh,int(CharacterSurface::Pants),-.195f,-.085f),calf=select(m,mesh,int(CharacterSurface::Pants),-.440f,-.280f);unsigned knee=intersections(thigh,calf,m.rig.height);
  std::cout<<'\n'<<label<<" hands/cuff "<<hs<<" hands/trousers "<<hp<<" boots/trousers "<<bp<<" thigh/calf "<<knee<<'\n';require(hs==0&&hp==0&&bp==0&&knee==0,"Extremity garment contact: "+label);
}
void motion(const CharacterModel &m, bool contact) {
  auto mesh = m.bind_mesh;
  const auto *vertices = mesh.vertices.data();
  const auto *indices = mesh.indices.data();
  unsigned bad = 0;
  double ms = 0;
  for (int pose = 0; pose < 7; ++pose)
    for (int frame = 0; frame < 16; ++frame) {
      CharacterAnimationSample a;
      a.seed = m.seed;
      a.phase_cycles = frame / 16.;
      a.idle_time = frame / 16.;
      a.speed = pose == 1   ? 1.4f
                : pose == 2 ? 3.8f
                : pose == 4 ? m.rig.height * .55f
                            : 0;
      a.move_weight = (pose == 1 || pose == 2 || pose == 4) ? 1 : 0;
      a.crouch_weight = (pose == 3 || pose == 4) ? 1 : 0;
      a.talk_weight = pose >= 5 ? 1 : 0;
      a.attention_yaw = pose == 6 ? ((frame % 2) ? -.9f : .9f) : 0;
      a.attention_pitch = pose == 6 ? ((frame % 3) ? -.4f : .4f) : 0;
      a.attention_weight = pose == 6 ? 1 : 0;
      a.stride_length =
          character_stride_length(m.rig.height, a.speed, a.crouch_weight);
      const auto p = sample_character_animation(m.rig, a);
      auto begin = std::chrono::steady_clock::now();
      apply_character_pose(m, p, mesh);
      ms += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin)
                .count();
      require(mesh.vertices.data() == vertices &&
                  mesh.indices.data() == indices,
              "Stable per-frame mesh storage");
      for (unsigned i = 0; i < mesh.vertices.size(); ++i) {
        const auto &v = mesh.vertices[i], &b = m.bind_mesh.vertices[i];
        bad += !finite(v.position) || !finite(v.normal) ||
               std::fabs(length(v.normal) - 1) > .001f;
        require(same(v.color, b.color) && v.uv.x == b.uv.x && v.uv.y == b.uv.y,
                "Pose preserves materials");
      }
      if(contact){const auto label="pose"+std::to_string(pose)+"-"+std::to_string(frame);contacts(m,mesh,label);extremity_contacts(m,mesh,label);}
    }
  require(bad == 0, "Finite unit geometry in 112 posed samples");
  std::cout << "Pose samples 112 mean_ms " << ms / 112 << '\n';
  apply_character_pose(m, CharacterPose{}, mesh);
  for (unsigned i = 0; i < mesh.vertices.size(); ++i)
    require(same(mesh.vertices[i].position, m.bind_mesh.vertices[i].position,
                 2e-6f * m.rig.height),
            "Exact bind reset");
}
void dump(const CharacterModel &m, std::string path) {
  std::ofstream out(path, std::ios::binary);
  std::uint32_t nv = m.bind_mesh.vertices.size(),
                ni = m.bind_mesh.indices.size();
  out.write((char *)&nv, 4);
  out.write((char *)&ni, 4);
  out.write((char *)&m.rig.height, 4);
  for (unsigned i = 0; i < nv; ++i) {
    const auto &v = m.bind_mesh.vertices[i];
    int s = material(v.uv);
    float data[]{v.position.x, v.position.y, v.position.z, v.normal.x,
                 v.normal.y,   v.normal.z,   v.uv.x,       v.uv.y,
                 v.color.x,    v.color.y,    v.color.z,    float(s)};
    out.write((char *)data, sizeof(data));
    const auto w = m.influences[i];
    float weights[]{float(w.first), float(w.second), w.first_weight,
                    float(w.third), w.third_weight};
    out.write((char *)weights, sizeof(weights));
  }
  out.write((char *)m.bind_mesh.indices.data(), ni * 4);
  require(bool(out), "Mesh dump written");
}
} // namespace
int main(int argc, char **argv) {
  try {
    bool contact = false;
    std::string prefix;
    for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--contacts")
        contact = true;
      else if (a == "--dump" && i + 1 < argc)
        prefix = argv[++i];
      else
        throw std::runtime_error("Unknown test option");
    }
    for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
      auto p = make_character_profile(1.86f, CharacterRole::Security,
                                      "NpcAlleyHmpd", lod);
      if (!prefix.empty())
        dump(p.model,
             prefix + (lod == CharacterLod::Near ? "-near.bin" : "-far.bin"));
      geometry(p);source(p.model);eyes(p.model);lip_attention_regression(p.model);hand_source_contract(p.model);closed_brim(p.model);
      sewn_topology(p.model,CharacterSurface::Shirt,1,-2,true);
      sewn_topology(p.model,CharacterSurface::Pants,1,-1,false);
      if (contact)
        contacts(p.model, p.model.bind_mesh, "bind");
      motion(p.model, contact);
    }
    for (auto id :
         {"npcalleyhmpd", "NpcAlleyHmpd_extra", "unregistered_security"})
      require(!make_character_profile(1.86f, CharacterRole::Security, id,
                                      CharacterLod::Far)
                   .individualized,
              "Exact identity dispatch");
    require(!make_character_profile(1.86f, CharacterRole::Commuter,
                                    "NpcAlleyHmpd", CharacterLod::Far)
                 .individualized,
            "Exact role dispatch");
    for (float height : {0.f, -1.f, std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::infinity()}) {
      bool threw = false;
      try {
        make_character_profile(height, CharacterRole::Security, "NpcAlleyHmpd",
                               CharacterLod::Near);
      } catch (const std::invalid_argument &) {
        threw = true;
      }
      require(threw, "Invalid height rejected");
    }
    std::cout << "Vale anatomy/model/motion checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Vale validation: " << e.what() << '\n';
    return 1;
  }
}
