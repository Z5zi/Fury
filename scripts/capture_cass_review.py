#!/usr/bin/env python3
"""Capture Cass's real production diagnostic stage; no image retouching."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--characterlab',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--group',choices=['sides','poses','special','diagnostic','attention','renderer-portraits','lod-distance','all'],default='all')
p.add_argument('--png-only',action='store_true',help='Retain verified lossless PNG pixels and PPM hash; remove redundant raw PPM')
a=p.parse_args();r=a.output_dir;r.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='1',MESA_SHADER_CACHE_DIR='/tmp/cass-mesa-cache')
exe=str(a.characterlab.resolve());results=[]
def capture(name,extra,width=640,height=800):
 cmd=[exe,'--id','NpcFence','--role','Fence','--height','1.78','--width',str(width),'--image-height',str(height),'--frames','1','--capture',str(r/(name+'.ppm')),'--report',str(r/(name+'.json')),*extra]
 start=time.monotonic()
 with(r/(name+'.log')).open('w')as log:subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 d=json.loads((r/(name+'.json')).read_text());assert d['validation_errors']==0 and d['identity']=='NpcFence' and d['individualized']
 raw=r/(name+'.ppm'); png=r/(name+'.png')
 with Image.open(raw) as im:
  pixels=im.tobytes(); shape=im.size; im.save(png)
 with Image.open(png) as check: assert check.size==shape and check.tobytes()==pixels
 raw_sha=hashlib.sha256(raw.read_bytes()).hexdigest()
 if a.png_only: raw.unlink()
 results.append({'name':name,'command':cmd,'report':name+'.json','source_fingerprint':d['source_sha256'],'framebuffer_png':png.name,'lossless_png_pixels_verified':True,'original_ppm_sha256':raw_sha,'raw_ppm_retained':not a.png_only,'seconds_including_startup':time.monotonic()-start})
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
if a.group=='diagnostic':
 for lod in ['near','far']:
  names=[]
  for yaw in [0,45,90]:
   name=f'{lod}-GLASSES-REMOVED-diagnostic-{yaw}'
   capture(name,['--lod',lod,'--face','--yaw',str(yaw),'--cass-no-glasses'])
   assert json.loads((r/(name+'.json')).read_text())['glasses_removed_diagnostic']
   names.append(name)
  sheet(names,lod+'-GLASSES-REMOVED-diagnostic-sheet',3)
if a.group=='attention':
 for lod in ['near','far']:
  names=[]
  for turn in [-.9,.9]:
   for pitch in [-.4,.4]:
    for yaw in [0,90,180,-90]:
     name=f'{lod}-attention-yaw-{turn}-pitch-{pitch}-view-{yaw}'
     capture(name,['--lod',lod,'--face','--pose','talk','--phase','.5','--attention-yaw',str(turn),'--attention-pitch',str(pitch),'--yaw',str(yaw)])
     names.append(name)
  sheet(names,lod+'-attention-extremes-sheet',4)
if a.group=='renderer-portraits':
 for lod in ['near','far']:
  names=[]
  for backend,args in [('gl',[]),('software',['--soft']),('cpu-ray',['--cpu-ray','--spp','4','--bounces','2'])]:
   for yaw in [0,45]:
    name=f'{lod}-{backend}-head-{yaw}'
    capture(name,['--lod',lod,'--face','--yaw',str(yaw),*args])
    report=json.loads((r/(name+'.json')).read_text())
    assert ('ray' in report['backend'].lower()) if backend=='cpu-ray' else ('software' in report['backend'].lower()) if backend=='software' else report['backend'].startswith('OpenGL')
    names.append(name)
  sheet(names,lod+'-renderer-portraits-sheet',3)
if a.group=='lod-distance':
 names=[]
 for distance in [11.5,12,12.5]:
  for lod in ['near','far']:
   name=f'{lod}-body-distance-{distance}'
   capture(name,['--lod',lod,'--distance',str(distance),'--yaw','35'],640,800)
   names.append(name)
 sheet(names,'lod-distance-pairs',2)
(r/('manifest-'+a.group+'.json')).write_text(json.dumps({'capture_kind':'diagnostic_engine_stage','gameplay_proof':False,'identity':'NpcFence','environment':{k:env[k]for k in ['SDL_VIDEODRIVER','FURY_CPU_THREADS','LIBGL_ALWAYS_SOFTWARE','LP_NUM_THREADS']},'captures':results},indent=2)+'\n')
