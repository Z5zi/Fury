#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/enforcer_anatomy.hpp"
#include "fury/generated/enforcer_hands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
using namespace fury;
void require(bool value, const char *why) {
  if (!value)
    throw std::runtime_error(why);
}
void require(bool value, const std::string &why) {
  if (!value)
    throw std::runtime_error(why);
}
std::string label(const CharacterModel &m) {
  return (m.lod == CharacterLod::Near ? "Near" : "Far") + std::string(" h=") +
         std::to_string(m.rig.height);
}
bool finite(Vec3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
int surface(Vec2 uv) {
  for (int i = 0; i < int(CharacterSurface::Count); ++i) {
    const auto &r = character_surface_region(CharacterSurface(i));
    const float s = float(character_surface_atlas_size);
    if (uv.x >= r.x / s && uv.x < (r.x + r.size) / s && uv.y >= r.y / s &&
        uv.y < (r.y + r.size) / s)
      return i;
  }
  return -1;
}
bool near(Vec3 a, Vec3 b, float tol = 1e-6f) { return length(a - b) <= tol; }
void mesh_check(const CharacterModel &m, const Mesh &mesh, bool bind) {
  require(mesh.vertices.size() == m.influences.size(),
          "Influence count mismatch");
  require(mesh.indices.size() % 3 == 0, "Incomplete indexed triangle");
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    const auto &v = mesh.vertices[i];
    const auto &w = m.influences[i];
    if (!(finite(v.position) && finite(v.normal) && finite(v.color)))
      throw std::runtime_error("Nonfinite vertex " + std::to_string(i));
    const float normal = length(v.normal);
    if (!(normal > .98f && normal < 1.02f))
      throw std::runtime_error("Invalid normal " + std::to_string(i) +
                               " magnitude " + std::to_string(normal));
    require(surface(v.uv) >= 0, "Vertex outside supported surface tile");
    require(w.first < character_bone_count && w.second < character_bone_count &&
                w.third < character_bone_count,
            "Invalid bone");
    require(w.first_weight >= 0 && w.third_weight >= 0 &&
                w.first_weight + w.third_weight <= 1.00001f,
            "Invalid normalized weight");
  }
  float minimum = 1.f;
  std::size_t reversed = 0;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    const auto a = mesh.indices[i], b = mesh.indices[i + 1],
               c = mesh.indices[i + 2];
    require(a < mesh.vertices.size() && b < mesh.vertices.size() &&
                c < mesh.vertices.size(),
            "Out of range triangle");
    const auto &A = mesh.vertices[a], &B = mesh.vertices[b],
               &C = mesh.vertices[c];
    require(surface(A.uv) == surface(B.uv) && surface(A.uv) == surface(C.uv),
            "Triangle crosses material tile");
    const Vec3 cross_normal =
        cross(B.position - A.position, C.position - A.position);
    const float area =
        dot(cross_normal, cross_normal) /
        (m.rig.height * m.rig.height * m.rig.height * m.rig.height);
    minimum = std::min(minimum, area);
    if (!(area > 1e-24f))
      throw std::runtime_error("Degenerate triangle " + std::to_string(i / 3));
    if (dot(cross_normal, A.normal + B.normal + C.normal) <= 0) {
      if (bind && m.rig.height == 1.95f)
        std::cout
            << "opposed triangle=" << i / 3 << " surface=" << surface(A.uv)
            << " center/h="
            << (A.position.x + B.position.x + C.position.x) / (3 * m.rig.height)
            << ","
            << (A.position.y + B.position.y + C.position.y) / (3 * m.rig.height)
            << ","
            << (A.position.z + B.position.z + C.position.z) / (3 * m.rig.height)
            << '\n';
      ++reversed;
    }
  }
  if (bind)
    require(reversed == 0, "Bind triangle opposes its smoothed vertex normals");
  if (bind)
    std::cout << "bind triangles=" << mesh.indices.size() / 3
              << " minimum cross squared/h4=" << minimum
              << " opposed smooth normals=" << reversed << '\n';
}
void rendered_role_palette(const CharacterModel& m) {
  const Vec3 shirt{.16f,.22f,.28f},pants{.095f,.13f,.175f},sole{.055f,.065f,.075f};
  unsigned shirts=0,trousers=0,soles=0;
  for(const auto& v:m.bind_mesh.vertices) {
    const int tile=surface(v.uv);const float y=v.position.y/m.rig.height;
    if(tile==int(CharacterSurface::Shirt)&&y>-.10f) {
      require(length(cross(v.color,shirt))<1e-7f,"Rendered shirt preserves original Enforcer role-color hue");
      require(v.color.x/shirt.x>.65f&&v.color.x/shirt.x<=1.00001f,"Cloth color variation remains bounded around original role color");
      shirts+=near(v.color,shirt,1e-7f);
    }
    if(tile==int(CharacterSurface::Pants)) {
      require(length(cross(v.color,pants))<1e-7f,"Rendered trousers preserve original Enforcer role-color hue");
      trousers+=near(v.color,pants,1e-7f);
    }
    if(tile==int(CharacterSurface::Rubber)&&y<-.38f){require(near(v.color,sole,1e-7f),"Boot soles retain original dark role color");++soles;}
  }
  require(shirts>100&&trousers>100&&soles>20,"Actual rendered role palette is populated in all principal surfaces");
}

