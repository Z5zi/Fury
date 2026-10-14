#!/usr/bin/env python3
"""Independent exact CC0 correspondence audit for Renn's head and connected hands.

No MPFB program code is used. This reader deliberately does not call Fury's OBJ
parser or mesh importer when reconstructing the selected source triangles.
"""
from pathlib import Path
import argparse,copy,gzip,hashlib,importlib.util,json,math,re,struct,sys,tempfile
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'third_party/renn_anatomy'
def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def array(text,name,width=1):
 a=text.index(' '+name+' =');a=text.index('{{',a)+2;b=text.index('}};',a);s=text[a:b]
 if width==1:return [int(n[:-1])for n in re.findall(r'\d+u',s)]
 return [tuple(float(n.strip().removesuffix('f'))for n in p.split(','))for p in re.findall(r'\{([^{}]+)\}',s)]
def vectors_close(actual,expected):
 assert len(actual)==len(expected)
 for i,(a,b)in enumerate(zip(actual,expected)):
  assert tuple(f32(x)for x in a)==tuple(f32(x)for x in b),('float32 source mismatch',i,a,b)
def independent_source():
 manifest=json.loads((ASSETS/'MANIFEST.json').read_text());recipe=json.loads((ASSETS/'renn_recipe.json').read_text())
 assert manifest['asset_license']=='CC0-1.0' and manifest['commit']=='d0a32e57a7f915cb2f2b95410e2117648c7bbb7e'
 inputs={}
 for category,root in [('shared_files',ROOT),('files',ASSETS)]:
  for item in manifest[category]:
   p=(root/item['path']).resolve();assert p.is_relative_to(root.resolve()) and str(p)not in inputs
   d=p.read_bytes();assert len(d)==item['bytes'] and hashlib.sha256(d).hexdigest()==item['sha256']
   assert hashlib.sha1(b'blob '+str(len(d)).encode()+b'\0'+d).hexdigest()==item['git_blob_sha1']
   assert item['url']==f"https://raw.githubusercontent.com/makehumancommunity/mpfb2/{manifest['commit']}/{item['upstream_path']}"
   inputs[str(p)]=item
 positions=[];uv=[];faces=[];groups=();triangle_offset=0
 for line in (ROOT/'third_party/mpfb_assets/base.obj').read_text().splitlines():
  a=line.split()
  if not a:continue
  if a[0]=='v':positions.append([float(x)for x in a[1:4]])
  elif a[0]=='vt':uv.append(tuple(float(x)for x in a[1:3]))
  elif a[0]=='g':groups=tuple(a[1:])
  elif a[0]=='f':
   corners=[tuple(int(x)-1 for x in q.split('/')[:2])for q in a[1:]]
   faces.append((groups,corners,triangle_offset));triangle_offset+=len(corners)-2
 def ids(group):return sorted({i for g,c,_ in faces if group in g for i,_ in c})
 assert len({x['path']for x in recipe['targets']})==len(recipe['targets'])
 assert abs(sum(x['weight']for x in recipe['targets']if '/macrodetails/'in x['path'])-1)<1e-12
 for item in recipe['targets']:
  path=(ROOT/item['path']).resolve();assert str(path)in inputs;seen=set();weight=item['weight'];assert math.isfinite(weight)and 0<=weight<=1
  for line in gzip.open(path,'rt'):
   a=line.split('#')[0].split()
   if not a:continue
   assert len(a)==4;i=int(a[0]);d=[float(x)for x in a[1:]];assert i not in seen and 0<=i<len(positions)and all(math.isfinite(x)for x in d);seen.add(i)
   for k in range(3):positions[i][k]+=weight*d[k]
 body=ids('body');lo=min(positions[i][1]for i in body);hi=max(positions[i][1]for i in body);scale=.991/(hi-lo)
 normalized=[(p[0]*scale,(p[1]-lo)*scale-.5,p[2]*scale)for p in positions]
 h=(ROOT/'engine/include/fury/generated/renn_anatomy.hpp').read_text();pids=array(h,'source_vertex_indices');uids=array(h,'source_uv_indices')
 vectors_close(array(h,'positions',3),[normalized[i]for i in pids]);vectors_close(array(h,'uv_coordinates',2),[uv[i]for i in uids])
 expected=[];face_ids=[];triangle_ids=[]
 for fi,(groups,corners,offset)in enumerate(faces):
  if 'body'not in groups or any(normalized[i][1]<.33 for i,_ in corners):continue
  for k in range(1,len(corners)-1):expected.extend((corners[0],corners[k],corners[k+1]));face_ids.append(fi);triangle_ids.append(offset+k-1)
 actual=list(zip([pids[i]for i in array(h,'indices')],[uids[i]for i in array(h,'uv_indices')]))
 assert actual==expected,'Head source corners/order changed'
 assert array(h,'triangle_source_faces')==face_ids
 assert array(h,'triangle_source_indices')==triangle_ids
 # Reconstruct hand frame and rest curl from actual source helper groups.
 def mean(group):return tuple(sum(positions[i][k]for i in ids(group))/len(ids(group))for k in range(3))
 def sub(a,b):return tuple(x-y for x,y in zip(a,b))
 def dot(a,b):return sum(x*y for x,y in zip(a,b))
 def norm(a):return math.sqrt(dot(a,a))
 def unit(a):return tuple(x/norm(a)for x in a)
 def cross(a,b):return(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
 wrist=mean('joint-l-hand');down=unit(sub(mean('joint-l-finger-3-1'),wrist));wide=sub(mean('joint-l-finger-5-1'),mean('joint-l-finger-2-1'));wide=unit(tuple(x-y*dot(wide,down)for x,y in zip(wide,down)));up=tuple(-x for x in down);normal=unit(cross(wide,up))
 hand=(ROOT/'engine/include/fury/generated/renn_hands.hpp').read_text();ids_h=array(hand,'source_vertex_indices');uv_h=array(hand,'source_uv_indices')
 local=[tuple(dot(sub(positions[i],wrist),axis)*scale for axis in (wide,up,normal))for i in ids_h]
 def curl(p):
  x,y,z=p;t=max(0.,min(1.,(-y-.025)/.080));t=t*t*(3-2*t)
  return(x*(1-.14*t)+.004*max(0.,min(1.,(-x-.015)/.025)),y+.009*t,z-.019*t)
 vectors_close(array(hand,'source_frame_positions',3),local);vectors_close(array(hand,'positions',3),[curl(p)for p in local]);vectors_close(array(hand,'uv_coordinates',2),[uv[i]for i in uv_h])
 expected=[];face_ids=[]
 for fi,(groups,corners,_)in enumerate(faces):
  if 'body'not in groups or not all(positions[i][0]>3.5 and dot(sub(positions[i],wrist),down)>-.23 for i,_ in corners):continue
  for k in range(1,len(corners)-1):expected.extend((corners[0],corners[k],corners[k+1]));face_ids.append(fi)
 actual=list(zip([ids_h[i]for i in array(hand,'near_indices')],[uv_h[i]for i in array(hand,'near_uv_indices')]))
 assert actual==expected and array(hand,'source_triangle_faces')==face_ids,'Hand source triangle corners/order changed'
 assert all(i<len(ids_h)for i in array(hand,'far_indices'))
 return {'pinned_input_files':len(inputs),'head_source_positions':len(pids),'head_source_triangles':len(triangle_ids),'head_source_uvs':len(uids),'hand_source_positions':len(ids_h),'hand_near_triangles':len(face_ids),'hand_far_triangles':len(array(hand,'far_indices'))//3,'all_correspondences':'exact binary32 positions/UVs, source face ids, winding and corner order'}
def malformed_checks():
 sys.path.insert(0,str(ROOT/'scripts'));import build_renn_anatomy as m
 original=m.ASSETS;manifest=json.loads((ASSETS/'MANIFEST.json').read_text());recipe=json.loads((ASSETS/'renn_recipe.json').read_text());passed=[]
 def reject(name,f):
  try:f()
  except (ValueError,AssertionError,KeyError,TypeError,OverflowError,IndexError,OSError):passed.append(name);return
  raise AssertionError('Invalid source accepted: '+name)
 with tempfile.TemporaryDirectory(prefix='renn-source-audit-')as tmp:
  t=Path(tmp);m.ASSETS=t
  mutations=[('license',lambda d:d.update(asset_license='MIT')),('commit',lambda d:d.update(commit='0'*40)),('upstream',lambda d:d.update(upstream='https://invalid.example')),('duplicate source',lambda d:d['shared_files'].append(copy.deepcopy(d['shared_files'][0]))),('source hash',lambda d:d['shared_files'][0].update(sha256='0'*64)),('source bytes',lambda d:d['shared_files'][0].update(bytes=1)),('source git blob',lambda d:d['shared_files'][0].update(git_blob_sha1='0'*40)),('unpinned URL',lambda d:d['shared_files'][0].update(url='https://invalid.example/base.obj'))]
  for name,change in mutations:
   d=copy.deepcopy(manifest);change(d);(t/'MANIFEST.json').write_text(json.dumps(d));reject(name,m.verified_inputs)
  m.ASSETS=original
  cases=[]
  for w in [-.001,1.001,float('nan'),float('inf'),-float('inf')]:
   d=copy.deepcopy(recipe);d['targets'][0]['weight']=w;cases.append(('target weight '+str(w),d))
  d=copy.deepcopy(recipe);d['targets'].append(copy.deepcopy(d['targets'][0]));cases.append(('duplicate recipe target',d))
  d=copy.deepcopy(recipe);d['targets'][0]['path']='third_party/renn_anatomy/unpinned.target.gz';cases.append(('unmanifested recipe target',d))
  d=copy.deepcopy(recipe);d['targets'][0]['weight']=.1;cases.append(('macro weights do not sum to one',d))
  for cutoff in [.29,.36,float('nan')]:
   d=copy.deepcopy(recipe);d['selection']['minimum_face_y']=cutoff;cases.append(('invalid head selection '+str(cutoff),d))
  for feet in [-.51,float('inf')]:
   d=copy.deepcopy(recipe);d['normalization']['body_feet_y']=feet;cases.append(('invalid feet '+str(feet),d))
  d=copy.deepcopy(recipe);d['selection']['obj_group']='joint-head';cases.append(('helper geometry is not anatomy',d))
  for epsilon in [0,-1,1e-10,float('nan')]:
   d=copy.deepcopy(recipe);d['degenerate_cross_length_squared_max']=epsilon;cases.append(('invalid degeneracy epsilon '+str(epsilon),d))
  for name,d in cases:reject(name,lambda d=d:m.build(d,manifest))
 m.ASSETS=original
 return passed
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ASSETS/'INDEPENDENT_SOURCE_AUDIT.json');o=p.parse_args()
 result=independent_source();result['malformed_cases_rejected']=malformed_checks();result['audit_script_sha256']=digest(Path(__file__));result['result']='passed';o.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
