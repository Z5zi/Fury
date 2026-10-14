#!/usr/bin/env python3
"""Exercise diagnostic crouch-walk/attention CLI without changing gameplay."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--characterlab',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--before-dir',type=Path)
a=p.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS=os.environ.get('FURY_CPU_THREADS','2'))
exe=str(a.characterlab.resolve()); records=[]
def capture(name,args):
 image=a.output_dir/(name+'.ppm');report=a.output_dir/(name+'.json')
 cmd=[exe,'--soft','--width','240','--image-height','320','--frames','1','--phase','.25','--capture',str(image),'--report',str(report),*args]
 r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 (a.output_dir/(name+'.log')).write_text(r.stdout)
 assert r.returncode==0,(name,r.stdout)
 d=json.loads(report.read_text());assert d['validation_errors']==0
 return image.read_bytes(),d
for pose in ['bind','idle','walk','talk','crouch']:
 data,d=capture('default-'+pose,['--pose',pose])
 explicit,e=capture('zero-'+pose,['--pose',pose,'--attention-yaw','0','--attention-pitch','0'])
 assert data==explicit,'Explicit zero attention changes pixels'
 assert d['posed_world_bounds_min']==e['posed_world_bounds_min'] and d['posed_world_bounds_max']==e['posed_world_bounds_max']
 assert d['attention_weight']==0
 if a.before_dir:
  assert data==(a.before_dir/('before-'+pose+'.ppm')).read_bytes(),'Omitted controls changed old pixels: '+pose
  old=json.loads((a.before_dir/('before-'+pose+'.json')).read_text())
  for key in ['pose','phase_cycles_initial','phase_cycles_final','posed_world_bounds_min','posed_world_bounds_max','camera_position','camera_target','vertices','triangles']:
   assert d[key]==old[key],('Changed old stage state',pose,key)
 records.append({'pose':pose,'omitted_and_zero_pixel_sha256':hashlib.sha256(data).hexdigest(),'old_default_compared':bool(a.before_dir)})
for flags in [['--attention-yaw','nan'],['--attention-pitch','inf'],['--attention-yaw','-inf'],['--attention-yaw','.9001'],['--attention-yaw','-.9001'],['--attention-pitch','.4001'],['--attention-pitch','-.4001'],['--pose','bind','--attention-yaw','.1'],['--pose','unknown']]:
 r=subprocess.run([exe,'--pose','idle',*flags],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 assert r.returncode!=0,('Invalid arguments accepted',flags)
 records.append({'rejected_arguments':flags,'diagnostic':r.stdout.strip()})
left,ld=capture('attention-left',['--pose','idle','--attention-yaw','-.9','--attention-pitch','-.4'])
right,rd=capture('attention-right',['--pose','idle','--attention-yaw','.9','--attention-pitch','.4'])
assert left!=right and ld['attention_weight']==rd['attention_weight']==1
cw,cd=capture('crouch-walk',['--pose','crouch-walk','--animate','--frames','4'])
assert cd['pose']=='crouchwalk' and cd['phase_cycles_final']>cd['phase_cycles_initial'] and cd['frames_submitted']==4
assert cd['posed_world_bounds_max'][1]<1.70
(a.output_dir/'summary.json').write_text(json.dumps({'checks':records,'positive_attention_bounds':[ld['posed_world_bounds_max'],rd['posed_world_bounds_max']],'crouch_walk_final_phase':cd['phase_cycles_final'],'result':'passed'},indent=2)+'\n')
print('Diagnostic stage controls passed: finite ranges, old/zero pixel/state identity, attention and advancing crouch-walk')
