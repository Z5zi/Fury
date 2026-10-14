#include "fury/crew.hpp"
#include "fury/npc.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace fury;
constexpr float kPi = 3.14159265358979323846f;
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
bool close(float a, float b, float tolerance = 1.e-4f) {
  return std::fabs(a - b) <= tolerance;
}
bool same(Vec3 a, Vec3 b, float tolerance = 1.e-5f) {
  return close(a.x,b.x,tolerance) && close(a.y,b.y,tolerance) && close(a.z,b.z,tolerance);
}
float distance(Vec3 a, Vec3 b) { return std::hypot(a.x-b.x,a.z-b.z); }
bool same_route(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i=0;i<a.size();++i) if (!same(a[i],b[i],0.f)) return false;
  return true;
}
NpcAgent walker() {
  NpcAgent a;
  a.name="movement-unit";
  a.display_name="Movement unit";
  a.entity_name="MovementUnit";
  a.position={0,.9f,0};
  a.waypoints={{0,0,20}};
  return a;
}
void check_finite(const NpcAgent& a) {
  require(std::isfinite(a.position.x) && std::isfinite(a.position.y) &&
          std::isfinite(a.position.z) && std::isfinite(a.yaw) &&
          std::isfinite(a.actual_speed) && std::isfinite(a.turn_rate) &&
          std::isfinite(a.travel_distance) && std::isfinite(a.anim_phase) &&
          std::isfinite(a.breathe_phase), "nonfinite motion state");
  require(a.move_weight >= 0.f && a.move_weight <= 1.f,"walk blend range");
}

template <typename Agent>
void unchanged_motion(const Agent& a, const Agent& before) {
  require(same(a.position,before.position,0.f) && a.yaw==before.yaw &&
          a.anim_phase==before.anim_phase && a.breathe_phase==before.breathe_phase &&
          a.move_weight==before.move_weight && a.travel_distance==before.travel_distance &&
          a.actual_speed==before.actual_speed && a.turn_rate==before.turn_rate &&
          same(a.velocity,before.velocity,0.f) && a.locomotion_speed==before.locomotion_speed &&
          a.yaw_speed==before.yaw_speed, "invalid dt must be a complete motion no-op");
}

