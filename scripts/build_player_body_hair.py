#!/usr/bin/env python3
"""Bake verified CC0 core-groom scalp/UV data and neutral source-strand fields.
No addon code is used. Source fit and Blender4.3.2 Far reduction are retained in
scripts/fit_player_body_hair.py and scripts/reduce_player_body_hair_lod.py.
"""
from pathlib import Path
import json,math,hashlib,io,sys,struct
import numpy as np
from PIL import Image,ImageFilter
R=Path(__file__).resolve().parents[1];A=R/'third_party/player_body_hair';H=R/'engine/include/fury/generated/player_body_hair.hpp'

CHECK='--check' in sys.argv
manifest=json.loads((A/'SOURCE_MANIFEST.json').read_text())
for name,digest in {**manifest['files'],**manifest.get('derived_inputs',{})}.items():
 assert hashlib.sha256((A/name).read_bytes()).hexdigest()==digest, f'Source hash mismatch: {name}'
def write(path,data):
 if isinstance(data,str):data=data.encode()
 if CHECK and path.suffix=='.png' and path.exists() and path.read_bytes()!=data:
  # zlib/zlib-ng builds encode identical pixels differently; compare the decoded image exactly.
  old,new=Image.open(path),Image.open(io.BytesIO(data))
  assert old.mode==new.mode and old.size==new.size and old.tobytes()==new.tobytes(), f'Generated content differs: {path}'
 elif CHECK:assert path.exists() and path.read_bytes()==data, f'Generated content differs: {path}'
 else:path.write_bytes(data)
def png(image,path):
 buffer=io.BytesIO();image.save(buffer,format='PNG');write(path,buffer.getvalue())
def unit(n):
 l=math.sqrt(sum(x*x for x in n));assert l>1e-15
 return [x/l for x in n]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def minus(a,b):return [x-y for x,y in zip(a,b)]
def ff(x):
 x=struct.unpack('f',struct.pack('f',x))[0]
 s=f'{x:.9g}';return s+('f' if '.' in s or 'e' in s else '.0f')
def emit_geometry(lod):
 d=json.loads((A/f'scalp-{lod}.json').read_text());positions=d['positions']
 # Preserve authored1.75m crown allowance, without moving the source face.
 for p in positions:
  t=max(0,min(1,(p[1]-.46)/.038));p[1]+=.00235*t*t*(3-2*t)
 normals=[[0.,0.,0.] for p in positions]
 for tri in d['triangles']:
  a,b,c=[positions[v] for v,uv in tri];n=cross(minus(b,a),minus(c,a))
  for v,uv in tri:
   normals[v]=[x+y for x,y in zip(normals[v],n)]
 normals=[unit(n) for n in normals]
 verts=[];inds=[];seen={};hard_faces=0
 def point(v,uv):
  n=normals[v];offset=.00035 if lod=='near' else .0011
  return [*[positions[v][i]+offset*n[i] for i in range(3)],*n,uv[0],1-uv[1]]
 def clip(poly,half):
  out=[]
  for a,b in zip(poly,poly[1:]+poly[:1]):
   ia=a[6]<=.5 if half==0 else a[6]>=.5
   ib=b[6]<=.5 if half==0 else b[6]>=.5
   if ia:out.append(a)
   if ia!=ib:
    t=(.5-a[6])/(b[6]-a[6]);out.append([x+(y-x)*t for x,y in zip(a,b)])
  return out
 for tri in d['triangles']:
  poly=[point(v,uv) for v,uv in tri]
  for tile in [0,1]:
   polygon=clip(poly,tile)
   if len(polygon)<3:continue
   for k in range(1,len(polygon)-1):
    tr=[polygon[0],polygon[k],polygon[k+1]]
    if sum(x*x for x in cross(minus(tr[1][:3],tr[0][:3]),minus(tr[2][:3],tr[0][:3])))<1e-22:continue
    face_n=unit(cross(minus(tr[1][:3],tr[0][:3]),minus(tr[2][:3],tr[0][:3])))
    mean_n=[sum(v[i] for v in tr) for i in range(3,6)]
    hard=sum(x*y for x,y in zip(face_n,mean_n))<=1e-6
    if hard:hard_faces+=1
    for raw in tr:
     p=raw[:]
     if hard:p[3:6]=face_n
     p[3:6]=unit(p[3:6]);p[6]=p[6]*2-tile
     key=tuple(round(x,9) for x in p)+(tile,)
     if key not in seen:seen[key]=len(verts);verts.append((p[:],tile))
     inds.append(seen[key])
 out=f'inline const std::array<Vertex,{len(verts)}> {lod}_vertices{{{{\n'
 for p,tile in verts:out+='  {{'+','.join(ff(x) for x in p[:3])+'},{'+','.join(ff(x) for x in p[3:6])+'},{'+','.join(ff(x) for x in p[6:])+f'}},{tile}'+'},\n'
 out+='}};\n'+f'inline constexpr std::array<std::uint32_t,{len(inds)}> {lod}_indices{{{{\n'
 for i in range(0,len(inds),24):out+='  '+','.join(map(str,inds[i:i+24]))+',\n'
 out+='}};\n'
 return out,{'vertices':len(verts),'triangles':len(inds)//3,'hard_normal_faces':hard_faces}

im=Image.open(A/'ponytail01_diffuse.png').convert('RGBA');src=np.asarray(im,dtype=np.float64)/255
rgb=src[:,:,:3];linear=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)
y=linear[:,:,0]*.2126+linear[:,:,1]*.7152+linear[:,:,2]*.0722
a=src[:,:,3]
# Alpha-aware broad normalization keeps the existing seeded chestnut tint and
# suppresses the source's broad complexion/highlight, retaining strand contrast.
blur_num=np.asarray(Image.fromarray(y*a,mode='F').filter(ImageFilter.GaussianBlur(24))) if False else None
# GaussianBlur does not support float mode on all Pillow versions. Filter an
# explicit16-bit-independent8-bit auxiliary field; the delivered reflectance is
# derived in floating point and alpha-premultiplied during downsampling.
num=np.asarray(Image.fromarray(np.uint8(np.rint(np.clip(y*a,0,1)*255))).filter(ImageFilter.GaussianBlur(24)),dtype=np.float64)/255
w=np.asarray(Image.fromarray(np.uint8(np.rint(a*255))).filter(ImageFilter.GaussianBlur(24)),dtype=np.float64)/255
broad=num/np.maximum(w,.015)
median=float(np.median(y[a>.75]))
# Retain the licensed authored strand/cluster contrast while removing hue.
# This includes authored low-frequency variation; it is not a scattering model.
factor=np.clip(.82*(np.maximum(y,.003)/max(median,.003))**.32,.24,1.)
srgb=np.where(factor<=.0031308,factor*12.92,1.055*factor**(1/2.4)-.055)
neutral=np.dstack([srgb,srgb,srgb,a]);neutral=Image.fromarray(np.uint8(np.rint(np.clip(neutral,0,1)*255)),'RGBA')
# Dedicated Player-only color detail candidates, prefiltered in linear light
# with alpha weighting; these do not expand the shared character atlas.
for size in [512,1024]:
 af=np.asarray(Image.fromarray(a.astype(np.float32),'F').resize((size,size),Image.Resampling.LANCZOS))
 af=np.clip(af,0,1)
 cf=np.asarray(Image.fromarray((factor*a).astype(np.float32),'F').resize((size,size),Image.Resampling.LANCZOS))/np.maximum(af,.0001)
 cf=np.clip(cf,0,1);cf=np.where(af>.005,cf,.82);sc=np.where(cf<=.0031308,cf*12.92,1.055*cf**(1/2.4)-.055)
 detail=Image.fromarray(np.uint8(np.rint(np.clip(np.dstack([sc,sc,sc,af]),0,1)*255)),'RGBA')
 png(detail,A/f'color-detail-{size}.png')
