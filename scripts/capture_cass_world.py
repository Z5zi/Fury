#!/usr/bin/env python3
"""Bounded Cass captures from the unmodified Vaultline world and live NPC controller."""
import argparse, hashlib, json, os, socket, subprocess, time
from pathlib import Path
from PIL import Image, ImageDraw

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vaultline',type=Path,required=True)
    parser.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--variant',choices=['accepted-seven-character-baseline','cass-individual'],required=True)
    parser.add_argument('--group',choices=['stills','motion','all'],default='all')
    parser.add_argument('--png-only',action='store_true',help='Preserve every framebuffer losslessly as PNG instead of redundant raw PPM')
    args=parser.parse_args(); root=args.output_dir.resolve();root.mkdir(parents=True,exist_ok=True)
    exe=args.vaultline.resolve();source=args.source_root.resolve();records=[]
    env={k:v for k,v in os.environ.items()if not k.startswith('FURY_')}
    env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',
               FURY_CPU_THREADS='1',LP_NUM_THREADS='1',OMP_NUM_THREADS='1',OPENBLAS_NUM_THREADS='1',
               LIBGL_ALWAYS_SOFTWARE='1',MESA_SHADER_CACHE_DIR='/tmp/cass-world-gl-cache',
               FURY_WORLD_ART='1',FURY_SURFACE_DETAIL='1',FURY_NPC_DETAIL='1',FURY_INDIVIDUAL_CHARACTERS='1',
               FURY_NET='embedded',FURY_QUALITY='high',LC_ALL='C')
    def capture(name,orbit,distance,frames=2,motion=False,talk=False,sequence=False,backend='gl',elevation=0):
        case=root/name;case.mkdir();work=case/'work';work.mkdir();(work/'assets').symlink_to(source/'assets',target_is_directory=True)
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM)as sock:
            sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        width,height=(480,640)if distance<1.5 else(480,360)
        command=[str(exe),'--npc-motion'if motion else'--npc-view','NpcFence','--npc-distance',str(distance),
                 '--npc-orbit',str(orbit),'--npc-elevation',str(elevation),'--frames',str(frames),'--capture-fps','12',
                 '--width',str(width),'--height',str(height),'--no-hud','--npc-audit',str(case/'audit.json')]
        if backend=='ray':command+=['--cpu-ray','--spp','2','--bounces','2']
        if talk:command+=['--npc-talk']
        command+=['--capture-sequence',str(case/'frames')]if sequence else['--capture',str(case/'frame.ppm')]
        started=time.monotonic()
        with(case/'runtime.log').open('w')as log:
            result=subprocess.run(command,cwd=work,env=env|{'FURY_NET_PORT':str(port)},stdout=log,stderr=subprocess.STDOUT,timeout=900)
        assert result.returncode==0,(name,result.returncode)
        audit=json.loads((case/'audit.json').read_text());state=json.loads((case/'audit.json.state.json').read_text());render=json.loads((case/'audit.json.render.json').read_text())
        profile=next(v for v in audit['profiles']if v['id']=='NpcFence')
        actor=next(v for v in state['agents']if v['id']=='NpcFence')
        assert render['validation_errors']==0 and profile['seed']==1362942633 and profile['role']=='fence'
        assert profile['individualized']==(args.variant=='cass-individual')
        assert abs(profile['height']-1.78)<1e-5 and actor['waypoints']==[[-84,0,46],[-88,0,50],[-82,0,50],[-86,0,45]]
        if backend=='gl':assert render['backend'].startswith('OpenGL'),render
        else:assert 'ray' in render['backend'].lower(),render
        record={'case':name,'capture_kind':'actual_Vaultline_world','variant':args.variant,'motion':motion,
                'talk_presentation_trigger':talk,'literal_dialogue_or_shop_transaction':False,'command':command,
                'executable_sha256':sha(exe),'seconds_including_startup_io':time.monotonic()-started,
                'simulated_fps':12,'frames_requested':frames,'profile':profile,'actor_state':actor,'render':render}
        if sequence:
            paths=sorted((case/'frames').glob('*.ppm'));assert len(paths)==frames
            record['frame_sha256']=[sha(p)for p in paths]
            for frame in sorted(set([0,frames//4,frames//2,3*frames//4,frames-1])):
                Image.open(paths[frame]).save(case/('frame-'+str(frame)+'.png'))
            subprocess.run(['ffmpeg','-y','-loglevel','error','-threads','1','-framerate','12','-i',str(case/'frames/frame_%06d.ppm'),
                            '-c:v','libx264','-threads','1','-crf','18','-pix_fmt','yuv420p',str(case/'motion.mp4')],check=True)
        else:
            Image.open(case/'frame.ppm').save(case/'frame.png');record['frame_sha256']=sha(case/'frame.ppm')
        if args.png_only:
            raw_paths=sorted((case/'frames').glob('*.ppm')) if sequence else [case/'frame.ppm']
            for raw in raw_paths:
                png=raw.with_suffix('.png')
                with Image.open(raw) as im:
                    pixels=im.tobytes(); shape=im.size; im.save(png)
                with Image.open(png) as verify: assert verify.size==shape and verify.tobytes()==pixels
                raw.unlink()
            record['retained_framebuffer_format']='lossless_png'
            record['all_decoded_pixels_equal_original_ppm']=True
            record['recorded_hash_format']='original_ppm'
        (case/'record.json').write_text(json.dumps(record,indent=2)+'\n');records.append(record)
        print(name,profile['near_triangles'],actor['position'],render['backend'],flush=True)
    if args.group in ['stills','all']:
        capture('shop-side',0,3.2)
        capture('shop-east',90,3.2)
        capture('shop-west',-90,3.2)
        capture('shop-portrait',45,1.15)
        capture('shop-distance',45,14.0)
        capture('shop-raised',45,2.4,elevation=25)
        capture('shop-back',180,2.4,elevation=15)
        capture('shop-front-close',0,1.65,elevation=10)
    if args.group in ['motion','all']:
        capture('live-route',90,3.2,frames=216,motion=True,sequence=True)
        capture('live-attention',0,1.15,frames=48,motion=True,talk=True,sequence=True)
    (root/('manifest-'+args.group+'.json')).write_text(json.dumps({'capture_kind':'actual_Vaultline_world',
        'route_scenery_controller_changes':False,'variant':args.variant,'records':records},indent=2)+'\n')
if __name__=='__main__':main()
