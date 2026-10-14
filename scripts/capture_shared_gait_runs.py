"""Matched native neutral-stage sequences at the production crew follow speed."""
from pathlib import Path
import os,subprocess,json,hashlib
from PIL import Image
root=Path(__file__).resolve().parents[1];out=root/'docs/characters/shared_gait_pass02'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
binaries={'original':root.parent/'fury-baseline18/build/apps/characterlab/fury_characterlab','candidate':out/'frozen-zero-offset/programs/fury_characterlab'}
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='1',FURY_CPU_THREADS='1')
records=[];inputs=[]
for name,identity,role,height in [('Rook','CrewRook','CrewTech','1.70'),('Sparrow','CrewSparrow','CrewScout','1.72')]:
    for version,binary in binaries.items():
        folder=out/f'run-sequence-{name.lower()}-{version}';folder.mkdir(exist_ok=True)
        sequence=folder/'frames';report=folder/'report.json'
        command=[str(binary),'--id',identity,'--role',role,'--height',height,'--pose','run','--speed','6.5','--phase','.4375','--yaw','90','--width','540','--image-height','720','--frames','90','--fps','60','--capture-sequence',str(sequence),'--report',str(report)]
        with (folder/'run.log').open('w') as log:subprocess.run(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        frames=sorted(sequence.glob('frame_*.ppm'));assert len(frames)==90
        meta=json.loads(report.read_text());assert meta['frames_submitted']==90 and meta['speed_meters_per_second']==6.5
        for index in [0,15,30,45,60,75,89]:
            image=Image.open(frames[index]);image.save(folder/f'frame_{index+1:03}.png')
        records.append({'actor':name,'version':version,'binary_sha256':sha(binary),'command':command,'report':meta,'raw_frames':[{'name':p.name,'sha256':sha(p)} for p in frames]})
        inputs.extend(['-framerate','30','-start_number','1','-i',str(sequence/'frame_%06d.ppm')])
font='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf';filters=[]
for i,record in enumerate(records):
    title=record['actor']+' / '+('Original' if record['version']=='original' else 'Shared gait')
    filters.append(f"[{i}:v]pad=iw:ih+58:0:58:black,drawtext=fontfile={font}:text='{title}':fontcolor=white:fontsize=20:x=14:y=6,drawtext=fontfile={font}:text='6.5 m/s - neutral stage - half speed':fontcolor=white:fontsize=17:x=14:y=32[p{i}]")
filters.extend(['[p0][p1]hstack=inputs=2[a]','[p2][p3]hstack=inputs=2[b]','[a][b]concat=n=2:v=1:a=0[v]'])
video=out/'crew-ordinary-run-side-comparison.mp4'
command=['ffmpeg','-y','-hide_banner','-loglevel','error','-threads','1','-filter_complex_threads','1',*inputs,'-filter_complex',';'.join(filters),'-map','[v]','-an','-c:v','libx264','-preset','fast','-crf','18','-threads','1','-pix_fmt','yuv420p','-movflags','+faststart',str(video)]
subprocess.run(command,check=True)
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','stream=width,height,nb_frames,r_frame_rate','-show_entries','format=duration','-of','json',str(video)],text=True))
assert probe['streams'][0]['nb_frames']=='180'
manifest={'kind':'Native neutral-stage paired captures, not game-world frames','production_follow_speed':6.5,'sample_fps':60,'playback_fps':30,'playback':'half speed; no generated/interpolated frames','captures':records,'video':{'name':video.name,'sha256':sha(video),'bytes':video.stat().st_size,'probe':probe,'command':command}}
(out/'CREW_RUN_VIDEO_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'video':str(video),'bytes':video.stat().st_size,'frames':180,'duration':probe['format']['duration']}))
