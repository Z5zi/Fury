// Direct production-model contract tests, not a normal-play seated certificate.
#include "../apps/vaultline/patrol_geometry.hpp"
#include <fury/application.hpp>
#include <fury/factions.hpp>
#include <fury/heat.hpp>
#include <fury/pursuit.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace fury;
namespace {
int checks=0;
void require(bool ok,const char* reason) {++checks;if(!ok)throw std::runtime_error(reason);}
bool close(double a,double b,double tolerance=2e-5) {return std::abs(a-b)<=tolerance;}
double distance(Vec3 a,Vec3 b={}) {return std::hypot(double(a.x)-b.x,double(a.z)-b.z);}
PursuitSystem ready(float radius,float speed,Vec3 start={0,0,20}) {
  PursuitSystem p;PatrolCar car;car.speed=speed;car.clearance_radius=radius;
  p.configure({car});p.cars()[0].active=true;p.cars()[0].position=start;return p;
}
float advance(PursuitSystem& p,float dt,float target_radius,bool seated=false,Vec3 target={}) {
  return p.update(dt,target,.5f,false,seated,false,target_radius);
}
// Exact legacy per-car movement/contact order, used only as a negative/reference
// model. Production code does not call this or accept a legacy-mode switch.
float legacy(PatrolCar& c,Vec3 target,float dt,bool seated) {
  c.contact_cooldown=std::max(0.f,c.contact_cooldown-dt);
  const float dx=target.x-c.position.x,dz=target.z-c.position.z;
  const float d=std::sqrt(dx*dx+dz*dz);
  if(d>=.35f) {
    const float step=c.speed*(seated?.82f:1.f)*dt;
    c.position.x+=dx*(1.f/d)*step;c.position.z+=dz*(1.f/d)*step;
  }
  if(d<=3.8f&&c.contact_cooldown<=0.f) {c.contact_cooldown=1.1f;return .07f;}
  return 0.f;
}
void stationary(float radius,float target_radius,bool seated,float speed,int hz) {
  const float dt=1.f/hz;const double arrival=double(radius)+target_radius;
  auto p=ready(radius,speed,{17,0,23});auto old=p.cars()[0];
  old.contact_cooldown=p.cars()[0].contact_cooldown=.4f;
  int first=-1,old_first=-1,pulses=0;std::vector<int> events,old_events;
  Vec3 rest{};bool at_rest=false;
  for(int frame=0;frame<hz*12;++frame) {
    const Vec3 before=p.cars()[0].position;const double before_d=distance(before);
    const float bump=advance(p,dt,target_radius,seated);
    const double after_d=distance(p.cars()[0].position);
    require(after_d>=arrival,"Separated approach crossed envelope");
    require(after_d<=before_d+2e-5,"Approach moved away from stationary target");
    const double requested=speed*(seated?.82f:1.f)*dt;
    require(close(distance(before,p.cars()[0].position),std::min(requested,std::max(0.,before_d-arrival)),3e-5),"Unexpected braking/speed before final step");
    require(bump==0.f||close(bump,.07f),"Unexpected heat amount");
    if(bump>0) {if(first<0)first=frame;++pulses;events.push_back(frame);}
    if(legacy(old,{},dt,seated)>0) {if(old_first<0)old_first=frame;old_events.push_back(frame);}
    if(close(after_d,arrival,3e-5)) {
      if(!at_rest){rest=p.cars()[0].position;at_rest=true;}
      require(distance(rest,p.cars()[0].position)<2e-5,"Rest drift/oscillation");
    }
  }
  require(at_rest&&pulses>=5,"Did not settle while retaining repeated contact heat");
  require(first==old_first,"Ordinary stationary first contact timing changed");
  require(events==old_events,"Ordinary stationary cooldown event cadence changed");
  require(distance(old.position)<arrival-.1,"Legacy negative control did not violate clearance");
  std::cout<<"stationary radius="<<radius<<" proxy="<<target_radius<<" seated="<<seated<<" speed="<<speed<<" hz="<<hz<<" first_contact="<<first<<" pulses="<<pulses<<" rest="<<distance(rest)<<'\n';
}
void hitches_and_motion(float radius,float proxy,bool seated,float speed) {
  const double arrival=double(radius)+proxy;
  for(float dt:{.5f,1.f,3.f,10.f,120.f,1e10f}) {
    auto p=ready(radius,speed);
    require(advance(p,dt,proxy,seated)==0.f,"Contact must sample pre-move, even during a hitch");
    require(distance(p.cars()[0].position)>=arrival,"Hitch crossed envelope");
    require(advance(p,120.f,proxy,seated)<=.070001f,"Hitch generated multiple pulses");
    require(close(distance(p.cars()[0].position),arrival),"Hitch failed to settle");
  }
  auto p=ready(radius,speed,{0,0,static_cast<float>(arrival)});
  const Vec3 overlapped=p.cars()[0].position;
  advance(p,1.f,proxy,seated,{0,0,1});
  require(distance(overlapped,p.cars()[0].position)==0.,"Pre-existing overlap was pushed out");
  advance(p,1.f,proxy,seated,p.cars()[0].position);
  require(distance(overlapped,p.cars()[0].position)==0.,"Co-located target caused movement");
  Vec3 target{0,0,-4};
  advance(p,.05f,proxy,seated,target);
  require(p.cars()[0].position.z<overlapped.z,"Target retreat did not restart pursuit");
  for(int f=0;f<60;++f)advance(p,1.f/12,proxy,seated,target);
  require(close(distance(p.cars()[0].position,target),arrival),"Reapproach failed to settle");
  // Move target laterally/away. Check only steps beginning outside clearance;
  // player-driven intrusion and deactivation by distance are excluded contracts.
  for(int f=0;f<300;++f) {
    target.x+=.04f;target.z-=.03f;
    const double before=distance(p.cars()[0].position,target);
    advance(p,1.f/12,proxy,seated,target);
    if(before>=arrival)require(distance(p.cars()[0].position,target)>=arrival,"Moving-target autonomous crossing");
  }
}
void lifecycle(float radius) {
  AppConfig config;const float proxy=config.effective_player_radius(false);
  PatrolCar a,b;a.speed=11.5f;b.speed=10.2f;a.clearance_radius=b.clearance_radius=radius;
  PursuitSystem p;p.configure({a,b});
  p.update(1,{},.54f,false,false,false,proxy);require(p.active_count()==0,"Spawned below threshold");
  p.update(.64f,{},.55f,false,false,false,proxy);require(p.active_count()==0,"Spawn interval shortened");
  p.update(.02f,{},.55f,false,false,false,proxy);require(p.active_count()==1,"First slot did not spawn");
  require(close(p.cars()[0].contact_cooldown,.38f),"Activation cooldown no longer 0.4 minus dt");
  p.update(1,{},.77f,false,false,false,proxy);require(p.active_count()==1,"Two slots below threshold");
  p.update(.01f,{},.78f,false,false,false,proxy);require(p.active_count()==2,"Second slot not activated");
  p.update(.01f,{},.28f,false,false,false,proxy);require(p.active_count()==1,"Cooloff must drop one per update");
  p.update(.01f,{},.28f,false,false,false,proxy);require(p.active_count()==0,"Cooloff second slot remains");
  p.update(.65f,{},.1f,true,false,false,proxy);require(p.active_count()==1,"Alarm failed to activate");
  p.update(.65f,{},.1f,true,false,false,proxy);require(p.active_count()==2,"Alarm second slot failed");
  p.update(.01f,{},1.f,true,false,true,proxy);require(p.active_count()==0,"Safehouse failed to clear");
  p.update(.64f,{},1.f,true,false,false,proxy);require(p.active_count()==0,"Safehouse failed to reset spawn timer");
  p.update(.02f,{},1.f,true,false,false,proxy);require(p.active_count()==1,"Reactivation failed");
  require(close(p.cars()[0].contact_cooldown,.38f),"Reactivation cooldown changed");
  for(bool seated:{false,true}) {
    const float limit=seated?38.f:52.f;
    auto lost=ready(radius,11.5f,{0,0,limit});advance(lost,.01f,proxy,seated);
    require(lost.active_count()==0,"Inclusive distance loss threshold changed");
    auto near=ready(radius,11.5f,{0,0,limit-.01f});advance(near,.01f,proxy,seated);
    require(near.active_count()==1,"Premature distance loss");
  }
  FactionReputations factions;
  for(int rep:{-100,0,100}) {
    factions.metro_watch=rep;p.configure({a,b});p.spawn_interval=factions.pursuit_spawn_interval();
    p.update(p.spawn_interval-.01f,{},.6f,false,false,false,proxy);
    require(p.active_count()==0,"Faction spawn interval ignored");
    p.update(.02f,{},.6f,false,false,false,proxy);
    require(p.active_count()==1,"Faction spawn interval not honored");
  }
  p=ready(radius,11.5f,{0,0,3.7f});p.cars().push_back(p.cars()[0]);
  require(close(advance(p,.1f,proxy),.14f),"Two active cars must each contribute one contact pulse");
  p=ready(radius,11.5f,{0,0,3.8f});require(close(advance(p,.01f,proxy),.07f),"Inclusive 3.8 contact boundary changed");
  p=ready(radius,11.5f,{0,0,3.81f});require(advance(p,.1f,proxy)==0.f,"Contact sampled after move instead of before");
  require(close(advance(p,.1f,proxy),.07f),"Contact zone entered but pulse missing");
}
void invalid_contract(float radius) {
  const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
  for(float r:{0.f,-1.f,nan,inf}) {
    auto p=ready(r,11.5f);advance(p,.2f,.45f);
    require(distance(p.cars()[0].position,{0,0,20})==0,"Invalid geometry enabled approach");
  }
  for(float r:{-1.f,nan,inf}) {
    auto p=ready(radius,11.5f);advance(p,.2f,r);
    require(distance(p.cars()[0].position,{0,0,20})==0,"Invalid target radius enabled approach");
  }
  for(float origin:{-500.f,-50.f,0.f,50.f,500.f}) {
    Vec3 target{origin,0,origin};auto p=ready(radius,11.5f,{origin+4,0,origin+4});
    advance(p,10.f,.45f,false,target);
    require(distance(p.cars()[0].position,target)>=double(radius)+.45f,"Translated terminal float rounding crossed envelope");
  }
  auto point=ready(radius,11.5f);advance(point,10.f,0.f);
  require(close(distance(point.cars()[0].position),radius),"Zero proxy point contract failed");
  for(float dt:{0.f,-1.f,nan,inf}) {
    auto p=ready(radius,11.5f);p.cars()[0].contact_cooldown=.4f;
    require(advance(p,dt,.45f)==0,"Invalid time generated heat");
    require(distance(p.cars()[0].position,{0,0,20})==0&&p.cars()[0].contact_cooldown==.4f,"Invalid time advanced state");
    p.update(dt,{},1,true,false,true,.45f);require(p.active_count()==0,"Safehouse must clear even with invalid dt");
  }
  for(Vec3 target:{Vec3{nan,0,0},Vec3{0,inf,0},Vec3{0,0,nan}}) {
    auto p=ready(radius,11.5f);require(advance(p,.1f,.45f,false,target)==0,"Invalid target generated heat");
    require(distance(p.cars()[0].position,{0,0,20})==0,"Invalid target poisoned position");
  }
  for(float speed:{0.f,-1.f,nan,inf}) {
    auto p=ready(radius,speed);advance(p,.2f,.45f);
    require(distance(p.cars()[0].position,{0,0,20})==0,"Invalid speed moved car");
  }
}
void geometry(const harbor::HarborPrimSet& parts,const Mesh& light,float radius) {
  require(radius>0&&std::isfinite(radius),"Loaded bound invalid");
  std::size_t vertices=0;
  for(const auto& p:parts.parts)vertices+=p.mesh->vertices.size();
  require(vertices>0,"Geometry unexpectedly empty");
  AppConfig config;
  // Every part vertex bounds every rendered triangle by convexity. Sweep yaw
  // and radial directions; same conservative circle covers any relative heading.
  double minimum_gap=1e10;
  for(bool seated:{false,true}) {
    const double proxy=config.effective_player_radius(seated);
    const double arrival=double(radius)+proxy;
    require(arrival<3.8,"Content invariant failed: do not shrink geometry/widen heat zone");
    for(int angle=0;angle<72;++angle) {
      const float yaw=float(angle)*.137f;
      Transform root;root.rotation_euler.y=yaw;
      const float bearing=yaw*.713f+.47f;
      root.position={static_cast<float>(arrival*std::sin(bearing)),0,static_cast<float>(arrival*std::cos(bearing))};
      const auto matrix=root.matrix();
      const auto check=[&](const Mesh& mesh,Vec3 offset) {
        for(const auto& vertex:mesh.vertices) {
          const auto local=vertex.position+offset;
          const double local_radius=std::hypot(double(local.x),double(local.z));
          require(local_radius<=radius,"Some loaded part is outside declared envelope");
          const auto world=transform_point(matrix,local);
          const double gap=distance(world);
          require(gap+3e-6>=proxy,"Yaw sweep infringed target footprint");
          minimum_gap=std::min(minimum_gap,gap);
        }
      };
      for(const auto& p:parts.parts)check(*p.mesh,{});
      check(light,vaultline::kPatrolLightOffset);
    }
  }
  std::cout<<"geometry parts="<<parts.parts.size()<<" vertices="<<vertices<<" radius="<<radius<<" minimum_gap="<<minimum_gap<<'\n';
  // Old sealed distances are far inside the known light box XZ envelope for
  // some relative heading. The geometry test must reject those old positions.
  for(float old_distance:{.150646f,.347567f,.35f})
    require(old_distance-radius<0.45,"Old position negative control missed obstruction");
}
void synthetic_geometry() {
  Scene scene;auto* small=scene.add_mesh(make_box({.2f,.2f,.2f},{1,1,1}));
  auto* wide=scene.add_mesh(make_box({8.f,1.f,2.f},{1,1,1}));
  Mesh light=make_box({.5f,.2f,1.f},{1,1,1});
  harbor::HarborPrimSet parts;parts.parts={{small,{}},{wide,{}}};
  const float bound=vaultline::patrol_clearance_radius(parts,light,{});
  require(bound>=std::sqrt(17.),"Only the first visual mesh was measured");
  const float offset=vaultline::patrol_clearance_radius(parts,light,{9,1,0});
  require(offset>9.25,"Independent light offset was excluded");
  require(bound+.81f>3.8f,"Incompatible content was silently shrunk");
  require(vaultline::patrol_clearance_radius({},light,{})<0,"Empty body needs invalid sentinel");
  parts.parts={{nullptr,{}}};require(vaultline::patrol_clearance_radius(parts,light,{})<0,"Null body needs invalid sentinel");
  Mesh empty;parts.parts={{&empty,{}}};require(vaultline::patrol_clearance_radius(parts,light,{})<0,"Empty geometry accepted");
  Mesh zero;zero.vertices.push_back({});parts.parts={{&zero,{}}};require(vaultline::patrol_clearance_radius(parts,light,{})<0,"Zero body bound accepted");
  zero.vertices[0].position.x=std::numeric_limits<float>::quiet_NaN();
  require(vaultline::patrol_clearance_radius(parts,light,{})<0,"NaN vertex accepted");
  zero.vertices[0].position={3.4028232635611926e38f,0,1.0208470502152477e35f};
  require(vaultline::patrol_clearance_radius(parts,light,{})<0,"Outward radius rounding overflow violated sentinel contract");
  parts.parts={{small,{}}};require(vaultline::patrol_clearance_radius(parts,empty,{})<0,"Empty light accepted");
  require(vaultline::patrol_clearance_radius(parts,light,{0,std::numeric_limits<float>::infinity(),0})<0,"Invalid offset accepted");
}
void mission_heat(float radius) {
  for(auto phase:{HeistPhase::Idle,HeistPhase::Breach,HeistPhase::Looting,HeistPhase::Escape}) {
    auto p=ready(radius,11.5f,{0,0,radius+.45f});
    HeatMeter heat;heat.value=.96f;
    const float bump=advance(p,.01f,.45f);
    require(close(bump,.07f),"Stopped patrol lost mission heat consequence");
    heat.value=std::min(1.f,heat.value+bump);
    const bool failed=heat.update(.01f,phase,{}, {},false);
    HeistController heist;heist.apply_net_sync(phase,0.f);heist.inventory().loot_bags=2;
    if(failed||(heat.is_max()&&heist.phase()==HeistPhase::Escape))heist.force_fail();
    if(phase==HeistPhase::Idle)require(heist.phase()==phase,"Idle contact now arrests/fails");
    else require(heist.phase()==HeistPhase::Failed&&heist.inventory().loot_bags==0&&heist.score().failures==1,"Heat failure/carry consequence changed");
  }
}
}
int main() {
 try {
  AppConfig config;require(close(config.effective_player_radius(false),.45),"Foot proxy changed");
  require(close(config.effective_player_radius(true),.81),"Seated proxy changed");
  config.player_radius=.6f;require(close(config.effective_player_radius(true),1.08),"Proxy ignores config authority");
  config.player_radius=.45f;
  Scene scene;auto real=harbor::load_harbor_material_groups(scene,"hmpd_cruiser",make_box({4.2f,1.35f,2},{1,1,1}));
  require(real.from_asset&&!real.used_fallback&&real.parts.size()>1,"Real material groups not loaded");
  Mesh light=make_box({.55f,.25f,1.4f},{.9f,.15f,.12f});
  const float radius=vaultline::patrol_clearance_radius(real,light,vaultline::kPatrolLightOffset);
  require(close(radius,2.706679885,2e-5),"Current authored geometry unexpectedly changed");
  auto fallback=harbor::load_harbor_material_groups(scene,"missing_test_cruiser",make_box({4.2f,1.35f,2},{1,1,1}));
  require(fallback.used_fallback,"Procedural fallback not exercised");
  const float fallback_radius=vaultline::patrol_clearance_radius(fallback,light,vaultline::kPatrolLightOffset);
  require(close(fallback_radius,std::hypot(2.1,1.),2e-5),"Fallback dimensions not measured");
  geometry(real,light,radius);geometry(fallback,light,fallback_radius);synthetic_geometry();
  for(float r:{radius,fallback_radius})for(bool seated:{false,true})for(float speed:{11.5f,10.2f}) {
    const float proxy=config.effective_player_radius(seated);
    for(int hz:{120,60,30,12,10,4})stationary(r,proxy,seated,speed,hz);
    hitches_and_motion(r,proxy,seated,speed);
  }
  lifecycle(radius);invalid_contract(radius);mission_heat(radius);
  std::cout<<"PASS patrol arrival clearance: "<<checks<<" checks; seated results are direct model tests only\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;}
}
