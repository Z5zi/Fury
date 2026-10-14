#!/usr/bin/env python3
"""Pax's existing world, actors and AI; bounded captures without scene edits."""
import argparse,json,math,os,subprocess,time
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--vaultline',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--stage-report',type=Path,required=True);a=p.parse_args();r=a.output_dir;r.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='2',FURY_NPC_DETAIL='1',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',MESA_SHADER_CACHE_DIR='/tmp/fury-mesa-cache',FURY_NET_PORT='47175')
exe=str(a.vaultline.resolve());records=[]
def capture(name,extra,frames=1,sequence=False,width=720,height=540,soft=False):
 cmd=[exe,*(['--soft']if soft else[]),'--frames',str(frames),'--capture-fps','12','--width',str(width),'--height',str(height),'--no-hud','--capture',str(r/(name+'.ppm')),'--npc-audit',str(r/(name+'.json')),*extra]
 if sequence:cmd+=['--capture-sequence',str(r/(name+'-frames'))]
 e=env|({'SDL_VIDEODRIVER':'dummy'}if soft else{})
 start=time.monotonic()
 with(r/(name+'.log')).open('w')as f:subprocess.run(cmd,env=e,stdout=f,stderr=subprocess.STDOUT,check=True)
 d=json.loads((r/(name+'.json.render.json')).read_text());assert d['validation_errors']==0
 Image.open(r/(name+'.ppm')).save(r/(name+'.png'))
 records.append({'name':name,'command':cmd,'wall_seconds_including_startup':time.monotonic()-start,'simulated_seconds':frames/12 if sequence else 0,'metrics':d})
 print(name,records[-1]['wall_seconds_including_startup'],flush=True)
for name,orbit,distance in [('spawn-front',0,3.2),('spawn-right',90,3.2),('spawn-back',180,3.2),('spawn-face',0,1.05)]:capture(name,['--npc-view','NpcCivD','--npc-distance',str(distance),'--npc-orbit',str(orbit)])
capture('spawn-raster-matched',['--npc-view','NpcCivD','--npc-distance','3.2'],soft=True)
# The renderer uses distance to the actual primary AABB, not actor-center or
# the --npc-distance number. Solve that exact bind-space rule for 12 m.
stage=json.loads(a.stage_report.read_text());lo=stage['posed_world_bounds_min'];hi=stage['posed_world_bounds_max']
def distance(d):
 # The original front camera is inside a building at this distance. Orbit
 # east and raise the diagnostic camera; leave actors and scenery unchanged.
 x,z=d,-.28*d;horizontal=math.hypot(x,z);angle=math.radians(12)
 raised=math.cos(angle)*horizontal-math.sin(angle)*.18
 eye=[x*raised/horizontal,.86+math.sin(angle)*horizontal+math.cos(angle)*.18,z*raised/horizontal]
 return math.sqrt(sum(max(0,lo[k]-eye[k],eye[k]-hi[k])**2 for k in range(3)))
lower,upper=0.,30.
for _ in range(60):
 mid=(lower+upper)/2
 if distance(mid)<12:lower=mid
 else:upper=mid
switch=(lower+upper)/2
for name,d in [('lod-before',switch-.035),('lod-after',switch+.035)]:
 capture(name,['--npc-view','NpcCivD','--npc-distance',str(d),'--npc-orbit','90','--npc-elevation','12'],width=960,height=720,soft=True)
 records[-1]['predicted_primary_aabb_distance_m']=distance(d);records[-1]['lod_rule']='Near <= 12 m; Far > 12 m from primary actual AABB'
capture('live-route',['--npc-motion','NpcCivD','--npc-distance','3.2'],frames=360,sequence=True,width=640,height=480)
capture('live-talk-presentation',['--npc-motion','NpcCivD','--npc-distance','1.10','--npc-talk'],frames=48,sequence=True,width=640,height=480)
for name in ['live-route','live-talk-presentation']:
 cmd=['ffmpeg','-v','error','-framerate','12','-start_number','1','-i',str(r/(name+'-frames')/'frame_%06d.ppm'),'-c:v','libx264','-threads','2','-pix_fmt','yuv420p','-crf','21','-y',str(r/(name+'.mp4'))]
 subprocess.run(cmd,check=True)
 frames=sorted((r/(name+'-frames')).glob('frame_*.ppm'));indices=[round(i*(len(frames)-1)/11)for i in range(12)]
 out=Image.new('RGB',(1280,780),'#20252a');draw=ImageDraw.Draw(out)
 for j,i in enumerate(indices):
  im=Image.open(frames[i]);im.thumbnail((320,240));x=j%4*320;y=j//4*260;out.paste(im,(x,y+20));draw.text((x+8,y+4),f'{name}: frame {i+1}, t={i/12:.2f}s',fill='white')
 out.save(r/(name+'-sheet.png'))
(r/'manifest.json').write_text(json.dumps({'identity':'NpcCivD','world_or_ai_changes':False,'gameplay_dialogue_input':False,'notes':['Motion runs real AI with fixed timestep; encoded cadence is not measured FPS.','Talk flag is presentation only, not Q input.','OpenGL uses Mesa llvmpipe; no physical GPU claim.'],'captures':records},indent=2)+'\n')
