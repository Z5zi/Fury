#include "fury/character_model.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

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
bool close(Vec3 a,Vec3 b,float tolerance=2e-5f) { return length(a-b)<=tolerance; }
bool same(Vec3 a,Vec3 b) {
  return std::abs(a.x-b.x)<=1e-6f && std::abs(a.y-b.y)<=1e-6f && std::abs(a.z-b.z)<=1e-6f;
}
bool same_vertices(const Mesh& a,const Mesh& b) {
  return a.vertices.size()==b.vertices.size() &&
    std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Vertex))==0;
}
CharacterModel fixture(CharacterInfluence influence,bool dq=true) {
  CharacterModel model;
  model.dual_quaternion_skinning=dq;
  model.bind_mesh.vertices={{{1.f,.5f,-.25f},{0,1,0},{.1f,.2f,.3f},{.25f,.75f},.6f},
                            {{0,1,0},{1,0,0},{.3f,.2f,.1f},{1,0},.8f},
                            {{0,0,1},{0,0,1},{.4f,.5f,.6f},{0,1},1.f}};
  model.bind_mesh.indices={0,1,2};
  model.influences.assign(model.bind_mesh.vertices.size(),influence);
  return model;
}
Mat4 pivot_rotation(Vec3 pivot,float angle,int axis) {
  const auto rotation=axis==0?rotate_x(angle):(axis==1?rotate_y(angle):rotate_z(angle));
  return translate(pivot)*rotation*translate(-pivot);
}
void expect_transform(const CharacterModel& model,const CharacterPose& pose,const Mat4& expected,
                      const char* message) {
  Mesh output=model.bind_mesh;
  apply_character_pose(model,pose,output);
  for(std::size_t i=0;i<output.vertices.size();++i) {
    const auto& source=model.bind_mesh.vertices[i];
    require(close(output.vertices[i].position,transform_point(expected,source.position)),message);
    require(close(output.vertices[i].normal,normalize(transform_direction(expected,source.normal))),
            "dual-quaternion normal disagrees with known rigid transform");
    require(std::abs(length(output.vertices[i].normal)-1.f)<2e-6f,"normal lost unit length");
  }
}

