#include "../apps/vaultline/harbor_assets.hpp"
#include "../apps/vaultline/meridian_wishlist.hpp"
#include "../apps/vaultline/npc_interaction.hpp"
#include "../apps/vaultline/npc_presentation.hpp"
#include "../apps/vaultline/npc_roster.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
const Vec3 kTranslation{0.f, 0.f, -1.5f};
const Vec3 kLobbyListener{0.f, 1.7f, -6.5f};
void require(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}
bool same(Vec3 a, Vec3 b, float tolerance = 1e-5f) {
  return length(a - b) <= tolerance;
}
bool same_route(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) if (!same(a[i], b[i])) return false;
  return true;
}
NpcSystem roster_system() {
  NpcSystem result;
  for (auto& spec : vaultline::make_npc_roster()) result.add(std::move(spec.agent));
  result.apply_schedules(true);
  return result;
}
NpcAgent& teller(NpcSystem& npcs) {
  for (auto& a : npcs.agents()) if (a.entity_name == "NpcTeller") return a;
  throw std::runtime_error("Production roster lacks NpcTeller");
}
struct Bounds {
  Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
  void add(Vec3 p) {
    lo = {std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
    hi = {std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
  }
};
Bounds mesh_bounds(const Mesh& mesh, const Mat4& transform) {
  Bounds result;
  for (const auto& v : mesh.vertices) result.add(transform_point(transform,v.position));
  return result;
}
// Positive separation on any world axis rigorously excludes all triangle
// contacts. Overlapping bounds fail this conservative placement regression;
// the test never treats a non-solid mesh's default collider as its geometry.
float separation(const Bounds& a, const Bounds& b) {
  return std::max({b.lo.x-a.hi.x,a.lo.x-b.hi.x,b.lo.y-a.hi.y,
                   a.lo.y-b.hi.y,b.lo.z-a.hi.z,a.lo.z-b.hi.z});
}
float horizontal_clearance(Vec3 p, const Bounds& b, float radius) {
  const float dx=std::max({b.lo.x-p.x,0.f,p.x-b.hi.x});
  const float dz=std::max({b.lo.z-p.z,0.f,p.z-b.hi.z});
  return std::hypot(dx,dz)-radius;
}
struct Obstacle { std::string name; Bounds bounds; };
struct Bank {
  Scene scene;
  std::vector<Obstacle> obstacles;
  Mesh worktop;
  Mat4 worktop_transform;
  meridian::WishlistController mission;
};
void load_real_bank(Bank& bank) {
  harbor::spawn_meridian_mutual(bank.scene);
  bank.mission.spawn(bank.scene);
  std::size_t teller_parts=0;
  Mat4 teller_transform;
  for (const auto& e : bank.scene.entities()) {
    if (!e.mesh) continue;
    if (e.name.rfind("MMTeller_",0)==0) {
      ++teller_parts;
      teller_transform=e.transform.matrix();
      require(!e.solid,"Counter audit must exercise actual non-solid visual meshes");
    }
    const auto b=mesh_bounds(*e.mesh,e.transform.matrix());
    // Exclude the intended walking floor/low route pads and overhead fixtures.
    if (e.visible && b.hi.y>.2f && b.lo.y<1.9f)
      bank.obstacles.push_back({e.name,b});
  }
  require(teller_parts==72,"All shipped teller primitives must load; no fallback counter");
  const auto* asset=harbor::find_asset("bank_teller_counter");
  std::string path,error; GltfAsset gltf;
  require(asset && harbor::resolve_mesh_path(asset->glb,path) && load_gltf(path,gltf,error),
          "Load actual teller GLB through the production importer: "+error);
  bool found=false;
  for (const auto& p : gltf.primitives) if (p.name=="TC_Top" && p.mesh) {
    bank.worktop=*p.mesh;
    bank.worktop_transform=teller_transform*p.transform;
    const auto source=mesh_bounds(bank.worktop,bank.worktop_transform);
    bool runtime_match=false;
    for (const auto& e : bank.scene.entities()) if (e.name.rfind("MMTeller_",0)==0 && e.mesh) {
      const auto actual=mesh_bounds(*e.mesh,e.transform.matrix());
      runtime_match |= same(source.lo,actual.lo) && same(source.hi,actual.hi) &&
                       bank.worktop.indices.size()==e.mesh->indices.size();
    }
    require(runtime_match,"TC_Top node transform must match a placed runtime mesh");
    found=true;
  }
  require(found && !bank.obstacles.empty(),"Actual worktop and bank surroundings are required");
}
bool edge_crosses_triangle(Vec3 p, Vec3 q, Vec3 a, Vec3 b, Vec3 c) {
  const auto d=q-p, e1=b-a, e2=c-a, h=cross(d,e2);
  const float det=dot(e1,h);
  if (std::fabs(det)<1e-9f) return false;
  const float inv=1.f/det;
  const auto s=p-a;
  const float u=inv*dot(s,h);
  if (u<0.f || u>1.f) return false;
  const auto k=cross(s,e1);
  const float v=inv*dot(d,k), t=inv*dot(e2,k);
  return v>=0.f && u+v<=1.f && t>1e-5f && t<1.f-1e-5f;
}
void old_position_negative_control(const Entity& body, const Bank& bank) {
  auto old_transform=body.transform;
  old_transform.position=old_transform.position-kTranslation;
  std::vector<Vec3> actor,desk;
  for (const auto& v : body.mesh->vertices)
    actor.push_back(transform_point(old_transform.matrix(),v.position));
  for (const auto& v : bank.worktop.vertices)
    desk.push_back(transform_point(bank.worktop_transform,v.position));
  std::size_t crossings=0;
  for (std::size_t i=0;i<body.mesh->indices.size();i+=3)
    for (int edge=0;edge<3;++edge)
      for (std::size_t j=0;j<bank.worktop.indices.size();j+=3)
        crossings+=edge_crosses_triangle(
            actor[body.mesh->indices[i+edge]],actor[body.mesh->indices[i+(edge+1)%3]],
            desk[bank.worktop.indices[j]],desk[bank.worktop.indices[j+1]],
            desk[bank.worktop.indices[j+2]]);
  require(crossings>0,"Old z=-7.4 negative control must pierce actual worktop triangles");
  std::cout<<"Old-position negative control: "<<crossings<<" edge/triangle crossings\n";
}
void verify_translation_and_timing() {
  auto corrected=roster_system();
  const auto authored=teller(corrected);
  require(same(authored.position,{.15f,.84f,-8.9f}) &&
          same(authored.home,{.15f,0,-8.9f}) &&
          same_route(authored.waypoints,{{.15f,0,-8.9f},{-.6f,0,-8.9f},{.7f,0,-8.9f}}),
          "Approved staff-side spawn/home/three route points");
  require(authored.speed==.35f && authored.height==1.68f &&
          authored.schedule==NpcSchedule::DayOnly && authored.kind==NpcKind::Civilian &&
          vaultline::npc_character_role(authored.entity_name,authored.kind)==CharacterRole::BankStaff,
          "Teller role, height, speed and schedule retained");
  for (int hz : {30,60}) {
    NpcSystem current,old;
    current.add(authored);
    auto original=authored;
    // Exact pre-fix float literals; reversing a float translation can lose an
    // ulp and is not a faithful independent baseline for timing comparison.
    original.position={.15f,.84f,-7.4f};
    original.home={.15f,0.f,-7.4f};
    original.waypoints={{.15f,0.f,-7.4f},{-.6f,0.f,-7.4f},{.7f,0.f,-7.4f}};
    original.base_waypoints=original.waypoints;
    old.add(original);
    int transitions=0,last=current.agents()[0].waypoint_index;
    for (int frame=0;frame<hz*60;++frame) {
      current.update(1.f/hz, .45f);old.update(1.f/hz, .45f);
      const auto& a=current.agents()[0];const auto& b=old.agents()[0];
      require(same(a.position,b.position+kTranslation) && a.yaw==b.yaw &&
              a.waypoint_index==b.waypoint_index && a.actual_speed==b.actual_speed &&
              a.travel_distance==b.travel_distance && a.anim_phase==b.anim_phase &&
              a.move_weight==b.move_weight && a.turn_rate==b.turn_rate,
              "Translation must preserve route shape, arrivals, timing and animation signals");
      if (a.waypoint_index!=last) { ++transitions;last=a.waypoint_index; }
    }
    require(transitions>=6,"Timing comparison includes multiple complete circuits");
  }
}
void verify_patrol_talk_and_dialogue(Bank& bank) {
  auto npcs=roster_system();CrewSystem crew;
  for (auto& spec : vaultline::make_crew_roster()) crew.add(std::move(spec.member));
  auto& a=teller(npcs);
  Entity body;body.name=a.entity_name;body.transform.position=a.position;
  vaultline::NpcPresentation presentation;
  presentation.install(bank.scene,body,a.height,CharacterRole::BankStaff,{.42f,.55f,.62f});
  old_position_negative_control(body,bank);
  for (const auto& o : bank.obstacles)
    require(horizontal_clearance(kLobbyListener,o.bounds,.45f)>0.f,
            "Dialogue camera must be on a legal full-body lobby approach: "+o.name);
  float min_clearance=std::numeric_limits<float>::max(),max_radius=0.f;
  std::size_t talk_samples=0;
  // Finite coverage: 60s at30Hz, normal0.35m/s patrol, then repeated ordinary
  // dialogue to the legal lobby listener. No artificial stress-pose inputs.
  // This certifies sampled poses, not every possible future gesture/gaze input.
  for (int frame=0;frame<=1800;++frame) {
    if (frame) {
      npcs.update(1.f/30.f, .45f);
      if (frame>=900 && (frame-900)%180==0)
        presentation.talk(a.entity_name,kLobbyListener,6.f);
      presentation.advance(a.entity_name,
          {a.position,a.yaw,a.actual_speed,a.travel_distance,a.move_weight,a.turn_rate,0.f,a.on_duty},
          1.f/30.f,kLobbyListener,100.f);
    }
    body.transform.position=a.position;body.transform.rotation_euler.y=a.yaw;
    const auto bounds=mesh_bounds(*body.mesh,body.transform.matrix());
    for (const auto& o : bank.obstacles) {
      const float gap=separation(bounds,o.bounds);
      require(gap>1e-4f,"Sampled teller mesh overlaps actual bank bounds: "+o.name+
              " at frame "+std::to_string(frame));
      min_clearance=std::min(min_clearance,gap);
    }
    for (const auto& v : body.mesh->vertices)
      max_radius=std::max(max_radius,std::hypot(v.position.x,v.position.z));
    const float yaw=std::atan2(a.position.z-kLobbyListener.z,a.position.x-kLobbyListener.x);
    const auto focus=vaultline::find_npc_talk_focus(npcs,crew,kLobbyListener,yaw);
    require(focus.entity_name==a.entity_name && focus.role==DialogueRole::Civilian,
            "Generic Q focus must still select Lia from lobby against the actual roster");
    talk_samples+=frame>=900;
  }
  require(talk_samples==901,"Dialogue sample family included");
  std::cout<<"1801 near-mesh samples (30Hz,60s;901 ordinary-talk samples), "
           <<bank.obstacles.size()<<" actual bank bounds; minimum AABB separation="
           <<min_clearance<<"m, observed horizontal radius="<<max_radius<<"m\n";
}
void verify_real_mission_reset(Bank& bank) {
  auto npcs=roster_system();TrafficSystem traffic;
  const auto baseline=teller(npcs);
  bank.mission.apply_world_state(bank.scene,npcs,traffic,meridian::MissionWorldState::Alarm);
  const auto alarm=teller(npcs);
  require(same(alarm.position,{-1.5f,.84f,-6.5f}) && same(alarm.home,{-1.5f,0,-6.5f}) &&
          same_route(alarm.waypoints,{{-1.5f,0,-6.5f}}) &&
          same_route(alarm.base_waypoints,alarm.waypoints) && alarm.waypoint_index==0 &&
          !alarm.chasing && alarm.actual_speed==0.f && alarm.move_weight==0.f &&
          alarm.speed==baseline.speed && alarm.height==baseline.height && alarm.on_duty,
          "Real Alarm relocation mutates authoritative agent with unchanged existing semantics");
  bank.mission.apply_world_state(bank.scene,npcs,traffic,meridian::MissionWorldState::Escape);
  const auto escape=teller(npcs);
  require(same(escape.position,alarm.position) && same(escape.home,alarm.home) &&
          same_route(escape.waypoints,alarm.waypoints) &&
          same_route(escape.base_waypoints,alarm.base_waypoints),"Escape retains alarm teller anchors");
  bank.mission.apply_world_state(bank.scene,npcs,traffic,meridian::MissionWorldState::PreHeist);
  const auto restored=teller(npcs);
  require(same(restored.position,baseline.position) && same(restored.home,baseline.home) &&
          same_route(restored.waypoints,baseline.waypoints) &&
          same_route(restored.base_waypoints,baseline.base_waypoints) &&
          restored.speed==baseline.speed && restored.base_speed==baseline.base_speed,
          "PreHeist reset must restore approved roster anchors, home and pace");
  npcs.apply_schedules(false);
  require(!teller(npcs).on_duty,"DayOnly teller still leaves duty at night");
  npcs.apply_schedules(true);
  require(teller(npcs).on_duty && same_route(teller(npcs).waypoints,baseline.waypoints),
          "Daytime restores teller without route drift");
}
} // namespace
int main() {
  try {
    verify_translation_and_timing();
    Bank bank;load_real_bank(bank);
    verify_patrol_talk_and_dialogue(bank);
    verify_real_mission_reset(bank);
    std::cout<<"Teller placement regressions passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr<<"Teller placement regression: "<<e.what()<<'\n';
    return 1;
  }
}
