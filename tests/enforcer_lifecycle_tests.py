#!/usr/bin/env python3
"""Run the real selected-job/complication lifecycle in a fresh local save directory."""
import argparse, hashlib, json, os, shutil, socket, subprocess, tempfile, time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--vaultline',type=Path,required=True)
p.add_argument('--source-root',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('FURY_')}
env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',OMP_NUM_THREADS='1',OPENBLAS_NUM_THREADS='1',FURY_NPC_DETAIL='1')
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM)as sock:
 sock.bind(('127.0.0.1',0));env['FURY_NET_PORT']=str(sock.getsockname()[1])
with tempfile.TemporaryDirectory(prefix='fresh-save-',dir=out)as directory:
 work=Path(directory);(work/'assets').symlink_to(a.source_root.resolve()/'assets',target_is_directory=True)
 command=[str(a.vaultline.resolve()),'--soft','--width','64','--height','64','--enforcer-lifecycle-audit',str(out/'lifecycle.json')]
 start=time.monotonic();r=subprocess.run(command,cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
 (out/'lifecycle.log').write_text(r.stdout)
 assert r.returncode==0,r.stdout[-2500:]
 d=json.loads((out/'lifecycle.json').read_text());assert d['passed'] and len(d['cycles'])==12
 progress=out/'production-progress-save';progress.mkdir(exist_ok=True)
 for file in work.glob('vaultline_session_slot*.json'):shutil.copy2(file,progress/file.name)
 assert (progress/'vaultline_session_slot0.json').is_file(),'Real progression did not persist its ordinary save'
 assert len(d['source_sha256'])==64 and d['production_ai_changed'] is False
 assert {c['mission_index']for c in d['cycles']}==set(range(6))
 assert sum(c['eligible']for c in d['cycles'])==4
 for c in d['cycles']:
  assert c['eligible']==(c['payout_tier']>=3)
  assert c['diagnostic_escape_speed']==9.0
  assert c['reset_to_idle'] is True
  if not c['eligible']:
   assert c['spawn'] is None and c['rare_actor_absent_by_tier'] is True
   assert c['cached_meshes_reused'] is None and c['duplicate_spawn_idempotent'] is None
   continue
  assert c['spawn'][0]==c['vault'][0]-4.5 and c['spawn'][2]==c['vault'][2]+3.5
  assert abs(c['spawn'][1]-.975)<1e-6 and 5.4<c['peak_actual_speed']<=5.601 and c['travel_distance']>1
  for k in ['cached_meshes_reused','material_reused','duplicate_spawn_idempotent','agent_removed','entity_hidden','reset_to_idle','pose_reset_within_contract','above_tolerance_control_rejected','locomotion_reset']:assert c[k],(c,k)
  assert c['maximum_pose_reset_error_m']<=c['pose_reset_tolerance_m']==2e-6
 assert {c['cleanup_phase']for c in d['cycles']}=={'Failed','Escape'}
 (out/'invocation.json').write_text(json.dumps({'command':command,'wall_seconds':time.monotonic()-start,'fresh_save_directory':True,'environment':{k:v for k,v in env.items()if k.startswith('FURY_')or k in ['SDL_VIDEODRIVER','SDL_AUDIODRIVER','LP_NUM_THREADS']},'report_sha256':hashlib.sha256((out/'lifecycle.json').read_bytes()).hexdigest()},indent=2)+'\n')
print('PASS: 12 actual selected-mission cycles; 4 eligible Enforcer spawn/pursuit/cleanup/reset cycles and 8 correct lower-tier absences; unchanged 2e-6 m pose reset contract')
