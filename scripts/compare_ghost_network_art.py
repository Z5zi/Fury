#!/usr/bin/env python3
"""Compare only observed states from two real received-UDP art captures."""
import argparse,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('original',type=Path);p.add_argument('current',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
read=lambda r,n:[json.loads(s) for s in (r/n).read_text().splitlines()]
original=read(a.original,'received.jsonl');current=read(a.current,'received.jsonl');assert len(original)==len(current)
allowed={'source_fingerprint','near_pose_hash','far_pose_hash','near_revision','far_revision','near_mesh_identity','far_mesh_identity','near_vertex_capacity','far_vertex_capacity'}
for x,y in zip(original,current):assert {k:v for k,v in x.items() if k not in allowed}=={k:v for k,v in y.items() if k not in allowed},x['frame']
sent1=read(a.original,'sent.jsonl');sent2=read(a.current,'sent.jsonl');assert sent1==sent2
state1=json.loads((a.original/'presentation.json.state.json').read_text());state2=json.loads((a.current/'presentation.json.state.json').read_text());assert state1==state2
profiles=lambda r:[p for p in json.loads((r/'presentation.json').read_text())['profiles'] if p['id']!='GhostLoop'];assert profiles(a.original)==profiles(a.current)
observations=[]
for name,rows in [('original',original),('current',current)]:
 constants={}
 for k in ['scene_mesh_count','near_mesh_identity','far_mesh_identity','near_vertex_capacity','far_vertex_capacity']:
  values={r[k] for r in rows};assert len(values)==1,(name,k);constants[k]=next(iter(values))
 repeated=0
 for x,y in zip(rows,rows[1:]):
  if not x['visible'] and not y['visible']:
   assert x['near_revision']==y['near_revision'] and x['far_revision']==y['far_revision'];repeated+=1
 assert repeated>0
 observations.append({'model':name,**constants,'repeated_hidden_pairs_without_pose_revision':repeated})
report={'status':'passed','same_actual_received_frames':len(current),'identical_raw_snapshot_packets':len(sent1),'same_complete_npc_crew_heist_state':True,'same_other_15_profile_entries':True,'same_PlayerBody_transform_visibility_material_and_pose_hashes_all_frames':True,'cache_observations':observations,'allowed_art_fields':sorted(allowed),'source_fingerprints':[original[0]['source_fingerprint'],current[0]['source_fingerprint']],'input_trace_sha256':{'original':hashlib.sha256((a.original/'sent.jsonl').read_bytes()).hexdigest(),'current':hashlib.sha256((a.current/'sent.jsonl').read_bytes()).hexdigest()},'timing_limit':'Identical opt-in peer-send rendezvous then real NetClient receive/presentation; normal asynchronous behavior remains separately captured. No direct assignment of synthetic received transforms.'};a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
