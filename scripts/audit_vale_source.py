#!/usr/bin/env python3
"""Independent source/corner reconstruction for Vale's pinned graphical recipe."""
import argparse,gzip,hashlib,json,math,re,struct,tempfile,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def f32(x):return struct.unpack('f',struct.pack('f',x))[0]
def array(path,name):
 text=path.read_text();m=re.search(r'\b'+re.escape(name)+r'\s*=\s*\{\{(.*?)\}\};',text,re.S)
 assert m, name
 s=m.group(1)
 if '{' in s:return [tuple(float(x.strip().removesuffix('f'))for x in row.split(',')if x.strip())for row in re.findall(r'\{([^{}]+)\}',s)]
 return [int(x.strip().removesuffix('u'))for x in s.replace('\n','').split(',')if x.strip()]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=ROOT/'docs/characters/vale/SOURCE_AUDIT.json');a=parser.parse_args()
 recipe_path=ROOT/'third_party/vale_anatomy/vale_recipe.json';recipe=json.loads(recipe_path.read_text());header=ROOT/'engine/include/fury/generated/vale_anatomy.hpp'
 positions=[];uvs=[];faces=[];groups=set();total_triangles=0
 for line in (ROOT/'third_party/mpfb_assets/base.obj').read_text().splitlines():
  row=line.split()
  if not row:continue
  if row[0]=='v':positions.append([float(x)for x in row[1:4]])
  elif row[0]=='vt':uvs.append(tuple(float(x)for x in row[1:3]))
  elif row[0]=='g':groups=set(row[1:])
  elif row[0]=='f':
   corners=[tuple(int(v)-1 for v in x.split('/')[:2])for x in row[1:]];faces.append((set(groups),corners,total_triangles));total_triangles+=len(corners)-2
 for target in recipe['targets']:
  with gzip.open(ROOT/target['path'],'rt')as f:
   for line in f:
    row=line.split('#',1)[0].split()
    if not row:continue
    i=int(row[0]);delta=list(map(float,row[1:]));assert len(delta)==3
    for axis in range(3):positions[i][axis]+=target['weight']*delta[axis]
 body={i for group,corners,_ in faces if 'body'in group for i,_ in corners};floor=min(positions[i][1]for i in body);crown=max(positions[i][1]for i in body);scale=.989/(crown-floor)
 p=[(v[0]*scale,(v[1]-floor)*scale-.5,v[2]*scale)for v in positions]
 selected=[];sf=[];st=[]
 for face_id,(group,corners,first)in enumerate(faces):
  if 'body'not in group or any(p[i][1]<.33 for i,_ in corners):continue
  for k in range(1,len(corners)-1):selected.append((corners[0],corners[k],corners[k+1]));sf.append(face_id);st.append(first+k-1)
 ids=sorted({i for tri in selected for i,_ in tri});vid={v:i for i,v in enumerate(ids)};uv_ids=sorted({u for tri in selected for _,u in tri});uid={v:i for i,v in enumerate(uv_ids)}
 expected={'source_vertex_indices':ids,'source_uv_indices':uv_ids,'indices':[vid[v]for t in selected for v,u in t],'uv_indices':[uid[u]for t in selected for v,u in t],'triangle_source_faces':sf,'triangle_source_indices':st}
 for key,value in expected.items():assert array(header,key)==value,key
 points=array(header,'positions');tex=array(header,'uv_coordinates');assert len(points)==len(ids)
 assert all(tuple(f32(v)for v in points[k])==tuple(f32(v)for v in p[i])for k,i in enumerate(ids)),'Positions independently reproduced'
 assert all(tuple(f32(v)for v in tex[k])==tuple(f32(v)for v in uvs[i])for k,i in enumerate(uv_ids)),'Unchanged canonical UVs'
 # Compare corresponding source vertices; record the exact independently read
 # peer headers. This measures distinction, not facial likeness or art quality.
 comparisons=[]
 for name,base in [('mira',ROOT),('jon',ROOT),('tessa',ROOT),('owen',ROOT),('lia',ROOT),('hale',ROOT.parent/'hale-study'),('renn',ROOT.parent/'renn-study')]:
  other=base/f'engine/include/fury/generated/{name}_anatomy.hpp'
  if not other.exists():continue
  op=array(other,'positions');oi=array(other,'source_vertex_indices');lookup=dict(zip(oi,op));errors=[]
  for i,pos in zip(ids,points):
   if i in lookup:errors.append(sum((x-y)**2 for x,y in zip(pos,lookup[i])))
  comparisons.append({'actor':name,'header_sha256':hashlib.sha256(other.read_bytes()).hexdigest(),'matched_vertices':len(errors),'differing_vertices':sum(e>1e-18 for e in errors),'rms_normalized_displacement':math.sqrt(sum(errors)/len(errors)),'maximum_normalized_displacement':math.sqrt(max(errors))})
 # Exercise numeric-parser rejection with temporary in-memory-scale fixtures.
 import build_vale_anatomy as va
 from build_mira_anatomy import read_obj
 rejected=[]
 cases={'bad_index':'2 0 0 0\n','negative_index':'-1 0 0 0\n','duplicate_index':'0 0 0 0\n0 1 1 1\n','nan_delta':'0 nan 0 0\n','inf_delta':'0 0 inf 0\n','missing_delta':'0 0 0\n','extra_delta':'0 0 0 0 0\n','empty':'# no data\n'}
 with tempfile.TemporaryDirectory()as td:
  old=va.ASSETS;va.ASSETS=Path(td);path=va.ASSETS/'targets/macrodetails/test.target.gz';path.parent.mkdir(parents=True)
  for name,data in cases.items():
   path.write_bytes(gzip.compress(data.encode()));r={'targets':[{'path':'targets/macrodetails/test.target.gz','weight':1}]};manifest={'files':[{'path':'targets/macrodetails/test.target.gz'}],'shared_files':[]}
   try:va.apply_targets([[0,0,0]],r,manifest)
   except (ValueError,AssertionError,RuntimeError,IndexError)as e:rejected.append({'case':name,'rejected':True,'message':str(e)})
   else:raise AssertionError('Accepted '+name)
  path.write_bytes(gzip.compress(b'0 0 0 0\n'))
  for name,w in [('nan_weight',float('nan')),('inf_weight',float('inf')),('negative_weight',-.1),('overweight',1.01),('macro_sum',.4)]:
   try:va.apply_targets([[0,0,0]],{'targets':[{'path':'targets/macrodetails/test.target.gz','weight':w}]},manifest)
   except (ValueError,AssertionError,RuntimeError)as e:rejected.append({'case':name,'rejected':True,'message':str(e)})
   else:raise AssertionError('Accepted '+name)
  va.ASSETS=old
  for name,data in {'nan_obj':'v nan 0 0\n','missing_uv':'v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n','bad_uv_index':'v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/2\n','bad_vertex_index':'v 0 0 0\nvt 0 0\nf 1/1 2/1 3/1\n'}.items():
   f=Path(td)/'bad.obj';f.write_text(data)
   try:read_obj(f)
   except (ValueError,AssertionError,RuntimeError)as e:rejected.append({'case':name,'rejected':True,'message':str(e)})
   else:raise AssertionError('Accepted '+name)
 report={'result':'passed','source_commit':'d0a32e57a7f915cb2f2b95410e2117648c7bbb7e','recipe_sha256':hashlib.sha256(recipe_path.read_bytes()).hexdigest(),'header_sha256':hashlib.sha256(header.read_bytes()).hexdigest(),'independently_reconstructed_vertices':len(ids),'independently_reconstructed_triangles':len(selected),'canonical_uv_corners':len(selected)*3,'source_corner_arrays_verified':list(expected),'removed_triangles':0,'shape_correspondence':comparisons,'malformed_rejections':rejected,'negative_control_count':len(rejected),'method':'Independent raw OBJ and gzip numeric target parse; explicit float32 comparison of every generated source position, UV corner and triangle provenance ID. Shape distances include normalization differences and are not a claim of likeness or realism.'}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['result','independently_reconstructed_vertices','independently_reconstructed_triangles','negative_control_count']}))
if __name__=='__main__':main()
