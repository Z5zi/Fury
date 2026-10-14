#include "fury/character_model.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fury {
namespace {
constexpr float pi = 3.14159265358979323846f;
const Vec3 dark{.055f,.065f,.075f};
const Vec3 stitching{.63f,.62f,.54f};
using Bone = CharacterBone;

std::uint32_t mix(std::uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
  x *= 0x846ca68bu; return x ^ (x >> 16);
}
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool same(Vec3 a, Vec3 b) {
  return std::abs(a.x-b.x)<=1e-6f && std::abs(a.y-b.y)<=1e-6f && std::abs(a.z-b.z)<=1e-6f;
}
// Count plus in-range uniqueness proves exact coverage, including every vertex.
// Keep bounded validation scratch out of metadata-free callers' stack frames.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void validate_deformed_normal_partition(const CharacterDeformedNormalRegion& region) {
  constexpr std::uint32_t chunk_vertices=4096;
  for(std::uint32_t chunk=0;chunk<region.vertex_count;) {
    std::uint64_t seen[chunk_vertices/64]{};
    const auto count=std::min(chunk_vertices,region.vertex_count-chunk);
    for(auto v:region.members) {
      // The first pass validates every member's range. Later bounded passes
      // reuse that proof without repeating it; no output has been modified.
      if(chunk==0 && (v<region.vertex_begin || v-region.vertex_begin>=region.vertex_count))
        throw std::invalid_argument("deformed-normal member outside its region");
      const auto relative=v-region.vertex_begin;
      if(relative<chunk || relative-chunk>=count) continue;
      const auto local=relative-chunk;
      const auto mask=std::uint64_t{1}<<(local%64);
      auto& word=seen[local/64];
      if(word&mask)
        throw std::invalid_argument("duplicate deformed-normal member leaves an omitted vertex");
      word|=mask;
    }
    // Advance by the remaining count too, avoiding overflow at UINT32_MAX.
    chunk+=count;
  }
}

// Isolated exact-output probe: bound stack scratch to explicitly opted-in regions.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
bool rebuild_cached_deformed_normals(const CharacterDeformedNormalRegion& region,
                                    Mesh& output,std::uint32_t first_triangle) {
  // Trivial scalars deliberately avoid eagerly initializing unread normal slots.
  struct CachedFace { float x,y,z; };
  CachedFace faces[4096];
  std::uint8_t ready[4096]{};
  bool changed=false;
  for(std::size_t g=0;g+1<region.member_offsets.size();++g) {
    Vec3 sum{};
    for(auto q=region.triangle_offsets[g];q<region.triangle_offsets[g+1];++q) {
      const auto t=region.triangles[q];
      const auto slot=(t-first_triangle)/3;
      if(!ready[slot]) {
        const auto a=output.vertices[output.indices[t]].position;
        const auto b=output.vertices[output.indices[t+1]].position;
        const auto c=output.vertices[output.indices[t+2]].position;
        const auto normal=normalize(cross(b-a,c-a));
        faces[slot]={normal.x,normal.y,normal.z};
        ready[slot]=1;
      }
      const auto& face=faces[slot];
      sum+=Vec3{face.x,face.y,face.z};
    }
    const auto normal=normalize(sum);
    for(auto q=region.member_offsets[g];q<region.member_offsets[g+1];++q) {
      auto& v=output.vertices[region.members[q]];
      if(!same(v.normal,normal)){v.normal=normal;changed=true;}
    }
  }
  return changed;
}

CharacterInfluence weight(Bone a, Bone b, float w) {
  return {static_cast<std::uint8_t>(a),static_cast<std::uint8_t>(b),w};
}
CharacterInfluence weight(Bone a) { return weight(a,a,1.f); }

// Scalar-first quaternions. This is an original implementation of normalized
// dual-quaternion blending; mathematical references are in CHARACTER_SKINNING.md.
struct Quaternion {
  float w{0.f};
  Vec3 v;
  Quaternion operator+(const Quaternion& b) const { return {w+b.w,v+b.v}; }
  Quaternion operator*(float s) const { return {w*s,v*s}; }
};
float quaternion_dot(const Quaternion& a,const Quaternion& b) {
  return a.w*b.w+dot(a.v,b.v);
}
Quaternion quaternion_product(const Quaternion& a,const Quaternion& b) {
  return {a.w*b.w-dot(a.v,b.v),b.v*a.w+a.v*b.w+cross(a.v,b.v)};
}
struct DualQuaternion { Quaternion real,dual; };

