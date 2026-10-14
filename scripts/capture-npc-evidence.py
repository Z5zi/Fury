#!/usr/bin/env python3
"""Capture original world actors with the production executable, without retouching.

Requires Pillow for lossless PNG/layouts and ffmpeg only for --phase motion/all.
Each output case is new: never mixes new frames into an earlier sequence.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time
from PIL import Image, ImageDraw, ImageFont

ACTORS = [
    ('NpcCivA','Mira Vale / commuter',0,False),
    ('NpcCivB','Jon Keel / market',45,False),
    ('NpcCivC','Tessa Quill / commuter',90,True),
    ('NpcCivAsh','Nell Ash / market',45,False),
    ('NpcCivD','Pax Wren / work vest',0,False),
    ('NpcCivE','Rina Holt / work vest',0,False),
    ('NpcTeller','Lia Merrow / bank staff',45,False),
    ('NpcBankCust','Owen Pike / customer',0,False),
    ('NpcGuard','Sgt. Hale / security',0,False),
    ('NpcDeskGuard','Ofc. Renn / security',45,True),
    ('NpcAlleyHmpd','Ofc. Vale / alley officer',-90,False),
    ('NpcFence','Cass Vesper / broker',45,False),
    ('CrewRook','Rook / technical crew',0,False),
    ('CrewSparrow','Sparrow / scout crew',0,False),
    ('NpcExtraGuard','Metro Watch / reinforcement',0,False),
    ('NpcEnforcer','Syndicate Enforcer',90,False),
    ('PlayerBody','Third-person player',0,False),
    ('GhostLoop','Network avatar',0,False),
]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def font(size):
    path=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
    return ImageFont.truetype(str(path),size) if path.exists() else ImageFont.load_default()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vaultline',type=Path,required=True)
    parser.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--phase',choices=['atlas','portraits','motion','all'],default='all')
    parser.add_argument('--workers',type=int,choices=[1,2],default=1)
    args=parser.parse_args()
    args.vaultline=args.vaultline.resolve();args.source_root=args.source_root.resolve();args.output_dir=args.output_dir.resolve()
    args.output_dir.mkdir(parents=True,exist_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith('FURY_')}
    env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',FURY_AUDIO_BACKEND='null',
               FURY_CPU_THREADS='2',FURY_QUALITY='high',FURY_WORLD_ART='1',FURY_SURFACE_DETAIL='1',
               FURY_NET='embedded',FURY_TRACE_MODE='ray',OMP_NUM_THREADS='2',OPENBLAS_NUM_THREADS='2')
    records=[]
    def capture(name,actor,detail,orbit=0,motion=False,width=300,height=380,frames=2,ray=False,distance=3.2,talk=False,sequence=False):
        case=args.output_dir/name;case.mkdir() # intentional collision failure
        work=case/'work';work.mkdir();(work/'assets').symlink_to(args.source_root/'assets',target_is_directory=True)
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
            sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        command=[str(args.vaultline),'--cpu-ray' if ray else '--soft','--npc-motion' if motion else '--npc-view',actor,
                 '--npc-orbit',str(orbit),'--npc-distance',str(distance),'--frames',str(frames),'--capture-fps','12',
                 '--width',str(width),'--height',str(height),'--no-hud','--npc-audit',str(case/'audit.json')]
        if ray:command+=['--spp','2','--bounces','2']
        if talk:command+=['--npc-talk']
        if sequence:command+=['--capture-sequence',str(case/'frames')]
        else:command+=['--capture',str(case/'frame.ppm')]
        start=time.monotonic()
        with (case/'runtime.log').open('w') as log:
            subprocess.run(command,cwd=work,env=env|{'FURY_NPC_DETAIL':str(detail),'FURY_NET_PORT':str(port)},
                           stdout=log,stderr=subprocess.STDOUT,timeout=240,check=True)
        record={'case':name,'actor':actor,'detail':detail,'orbit':orbit,'motion':motion,'simulated_fps':12,
                'frames':frames,'width':width,'height':height,'ray':ray,'distance':distance,'talk_presentation':talk,
                'elapsed_seconds_including_startup_io':time.monotonic()-start,'command':command,
                'executable_sha256':sha(args.vaultline),'metrics':json.loads((case/'audit.json.render.json').read_text()),
                'profiles':json.loads((case/'audit.json').read_text()),'state':json.loads((case/'audit.json.state.json').read_text())}
        if sequence:
            record['frame_sha256']=[sha(p) for p in sorted((case/'frames').glob('*.ppm'))]
            subprocess.run(['ffmpeg','-y','-loglevel','error','-framerate','12','-i',str(case/'frames/frame_%06d.ppm'),
                            '-c:v','libx264','-crf','20','-pix_fmt','yuv420p',str(case/'motion.mp4')],check=True)
        else:
            Image.open(case/'frame.ppm').save(case/'frame.png')
            record['frame_sha256']=sha(case/'frame.ppm')
        (case/'record.json').write_text(json.dumps(record,indent=2)+'\n')
        return record
    if args.phase in ('atlas','all'):
        jobs=[(f'atlas-{id}-{detail}',id,detail,orbit,moving) for id,label,orbit,moving in ACTORS for detail in (0,1)]
        def run(job):
            name,id,detail,orbit,moving=job
            return capture(name,id,detail,orbit,moving,frames=48 if moving else 2)
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:records+=list(pool.map(run,jobs))
        for detail in (0,1):
            atlas=Image.new('RGB',(1800,1260),(17,23,32));draw=ImageDraw.Draw(atlas)
            for i,(id,label,orbit,moving) in enumerate(ACTORS):
                x=i%6*300;y=i//6*420
                atlas.paste(Image.open(args.output_dir/f'atlas-{id}-{detail}'/'frame.png'),(x,y+40))
                draw.text((x+7,y+3),label,font=font(13),fill='white')
                draw.text((x+7,y+21),'Live AI, t=4s' if moving else 'Original world position, frozen',font=font(10),fill=(172,188,208))
            atlas.save(args.output_dir/('population-after.png' if detail else 'population-before.png'))
    if args.phase in ('portraits','all'):
        cases=[('mira','NpcCivA',0,3.2),('mira-face','NpcCivA',0,1.05),('dock','NpcCivD',0,3.2),
               ('teller','NpcTeller',45,1.2),('rook','CrewRook',0,3.2),('sparrow','CrewSparrow',0,3.2),
               ('guard','NpcExtraGuard',0,3.2),('fence','NpcFence',45,1.2),('enforcer','NpcEnforcer',90,3.2)]
        jobs=[(name,id,orbit,distance,detail) for name,id,orbit,distance in cases for detail in (0,1)]
        def run(job):
            name,id,orbit,distance,detail=job
            return capture(f'portrait-{name}-{detail}',id,detail,orbit,width=480,height=600,ray=True,distance=distance)
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:records+=list(pool.map(run,jobs))
        for name,id,orbit,distance in cases:
            pair=Image.new('RGB',(960,640),(17,23,32));draw=ImageDraw.Draw(pair)
            for detail in (0,1):
                pair.paste(Image.open(args.output_dir/f'portrait-{name}-{detail}'/'frame.png'),(detail*480,40))
                draw.text((detail*480+12,12),'Articulated upgrade' if detail else 'Previous character presentation',font=font(18),fill='white')
            pair.save(args.output_dir/f'comparison-{name}.png')
    if args.phase in ('motion','all'):
        cases=[('mira','NpcCivA',0,False),('officer','NpcAlleyHmpd',-90,False),
               ('tessa','NpcCivC',90,False),('rook-talk','CrewRook',0,True)]
        for name,id,orbit,talk in cases:
            for detail in (0,1):records.append(capture(f'motion-{name}-{detail}',id,detail,orbit,True,480,360,48,talk=talk,sequence=True))
            sources=[args.output_dir/f'motion-{name}-{detail}'/'motion.mp4' for detail in (0,1)]
            filtergraph="[0:v]pad=iw:ih+48:0:48:color=0x111720,drawtext=text='Previous presentation':fontcolor=white:fontsize=18:x=12:y=8[a];[1:v]pad=iw:ih+48:0:48:color=0x111720,drawtext=text='Articulated upgrade':fontcolor=white:fontsize=18:x=12:y=8[b];[a][b]hstack=inputs=2,drawtext=text='Actual game frames - 12 fps simulated cadence (offline capture)':fontcolor=white:fontsize=13:x=12:y=30"
            if Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf').exists():
                filtergraph=filtergraph.replace('drawtext=text=', 'drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf:text=')
            subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(sources[0]),'-i',str(sources[1]),'-filter_complex',filtergraph,
                            '-c:v','libx264','-crf','20','-pix_fmt','yuv420p',str(args.output_dir/f'comparison-{name}.mp4')],check=True)
    # Presentation must never alter the real simulation in matched A/B runs.
    by_case={r['case']:r for r in records}
    for record in records:
        if record['detail']==0:
            partner=by_case[record['case'][:-1]+'1']
            if record['state']!=partner['state']:
                raise RuntimeError('A/B gameplay state mismatch: '+record['case'])
    (args.output_dir/f'manifest-{args.phase}.json').write_text(json.dumps({'executable_sha256':sha(args.vaultline),'records':records},indent=2)+'\n')
    print(f'Captured {len(records)} actual game runs in {args.output_dir}',flush=True)
if __name__=='__main__':main()
