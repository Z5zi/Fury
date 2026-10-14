"""Package native zero-offset gameplay captures; never generates scene pixels."""
from pathlib import Path
import hashlib,json,os,subprocess
from PIL import Image

root=Path(__file__).resolve().parents[1]
out=root/'docs/characters/shared_gait_pass02'
baseline=out/'world-baseline-sequence';candidate=out/'world-candidate'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
a=json.loads((baseline/'audit.json').read_text());b=json.loads((candidate/'audit.json').read_text())
assert len(a['frames'])==len(b['frames'])==300
fields=[k for k in a['frames'][0] if k!='animation']
assert all(all(x[k]==y[k] for k in fields) for x,y in zip(a['frames'],b['frames']))
assert a['invariant_failures']==b['invariant_failures']==0
assert a['renderer']==b['renderer']
manifest={'kind':'native OpenGL gameplay A/B; no actor relocation, generated pixels or interpolation','baseline_source':a['source_fingerprint'],'candidate_source':b['source_fingerprint'],'source_frames':300,'simulation_fps':30,'non_animation_fields_exactly_equal':fields,'renderer':a['renderer'],'frames':{}}
selected={0,1,44,59,65,74,85,90,99,110,115,118,123,124,128,130,134,135,140,142,154,155,170,174,180,195,210,225,259,299}
for folder in [baseline,candidate]:
    frames=sorted(folder.glob('frame_*.ppm'));assert len(frames)==300
    manifest['frames'][folder.name]=[{'name':p.name,'sha256':sha(p)} for p in frames]
    for p in frames:
        if int(p.stem.split('_')[1]) in selected:
            with Image.open(p) as image:image.save(p.with_suffix('.png'))
            with Image.open(p) as original,Image.open(p.with_suffix('.png')) as png:assert original.tobytes()==png.tobytes()
font='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
left='Original 18-character baseline  d28ca90f'
right='Shared gait candidate  6eca8e7e'
filters=f"[0:v]pad=iw:ih+40:0:40:black,drawtext=fontfile={font}:text='{left}':fontcolor=white:fontsize=21:x=18:y=9[l];[1:v]pad=iw:ih+40:0:40:black,drawtext=fontfile={font}:text='{right}':fontcolor=white:fontsize=21:x=18:y=9[r];[l][r]hstack=inputs=2[v]"
video=out/'player-native-gameplay-comparison.mp4'
command=['ffmpeg','-y','-hide_banner','-loglevel','error','-threads','1','-filter_complex_threads','1','-framerate','30','-i',str(baseline/'frame_%03d.ppm'),'-framerate','30','-i',str(candidate/'frame_%03d.ppm'),'-filter_complex',filters,'-map','[v]','-an','-c:v','libx264','-preset','fast','-crf','18','-threads','1','-pix_fmt','yuv420p','-movflags','+faststart',str(video)]
subprocess.run(command,check=True)
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','stream=width,height,nb_frames,r_frame_rate','-show_entries','format=duration','-of','json',str(video)],text=True))
assert probe['streams'][0]['nb_frames']=='300'
manifest['video']={'path':video.name,'sha256':sha(video),'bytes':video.stat().st_size,'probe':probe,'command':command,'encoding':'H.264 CRF18; selected original PNG frames retained losslessly'}
(out/'PLAYER_NATIVE_VIDEO_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'video':str(video),'bytes':video.stat().st_size,'source_fields_exact':len(fields),'frames':300}))
