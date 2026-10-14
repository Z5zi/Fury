#!/usr/bin/env python3
"""Final normal-off and unanswered-localhost startup controls, fresh saves."""
from pathlib import Path
import subprocess,socket,os,json,hashlib,gzip
from PIL import Image
root=Path(__file__).resolve().parents[1];out=root/'ghost-evidence/final-inactive';out.mkdir(exist_ok=False)
env={k:v for k,v in os.environ.items() if not k.startswith('FURY_')};env.update(SDL_VIDEODRIVER='offscreen',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='1',FURY_CPU_THREADS='1',FURY_AUDIO_BACKEND='null',SDL_AUDIODRIVER='dummy');records=[]
for mode in ['off','unanswered-join']:
 runs=[]
 for model in ['original','current']:
  r=out/(mode+'-'+model);r.mkdir();work=r/'fresh-save';work.mkdir();(work/'assets').symlink_to(root/'assets',target_is_directory=True);exe=root/('ghost-evidence/final-model-only-baseline/vaultline' if model=='original' else 'build/apps/vaultline/vaultline')
  with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
   reservation.bind(('127.0.0.1',0));cmd=[str(exe),'--frames','8','--capture-fps','10','--width','480','--height','360','--no-hud','--capture',str(r/'final.ppm'),'--npc-audit',str(r/'presentation.json')]
   if mode=='unanswered-join':cmd+=['--net=join','--net-host=127.0.0.1',f'--net-port={reservation.getsockname()[1]}','--ghost-net-review',str(r/'received.jsonl')]
   else:cmd+=['--net=off']
   with (r/'app.log').open('w') as f:subprocess.run(cmd,cwd=work,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
  raw=(r/'final.ppm').read_bytes();Image.open(r/'final.ppm').save(r/'final.png');(r/'final.ppm.gz').write_bytes(gzip.compress(raw));assert gzip.decompress((r/'final.ppm.gz').read_bytes())==raw;(r/'final.ppm').unlink();record={'model':model,'command':cmd,'ppm_sha256':hashlib.sha256(raw).hexdigest()}
  if mode=='unanswered-join':
   rows=[json.loads(s) for s in (r/'received.jsonl').read_text().splitlines()];assert all(not s['visible'] and not s['received'] for s in rows);assert len({(s['near_revision'],s['far_revision'],s['near_mesh_identity'],s['far_mesh_identity'],s['scene_mesh_count']) for s in rows})==1;record['source_fingerprint']=rows[0]['source_fingerprint'];record['PlayerBody']=[s['PlayerBody'] for s in rows]
  runs.append(record)
 assert runs[0]['ppm_sha256']==runs[1]['ppm_sha256'];
 if mode=='unanswered-join':assert runs[0]['PlayerBody']==runs[1]['PlayerBody']
 records.append({'mode':mode,'identical_whole_frame_pixels':True,'runs':runs})
(out/'RESULT.json').write_text(json.dumps({'status':'passed','runs':records,'notes':'Off is normal startup without a diagnostic follow camera. Unanswered join uses a reserved localhost socket that never responds; connected() represents an open socket, not an accepted peer handshake. Ghost stays hidden and cached poses remain unchanged.'},indent=2)+'\n')
