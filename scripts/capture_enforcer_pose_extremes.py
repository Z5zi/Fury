#!/usr/bin/env python3
"""Inspect own-source jaw/neck, height and LOD appearance with the real stage."""
import argparse,os,json,subprocess
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--characterlab',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1');cases=[]
for lod in ['near','far']:
 for yaw in [-.9,0,.9]:
  for pitch in [-.4,0,.4]:
   for view in [0,90,180,-90]:cases.append((f'{lod}-attention-{yaw}-{pitch}-view-{view}',1.95,['--lod',lod,'--pose','talk','--phase','.125','--attention-yaw',str(yaw),'--attention-pitch',str(pitch),'--yaw',str(view),'--face'],640,800))
 for height in [1.35,2.1]:
  for pose in ['run','crouch']:
   for view in [45,135]:cases.append((f'{lod}-height-{height}-{pose}-{view}',height,['--lod',lod,'--pose',pose,'--phase','.2','--yaw',str(view)],480,640))
 for pose in ['bind','walk','crouch']:cases.append((f'{lod}-distance12-{pose}',1.95,['--lod',lod,'--pose',pose,'--phase','.15','--yaw','45','--distance','12'],960,540))
 for view in [0,90,180]:cases.append((f'{lod}-knee-crouch-{view}',1.95,['--lod',lod,'--pose','crouch','--phase','0','--yaw',str(view)],640,800))
records=[]
for name,height,extra,width,pixels in cases:
 cmd=[str(a.characterlab.resolve()),'--id','NpcEnforcer','--role','Enforcer','--height',str(height),'--width',str(width),'--image-height',str(pixels),'--frames','1','--capture',str(out/(name+'.ppm')),'--report',str(out/(name+'.json')),*extra]
 result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120);(out/(name+'.log')).write_text(result.stdout);assert result.returncode==0,result.stdout[-2000:]
 d=json.loads((out/(name+'.json')).read_text());assert d['identity']=='NpcEnforcer'and d['individualized']and d['validation_errors']==0
 im=Image.open(out/(name+'.ppm'));im.save(out/(name+'.png'));assert im.tobytes()==Image.open(out/(name+'.png')).tobytes();(out/(name+'.ppm')).unlink();records.append({'name':name,'command':cmd,'source_sha256':d['source_sha256']});print(name,flush=True)
(out/'manifest.json').write_text(json.dumps({'kind':'actual-production-diagnostic-stage','lod_distance_note':'Near/Far are forced diagnostic profiles at 12m, not an automatic gameplay transition claim','captures':records},indent=2)+'\n')
