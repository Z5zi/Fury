#!/usr/bin/env python3
"""Validate actual received-state traces; never treat sent ticks as received frames."""
import argparse,json,math,struct,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--lod',choices=['near','far'],default='near');a=p.parse_args();r=a.directory
received=[json.loads(x) for x in (r/'received.jsonl').read_text().splitlines()];sent=[json.loads(x) for x in (r/'sent.jsonl').read_text().splitlines()]
close=lambda a,b:abs(a-b)<3e-5
frames={'hidden':[],'fresh_peer':[],'received_motion':[],'cash_flash':[],'disconnected':[]};previous=None;maximum_gap=0;packet_hashes=[]
for tick in sent:
 if 'packet_hex' not in tick:continue
 packet=bytes.fromhex(tick['packet_hex']);magic,version,kind,size=struct.unpack_from('<IHHI',packet);assert magic==0x564c544c and version==1 and kind==4 and len(packet)==12+size
 count,=struct.unpack_from('<H',packet,12);assert count==tick['roster_count'];packet_hashes.append(hashlib.sha256(packet).hexdigest())
 if count:
  fields=struct.unpack_from('<IfffffBBBBf',packet,14);assert fields[0]==tick['id'];assert all(close(x,y) for x,y in zip(fields[1:4],tick['position']))
for d in received:
 available=d['connected'] and bool(d['received']);assert d['visible']==available,(d['frame'],'visibility')
 if not available:
  assert not d['position_valid'] and d['travel_distance']==0 and d['last_cash']==-1 and d['cash_flash']==0
  assert all(close(x,1) for x in d['albedo']) and close(d['emissive'],.05)
  frames['hidden'].append(d['frame'])
  if previous and not(previous['connected'] and previous['received']):
   assert previous['near_pose_hash']==d['near_pose_hash'] and previous['far_pose_hash']==d['far_pose_hash'],'repeated absence must not keep reposing'
 else:
  state=d['received'][0];assert d['position_valid']
  assert all(close(x,y) for x,y in zip(d['entity_position'],[state['position'][0],.9,state['position'][2]]));assert close(d['entity_yaw'],math.pi/2-state['yaw'])
  fresh=not previous or not previous['connected'] or not previous['received'] or previous['received'][0]['id']!=state['id']
  if fresh:
   frames['fresh_peer'].append(d['frame']);assert d['travel_distance']==0 and d['cash_flash']==0
  else:
   dist=min(1,math.hypot(d['entity_position'][0]-previous['entity_position'][0],d['entity_position'][2]-previous['entity_position'][2]));assert close(d['travel_distance']-previous['travel_distance'],dist)
   if dist>1e-6:frames['received_motion'].append(d['frame'])
   flash=.85 if state['cash']>previous['last_cash']+.5 else previous['cash_flash'];assert close(d['cash_flash'],max(0,flash-d['dt']))
  heat=max(0,min(1,state['heat']));flash=d['cash_flash'];assert all(close(x,y) for x,y in zip(d['albedo'],[1+.2*flash,1-.15*heat,1-.2*heat]));assert close(d['emissive'],(.45 if state['in_heist'] else .05)+1.8*flash)
  assert close(d['last_cash'],state['cash'])
  if flash>0:frames['cash_flash'].append(d['frame'])
 if not d['connected']:frames['disconnected'].append(d['frame'])
 previous=d
assert packet_hashes and frames['hidden'] and frames['fresh_peer'] and frames['received_motion'] and frames['cash_flash'] and frames['disconnected']
assert len(set(d[a.lod+'_pose_hash'] for d in received if d['frame'] in frames['received_motion']))>5
report={'status':'passed','source_fingerprints':sorted(set(d['source_fingerprint'] for d in received)),'received_frames':len(received),'reviewed_lod':a.lod,'sent_udp_packets':len(packet_hashes),'exact_packet_sha256':packet_hashes,'frames':frames,'assertions':['visible iff connected and received roster nonempty','initial and reappearing peer starts travel and cash flash at zero','remote heading and position follow actual received state','distance-derived travel follows real received position deltas','existing heat/cash emissive and albedo equations remain exact','empty/disconnect clears position-valid, accumulated travel and cash history','repeated hidden frames leave cached poses stable','real received motion changes '+a.lod+' mesh pose','packet bytes use exact production v1 snapshot layout'],'timing_limit':'UDP receive may lag send by a simulation frame; sent tick and received frame numbers are separate. Protocol has no sequence or timestamp ordering; last-arrival behavior is separately covered by the socket test.'}
(r/'NETWORK_CONTRACT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='exact_packet_sha256'},indent=2))
