// Fury DXR reference renderer. All transport is in scene-linear RGB.
// DXR 1.1 inline ray queries traverse hardware-built acceleration structures.
struct VertexData {
  float3 position; uint material;
  float3 normal; float pad0;
  float3 color; float pad1;
  float2 uv; float2 pad2;
  float3 previous_position; float pad3;
};
struct MaterialData {
  float4 albedo;
  float4 surface; // metallic, roughness, emissive, texture-array layer
  float4 optical; // wetness, transmission, IOR, reserved
  float4 shading; // alpha cutoff, normal scale, double sided, water
  float4 emission;
  float4 animation;
};
struct InstanceData {
  column_major float4x4 model,previous_model,normal_matrix;
  uint vertex_offset,material,pad0,pad1;
};
cbuffer Frame : register(b0) {
  column_major float4x4 inverse_vp;
  column_major float4x4 current_vp;
  column_major float4x4 previous_vp;
  float4 camera_time;
  float4 sun_direction_intensity;
  float4 sun_color;
  float4 ambient;
  float4 fog_color_start;
  float4 fog_end_exposure;
  float4 point_position_radius[4];
  float4 point_color_intensity[4];
  uint4 dimensions; // render width/height, display width/height
  uint4 sampling; // frame, samples per pixel, bounces, accumulated frames
  float4 jitter_filter; // sample offset in render pixels, denoise, path mode
  uint4 options; // reset, point count, native accumulation, use upscaler
  float4 previous_jitter; // xy previous sample offset; z temporal denoise enabled
  float4 previous_camera;
};
RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<VertexData> vertices : register(t1);
StructuredBuffer<MaterialData> materials : register(t2);
StructuredBuffer<InstanceData> instances : register(t3);
Texture2D<float4> radiance_input : register(t5);
Texture2D<float4> geometry_input : register(t6);
Texture2D<float4> filtered_input : register(t7);
Texture2D<float4> upscaled_input : register(t8);
Texture2D<float4> history_color : register(t9);
Texture2D<float4> history_geometry : register(t10);
Texture2D<float4> temporal_input : register(t11);
Texture2D<float4> sky_input : register(t12);
Texture2D<float4> direct_input : register(t13);
Texture2D<float> depth_input : register(t14);
Texture2D<float2> motion_input : register(t15);
Texture2D<float4> material_textures[256] : register(t16);
SamplerState wrap_sampler : register(s0);
SamplerState clamp_sampler : register(s1);
RWTexture2D<float4> radiance_output : register(u0);
RWTexture2D<float> depth_output : register(u1);
RWTexture2D<float2> motion_output : register(u2);
RWTexture2D<float4> geometry_output : register(u3);
RWTexture2D<float4> filtered_output : register(u4);
RWTexture2D<float> reactive_output : register(u5);
RWTexture2D<float4> temporal_output : register(u6);
RWTexture2D<float4> temporal_geometry : register(u7);
RWTexture2D<float4> sky_output : register(u8);
RWTexture2D<float4> direct_output : register(u9);

