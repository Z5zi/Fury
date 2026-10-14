#!/usr/bin/env python3
"""Capture Rook's actual engine-stage pixels with source/camera/state metadata.

This is diagnostic stage evidence, not a gameplay driver. PPMs are encoded
losslessly to PNG immediately to keep disk use bounded.
"""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser()
p.add_argument('--binary',required=True);p.add_argument('--output',required=True)
p.add_argument('--suite',choices=['body','face','all','motion'],default='all')
p.add_argument('--baseline',action='store_true');p.add_argument('--width',type=int,default=480)
a=p.parse_args();binary=Path(a.binary).resolve();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_CPU_THREADS=str(min(2,len(os.sched_getaffinity(0))) if hasattr(os,'sched_getaffinity') else 2),FURY_AUDIO_BACKEND='null')
sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
shots=[]
if a.suite in ['all','body','face']:
 for face in ([False,True] if a.suite=='all' else [a.suite=='face']):
  for lod in ['near','far']:
   for clay in [False,True]:
    for yaw in [0,45,90,135,180,-135,-90,-45]:
     shots.append((f'{"face" if face else "body"}-{lod}-{"clay" if clay else "material"}-{yaw}',
      ['--yaw',str(yaw),'--lod',lod]+(['--face'] if face else [])+(['--clay'] if clay else [])))
else:
 for lod in ['near','far']:
  for pose in ['idle','walk','run','talk','crouch','crouch-walk']:
   for phase in [0,.25,.5,.75]:
    for yaw in [0,90]:
     extra=['--pose',pose,'--phase',str(phase),'--yaw',str(yaw),'--lod',lod]
     if pose=='talk':extra+=['--attention-yaw','.8','--attention-pitch','-.2']
     shots.append((f'{pose}-{lod}-p{phase}-y{yaw}',extra))
manifest={'capture_kind':'diagnostic_engine_stage','world_proof':False,'binary':str(binary),'binary_sha256':sha(binary),'baseline':a.baseline,'environment':{k:env[k] for k in ['SDL_VIDEODRIVER','SDL_AUDIODRIVER','FURY_CPU_THREADS','FURY_AUDIO_BACKEND']},'captures':[]}
for name,extra in shots:
 ppm=out/(name+'.ppm');report=out/(name+'.json');log=out/(name+'.log')
 cmd=[str(binary),'--soft','--id','CrewRook','--role','CrewTech','--height','1.70','--width',str(a.width),'--image-height',str(a.width*4//3),'--frames','1','--capture',str(ppm),'--report',str(report),*extra]
 started=time.time()
 with log.open('w') as f:result=subprocess.run(cmd,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
 if result.returncode:raise RuntimeError(f'{name}: {log.read_text()}')
 png=ppm.with_suffix('.png');Image.open(ppm).save(png)
 record=json.loads(report.read_text());record.update(command=cmd,image=str(png),png_sha256=sha(png),ppm_sha256=sha(ppm),process_wall_seconds=time.time()-started)
 ppm.unlink();manifest['captures'].append(record)
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print(name,record['triangles'],record['last_cpu_frame_ms'],flush=True)
# The smaller contact sheets are indexes; full individual PNGs remain available.
for i in range(0,len(shots),8):
 group=shots[i:i+8];cw,ch=240,340
 sheet=Image.new('RGB',(cw*4,ch*2),(24,25,27));draw=ImageDraw.Draw(sheet)
 for j,(name,_) in enumerate(group):
  im=Image.open(out/(name+'.png'));im.thumbnail((cw,ch-20))
  x=j%4*cw;y=j//4*ch;sheet.paste(im,(x+(cw-im.width)//2,y+20));draw.text((x+4,y+3),name,fill='white')
 sheet.save(out/f'contact-{i//8:02d}.png')
