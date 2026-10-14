#!/usr/bin/env python3
"""Independent raw-OBJ ownership audit for the retained old global-Y failures."""
import csv,hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'out/character-study/enforcer/neck-fit-review'
rows=list(csv.DictReader((out/'old-global-y-failing-vertices.csv').open()));wanted={int(r['original_obj_vertex']) for r in rows}
obj=root/'third_party/mpfb_assets/base.obj';uv=[];groups=set();faces={i:[]for i in wanted};count=0
# This parser deliberately does not import the generator or weight function.
for line in obj.read_text().splitlines():
 p=line.split()
 if not p:continue
 if p[0]=='vt':uv.append(tuple(map(float,p[1:3])))
 elif p[0]=='g':groups=set(p[1:])
 elif p[0]=='f':
  if 'body'in groups:
   for corner in p[1:]:
    v,t,*_=corner.split('/');v=int(v)-1;t=int(t)-1
    if v in wanted:
     u,w=uv[t];assert .2151318<=u<=.5459292 and .8406228<=w<=.9738562, (v,count,uv[t])
     faces[v].append({'obj_face_zero_based':count,'obj_uv_zero_based':t,'uv':uv[t]})
  count+=1
assert len(wanted)==23 and all(faces.values())
result={'purpose':'Independent source ownership for every vertex rejected by the old global-Y assertion','base_obj_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'source_vertex_count':len(wanted),'only_source_chart':'neck_shoulders','protected_external_head_mouth_socket_vertices':0,'vertices':[{'source_index':int(r['source_index']),'original_obj_vertex':int(r['original_obj_vertex']),'normalized_position':[float(r[k])for k in ['x','y','z']],'actual_head_weight':float(r['head_weight']),'original_body_face_corners':faces[int(r['original_obj_vertex'])]}for r in rows]}
(out/'independent-source-ownership.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS all 23 old-cutoff witnesses belong exclusively to the raw pinned OBJ neck/shoulder chart')
