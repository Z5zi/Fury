#!/usr/bin/env python3
"""GhostLoop production-stage evidence. Images only resized in contact sheets."""
import argparse,os,json,subprocess,time
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--kind',choices=['sides','poses','initial'],default='sides');p.add_argument('--baseline',action='store_true');a=p.parse_args();r=a.out;r.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1');results=[]
def capture(name,extra):
 cmd=[str(a.exe.resolve()),'--id','GhostLoop','--role','Ghost','--height','1.80','--width','576','--image-height','720','--frames','1','--capture',str((r/(name+'.ppm')).resolve()),'--report',str((r/(name+'.json')).resolve()),*extra]
 t=time.monotonic()
 with (r/(name+'.log')).open('w') as log:subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 d=json.loads((r/(name+'.json')).read_text());assert d['identity']=='GhostLoop' and d['validation_errors']==0;assert d['individualized']!=a.baseline
 Image.open(r/(name+'.ppm')).save(r/(name+'.png'));(r/(name+'.ppm')).unlink();results.append({'name':name,'command':cmd,'elapsed':time.monotonic()-t,'fingerprint':d['source_sha256']});print(name,d['triangles'],flush=True)
def sheet(names,name,columns=4):
 w,h=288,385;im=Image.new('RGB',(w*columns,h*((len(names)+columns-1)//columns)),'#20272a');draw=ImageDraw.Draw(im)
 for i,n in enumerate(names):
  v=Image.open(r/(n+'.png'));v.thumbnail((w,h-25));x=i%columns*w;y=i//columns*h;im.paste(v,(x+(w-v.width)//2,y+25));draw.text((x+6,y+7),n,fill='white')
 im.save(r/(name+'.png'))
if a.kind in ['sides','initial']:
 for lod in (['near','far'] if a.kind=='sides' else ['near']):
  for mode in ['material','clay']:
   for frame in ['body','head']:
    names=[]
    for yaw in ([0,45,90,135,180,-135,-90,-45] if a.kind=='sides' else [0,90,180,-90]):
     n=f'{lod}-{mode}-{frame}-{yaw}';capture(n,['--lod',lod,'--yaw',str(yaw),*(['--clay'] if mode=='clay' else []),*(['--face'] if frame=='head' else [])]);names.append(n)
    sheet(names,f'{lod}-{mode}-{frame}-sheet')
if a.kind=='poses':
 for lod in ['near','far']:
  names=[]
  for pose,phase in [('idle',.1),('walk',.15),('walk',.65),('run',.2),('run',.7),('crouch',0),('crouch-walk',.15),('crouch-walk',.65)]:
   for yaw in [0,90,180]:
    n=f'{lod}-{pose}-{phase}-{yaw}';capture(n,['--lod',lod,'--pose',pose,'--phase',str(phase),'--yaw',str(yaw)]);names.append(n)
  sheet(names,f'{lod}-poses-sheet',6)
  for yaw in [-.75,.75]:
   for frame in ['body','head']:
    n=f'{lod}-attention-{yaw}-{frame}';capture(n,['--lod',lod,'--pose','talk','--phase','.5','--attention-yaw',str(yaw),'--attention-pitch','-.20',*(['--face'] if frame=='head' else [])])
(r/'manifest.json').write_text(json.dumps({'identity':'GhostLoop','diagnostic_stage':True,'network_gameplay_proof':False,'baseline':a.baseline,'captures':results},indent=2)+'\n')
