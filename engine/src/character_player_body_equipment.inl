// Actor-owned sling/pouch construction. Every strap point is fitted to the
// triangulated shirt, with its barycentric skin weights; no reference assets.
struct PlayerShirtHit {
  Vec3 point, normal;
  CharacterInfluence weights;
};
PlayerShirtHit player_shirt_hit(const player_body_shirt_detail::ShirtSupport &shirt,
Vec3 guide) {
  const Vec3 origin{
    0, std::clamp(guide.y,.12f,.285f),0
  };
  const Vec3 direction = normalize(guide-origin);
  float best=-1;
  PlayerShirtHit result{};
  for (const auto &tri: shirt.triangles) {
    const auto &a=shirt.nodes[tri[0]], &c=shirt.nodes[tri[1]], &d=shirt.nodes[tri[2]];
    const Vec3 e=c.p-a.p, f=d.p-a.p, q=cross(direction,f);
    const float det=dot(e,q);
    if(std::fabs(det)<1e-12f) continue;
    const Vec3 off=origin-a.p, r=cross(off,e);
    const float u=dot(off,q)/det,v=dot(direction,r)/det,t=dot(f,r)/det;
    if(u<-.00001f || v<-.00001f || u+v>1.00001f || t<=best)continue;
    best=t;
    result.point=origin+direction*t;
    result.normal=normalize(cross(e,f));
    if(dot(result.normal,direction)<0)result.normal=-result.normal;
    std::array<float,character_bone_count> w{};
    const P pts[]{
      a,c,d
    };
    const float bw[]{
      1-u-v,u,v
    };
    for(int j=0;j<3;++j) {
      const auto &s=pts[j].w;
      w[s.first]+=bw[j]*s.first_weight;
      w[s.second]+=bw[j]*(1-s.first_weight-s.third_weight);
      w[s.third]+=bw[j]*s.third_weight;
    }
    std::array<int,character_bone_count> order{};
    for(std::size_t j=0;j<order.size();++j)order[j]=int(j);
    std::stable_sort(order.begin(),order.end(),[&](int l,int r){
      return w[l]>w[r];
    });
    const float total=w[order[0]]+w[order[1]]+w[order[2]];
    result.weights={
      std::uint8_t(order[0]),std::uint8_t(order[1]),w[order[0]]/total,std::uint8_t(order[2]),w[order[2]]/total
    };
  }
  if(best<=0)throw std::runtime_error("PlayerBody equipment lacks shirt support");
  return result;
}
Vec3 player_sling_guide(float t) {
  static const std::array<Vec3,8> points{
    {
      {
        -.074f,.305f,.102f
      },{
        .015f,.186f,.103f
      },{
        .079f,.087f,.086f
      },
      {
        .113f,.083f,.005f
      },{
        .076f,.100f,-.085f
      },{
        -.015f,.220f,-.094f
      },
      {
        -.075f,.312f,-.070f
      },{
        -.087f,.360f,.012f
      }
    }
  };
  const float x=t*points.size();
  const int k=int(std::floor(x));
  const float u=x-std::floor(x);
  const auto at=[&](int j){
    return points[std::size_t((j+int(points.size())*2)%int(points.size()))];
  };
  return (at(k)*2+(at(k+1)-at(k-1))*u+(at(k-1)*2-at(k)*5+at(k+1)*4-at(k+2))*(u*u)+(-at(k-1)+at(k)*3-at(k+1)*3+at(k+2))*(u*u*u))*.5f;
}
void make_player_body_equipment(Sculpt &b,const player_body_shirt_detail::ShirtSupport &shirt) {
  const Vec3 dark{
    .055f,.065f,.075f
  };
  // Clip the sling directly from this LOD's sewn-shirt facets. A separately
  // sampled analytic ribbon cut through coarse shoulder/waist chords even when
  // its vertices were above the shirt. Shared facet support fixes that cause.
  struct CutPoint {
    Vec3 p,n;
    CharacterInfluence w;
    float d;
  };
  std::vector<Vec3> normal(shirt.nodes.size());
  for(auto tri:shirt.triangles) {
    const Vec3 n=cross(shirt.nodes[tri[1]].p-shirt.nodes[tri[0]].p,shirt.nodes[tri[2]].p-shirt.nodes[tri[0]].p);
    for(auto i:tri)normal[i]+=n;
  }
  for(auto &n:normal)n=normalize(n);
  std::vector<PlayerShirtHit> center;
  constexpr int steps=192;
  for(int i=0;i<=steps;++i)center.push_back(player_shirt_hit(shirt,player_sling_guide(float(i)/steps)));
  const auto distance=[&](Vec3 p){
    float best=1.f,signed_distance=1.f;
    for(int i=0;i<steps;++i){
      const Vec3 tangent=center[i+1].point-center[i].point;
      const float q=dot(tangent,tangent);
      if(q<1e-12f)continue;
      const float t=std::clamp(dot(p-center[i].point,tangent)/q,0.f,1.f);
      const Vec3 near=center[i].point+tangent*t,diff=p-near;
      const float d=length(diff);
      if(d<best){
        best=d;
        const Vec3 n=normalize(center[i].normal*(1-t)+center[i+1].normal*t);
        signed_distance=std::copysign(d,dot(diff,cross(tangent,n)));
      }
    }return signed_distance;
  };
  const auto mix_weight=[](CharacterInfluence a,CharacterInfluence c,float t){
    std::array<float,character_bone_count>w{};
    for(auto pair:{
      std::pair<CharacterInfluence,float>{
        a,1-t
      },{
        c,t
      }
    }){
      auto s=pair.first;
      float v=pair.second;
      w[s.first]+=s.first_weight*v;
      w[s.second]+=(1-s.first_weight-s.third_weight)*v;
      w[s.third]+=s.third_weight*v;
    }
    std::array<unsigned,character_bone_count>order{};
    for(unsigned i=0;i<order.size();++i)order[i]=i;
    std::stable_sort(order.begin(),order.end(),[&](unsigned a,unsigned c){
      return w[a]>w[c];
    });
    float sum=w[order[0]]+w[order[1]]+w[order[2]];
    return CharacterInfluence{
      std::uint8_t(order[0]),std::uint8_t(order[1]),w[order[0]]/sum,std::uint8_t(order[2]),w[order[2]]/sum
    };
  };
  const auto clip=[&](std::vector<CutPoint> polygon,float limit,bool below){
    std::vector<CutPoint> result;
    if(polygon.empty())return result;
    for(std::size_t i=0;i<polygon.size();++i){
      const auto&a=polygon[i],&c=polygon[(i+1)%polygon.size()];
      bool ia=below?a.d<=limit:a.d>=limit,ic=below?c.d<=limit:c.d>=limit;
      if(ia)result.push_back(a);
      if(ia!=ic){
        float t=(limit-a.d)/(c.d-a.d);
        result.push_back({
          a.p*(1-t)+c.p*t,normalize(a.n*(1-t)+c.n*t),mix_weight(a.w,c.w,t),limit
        });
      }
    }
    return result;
  };
  std::vector<CutPoint> nodes;
  std::vector<std::array<unsigned,3>> faces;
  std::map<std::array<long long,3>,unsigned> lookup;
  const auto node=[&](const CutPoint&p){
    const std::array<long long,3>key{
      std::llround(p.p.x*1e6),std::llround(p.p.y*1e6),std::llround(p.p.z*1e6)
    };
    // Clip arithmetic traverses shared source edges in opposite directions.
    // A neighboring-bin search identifies the same physical intersection even
    // when the final float lies on the opposite side of a quantization boundary.
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){
      auto it=lookup.find({
        key[0]+x,key[1]+y,key[2]+z
      });
      if(it==lookup.end())continue;
      const Vec3 d=nodes[it->second].p-p.p;
      if(std::fabs(d.x)<1e-6f&&std::fabs(d.y)<1e-6f&&std::fabs(d.z)<1e-6f)return it->second;
    }
    unsigned i=nodes.size();
    lookup.emplace(key,i);
    nodes.push_back(p);
    return i;
  };
  std::vector<float> distances;
  for(const auto&p:shirt.nodes)distances.push_back(distance(p.p));
  for(auto tri:shirt.triangles){
    const float radius=std::max({
      length(shirt.nodes[tri[0]].p-shirt.nodes[tri[1]].p),length(shirt.nodes[tri[1]].p-shirt.nodes[tri[2]].p),length(shirt.nodes[tri[2]].p-shirt.nodes[tri[0]].p)
    });
    if(std::min({
      std::fabs(distances[tri[0]]),std::fabs(distances[tri[1]]),std::fabs(distances[tri[2]])
    })>radius+.00925f)continue;
    std::vector<CutPoint> poly;
    for(auto i:tri)poly.push_back({
      shirt.nodes[i].p,normal[i],shirt.nodes[i].w,distances[i]
    });
    poly=clip(clip(poly,.00925f,true),-.00925f,false);
    if(poly.size()<3)continue;
    for(unsigned j=1;j+1<poly.size();++j){
      auto a=node(poly[0]),c=node(poly[j]),d=node(poly[j+1]);
      if(a==c||c==d||d==a)continue;
      faces.push_back({
        a,c,d
      });
    }
  }
  const unsigned first=b.m.bind_mesh.vertices.size(),count=nodes.size();
  for(int layer=0;layer<2;++layer)for(const auto&p:nodes)b.vertex({
    p.p+p.n*(layer?.0040f:.0016f),dark,p.w,character_surface_uv(S::Leather,.5f+p.d/.03f,std::clamp((p.p.y-.04f)/.35f,0.f,1.f))
  });
  std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,unsigned>> boundary;
  std::map<std::pair<unsigned,unsigned>,unsigned> edges;
  for(auto tri:faces){
    b.tri(first+tri[0]+count,first+tri[1]+count,first+tri[2]+count);
    b.tri(first+tri[0],first+tri[2],first+tri[1]);
    for(int k=0;k<3;++k){
      auto a=tri[k],c=tri[(k+1)%3];
      auto key=std::minmax(a,c);
      ++edges[key];
      boundary[key]={
        a,c
      };
    }
  }
  for(auto entry:edges)if(entry.second==1){
    auto edge=boundary[entry.first];
    unsigned a=first+edge.first,c=first+edge.second;
    b.tri(a,c,c+count);
    b.tri(a,c+count,a+count);
  }
  // Preserve the original narrow center closure on actual shirt facets.
  // This low relief sits beneath the existing diagonal sling.
  for(auto tri:shirt.triangles) {
    if(shirt.nodes[tri[0]].p.z<=0 || shirt.nodes[tri[1]].p.z<=0 || shirt.nodes[tri[2]].p.z<=0) continue;
    std::vector<CutPoint> poly;
    for(auto i:tri) poly.push_back({shirt.nodes[i].p,normal[i],shirt.nodes[i].w,shirt.nodes[i].p.x});
    poly=clip(clip(poly,.0025f,true),-.0025f,false);
    for(auto &p:poly)p.d=p.p.y;
    poly=clip(clip(poly,.327f,true),.083f,false);
    for(unsigned j=1;j+1<poly.size();++j) {
      const CutPoint points[]{poly[0],poly[j],poly[j+1]};
      if(length(cross(points[1].p-points[0].p,points[2].p-points[0].p))<1e-11f)continue;
      const unsigned first_closure=b.m.bind_mesh.vertices.size();
      for(const auto&p:points)b.vertex({p.p+p.n*.0014f,b.m.appearance.shirt*.55f,p.w,character_surface_uv(S::Leather,(p.p.x+.0025f)/.005f,(p.p.y-.083f)/.244f)});
      b.tri(first_closure,first_closure+1,first_closure+2);
    }
  }
  // The original Near-only quiet metal buckle; waistband color follows the
  // existing trouser surface, adding no new Far geometry or pose weights.
  if(b.near) {
    const Vec3 c{0,.047f,.064f},r{.0085f,.006f,.002f};
    const std::array<Vec3,8> corners{{c+Vec3{-r.x,-r.y,-r.z},c+Vec3{r.x,-r.y,-r.z},c+Vec3{r.x,r.y,-r.z},c+Vec3{-r.x,r.y,-r.z},c+Vec3{-r.x,-r.y,r.z},c+Vec3{r.x,-r.y,r.z},c+Vec3{r.x,r.y,r.z},c+Vec3{-r.x,r.y,r.z}}};
    const int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,4,7,3},{1,2,6,5},{0,1,5,4},{3,7,6,2}};
    for(const auto&f:faces){unsigned first_buckle=b.m.bind_mesh.vertices.size();for(int i=0;i<4;++i)b.vertex({corners[f[i]],{.55f,.56f,.51f},wt(B::Pelvis),character_surface_uv(S::Metal,float(i==1||i==2),float(i>=2))});b.tri(first_buckle,first_buckle+1,first_buckle+2);b.tri(first_buckle,first_buckle+2,first_buckle+3);}
  }
  // Compact rounded back pouch. Its inner surface is supported by the actual
  // shirt across the whole footprint; shallow gussets carry its depth outward.
  const int around=b.near?40:20, depth_rows=b.near?8:4;
  const float cy=.202f,rx=.0575f,ry=.071f;
  const auto outline=[&](float a){
    const auto signed_power=[](float x){
      return std::copysign(std::pow(std::fabs(x),.42f),x);
    };
    return Vec3{
      rx*signed_power(std::cos(a)),cy+ry*signed_power(std::sin(a)),-.15f
    };
  };
  const std::size_t pouch_start=b.m.bind_mesh.indices.size();
  b.grid(around,depth_rows,[&](float u,float v){
    const float a=u==1.f?0.f:2*pi*u;Vec3 edge=outline(a);
    const float inset=.91f+.09f*std::sin(pi*v);
    edge.x*=inset;edge.y=cy+(edge.y-cy)*inset;
    auto support=player_shirt_hit(shirt,edge);
    Vec3 p=support.point; p.z-=.006f+.046f*v;
    p.x=edge.x;p.y=edge.y;
    return P{
      p,dark*(.95f-.12f*g(v,.05f,.07f)-.10f*g(v,.95f,.07f)),player_body_shirt_detail::body_weight(p.y),character_surface_uv(S::Leather,u,v)
    };
  },true,true);
  const std::size_t pouch_end=b.m.bind_mesh.indices.size();
  const auto pouch_back=[&](float x,float y) {
    float back=1.f;
    for(std::size_t k=pouch_start;k<pouch_end;k+=3) {
      const auto &mesh=b.m.bind_mesh;
      Vec3 a=mesh.vertices[mesh.indices[k]].position*(1/b.h),c=mesh.vertices[mesh.indices[k+1]].position*(1/b.h),d=mesh.vertices[mesh.indices[k+2]].position*(1/b.h);
      const Vec3 e=c-a,f=d-a;
      const float det=e.x*f.y-e.y*f.x;
      if(std::fabs(det)<1e-12f)continue;
      const float u=((x-a.x)*f.y-(y-a.y)*f.x)/det,v=(e.x*(y-a.y)-e.y*(x-a.x))/det;
      if(u>=-1e-5f&&v>=-1e-5f&&u+v<=1.00001f)back=std::min(back,a.z+u*e.z+v*f.z);
    }
    if(back>0)throw std::runtime_error("Player pouch trim lacks actual LOD support");
    return back;
  };
  // Real outer-facet support keeps the stitched flap above each LOD's pouch.
  b.grid(b.near?12:4,b.near?8:3,[&](float u,float v){
    const float x=lerp(-.046f,.046f,u),y=lerp(.215f,.259f,v);
    return P{
      {
        x,y,pouch_back(x,y)-.0009f
      },dark*.85f,player_body_shirt_detail::body_weight(y),character_surface_uv(S::Leather,u,v)
    };
  },false,false);
  // A narrow zipper seam on the supported face; hardware remains quiet and
  // unbranded. Far keeps the closure silhouette rather than tiny individual teeth.
  b.grid(b.near?16:6,2,[&](float u,float v){
    const float x=lerp(-.041f,.041f,u),y=.214f+v*.002f;
    return P{
      {
        x,y,pouch_back(x,y)-.0014f
      },dark*.56f,player_body_shirt_detail::body_weight(y),character_surface_uv(S::Leather,u,v)
    };
  },false,false);
  if(b.near) {
    std::vector<Vec3> stitch;
    for(int i=0;i<=18;++i){
      float x=lerp(-.044f,.044f,float(i)/18),y=.2175f;
      stitch.push_back({
        x,y,pouch_back(x,y)-.0017f
      });
    }
    b.tube(stitch,std::vector<float>(stitch.size(),.00028f),.6f,dark*1.35f,S::Leather,
    [&](float){
      return player_body_shirt_detail::body_weight(.2175f);
    },4);
  }
  // Small original chest accent, fitted instead of floating over the torso.
  b.grid(b.near?6:2,b.near?4:2,[&](float u,float v){
    const Vec3 guide{
      lerp(.034f,.066f,u),lerp(.235f,.216f,v),.13f
    };
    auto hit=player_shirt_hit(shirt,guide);
    return P{
      hit.point+hit.normal*.0025f,b.m.appearance.accent,hit.weights,character_surface_uv(S::Leather,u,v)
    };
  },false,false);
}