void invalid_and_hitch_time() {
  NpcSystem n;
  n.add(walker());
  CrewSystem c;
  c.add({});
  n.update(.1f, .45f);
  c.update(.1f,{20,0,0},0.f,true);
  const auto nb=n.agents()[0];
  const auto cb=c.members()[0];
  for (float dt : {0.f, -1.f, -1000.f, std::numeric_limits<float>::quiet_NaN(),
                   std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
    n.update(dt, .45f);
    c.update(dt, {20,0,0},0.f,true);
    unchanged_motion(n.agents()[0],nb);
    unchanged_motion(c.members()[0],cb);
  }
  NpcSystem bounded=n;
  n.update(std::numeric_limits<float>::max(), .45f);
  bounded.update(.25f, .45f);
  unchanged_motion(n.agents()[0],bounded.agents()[0]);
  require(distance(n.agents()[0].position,nb.position) <= nb.speed*.25f + .0001f,
          "hitch teleported patrol");
  n.update(std::numeric_limits<float>::denorm_min(), .45f);
  check_finite(n.agents()[0]);
  require(std::isfinite(n.agents()[0].velocity.x) && std::isfinite(n.agents()[0].velocity.z),
          "tiny positive dt corrupted measured velocity");
  CrewSystem bounded_crew=c;
  c.update(10000.f,{20,0,0},0.f,true);
  bounded_crew.update(.25f,{20,0,0},0.f,true);
  unchanged_motion(c.members()[0],bounded_crew.members()[0]);
}

void smooth_start_stop_and_measured_motion() {
  NpcSystem n;
  n.add(walker());
  constexpr float dt=1.f/60.f;
  float previous_speed=0.f;
  float last_phase=0.f;
  double measured_distance=0;
  bool was_full_speed=false, was_braking=false;
  for (int i=0;i<850;++i) {
    const auto before=n.agents()[0];
    n.update(dt, .45f);
    const auto& a=n.agents()[0];
    const float delta=distance(a.position,before.position);
    measured_distance+=delta;
    require(a.position.z>=before.position.z && a.position.z<=20.f,"single target overshoot/reversal");
    require(close(a.actual_speed,delta/dt,.0001f),"actual speed must measure displacement");
    require(close(a.velocity.z,delta/dt,.0001f) && a.velocity.y==0.f,"velocity must measure XZ displacement");
    require(std::fabs(std::remainder(a.anim_phase-last_phase-delta*3.2f,2.f*kPi))<1.e-5f,
            "legacy gait phase must track actual distance");
    require(a.locomotion_speed-previous_speed<=4.f*dt+.0001f,"unbounded patrol acceleration");
    // The final <1mm settling threshold is a deliberate numerical stop.
    if (20.f-a.position.z>.002f) {
      require(previous_speed-a.locomotion_speed<=7.f*dt+.0001f,"unbounded patrol braking");
    }
    if (a.actual_speed>a.speed*.95f) was_full_speed=true;
    if (was_full_speed && a.actual_speed<a.speed*.8f) was_braking=true;
    if (i==0) require(a.move_weight>0.f && a.move_weight<.1f,"walk blend snapped to full");
    previous_speed=a.locomotion_speed;
    last_phase=a.anim_phase;
    check_finite(a);
  }
  auto settled=n.agents()[0];
  require(was_full_speed && was_braking,"patrol should accelerate and brake");
  require(distance(settled.position,{0,0,20})<=.0011f,"single waypoint did not settle");
  require(std::fabs(settled.travel_distance-measured_distance)<1.e-6,"travel distance mismatch");
  require(settled.move_weight==0.f && settled.actual_speed==0.f,"idle should fully settle");
  for(int i=0;i<100;++i) n.update(dt, .45f);
  require(same(n.agents()[0].position,settled.position,0.f) &&
          n.agents()[0].anim_phase==settled.anim_phase &&
          n.agents()[0].travel_distance==settled.travel_distance,"idle gait or position jitter");
  require(n.agents()[0].breathe_phase!=settled.breathe_phase,"idle breathing froze");
}

void smooth_heading_and_straight_segments() {
  for (float dt : {1.f/120.f,1.f/60.f,1.f/30.f,.1f,.25f}) {
    NpcSystem n;
    auto a=walker();
    a.speed=1.7f;
    a.waypoints={{0,0,0},{3,0,0},{3,0,4},{0,0,4},{0,0,0}};
    n.add(a);
    int transitions=0;
    for(int i=0;i<static_cast<int>(100.f/dt);++i) {
      const auto before=n.agents()[0];
      n.update(dt, .45f);
      const auto& now=n.agents()[0];
      if(now.waypoint_index!=before.waypoint_index) ++transitions;
      require(std::fabs(std::remainder(now.yaw-before.yaw,2.f*kPi)) <=4.2f*dt+.0001f,
              "patrol heading snapped");
      require(std::fabs(now.turn_rate)<=4.2f+.0001f,"turn signal exceeds heading limit");
      require(now.position.x>=-.0001f && now.position.x<=3.0001f &&
              now.position.z>=-.0001f && now.position.z<=4.0001f,"patrol escaped rectangle");
      const float edge=(std::min)({std::fabs(now.position.x),std::fabs(now.position.x-3.f),
                                    std::fabs(now.position.z),std::fabs(now.position.z-4.f)});
      require(edge<.0011f,"inertia cut across authored rectangle");
      require(distance(before.position,now.position)<=a.speed*dt+.0001f,"patrol step too large");
      check_finite(now);
    }
    require(transitions>=20,"patrol failed to cycle route");
    require(same_route(n.agents()[0].waypoints,a.waypoints),"movement rewrote authored route");
  }
  NpcSystem wrap;
  auto a=walker();
  a.yaw=kPi-.03f;
  a.waypoints={{-.6f,0,-20.f}};
  wrap.add(a);
  wrap.update(.02f, .45f);
  require(std::remainder(wrap.agents()[0].yaw-a.yaw,2.f*kPi)>0.f,"heading must take short arc across pi");

  NpcSystem duplicates;
  a=walker(); a.waypoints={{0,0,0},{0,0,0},{0,0,0}}; a.waypoint_index=-99;
  duplicates.add(a);
  duplicates.update(100.f, .45f);
  require(duplicates.agents()[0].actual_speed==0.f && duplicates.agents()[0].travel_distance==0.0,
          "coincident route should idle without looping forever");
  duplicates.agents()[0].waypoints.push_back({0,0,3});
  duplicates.update(.1f, .45f);
  require(duplicates.agents()[0].position.z>0.f,"duplicate route did not resume");
}

void chase_selection_pause_and_resumption() {
  for (NpcKind kind:{NpcKind::Civilian,NpcKind::Fence,NpcKind::Guard,NpcKind::Enforcer}) {
    NpcSystem n;
    auto a=walker(); a.kind=kind; a.chasing=true; a.chase_target={5,0,0};
    n.add(a);
    n.update(.1f, .45f,{1000,0,1000},1.f);
    const bool threat=kind==NpcKind::Guard || kind==NpcKind::Enforcer;
    require((n.agents()[0].travel_distance>0)==threat,"far update/chase role eligibility changed");
    if(!threat) {
      n.update(.25f, .45f);
      require(n.agents()[0].position.z>0 && n.agents()[0].position.x==0.f,
              "civilian/fence incorrectly chased instead of using route");
      continue;
    }
    for(int i=0;i<400;++i) n.update(1.f/60.f, .45f,{1000,0,1000},1.f);
    const auto settled=n.agents()[0];
    require(distance(settled.position,a.chase_target)>=.45f+a.radius-.0001f &&
            distance(settled.position,a.chase_target)<=.45f+a.radius+.0011f,"summed-radius chase arrival failed");
    require(settled.actual_speed==0 && settled.move_weight==0,"chase idle not stable");
    for(int repetition=0;repetition<5;++repetition) {
      n.agents()[0].chasing=false;
      const float previous_z=n.agents()[0].position.z;
      for(int i=0;i<90;++i) n.update(1.f/60.f, .45f);
      require(n.agents()[0].position.z>previous_z,"patrol failed to resume after chase");
      require(n.agents()[0].waypoint_index==0,"chase rewrote patrol index");
      n.agents()[0].chasing=true;
      n.agents()[0].chase_target=n.agents()[0].position+Vec3{2,0,0};
      for(int i=0;i<120;++i) n.update(1.f/60.f, .45f);
      check_finite(n.agents()[0]);
    }
  }
  NpcSystem paused;
  paused.add(walker()); paused.update(.25f, .45f);
  const auto before=paused.agents()[0];
  paused.update(.1f, .45f,{1000,0,1000},1.f);
  require(same(paused.agents()[0].position,before.position,0.f) &&
          paused.agents()[0].actual_speed==0.f && paused.agents()[0].velocity.z==0.f,
          "far perf skip has phantom motion");
  paused.update(.1f, .45f);
  require(paused.agents()[0].position.z>before.position.z,"far NPC did not resume");
}

void close_moving_targets_and_invalid_speeds() {
  for (NpcKind kind : {NpcKind::Guard, NpcKind::Enforcer}) {
    for (float hitch : {.1f, .25f, 100.f}) {
      NpcSystem n;
      auto a=walker(); a.kind=kind; a.chasing=true; a.chase_target={0,0,100};
      a.chase_speed=kind==NpcKind::Enforcer ? 5.6f : 3.5f;
      n.add(a);
      const float player_radius=.45f, stop=player_radius+a.radius;
      for(int i=0;i<90;++i) n.update(1.f/60.f,player_radius);
      require(n.agents()[0].actual_speed>a.chase_speed-.01f,"chase never reached authored speed");
      n.agents()[0].chase_target=n.agents()[0].position+Vec3{0,0,stop+.01f};
      const auto before=n.agents()[0];
      n.update(hitch,player_radius);
      require(n.agents()[0].position.z<=before.chase_target.z-stop+.0001f,
              "moving close target caused summed-radius overshoot at speed/hitch");
      require(n.agents()[0].position.z>before.position.z,
              "close target case must exercise forward braking rather than already-overlapping idle");
      require(distance(n.agents()[0].position,before.position)<=.0101f,
              "close target movement exceeded remaining clearance");
      // Player intrusion/co-located targets remain an explicit no-push-out case.
      n.agents()[0].chase_target=n.agents()[0].position;
      const auto on_target=n.agents()[0];
      for(int i=0;i<30;++i) n.update(1.f/30.f,player_radius);
      require(same(n.agents()[0].position,on_target.position,0.f) &&
              n.agents()[0].travel_distance==on_target.travel_distance,"overlap must idle without push-out");
      for(float speed:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity()}) {
        n.agents()[0].chase_speed=speed;
        n.agents()[0].chase_target={0,0,100};
        n.update(.1f,player_radius);
        require(same(n.agents()[0].position,on_target.position,0.f),"invalid configured speed moved NPC");
        check_finite(n.agents()[0]);
      }
    }
  }
  CrewSystem crew;
  CrewMember c; c.follow_offset={}; crew.add(c);
  for(int i=0;i<90;++i) crew.update(1.f/60.f,{100,0,0},0.f,true);
  const auto moving=crew.members()[0];
  const Vec3 near=moving.position+Vec3{.005f,0,0};
  crew.update(.25f,near,0.f,true);
  require(crew.members()[0].position.x<=near.x &&
          crew.members()[0].position.x>=moving.position.x,"close follow target overshot");
}

