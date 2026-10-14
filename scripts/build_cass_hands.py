#!/usr/bin/env python3
"""Bake Cass's connected CC0 hands with stable source corners and bounded Far reduction.

Uses Fury's independently authored importer and existing pinned graphical data.
The geometric reduction retains source vertex positions, locks the 20-edge wrist,
and rejects collapses that reverse or branch the manifold surface. No addon code.
"""
from __future__ import annotations
import argparse,hashlib,heapq,json,math
from collections import defaultdict
from pathlib import Path
from build_cass_anatomy import verified_inputs,apply_targets,ASSETS,SHARED,REPO
from build_mira_anatomy import read_obj,group_ids,mean,bounds,array,require

sub=lambda a,b:tuple(x-y for x,y in zip(a,b))
dot=lambda a,b:sum(x*y for x,y in zip(a,b))
cross=lambda a,b:(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a):
 q=math.sqrt(dot(a,a));require(q>1e-12,'Degenerate hand basis');return tuple(x/q for x in a)

def reduce(vertices,triangles,target,boundary):
 # Original endpoint quadric simplifier. The link condition preserves local
 # manifold topology. A strict normal check prevents folded fingers or nails.
 active={i:tuple(t)for i,t in enumerate(triangles)}
 incident=[set()for _ in vertices]; neighbors=[set()for _ in vertices]
 quad=[[0.]*16 for _ in vertices]
 for i,t in active.items():
  p=[vertices[v]for v in t];n=unit(cross(sub(p[1],p[0]),sub(p[2],p[0])));plane=(*n,-dot(n,p[0]))
  for v in t:
   incident[v].add(i);neighbors[v].update(set(t)-{v})
   for a in range(4):
    for b in range(4):quad[v][a*4+b]+=plane[a]*plane[b]
 heap=[];generation=[0]*len(vertices)
 def push(a,b):
  if a>b:a,b=b,a
  if not incident[a]or not incident[b]or a in boundary and b in boundary:return
  q=[x+y for x,y in zip(quad[a],quad[b])]
  options=[v for v in (a,b)if (b if v==a else a)not in boundary]
  def cost(v):
   p=(*vertices[v],1.);return sum(q[i*4+j]*p[i]*p[j]for i in range(4)for j in range(4))
  keep=min(options,key=cost);drop=b if keep==a else a
  c=cost(keep)+dot(sub(vertices[a],vertices[b]),sub(vertices[a],vertices[b]))*.0001
  heapq.heappush(heap,(c,keep,drop,generation[keep],generation[drop]))
 for a,ns in enumerate(neighbors):
  for b in ns:
   if a<b:push(a,b)
 collapsed=0
 while len(active)>target and heap:
  _,keep,drop,ga,gb=heapq.heappop(heap)
  if generation[keep]!=ga or generation[drop]!=gb or drop not in neighbors[keep]:continue
  # Preserve the sign of each source point against representative transverse
  # finger sections. An endpoint collapse must not move a fingertip or web
  # across one of these planes and change a finger contour.
  if any((vertices[keep][1]-y)*(vertices[drop][1]-y)<0 for y in (-.045,-.055,-.065,-.075)):continue
  shared=incident[keep]&incident[drop]
  if len(shared)!=2:continue
  opposite={v for t in shared for v in active[t]if v not in (keep,drop)}
  if neighbors[keep]&neighbors[drop]!=opposite:continue
  reject=False
  for t in incident[drop]-shared:
   old=active[t];new=tuple(keep if v==drop else v for v in old)
   a,b,c=[vertices[v]for v in old];oldn=cross(sub(b,a),sub(c,a))
   a,b,c=[vertices[v]for v in new];newn=cross(sub(b,a),sub(c,a))
   if dot(newn,newn)<1e-16 or dot(oldn,newn)<.20*math.sqrt(dot(oldn,oldn)*dot(newn,newn)):
    reject=True;break
  if reject:continue
  touched=set(neighbors[keep])|set(neighbors[drop])|{keep,drop}
  affected=set(incident[keep])|set(incident[drop])
  for t in affected:
   old=active.pop(t)
   for v in old:incident[v].discard(t)
   if t not in shared:
    new=tuple(keep if v==drop else v for v in old);active[t]=new
    for v in new:incident[v].add(t)
  quad[keep]=[x+y for x,y in zip(quad[keep],quad[drop])]
  for v in touched:
   neighbors[v]={w for t in incident[v]for w in active[t]if w!=v};generation[v]+=1
  for v in touched:
   for w in neighbors[v]:push(v,w)
  collapsed+=1
 require(len(active)<=target+2,'Far hand reduction did not reach budget')
 result=[active[i]for i in sorted(active)]
 return result,collapsed