static const float PI = 3.14159265359;
uint hash_uint(uint x) {
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
  return x;
}
float random(inout uint seed) {
  seed = hash_uint(seed + 0x9e3779b9);
  return float(seed >> 8) * (1.0 / 16777216.0);
}
float3 safe_normalize(float3 v) { return v * rsqrt(max(dot(v,v), 1e-20)); }
float3 tangent(float3 n) {
  return safe_normalize(cross(abs(n.y) < .95 ? float3(0,1,0) : float3(1,0,0), n));
}
float3 local_to_world(float3 v, float3 n) {
  float3 t = tangent(n);
  return safe_normalize(t*v.x + cross(n,t)*v.y + n*v.z);
}
float3 cosine_sample(float3 n, inout uint seed) {
  float r = sqrt(random(seed)), phi = 2*PI*random(seed);
  return local_to_world(float3(r*cos(phi), r*sin(phi), sqrt(max(0,1-r*r))), n);
}
float2 atmosphere_intersection(float3 p,float3 d,float radius) {
  float b=dot(p,d),c=dot(p,p)-radius*radius,disc=b*b-c;
  if(disc<0) return float2(-1,-1);
  float root=sqrt(disc); return float2(-b-root,-b+root);
}
float2 atmosphere_density(float3 position) {
  float height=max(0,length(position)-6360);
  return exp(-height/float2(8,1.2));
}
[numthreads(8,8,1)]
void sky_cs(uint3 dispatch_id:SV_DispatchThreadID) {
  uint width,height; sky_output.GetDimensions(width,height);
  if(any(dispatch_id.xy>=uint2(width,height))) return;
  float2 uv=(float2(dispatch_id.xy)+.5)/float2(width,height);
  float theta=uv.y*PI,phi=(uv.x-.5)*2*PI;
  float3 direction=float3(sin(theta)*cos(phi),cos(theta),sin(theta)*sin(phi));
  float3 origin=float3(0,6360.05,0),sun=-sun_direction_intensity.xyz;
  float2 planet=atmosphere_intersection(origin,direction,6360);
  if(planet.x>0) { sky_output[dispatch_id.xy]=float4(ambient.rgb*.12,1); return; }
  float distance=atmosphere_intersection(origin,direction,6440).y;
  const float3 beta_rayleigh=float3(.0058,.0135,.0331);
  const float3 beta_mie=.015.xxx;
  float2 view_depth=0; float3 rayleigh=0,mie=0;
  const uint steps=16,light_steps=6;
  float step=distance/steps;
  for(uint i=0;i<steps;++i) {
    float3 sample_position=origin+direction*((i+.5)*step);
    float2 density=atmosphere_density(sample_position);
    view_depth+=density*step*.5;
    float2 ground=atmosphere_intersection(sample_position,sun,6360);
    if(ground.x<0) {
      float light_distance=atmosphere_intersection(sample_position,sun,6440).y;
      float light_step=light_distance/light_steps; float2 light_depth=0;
      for(uint j=0;j<light_steps;++j) light_depth+=atmosphere_density(sample_position+sun*((j+.5)*light_step))*light_step;
      float3 transmission=exp(-(view_depth.x+light_depth.x)*beta_rayleigh-(view_depth.y+light_depth.y)*beta_mie*1.1);
      rayleigh+=density.x*transmission*step; mie+=density.y*transmission*step;
    }
    view_depth+=density*step*.5;
  }
  float cosine=dot(direction,sun),g=.76;
  float phase_r=3*(1+cosine*cosine)/(16*PI);
  float phase_m=(1-g*g)/(4*PI*pow(max(.001,1+g*g-2*g*cosine),1.5));
  float3 sky=(rayleigh*beta_rayleigh*phase_r+mie*beta_mie*phase_m)*20*max(.05,sun_direction_intensity.w/4.8);
  sky_output[dispatch_id.xy]=float4(max(0,sky),1);
}
float3 environment(float3 d,bool show_sun) {
  float2 uv=float2(atan2(d.z,d.x)/(2*PI)+.5,acos(clamp(d.y,-1,1))/PI);
  float3 sky=sky_input.SampleLevel(wrap_sampler,uv,0).rgb;
  // Direct-light sampling handles the sun for non-delta BSDF paths, avoiding
  // counting its contribution both in next-event estimation and a sky hit.
  float sun=show_sun ? smoothstep(.999988,.999990,dot(d,-sun_direction_intensity.xyz)) : 0;
  return sky+sun_color.rgb*sun_direction_intensity.w*sun/0.000068;
}
bool accept_triangle(uint instance_id,uint primitive,float2 bary,bool front,inout uint seed) {
  InstanceData instance=instances[instance_id];
  uint index=instance.vertex_offset+primitive*3;
  VertexData a=vertices[index],b=vertices[index+1],c=vertices[index+2];
  MaterialData m=materials[instance.material];
  if(!front && m.shading.z==0) return false;
  if(m.shading.x<0 && m.animation.z==0) return true;
  float2 uv=a.uv*(1-bary.x-bary.y)+b.uv*bary.x+c.uv*bary.y+m.animation.xy*camera_time.w;
  uint texture_index=NonUniformResourceIndex(uint(m.surface.w)*4);
  float vertex_alpha=a.pad1*(1-bary.x-bary.y)+b.pad1*bary.x+c.pad1*bary.y;
  float alpha=saturate(material_textures[texture_index].SampleLevel(wrap_sampler,uv,0).a*m.albedo.a*vertex_alpha);
  if(m.animation.z!=0) return alpha>=1 || (alpha>0 && random(seed)<alpha);
  return alpha>=m.shading.x;
}
bool occluded(float3 origin, float3 direction, float distance,inout uint seed) {
  RayDesc ray; ray.Origin = origin; ray.Direction = direction;
  ray.TMin = .001; ray.TMax = max(.002, distance);
  RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
  q.TraceRayInline(scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 255, ray);
  while(q.Proceed()) {
    if(q.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE &&
       accept_triangle(q.CandidateInstanceID(),q.CandidatePrimitiveIndex(),q.CandidateTriangleBarycentrics(),q.CandidateTriangleFrontFace(),seed))
      q.CommitNonOpaqueTriangleHit();
  }
  return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}