bool rigid_matrix(const Mat4& m) {
  // Rigid pose composition accumulates float roundoff. Reject actual scale,
  // shear, reflection, and projective matrices rather than discarding them.
  constexpr float tolerance=1e-4f;
  if(m.at(0,3)!=0.f || m.at(1,3)!=0.f || m.at(2,3)!=0.f || m.at(3,3)!=1.f)
    return false;
  const Vec3 x{m.at(0,0),m.at(0,1),m.at(0,2)};
  const Vec3 y{m.at(1,0),m.at(1,1),m.at(1,2)};
  const Vec3 z{m.at(2,0),m.at(2,1),m.at(2,2)};
  return std::abs(dot(x,x)-1.f)<=tolerance && std::abs(dot(y,y)-1.f)<=tolerance &&
         std::abs(dot(z,z)-1.f)<=tolerance && std::abs(dot(x,y))<=tolerance &&
         std::abs(dot(x,z))<=tolerance && std::abs(dot(y,z))<=tolerance &&
         std::abs(dot(cross(x,y),z)-1.f)<=tolerance;
}
DualQuaternion matrix_dual_quaternion(const Mat4& m) {
  float q[4]{}; // w, x, y, z
  const float trace=m.at(0,0)+m.at(1,1)+m.at(2,2);
  if(trace>0.f) {
    const float s=2.f*std::sqrt(1.f+trace);
    q[0]=s*.25f;
    q[1]=(m.at(1,2)-m.at(2,1))/s;
    q[2]=(m.at(2,0)-m.at(0,2))/s;
    q[3]=(m.at(0,1)-m.at(1,0))/s;
  } else {
    // Select the largest diagonal, including all three 180-degree branches.
    int i=0;
    if(m.at(1,1)>m.at(i,i)) i=1;
    if(m.at(2,2)>m.at(i,i)) i=2;
    const int j=(i+1)%3,k=(i+2)%3;
    const float s=2.f*std::sqrt(1.f+m.at(i,i)-m.at(j,j)-m.at(k,k));
    q[i+1]=s*.25f;
    q[0]=(m.at(j,k)-m.at(k,j))/s;
    q[j+1]=(m.at(i,j)+m.at(j,i))/s;
    q[k+1]=(m.at(i,k)+m.at(k,i))/s;
  }
  Quaternion rotation{q[0],{q[1],q[2],q[3]}};
  rotation=rotation*(1.f/std::sqrt(quaternion_dot(rotation,rotation)));
  const Quaternion translation{0.f,{m.at(3,0),m.at(3,1),m.at(3,2)}};
  const Quaternion dual=quaternion_product(translation*.5f,rotation);
  if(!std::isfinite(dual.w) || !finite(dual.v))
    throw std::invalid_argument("character translation exceeds dual-quaternion range");
  return {rotation,dual};
}
float second_weight(const CharacterInfluence& influence) {
  // The sum was validated first. Clamp only subtraction roundoff at the zero
  // boundary (for example first=.8f, third=.2f); do not clamp authored weights.
  return std::max(0.f,1.f-influence.first_weight-influence.third_weight);
}
int single_bone(const CharacterInfluence& influence,float second) {
  int bone=-1;
  const std::uint8_t indices[3]={influence.first,influence.second,influence.third};
  const float weights[3]={influence.first_weight,second,influence.third_weight};
  for(int i=0;i<3;++i) if(weights[i]>0.f) {
    if(bone>=0 && bone!=indices[i]) return -1;
    bone=indices[i];
  }
  return bone;
}
DualQuaternion blend_dual_quaternions(
    const std::array<DualQuaternion,character_bone_count>& bones,
    const CharacterInfluence& influence,float second) {
  const std::uint8_t indices[3]={influence.first,influence.second,influence.third};
  const float weights[3]={influence.first_weight,second,influence.third_weight};
  // The largest active influence supplies a stable hemisphere reference. Its
  // projection bounds the blended real norm away from zero (at least 1/3).
  int reference=0;
  for(int i=1;i<3;++i) if(weights[i]>weights[reference]) reference=i;
  const auto& anchor=bones[indices[reference]].real;
  DualQuaternion blend;
  for(int i=0;i<3;++i) if(weights[i]>0.f) {
    const auto& bone=bones[indices[i]];
    const float w=quaternion_dot(anchor,bone.real)<0.f?-weights[i]:weights[i];
    blend.real=blend.real+bone.real*w;
    blend.dual=blend.dual+bone.dual*w;
  }
  const float inverse_norm=1.f/std::sqrt(quaternion_dot(blend.real,blend.real));
  blend.real=blend.real*inverse_norm;
  blend.dual=blend.dual*inverse_norm;
  // Full unit dual-quaternion normalization: real.real=1 and real.dual=0.
  blend.dual=blend.dual+blend.real*(-quaternion_dot(blend.real,blend.dual));
  return blend;
}
Vec3 quaternion_rotate(const Quaternion& q,Vec3 v) {
  return v+cross(q.v,cross(q.v,v)+v*q.w)*2.f;
}