void source_head(const CharacterModel &m) {
  std::set<unsigned> rigid_source, neck_source, join;
  for(unsigned t=0;t<enforcer_anatomy::indices.size();t+=3) {
    const auto uv=enforcer_anatomy::uv_coordinates[enforcer_anatomy::uv_indices[t]];
    auto &set=uv.y>.8406f?neck_source:rigid_source;
    for(int k=0;k<3;++k)set.insert(enforcer_anatomy::indices[t+k]);
  }
  std::set_intersection(rigid_source.begin(),rigid_source.end(),neck_source.begin(),neck_source.end(),std::inserter(join,join.end()));
  require(join.size()==48,"Exact original 48-vertex jaw/neck seam is fully rigid");
  std::vector<unsigned> triangles;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3)
    if (surface(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) ==
        int(CharacterSurface::Face))
      triangles.push_back(t);
  require(triangles.size() == enforcer_anatomy::indices.size() / 3,
          "Dropped or added source Face triangles");
  for (unsigned i = 0; i < triangles.size(); ++i)
    for (int k = 0; k < 3; ++k) {
      const auto &v =
          m.bind_mesh.vertices[m.bind_mesh.indices[triangles[i] + k]];
      const auto expected =
          enforcer_anatomy::positions[enforcer_anatomy::indices[3 * i +
                                                                      k]] *
          m.rig.height;
      require(near(v.position, expected, 2e-7f),
              "Source anatomical position or corner order changed");
      const auto source_uv = enforcer_anatomy::uv_coordinates
          [enforcer_anatomy::uv_indices[3 * i + k]];
      const auto expected_uv = character_surface_uv(CharacterSurface::Face,
                                                    source_uv.x, source_uv.y);
      if(rigid_source.count(enforcer_anatomy::indices[3*i+k])) {
        const auto w=m.influences[m.bind_mesh.indices[triangles[i]+k]];
        require(w.first==bone_index(CharacterBone::Head)&&w.first_weight==1.f&&w.third_weight==0.f,
          "Every external head, jaw, lip, socket and shared neck-seam vertex is rigid to Head");
      }
      require(v.uv.x == expected_uv.x && v.uv.y == expected_uv.y,
              "Canonical per-corner Face UV changed");
    }
  // The connected lower neck underlay must meet every imported attachment
  // point.
  for (auto source : enforcer_anatomy::attachment_boundary_indices) {
    const Vec3 expected = enforcer_anatomy::positions[source] * m.rig.height;
    int matches = 0;
    for (const auto &v : m.bind_mesh.vertices)
      if (surface(v.uv) == int(CharacterSurface::Skin) &&
          near(v.position, expected, 3e-7f))
        ++matches;
    require(matches >= 1, "Missing exact connected neck attachment");
  }
}
std::array<std::vector<Vec3>, 2> source_eye_openings() {
  std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> charts;
  for (std::size_t i = 0; i < enforcer_anatomy::indices.size(); i += 3) {
    const auto uv =
        enforcer_anatomy::uv_coordinates[enforcer_anatomy::uv_indices[i]];
    const unsigned chart = uv.x < .08f ? 2 : uv.x < .16f ? 1 : 0;
    for (int c = 0; c < 3; ++c) {
      auto a = enforcer_anatomy::indices[i + c],
           b = enforcer_anatomy::indices[i + (c + 1) % 3];
      if (a > b)
        std::swap(a, b);
      charts[{a, b}] |= 1u << chart;
    }
  }
  std::array<std::set<std::uint32_t>, 2> corners;
  for (const auto &e : charts)
    for (unsigned side = 0; side < 2; ++side)
      if (e.second == (1u | (1u << (side + 1)))) {
        corners[side].insert(e.first.first);
        corners[side].insert(e.first.second);
      }
  std::array<std::vector<Vec3>, 2> result;
  for (unsigned side = 0; side < 2; ++side) {
    require(corners[side].size() >= 20,
            "Both independent socket-to-skin seams are recovered");
    for (auto i : corners[side])
      result[side].push_back(enforcer_anatomy::positions[i]);
  }
  return result;
}

