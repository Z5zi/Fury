#!/usr/bin/env python3
"""Run a synthetic localhost peer and the real Vaultline NetClient/presentation path."""
import argparse,gzip,hashlib,json,os,socket,subprocess,time,resource
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser();p.add_argument('--exe',type=Path);p.add_argument('--fps',type=float,default=10);p.add_argument('--barrier',action='store_true');p.add_argument('--out',type=Path,required=True);p.add_argument('--frames',type=int,default=96);p.add_argument('--disconnect',type=int,default=89);p.add_argument('--reconnect',type=int,default=97);p.add_argument('--reconnect-id',type=int,default=2);p.add_argument('--silence-after',type=int,default=0);p.add_argument('--distance',type=float,default=3.2);p.add_argument('--orbit',type=float,default=0);p.add_argument('--no-sequence',action='store_true');a=p.parse_args();root=Path(__file__).resolve().parents[1];r=a.out.resolve();r.mkdir(parents=True,exist_ok=False);work=r/'fresh-save';work.mkdir();(work/'assets').symlink_to(root/'assets',target_is_directory=True)
env={k:v for k,v in os.environ.items() if not k.startswith('FURY_')};env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',FURY_CPU_THREADS='1',LP_NUM_THREADS='1',OMP_NUM_THREADS='1',OPENBLAS_NUM_THREADS='1',LIBGL_ALWAYS_SOFTWARE='1',FURY_QUALITY='high')
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
peer_cmd=[str(root/'build/tests/ghost_loopback_peer'),str(port),str(r/'sent.jsonl'),str(a.reconnect_id),str(a.silence_after),*([str(r/'peer-sent.txt')] if a.barrier else [])];peer_log=(r/'peer.log').open('w');peer=subprocess.Popen(peer_cmd,env=env,stdout=peer_log,stderr=subprocess.STDOUT)
try:
 deadline=time.monotonic()+5
 while 'ready' not in (r/'peer.log').read_text():
  if peer.poll() is not None:raise RuntimeError('peer failed')
  if time.monotonic()>deadline:raise RuntimeError('peer startup deadline')
  time.sleep(.01)
 cmd=[str(a.exe.resolve() if a.exe else root/'build/apps/vaultline/vaultline'),'--net=join','--net-host=127.0.0.1',f'--net-port={port}','--ghost-net-review',str(r/'received.jsonl'),'--width','640','--height','480','--frames',str(a.frames),'--capture-fps',str(a.fps),'--npc-distance',str(a.distance),'--npc-orbit',str(a.orbit),'--no-hud','--capture',str(r/'final.ppm'),'--npc-audit',str(r/'presentation.json')]
 if a.barrier:cmd+=['--ghost-net-peer-ack',str(r/'peer-sent.txt')]
 if a.disconnect:cmd+=['--ghost-net-disconnect',str(a.disconnect)]
 if a.reconnect:cmd+=['--ghost-net-reconnect',str(a.reconnect)]
 if not a.no_sequence:cmd+=['--capture-sequence',str(r/'frames')]
 before_cpu=resource.getrusage(resource.RUSAGE_CHILDREN)
 begin=time.monotonic()
 with (r/'app.log').open('w') as log:result=subprocess.run(cmd,cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT)
 elapsed=time.monotonic()-begin
 after_cpu=resource.getrusage(resource.RUSAGE_CHILDREN)
 app_cpu=after_cpu.ru_utime+after_cpu.ru_stime-before_cpu.ru_utime-before_cpu.ru_stime
 app_rss=after_cpu.ru_maxrss
 if result.returncode:raise RuntimeError(f'Vaultline returned {result.returncode}')
finally:
 peer.terminate();peer.wait(timeout=5);peer_log.close()
received=[json.loads(x) for x in (r/'received.jsonl').read_text().splitlines()];sent=[json.loads(x) for x in (r/'sent.jsonl').read_text().splitlines()];assert len(received)==a.frames
records=[];selected={1,8,12,20,28,36,42,48,56,64,68,72,76,84,88,89,92,96,97,98,104,112,120};tiles=[]
for path in sorted((r/'frames').glob('*.ppm')) if (r/'frames').exists() else []:
 number=int(path.stem.split('_')[-1]);raw=path.read_bytes();record={'frame':number,'file':path.name+'.gz','ppm_sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)}
 with gzip.open(str(path)+'.gz','wb',compresslevel=6) as f:f.write(raw)
 assert gzip.decompress(Path(str(path)+'.gz').read_bytes())==raw
 if number in selected:
  im=Image.open(path);png=r/f'frame-{number:03d}.png';im.save(png);record['png']=png.name;tiles.append((number,png))
 path.unlink();records.append(record)
Image.open(r/'final.ppm').save(r/'final.png');raw=(r/'final.ppm').read_bytes();(r/'final.ppm.gz').write_bytes(gzip.compress(raw));assert gzip.decompress((r/'final.ppm.gz').read_bytes())==raw;(r/'final.ppm').unlink()
w,h=320,260;sheet=Image.new('RGB',(w*4,h*((len(tiles)+3)//4)),'#20272a');draw=ImageDraw.Draw(sheet)
for i,(n,path) in enumerate(tiles):
 im=Image.open(path);im.thumbnail((w,h-20));x=i%4*w;y=i//4*h;sheet.paste(im,(x,y+20));d=received[n-1];draw.text((x+4,y+4),f'{n}: remotes={d["received_roster_size"]} connected={d["connected"]}',fill='white')
if tiles:sheet.save(r/'network-contact-sheet.png')
(r/'manifest.json').write_text(json.dumps({'identity':'GhostLoop','proof':'synthetic localhost UDP sent via production packet pack_state/write_header; received by real NetClient Join path then unchanged apps/vaultline/main.cpp remote-player block','direct_remote_transform_assignment':False,'external_server_or_user_data':False,'peer_command':peer_cmd,'app_command':cmd,'environment':{k:env[k] for k in ['SDL_VIDEODRIVER','FURY_CPU_THREADS','LP_NUM_THREADS','LIBGL_ALWAYS_SOFTWARE']},'frames':a.frames,'elapsed_wall_seconds':elapsed,'application_cpu_seconds':app_cpu,'application_peak_rss_kib':app_rss,'simulated_fps':a.fps,'peer_send_barrier':a.barrier,'timing_limit':('An opt-in one-second bounded filesystem rendezvous waits until the peer has queued its actual UDP snapshot before production NetClient.poll. This makes controlled art comparisons repeatable; it is not normal network timing. ' if a.barrier else '')+'Real asynchronous localhost UDP replies may be visible on a later simulation frame. Sent ticks and received frames are separate evidence; neither is relabeled as the other. Socket disconnection is a diagnostic invocation of the real disconnect method.','captures':records},indent=2)+'\n')
print(json.dumps({'received_frames':len(received),'sent_states':len(sent),'wall_seconds':elapsed,'empty_roster_frames':[d['frame'] for d in received if not d['received_roster_size']],'disconnected_frames':[d['frame'] for d in received if not d['connected']]},indent=2))