void summed_radius_arrival_and_moving_player() {
  // Independently varied player and per-agent radii reject a fixed .85 substitute.
  for (NpcKind kind : {NpcKind::Guard,NpcKind::Enforcer})
    for(float speed : {3.4f,3.5f,3.8f,4.f,5.6f})
      for(float player_radius : {.2f,.45f,.7f})
        for(float dt : {1.f/120.f,1.f/60.f,1.f/30.f,.1f,.25f}) {
          NpcSystem n;
          for(float radius : {.25f,.4f,.65f}) {
            auto a=walker();a.kind=kind;a.radius=radius;a.chase_speed=speed;
            a.chasing=true;a.chase_target={0,0,12};a.yaw=0.f;n.add(a);
          }
          for(int frame=0;frame<int(12.f/dt);++frame) {
            n.update(dt,player_radius,{1000,0,1000},1.f);
            for(const auto& a:n.agents()) {
              require(distance(a.position,a.chase_target)>=player_radius+a.radius-.0001f,
                      "stationary approach crossed configured clearance");
              require(a.actual_speed<=speed+.001f,"arrival exceeded authored chase speed");
            }
          }
          const auto settled=n.agents();
          for(const auto& a:settled) {
            require(distance(a.position,a.chase_target)<=player_radius+a.radius+.0011f,
                    "threat did not reach configured clearance");
            require(a.actual_speed==0.f && a.move_weight==0.f,"arrival did not settle to idle");
          }
          for(int frame=0;frame<int(1.f/dt);++frame)n.update(dt,player_radius);
          for(std::size_t i=0;i<settled.size();++i) {
            const auto& a=n.agents()[i];const auto& b=settled[i];
            require(same(a.position,b.position,0.f)&&a.travel_distance==b.travel_distance&&
                    a.anim_phase==b.anim_phase&&a.actual_speed==0.f,"settled arrival jittered or walked in place");
            require(a.breathe_phase!=b.breathe_phase,"settled threat stopped active breathing");
          }
          // The player retreats continuously; current target updates remain authoritative.
          for(int frame=0;frame<int(3.f/dt);++frame) {
            for(auto& a:n.agents())a.chase_target.z+=dt;
            const auto before=n.agents();n.update(dt,player_radius);
            for(std::size_t i=0;i<before.size();++i) {
              const auto& a=n.agents()[i];
              require(distance(a.position,a.chase_target)>=player_radius+a.radius-.0001f,
                      "moving player approach crossed current target clearance");
              require(distance(a.position,before[i].position)<=speed*dt+.001f,
                      "moving player caused chase teleport");
            }
          }
          for(int frame=0;frame<int(4.f/dt);++frame)n.update(dt,player_radius);
          for(std::size_t i=0;i<settled.size();++i) {
            const auto& a=n.agents()[i];
            require(a.travel_distance>settled[i].travel_distance+2.9 &&
                    distance(a.position,a.chase_target)<=player_radius+a.radius+.0011f,
                    "retreat did not trigger continuous reapproach and settling");
            require(a.waypoint_index==settled[i].waypoint_index&&same_route(a.waypoints,settled[i].waypoints),
                    "reapproach rewrote pending patrol");
          }
        }
}