float projected_front(Vec3 p, const Mesh &mesh,
                      const std::vector<std::size_t> &triangles) {
  float front = -std::numeric_limits<float>::infinity();
  for (auto t : triangles) {
    const auto a = mesh.vertices[mesh.indices[t]].position;
    const auto b = mesh.vertices[mesh.indices[t + 1]].position;
    const auto c = mesh.vertices[mesh.indices[t + 2]].position;
    const float d = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(d) < 1e-14f)
      continue;
    const float u = ((p.x - a.x) * (c.y - a.y) - (p.y - a.y) * (c.x - a.x)) / d;
    const float v = ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) / d;
    if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
      front = std::max(front, a.z + u * (b.z - a.z) + v * (c.z - a.z));
  }
  return front;
}

void check_socket_fit(const CharacterModel &model) {
  const auto openings = source_eye_openings();
  std::vector<std::size_t> eyes, face;
  for (std::size_t t = 0; t < model.bind_mesh.indices.size(); t += 3) {
    const int material =
        surface(model.bind_mesh.vertices[model.bind_mesh.indices[t]].uv);
    if (material == int(CharacterSurface::Eyes))
      eyes.push_back(t);
    if (material == int(CharacterSurface::Face))
      face.push_back(t);
  }
  float nearest_gap = std::numeric_limits<float>::infinity(),
        largest_protrusion = -nearest_gap;
  for (const auto &ring : openings) {
    Vec3 center{};
    for (auto p : ring)
      center += p;
    center = center * (1.f / ring.size());
    // Every rim point and inward spoke has real globe/iris geometry behind it.
    for (auto rim : ring)
      for (float fraction : {0.f, .25f, .5f, .75f, 1.f}) {
        const auto p =
            (center * (1 - fraction) + rim * fraction) * model.rig.height;
        const float front = projected_front(p, model.bind_mesh, eyes);
        require(std::isfinite(front),
                label(model) +
                    "eye mesh covers every socket rim and aperture spoke");
        if (fraction < 1) {
          const float skin_front = projected_front(p, model.bind_mesh, face);
          require(front > skin_front + model.rig.height * .0001f,
                  label(model) +
                      "eye remains visible through the actual socket aperture");
        }
        if (fraction == 1) {
          const float delta = (front - p.z) / model.rig.height;
          nearest_gap = std::min(nearest_gap, delta);
          largest_protrusion = std::max(largest_protrusion, delta);
        }
      }
  }
  std::cout << label(model) << ": actual socket-rim eye depth/h " << nearest_gap
            << " .. " << largest_protrusion << '\n';
  require(nearest_gap > -.003f && largest_protrusion < .002f,
          label(model) + " actual eye rim range/h " +
              std::to_string(nearest_gap) + ".." +
              std::to_string(largest_protrusion) + " exceeds [-.003,.002]");
}

