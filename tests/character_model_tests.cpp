#include "fury/character_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <new>
#include <set>
#include <stdexcept>
#include <string>

namespace {
bool count_allocations=false;
std::size_t allocations=0;
}
#if defined(_MSC_VER)
# define TEST_NOINLINE __declspec(noinline)
#else
# define TEST_NOINLINE __attribute__((noinline))
#endif
TEST_NOINLINE void* operator new(std::size_t size) {
  if(count_allocations) ++allocations;
  if(void* p=std::malloc(size?size:1)) return p;
  throw std::bad_alloc();
}
TEST_NOINLINE void* operator new[](std::size_t size) { return ::operator new(size); }
TEST_NOINLINE void operator delete(void* p) noexcept { std::free(p); }
TEST_NOINLINE void operator delete[](void* p) noexcept { std::free(p); }
TEST_NOINLINE void operator delete(void* p,std::size_t) noexcept { std::free(p); }
TEST_NOINLINE void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

namespace {
using namespace fury;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool same(Vec3 a,Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
bool close(Vec3 a,Vec3 b,float tolerance=1e-5f) { return length(a-b)<tolerance; }
bool mesh_equal(const Mesh& a,const Mesh& b) {
  if(a.indices!=b.indices || a.vertices.size()!=b.vertices.size()) return false;
  for(std::size_t i=0;i<a.vertices.size();++i) {
    const auto& x=a.vertices[i]; const auto& y=b.vertices[i];
    if(!same(x.position,y.position) || !same(x.normal,y.normal) || !same(x.color,y.color) ||
       x.uv.x!=y.uv.x || x.uv.y!=y.uv.y || x.opacity!=y.opacity) return false;
  }
  return true;
}
void check_geometry(const CharacterModel& model) {
  const auto& mesh=model.bind_mesh; const float h=model.rig.height;
  require(!mesh.vertices.empty() && !mesh.indices.empty(),"empty model");
  require(mesh.indices.size()%3==0,"triangle topology");
  require(mesh.indices.size()/3<=2500,"near model exceeds 2500-triangle budget");
  require(mesh.vertices.size()==model.influences.size(),"skin weights missing");
  require(mesh.vertices.size()<2500,"vertex budget");
  float bottom=h,top=-h;
  std::array<bool,character_bone_count> bones{};
  for(std::size_t i=0;i<mesh.vertices.size();++i) {
    const auto& v=mesh.vertices[i]; const auto& w=model.influences[i];
    require(finite(v.position) && finite(v.normal) && finite(v.color),"nonfinite geometry");
    require(std::abs(length(v.normal)-1.f)<1e-4f,"nonunit vertex normal");
    require(std::isfinite(v.uv.x) && std::isfinite(v.uv.y),"invalid UV");
    require(v.color.x>=0 && v.color.x<=1 && v.color.y>=0 && v.color.y<=1 &&
            v.color.z>=0 && v.color.z<=1,"nonphysical vertex color");
    require(w.first<character_bone_count && w.second<character_bone_count &&
            w.first_weight>=0 && w.first_weight<=1,"invalid skin influence");
    bones[w.first]=true;
    require(std::abs(v.position.x)<h*.25f && std::abs(v.position.z)<h*.16f,
            "anatomical rest bounds");
    bottom=std::min(bottom,v.position.y); top=std::max(top,v.position.y);
  }
  require(std::abs(bottom+h*.5f)<1e-6f && std::abs(top-h*.5f)<1e-6f,"height/ground changed");
  for(bool used:bones) require(used,"unused rig joint");
  double volume=0;
  for(std::size_t i=0;i<mesh.indices.size();i+=3) {
    for(int j=0;j<3;++j) require(mesh.indices[i+j]<mesh.vertices.size(),"index out of range");
    const auto& a=mesh.vertices[mesh.indices[i]];
    const auto& b=mesh.vertices[mesh.indices[i+1]];
    const auto& c=mesh.vertices[mesh.indices[i+2]];
    const auto face=cross(b.position-a.position,c.position-a.position);
    require(length(face)>h*h*1e-9f,"degenerate face");
    require(dot(face,a.normal+b.normal+c.normal)>0,"normal/winding disagreement");
    volume+=dot(a.position,cross(b.position,c.position))/6.;
  }
  require(volume>h*h*h*.006 && volume<h*h*h*.05,"closed outward-winding body volume");
  for(std::size_t i=0;i<character_bone_count;++i) {
    require(model.rig.joints[i].parent<int(i),"rig parent must precede child");
    require(finite(model.rig.joints[i].bind_position),"invalid rig position");
  }
  for(const auto foot:{CharacterBone::LeftFoot,CharacterBone::RightFoot}) {
    float sole=h;
    for(std::size_t i=0;i<mesh.vertices.size();++i)
      if(model.influences[i].first==bone_index(foot)) sole=std::min(sole,mesh.vertices[i].position.y);
    require(std::abs(sole+h*.5f)<1e-6f,"foot has no correctly grounded sole");
    require(std::abs(model.rig.joints[bone_index(foot)].bind_position.y+h*.46f)<1e-6f,
            "ankle/sole IK convention changed");
  }
}
// Weld coincident cap/face vertices for each authored component, then require
// a path of touching/overlapping envelopes to the body. This catches floating
// thumbs, cuffs, ears, pouches and LOD pieces without requiring one watertight
// union mesh (layered garments intentionally overlap).
void check_attachments(const CharacterModel& model) {
  const auto& mesh=model.bind_mesh;
  std::vector<std::size_t> parent(mesh.vertices.size());
  std::iota(parent.begin(),parent.end(),0);
  auto root=[&](std::size_t a) {
    while(parent[a]!=a) { parent[a]=parent[parent[a]]; a=parent[a]; } return a;
  };
  auto join=[&](std::size_t a,std::size_t b) { parent[root(a)]=root(b); };
  std::map<std::array<float,3>,std::size_t> welded;
  for(std::size_t i=0;i<mesh.vertices.size();++i) {
    const auto p=mesh.vertices[i].position;
    const auto result=welded.emplace(std::array<float,3>{p.x,p.y,p.z},i);
    if(!result.second) join(i,result.first->second);
  }
  for(std::size_t i=0;i<mesh.indices.size();i+=3) {
    join(mesh.indices[i],mesh.indices[i+1]); join(mesh.indices[i],mesh.indices[i+2]);
  }
  struct Bounds { Vec3 lo{100,100,100},hi{-100,-100,-100}; };
  std::map<std::size_t,Bounds> groups;
  for(std::size_t i=0;i<mesh.vertices.size();++i) {
    auto& box=groups[root(i)]; const auto p=mesh.vertices[i].position;
    box.lo={std::min(box.lo.x,p.x),std::min(box.lo.y,p.y),std::min(box.lo.z,p.z)};
    box.hi={std::max(box.hi.x,p.x),std::max(box.hi.y,p.y),std::max(box.hi.z,p.z)};
  }
  const float tolerance=.003f*model.rig.height;
  auto overlaps=[&](const Bounds& a,const Bounds& b) {
    return a.lo.x<=b.hi.x+tolerance && a.hi.x>=b.lo.x-tolerance &&
           a.lo.y<=b.hi.y+tolerance && a.hi.y>=b.lo.y-tolerance &&
           a.lo.z<=b.hi.z+tolerance && a.hi.z>=b.lo.z-tolerance;
  };
  std::set<std::size_t> attached{groups.begin()->first};
  bool progress=true;
  while(progress) {
    progress=false;
    for(const auto& a:groups) if(!attached.count(a.first))
      for(const auto& b:groups) if(attached.count(b.first) && overlaps(a.second,b.second)) {
        attached.insert(a.first); progress=true; break;
      }
  }
  if(attached.size()!=groups.size()) {
    for(const auto& g:groups) if(!attached.count(g.first))
      std::cerr<<"Detached envelope: "<<character_role_name(model.role)<<" center "
               <<(g.second.lo.x+g.second.hi.x)*.5f<<","<<(g.second.lo.y+g.second.hi.y)*.5f
               <<","<<(g.second.lo.z+g.second.hi.z)*.5f<<'\n';
  }
  require(attached.size()==groups.size(),"disconnected anatomy/clothing attachment envelope");
  const auto& r=model.rig; const float h=r.height;
  for(int side=0;side<2;++side) {
    const auto thigh=side?CharacterBone::RightThigh:CharacterBone::LeftThigh;
    const auto shin=side?CharacterBone::RightShin:CharacterBone::LeftShin;
    const auto foot=side?CharacterBone::RightFoot:CharacterBone::LeftFoot;
    const auto upper=side?CharacterBone::RightUpperArm:CharacterBone::LeftUpperArm;
    const auto forearm=side?CharacterBone::RightForearm:CharacterBone::LeftForearm;
    const auto hand=side?CharacterBone::RightHand:CharacterBone::LeftHand;
    auto distance=[&](CharacterBone a,CharacterBone b) {
      return length(r.joints[bone_index(a)].bind_position-r.joints[bone_index(b)].bind_position)/h;
    };
    require(std::abs(distance(thigh,shin)-.225f)<1e-6f && std::abs(distance(shin,foot)-.235f)<1e-6f,
            "bind leg lengths changed");
    require(std::abs(distance(upper,forearm)-std::sqrt(.135f*.135f+.024f*.024f))<1e-6f &&
            std::abs(distance(forearm,hand)-std::sqrt(.138f*.138f+.015f*.015f))<1e-6f,
            "bind arm lengths changed");
  }
}
void facial_depth(const CharacterModel& model) {
  const auto& mesh=model.bind_mesh;
  // Intersect an independent XY ray against the actual triangulated skin.
  // This tests real surface depth, not the feature-placement approximation.
  auto front=[&](Vec3 p) {
    float result=-100.f;
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
      const auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];
      const auto& va=mesh.vertices[ia]; const auto& vb=mesh.vertices[ib]; const auto& vc=mesh.vertices[ic];
      if(model.influences[ia].first!=bone_index(CharacterBone::Head) ||
         !same(va.color,model.appearance.skin) || !same(vb.color,model.appearance.skin) ||
         !same(vc.color,model.appearance.skin)) continue;
      const auto a=va.position,b=vb.position,c=vc.position;
      const float denominator=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
      if(std::abs(denominator)<1e-10f) continue;
      const float u=((b.y-c.y)*(p.x-c.x)+(c.x-b.x)*(p.y-c.y))/denominator;
      const float v=((c.y-a.y)*(p.x-c.x)+(a.x-c.x)*(p.y-c.y))/denominator;
      if(u>=-1e-5f && v>=-1e-5f && u+v<=1.f+1e-5f)
        result=std::max(result,u*a.z+v*b.z+(1.f-u-v)*c.z);
    }
    return result;
  };
  float max_eye=-1,max_nose=-1; int eyes=0,nose=0;
  for(std::size_t i=0;i<mesh.vertices.size();++i) {
    const auto& v=mesh.vertices[i];
    if(model.influences[i].first!=bone_index(CharacterBone::Head)) continue;
    const bool eye=same(v.color,{.66f,.65f,.60f}) || same(v.color,model.appearance.eyes);
    const bool nasal=same(v.color,model.appearance.skin*.98f);
    if(!eye && !nasal) continue;
    const float surface=front(v.position);
    require(surface>-1.f,"facial detail must have skin directly behind it");
    const float depth=(v.position.z-surface)/model.rig.height;
    if(eye) { max_eye=std::max(max_eye,depth); ++eyes; }
    if(nasal) { max_nose=std::max(max_nose,depth); ++nose; }
  }
  require(eyes>0 && max_eye>.0003f && max_eye<.0025f,"eyes must remain shallow within eyelids");
  require(nose>0 && max_nose>.006f && max_nose<.014f,"nose tip anatomical projection");
}
void profiles_and_geometry() {
  require(static_cast<int>(CharacterRole::Count)==11,"character role coverage changed");
  require(std::string(character_role_name(CharacterRole::BankStaff))=="bank_staff","bank-staff label");
  require(character_seed("foo")==0xa9f37ed7u,"portable FNV-1a hash changed");
  std::set<std::array<float,3>> skins,hairs,shirts;
  std::set<float> shoulders; std::set<std::uint8_t> hairstyles;
  std::size_t max_near=0,max_far=0;
  for(unsigned id=0;id<32;++id) {
    const auto seed=character_seed("citizen_"+std::to_string(id));
    const auto appearance=character_appearance(seed);
    skins.insert({appearance.skin.x,appearance.skin.y,appearance.skin.z});
    hairs.insert({appearance.hair.x,appearance.hair.y,appearance.hair.z});
    shirts.insert({appearance.shirt.x,appearance.shirt.y,appearance.shirt.z});
    shoulders.insert(appearance.shoulder_scale); hairstyles.insert(appearance.hair_style);
    for(int r=0;r<int(CharacterRole::Count);++r) {
      const auto role=static_cast<CharacterRole>(r);
      const float h=id%3==0?1.61f:(id%3==1?1.76f:1.91f);
      const auto near=make_character_model(h,role,seed);
      const auto far=make_character_model(h,role,seed,CharacterLod::Far);
      check_geometry(near); check_geometry(far);
      check_attachments(near); check_attachments(far);
      if(role==CharacterRole::Commuter) { facial_depth(near); facial_depth(far); }
      require(near.rig.height==h && far.rig.height==h,"requested actor height clamped");
      require(same(near.appearance.skin,appearance.skin) && same(near.appearance.hair,appearance.hair)
          && near.appearance.shoulder_scale==appearance.shoulder_scale,"occupation changed appearance");
      require(same(far.appearance.skin,near.appearance.skin),"LOD changes identity");
      require(far.bind_mesh.indices.size()<near.bind_mesh.indices.size()*.57f,"far LOD savings lost");
      for(std::size_t i=0;i<character_bone_count;++i)
        require(same(near.rig.joints[i].bind_position,far.rig.joints[i].bind_position),"LOD changes rig");
      max_near=std::max(max_near,near.bind_mesh.indices.size()/3);
      max_far=std::max(max_far,far.bind_mesh.indices.size()/3);
    }
  }
  require(skins.size()>=6 && hairs.size()>=5 && shirts.size()>=6 && shoulders.size()>=12 && hairstyles.size()==4,
          "stable population has too little useful variation");
  const auto teller=make_character_model(1.8f,CharacterRole::BankStaff,18273);
  for(std::size_t i=0;i<teller.bind_mesh.vertices.size();++i) {
    const auto& v=teller.bind_mesh.vertices[i];
    if(teller.influences[i].first==bone_index(CharacterBone::Pelvis))
      require(std::abs(v.position.x)<.12f*teller.rig.height,"bank staff must not carry a commuter satchel");
  }
  const auto a=make_character_model(1.8f,CharacterRole::Commuter,18273);
  const auto b=make_character_model(1.8f,CharacterRole::Commuter,18273);
  require(mesh_equal(a.bind_mesh,b.bind_mesh),"deterministic model mismatch");
  require(a.bind_mesh.geometry_identity!=b.bind_mesh.geometry_identity,"separate instances share geometry identity");
  for(int i=1;i<int(CharacterRole::Count);++i) {
    const auto c=make_character_model(1.8f,static_cast<CharacterRole>(i),18273);
    require(!mesh_equal(a.bind_mesh,c.bind_mesh),"role is only a repeated mannequin");
  }
  std::cout<<"Character geometry: 704 role/identity/LOD combinations; max "<<max_near
           <<" near / "<<max_far<<" far triangles\n";
}
void deformation() {
  const auto model=make_character_model(1.76f,CharacterRole::CrewTech,character_seed("crew-tech"));
  Mesh output=model.bind_mesh;
  auto* vertices=output.vertices.data(); auto* indices=output.indices.data();
  const auto identity=output.geometry_identity; const auto topology=output.indices;
  const auto vertex_capacity=output.vertices.capacity(),index_capacity=output.indices.capacity();
  output.gpu_vao=31; output.gpu_vbo=32; output.gpu_ibo=33; output.gpu_uploaded=true; output.gpu_dirty=false;
  require(!apply_character_pose(model,CharacterPose{},output),"identity pose needlessly marks dirty");
  require(!output.gpu_dirty && output.geometry_revision==0,"idle revision changed");
  std::array<Vec3,character_bone_count> rotations{};
  auto pose=make_character_pose(model.rig,rotations,{.03f,.02f,-.05f});
  require(apply_character_pose(model,pose,output),"translated pose did not change");
  for(std::size_t i=0;i<output.vertices.size();++i)
    require(close(output.vertices[i].position,model.bind_mesh.vertices[i].position+Vec3{.03f,.02f,-.05f}),
            "root translation is not uniform");
  const auto revision=output.geometry_revision;
  require(!apply_character_pose(model,pose,output) && revision==output.geometry_revision,
          "identical pose dirties buffers");
  count_allocations=true; allocations=0;
  for(int frame=0;frame<180;++frame) {
    const float phase=float(frame)*.09f;
    rotations[bone_index(CharacterBone::LeftUpperArm)]={.48f*std::sin(phase),0,-.12f};
    rotations[bone_index(CharacterBone::LeftForearm)]={-.37f,0,0};
    rotations[bone_index(CharacterBone::Head)]={0,.23f*std::sin(phase*.3f),0};
    pose=make_character_pose(model.rig,rotations);
    apply_character_pose(model,pose,output);
  }
  count_allocations=false;
  require(allocations==0,"per-frame CPU skinning allocated memory");
  require(output.geometry_identity==identity && output.vertices.data()==vertices && output.indices.data()==indices,
          "pose replaced stable geometry/buffers");
  require(output.vertices.capacity()==vertex_capacity && output.indices.capacity()==index_capacity &&
          output.indices==topology,"pose rebuilt topology");
  require(output.gpu_vao==31 && output.gpu_vbo==32 && output.gpu_ibo==33 && output.gpu_uploaded,
          "pose discarded renderer handles");
  bool moved_left=false;
  for(std::size_t i=0;i<output.vertices.size();++i) {
    const auto& v=output.vertices[i]; const auto& original=model.bind_mesh.vertices[i];
    require(finite(v.position) && std::abs(length(v.normal)-1.f)<1e-4f,"skin normal/position invalid");
    require(same(v.color,original.color) && v.uv.x==original.uv.x && v.uv.y==original.uv.y,
            "skinning changed albedo/UV");
    if(model.influences[i].first==bone_index(CharacterBone::LeftHand))
      moved_left |= length(v.position-original.position)>.03f;
    if(model.influences[i].first==bone_index(CharacterBone::RightFoot))
      require(close(v.position,original.position),"arm pose moved unrelated foot");
  }
  require(moved_left,"joint hierarchy failed to articulate hand");
  apply_character_pose(model,CharacterPose{},output);
  for(std::size_t i=0;i<output.vertices.size();++i)
    require(close(output.vertices[i].position,model.bind_mesh.vertices[i].position),"bind reset drifted");
  std::cout<<"Character skinning: 180 frames, zero allocations, stable topology/identity/handles\n";
}
void invalid_inputs() {
  for(float h:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
    bool failed=false; try { make_character_model(h,CharacterRole::Player,0); } catch(const std::invalid_argument&) { failed=true; }
    require(failed,"invalid height accepted");
  }
  bool failed=false;
  try { make_character_model(1.7f,CharacterRole::Count,0); } catch(const std::invalid_argument&) { failed=true; }
  require(failed,"invalid role accepted");
  failed=false;
  try { make_character_model(1.7f,CharacterRole::Player,0,static_cast<CharacterLod>(99)); }
  catch(const std::invalid_argument&) { failed=true; }
  require(failed,"invalid LOD accepted");
  auto model=make_character_model(1.7f,CharacterRole::Player,0); Mesh output=model.bind_mesh;
  failed=false;
  try { apply_character_pose(model,CharacterPose{},model.bind_mesh); } catch(const std::invalid_argument&) { failed=true; }
  require(failed,"bind mesh may not be its own mutable output");
  CharacterPose pose; pose.skin_matrices[0].m[0]=std::numeric_limits<float>::quiet_NaN();
  failed=false;
  try { apply_character_pose(model,pose,output); } catch(const std::invalid_argument&) { failed=true; }
  require(failed && mesh_equal(model.bind_mesh,output),"invalid pose partially mutated mesh");
  output.vertices.pop_back(); failed=false;
  try { apply_character_pose(model,CharacterPose{},output); } catch(const std::invalid_argument&) { failed=true; }
  require(failed,"wrong output mesh accepted");
}
}
int main() {
  try { profiles_and_geometry(); deformation(); invalid_inputs(); }
  catch(const std::exception& error) { count_allocations=false; std::cerr<<"Character model tests: "<<error.what()<<'\n'; return 1; }
  std::cout<<"Character model tests passed\n"; return 0;
}
