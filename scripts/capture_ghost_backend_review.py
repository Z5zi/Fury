#!/usr/bin/env python3
"""Actual production raster/path material and fitting inspection images."""
from pathlib import Path
import os,subprocess,json,time,gzip,hashlib
from PIL import Image
root=Path(__file__).resolve().parents[1];r=root/'ghost-evidence/backend-review';r.mkdir(exist_ok=False)
base_env={k:v for k,v in os.environ.items() if not k.startswith('FURY_')};base_env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1')
records=[]
cases=[('path-head-45',['--face','--yaw','45']),('path-head-minus45',['--face','--yaw','-45']),('path-head-clay',['--face','--yaw','45','--clay']),('path-body-run',['--pose','run','--phase','.1875','--yaw','90']),('path-body-crouch-stress',['--pose','crouch-walk','--phase','0','--yaw','45']),('path-far-head',['--face','--yaw','45','--lod','far']),('soft-head-45',['--face','--yaw','45']),('original-path-head-45',['--face','--yaw','45'])]
for name,args in cases:
 original=name.startswith('original');path='path' in name;exe=root/('ghost-evidence/baseline-binaries/fury_characterlab' if original else 'build/apps/characterlab/fury_characterlab');env=base_env.copy()
 if path:env['FURY_TRACE_MODE']='path'
 cmd=[str(exe),'--cpu-ray' if path else '--soft','--id','GhostLoop','--role','Ghost','--height','1.8','--width','384','--image-height','480','--frames','4' if path else '1','--capture',str(r/(name+'.ppm')),'--report',str(r/(name+'.json')),*args]
 if path:cmd+=['--spp','4','--bounces','3']
 started=time.monotonic()
 with (r/(name+'.log')).open('w') as f:subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
 d=json.loads((r/(name+'.json')).read_text());assert d['validation_errors']==0 and d['identity']=='GhostLoop';assert d['individualized']!=original
 raw=(r/(name+'.ppm')).read_bytes();Image.open(r/(name+'.ppm')).save(r/(name+'.png'));(r/(name+'.ppm.gz')).write_bytes(gzip.compress(raw));assert gzip.decompress((r/(name+'.ppm.gz')).read_bytes())==raw;(r/(name+'.ppm')).unlink()
 records.append({'name':name,'command':cmd,'startup_inclusive_wall_seconds':time.monotonic()-started,'ppm_sha256':hashlib.sha256(raw).hexdigest(),'report':d});(r/'manifest.json').write_text(json.dumps(records,indent=2)+'\n');print(name,d['backend'],records[-1]['startup_inclusive_wall_seconds'],flush=True)