// Derive the lip seam from the exact canonical charts, rather than a broad
// coordinate box or a helper joint that lives inside the mouth.
std::set<unsigned> lip_runtime_vertices(const CharacterModel& m) {
  std::set<unsigned> external, interior, source_lip, runtime;
  for (unsigned t = 0; t < enforcer_anatomy::indices.size(); t += 3) {
    const auto uv = enforcer_anatomy::uv_coordinates[enforcer_anatomy::uv_indices[t]];
    const bool head = uv.x > .64f && uv.y >= .14869f && uv.y <= .81764f;
    const bool mouth = uv.x > .75f && uv.x < .93f && uv.y < .13645f;
    for (unsigned k = 0; k < 3; ++k) {
      if (head) external.insert(enforcer_anatomy::indices[t + k]);
      if (mouth) interior.insert(enforcer_anatomy::indices[t + k]);
    }
  }
  std::set_intersection(external.begin(), external.end(), interior.begin(),
                        interior.end(), std::inserter(source_lip, source_lip.end()));
  require(source_lip.size() == 38, "Exact 38-source-vertex anatomical lip seam");
  unsigned cursor = 0;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    if (surface(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) !=
        int(CharacterSurface::Face)) continue;
    for (unsigned k = 0; k < 3; ++k, ++cursor)
      if (source_lip.count(enforcer_anatomy::indices[cursor]))
        runtime.insert(m.bind_mesh.indices[t + k]);
  }
  require(runtime.size() >= source_lip.size(), "Every source lip corner reaches runtime");
  return runtime;
}
std::set<unsigned> protected_head_runtime_vertices(const CharacterModel& m) {
  // Recover ownership independently from canonical source chart coordinates.
  // Joining all non-neck charts includes the entire jaw and both copies of
  // the true neck attachment seam, regardless of their height.
  std::set<unsigned> source, runtime;
  for(unsigned t=0;t<enforcer_anatomy::indices.size();t+=3) {
    const auto uv=enforcer_anatomy::uv_coordinates[enforcer_anatomy::uv_indices[t]];
    const bool neck=uv.x>=.2151319f&&uv.x<=.5459291f&&uv.y>=.8406229f&&uv.y<=.9738561f;
    if(!neck)for(int k=0;k<3;++k)source.insert(enforcer_anatomy::indices[t+k]);
  }
  unsigned cursor=0;
  for(unsigned t=0;t<m.bind_mesh.indices.size();t+=3) {
    if(surface(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv)!=int(CharacterSurface::Face))continue;
    for(int k=0;k<3;++k,++cursor)if(source.count(enforcer_anatomy::indices[cursor]))runtime.insert(m.bind_mesh.indices[t+k]);
  }
  require(runtime.size()>4000,"Complete source-owned rigid head region reaches runtime");
  return runtime;
}
float rigid_attention_error(const CharacterModel& m,const std::set<unsigned>& vertices) {
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
      for (const auto vertex : vertices) {
        const auto expected = transform_point(pose.skin_matrices[bone_index(CharacterBone::Head)],
                                               m.bind_mesh.vertices[vertex].position);
        maximum = std::max(maximum, length(mesh.vertices[vertex].position - expected));
      }
    }
  return maximum / m.rig.height;
}
float lip_attention_error(const CharacterModel& m) {
  return rigid_attention_error(m,lip_runtime_vertices(m));
}
void lip_attention_regression(const CharacterModel& m) {
  const float error = lip_attention_error(m);
  std::cout << "Exact lip attention maximum rigid-Head error/h " << error << '\n';
  require(error < 2e-6f, "All source lip corners stay rigid to Head in nine attention samples");
  auto broken = m;
  for (const auto vertex : lip_runtime_vertices(broken)) {
    const float y = broken.bind_mesh.vertices[vertex].position.y / broken.rig.height;
    const float t = std::clamp((y - .355f) / .044f, 0.f, 1.f);
    broken.influences[vertex] = {std::uint8_t(CharacterBone::Head),
                                std::uint8_t(CharacterBone::Neck), t * t * (3 - 2 * t)};
  }
  const float old_error = lip_attention_error(broken);
  std::cout << "Historical Jon lip-weight negative control rigid-Head error/h " << old_error << '\n';
  require(old_error > .001f, "Regression detects the historical neck-to-lip shear weighting applied to this source");
  const auto protected_vertices=protected_head_runtime_vertices(m);
  const float protected_error=rigid_attention_error(m,protected_vertices);
  require(protected_error<2e-6f,"Every source-owned jaw, mouth, socket and head corner stays rigid during extreme attention");
  const auto low_jaw=*std::min_element(protected_vertices.begin(),protected_vertices.end(),[&](auto a,auto b){return m.bind_mesh.vertices[a].position.y<m.bind_mesh.vertices[b].position.y;});
  for(auto corrupt: {low_jaw,*lip_runtime_vertices(m).begin()}) {
    auto damaged=m;
    damaged.influences[corrupt]={std::uint8_t(CharacterBone::Neck),std::uint8_t(CharacterBone::Neck),1.f};
    const float damage=rigid_attention_error(damaged,protected_vertices);
    std::cout<<"Protected source-vertex Neck corruption control "<<corrupt<<" error/h "<<damage<<'\n';
    require(damage>.001f,"Independent rigid-head test misses a protected low-jaw or mouth vertex assigned to Neck");
  }
}

