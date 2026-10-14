#!/usr/bin/env python3
"""Actual production-stage detail comparisons, with lossless pixel preservation."""
import argparse,json,os,subprocess,hashlib
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--characterlab',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1')
cases=[]
for lod in ['near','far']:
 for yaw in [0,45,90,135,180,-135,-90,-45]:cases.append((f'{lod}-hands-torso-{yaw}',['--lod',lod,'--yaw',str(yaw),'--distance','1.4']))
 for pose,phase in [('crouch','0'),('crouch-walk','.15'),('run','.2')]:
  for yaw in [0,90,180]:cases.append((f'{lod}-{pose}-{phase}-{yaw}',['--lod',lod,'--pose',pose,'--phase',phase,'--yaw',str(yaw)]))
records=[]
for name,extra in cases:
 cmd=[str(a.characterlab.resolve()),'--id','NpcEnforcer','--role','Enforcer','--height','1.95','--width','800','--image-height','800','--frames','1','--capture',str(out/(name+'.ppm')),'--report',str(out/(name+'.json')),*extra]
 r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120);(out/(name+'.log')).write_text(r.stdout);assert r.returncode==0,r.stdout[-2000:]
 d=json.loads((out/(name+'.json')).read_text());assert d['validation_errors']==0 and d['identity']=='NpcEnforcer' and d['individualized']
 im=Image.open(out/(name+'.ppm'));im.save(out/(name+'.png'));assert im.tobytes()==Image.open(out/(name+'.png')).tobytes();(out/(name+'.ppm')).unlink()
 records.append({'name':name,'command':cmd,'source_sha256':d['source_sha256'],'png_sha256':hashlib.sha256((out/(name+'.png')).read_bytes()).hexdigest()});print(name,d['triangles'],flush=True)
(out/'manifest.json').write_text(json.dumps({'kind':'actual-production-character-stage','purpose':'Hands/cuffs/torso detail and ordinary Far knee correction','scenery_or_actor_origin_changed':False,'runs':records},indent=2)+'\n')