struct Hit {
  float3 position, previous_position, normal, geometric_normal, base, emission, absorption;
  float2 uv;
  float distance, metallic, roughness, emissive, transmission, ior, reactive;
  bool entering;
};
bool trace(float3 origin, float3 direction,inout uint seed, out Hit hit) {
  hit = (Hit)0;
  RayDesc ray; ray.Origin=origin; ray.Direction=direction; ray.TMin=.001; ray.TMax=10000;
  RayQuery<RAY_FLAG_NONE> q;
  q.TraceRayInline(scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 255, ray);
  while(q.Proceed()) {
    if(q.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE &&
       accept_triangle(q.CandidateInstanceID(),q.CandidatePrimitiveIndex(),q.CandidateTriangleBarycentrics(),q.CandidateTriangleFrontFace(),seed))
      q.CommitNonOpaqueTriangleHit();
  }
  if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT) return false;
  InstanceData instance=instances[q.CommittedInstanceID()];
  uint index = instance.vertex_offset+q.CommittedPrimitiveIndex() * 3;
  VertexData a=vertices[index], b=vertices[index+1], c=vertices[index+2];
  a.position=mul(instance.model,float4(a.position,1)).xyz;
  b.position=mul(instance.model,float4(b.position,1)).xyz;
  c.position=mul(instance.model,float4(c.position,1)).xyz;
  a.previous_position=mul(instance.previous_model,float4(a.previous_position,1)).xyz;
  b.previous_position=mul(instance.previous_model,float4(b.previous_position,1)).xyz;
  c.previous_position=mul(instance.previous_model,float4(c.previous_position,1)).xyz;
  a.normal=mul(instance.normal_matrix,float4(a.normal,0)).xyz;
  b.normal=mul(instance.normal_matrix,float4(b.normal,0)).xyz;
  c.normal=mul(instance.normal_matrix,float4(c.normal,0)).xyz;
  float2 bc=q.CommittedTriangleBarycentrics(); float3 w=float3(1-bc.x-bc.y,bc);
  MaterialData m=materials[instance.material];
  hit.distance=q.CommittedRayT(); hit.position=origin+direction*hit.distance;
  hit.previous_position=a.previous_position*w.x+b.previous_position*w.y+c.previous_position*w.z;
  hit.geometric_normal=safe_normalize(cross(b.position-a.position,c.position-a.position));
  hit.normal=safe_normalize(a.normal*w.x+b.normal*w.y+c.normal*w.z);
  hit.entering=dot(hit.normal,direction)<0;
  if (dot(hit.normal,direction)>0) hit.normal=-hit.normal;
  if (dot(hit.geometric_normal,direction)>0) hit.geometric_normal=-hit.geometric_normal;
  hit.uv=a.uv*w.x+b.uv*w.y+c.uv*w.z+m.animation.xy*camera_time.w;
  uint slot=uint(m.surface.w);
  uint color_texture=NonUniformResourceIndex(slot*4),normal_texture=NonUniformResourceIndex(slot*4+1);
  uint surface_texture=NonUniformResourceIndex(slot*4+2),emission_texture=NonUniformResourceIndex(slot*4+3);
  uint texture_width,texture_height,mip_count;
  material_textures[color_texture].GetDimensions(0,texture_width,texture_height,mip_count);
  float density=max(length(b.uv-a.uv)/max(length(b.position-a.position),1e-5),
                    length(c.uv-a.uv)/max(length(c.position-a.position),1e-5));
  // Reconstruction needs texture detail at the output pixel footprint. This is
  // equivalent to log2(render/display) mip bias rather than baking low-res blur.
  uint footprint_height=options.w!=0 ? dimensions.w : dimensions.y;
  float footprint=2*hit.distance*fog_end_exposure.z/max(footprint_height,1u);
  float lod=clamp(log2(max(density*max(texture_width,texture_height)*footprint,1)),0,float(mip_count-1));
  hit.base=max(0, m.albedo.rgb*(a.color*w.x+b.color*w.y+c.color*w.z));
  float4 base_sample=material_textures[color_texture].SampleLevel(wrap_sampler,hit.uv,lod);
  hit.base *= base_sample.rgb;
  float4 packed_surface=material_textures[surface_texture].SampleLevel(wrap_sampler,hit.uv,lod);
  hit.metallic=saturate(m.surface.x*packed_surface.b); hit.roughness=clamp(m.surface.y*packed_surface.g,.045,1);
  hit.emission=m.emission.rgb*m.surface.z*material_textures[emission_texture].SampleLevel(wrap_sampler,hit.uv,lod).rgb;
  hit.emissive=max(0,m.surface.z); hit.transmission=saturate(m.optical.y);
  hit.ior=max(1.001,m.optical.z); hit.reactive=max(saturate(max(hit.emission.x,max(hit.emission.y,hit.emission.z))*.1),m.optical.w);
  if(m.animation.z!=0) hit.reactive=max(hit.reactive,1-base_sample.a*m.albedo.a);
  hit.absorption=-log(clamp(hit.base,.01,1))*.1;
  if (m.shading.w==0) {
    float2 d1=b.uv-a.uv,d2=c.uv-a.uv;
    float det=d1.x*d2.y-d1.y*d2.x;
    if (abs(det)>1e-8) {
      float3 t=((b.position-a.position)*d2.y-(c.position-a.position)*d1.y)/det;
      t=safe_normalize(t-hit.normal*dot(t,hit.normal));
      float3 bitangent=cross(hit.normal,t)*sign(det);
      float3 map=material_textures[normal_texture].SampleLevel(wrap_sampler,hit.uv,lod).xyz*2-1;
      map.xy*=m.shading.y;
      hit.normal=safe_normalize(t*map.x+bitangent*map.y+hit.normal*map.z);
      if(dot(hit.normal,hit.geometric_normal)<.1) hit.normal=hit.geometric_normal;
    }
  }
  if (m.optical.x>0) { hit.roughness=lerp(hit.roughness,.09,saturate(m.optical.x)); hit.base*=1-.35*saturate(m.optical.x); }
  if (m.shading.w!=0) {
    float2 p=hit.position.xz;
    float t=camera_time.w;
    float sx=.075*cos(p.x*.75+p.y*.3+t*.8)+.025*cos(p.x*2.1-p.y*1.5+t*1.7);
    float sz=.055*cos(p.y*.8-p.x*.2-t*.6)+.025*sin(p.y*2.3+p.x*1.1+t*1.5);
    float wave_visibility=1-smoothstep(.12,1.5,footprint);
    hit.normal=safe_normalize(hit.normal+float3(sx,0,sz)*wave_visibility);
    hit.base=lerp(hit.base,float3(.035,.12,.105),.7);
    hit.roughness=.065; hit.transmission=.86; hit.ior=1.333; hit.reactive=max(hit.reactive,.12);
    hit.absorption=float3(.28,.08,.035);
  }
  return true;
}
float3 fresnel(float v, float3 f0) { return f0+(1-f0)*pow(1-saturate(v),5); }
float distribution(float nh, float roughness) {
  float a=roughness*roughness, a2=a*a;
  float d=nh*nh*(a2-1)+1;
  return a2/max(PI*d*d,1e-8);
}
float masking(float nv, float roughness) {
  float a=roughness*roughness;
  return 2*nv/max(nv+sqrt(a*a+(1-a*a)*nv*nv),1e-6);
}
float3 brdf(Hit h, float3 v, float3 l) {
  float nl=saturate(dot(h.normal,l)), nv=saturate(dot(h.normal,v));
  if(nl<=0 || nv<=0) return 0;
  float3 hv=safe_normalize(l+v);
  float3 f=fresnel(dot(v,hv),lerp(.04.xxx,h.base,h.metallic));
  float d=distribution(saturate(dot(h.normal,hv)),h.roughness);
  float g=masking(nv,h.roughness)*masking(nl,h.roughness);
  return ((1-f)*(1-h.metallic)*h.base/PI+f*(d*g/max(4*nv*nl,1e-6)))*(1-h.transmission);
}
float3 direct_light(Hit h, float3 v, inout uint seed) {
  float3 result=0;
  float3 sun=safe_normalize(-sun_direction_intensity.xyz);
  float r=.00465*sqrt(random(seed)), p=2*PI*random(seed);
  float3 t=tangent(sun);
  float3 l=safe_normalize(sun+t*(r*cos(p))+cross(sun,t)*(r*sin(p)));
  float nl=saturate(dot(h.normal,l));
  if(nl>0 && !occluded(h.position+h.geometric_normal*.003,l,10000,seed))
    result+=brdf(h,v,l)*sun_color.rgb*sun_direction_intensity.w*nl;
  for(uint i=0;i<min(options.y,4u);++i) {
    float3 delta=point_position_radius[i].xyz-h.position;
    float distance=length(delta); l=delta/max(distance,.001);
    float falloff=saturate(1-pow(distance/max(point_position_radius[i].w,.01),4));
    nl=saturate(dot(h.normal,l));
    if(nl>0 && falloff>0 && !occluded(h.position+h.geometric_normal*.003,l,distance-.005,seed))
      result+=brdf(h,v,l)*point_color_intensity[i].rgb*point_color_intensity[i].w*
              nl*falloff*falloff/max(distance*distance,.04);
  }
  return result;
}
float3 transport(float3 origin,float3 direction,inout uint seed,out Hit primary,out bool primary_hit,out float3 primary_direct) {
  float3 radiance=0,throughput=1,medium_absorption=0;
  primary=(Hit)0; primary_hit=false; primary_direct=0;
  bool delta_path=false;
  uint bounces=jitter_filter.w>0 ? sampling.z : 2;
  for(uint bounce=0;bounce<bounces;++bounce) {
    Hit h;
    if(!trace(origin,direction,seed,h)) {
      float3 sky=environment(direction,bounce==0 || delta_path);
      radiance+=throughput*exp(-medium_absorption*50)*sky; if(bounce==0) primary_direct=sky; break;
    }
    if(bounce==0) { primary=h; primary_hit=true; }
    throughput*=exp(-medium_absorption*h.distance);
    float3 v=-direction;
    float3 direct=h.emission+direct_light(h,v,seed);
    if(bounce==0) primary_direct=direct;
    radiance+=throughput*direct;
    if(jitter_filter.w==0) {
      radiance+=throughput*h.base*ambient.rgb*(1-h.metallic)*(1-h.transmission);
      if(h.roughness>.35 && h.transmission<.1) break;
    }
    float3 next_direction;
    if(h.transmission>0 && random(seed)<h.transmission) {
      delta_path=true;
      float f0=pow((h.ior-1)/(h.ior+1),2);
      float f=f0+(1-f0)*pow(1-saturate(dot(h.normal,v)),5);
      float reflection_probability=clamp(f,.15,.85);
      if(random(seed)<reflection_probability) {
        next_direction=reflect(direction,h.normal);
        throughput*=f/reflection_probability;
      }
      else {
        next_direction=refract(direction,h.normal,h.entering ? 1/h.ior : h.ior);
        if(dot(next_direction,next_direction)<.1) {
          next_direction=reflect(direction,h.normal);
          throughput*=(1-f)/(1-reflection_probability);
        } else {
          throughput*=(1-f)/(1-reflection_probability);
          medium_absorption=h.entering ? h.absorption : 0;
        }
      }
    } else {
      delta_path=false;
      float spec_probability=lerp(.25,.9,h.metallic);
      if(random(seed)<spec_probability) {
        float u=random(seed),phi=2*PI*random(seed),a=h.roughness*h.roughness;
        float cos_theta=sqrt((1-u)/max(1+(a*a-1)*u,1e-6));
        float sin_theta=sqrt(max(0,1-cos_theta*cos_theta));
        float3 half_vector=local_to_world(float3(sin_theta*cos(phi),sin_theta*sin(phi),cos_theta),h.normal);
        next_direction=reflect(direction,half_vector);
      } else next_direction=cosine_sample(h.normal,seed);
      float nl=dot(h.normal,next_direction);
      if(nl<=0) break;
      float3 hv=safe_normalize(v+next_direction);
      float nh=saturate(dot(h.normal,hv)),vh=saturate(dot(v,hv));
      float pdf=(1-spec_probability)*nl/PI+spec_probability*distribution(nh,h.roughness)*nh/max(4*vh,1e-6);
      throughput*=brdf(h,v,next_direction)*nl/max(pdf,1e-6)/max(1-h.transmission,.01);
    }
    if(any(!isfinite(throughput))) break;
    if(bounce>=2) {
      float survive=clamp(max(throughput.x,max(throughput.y,throughput.z)),.05,.95);
      if(random(seed)>survive) break;
      throughput/=survive;
    }
    direction=safe_normalize(next_direction);
    origin=h.position+h.geometric_normal*(dot(direction,h.geometric_normal)>0 ? .003 : -.003);
  }
  return max(radiance,0);
}

