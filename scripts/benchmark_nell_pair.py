#!/usr/bin/env python3
"""Plan or serially measure a source-verified generic/Nell binary pair. No build or source edits."""
import argparse, hashlib, json, os, socket, statistics, subprocess, sys
from pathlib import Path

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

p=argparse.ArgumentParser(description=__doc__)
for key in ['before-bin','after-bin','before-manifest','after-manifest','source-root','output-dir']:
    p.add_argument('--'+key, type=Path, required=True)
p.add_argument('--run', action='store_true', help='Run only in a reserved idle measurement window; default prints the plan')
p.add_argument('--repeats', type=int, default=3)
a=p.parse_args()
if not 1 <= a.repeats <= 5: p.error('repeats must be 1..5')
roots={'before':a.before_bin.resolve(),'after':a.after_bin.resolve()}
manifest={k:json.loads(v.read_text()) for k,v in [('before',a.before_manifest),('after',a.after_manifest)]}
inputs={k:v.get('source_file_sha256',v.get('source_sha256')) for k,v in manifest.items()}
if not all(isinstance(x,dict) for x in inputs.values()): raise ValueError('Each manifest needs a complete rendering-input SHA256 map')
if inputs['before'].keys()!=inputs['after'].keys(): raise ValueError('Source input sets differ')
different=[k for k in inputs['before'] if inputs['before'][k]!=inputs['after'][k]]
if different!=['engine/src/character_profile.cpp']: raise ValueError(f'Unmatched pair: source differences are {different}')
for variant in roots:
    for binary in ['fury_characterlab','vaultline']:
        if sha(roots[variant]/binary)!=manifest[variant]['binaries'][binary]: raise ValueError(f'{variant} binary hash mismatch: {binary}')
source=a.source_root.resolve(); out=a.output_dir.resolve()
base={k:v for k,v in os.environ.items() if not k.startswith('FURY_')}
base.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='2',FURY_TRACE_MODE='ray',FURY_WORLD_ART='1',FURY_SURFACE_DETAIL='1',FURY_NPC_DETAIL='1',FURY_QUALITY='high',FURY_NET='embedded',LC_ALL='C')
plan=[]
for scenario in ['stage-near','stage-far','world']:
    for backend in ['raster','ray']:
        for repeat in range(a.repeats):
            for variant in (['before','after'] if repeat%2==0 else ['after','before']):
                case=out/f'{scenario}-{backend}-{repeat}-{variant}'
                common=['--soft' if backend=='raster' else '--cpu-ray','--frames','8' if backend=='raster' else '4','--no-hud']
                if backend=='ray': common+=['--spp','1','--bounces','2']
                if scenario=='world':
                    cmd=[str(roots[variant]/'vaultline')]+common+['--npc-motion','NpcCivAsh','--npc-distance','3.2','--npc-orbit','45','--npc-elevation','25','--capture-fps','12','--width','480','--height','360','--npc-audit',str(case/'audit.json')]
                    report=case/'audit.json.render.json'
                else:
                    cmd=[str(roots[variant]/'fury_characterlab')]+common+['--id','NpcCivAsh','--role','Market','--height','1.75','--lod',scenario.split('-')[1],'--pose','walk','--phase','0','--animate','--fps','12','--yaw','45','--distance','3.2375','--width','480','--image-height','640','--report',str(case/'report.json')]
                    report=case/'report.json'
                plan.append(dict(scenario=scenario,backend=backend,repeat=repeat,variant=variant,command=cmd,case=str(case),report=str(report)))
if not a.run:
    print(json.dumps({'status':'plan_only_not_measured','source_difference':different,'settings':{k:v for k,v in base.items() if k.startswith(('FURY_','SDL_'))},'cases':plan},indent=2)); sys.exit()
out.mkdir(parents=True,exist_ok=False)
records=[]
wrapper="import json,resource,subprocess,sys,time; t=time.monotonic(); r=subprocess.run(sys.argv[2:]); json.dump({'peak_rss_kib':resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,'elapsed_seconds':time.monotonic()-t},open(sys.argv[1],'w')); sys.exit(r.returncode)"
for spec in plan:
    case=Path(spec['case']); case.mkdir(); work=case/'work'; work.mkdir(); (work/'assets').symlink_to(source/'assets',target_is_directory=True)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s: s.bind(('127.0.0.1',0)); port=s.getsockname()[1]
    env=base|{'FURY_NET_PORT':str(port)}
    with (case/'runtime.log').open('w') as log:
        subprocess.run([sys.executable,'-c',wrapper,str(case/'resource.json')]+spec['command'],cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240,check=True)
    d=json.loads(Path(spec['report']).read_text())
    observed=d.get('source_sha256',d.get('source_fingerprint'))
    if observed!=manifest[spec['variant']]['source_fingerprint']: raise ValueError('Observed source fingerprint mismatch')
    ms=d.get('last_cpu_frame_ms',d.get('cpu_frame_ms'))
    if d['validation_errors'] or not ms or ms<=0: raise ValueError('Invalid renderer metrics')
    if spec['scenario']!='world' and d['individualized']!=(spec['variant']=='after'): raise ValueError('Wrong generic/individualized identity')
    record=spec|{'metrics':d,'resources':json.loads((case/'resource.json').read_text()),'last_frame_ms':ms}
    records.append(record); (out/'records.json').write_text(json.dumps(records,indent=2)+'\n')
    print(spec['scenario'],spec['backend'],spec['repeat'],spec['variant'],ms,flush=True)
summary=[]
for scenario in ['stage-near','stage-far','world']:
    for backend in ['raster','ray']:
        row={'scenario':scenario,'backend':backend}
        for variant in roots:
            r=[x for x in records if x['scenario']==scenario and x['backend']==backend and x['variant']==variant]
            row[variant]={'median_last_frame_ms':statistics.median(x['last_frame_ms'] for x in r),'last_frame_samples_ms':[x['last_frame_ms'] for x in r],'median_startup_inclusive_seconds':statistics.median(x['resources']['elapsed_seconds'] for x in r),'peak_rss_mib':max(x['resources']['peak_rss_kib']/1024 for x in r)}
        row['after_before_last_frame_ratio']=row['after']['median_last_frame_ms']/row['before']['median_last_frame_ms']; summary.append(row)
result={'status':'measured','cpu_workers':2,'source_difference':different,'manifests':manifest,'benchmark_sha256':sha(__file__),'summary':summary,'records':records,'limits':['Last frame only, not per-frame percentiles or a real-time guarantee.','Raster submission/presentation and ray TLAS/trace/resolve exclude main-loop skinning; startup-inclusive wall time includes setup.','Linux peak RSS units are KiB.','12 fps is offline simulation cadence, not achieved rendering fps.','No hardware-GPU or GTA VI parity claim.']}
(out/'benchmark.json').write_text(json.dumps(result,indent=2)+'\n')