struct Ring { Vec3 center; float rx, rz; CharacterInfluence influence; };
struct Builder {
  CharacterModel& model;
  float h;
  bool near;
  int sides;
  std::uint32_t vertex(Vec3 p, Vec3 color, CharacterInfluence influence, Vec2 uv={}) {
    const auto i=static_cast<std::uint32_t>(model.bind_mesh.vertices.size());
    model.bind_mesh.vertices.push_back({p*h,{},color,uv});
    model.influences.push_back(influence); return i;
  }
  void tri(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    auto& indices=model.bind_mesh.indices;
    indices.push_back(a); indices.push_back(b); indices.push_back(c);
  }
  // Closed, smooth elliptical ring loft; both caps have independent vertices so
  // soles, cuffs and hems retain an authored hard edge rather than a dark fan.
  void loft(const std::vector<Ring>& rings, Vec3 color, int count=0) {
    const int n=count?count:sides;
    const auto start=static_cast<std::uint32_t>(model.bind_mesh.vertices.size());
    for(std::size_t row=0;row<rings.size();++row) {
      const auto& r=rings[row];
      for(int i=0;i<n;++i) {
        const float a=2*pi*float(i)/float(n);
        vertex(r.center+Vec3{r.rx*std::cos(a),0,r.rz*std::sin(a)},color,r.influence,
               {float(i)/float(n),float(row)/float(rings.size()-1)});
      }
    }
    for(std::size_t row=0;row+1<rings.size();++row) for(int i=0;i<n;++i) {
      const auto a=start+static_cast<std::uint32_t>(row*n+i);
      const auto d=start+static_cast<std::uint32_t>(row*n+(i+1)%n);
      const auto b=a+n,c=d+n;
      tri(a,b,c); tri(a,c,d);
    }
    for(int end=0;end<2;++end) {
      const auto& r=end?rings.back():rings.front();
      const auto c=vertex(r.center,color,r.influence);
      const auto base=static_cast<std::uint32_t>(model.bind_mesh.vertices.size());
      for(int i=0;i<n;++i) {
        const float a=2*pi*float(i)/float(n);
        vertex(r.center+Vec3{r.rx*std::cos(a),0,r.rz*std::sin(a)},color,r.influence);
      }
      for(int i=0;i<n;++i) {
        const auto a=base+i,b=base+(i+1)%n;
        if(end) tri(c,b,a); else tri(c,a,b);
      }
    }
  }
  void oval(Vec3 center,Vec3 radius,Vec3 color,Bone bone,int n=0) {
    const auto w=weight(bone);
    if(near) {
      loft({{center+Vec3{0,-radius.y,0},radius.x*.22f,radius.z*.22f,w},
            {center+Vec3{0,-radius.y*.65f,0},radius.x*.79f,radius.z*.79f,w},
            {center,radius.x,radius.z,w},
            {center+Vec3{0,radius.y*.65f,0},radius.x*.79f,radius.z*.79f,w},
            {center+Vec3{0,radius.y,0},radius.x*.22f,radius.z*.22f,w}},color,n);
    } else {
      loft({{center+Vec3{0,-radius.y,0},radius.x*.33f,radius.z*.33f,w},
            {center,radius.x,radius.z,w},
            {center+Vec3{0,radius.y,0},radius.x*.33f,radius.z*.33f,w}},color,n);
    }
  }
  // Chamfered rectangular prism: clearer clothing/accessory planes than an
  // ellipsoid, without the former stack-of-perfect-boxes silhouette.
  void panel(Vec3 center,Vec3 size,Vec3 color,Bone bone) {
    const float x=size.x*.5f,y=size.y*.5f,z=size.z*.5f;
    const float bevel=std::min(x,z)*.32f;
    const std::vector<Vec3> perimeter=near?std::vector<Vec3>{{-x+bevel,0,-z},{x-bevel,0,-z},
      {x,0,-z+bevel},{x,0,z-bevel},{x-bevel,0,z},{-x+bevel,0,z},
      {-x,0,z-bevel},{-x,0,-z+bevel}}:
      std::vector<Vec3>{{-x,0,-z},{x,0,-z},{x,0,z},{-x,0,z}};
    const int count=static_cast<int>(perimeter.size());
    const auto w=weight(bone);
    // Clockwise XZ loop when viewed from above gives outward side winding.
    for(int i=0;i<count;++i) {
      const auto a=perimeter[i],b=perimeter[(i+1)%count];
      const auto v=vertex(center+a+Vec3{0,-y,0},color,w);
      vertex(center+a+Vec3{0,y,0},color,w);
      vertex(center+b+Vec3{0,y,0},color,w);
      vertex(center+b+Vec3{0,-y,0},color,w);
      tri(v,v+1,v+2); tri(v,v+2,v+3);
    }
    for(int end=0;end<2;++end) {
      const float yy=end?y:-y;
      const auto c=vertex(center+Vec3{0,yy,0},color,w);
      const auto base=static_cast<std::uint32_t>(model.bind_mesh.vertices.size());
      for(const auto p:perimeter) vertex(center+p+Vec3{0,yy,0},color,w);
      for(int i=0;i<count;++i) {
        if(end) tri(c,base+(i+1)%count,base+i);
        else tri(c,base+i,base+(i+1)%count);
      }
    }
  }
  void ribbon(Vec3 a,Vec3 b,float width,float depth,Vec3 color,Bone bone) {
    // Oriented narrow flat fabric strap, following the front/back of the torso.
    const Vec3 axis=normalize(b-a),side=normalize(cross(axis,{0,0,1}))*width*.5f;
    const Vec3 dz{0,0,depth*.5f};
    const std::array<Vec3,8> p{{a-side-dz,a+side-dz,b+side-dz,b-side-dz,
                             a-side+dz,a+side+dz,b+side+dz,b-side+dz}};
    constexpr int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},
                              {3,7,6,2},{0,4,7,3},{1,2,6,5}};
    for(const auto& face:faces) {
      const auto start=static_cast<std::uint32_t>(model.bind_mesh.vertices.size());
      for(int i:face) vertex(p[i],color,weight(bone));
      tri(start,start+1,start+2); tri(start,start+2,start+3);
    }
  }
  void finish() {
    auto& mesh=model.bind_mesh;
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
      auto& a=mesh.vertices[mesh.indices[i]];
      auto& b=mesh.vertices[mesh.indices[i+1]];
      auto& c=mesh.vertices[mesh.indices[i+2]];
      const Vec3 normal=cross(b.position-a.position,c.position-a.position);
      a.normal+=normal; b.normal+=normal; c.normal+=normal;
    }
    for(auto& v:mesh.vertices) v.normal=normalize(v.normal);
  }
};

CharacterRig make_rig(float h,const CharacterAppearance& a) {
  CharacterRig rig; rig.height=h;
  auto set=[&](Bone b,int parent,Vec3 pos) { rig.joints[bone_index(b)]={parent,pos*h}; };
  auto child=[&](Bone b,Bone parent,Vec3 pos) { set(b,int(bone_index(parent)),pos); };
  set(Bone::Pelvis,-1,{0,0,0});
  child(Bone::Spine,Bone::Pelvis,{0,.115f,0});
  child(Bone::Chest,Bone::Spine,{0,.235f,0});
  child(Bone::Neck,Bone::Chest,{0,.313f,0});
  child(Bone::Head,Bone::Neck,{0,.356f,0});
  for(int side=0;side<2;++side) {
    const float sign=side?1.f:-1.f;
    const Bone upper=side?Bone::RightUpperArm:Bone::LeftUpperArm;
    const Bone forearm=side?Bone::RightForearm:Bone::LeftForearm;
    const Bone hand=side?Bone::RightHand:Bone::LeftHand;
    const Bone thigh=side?Bone::RightThigh:Bone::LeftThigh;
    const Bone shin=side?Bone::RightShin:Bone::LeftShin;
    const Bone foot=side?Bone::RightFoot:Bone::LeftFoot;
    child(upper,Bone::Chest,{sign*.133f*a.shoulder_scale,.24f,0});
    child(forearm,upper,{sign*(.133f*a.shoulder_scale+.024f),.105f,0});
    child(hand,forearm,{sign*(.133f*a.shoulder_scale+.039f),-.033f,0});
    child(thigh,Bone::Pelvis,{sign*.057f,0,0});
    child(shin,thigh,{sign*.057f,-.225f,0});
    child(foot,shin,{sign*.057f,-.46f,0});
  }
  return rig;
}