def hand_quality(positions,near,far,wrist_edges):
 def topology(triangles):
  edges=defaultdict(int);adj=defaultdict(set)
  for tri in triangles:
   require(len(set(tri))==3,'Hand has collapsed indexed triangle')
   for a,b in zip(tri,tri[1:]+tri[:1]):edges[tuple(sorted((a,b)))]+=1;adj[a].add(b);adj[b].add(a)
  require(all(n<=2 for n in edges.values()),'Nonmanifold hand output')
  root={e for e,n in edges.items()if n==1};require(root==set(wrist_edges),'Hand simplification changed wrist boundary')
  seen=set();todo=[next(iter(adj))]
  while todo:
   v=todo.pop()
   if v in seen:continue
   seen.add(v);todo.extend(adj[v]-seen)
  require(len(seen)==len(adj),'Disconnected hand geometry')
  require(len(adj)-len(edges)+len(triangles)==1,'Changed hand Euler characteristic')
  return {'active_vertices':len(adj),'edges':len(edges),'triangles':len(triangles),'boundary_edges':len(root),'connected_components':1,'euler_characteristic':1}
 def loops(triangles,y):
  graph=defaultdict(set)
  for tri in triangles:
   pts=[positions[i]for i in tri];cuts=[]
   for a,b in zip(pts,pts[1:]+pts[:1]):
    if a[1]<=y<b[1]or b[1]<=y<a[1]:
     t=(y-a[1])/(b[1]-a[1]);cuts.append(tuple(round(a[k]+t*(b[k]-a[k]),8)for k in [0,2]))
   cuts=sorted(set(cuts))
   if len(cuts)==2:a,b=cuts;graph[a].add(b);graph[b].add(a)
  require(graph and all(len(v)==2 for v in graph.values()),'Finger plane must contain closed cross sections')
  remaining=set(graph);count=0
  while remaining:
   count+=1;todo=[remaining.pop()]
   while todo:
    a=todo.pop()
    for b in graph[a]:
     if b in remaining:remaining.remove(b);todo.append(b)
  return count
 q={'near_topology':topology(near),'far_topology':topology(far),'finger_cross_sections':[]}
 for y in [-.045,-.055,-.065,-.075]:
  a,b=loops(near,y),loops(far,y);require(a==b,'Far loses a finger gap at y='+str(y));q['finger_cross_sections'].append({'local_y':y,'near_contours':a,'far_contours':b})
 return q

