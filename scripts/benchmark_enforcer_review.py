#!/usr/bin/env python3
"""Bounded single-worker frame-loop timing; no hardware/AAA performance claim."""
import argparse,json,os,subprocess,time,statistics
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--characterlab',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1')
rows=[]
for backend in ['opengl','software']:
 for lod in ['near','far']:
  for legacy in [True,False]:
   values=[]
   for trial in range(3):
    name=f'{backend}-{lod}-'+('legacy'if legacy else'enforcer')+f'-{trial}'
    cmd=[str(a.characterlab.resolve()),'--id','NpcEnforcer','--role','Enforcer','--height','1.95','--lod',lod,'--width','320','--image-height','480','--pose','walk','--phase','.25','--animate','--frames','24','--capture',str(out/(name+'.ppm')),'--report',str(out/(name+'.json'))]
    if backend=='software':cmd+=['--soft']
    if legacy:cmd+=['--legacy-profile']
    start=time.monotonic();r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);elapsed=time.monotonic()-start;(out/(name+'.log')).write_text(r.stdout);assert r.returncode==0,r.stdout[-2000:]
    d=json.loads((out/(name+'.json')).read_text());assert d['validation_errors']==0 and d['frames_submitted']==24
    (out/(name+'.ppm')).unlink()
    values.append({'command':cmd,'process_seconds':elapsed,'frame_loop_seconds':d['wall_seconds'],'mean_frame_loop_ms':1000*d['wall_seconds']/24,'last_cpu_frame_ms':d['last_cpu_frame_ms'],'vertices':d['vertices'],'triangles':d['triangles'],'source_sha256':d['source_sha256']})
   rows.append({'backend':backend,'lod':lod,'legacy':legacy,'median_mean_frame_loop_ms':statistics.median(v['mean_frame_loop_ms']for v in values),'samples':values});print(name,rows[-1]['median_mean_frame_loop_ms'],flush=True)
(out/'summary.json').write_text(json.dumps({'scope':'Three 24-frame runs per case at 320x480; one pinned CPU; OpenGL is llvmpipe, not a hardware GPU. Shared host activity was not excluded.','notes':['Frame-loop time excludes profile construction; total process time is retained separately.','No isolated constructor, native GPU, DXR, 1440p/120 fps or production-game FPS claim.','The legacy profile is the original generic actor/material in the same executable.','Software last-frame timing is reported as measured; OpenGL zero CPU counters are unavailable, not zero-cost frames.'],'cases':rows},indent=2)+'\n')