void anatomy(Builder& b) {
  auto& m=b.model; const auto& a=m.appearance;
  const bool uniform=m.role==CharacterRole::Security || m.role==CharacterRole::Enforcer;
  const Vec3 shirt=uniform?Vec3{.16f,.22f,.28f}:
    (m.role==CharacterRole::BankStaff?Vec3{.62f,.66f,.68f}:a.shirt);
  const Vec3 trousers=uniform?Vec3{.095f,.13f,.175f}:a.trousers;
  const auto wp=weight(Bone::Pelvis),wc=weight(Bone::Chest);
  const float sw=a.shoulder_scale,ww=a.waist_scale;
  b.loft({{{0,-.039f,0},.091f*ww,.055f,wp},{{0,.025f,0},.096f*ww,.06f,wp},
          {{0,.060f,0},.086f*ww,.051f,weight(Bone::Pelvis,Bone::Spine,.55f)}},trousers);
  b.loft({{{0,.046f,0},.09f*ww,.058f,weight(Bone::Pelvis,Bone::Spine,.65f)},
          {{0,.125f,0},.085f*ww,.057f,weight(Bone::Spine)},
          {{0,.225f,0},.12f*sw,.067f,wc},{{0,.274f,0},.107f*sw,.053f,wc}},shirt);
  b.loft({{{0,.267f,0},.032f,.032f,weight(Bone::Neck)},
          {{0,.341f,0},.031f,.029f,weight(Bone::Head)}},a.skin,b.near?8:6);
  // Collar is a separate garment layer, not a differently tinted neck.
  b.loft({{{0,.269f,0},.043f,.039f,wc},{{0,.284f,0},.037f,.034f,wc}},shirt*.72f,b.near?10:6);
  for(int side=0;side<2;++side) {
    const float sign=side?1.f:-1.f;
    const Bone upper=side?Bone::RightUpperArm:Bone::LeftUpperArm;
    const Bone forearm=side?Bone::RightForearm:Bone::LeftForearm;
    const Bone hand=side?Bone::RightHand:Bone::LeftHand;
    const Bone thigh=side?Bone::RightThigh:Bone::LeftThigh;
    const Bone shin=side?Bone::RightShin:Bone::LeftShin;
    const Bone foot=side?Bone::RightFoot:Bone::LeftFoot;
    const float shoulder=sign*.133f*sw,elbow=sign*(.133f*sw+.024f),wrist=sign*(.133f*sw+.039f);
    // Rings spanning the elbow/knee carry two influences; sleeve seams and
    // separately rounded hands/boots disguise extremity junctions naturally.
    b.loft({{{elbow,.097f,0},.025f,.026f,weight(upper,forearm,.40f)},
            {{shoulder+sign*.012f,.173f,0},.035f,.035f,weight(upper)},
            {{shoulder,.242f,0},.037f,.037f,weight(upper,Bone::Chest,.88f)},
            {{shoulder-sign*.003f,.259f,0},.027f,.028f,weight(upper)}},shirt*.96f,b.near?8:6);
    const bool rolled=m.role==CharacterRole::Dock || m.role==CharacterRole::Market;
    b.loft({{{wrist,-.036f,0},.018f,.02f,weight(forearm,hand,.88f)},
            {{(wrist+elbow)*.5f,.035f,0},.025f,.026f,weight(forearm)},
            {{elbow,.110f,0},.026f,.027f,weight(forearm,upper,.72f)}},rolled?a.skin:shirt*.91f,b.near?8:6);
    b.loft({{{rolled?elbow:wrist,rolled?.092f:-.025f,0},.027f,.028f,weight(rolled?forearm:hand)},
            {{rolled?elbow:wrist,rolled?.111f:-.009f,0},.028f,.029f,weight(forearm)}},shirt*.72f,b.near?8:6);
    b.oval({wrist,-.063f,.003f},{.023f,.034f,.025f},a.skin,hand,b.near?8:6);
    if(b.near) b.oval({wrist-sign*.02f,-.055f,.018f},{.009f,.02f,.012f},a.skin*.95f,hand,6);
    const float x=sign*.057f;
    b.loft({{{x,-.235f,0},.031f,.033f,weight(thigh,shin,.42f)},
            {{x,-.117f,0},.043f,.045f,weight(thigh)},
            {{x,-.009f,0},.046f,.05f,weight(thigh,Bone::Pelvis,.87f)}},trousers,b.near?8:6);
    b.loft({{{x,-.454f,0},.026f,.028f,weight(shin,foot,.92f)},
            {{x,-.35f,-.003f},.034f,.035f,weight(shin)},
            {{x,-.223f,0},.031f,.033f,weight(shin,thigh,.73f)}},trousers*.93f,b.near?8:6);
    // The flat bottom is exactly -height/2 for every role and LOD.
    b.loft({{{x,-.5f,.033f},.038f,.067f,weight(foot)},
            {{x,-.486f,.033f},.038f,.067f,weight(foot)}},dark,b.near?10:6);
    b.loft({{{x,-.485f,.032f},.037f,.065f,weight(foot)},
            {{x,-.459f,.018f},.034f,.045f,weight(foot)},
            {{x,-.435f,0},.025f,.028f,weight(foot)}},a.shoes,b.near?10:6);
    if(b.near) b.panel({x,-.46f,.051f},{.036f,.006f,.019f},stitching*.75f,foot);
  }
}