[numthreads(8,8,1)]
void trace_cs(uint3 dispatch_id:SV_DispatchThreadID) {
  uint2 pixel=dispatch_id.xy;
  if(any(pixel>=dimensions.xy)) return;
  uint seed=hash_uint(pixel.x+pixel.y*dimensions.x+sampling.x*0x9e3779b9);
  float3 sum=0,direct_sum=0; Hit first=(Hit)0; bool first_hit=false;
  for(uint sample=0;sample<sampling.y;++sample) {
    // Temporal reconstruction receives one declared camera jitter per frame.
    // Additional transport samples must retain that same primary-ray projection.
    float2 offset=(sample==0 || options.w!=0) ? jitter_filter.xy : float2(random(seed)-.5,random(seed)-.5);
    float2 uv=(float2(pixel)+.5+offset)/float2(dimensions.xy);
    float4 far_point=mul(inverse_vp,float4(uv.x*2-1,1-uv.y*2,1,1));
    float3 direction=safe_normalize(far_point.xyz/far_point.w-camera_time.xyz);
    Hit h; bool valid; float3 direct;
    float3 color=transport(camera_time.xyz,direction,seed,h,valid,direct);
    if(valid) {
      float fog=1-exp(-max(0,h.distance-fog_color_start.w)/max(1,fog_end_exposure.x)*.35);
      color=lerp(color,fog_color_start.rgb,fog);
      direct*=1-fog;
    }
    sum+=max(color-direct,0); direct_sum+=direct;
    if(sample==0) { first=h; first_hit=valid; }
  }
  float3 result=sum/max(1u,sampling.y);
  if(any(!isfinite(result))) result=0;
  result=clamp(result,0,65000);
  radiance_output[pixel]=float4(result,1);
  float3 direct=direct_sum/max(1u,sampling.y);
  if(any(!isfinite(direct))) direct=0;
  direct=clamp(direct,0,65000);
  if(options.z!=0 && options.x==0 && sampling.w>0)
    direct=lerp(direct_output[pixel].rgb,direct,1.0/(min(sampling.w,1023u)+1));
  direct_output[pixel]=float4(direct,1);
  if(first_hit) {
    float4 clip=mul(current_vp,float4(first.position,1));
    float4 previous=mul(previous_vp,float4(first.previous_position,1));
    float2 current_uv=clip.xy/clip.w*float2(.5,-.5)+.5;
    bool valid_previous=previous.w>1e-4;
    float2 previous_uv=valid_previous ? previous.xy/previous.w*float2(.5,-.5)+.5 : current_uv;
    depth_output[pixel]=saturate(clip.z/clip.w);
    motion_output[pixel]=clamp((previous_uv-current_uv)*float2(dimensions.zw),-32000,32000);
    geometry_output[pixel]=float4(first.normal,first.distance);
    reactive_output[pixel]=valid_previous ? first.reactive : 1;
  } else {
    float2 uv=(float2(pixel)+.5+jitter_filter.xy)/float2(dimensions.xy);
    float4 far_point=mul(inverse_vp,float4(uv.x*2-1,1-uv.y*2,1,1));
    float3 direction=safe_normalize(far_point.xyz/far_point.w-camera_time.xyz);
    float4 previous=mul(previous_vp,float4(direction,0));
    float2 previous_uv=previous.w>1e-4 ? previous.xy/previous.w*float2(.5,-.5)+.5 : uv;
    motion_output[pixel]=clamp((previous_uv-uv)*float2(dimensions.zw),-32000,32000);
    depth_output[pixel]=1; geometry_output[pixel]=float4(0,0,0,10000); reactive_output[pixel]=0;
  }
}