void hand_source_contract(const CharacterModel&m){
 using K=std::array<float,15>;std::map<K,unsigned> actual;
 auto key=[](const std::array<Vec3,3>&p,const std::array<Vec2,3>&uv){K k{};for(unsigned i=0;i<3;++i){k[i*5]=p[i].x;k[i*5+1]=p[i].y;k[i*5+2]=p[i].z;k[i*5+3]=uv[i].x;k[i*5+4]=uv[i].y;}return k;};
 for(unsigned t=0;t<m.bind_mesh.indices.size();t+=3){if(surface(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv)!=int(CharacterSurface::Skin))continue;std::array<Vec3,3>p;std::array<Vec2,3>u;for(int k=0;k<3;++k){auto&v=m.bind_mesh.vertices[m.bind_mesh.indices[t+k]];p[k]=v.position;u[k]=v.uv;}++actual[key(p,u)];}
 unsigned checked=0;
 for(float side:{-1.f,1.f}){
  auto position=[&](unsigned i){Vec3 p=enforcer_hands::positions[i];p.x*=side;const float angle=side*1.16f,c=std::cos(angle),s=std::sin(angle);return (Vec3{c*p.x+s*p.z,p.y,-s*p.x+c*p.z}+Vec3{side*.184f,-.043f,.018f})*m.rig.height;};
  auto check=[&](const auto&indices,const auto&uv_indices){for(unsigned t=0;t<indices.size();t+=3){std::array<Vec3,3>p;std::array<Vec2,3>u;for(unsigned k=0;k<3;++k){unsigned j=side>0?k:(k==0?0:3-k);p[k]=position(indices[t+j]);auto uv=enforcer_hands::uv_coordinates[uv_indices[t+j]];u[k]=character_surface_uv(CharacterSurface::Skin,uv.x,uv.y);}auto it=actual.find(key(p,u));require(it!=actual.end()&&it->second>0,"Every anatomical hand triangle keeps its position, winding and UV corner");--it->second;++checked;}};
  if(m.lod==CharacterLod::Near)check(enforcer_hands::near_indices,enforcer_hands::near_uv_indices);else check(enforcer_hands::far_indices,enforcer_hands::far_uv_indices);
  for(auto id:enforcer_hands::wrist_boundary_indices){Vec3 p=position(id);unsigned copies=0;Vec3 first{};for(const auto&v:m.bind_mesh.vertices)if(near(v.position,p,1e-7f*m.rig.height)){if(!copies)first=v.normal;else require(near(v.normal,first,2e-5f),"Wrist geometric seam shares exact smooth normal");++copies;}require(copies>=2,"Every exact anatomical wrist point is sewn into the forearm");}
 }
 std::cout<<"Exact anatomical hand triangles checked "<<checked<<"; complete wrist rings retained\n";
}