void schedules_are_repeatable() {
  NpcSystem n;
  auto a=walker();
  a.schedule=NpcSchedule::NightTighten;
  a.home={2,0,3};
  a.waypoints={{-1,0,2},{5,0,4},{2,0,-3}};
  a.waypoint_index=1;
  n.add(a);
  auto day=walker(); day.schedule=NpcSchedule::DayOnly; n.add(day);
  n.apply_schedules(true);
  for(int repeat=0;repeat<30;++repeat) {
    n.apply_schedules(false);
    const auto night=n.agents()[0];
    require(close(night.speed,a.speed*1.45f),"night speed must be based on day speed");
    for(std::size_t i=0;i<a.waypoints.size();++i) {
      require(close(night.waypoints[i].x,a.home.x+(a.waypoints[i].x-a.home.x)*.52f) &&
              close(night.waypoints[i].z,a.home.z+(a.waypoints[i].z-a.home.z)*.52f),
              "night patrol compression changed/compounded");
    }
    n.apply_schedules(false);
    require(same_route(n.agents()[0].waypoints,night.waypoints),"night reapplication changed route");
    const auto off=n.agents()[1];
    require(!off.on_duty && !off.chasing && off.actual_speed==0.f,"day-only still active at night");
    n.update(.1f, .45f);
    unchanged_motion(n.agents()[1],off);
    n.apply_schedules(true);
    require(same_route(n.agents()[0].waypoints,a.waypoints) &&
            n.agents()[0].speed==a.speed && n.agents()[1].on_duty,"day route/schedule failed to restore");
    n.update(.1f, .45f);
    require(n.agents()[0].name==a.name && n.agents()[0].display_name==a.display_name &&
            n.agents()[0].entity_name==a.entity_name && n.agents()[0].height==a.height &&
            n.agents()[0].radius==a.radius,"schedule changed identity or dimensions");
  }
  n.agents()[0].waypoint_index=999;
  n.apply_schedules(true);
  require(n.agents()[0].waypoint_index==0,"day schedule failed to repair invalid index");
  auto stationary=walker(); stationary.waypoints.clear(); stationary.schedule=NpcSchedule::NightTighten;
  n.add(stationary); n.apply_schedules(false); n.update(.1f, .45f);
  require(n.agents().back().actual_speed==0.f,"empty night route moved");
}

