#!/usr/bin/env python3
"""Capture actual rare-spawn pursuit through the real lifecycle/Application loop."""
import argparse,hashlib,json,os,socket,subprocess,tempfile
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--vaultline',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items()if not k.startswith('FURY_')};env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1',FURY_NPC_DETAIL='1')
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM)as sock:sock.bind(('127.0.0.1',0));env['FURY_NET_PORT']=str(sock.getsockname()[1])
with tempfile.TemporaryDirectory(dir=out)as directory:
 work=Path(directory);(work/'assets').symlink_to(root/'assets',target_is_directory=True)
 cmd=[str(a.vaultline.resolve()),'--width','640','--height','480','--no-hud','--enforcer-lifecycle-audit',str(out/'lifecycle.json'),'--enforcer-lifecycle-capture',str(out/'frames')]
 result=subprocess.run(cmd,cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);(out/'run.log').write_text(result.stdout)
 (out/'invocation.json').write_text(json.dumps({'command':cmd,'binary_sha256':hashlib.sha256(a.vaultline.read_bytes()).hexdigest(),'exit_code':result.returncode,'fresh_save':True},indent=2)+'\n')
 assert result.returncode==0,result.stdout[-3500:]
frames=json.loads((out/'frames.json').read_text());assert len(frames['frames'])==8 and frames['validation_errors']==0
for f in frames['frames']:assert f['phase']=='Looting' and f['alive'] and f['chasing'] and 5.4<f['actual_speed']<5.601
files=sorted((out/'frames').glob('*.ppm'));assert len(files)==8
for file in files:
 im=Image.open(file);im.save(file.with_suffix('.png'));assert im.tobytes()==Image.open(file.with_suffix('.png')).tobytes();file.unlink()
print('PASS 8 actual Looting/pursuit frames and full 12-cycle lifecycle; source',frames['source_fingerprint'])
