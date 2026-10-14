from pathlib import Path
import sys,json,math
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from build_player_body_anatomy import verified_inputs,apply_targets
from build_mira_anatomy import read_obj,group_ids,bounds
base,uv,faces=read_obj(ROOT/'third_party/mpfb_assets/base.obj')
recipe=json.loads((ROOT/'third_party/player_body_anatomy/player_body_recipe.json').read_text());morphed,_=apply_targets(base,recipe,verified_inputs())
lo,hi=bounds([morphed[i] for i in group_ids(faces,'body')]);scale=.993/(hi[1]-lo[1])
source=ROOT/'third_party/player_body_hair'
hp,hu,hf=read_obj(source/'ponytail01.obj')
scales=[1,1,1];mapping=[];started=False
for line in (source/'ponytail01.mhclo').read_text().splitlines():
 f=line.split('#')[0].split()
 if not f:continue
 if f[0] in ['x_scale','y_scale','z_scale']:
  axis='xyz'.index(f[0][0]);scales[axis]=abs(morphed[int(f[1])][axis]-morphed[int(f[2])][axis])/float(f[3])
 if f[0]=='verts':started=True;continue
 if started:
  if not f[0].lstrip('-').isdigit():continue
  if len(f)==1:mapping.append(tuple(morphed[int(f[0])]))
  else:
   assert len(f)==9,f
   ids=list(map(int,f[:3]));weights=list(map(float,f[3:6]));offset=list(map(float,f[6:]))
   assert abs(sum(weights)-1)<.00003
   mapping.append(tuple(sum(morphed[ids[k]][a]*weights[k] for k in range(3))+offset[a]*scales[a] for a in range(3)))
assert len(mapping)==len(hp),(len(mapping),len(hp))
positions=[(p[0]*scale,(p[1]-lo[1])*scale-.5,p[2]*scale) for p in mapping]
parents=list(range(len(hp)))
def find(a):
 while parents[a]!=a:parents[a]=parents[parents[a]];a=parents[a]
 return a
for _,poly in hf:
 for idx,_ in poly[1:]:parents[find(idx)]=find(poly[0][0])
comps={}
for i in range(len(hp)):comps.setdefault(find(i),[]).append(i)
report=[]
for ids in comps.values():
 pl,ph=bounds([positions[i] for i in ids]);report.append({'vertices':len(ids),'triangles':sum(len(f)-2 for _,f in hf if f[0][0] in ids),'min':pl,'max':ph,'indices':ids})
report.sort(key=lambda c:-c['vertices'])
out=ROOT/'third_party/player_body_hair/fit';out.mkdir(exist_ok=True)
(out/'fit.json').write_text(json.dumps({'source_normalization_scale':scale,'mhclo_axis_scales':scales,'vertices':positions,'uvs':hu,'faces':[[list(c) for c in f] for _,f in hf],'components':report},indent=2))
with (out/'fitted-source.obj').open('w') as f:
 for p in positions:f.write('v '+' '.join(map(str,p))+'\n')
 for uv in hu:f.write('vt '+' '.join(map(str,uv))+'\n')
 for _,poly in hf:f.write('f '+' '.join(f'{i+1}/{j+1}' for i,j in poly)+'\n')
print('scale',scale,'axis',scales,'bounds',bounds(positions),'components',len(comps))
for n,c in enumerate(report):print(n,{k:v for k,v in c.items() if k!='indices'})