// Frozen pre-extension deformation arithmetic. Compare bytes rather than an
// approximate mathematical expectation to catch changed legacy operation order.
void legacy_pose(const CharacterModel& model,const CharacterPose& pose,Mesh& output) {
  for(std::size_t i=0;i<output.vertices.size();++i) {
    const auto& source=model.bind_mesh.vertices[i]; const auto& influence=model.influences[i];
    const auto& a=pose.skin_matrices[influence.first];
    Vec3 position=transform_point(a,source.position),normal=transform_direction(a,source.normal);
    if(influence.first!=influence.second && influence.first_weight<1.f) {
      const auto& b=pose.skin_matrices[influence.second]; const float w=influence.first_weight;
      position=position*w+transform_point(b,source.position)*(1.f-w);
      normal=normal*w+transform_direction(b,source.normal)*(1.f-w);
    }
    normal=normalize(normal);
    if(!same(output.vertices[i].position,position) || !same(output.vertices[i].normal,normal)) {
      output.vertices[i].position=position; output.vertices[i].normal=normal;
    }
  }
}
void legacy_compatibility() {
  const CharacterInfluence aggregate{1,2,.35f};
  require(aggregate.third==0 && aggregate.third_weight==0.f,"old aggregate initializer changed");
  for(int role=0;role<int(CharacterRole::Count);++role) for(auto lod:{CharacterLod::Near,CharacterLod::Far}) {
    auto model=make_character_model(1.81f,static_cast<CharacterRole>(role),2047,lod);
    require(!model.dual_quaternion_skinning,"generic model opted into DQ unexpectedly");
    Mesh actual=model.bind_mesh,expected=model.bind_mesh;
    for(int frame=0;frame<12;++frame) {
      std::array<Vec3,character_bone_count> rotations{};
      for(std::size_t bone=0;bone<rotations.size();++bone) {
        const float phase=float(frame)*.2f+float(bone)*.13f;
        rotations[bone]={.7f*std::sin(phase),.2f*std::cos(phase),-.3f*std::sin(phase*.7f)};
      }
      const auto pose=make_character_pose(model.rig,rotations,{.04f,.1f,-.2f});
      legacy_pose(model,pose,expected);
      apply_character_pose(model,pose,actual);
      require(same_vertices(actual,expected),"two-bone LBS changed vertex bytes");
    }
  }
  for(const auto influence:{CharacterInfluence{0,0,.35f},CharacterInfluence{0,1,1.f},
                            CharacterInfluence{0,1,0.f}}) {
    auto model=fixture(influence);
    CharacterPose pose;
    pose.skin_matrices[0]=translate({.15f,-.3f,.12f})*rotate_x(.45f)*rotate_y(-.72f);
    pose.skin_matrices[1]=translate({-.6f,.7f,-.1f})*rotate_z(.83f);
    Mesh actual=model.bind_mesh,expected=model.bind_mesh;
    legacy_pose(model,pose,expected);
    apply_character_pose(model,pose,actual);
    require(same_vertices(actual,expected),"single-bone DQ path changed legacy bytes");
  }
}
void rigid_interpolation() {
  CharacterPose pose;
  auto model=fixture({0,1,.5f});
  const Vec3 pivot{.7f,-.35f,.2f};
  pose.skin_matrices[0]=pivot_rotation(pivot,0.f,2);
  pose.skin_matrices[1]=pivot_rotation(pivot,radians(120.f),2);
  expect_transform(model,pose,pivot_rotation(pivot,radians(60.f),2),"two-bone midpoint incorrect");
  Mesh dq=model.bind_mesh;
  apply_character_pose(model,pose,dq);
  model.dual_quaternion_skinning=false;
  Mesh lbs=model.bind_mesh;
  apply_character_pose(model,pose,lbs);
  const Vec3 before=model.bind_mesh.vertices[0].position-pivot;
  const Vec3 after=dq.vertices[0].position-pivot;
  const Vec3 collapsed=lbs.vertices[0].position-pivot;
  const float radius=std::hypot(before.x,before.y);
  require(std::abs(std::hypot(after.x,after.y)-radius)<2e-6f,"DQ midpoint collapsed rotation radius");
  require(std::abs(std::hypot(collapsed.x,collapsed.y)-radius*.5f)<2e-6f,
          "volume fixture no longer exposes the expected LBS collapse");

  model=fixture({0,1,.25f,2,.25f});
  pose.skin_matrices[0]=pivot_rotation(pivot,radians(-60.f),1);
  pose.skin_matrices[1]=pivot_rotation(pivot,0.f,1);
  pose.skin_matrices[2]=pivot_rotation(pivot,radians(60.f),1);
  expect_transform(model,pose,Mat4::identity(),"symmetric three-bone rotation is not identity");

  // Identical orientation and differing translations have an exact weighted
  // translation expectation, independent of quaternion normalization details.
  const Mat4 rotation=rotate_x(.63f)*rotate_y(-.91f)*rotate_z(.27f);
  const Vec3 a{.8f,-.4f,1.1f},b{-.2f,.7f,.3f},c{.5f,.1f,-.6f};
  pose.skin_matrices[0]=translate(a)*rotation;
  pose.skin_matrices[1]=translate(b)*rotation;
  pose.skin_matrices[2]=translate(c)*rotation;
  const auto expected=translate(a*.25f+b*.5f+c*.25f)*rotation;
  expect_transform(model,pose,expected,"three-bone translation/rotation interpolation incorrect");
  model.dual_quaternion_skinning=false;
  expect_transform(model,pose,expected,"three-bone LBS residual second weight incorrect");

  model=fixture({0,1,.25f});
  expect_transform(model,pose,translate(a*.25f+b*.75f)*rotation,
                   "two-bone translation/rotation interpolation incorrect");
  model=fixture({0,1,.8f,2,.2f});
  expect_transform(model,pose,translate(a*.8f+c*.2f)*rotation,
                   "normalized weights at floating-point boundary rejected");
  model=fixture({0,1,0.f,2,1.f});
  expect_transform(model,pose,pose.skin_matrices[2],"third-only vertex used the wrong bone");
  model=fixture({2,2,.25f,2,.25f});
  expect_transform(model,pose,pose.skin_matrices[2],"duplicate three-bone indices did not remain rigid");
  model=fixture({0,1,0.f,2,.5f});
  pose.skin_matrices[0]=rotate_x(radians(180.f)); // Inactive reference must be ignored.
  pose.skin_matrices[1]=pivot_rotation(pivot,radians(-40.f),2);
  pose.skin_matrices[2]=pivot_rotation(pivot,radians(40.f),2);
  expect_transform(model,pose,Mat4::identity(),"zero first weight corrupted the hemisphere reference");
}
void quaternion_branches() {
  auto model=fixture({0,1,.5f});
  const Vec3 pivot{.4f,.1f,-.2f};
  for(int axis=0;axis<3;++axis) {
    CharacterPose pose;
    pose.skin_matrices[0]=pivot_rotation(pivot,radians(179.f),axis);
    pose.skin_matrices[1]=pivot_rotation(pivot,radians(-179.f),axis);
    expect_transform(model,pose,pivot_rotation(pivot,radians(180.f),axis),
                     "near-180-degree rotation took the long way around");
    // Across the trace/diagonal conversion boundary, +/-119-degree rotations
    // produce opposite quaternion hemispheres and must still pass through 180.
    pose.skin_matrices[0]=pivot_rotation(pivot,radians(119.f),axis);
    pose.skin_matrices[1]=pivot_rotation(pivot,radians(-119.f),axis);
    expect_transform(model,pose,pivot_rotation(pivot,radians(180.f),axis),
                     "opposite quaternion hemispheres cancelled");
    pose.skin_matrices[0]=pivot_rotation(pivot,radians(180.f),axis);
    pose.skin_matrices[1]=pose.skin_matrices[0];
    expect_transform(model,pose,pose.skin_matrices[0],"180-degree diagonal conversion failed");
    // Exercise off-diagonal entries in each largest-diagonal conversion branch,
    // rather than only rotations whose quaternion has one nonzero vector axis.
    const auto tilted=pivot_rotation(pivot,radians(179.f),axis)*rotate_y(.37f)*rotate_z(-.23f)*rotate_x(.19f);
    pose.skin_matrices[0]=tilted; pose.skin_matrices[1]=tilted;
    expect_transform(model,pose,tilted,"tilted near-180-degree diagonal conversion failed");
  }
}
void storage_and_dirty_state() {
  auto model=make_character_model(1.75f,CharacterRole::Market,1937);
  model.dual_quaternion_skinning=true;
  // Exercise true three-bone blends on a full mesh during allocation checks.
  for(std::size_t i=0;i<model.influences.size();i+=5) model.influences[i]={0,1,.3f,2,.2f};
  Mesh output=model.bind_mesh;
  const auto* vertex_storage=output.vertices.data();
  const auto* index_storage=output.indices.data();
  const auto vertex_capacity=output.vertices.capacity(),index_capacity=output.indices.capacity();
  const auto identity=output.geometry_identity;
  output.gpu_vao=7; output.gpu_vbo=8; output.gpu_ibo=9; output.gpu_uploaded=true;
  output.gpu_dirty=false;
  require(!apply_character_pose(model,CharacterPose{},output),"DQ bind identity dirties output");
  require(same_vertices(output,model.bind_mesh),"DQ bind identity changes vertex bytes");
  std::array<Vec3,character_bone_count> rotations{};
  count_allocations=true; allocations=0;
  for(int frame=0;frame<180;++frame) {
    for(std::size_t bone=0;bone<rotations.size();++bone)
      rotations[bone]={.45f*std::sin(float(frame)*.09f+float(bone)),.12f,-.07f};
    apply_character_pose(model,make_character_pose(model.rig,rotations,{.01f,.02f,.03f}),output);
  }
  count_allocations=false;
  require(allocations==0,"DQ skinning allocated per frame");
  require(output.geometry_revision==180 && output.gpu_dirty,"DQ did not mark each changed pose once");
  output.gpu_dirty=false;
  require(!apply_character_pose(model,make_character_pose(model.rig,rotations,{.01f,.02f,.03f}),output) &&
          output.geometry_revision==180 && !output.gpu_dirty,"repeated DQ pose needlessly dirties output");
  require(output.vertices.data()==vertex_storage && output.indices.data()==index_storage &&
          output.vertices.capacity()==vertex_capacity && output.indices.capacity()==index_capacity &&
          output.geometry_identity==identity && output.indices==model.bind_mesh.indices,
          "DQ changed mesh storage, identity, or topology");
  require(output.gpu_vao==7 && output.gpu_vbo==8 && output.gpu_ibo==9 && output.gpu_uploaded,
          "DQ discarded GPU handles");
  for(std::size_t i=0;i<output.vertices.size();++i) {
    const auto& a=output.vertices[i]; const auto& b=model.bind_mesh.vertices[i];
    require(std::memcmp(&a.color,&b.color,sizeof(Vec3))==0 &&
            std::memcmp(&a.uv,&b.uv,sizeof(Vec2))==0 && a.opacity==b.opacity,
            "DQ altered color, UV, or opacity");
    require(std::isfinite(length(a.position)) && std::abs(length(a.normal)-1.f)<2e-6f,
            "DQ produced invalid output geometry");
  }
  require(apply_character_pose(model,CharacterPose{},output),"DQ bind reset did not change output");
  for(std::size_t i=0;i<output.vertices.size();++i)
    require(close(output.vertices[i].position,model.bind_mesh.vertices[i].position),"DQ bind reset drifted");
}
// Hard-edge/UV corners can share a position and weights but have different
// normals. A normal refresh must never decide whether a shared position moves.
void split_corner_cache_regression() {
  const CharacterInfluence influences[]={{0,0,1.f},{0,1,.35f},{0,1,.3f,2,.2f}};
  for(bool dq:{false,true}) for(const auto influence:influences) for(bool reverse:{false,true}) {
    auto model=fixture(influence,dq);
    model.bind_mesh.vertices[0].position=model.bind_mesh.vertices[1].position={.125f,0,0};
    model.bind_mesh.vertices[0].normal=reverse?Vec3{0,1,0}:Vec3{1,0,0};
    model.bind_mesh.vertices[1].normal=reverse?Vec3{1,0,0}:Vec3{0,1,0};
    Mesh output=model.bind_mesh;
    const auto* storage=output.vertices.data();
    const auto identity=output.geometry_identity;
    output.gpu_dirty=false;
    CharacterPose pose;
    auto set_pose=[&](float translation,float angle) {
      for(auto& matrix:pose.skin_matrices) matrix=translate({translation,0,0})*rotate_x(angle);
    };
    auto check_seam=[&] {
      require(std::memcmp(&output.vertices[0].position,&output.vertices[1].position,sizeof(Vec3))==0,
              "normal-only refresh opened a split-corner seam");
    };
    set_pose(.5e-6f,0.f);
    require(!apply_character_pose(model,pose,output) && same_vertices(output,model.bind_mesh) &&
            output.geometry_revision==0 && !output.gpu_dirty,"sub-epsilon pose bypassed the cache");
    // The x-normal is unchanged; the y-normal rotates beyond the cache epsilon.
    // Position moves less than that epsilon, reproducing the original failure.
    set_pose(.5e-6f,2e-4f);
    require(apply_character_pose(model,pose,output),"normal-only change did not dirty the mesh");
    check_seam();
    for(unsigned i=0;i<2;++i)
      require(std::memcmp(&output.vertices[i].position,&model.bind_mesh.vertices[i].position,sizeof(Vec3))==0,
              "normal refresh bypassed the independent position cache");
    const unsigned rotating=reverse?0:1;
    require(length(output.vertices[rotating].normal-model.bind_mesh.vertices[rotating].normal)>1e-4f,
            "rotating hard-edge normal was not refreshed");
    require(output.geometry_revision==1 && output.gpu_dirty,"normal-only refresh did not mark dirty exactly once");
    const Mesh repeated=output;
    output.gpu_dirty=false;
    require(!apply_character_pose(model,pose,output) && same_vertices(output,repeated) &&
            output.geometry_revision==1 && !output.gpu_dirty,"repeated split-corner pose dirtied or changed output");
    // Accumulated movement crosses the position epsilon while normals stay put.
    set_pose(2e-6f,2e-4f);
    require(apply_character_pose(model,pose,output),"position-only change did not dirty the mesh");
    check_seam();
    require(output.vertices[0].position.x-model.bind_mesh.vertices[0].position.x>1e-6f &&
            output.geometry_revision==2,"position cache did not refresh accumulated motion");
    for(unsigned i=0;i<2;++i)
      require(std::memcmp(&output.vertices[i].normal,&repeated.vertices[i].normal,sizeof(Vec3))==0,
              "position-only refresh changed an unchanged normal");
    // More changing normals plus sub-epsilon translation must keep both copies
    // on the same cached position through repeated and reversed pose histories.
    for(int frame=0;frame<32;++frame) {
      set_pose(2e-6f+float(frame%5)*.4e-6f,(frame%2?1.f:-1.f)*(float(frame)+1.f)*2e-4f);
      apply_character_pose(model,pose,output);
      check_seam();
    }
    require(apply_character_pose(model,CharacterPose{},output),"split-corner bind reset was ignored");
    check_seam();
    require(output.vertices.data()==storage && output.geometry_identity==identity &&
            output.indices==model.bind_mesh.indices,"split-corner cache replaced storage or topology");
    for(unsigned i=0;i<output.vertices.size();++i) {
      const auto& actual=output.vertices[i]; const auto& source=model.bind_mesh.vertices[i];
      require(std::memcmp(&actual.color,&source.color,sizeof(Vec3))==0 &&
              std::memcmp(&actual.uv,&source.uv,sizeof(Vec2))==0 && actual.opacity==source.opacity,
              "split-corner cache changed non-pose attributes");
    }
  }
}
void expect_rejected(const CharacterModel& model,const CharacterPose& pose,const char* message) {
  Mesh output=model.bind_mesh;
  output.gpu_dirty=false;
  bool rejected=false;
  try { apply_character_pose(model,pose,output); }
  catch(const std::invalid_argument&) { rejected=true; }
  require(rejected,message);
  require(same_vertices(output,model.bind_mesh) && !output.gpu_dirty && output.geometry_revision==0,
          "rejected input partially mutated output");
}
void deformed_normal_regions() {
  auto model=fixture({0,1,.5f});
  CharacterDeformedNormalRegion region;
  region.vertex_begin=0;region.vertex_count=3;
  region.member_offsets={0,1,2,3};region.members={0,1,2};
  region.triangle_offsets={0,1,2,3};region.triangles={0,0,0};
  model.deformed_normal_regions.push_back(region);
  CharacterPose pose;pose.skin_matrices[0]=rotate_y(.31f);pose.skin_matrices[1]=rotate_x(-.22f);
  Mesh output=model.bind_mesh;const auto* storage=output.vertices.data();
  count_allocations=true;allocations=0;apply_character_pose(model,pose,output);count_allocations=false;
  require(allocations==0&&output.vertices.data()==storage,"deformed normal rebuild allocates or replaces storage");
  const auto normal=normalize(cross(output.vertices[1].position-output.vertices[0].position,output.vertices[2].position-output.vertices[0].position));
  for(auto&v:output.vertices)require(close(v.normal,normal),"deformed normals disagree with actual triangle geometry");
  const auto rev=output.geometry_revision;
  require(!apply_character_pose(model,pose,output)&&output.geometry_revision==rev,"normal reconstruction dirties an exact repeated pose");
  for(unsigned i=0;i<3;++i)require(std::memcmp(&output.vertices[i].uv,&model.bind_mesh.vertices[i].uv,sizeof(Vec2))==0&&std::memcmp(&output.vertices[i].color,&model.bind_mesh.vertices[i].color,sizeof(Vec3))==0,"normal reconstruction alters material bytes");
  for(int fault=0;fault<8;++fault){auto invalid=model;auto&r=invalid.deformed_normal_regions[0];
    if(fault==0)r.vertex_count=4;if(fault==1)r.member_offsets[1]=5;if(fault==2)r.members[2]=7;
    if(fault==3)r.triangles[2]=1;if(fault==4)r.triangle_offsets[2]=0;if(fault==5)r.triangle_offsets.pop_back();if(fault==6)r.vertex_begin=2;
    if(fault==7)r.members[2]=r.members[1]; // Duplicate across groups also omits vertex 2.
    expect_rejected(invalid,pose,"malformed normal reconstruction metadata accepted");
  }
}
void deformed_normal_partition_boundaries() {
  CharacterPose pose;pose.skin_matrices[0]=rotate_y(.31f);pose.skin_matrices[1]=rotate_x(-.22f);
  // Nonzero base and reverse membership order exercise relative chunk bounds,
  // without relying on sorted members or probabilistic sum/XOR checks.
  for(std::uint32_t count:{4096u,4097u,8193u}) {
    auto model=fixture({0,1,.5f});const auto original=model.bind_mesh.vertices;
    constexpr std::uint32_t begin=2;
    model.bind_mesh.vertices.resize(begin+count,original[0]);
    for(unsigned i=0;i<3;++i)model.bind_mesh.vertices[begin+i]=original[i];
    model.bind_mesh.indices={begin,begin+1,begin+2};
    model.influences.assign(model.bind_mesh.vertices.size(),{0,1,.5f});
    CharacterDeformedNormalRegion region;
    region.vertex_begin=begin;region.vertex_count=count;
    region.member_offsets={0,count};region.triangle_offsets={0,1};region.triangles={0};
    for(std::uint32_t i=0;i<count;++i)region.members.push_back(begin+count-1-i);
    model.deformed_normal_regions.push_back(region);
    Mesh output=model.bind_mesh;const auto* storage=output.vertices.data();
    count_allocations=true;allocations=0;apply_character_pose(model,pose,output);count_allocations=false;
    require(allocations==0&&output.vertices.data()==storage,"Chunked partition validation allocates or replaces storage");
    const auto revision=output.geometry_revision;
    require(!apply_character_pose(model,pose,output)&&output.geometry_revision==revision,"Chunked partition validation changes exact repeated-pose behavior");
    for(std::uint32_t at:{0u,4095u,4096u,count-1}) {
      if(at>=count)continue;
      auto invalid=model;auto&members=invalid.deformed_normal_regions[0].members;
      members[at]=members[(at+1)%count];
      expect_rejected(invalid,pose,"Duplicate/omitted membership across a bitmap boundary was accepted");
    }
    auto invalid=model;invalid.deformed_normal_regions[0].members.back()=begin-1;
    expect_rejected(invalid,pose,"Late out-of-range member passed partition validation");
  }
}
void invalid_inputs() {
  const float nan=std::numeric_limits<float>::quiet_NaN();
  const float infinity=std::numeric_limits<float>::infinity();
  const auto out_of_range=static_cast<std::uint8_t>(character_bone_count);
  const CharacterInfluence invalid[]={
    {out_of_range,1,.5f},{0,out_of_range,.5f},{0,1,.5f,out_of_range,0.f},
    {0,1,-.01f},{0,1,1.01f},{0,1,nan},{0,1,infinity},
    {0,1,.5f,2,-.01f},{0,1,.5f,2,nan},{0,1,.5f,2,infinity},{0,1,.75f,2,.26f}};
  for(bool dq:{false,true}) for(const auto influence:invalid) {
    auto model=fixture({0,1,.5f},dq);
    model.influences.back()=influence; // Late invalid vertex must not permit early writes.
    CharacterPose pose; pose.skin_matrices[0]=translate({1,0,0});
    expect_rejected(model,pose,"invalid influence accepted");
  }
  auto model=fixture({0,1,.5f});
  for(int mode=0;mode<8;++mode) {
    CharacterPose pose;
    auto& m=pose.skin_matrices.back(); // Even an unused invalid bone is rejected.
    if(mode==0) m=scale({1.02f,1,1});
    if(mode==1) m.at(1,0)=.01f;
    if(mode==2) m=scale({-1,1,1});
    if(mode==3) m.at(0,3)=.01f;
    if(mode==4) m.at(3,3)=2.f;
    if(mode==5) m.at(0,0)=nan;
    if(mode==6) m.at(3,0)=infinity;
    if(mode==7) m=scale({0,0,0});
    expect_rejected(model,pose,"invalid or non-rigid DQ matrix accepted");
  }
  // The opt-in restriction must not change the historical finite-matrix LBS contract.
  model.dual_quaternion_skinning=false;
  CharacterPose pose;
  pose.skin_matrices[0]=scale({2.f,-.5f,1.1f}); pose.skin_matrices[0].at(1,0)=.15f;
  Mesh output=model.bind_mesh,expected=model.bind_mesh;
  legacy_pose(model,pose,expected);
  apply_character_pose(model,pose,output);
  require(same_vertices(output,expected),"DQ validation changed legacy non-rigid behavior");
}
}
int main() {
  try {
    legacy_compatibility(); rigid_interpolation(); quaternion_branches();
    storage_and_dirty_state(); split_corner_cache_regression(); deformed_normal_regions(); deformed_normal_partition_boundaries(); invalid_inputs();
  } catch(const std::exception& error) {
    count_allocations=false;
    std::cerr<<"Character skinning tests: "<<error.what()<<'\n'; return 1;
  }
  std::cout<<"Character skinning tests passed: legacy bytes, rigid DQ interpolation, "
              "three weights, split-corner cache coherence, exact chunked partitions, validation, and 180 allocation-free frames\n";
  return 0;
}