[numthreads(8,8,1)]
void temporal_cs(uint3 dispatch_id:SV_DispatchThreadID) {
  int2 p=int2(dispatch_id.xy);
  if(any(p>=int2(dimensions.xy))) return;
  float4 color=radiance_input[p],g=geometry_input[p];
  if(options.z!=0 && options.x==0 && sampling.w>0) {
    // A stationary reference integrates a pixel's subpixel samples directly.
    // Reprojecting jitter here would repeatedly resample and blur the reference.
    float4 old=history_color[p];
    float count=min(old.a+1,1024);
    temporal_output[p]=float4(lerp(old.rgb,color.rgb,1/count),count);
    temporal_geometry[p]=g;
    return;
  }
  float count=1;
  bool enabled=options.z!=0 || previous_jitter.z!=0;
  float2 previous_pixel=float2(p)+.5+jitter_filter.xy-previous_jitter.xy+
                        motion_output[p]*float2(dimensions.xy)/float2(dimensions.zw);
  bool in_bounds=all(previous_pixel>=.5) && all(previous_pixel<float2(dimensions.xy)-.5);
  if(enabled && options.x==0 && in_bounds) {
    int2 previous_p=int2(previous_pixel);
    float4 old_g=history_geometry[previous_p];
    float4 old=history_color.SampleLevel(clamp_sampler,previous_pixel/float2(dimensions.xy),0);
    float position_tolerance=max(.025,g.w*.02+length(camera_time.xyz-previous_camera.xyz)*2);
    bool matching=(g.w>9999 && old_g.w>9999) ||
                   (dot(g.xyz,old_g.xyz)>.9 && abs(g.w-old_g.w)<position_tolerance);
    if(matching && all(isfinite(old.rgb))) {
      {
        float3 mean=0,second=0;
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
          int2 q=clamp(p+int2(x,y),int2(0,0),int2(dimensions.xy)-1);
          float3 value=radiance_input[q].rgb;
          mean+=value/9; second+=value*value/9;
        }
        float3 deviation=sqrt(max(second-mean*mean,0));
        float3 previous=clamp(old.rgb,mean-3*deviation-.03,mean+3*deviation+.03);
        count=min(old.a+1,24*(1-.7*reactive_output[p]));
        color.rgb=lerp(previous,color.rgb,1/max(count,1));
      }
    }
  }
  temporal_output[p]=float4(color.rgb,count);
  temporal_geometry[p]=g;
}