void head(Builder& b) {
  const auto& a=b.model.appearance;
  const float f=a.face_scale;
  const auto w=weight(Bone::Head);
  const int n=b.near?12:8;
  b.loft({{{0,.336f,.009f},(.032f+.003f*a.face_style)*f,.036f,w},{{0,.360f,.004f},(.044f+.003f*a.face_style)*f,.049f,w},
          {{0,.403f,0},.064f*f,.06f,w},{{0,.45f,-.003f},.06f*f,.058f,w},
          {{0,.477f,-.004f},.042f*f,.043f,w}},a.skin,n);
  // Surface-following features stay within a few millimetres of the actual
  // faceted cheek. A forward-floating eyeball becomes conspicuous under ray
  // shadows even when it looked tolerable in ambient-only raster lighting.
  auto face_front=[&](float x,float y) {
    const float t=(y-.403f)/(.45f-.403f);
    const float rx=(.064f*(1.f-t)+.06f*t)*f;
    const float rz=.06f*(1.f-t)+.058f*t;
    const float xx=std::abs(x);
    for(int i=0;i<n/4;++i) {
      const float aa=2*pi*float(i)/float(n),bb=2*pi*float(i+1)/float(n);
      const float x0=rx*std::cos(aa),x1=rx*std::cos(bb);
      if(xx>=x1-1e-6f && xx<=x0+1e-6f) {
        const float u=(xx-x0)/(x1-x0);
        return -.003f*t+rz*(std::sin(aa)*(1.f-u)+std::sin(bb)*u);
      }
    }
    return -.003f*t+rz;
  };
  auto conform=[&](std::size_t begin) {
    auto& vertices=b.model.bind_mesh.vertices;
    for(std::size_t i=begin;i<vertices.size();++i)
      vertices[i].position.z+=face_front(vertices[i].position.x/b.h,
                                        vertices[i].position.y/b.h)*b.h;
  };
  for(int side=0;side<2;++side) {
    const float sign=side?1.f:-1.f,x=sign*.0205f*f;
    b.oval({sign*.064f*f,.411f,0},{.010f,.022f,.013f},a.skin*.95f,Bone::Head,b.near?6:4);
    auto begin=b.model.bind_mesh.vertices.size();
    b.oval({x,.423f,b.near?-.0005f:.0003f},{.0095f,.0034f,.0016f},
           b.near?Vec3{.66f,.65f,.60f}:a.eyes,Bone::Head,b.near?6:4);
    conform(begin);
    if(b.near) {
      begin=b.model.bind_mesh.vertices.size();
      b.oval({x,.423f,.0007f},{.0032f,.0029f,.0011f},a.eyes,Bone::Head,6);
      conform(begin);
      // Shallow upper/lower skin lids partly cover the white aperture.
      for(float yy:{.4262f,.4198f}) {
        begin=b.model.bind_mesh.vertices.size();
        b.ribbon({x-.0095f,yy,.0006f},{x+.0095f,yy,.0006f},
                 .0018f,.002f,a.skin*.97f,Bone::Head);
        conform(begin);
      }
      begin=b.model.bind_mesh.vertices.size();
      b.panel({x,.435f,.0009f},{.018f,.003f,.002f},a.hair*.83f,Bone::Head);
      conform(begin);
    }
  }
  // Short rounded bridge/tip: closed eight-sided rings avoid the old sharp
  // four-centimetre point and its oversized ray-traced shadow across the face.
  b.loft({{{0,.394f,.059f},.010f,.006f,w},
          {{0,.402f,.060f},.009f,.011f,w},
          {{0,.429f,.056f},.0045f,.005f,w}},a.skin*.98f,b.near?8:6);
  if(b.near) {
    b.panel({0,.380f,.052f},{.027f,.0035f,.006f},a.skin*.62f,Bone::Head);
    b.panel({0,.366f,.048f},{.022f,.005f,.004f},a.skin*.92f,Bone::Head);
  }
  const bool cap=b.model.role==CharacterRole::Security;
  const Vec3 hair=cap?Vec3{.10f,.155f,.21f}:a.hair;
  // Structured cap/swept crown. The upper plane is at the actor's exact height.
  b.loft({{{0,.454f,-.009f},.063f*f,.054f,w},
          {{a.hair_style==1?.008f:0.f,.477f,-.007f},.06f*f,.053f,w},
          {{a.hair_style==1?.012f:0.f,.5f,-.009f},.028f*f,.027f,w}},hair,n);
  // Back/side hair volumes avoid a helmet-like straight fringe across the face.
  b.panel({0,.439f,-.05f},{.097f*f,.046f,.018f},hair,Bone::Head);
  if(a.hair_style==2 && !cap) {
    b.oval({0,.448f,-.074f},{.028f,.031f,.025f},hair,Bone::Head,b.near?8:6);
  } else if(a.hair_style==3 && !cap) {
    for(float sign:{-1.f,1.f}) b.panel({sign*.057f*f,.435f,-.012f},
      {.014f,.04f,.045f},hair,Bone::Head);
  }
  if(cap) {
    b.panel({0,.458f,.044f},{.12f,.009f,.098f},hair,Bone::Head);
    if(b.near) b.panel({0,.48f,.045f},{.015f,.015f,.008f},{.63f,.70f,.66f},Bone::Head);
  }
}