void source_hand_topology() {
  for (bool far : {false, true}) {
    std::map<std::pair<unsigned, unsigned>, unsigned> edges;
    std::map<unsigned, std::set<unsigned>> adj;
    const auto check = [&](auto const &indices) {
      for (unsigned i = 0; i < indices.size(); i += 3)
        for (int k = 0; k < 3; ++k) {
          unsigned a = indices[i + k], b = indices[i + (k + 1) % 3];
          require(a != b, "Repeated hand index");
          adj[a].insert(b);
          adj[b].insert(a);
          if (a > b)
            std::swap(a, b);
          ++edges[{a, b}];
        }
    };
    if (far)
      check(enforcer_hands::far_indices);
    else
      check(enforcer_hands::near_indices);
    unsigned boundary = 0;
    for (auto const &e : edges) {
      require(e.second <= 2, "Nonmanifold hand");
      boundary += e.second == 1;
    }
    require(boundary == 20, "Hand wrist boundary changed");
    std::set<unsigned> seen;
    std::vector<unsigned> stack{adj.begin()->first};
    while (!stack.empty()) {
      auto v = stack.back();
      stack.pop_back();
      if (!seen.insert(v).second)
        continue;
      for (auto n : adj[v])
        stack.push_back(n);
    }
    require(seen.size() == adj.size(), "Hand has disconnected fingers/palm");
    const std::size_t t = far ? enforcer_hands::far_indices.size() / 3
                              : enforcer_hands::near_indices.size() / 3;
    require(int(adj.size()) - int(edges.size()) + int(t) == 1,
            "Hand topology has extra hole");
  }
}
#include "character_enforcer_contacts.hpp"

