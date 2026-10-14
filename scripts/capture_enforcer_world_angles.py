#!/usr/bin/env python3
"""Observe unchanged Enforcer spawn from viable camera angles; never move actor/scenery."""
import argparse,json,os,socket,subprocess,tempfile,shutil,hashlib
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--vaultline',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--progress-save',type=Path);p.add_argument('--orbits',default='30,60,90,135,180');p.add_argument('--distance',default='2.5');p.add_argument('--motion',action='store_true');a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items()if not k.startswith('FURY_')};env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1')
records=[]
for yaw in a.orbits.split(','):
 name=f'orbit-{yaw}-distance-{a.distance}'+('-motion'if a.motion else'')
 with tempfile.TemporaryDirectory(dir=out)as directory:
  work=Path(directory);(work/'assets').symlink_to(root/'assets',target_is_directory=True)
  if a.progress_save:shutil.copy2(a.progress_save,work/'vaultline_session_slot0.json')
  with socket.socket(socket.AF_INET,socket.SOCK_DGRAM)as sock:sock.bind(('127.0.0.1',0));env['FURY_NET_PORT']=str(sock.getsockname()[1])
  cmd=[str(a.vaultline.resolve()),'--npc-motion'if a.motion else'--npc-view','NpcEnforcer','--npc-distance',a.distance,'--npc-orbit',yaw,'--width','640','--height','480','--frames','16'if a.motion else'2','--capture-fps','12','--no-hud','--capture',str(out/(name+'.ppm')),'--npc-audit',str(out/(name+'.json'))]
  if a.motion:cmd+=['--capture-sequence',str(out/(name+'-frames'))]
  result=subprocess.run(cmd,cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);(out/(name+'.log')).write_text(result.stdout);assert result.returncode==0,result.stdout[-2000:]
  for file in [out/(name+'.ppm'),*sorted((out/(name+'-frames')).glob('*.ppm'))]:
   im=Image.open(file);im.save(file.with_suffix('.png'));assert im.tobytes()==Image.open(file.with_suffix('.png')).tobytes();file.unlink()
  state=json.loads((out/(name+'.json.state.json')).read_text());actor=next(x for x in state['agents']if x['id']=='NpcEnforcer')
  render=json.loads((out/(name+'.json.render.json')).read_text());assert render['validation_errors']==0
  records.append({'name':name,'command':cmd,'actor_state':actor,'camera':render['camera'],'source_fingerprint':render['source_fingerprint'],'progress_save_sha256':hashlib.sha256(a.progress_save.read_bytes()).hexdigest()if a.progress_save else None})
  print(name,actor['position'],actor['actual_speed'],flush=True)
(out/'manifest.json').write_text(json.dumps({'kind':'actual-world-canonical-spawn-camera-survey','actor_or_scenery_repositioned':False,'progress_save_origin':'unchanged ordinary production lifecycle'if a.progress_save else'fresh default','rare_transition_proof':'separate lifecycle report; these cameras use diagnostic spawn entry','records':records},indent=2)+'\n')