void wardrobe(Builder& b) {
  const auto& a=b.model.appearance; const auto role=b.model.role;
  const auto chest=Bone::Chest,pelvis=Bone::Pelvis;
  const Vec3 trim=a.accent,leather{.22f,.145f,.085f};
  // Waistband, buckle and jacket closure make the clothing read as clothing.
  b.loft({{{0,.034f,0},.095f*a.waist_scale,.061f,weight(pelvis)},
          {{0,.049f,0},.095f*a.waist_scale,.061f,weight(pelvis)}},dark,b.near?10:6);
  if(b.near) b.panel({0,.042f,.063f},{.017f,.012f,.006f},{.55f,.56f,.51f},pelvis);
  b.ribbon({0,.083f,.061f},{0,.264f,.063f},.005f,.004f,a.shirt*.55f,chest);
  switch(role) {
  case CharacterRole::Commuter:
    // Long jacket front panels and a proper diagonal satchel, not a body tint.
    for(float s:{-1.f,1.f}) b.panel({s*.053f,.142f,.058f},
      {.079f,.211f,.025f},a.shirt*.82f,chest);
    b.ribbon({-.088f,.256f,.073f},{.106f,.067f,.069f},.018f,.006f,leather,chest);
    b.panel({.119f,.032f,.024f},{.082f,.096f,.072f},leather,pelvis);
    if(b.near) b.panel({.119f,.055f,.063f},{.071f,.035f,.008f},leather*.83f,pelvis);
    break;
  case CharacterRole::Market:
    // Bib apron and front patch pocket, tied at the waist.
    b.panel({0,.178f,.074f},{.116f,.167f,.014f},trim,chest);
    b.panel({0,.012f,.071f},{.162f,.155f,.016f},trim,pelvis);
    b.ribbon({-.046f,.25f,.07f},{-.032f,.285f,.04f},.012f,.007f,trim,chest);
    b.ribbon({.046f,.25f,.07f},{.032f,.285f,.04f},.012f,.007f,trim,chest);
    if(b.near) b.panel({0,.022f,.082f},{.094f,.047f,.008f},trim*.79f,pelvis);
    break;
  case CharacterRole::Dock:
    // High-visibility work vest, reflective strips and cargo thigh pocket.
    for(float s:{-1.f,1.f}) {
      b.panel({s*.065f,.179f,.064f},{.09f,.182f,.026f},{.71f,.39f,.09f},chest);
      b.ribbon({s*.06f,.10f,.079f},{s*.06f,.26f,.079f},.013f,.004f,{.70f,.75f,.65f},chest);
    }
    b.panel({-.081f,-.103f,.038f},{.067f,.065f,.027f},a.trousers*.82f,Bone::LeftThigh);
    break;
  case CharacterRole::Security:
    b.panel({0,.178f,.065f},{.169f,.16f,.035f},{.08f,.12f,.165f},chest);
    b.panel({-.047f,.216f,.086f},{.029f,.033f,.008f},{.64f,.68f,.53f},chest);
    b.panel({.073f,.202f,.085f},{.026f,.048f,.022f},dark,chest);
    b.ribbon({.077f,.222f,.087f},{.077f,.265f,.055f},.004f,.004f,dark,chest);
    b.panel({-.1f,.015f,0},{.04f,.078f,.045f},dark,pelvis);
    break;
  case CharacterRole::Fence:
    // Tailored lapels, warm scarf and glasses differentiate the contact.
    for(float s:{-1.f,1.f}) {
      b.panel({s*.058f,.144f,.061f},{.079f,.228f,.025f},a.shirt*.68f,chest);
      b.ribbon({s*.079f,.251f,.076f},{s*.019f,.16f,.076f},.026f,.008f,trim*.64f,chest);
      if(b.near) b.panel({s*.025f*a.face_scale,.423f,.065f},{.039f,.019f,.006f},dark,Bone::Head);
    }
    b.panel({0,.283f,.024f},{.078f,.037f,.094f},trim,chest);
    if(b.near) b.panel({0,.423f,.067f},{.016f,.004f,.004f},dark,Bone::Head);
    break;
  case CharacterRole::CrewScout:
    // Cropped asymmetric jacket, compact backpack and shoulder straps.
    b.panel({0,.177f,-.075f},{.138f,.168f,.065f},{.19f,.265f,.24f},chest);
    for(float s:{-1.f,1.f}) b.ribbon({s*.074f,.252f,.052f},
      {s*.056f,.09f,.05f},.017f,.007f,dark,chest);
    b.panel({.1f,.135f,.057f},{.038f,.036f,.02f},trim,chest);
    break;
  case CharacterRole::CrewTech:
    // Utility harness and repair/tool pouch; rolled forearm cuff detail.
    b.ribbon({-.085f,.257f,.064f},{.083f,.075f,.062f},.027f,.009f,dark,chest);
    b.panel({-.116f,.021f,.019f},{.072f,.097f,.066f},{.29f,.24f,.16f},pelvis);
    b.panel({.08f,.194f,.065f},{.041f,.05f,.018f},trim,chest);
    if(b.near) for(float x:{-.128f,-.111f})
      b.panel({x,.074f,.056f},{.01f,.037f,.012f},{.48f,.5f,.47f},pelvis);
    break;
  case CharacterRole::Enforcer:
    // Broad quilted protective vest and shoulder pads; same anatomical rig.
    b.panel({0,.18f,.071f},{.201f,.178f,.052f},{.12f,.14f,.16f},chest);
    for(float s:{-1.f,1.f}) {
      b.oval({s*.139f*a.shoulder_scale,.242f,0},{.044f,.03f,.047f},dark,
             s<0?Bone::LeftUpperArm:Bone::RightUpperArm,b.near?8:6);
      if(b.near) b.panel({s*.049f,.163f,.099f},{.074f,.049f,.008f},a.accent*.55f,chest);
    }
    break;
  case CharacterRole::Player:
    b.panel({0,.173f,-.074f},{.115f,.142f,.049f},dark,chest);
    b.ribbon({-.08f,.256f,.064f},{.053f,.076f,.062f},.019f,.007f,dark,chest);
    b.panel({.048f,.219f,.065f},{.032f,.019f,.012f},trim,chest);
    break;
  case CharacterRole::Ghost:
    // Network proxy is a fully articulated person with a readable cyan vest.
    for(float s:{-1.f,1.f}) b.panel({s*.06f,.174f,.064f},
      {.085f,.186f,.022f},{.11f,.49f,.53f},chest);
    b.ribbon({-.098f,.148f,.078f},{.098f,.148f,.078f},.013f,.006f,{.57f,.79f,.75f},chest);
    break;
  case CharacterRole::BankStaff:
    // Clean service-counter clothing: pointed collar, narrow tie and name badge.
    // No shoulder bag, outside-work equipment or silhouette-changing props.
    for(float s:{-1.f,1.f}) b.ribbon({s*.023f,.28f,.035f},
      {s*.029f,.242f,.061f},.024f,.006f,{.77f,.79f,.76f},chest);
    b.oval({0,.261f,.055f},{.010f,.011f,.005f},trim*.62f,chest,b.near?6:4);
    b.ribbon({0,.252f,.060f},{0,.22f,.066f},.017f,.005f,trim*.62f,chest);
    b.ribbon({0,.22f,.066f},{0,.122f,.060f},.015f,.005f,trim*.62f,chest);
    b.panel({-.055f,.213f,.061f},{.046f,.021f,.008f},dark,chest);
    if(b.near) {
      b.panel({-.055f,.213f,.066f},{.039f,.014f,.003f},{.75f,.76f,.68f},chest);
      b.panel({-.055f,.214f,.068f},{.027f,.003f,.002f},dark,chest);
    }
    break;
  case CharacterRole::Count: break;
  }
}
}  // namespace

