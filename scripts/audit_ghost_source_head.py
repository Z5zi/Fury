#!/usr/bin/env python3
"""Write GhostLoop's complete canonical mouth/exterior-head witness, without a jaw height cut."""
from pathlib import Path
import hashlib,json,re,struct
r=Path(__file__).resolve().parents[1];h=r/'engine/include/fury/generated/ghost_anatomy.hpp';text=h.read_text()
def array(name,width=1):
 m=re.search(r'\b'+name+r'\s*=\s*\{\{(.*?)\}\};',text,re.S);assert m
 a=[float(x.rstrip('fu')) for x in re.findall(r'[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?[fu]?',m[1])]
 if width==1:return [int(v) for v in a]
 return [a[i:i+width] for i in range(0,len(a),width)]
pos=array('positions',3);uv=array('uv_coordinates',2);idx=array('indices');ui=array('uv_indices');source=array('source_vertex_indices');groups={x:set() for x in ['mouth','exterior_head_and_complete_jaw','eye_sockets']}
for t in range(0,len(idx),3):
 u,v=uv[ui[t]]
 group='mouth' if .757633<=u<=.923741 and v<.136441 else 'exterior_head_and_complete_jaw' if u>=.641249 and .148696<=v<=.8176341 else 'eye_sockets' if u<.16 and .54<v<.64 else None
 if group:groups[group].update(idx[t:t+3])
out={'identity':'GhostLoop','source_header_sha256':hashlib.sha256(h.read_bytes()).hexdigest(),'index_basis':'All generated and upstream OBJ indices are zero based. The complete exterior-head chart includes the jaw and chin; no arbitrary Y cutoff chooses or weights the jaw.','runtime_weight_rule':'Every source position touching exterior-head, mouth or eye-socket charts is rigid Head. Remaining neck weights use distance along actual source edges from that seam.','reviewed_motion':'The C++ production-model check compares every corresponding emitted corner with the actual Head matrix in 24 combined yaw/pitch/talk samples per height and LOD, including maximum yaw ±0.9 and pitch ±0.4. Deliberately corrupted mouth and exterior-head influences must fail.'}
for name,ids in groups.items():
 ids=sorted(ids);pts=[pos[i] for i in ids];out[name]={'vertex_count':len(ids),'generated_position_indices':ids,'source_vertex_indices':[source[i] for i in ids],'bounds_min':[min(p[k] for p in pts) for k in range(3)],'bounds_max':[max(p[k] for p in pts) for k in range(3)]}
p=r/'ghost-evidence/JAW_MOUTH_SOURCE_WITNESS.json';p.write_text(json.dumps(out,indent=2)+'\n');print({k:len(v) for k,v in groups.items()})