void crew_follows_camera_basis_and_ground() {
  for(float yaw:{0.f,kPi*.5f,-kPi*.5f,kPi}) {
    for(float height:{1.4f,1.7f,1.72f,2.0f}) {
      CrewSystem crew;
      CrewMember c; c.height=height; c.follow_offset={-1.8f,0,-1.4f};
      c.position={-10,99,-10}; crew.add(c);
      const Vec3 player{2,4,3};
      const Vec3 expected{player.x-std::sin(yaw)*c.follow_offset.x+std::cos(yaw)*c.follow_offset.z,
                          height*.5f,
                          player.z+std::cos(yaw)*c.follow_offset.x+std::sin(yaw)*c.follow_offset.z};
      for(int i=0;i<700;++i) {
        const auto before=crew.members()[0];
        crew.update(1.f/60.f,player,yaw,true);
        const auto& now=crew.members()[0];
        require(now.position.y==height*.5f,"crew feet float or sink");
        require(distance(now.position,expected)<=distance(before.position,expected)+1.e-5f,
                "crew moved away from fixed target");
        require(distance(before.position,now.position)<=c.follow_speed/60.f+.0001f,
                "crew overshot movement budget");
      }
      const auto stopped=crew.members()[0];
      require(distance(stopped.position,expected)<=.0011f,"camera-relative crew target incorrect");
      require(stopped.actual_speed==0.f && stopped.move_weight==0.f,"crew did not idle on arrival");
      crew.update(.2f,player,yaw,true);
      require(same(crew.members()[0].position,stopped.position,0.f) &&
              crew.members()[0].anim_phase==stopped.anim_phase,"crew arrival jitter");
      for(int repetition=0;repetition<4;++repetition) {
        crew.update(.25f,{20,0,20},yaw,true);
        const auto moving=crew.members()[0];
        crew.update(.05f,player,yaw,false);
        require(same(crew.members()[0].position,moving.position,0.f) &&
                crew.members()[0].actual_speed==0.f && crew.members()[0].anim_phase==moving.anim_phase,
                "crew moved outside follow phases");
        crew.update(.25f,{20,0,20},yaw,true);
        require(crew.members()[0].travel_distance>moving.travel_distance,"crew follow did not resume");
      }
      crew.members()[0].active=false;
      const auto inactive=crew.members()[0];
      crew.update(.25f,player,yaw,true);
      require(same(crew.members()[0].position,inactive.position,0.f) &&
              crew.members()[0].breathe_phase==inactive.breathe_phase &&
              crew.members()[0].travel_distance==inactive.travel_distance,"inactive crew moved/animated");
      crew.members()[0].active=true;
      const auto before=crew.members()[0];
      for(int i=0;i<90;++i) crew.update(1.f/60.f,player,yaw,true);
      require(crew.members()[0].travel_distance>before.travel_distance,"reactivated crew failed to resume");
    }
  }
}

