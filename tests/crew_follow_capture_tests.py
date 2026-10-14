#!/usr/bin/env python3
"""Actual production crew/E/timer mission with frozen live-world review snapshots.

This controls the diagnostic player camera and calls the production update
closure. It does not claim a fully navigated SDL UI playthrough.
"""
import argparse, hashlib, json, math, os, socket, subprocess, tempfile, time
from pathlib import Path
from PIL import Image

def _link_dir(link, target):
    """Symlink a directory; on Windows without symlink privilege use a junction, then a copy."""
    import os as _os, shutil as _shutil, subprocess as _subprocess
    from pathlib import Path as _Path
    link, target = _Path(link), _Path(target)
    try:
        link.symlink_to(target, target_is_directory=True)
        return
    except OSError:
        if _os.name != 'nt':
            raise
    if _subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(target)],
                       capture_output=True).returncode == 0:
        return
    _shutil.copytree(target, link)

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--vaultline',type=Path,required=True)
p.add_argument('--source-root',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--actor',choices=['CrewRook','CrewSparrow'],default='CrewSparrow')
p.add_argument('--width',type=int,default=640)
p.add_argument('--height',type=int,default=480)
a=p.parse_args();output_root=a.output_dir.resolve();output_root.mkdir(parents=True,exist_ok=True)
# Keep earlier evidence intact while giving every invocation a fresh capture directory.
out=Path(tempfile.mkdtemp(prefix="run-",dir=output_root))
env={k:v for k,v in os.environ.items()if not k.startswith('FURY_')}
env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',OMP_NUM_THREADS='1',OPENBLAS_NUM_THREADS='1',FURY_NPC_DETAIL='1')
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM)as sock:
 sock.bind(('127.0.0.1',0));env['FURY_NET_PORT']=str(sock.getsockname()[1])
with tempfile.TemporaryDirectory(prefix='fresh-save-',dir=out)as directory:
 work=Path(directory);_link_dir((work/'assets'),a.source_root.resolve()/'assets')
 command=[str(a.vaultline.resolve()),'--soft','--width',str(a.width),'--height',str(a.height),'--crew-follow-audit',str(out/'follow.json'),'--crew-capture-actor',a.actor]
 start=time.monotonic();result=subprocess.run(command,cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
 (out/'follow.log').write_text(result.stdout)
 assert result.returncode==0,result.stdout[-4000:]
 d=json.loads((out/'follow.json').read_text());assert d['passed'] and d['actor']==a.actor
 assert d['production_ai_changed'] is False and d['actors_relocated_for_capture'] is False
 assert d['render_preserved_gameplay_state'] and d['cardinal_offsets_preserved'] and d['real_breach_loot_escape_success_reset']
 assert {s['state']['phase']for s in d['snapshots']} >= {'Idle','Approach','Breach','Looting','Escape','Success'}
 assert d['rendered_snapshots']==len(d['snapshots'])>=10 and len(d['source_sha256'])==64
 decoded=set();frames=[]
 for snapshot in d['snapshots']:
  assert snapshot['presentation_refresh_uses_live_state'] is True and snapshot['presentation_refresh_advances_time'] is False
  assert snapshot['validation_errors']==0 and snapshot['triangles']>10000 and snapshot['instances']>100
  assert math.isfinite(snapshot['cpu_frame_ms']) and snapshot['cpu_frame_ms']>=0
  for c in snapshot['state']['crew']:
   rook=c['id']=='CrewRook';assert c['id'] in ['CrewRook','CrewSparrow']
   assert abs(c['height']-(1.70 if rook else 1.72))<1e-6
   assert all(abs(a-b)<1e-6 for a,b in zip(c['offset'],[-1.8,0,-1.4]if rook else[1.8,0,-1.2]))
  raw=out/snapshot['frame'];png=raw.with_suffix('.png')
  im=Image.open(raw);assert im.size==(a.width,a.height);pixels=im.tobytes();im.save(png)
  assert Image.open(png).tobytes()==pixels
  digest=hashlib.sha256(pixels).hexdigest();decoded.add(digest)
  frames.append({'label':snapshot['label'],'path':str(png.relative_to(out)),'decoded_rgb_sha256':digest,'original_ppm_sha256':hashlib.sha256(raw.read_bytes()).hexdigest()})
  im.close();raw.unlink()
 assert len(decoded)>=8,'World captures did not change across real phases/positions'
 manifest={'command':command,'wall_seconds':time.monotonic()-start,'fresh_save_directory':True,'executable_sha256':hashlib.sha256(a.vaultline.read_bytes()).hexdigest(),'environment':{k:v for k,v in env.items()if k.startswith('FURY_')or k in ['SDL_VIDEODRIVER','SDL_AUDIODRIVER','LP_NUM_THREADS']},'frames':frames,'report_sha256':hashlib.sha256((out/'follow.json').read_bytes()).hexdigest()}
 (out/'invocation.json').write_text(json.dumps(manifest,indent=2)+'\n')
(output_root/'LATEST_RUN.json').write_text(json.dumps({'run_directory':out.name,'passed':True,'actor':a.actor},indent=2)+'\n')
print('PASS: production crew offsets, bounded follow travel, real E/timer heist, extraction/reset and actual world captures; diagnostic player camera, not a navigated UI playthrough')