void poses(const CharacterModel &m, bool ordinary_only) {
  Mesh posed = m.bind_mesh;
  const auto *vptr = posed.vertices.data();
  const auto *iptr = posed.indices.data();
  const auto indices = posed.indices;
  unsigned samples = 0;
  EnforcerContacts contacts(m);
  contacts.check(m.bind_mesh, label(m) + " bind");
  for (int mode = 0; mode < 12; ++mode)
    for (int step = 0; step < 24; ++step) {
      if (ordinary_only && (mode == 4 || mode == 9))
        continue;
      CharacterAnimationSample s;
      s.seed = m.seed;
      s.idle_time = step / 12.;
      s.phase_cycles = step / 24.;
      if (mode == 1 || mode == 2 || mode == 4) {
        s.speed = mode == 2 ? 5.6f : mode == 4 ? 1.f : 2.4f;
        s.move_weight = 1;
      }
      if (mode == 3 || mode == 4)
        s.crouch_weight = 1;
      if (mode == 5)
        s.talk_weight = 1;
      if (mode == 6) {
        s.attention_weight = 1;
        s.attention_yaw = step % 2 ? .9f : -.9f;
        s.attention_pitch = step % 3 ? .4f : -.4f;
      }
      if (mode >= 7 && mode <= 9) {
        s.speed = m.rig.height * (mode == 7 ? .85f : mode == 8 ? 2.6f : .55f);
        s.move_weight = 1;
        if (mode == 9)
          s.crouch_weight = 1;
      }
      if (mode == 10 || mode == 11) {
        s.attention_weight = 1;
        s.attention_yaw = step % 2 ? .9f : -.9f;
        s.attention_pitch = step % 3 ? .4f : -.4f;
        if (mode == 10)
          s.talk_weight = 1;
        else
          s.crouch_weight = 1;
      }
      auto pose = sample_character_animation(m.rig, s);
      apply_character_pose(m, pose, posed);
      mesh_check(m, posed, false);
      for (unsigned i = 0; i < posed.vertices.size(); ++i) {
        const auto bone = m.influences[i].first;
        if (bone == bone_index(CharacterBone::LeftFoot) ||
            bone == bone_index(CharacterBone::RightFoot))
          require(posed.vertices[i].position.y >= -.5001f * m.rig.height,
                  "Actual boot sole penetrates ground");
        require(
            near(posed.vertices[i].color, m.bind_mesh.vertices[i].color, 0.f),
            "Pose changed exact vertex palette");
      }
      contacts.check(posed, label(m) + " mode=" + std::to_string(mode) +
                                " phase=" + std::to_string(step));
      require(posed.vertices.data() == vptr && posed.indices.data() == iptr,
              "Pose changed storage allocation");
      require(posed.indices == indices, "Pose changed indexed geometry");
      ++samples;
    }
  std::cout << "pose samples=" << samples
            << " stable mesh buffers/indices, finite surfaces, all "
               "garment/wrist/boot contacts\n";
}
void exact_height(const CharacterModel &m) {
  float low = 100.f, high = -100.f;
  for (auto const &v : m.bind_mesh.vertices) {
    low = std::min(low, v.position.y);
    high = std::max(high, v.position.y);
  }
  require(std::fabs(low + m.rig.height * .5f) < 1e-6f,
          "Soles changed actor floor");
  require(std::fabs(high - m.rig.height * .5f) < 1e-6f,
          "Hair changed actor ceiling");
  std::cout << "height=" << m.rig.height
            << " actual vertical extent=" << high - low << '\n';
}
} // namespace
int main(int argc, char **argv) {
  try {
    const bool ordinary_only =
        argc == 2 && std::strcmp(argv[1], "--ordinary") == 0;
    require(argc == 1 || ordinary_only,
            "Expected no arguments for full stress checks, or --ordinary");
    source_hand_topology();
    enforcer_intersection_controls();
    auto texture = character_surface_textures();
    for (float h : {1.95f, 1.35f, 2.10f})
      for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
        auto p = make_character_profile(h, CharacterRole::Enforcer,
                                        "NpcEnforcer", lod);
        auto &m = p.model;
        require(p.individualized, "Syndicate Enforcer did not select own profile");
        require(m.bind_mesh.indices.size() / 3 <=
                    (lod == CharacterLod::Near ? 40000u : 16000u),
                "Syndicate Enforcer exceeds established 40000 Near /16000 Far "
                "triangle cap: " +
                    std::to_string(m.bind_mesh.indices.size() / 3));
        require(m.seed == 0xbb361ac4u, "Stable seed changed");
        require(m.role == CharacterRole::Enforcer && m.rig.height == h,
                "Canonical role/height changed");
        require(m.dual_quaternion_skinning,
                "Missing individual rigid skinning");
        require(p.material.textures == texture,
                "Profile duplicates shared immutable texture atlas");
        const auto original = character_appearance(m.seed);
        require(near(m.appearance.skin, original.skin) &&
                    near(m.appearance.hair, original.hair) && near(m.appearance.shirt,original.shirt) &&
                    near(m.appearance.trousers,original.trousers) && near(m.appearance.shoes,original.shoes) &&
                    near(m.appearance.accent,original.accent),
                "Original skin/hair palette changed");
        mesh_check(m, m.bind_mesh, true);
        rendered_role_palette(m);
        exact_height(m);
        enforcer_closed_layers(m);
        enforcer_scalp_coverage(m);
        source_head(m);
        hand_source_contract(m);
        lip_attention_regression(m);
        check_socket_fit(m);
        poses(m, ordinary_only);
        if (h == 1.95f)
          enforcer_contact_classification_controls(m);
      }
    for (auto id :
         {"NpcEnforcer_extra", "npcenforcer", "unknown_security_actor", "UnrelatedGuard"}) {
      auto p = make_character_profile(1.95f, CharacterRole::Enforcer, id,
                                      CharacterLod::Near);
      require(!p.individualized,
              "Syndicate Enforcer captured an unregistered or non-exact identity");
    }
    auto wrong = make_character_profile(1.95f, CharacterRole::Market,
                                        "NpcEnforcer", CharacterLod::Near);
    require(!wrong.individualized, "Profile matches wrong appearance role");
    std::cout
        << "PASS Syndicate Enforcer source, topology, height, identity, "
           "palettes, LOD, "
        << (ordinary_only ? 1440 : 1728)
        << " pose/contact samples and isolation"
        << (ordinary_only
                ? " (full crouch-walk remains a separate strict failure gate)"
                : "")
        << '\n';
    return 0;
  } catch (std::exception const &e) {
    std::cerr << "FAIL Syndicate Enforcer: " << e.what() << '\n';
    return 1;
  }
}
