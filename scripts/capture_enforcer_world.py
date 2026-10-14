#!/usr/bin/env python3
"""Retain actual-world Syndicate Enforcer before/after and live motion in untouched scenery."""
import argparse,json,os,socket,subprocess,tempfile,time,hashlib
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--vaultline',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items()if not k.startswith('FURY_')};env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1')
records=[]
for name,exe,orbit,motion in [('before-side',a.baseline,-90,False),('after-side',a.vaultline,-90,False),('after-rear',a.vaultline,-135,False),('after-front',a.vaultline,0,False),('after-motion',a.vaultline,-90,True)]:
 with tempfile.TemporaryDirectory(prefix='enforcer-world-',dir=out)as directory:
  work=Path(directory);(work/'assets').symlink_to(root/'assets',target_is_directory=True)
  with socket.socket(socket.AF_INET,socket.SOCK_DGRAM)as sock:sock.bind(('127.0.0.1',0));env['FURY_NET_PORT']=str(sock.getsockname()[1])
  cmd=[str(exe.resolve()),'--npc-motion'if motion else'--npc-view','NpcEnforcer','--npc-distance','2.5','--npc-orbit',str(orbit),'--width','800','--height','600','--frames','12'if motion else'2','--capture-fps','12','--no-hud','--capture',str(out/(name+'.ppm')),'--npc-audit',str(out/(name+'.json'))]
  if motion:cmd+=['--capture-sequence',str(out/'motion-frames')]
  start=time.monotonic();r=subprocess.run(cmd,cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);(out/(name+'.log')).write_text(r.stdout);assert r.returncode==0,r.stdout[-2000:]
  source=Image.open(out/(name+'.ppm'));source.save(out/(name+'.png'));assert source.tobytes()==Image.open(out/(name+'.png')).tobytes()
  profile=json.loads((out/(name+'.json')).read_text());actor=next(p for p in profile['profiles']if p['id']=='NpcEnforcer');assert actor['seed']==3140885188 and abs(actor['height']-1.95)<1e-6
  assert actor['individualized']==(name!='before-side')
  renderer=json.loads((out/(name+'.json.render.json')).read_text());assert renderer['validation_errors']==0
  records.append({'name':name,'command':cmd,'process_seconds':time.monotonic()-start,'actor':actor,'renderer':renderer,'image_sha256':hashlib.sha256((out/(name+'.ppm')).read_bytes()).hexdigest()})
  (out/(name+'.ppm')).unlink();print(name,actor['near_triangles'],actor['far_triangles'],flush=True)
# Every PPM is losslessly encoded as PNG, pixel-compared, then removed for storage.
frames=sorted((out/'motion-frames').glob('*.ppm'))
for i,p in enumerate(frames):
 im=Image.open(p);target=p.with_suffix('.png');im.save(target);assert im.tobytes()==Image.open(target).tobytes();p.unlink()
 if i in [0,len(frames)//2,len(frames)-1]:im.save(out/f'motion-sample-{i:03d}.png')
(out/'manifest.json').write_text(json.dumps({'kind':'actual-vaultline-world-captures','scenery_changed':False,'npc_repositioned':False,'notes':['Portrait and motion diagnostics call the real spawn lambda. They do not independently prove reset/respawn; use the separate selected-mission lifecycle report.','Scene fixtures/occlusion remain present.','OpenGL world renderer counters may be unavailable/zero; those values are not treated as GPU timing or zero geometry.'],'runs':records},indent=2)+'\n')