[numthreads(8,8,1)]
void filter_cs(uint3 dispatch_id:SV_DispatchThreadID) {
  int2 pixel=int2(dispatch_id.xy);
  if(any(pixel>=int2(dimensions.xy))) return;
  float4 center=temporal_input[pixel];
  float3 direct=direct_input[pixel].rgb;
  if(jitter_filter.z==0 || (options.z!=0 && sampling.w>32)) { filtered_output[pixel]=float4(clamp(center.rgb+direct,0,65000),1); return; }
  float4 geometry=geometry_input[pixel];
  float3 sum=0; float weight_sum=0;
  for(int y=-2;y<=2;++y) for(int x=-2;x<=2;++x) {
    int2 p=clamp(pixel+int2(x,y),int2(0,0),int2(dimensions.xy)-1);
    float4 g=geometry_input[p];
    float nw=geometry.w>9999 && g.w>9999 ? 1 : pow(saturate(dot(geometry.xyz,g.xyz)),32);
    float dw=exp(-abs(g.w-geometry.w)/max(.015,geometry.w*.004));
    float weight=exp(-float(x*x+y*y)*.3)*nw*dw;
    sum+=temporal_input[p].rgb*weight; weight_sum+=weight;
  }
  filtered_output[pixel]=float4(clamp((weight_sum>1e-6 ? sum/weight_sum : center.rgb)+direct,0,65000),1);
}

