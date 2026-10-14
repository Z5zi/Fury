#!/usr/bin/env python3
"""Sequential production-scene NPC A/B benchmark; timings are observations, not a frame-rate guarantee."""
import argparse,hashlib,json,os,socket,statistics,subprocess,time,sys
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--vaultline',type=Path,required=True)
p.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--repeats',type=int,default=3)
a=p.parse_args()
if not 1<=a.repeats<=5:p.error('--repeats must be 1..5')
a.vaultline=a.vaultline.resolve();a.source_root=a.source_root.resolve();a.output_dir=a.output_dir.resolve();a.output_dir.mkdir(parents=True,exist_ok=True)
base={k:v for k,v in os.environ.items() if not k.startswith('FURY_')}
base.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='2',
            FURY_TRACE_MODE='ray',FURY_WORLD_ART='1',FURY_SURFACE_DETAIL='1',FURY_NET='embedded',LC_ALL='C')
records=[]
for quality,width,height in [('low',320,180),('high',480,360)]:
 for backend in ('raster','ray'):
  for repetition in range(a.repeats):
   for detail in (0,1):
    case=a.output_dir/f'{quality}-{backend}-{detail}-{repetition}';case.mkdir()
    work=case/'work';work.mkdir();(work/'assets').symlink_to(a.source_root/'assets',target_is_directory=True)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
     sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    cmd=[str(a.vaultline),'--soft' if backend=='raster' else '--cpu-ray','--npc-motion','NpcCivA','--frames','8' if backend=='raster' else '4',
         '--capture-fps','12','--width',str(width),'--height',str(height),'--no-hud','--npc-audit',str(case/'audit.json')]
    if backend=='ray':cmd+=['--spp','1','--bounces','2']
    wrapped=cmd
    if os.name=='posix':
     # One fresh Python parent per sample: RUSAGE_CHILDREN belongs only to this game process.
     resource_code="import json,resource,subprocess,sys,time; t=time.monotonic(); p=subprocess.run(sys.argv[2:]); json.dump({'peak_rss_kib':resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,'elapsed_seconds':time.monotonic()-t},open(sys.argv[1],'w')); sys.exit(p.returncode)"
     wrapped=[sys.executable,'-c',resource_code,str(case/'resource.json')]+cmd
    start=time.monotonic()
    with (case/'runtime.log').open('w') as log:subprocess.run(wrapped,cwd=work,env=base|{'FURY_NPC_DETAIL':str(detail),'FURY_QUALITY':quality,'FURY_NET_PORT':str(port)},stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
    elapsed=time.monotonic()-start
    metrics=json.loads((case/'audit.json.render.json').read_text())
    if metrics['validation_errors'] or metrics['cpu_frame_ms']<=0:raise RuntimeError('Invalid renderer metrics')
    record={'quality':quality,'width':width,'height':height,'backend':backend,'detail':detail,'repeat':repetition,
            'wall_seconds_including_startup':elapsed,'metrics':metrics,'profiles':json.loads((case/'audit.json').read_text()),
            'resources':json.loads((case/'resource.json').read_text()) if (case/'resource.json').exists() else {},'command':cmd}
    records.append(record)
    print(quality,backend,detail,repetition,round(metrics['cpu_frame_ms'],2),'ms',flush=True)
summary=[]
for quality,width,height in [('low',320,180),('high',480,360)]:
 for backend in ('raster','ray'):
  row={'quality':quality,'width':width,'height':height,'backend':backend}
  for detail in (0,1):
   samples=[r for r in records if r['quality']==quality and r['backend']==backend and r['detail']==detail]
   row[str(detail)]={'median_last_render_ms':statistics.median(r['metrics']['cpu_frame_ms'] for r in samples),
                    'median_startup_inclusive_seconds':statistics.median(r['wall_seconds_including_startup'] for r in samples),
                    'peak_rss_mib':max((r['resources'].get('peak_rss_kib',0)/1024 for r in samples)),
                    'last_frame_samples_ms':[r['metrics']['cpu_frame_ms'] for r in samples]}
  row['render_cost_ratio']=row['1']['median_last_render_ms']/row['0']['median_last_render_ms'];summary.append(row)
report={'executable_sha256':hashlib.sha256(a.vaultline.read_bytes()).hexdigest(),'benchmark_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'cpu_workers':2,'repeats':a.repeats,'notes':['Sequential matched production scene; no parallel benchmark cases.','Last-frame software timing includes submission/raster/presentation; ray timing includes TLAS/trace/resolve but excludes earlier BLAS preparation. Both exclude main-loop skinning/update. Startup-inclusive wall times include the complete run.','High/low differ in cull/fog/FX as well as resolution.','Peak RSS uses Linux RUSAGE_CHILDREN (KiB); platform-specific units need adjustment elsewhere.','No claim of real-time ray tracing or hardware-GPU performance.'],
        'summary':summary,'records':records}
(a.output_dir/'benchmark.json').write_text(json.dumps(report,indent=2)+'\n')
