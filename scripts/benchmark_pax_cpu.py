#!/usr/bin/env python3
"""Bounded CPU portrait and populated-scene samples; no live FPS inference."""
import argparse,json,os,statistics,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--repeats',type=int,default=3);a=p.parse_args();r=a.output_dir.resolve();r.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1];stage=root/'build/apps/characterlab/fury_characterlab';game=root/'build/apps/vaultline/vaultline'
e=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='2',FURY_NPC_DETAIL='1',FURY_NET_PORT='47176')
rows=[]
for repeat in range(a.repeats):
 for scene in ['portrait-legacy','portrait-pax','world-frozen','world-live']:
  for mode in ['raster','path']:
   name=f'{scene}-{mode}-{repeat+1}';path=mode=='path';portrait=scene.startswith('portrait');report=r/(name+'.json')
   if portrait:
    cmd=[str(stage),'--cpu-ray'if path else'--soft','--id','NpcCivD','--role','Dock','--height','1.72','--face','--width','240'if path else'480','--image-height','320'if path else'640','--frames','2','--report',str(report),'--capture',str(r/(name+'.ppm'))]
    if scene.endswith('legacy'):cmd+=['--legacy-profile']
   else:
    cmd=[str(game),'--cpu-ray'if path else'--soft','--width','320'if path else'480','--height','240'if path else'360','--frames','4','--capture-fps','12','--no-hud','--npc-motion'if scene.endswith('live')else'--npc-view','NpcCivD','--npc-distance','3.2','--npc-audit',str(report),'--capture',str(r/(name+'.ppm'))]
   if path:cmd+=['--spp','1','--bounces','2']
   env=e.copy();env.pop('FURY_TRACE_MODE',None)
   if path:env['FURY_TRACE_MODE']='path'
   work=r/(name+'-work');work.mkdir(exist_ok=False);(work/'assets').symlink_to(root/'assets',target_is_directory=True)
   start=time.monotonic()
   with(r/(name+'.log')).open('w')as f:subprocess.run(cmd,env=env,cwd=work,stdout=f,stderr=subprocess.STDOUT,check=True)
   wall=time.monotonic()-start;data=json.loads((report if portrait else Path(str(report)+'.render.json')).read_text());assert data['validation_errors']==0
   if portrait:assert data['trace_mode']==mode
   rows.append({'name':name,'scene':scene,'mode':mode,'repeat':repeat+1,'startup_inclusive_wall_seconds':wall,'last_renderer_ms':data['last_cpu_frame_ms'if portrait else'cpu_frame_ms'],'command':cmd,'report':data});print(name,round(rows[-1]['last_renderer_ms'],3),round(wall,3),flush=True)
   (r/'samples.json').write_text(json.dumps(rows,indent=2)+'\n')
summary=[]
for scene in ['portrait-legacy','portrait-pax','world-frozen','world-live']:
 for mode in ['raster','path']:
  samples=[v for v in rows if v['scene']==scene and v['mode']==mode];values=[v['last_renderer_ms']for v in samples]
  summary.append({'scene':scene,'mode':mode,'samples':len(samples),'renderer_ms_median':statistics.median(values),'renderer_ms_min':min(values),'renderer_ms_max':max(values),'application_wall_seconds_median':statistics.median(v['startup_inclusive_wall_seconds']for v in samples)})
(r/'summary.json').write_text(json.dumps({'threads':2,'description':'Last renderer frame time and separate startup-inclusive application time. Fixed simulation/encoding cadence is not measured live FPS. CPU path uses 1 spp, 2 bounces. World cases are absolute current-scene costs including the original population on the fixed Pax private base; no matched pre-change world binary is available. Each process has a fresh working directory and assets symlink.','results':summary},indent=2)+'\n')