void crew_proximity_and_capacity() {
  CrewSystem c;
  require(c.loot_speed_boost({})==1.f,"solo loot boost changed");
  CrewMember rook; rook.name="Rook"; rook.position={3,.85f,4}; c.add(rook);
  require(c.nearby_count({},5.f)==1 && close(c.loot_speed_boost({}),1.175f),"one crew edge-radius boost changed");
  CrewMember sparrow; sparrow.name="Sparrow"; sparrow.position={0,.86f,0}; c.add(sparrow);
  require(c.nearby_count({},5.f)==2 && close(c.loot_speed_boost({}),1.35f),"two crew boost changed");
  c.members()[0].active=false;
  require(c.nearby_count({},5.f)==1 && close(c.loot_speed_boost({}),1.175f),"inactive crew contributes loot buff");
  CrewMember replacement; replacement.name="Replacement"; replacement.position={10,.85f,0};
  c.add(replacement);
  require(c.members().size()==2 && c.members()[0].name=="Rook" && c.members()[1].name=="Replacement",
          "crew capacity replacement semantics changed");
  require(c.nearby_count({},5.f)==0 && c.loot_speed_boost({})==1.f,"distant crew contributes loot buff");
}
}  // namespace

int main() {
  try {
    invalid_and_hitch_time();
    smooth_start_stop_and_measured_motion();
    smooth_heading_and_straight_segments();
    chase_selection_pause_and_resumption();
    close_moving_targets_and_invalid_speeds();
    summed_radius_arrival_and_moving_player();
    schedules_are_repeatable();
    crew_follows_camera_basis_and_ground();
    crew_proximity_and_capacity();
    std::cout<<"NPC/crew locomotion: timing, route fidelity, schedules, chase, follow, and loot tests passed\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr<<"NPC/crew locomotion failure: "<<e.what()<<'\n';
    return 1;
  }
}
