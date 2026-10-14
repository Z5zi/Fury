#!/usr/bin/env python3
"""Capture Metro Watch's real production diagnostic stage; no image retouching."""
import argparse,json,os,subprocess,time
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--characterlab',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--group',choices=['sides','poses','special','all'],default='all')
p.add_argument('--legacy',action='store_true');a=p.parse_args();r=a.output_dir;r.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='1',MESA_SHADER_CACHE_DIR='/tmp/fury-metro-mesa-cache')
exe=str(a.characterlab.resolve());results=[]
def capture(name,extra,width=640,height=800):
 cmd=[exe,'--id','NpcExtraGuard','--role','Security','--height','1.82','--width',str(width),'--image-height',str(height),'--frames','1','--capture',str(r/(name+'.ppm')),'--report',str(r/(name+'.json')),*(['--legacy-profile']if a.legacy else[]),*extra]
 start=time.monotonic()
 with(r/(name+'.log')).open('w')as log:subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 d=json.loads((r/(name+'.json')).read_text());assert d['validation_errors']==0 and d['identity']=='NpcExtraGuard' and d['individualized']!=a.legacy
 Image.open(r/(name+'.ppm')).save(r/(name+'.png'))
 results.append({'name':name,'command':cmd,'report':name+'.json','seconds_including_startup':time.monotonic()-start})
 print(name,d['backend'],d['triangles'],flush=True)
def sheet(names,name,columns=4):
 tile_w,tile_h=320,430;out=Image.new('RGB',(tile_w*columns,tile_h*((len(names)+columns-1)//columns)),'#1d252a');draw=ImageDraw.Draw(out)
 for i,n in enumerate(names):
  im=Image.open(r/(n+'.png'));im.thumbnail((tile_w,tile_h-30));x=i%columns*tile_w;y=i//columns*tile_h;out.paste(im,(x+(tile_w-im.width)//2,y+30));draw.text((x+8,y+10),n,fill='white')
 out.save(r/(name+'.png'))
if a.group in ['sides','all']:
 for lod in ['near','far']:
  for mode in ['material','clay']:
   for frame in ['body','head']:
    names=[]
    for yaw in [0,45,90,135,180,-135,-90,-45]:
     name=f'{lod}-{mode}-{frame}-{yaw}';extra=['--lod',lod,'--yaw',str(yaw)]
     if mode=='clay':extra+=['--clay']
     if frame=='head':extra+=['--face']
     capture(name,extra);names.append(name)
    sheet(names,f'{lod}-{mode}-{frame}-sheet')
if a.group in ['special','all']:
 for lod in ['near','far']:
  for name,extra in [('body-low',['--elevation','-12']),('body-top',['--elevation','45']),('scalp-top',['--face','--elevation','70']),('face-under',['--face','--elevation','-25'])]:
   capture(lod+'-'+name,['--lod',lod,*extra])
 capture('near-body-detail',['--yaw','35'],960,1280)
 capture('near-body-detail-side',['--yaw','90'],960,1280)
if a.group in ['poses','all']:
 for lod in ['near','far']:
  names=[]
  for pose,phase in [('idle',.125),('walk',.15),('walk',.65),('run',.2),('run',.7),('crouch',0),('crouch-walk',.15),('crouch-walk',.65)]:
   for yaw in [0,90,180]:
    name=f'{lod}-{pose}-{phase}-{yaw}';capture(name,['--lod',lod,'--pose',pose,'--phase',str(phase),'--yaw',str(yaw)]);names.append(name)
  sheet(names,lod+'-poses-sheet',6)
 for turn in [-.75,.75]:
  for face in [False,True]:
   name=f'near-talk-turn-{turn}-'+('head'if face else'body')
   capture(name,['--pose','talk','--phase','.5','--attention-yaw',str(turn),'--attention-pitch','-.20',*(['--face']if face else[])])
(r/('manifest-'+a.group+'.json')).write_text(json.dumps({'capture_kind':'diagnostic_engine_stage','gameplay_proof':False,'identity':'NpcExtraGuard','environment':{k:env[k]for k in ['SDL_VIDEODRIVER','FURY_CPU_THREADS','LIBGL_ALWAYS_SOFTWARE','LP_NUM_THREADS']},'captures':results},indent=2)+'\n')