fields=[]
for box in [(0,0,1024,2048),(1024,0,2048,2048),(880,1080,1040,1930)]:
 f=neutral.crop(box).resize((192,192),Image.Resampling.LANCZOS)
 if len(fields)==2:
  px=np.array(f);valid=px[:,:,3]>32;fallback=int(np.median(px[:,:,0][valid]));px[:,:,:3][~valid]=fallback;px[:,:,3]=255;f=Image.fromarray(px,'RGBA')
 fields.append(f)
text='#pragma once\n#include "fury/math.hpp"\n#include <array>\n#include <cstdint>\nnamespace fury::player_body_hair {\nstruct Vertex { Vec3 p,n;Vec2 uv;std::uint8_t tile; };\n'
stats={}
for lod in ['near','far']:
 s,stats[lod]=emit_geometry(lod);text+=s
for name,f in zip(['left','right','fold'],fields):
 out=io.BytesIO();f.save(out,format='PNG');data=out.getvalue();write(A/f'{name}-neutral.png',data)
 # Embed the verified committed PNG bytes (pixels checked above); encoders differ by zlib build.
 data=(A/f'{name}-neutral.png').read_bytes()
 text+=f'inline constexpr unsigned char {name}_png[]={{\n'
 for i in range(0,len(data),24):text+=' '+','.join(map(str,data[i:i+24]))+',\n'
 text+='};\n'
text+='}\n';write(H,text);write(A/'GENERATED_STATS.json',json.dumps(stats,indent=2)+'\n');print(stats,H.stat().st_size)

# The high-resolution override payload is a separate header so ordinary source
# geometry consumers do not parse several megabytes of image initializer data.
parts=['// Original color-detail adapter data derived from CC0 MakeHuman ponytail01.\n#pragma once\n#include <cstdint>\nnamespace fury::player_body_hair_detail {\n']
for size in [512,1024]:
 data=(A/f'color-detail-{size}.png').read_bytes();parts.append(f'inline constexpr unsigned char png_{size}[]={{\n')
 for i in range(0,len(data),32):parts.append(' '+','.join(map(str,data[i:i+32]))+',\n')
 parts.append('};\n')
parts.append('}\n');write(R/'engine/include/fury/generated/player_body_hair_detail.hpp',''.join(parts))
