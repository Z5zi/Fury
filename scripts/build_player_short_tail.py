#!/usr/bin/env python3
"""Shorten the verified CC0 artist-authored tied tail, preserving source UVs.
Near keeps the original source topology; Far uses a reviewed offline reduction.
Both retain the source alpha mask. This isolated runtime option defaults off.
"""
from pathlib import Path
import json, math, struct, hashlib, sys
import numpy as np
R=Path(__file__).resolve().parents[1]; A=R/'third_party/player_body_hair'
CHECK='--check' in sys.argv

def write(p,s):
 if CHECK: assert p.read_text()==s,p
 else: p.write_text(s)

j=json.loads((A/'fit/fit.json').read_text());manifest=json.loads((A/'SOURCE_MANIFEST.json').read_text())
for name in ['ponytail01.obj','ponytail01.mhclo','ponytail01_diffuse.png']:
 assert hashlib.sha256((A/name).read_bytes()).hexdigest()==manifest['files'][name]
cs=[c for c in j['components'] if c['min'][1]<.37];keep={i for c in cs for i in c['indices']}
positions=np.array(j['vertices'],dtype=float)
def warp(p):
 x,y,z=p
 if y<.445:
  d=.445-y;y=.445-d*(.44+.56*math.exp(-d/.025))
 return np.array([x,y,z])
for i in keep: positions[i]=warp(positions[i])
triangles=[]
for f in j['faces']:
 if f[0][0] not in keep: continue
 for k in range(1,len(f)-1): triangles.append([(i,j['uvs'][u]) for i,u in [f[0],f[k],f[k+1]]])

def unit(n): return np.asarray(n)/np.linalg.norm(n)
def clip(poly,half):
 out=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  ia=a[6]<=.5 if half==0 else a[6]>=.5;ib=b[6]<=.5 if half==0 else b[6]>=.5
  if ia: out.append(a)
  if ia!=ib:
   t=(.5-a[6])/(b[6]-a[6]);out.append([x+(y-x)*t for x,y in zip(a,b)])
 return out

def build(positions,triangles):
 normals=np.zeros_like(positions)
 for tri in triangles:
  a,b,c=[positions[i] for i,u in tri];n=np.cross(b-a,c-a)
  for i,u in tri: normals[i]+=n
 for i in range(len(normals)):
  n=np.linalg.norm(normals[i]);normals[i]/=n if n>1e-14 else 1.
 verts=[];inds=[];seen={};hard_faces=0;originals={};pending={};triangle_components=[]
 parents=list(range(len(positions)))
 def root(i):
  while parents[i]!=i:
   parents[i]=parents[parents[i]];i=parents[i]
  return i
 for tri in triangles:
  for i,u in tri[1:]:parents[root(i)]=root(tri[0][0])
 for tri in triangles:
  poly=[[*positions[i],*normals[i],uv[0],1-uv[1],*(1. if k==q else 0. for k in range(3))] for q,(i,uv) in enumerate(tri)]
  for tile in [0,1]:
   polygon=clip(poly,tile)
   for k in range(1,len(polygon)-1):
    tr=[polygon[0],polygon[k],polygon[k+1]];n=np.cross(np.array(tr[1][:3])-tr[0][:3],np.array(tr[2][:3])-tr[0][:3])
    if np.linalg.norm(n)<1e-12: continue
    triangle_components.append(root(tri[0][0]))
    n=unit(n);hard=min(np.dot(n,unit(p[3:6])) for p in tr)<.55;hard_faces+=int(hard)
    for raw in tr:
     p=raw[:8];p[3:6]=unit(n if hard else p[3:6]);p[6]=p[6]*2-tile
     key=tuple(round(x,9) for x in p)+(tile,)
     if key not in seen: seen[key]=len(verts);verts.append((p,tile))
     vertex=seen[key];inds.append(vertex)
     weights=[(tri[k][0],w) for k,w in enumerate(raw[8:]) if w>1e-8]
     if len(weights)==1: originals.setdefault(weights[0][0],vertex)
     elif len(weights)==2: pending[vertex]=(weights[0][0],weights[1][0],weights[1][1])
     else: assert False,weights
 edges=[]
 for vertex,(a,b,t) in sorted(pending.items()):
  assert a in originals and b in originals
  edges.append((vertex,originals[a],originals[b],t))
 assert not ({e[0] for e in edges}&{p for e in edges for p in e[1:3]}), 'Edge attachment parents must be original corners'
 return verts,inds,hard_faces,edges,triangle_components

def ff(x):
 x=struct.unpack('f',struct.pack('f',x))[0];s=f'{x:.9g}';return s+('f' if '.' in s or 'e' in s else '.0f')
near=build(positions,triangles)
f=json.loads((A/'short-tail-far.json').read_text())
assert f['fit_sha256']==hashlib.sha256((A/'fit/fit.json').read_bytes()).hexdigest()
far=build(np.array(f['positions'],dtype=float),f['triangles'])
out='#pragma once\n// Isolated CC0 short tied-tail prototype. Source records remain in third_party/player_body_hair.\n#include "fury/generated/player_body_hair.hpp"\nnamespace fury::player_body_short_tail {\nusing Vertex=player_body_hair::Vertex;\n'
out+='struct EdgeAttachment { std::uint32_t vertex,first,second;float fraction; };\n'
for prefix,(verts,inds,hard,edges,components) in [('',near),('far_',far)]:
 out+=f'inline const std::array<Vertex,{len(verts)}> {prefix}vertices{{{{\n'
 for p,tile in verts: out+='  {{'+','.join(ff(x) for x in p[:3])+'},{'+','.join(ff(x) for x in p[3:6])+'},{'+','.join(ff(x) for x in p[6:])+f'}},{tile}'+'},\n'
 out+='}};\n'+f'inline constexpr std::array<std::uint32_t,{len(inds)}> {prefix}indices{{{{\n'
 for i in range(0,len(inds),24):out+='  '+','.join(map(str,inds[i:i+24]))+',\n'
 out+='}};\n'
 out+=f'inline constexpr std::array<std::uint32_t,{len(components)}> {prefix}triangle_components{{{{\n'
 for i in range(0,len(components),24):out+='  '+','.join(map(str,components[i:i+24]))+',\n'
 out+='}};\n'
 out+=f'inline constexpr std::array<EdgeAttachment,{len(edges)}> {prefix}edge_attachments{{{{\n'
 for v,a,b,t in edges:out+=f'  {{{v},{a},{b},{ff(t)}}},\n'
 out+='}};\n'
out+='}\n';write(R/'engine/include/fury/generated/player_body_short_tail.hpp',out)
stats={'source_components':len(cs),'source_vertices':len(keep),'source_triangles':len(triangles),'near':{'vertices':len(near[0]),'triangles':len(near[1])//3,'hard_normal_faces':near[2],'edge_attachments':len(near[3])},'far':{'vertices':len(far[0]),'triangles':len(far[1])//3,'hard_normal_faces':far[2],'edge_attachments':len(far[3])},'bounds_min':positions[sorted(keep)].min(axis=0).tolist(),'bounds_max':positions[sorted(keep)].max(axis=0).tolist(),'far_source_sha256':hashlib.sha256((A/'short-tail-far.json').read_bytes()).hexdigest()}
write(A/'short-tail-prototype-stats.json',json.dumps(stats,indent=2)+'\n');print(stats)