struct Fullscreen { float4 position:SV_Position; float2 uv:TEXCOORD; };
Fullscreen present_vs(uint id:SV_VertexID) {
  Fullscreen o;
  o.uv=float2((id<<1)&2,id&2);
  o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);
  return o;
}
float3 aces_fitted(float3 color) {
  return saturate((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14));
}
float4 present_ps(Fullscreen input):SV_Target {
  uint view=uint(fog_end_exposure.w);
  if(view==1) return float4(pow(saturate(depth_input.SampleLevel(clamp_sampler,input.uv,0)),80).xxx,1);
  if(view==2) return float4(geometry_input.SampleLevel(clamp_sampler,input.uv,0).xyz*.5+.5,1);
  if(view==3) return float4(saturate(.5+motion_input.SampleLevel(clamp_sampler,input.uv,0)/32),.5,1);
  float3 hdr=options.w!=0 ? upscaled_input.SampleLevel(clamp_sampler,input.uv,0).rgb :
                           filtered_input.SampleLevel(clamp_sampler,input.uv,0).rgb;
  if(view==4) hdr=direct_input.SampleLevel(clamp_sampler,input.uv,0).rgb;
  if(view==5) hdr=radiance_input.SampleLevel(clamp_sampler,input.uv,0).rgb;
  float3 mapped=aces_fitted(max(0,hdr)*fog_end_exposure.y);
  float3 srgb=select(mapped<=.0031308,12.92*mapped,1.055*pow(mapped,1.0/2.4)-.055);
  return float4(srgb,1);
}
struct HudInput { float2 position:POSITION; float4 color:COLOR; };
struct HudOutput { float4 position:SV_Position; float4 color:COLOR; };
HudOutput hud_vs(HudInput input) {
  HudOutput o;
  o.position=float4(input.position/float2(dimensions.zw)*float2(2,-2)+float2(-1,1),0,1);
  o.color=input.color; return o;
}
float4 hud_ps(HudOutput input):SV_Target { return input.color; }