CharacterPose::CharacterPose() { for(auto& matrix:skin_matrices) matrix=Mat4::identity(); }
std::uint32_t character_seed(std::string_view id) {
  std::uint32_t hash=2166136261u;
  for(const unsigned char c:id) { hash^=c; hash*=16777619u; }
  return hash;
}
CharacterAppearance character_appearance(std::uint32_t seed) {
  static const std::array<Vec3,7> skin{{{.90f,.69f,.52f},{.73f,.49f,.33f},
    {.52f,.31f,.20f},{.35f,.205f,.14f},{.79f,.585f,.415f},{.62f,.405f,.285f},{.94f,.765f,.635f}}};
  static const std::array<Vec3,6> hair{{{.055f,.042f,.033f},{.17f,.095f,.045f},
    {.36f,.22f,.105f},{.51f,.385f,.20f},{.42f,.44f,.42f},{.27f,.11f,.052f}}};
  static const std::array<Vec3,8> shirts{{{.27f,.38f,.43f},{.48f,.23f,.16f},
    {.29f,.36f,.24f},{.48f,.42f,.28f},{.30f,.26f,.38f},{.23f,.39f,.37f},
    {.49f,.34f,.37f},{.53f,.52f,.43f}}};
  static const std::array<Vec3,5> trousers{{{.12f,.16f,.20f},{.20f,.19f,.15f},
    {.19f,.23f,.20f},{.25f,.21f,.17f},{.16f,.15f,.20f}}};
  static const std::array<Vec3,5> accent{{{.68f,.49f,.20f},{.44f,.19f,.14f},
    {.24f,.44f,.43f},{.53f,.52f,.39f},{.43f,.32f,.45f}}};
  CharacterAppearance a;
  a.skin=skin[mix(seed^0x126345abu)%skin.size()];
  a.hair=hair[mix(seed^0x789acff1u)%hair.size()];
  a.eyes=mix(seed^0x15a45deu)%3==0?Vec3{.16f,.23f,.23f}:Vec3{.13f,.08f,.045f};
  a.shirt=shirts[mix(seed^0xae12901u)%shirts.size()];
  a.trousers=trousers[mix(seed^0x656d145u)%trousers.size()];
  a.accent=accent[mix(seed^0xab73c14u)%accent.size()];
  a.shoes=mix(seed^0x6724561u)%2?Vec3{.19f,.125f,.078f}:Vec3{.075f,.085f,.095f};
  a.shoulder_scale=.91f+float(mix(seed^0x6418bb1u)%19)*.01f;
  a.waist_scale=.89f+float(mix(seed^0x83a4861u)%25)*.01f;
  a.face_scale=.94f+float(mix(seed^0x4784251u)%13)*.01f;
  a.hair_style=static_cast<std::uint8_t>(mix(seed^0x101451u)%4);
  a.face_style=static_cast<std::uint8_t>(mix(seed^0x341781u)%3);
  return a;
}
const char* character_role_name(CharacterRole role) {
  constexpr const char* names[]{"commuter","market","dock","security","fence",
    "crew_scout","crew_tech","enforcer","player","ghost","bank_staff"};
  const auto i=static_cast<std::size_t>(role);
  return i<static_cast<std::size_t>(CharacterRole::Count)?names[i]:"invalid";
}
CharacterModel make_character_model(float height,CharacterRole role,std::uint32_t seed,CharacterLod lod) {
  if(!std::isfinite(height) || height<=0.f) throw std::invalid_argument("character height must be positive and finite");
  if(role>=CharacterRole::Count) throw std::invalid_argument("invalid character role");
  if(lod!=CharacterLod::Near && lod!=CharacterLod::Far) throw std::invalid_argument("invalid character LOD");
  CharacterModel model;
  model.appearance=character_appearance(seed); model.role=role; model.lod=lod; model.seed=seed;
  model.rig=make_rig(height,model.appearance);
  model.bind_mesh.vertices.reserve(lod==CharacterLod::Near?2200:1300);
  model.bind_mesh.indices.reserve(lod==CharacterLod::Near?6500:4200);
  model.influences.reserve(lod==CharacterLod::Near?2200:1300);
  Builder builder{model,height,lod==CharacterLod::Near,lod==CharacterLod::Near?10:6};
  anatomy(builder); head(builder); wardrobe(builder); builder.finish();
  return model;
}
CharacterPose make_character_pose(const CharacterRig& rig,
    const std::array<Vec3,character_bone_count>& euler,Vec3 root_translation) {
  if(!finite(root_translation)) throw std::invalid_argument("nonfinite root translation");
  CharacterPose pose;
  std::array<Mat4,character_bone_count> global;
  for(std::size_t i=0;i<character_bone_count;++i) {
    const auto& joint=rig.joints[i];
    if(!finite(euler[i]) || !finite(joint.bind_position) || joint.parent>=int(i) || joint.parent < -1)
      throw std::invalid_argument("invalid character rig/rotation");
    const auto rotation=rotate_z(euler[i].z)*rotate_y(euler[i].y)*rotate_x(euler[i].x);
    if(joint.parent<0) global[i]=translate(joint.bind_position+root_translation)*rotation;
    else global[i]=global[joint.parent]*translate(joint.bind_position-rig.joints[joint.parent].bind_position)*rotation;
    pose.skin_matrices[i]=global[i]*translate(-joint.bind_position);
  }
  return pose;
}
bool apply_character_pose(const CharacterModel& model,const CharacterPose& pose,Mesh& output) {
  if(&output==&model.bind_mesh || output.vertices.size()!=model.bind_mesh.vertices.size() ||
     output.indices.size()!=model.bind_mesh.indices.size() ||
     model.influences.size()!=model.bind_mesh.vertices.size())
    throw std::invalid_argument("initialize character output from matching bind mesh");
  for(const auto& matrix:pose.skin_matrices) for(float value:matrix.m)
    if(!std::isfinite(value)) throw std::invalid_argument("nonfinite character pose");
  for(const auto& influence:model.influences) {
    if(influence.first>=character_bone_count || influence.second>=character_bone_count ||
       influence.third>=character_bone_count || !std::isfinite(influence.first_weight) ||
       !std::isfinite(influence.third_weight) || influence.first_weight<0.f ||
       influence.third_weight<0.f || influence.first_weight+influence.third_weight>1.f)
      throw std::invalid_argument("invalid character influence indices/weights");
  }
  std::size_t prior_region_end=0;
  for(const auto& region:model.deformed_normal_regions) {
    if(region.vertex_begin>output.vertices.size() ||
       region.vertex_count>output.vertices.size()-region.vertex_begin ||
       region.member_offsets.size()<2 ||
       region.triangle_offsets.size()!=region.member_offsets.size() ||
       region.member_offsets.front()!=0 || region.triangle_offsets.front()!=0 ||
       region.member_offsets.back()!=region.members.size() ||
       region.triangle_offsets.back()!=region.triangles.size() ||
       region.members.size()!=region.vertex_count)
      throw std::invalid_argument("invalid deformed-normal region");
    if(region.vertex_begin<prior_region_end)
      throw std::invalid_argument("deformed-normal regions must be ordered and disjoint");
    prior_region_end=region.vertex_begin+region.vertex_count;
    for(std::size_t g=1;g<region.member_offsets.size();++g)
      if(region.member_offsets[g]<=region.member_offsets[g-1] ||
         region.triangle_offsets[g]<=region.triangle_offsets[g-1])
        throw std::invalid_argument("empty or reversed deformed-normal group");
    validate_deformed_normal_partition(region);
    for(auto t:region.triangles) {
      if(t%3 || t>output.indices.size() || output.indices.size()-t<3)
        throw std::invalid_argument("invalid deformed-normal triangle");
      for(int k=0;k<3;++k)
        if(output.indices[t+k]<region.vertex_begin ||
           output.indices[t+k]-region.vertex_begin>=region.vertex_count)
          throw std::invalid_argument("deformed-normal triangle outside its region");
    }
    std::uint32_t previous_edge=0;
    bool first_edge=true;
    for(const auto& e:region.edge_attachments) {
      for(auto v:{e.vertex,e.first,e.second})
        if(v<region.vertex_begin || v-region.vertex_begin>=region.vertex_count)
          throw std::invalid_argument("posed edge attachment outside its region");
      if(e.vertex==e.first || e.vertex==e.second || e.first==e.second ||
         !std::isfinite(e.fraction) || e.fraction<=0 || e.fraction>=1)
        throw std::invalid_argument("invalid posed edge attachment");
      if(!first_edge && e.vertex<=previous_edge)
        throw std::invalid_argument("posed edge attachments must be sorted and unique");
      previous_edge=e.vertex;first_edge=false;
      for(auto parent:{e.first,e.second}) {
        const auto found=std::lower_bound(region.edge_attachments.begin(),region.edge_attachments.end(),parent,
          [](const auto& edge,std::uint32_t vertex){return edge.vertex<vertex;});
        if(found!=region.edge_attachments.end() && found->vertex==parent)
          throw std::invalid_argument("posed edge attachment parent is not an original corner");
      }
    }
  }
  std::array<DualQuaternion,character_bone_count> dual_quaternions;
  if(model.dual_quaternion_skinning) {
    for(std::size_t i=0;i<character_bone_count;++i) {
      if(!rigid_matrix(pose.skin_matrices[i]))
        throw std::invalid_argument("dual-quaternion character pose must be rigid");
      dual_quaternions[i]=matrix_dual_quaternion(pose.skin_matrices[i]);
    }
  }
  bool changed=false;
  std::size_t edge_region=0,edge_index=0;
  for(std::size_t i=0;i<output.vertices.size();++i) {
    while(edge_region<model.deformed_normal_regions.size() &&
          edge_index==model.deformed_normal_regions[edge_region].edge_attachments.size()) {
      ++edge_region;edge_index=0;
    }
    if(edge_region<model.deformed_normal_regions.size() &&
       model.deformed_normal_regions[edge_region].edge_attachments[edge_index].vertex==i) {
      ++edge_index;
      continue; // Its final position and normal are resolved below, once.
    }
    const auto& source=model.bind_mesh.vertices[i]; const auto& influence=model.influences[i];
    Vec3 position,normal;
    if(influence.third_weight==0.f && (!model.dual_quaternion_skinning ||
       influence.first==influence.second || influence.first_weight==0.f || influence.first_weight==1.f)) {
      // Keep the original two-bone arithmetic exactly, including endpoint
      // weights. Rigid two-slot vertices also use this path when DQ is enabled.
      const auto& a=pose.skin_matrices[influence.first];
      position=transform_point(a,source.position); normal=transform_direction(a,source.normal);
      if(influence.first!=influence.second && influence.first_weight<1.f) {
        const auto& b=pose.skin_matrices[influence.second]; const float w=influence.first_weight;
        position=position*w+transform_point(b,source.position)*(1.f-w);
        normal=normal*w+transform_direction(b,source.normal)*(1.f-w);
      }
    } else {
      const float second=second_weight(influence);
      const int rigid_bone=single_bone(influence,second);
      if(rigid_bone>=0) {
        const auto& matrix=pose.skin_matrices[rigid_bone];
        position=transform_point(matrix,source.position);
        normal=transform_direction(matrix,source.normal);
      } else if(model.dual_quaternion_skinning) {
        const auto blended=blend_dual_quaternions(dual_quaternions,influence,second);
        const Quaternion conjugate{blended.real.w,-blended.real.v};
        const Vec3 translation=quaternion_product(blended.dual,conjugate).v*2.f;
        position=quaternion_rotate(blended.real,source.position)+translation;
        normal=quaternion_rotate(blended.real,source.normal);
      } else {
        const auto& a=pose.skin_matrices[influence.first];
        const auto& b=pose.skin_matrices[influence.second];
        const auto& c=pose.skin_matrices[influence.third];
        position=transform_point(a,source.position)*influence.first_weight+transform_point(b,source.position)*second+
                 transform_point(c,source.position)*influence.third_weight;
        normal=transform_direction(a,source.normal)*influence.first_weight+transform_direction(b,source.normal)*second+
               transform_direction(c,source.normal)*influence.third_weight;
      }
    }
    normal=normalize(normal);
    // Cache positions independently of corner normals. Hard edges and UV seams
    // may duplicate a position/weight pair with different normals; letting a
    // normal-only change refresh position would split those sewn copies.
    const bool position_changed=!same(output.vertices[i].position,position);
    if(position_changed) output.vertices[i].position=position;
    bool rebuild_normal=false;
    for(const auto& region:model.deformed_normal_regions)
      if(i>=region.vertex_begin && i-region.vertex_begin<region.vertex_count) {
        rebuild_normal=true; break;
      }
    if(position_changed) changed=true;
    if(!rebuild_normal && (position_changed || !same(output.vertices[i].normal,normal))) {
      output.vertices[i].normal=normal; changed=true;
    }
  }
  // No temporary allocation and no transient normal writes: repeated poses
  // compare only their final values, preserving exact hold/revision behavior.
  for(const auto& region:model.deformed_normal_regions)
    for(const auto& e:region.edge_attachments) {
      const auto p=output.vertices[e.first].position*(1-e.fraction)+
                   output.vertices[e.second].position*e.fraction;
      auto& v=output.vertices[e.vertex];
      if(!same(v.position,p)){v.position=p;changed=true;}
    }
  for(const auto& region:model.deformed_normal_regions) {
    // Input validation above guarantees a nonempty, triangle-aligned reference list.
    const auto bounds=std::minmax_element(region.triangles.begin(),region.triangles.end());
    if((*bounds.second-*bounds.first)/3<4096) {
      if(rebuild_cached_deformed_normals(region,output,*bounds.first)) changed=true;
      continue;
    }
    for(std::size_t g=0;g+1<region.member_offsets.size();++g) {
      // Unit-face votes keep tiny UV-cut and decimated faces from having
      // their shading normal dominated by a much larger neighboring face.
      Vec3 sum{};
      for(auto q=region.triangle_offsets[g];q<region.triangle_offsets[g+1];++q) {
        const auto t=region.triangles[q];
        const auto a=output.vertices[output.indices[t]].position;
        const auto b=output.vertices[output.indices[t+1]].position;
        const auto c=output.vertices[output.indices[t+2]].position;
        sum+=normalize(cross(b-a,c-a));
      }
      const auto normal=normalize(sum);
      for(auto q=region.member_offsets[g];q<region.member_offsets[g+1];++q) {
        auto& v=output.vertices[region.members[q]];
        if(!same(v.normal,normal)){v.normal=normal;changed=true;}
      }
    }
  }
  if(changed) output.mark_dirty();
  return changed;
}
}  // namespace fury