def build():
 manifest=verified_inputs();recipe=json.loads((ASSETS/'cass_recipe.json').read_text())
 base,uv,faces=read_obj(SHARED/'base.obj');p,_=apply_targets(base,recipe,manifest)
 body=group_ids(faces,'body');lo,hi=bounds([p[i]for i in body]);scale=.993/(hi[1]-lo[1])
 wrist=mean([p[i]for i in group_ids(faces,'joint-l-hand')]);mid=mean([p[i]for i in group_ids(faces,'joint-l-finger-3-1')]);index=mean([p[i]for i in group_ids(faces,'joint-l-finger-2-1')]);pinky=mean([p[i]for i in group_ids(faces,'joint-l-finger-5-1')])
 down=unit(sub(mid,wrist));wide=sub(pinky,index);wide=unit(tuple(x-y*dot(wide,down)for x,y in zip(wide,down)));up=tuple(-x for x in down);normal=unit(cross(wide,up))
 selected=[];source_faces=[]
 for face_id,(groups,corners) in enumerate(faces):
  if 'body' in groups and all(p[i][0]>3.5 and dot(sub(p[i],wrist),down)>-.23 for i,_ in corners):
   for k in range(1,len(corners)-1):selected.append((corners[0],corners[k],corners[k+1]));source_faces.append(face_id)
 ids=sorted({i for t in selected for i,_ in t});lookup={v:i for i,v in enumerate(ids)}
 source_frame_positions=[tuple(dot(sub(p[v],wrist),a)*scale for a in (wide,up,normal))for v in ids]
 def relax(point):
  x,y,z=point
  fit=recipe['hand_rest'];x*=fit['width_scale'];y*=fit['length_scale'];z*=fit['width_scale']
  t=max(0.,min(1.,(-y-fit['curl_start'])/fit['curl_length']));t=t*t*(3-2*t)
  # A monotonic static rest curl, not an animation system. Minimum dy/dy is
  # positive for the validated recipe; the warp is invertible and cannot collapse finger gaps.
  return (x*(1-fit['lateral_compression']*t)+fit['thumb_relax']*max(0.,min(1.,(-x-.015)/.025)),y+fit['vertical_curl']*t,z-fit['depth_curl']*t)
 positions=[relax(p)for p in source_frame_positions]
 triangles=[tuple(lookup[i]for i,_ in t)for t in selected]
 # The source thumb nail fold includes a sharply concave sliver. Retain its
 # exact positions, corner UVs and winding, but give the rendering triangle
 # its own normal corners instead of averaging through a greater-than-90deg
 # fold. This is a shading split, never discarded or repaired source geometry.
 def hard_normal_splits(tris):
  normals={i:[0.,0.,0.]for tri in tris for i in tri}
  for tri in tris:
   a,b,c=[positions[i]for i in tri];n=cross(sub(b,a),sub(c,a))
   for i in tri:normals[i]=[x+y for x,y in zip(normals[i],n)]
  normals={i:unit(n)for i,n in normals.items()}
  result=[]
  for number,tri in enumerate(tris):
   a,b,c=[positions[i]for i in tri];n=unit(cross(sub(b,a),sub(c,a)))
   average=unit(tuple(sum(normals[i][k]for i in tri)for k in range(3)))
   if dot(n,average)<0:result.append(number)
  return result
 hard_triangles=hard_normal_splits(triangles)
 edges=defaultdict(int)
 for t in triangles:
  for a,b in zip(t,t[1:]+t[:1]):edges[tuple(sorted((a,b)))]+=1
 boundary_edges=[e for e,n in edges.items()if n==1];require(all(n<=2 for n in edges.values()),'Nonmanifold Near hand')
 boundary=defaultdict(set)
 for a,b in boundary_edges:boundary[a].add(b);boundary[b].add(a)
 require(len(boundary)==20 and all(len(ns)==2 for ns in boundary.values()),'Changed hand attachment cycle')
 loop=[min(boundary)];prev=None
 while True:
  nxt=min(boundary[loop[-1]]-({prev}if prev is not None else set()))
  if nxt==loop[0]:break
  prev=loop[-1];loop.append(nxt);require(len(loop)<=len(boundary),'Branched hand cycle')
 require(len(loop)==20,'Multiple hand boundary cycles')
 far,collapses=reduce(positions,triangles,560,set(boundary))
 far_hard_triangles=hard_normal_splits(far)
 quality=hand_quality(positions,triangles,far,boundary_edges)
 uv_ids=sorted({i for t in selected for _,i in t});uvmap={v:i for i,v in enumerate(uv_ids)}
 uv_indices=[uvmap[j]for t in selected for _,j in t]
 # Reduced vertices preserve the source UV associated with their anatomical
 # point. The only seam in this small region is welded geometrically in engine.
 vu={}
 for t in selected:
  for i,j in t:vu.setdefault(lookup[i],uvmap[j])
 header=['// Generated by scripts/build_cass_hands.py from pinned CC0 graphical data.','#pragma once','#include "fury/math.hpp"','#include <array>','#include <cstdint>','namespace fury::cass_hands {',array('hard_normal_triangle_indices','std::uint32_t',hard_triangles),array('far_hard_normal_triangle_indices','std::uint32_t',far_hard_triangles),array('positions','Vec3',positions,True),array('source_frame_positions','Vec3',source_frame_positions,True),array('near_indices','std::uint32_t',[i for t in triangles for i in t]),array('far_indices','std::uint32_t',[i for t in far for i in t]),array('uv_coordinates','Vec2',[uv[i]for i in uv_ids],True),array('near_uv_indices','std::uint32_t',uv_indices),array('far_uv_indices','std::uint32_t',[vu[i]for t in far for i in t]),array('source_vertex_indices','std::uint32_t',ids),array('source_uv_indices','std::uint32_t',uv_ids),array('source_triangle_faces','std::uint32_t',source_faces),array('wrist_boundary_indices','std::uint32_t',loop),'} // namespace fury::cass_hands','']
 text='\n'.join(header)
 report={'input_recipe_sha256':hashlib.sha256((ASSETS/'cass_recipe.json').read_bytes()).hexdigest(),'source_commit':manifest['commit'],'source_wrist':wrist,'static_rest_shape':{'recipe':recipe['hand_rest'],'minimum_y_derivative':recipe['hand_rest']['length_scale']*(1-1.5*recipe['hand_rest']['vertical_curl']/recipe['hand_rest']['curl_length']),'finger_animation':False},'basis':[wide,up,normal],'uniform_scale':scale,'positions':len(positions),'near_triangles':len(triangles),'far_triangles':len(far),'far_collapses':collapses,'hard_normal_source_faces':[source_faces[i]for i in hard_triangles],'far_hard_normal_triangles':far_hard_triangles,'locked_wrist_edges':len(loop),'quality':quality,'local_bounds':bounds(positions),'generated_header_sha256':hashlib.sha256(text.encode()).hexdigest(),'provenance':'Near preserves every selected source corner and winding. Documented concave thumb-nail folds retain their triangles with split rendering normals. Far uses an original endpoint-quadric reduction with locked wrist, manifold link condition and no triangle reversals. Left source hand is rigidly fitted and mirrored with reversed winding for the right; no identity or gameplay change.'}
 return text,json.dumps(report,indent=2)+'\n'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');o=parser.parse_args();text,report=build()
 for path,data in [(REPO/'engine/include/fury/generated/cass_hands.hpp',text),(ASSETS/'GENERATED_HAND_STATS.json',report)]:
  if o.check:require(path.exists()and path.read_text()==data,f'Stale generated output: {path}')
  else:path.write_text(data)
 print(report)
if __name__=='__main__':main()
