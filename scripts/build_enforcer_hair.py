#!/usr/bin/env python3
"""Original fitted Enforcer scalp shell from its own pinned anatomical source.

Retains source scalp triangulation, clips one explicit hairline, and offsets
along source normals. Far uses Fury's original endpoint-quadric reducer with
locked hairline and crown. No addon program logic or proprietary asset.
"""
import argparse, hashlib, json, math
from pathlib import Path
from build_enforcer_anatomy import verified_inputs, apply_targets, ASSETS, SHARED, REPO
from build_enforcer_hands import reduce, unit, cross, sub
from build_mira_anatomy import read_obj, group_ids, bounds, mean, float32, array, require

def build():
 manifest=verified_inputs();recipe=json.loads((ASSETS/'enforcer_recipe.json').read_text())
 base,uv,faces=read_obj(SHARED/'base.obj');morphed,_=apply_targets(base,recipe,manifest)
 body=group_ids(faces,'body');lo,hi=bounds([morphed[i]for i in body]);scale=.993/(hi[1]-lo[1])
 p=[(x*scale,(y-lo[1])*scale-.5,z*scale)for x,y,z in morphed]
 center=mean([p[i]for i in group_ids(faces,'joint-head')])
 def edge(q):
  a=math.atan2(q[2]-center[2],q[0])
  return .429+.022*max(0.,math.sin(a))-.021*max(0.,-math.sin(a))+.017*abs(math.cos(a))**8+.00045*math.sin(7*a+.4)
 normals=[[0.,0.,0.]for _ in p];triangles=[]
 for fid,(groups,corners) in enumerate(faces):
  if 'body' not in groups:continue
  for k in range(1,len(corners)-1):
   ids=tuple(corners[j][0]for j in (0,k,k+1));n=cross(sub(p[ids[1]],p[ids[0]]),sub(p[ids[2]],p[ids[0]]))
   for i in ids:normals[i]=[x+y for x,y in zip(normals[i],n)]
   u,v=uv[corners[0][1]]
   if u>.64 and .14869<=v<=.81764:triangles.append((fid,ids))
 normals=[unit(n)if any(n)else(0,1,0)for n in normals]
 points=[];source=[];lookup={};near=[];triangle_sources=[];locked=set()
 def node(a,b=None):
  if b is None:key=(a,a);q=p[a];n=normals[a];t=0.
  else:
   a,b=sorted((a,b));key=(a,b);low,high=0.,1.;fa=p[a][1]-edge(p[a])
   for _ in range(48):
    t=(low+high)*.5;q=tuple(x*(1-t)+y*t for x,y in zip(p[a],p[b]));fq=q[1]-edge(q)
    if (fq>=0)==(fa>=0):low=t
    else:high=t
   t=(low+high)*.5;q=tuple(x*(1-t)+y*t for x,y in zip(p[a],p[b]));n=unit(tuple(x*(1-t)+y*t for x,y in zip(normals[a],normals[b])))
  if key in lookup:return lookup[key]
  f=max(0.,min(1.,(q[1]-.476)/.017));f=f*f*(3-2*f);offset=.0024+.0046*f
  out=[x+y*offset for x,y in zip(q,n)]
  if q[1]>.492999:out[1]=.5
  i=len(points);lookup[key]=i;points.append(tuple(float32(v)for v in out));source.append({'a':a,'b':a if b is None else b,'t':t})
  if b is not None or q[1]>.4915:locked.add(i)
  return i
 for fid,ids in triangles:
  polygon=[]
  for k,a in enumerate(ids):
   b=ids[(k+1)%3];ina=p[a][1]>=edge(p[a]);inb=p[b][1]>=edge(p[b])
   if ina:polygon.append(node(a))
   if ina!=inb:polygon.append(node(a,b))
  for k in range(1,len(polygon)-1):near.append((polygon[0],polygon[k],polygon[k+1]));triangle_sources.append(fid)
 far,collapsed=reduce(points,near,400,locked)
 header=['// Generated original Enforcer scalp shell; source-connected clipping and original LOD reduction.','#pragma once','#include "fury/math.hpp"','#include <array>','#include <cstdint>','namespace fury::enforcer_hair {',array('positions','Vec3',points,True),array('near_indices','std::uint32_t',[v for t in near for v in t]),array('far_indices','std::uint32_t',[v for t in far for v in t]),'}']
 content='\n'.join(header)+'\n'
 report={'source_commit':manifest['commit'],'recipe_sha256':hashlib.sha256((ASSETS/'enforcer_recipe.json').read_bytes()).hexdigest(),'vertices':len(points),'near_triangles':len(near),'far_triangles':len(far),'locked_hairline_crown_vertices':sorted(locked),'far_collapses':collapsed,'bounds':bounds(points),'source_barycentric_correspondence':source,'near_source_faces':triangle_sources,'generated_header_sha256':hashlib.sha256(content.encode()).hexdigest()}
 return content,json.dumps(report,indent=2)+'\n'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args();header,report=build()
 for path,text in [(REPO/'engine/include/fury/generated/enforcer_hair.hpp',header),(ASSETS/'GENERATED_HAIR_STATS.json',report)]:
  if args.check:require(path.read_text()==text,'Enforcer scalp output differs: '+str(path))
  else:path.write_text(text)
 print('PASS Enforcer source-fitted scalp reconstruction'+(' --check'if args.check else''))
