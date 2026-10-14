"""Native per-backend equality of menu policy and explicit legacy hair bundles."""
import os,pathlib,subprocess,sys,json,hashlib,shutil
from PIL import Image

assets=pathlib.Path(__file__).resolve().parents[1]/'assets'
def link_assets(work):
    target=work/'assets'
    if target.exists():
        if target.resolve()==assets.resolve():return
        # A previous run's copy fallback (no symlink privilege) is stale: rebuild it.
        assert target.is_dir() and not target.is_symlink(),('Unexpected fixture assets',target)
        shutil.rmtree(target)
    try:target.symlink_to(assets,target_is_directory=True)
    except OSError:
        def link_or_copy(source,destination):
            try:return os.link(source,destination)
            except OSError:return shutil.copy2(source,destination)
        shutil.copytree(assets,target,copy_function=link_or_copy)
binary=pathlib.Path(sys.argv[1]).resolve();root=pathlib.Path(sys.argv[2]).resolve();root.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
for key in ['FURY_PLAYER_SOURCE_HAIR','FURY_PLAYER_SHORT_TAIL','FURY_PLAYER_HAIR_DETAIL','FURY_QUALITY','FURY_RENDERER','FURY_SOFT']:env.pop(key,None)
env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='1',FURY_CPU_THREADS='1')
if os.name=='nt':env.pop('SDL_VIDEODRIVER',None)  # Windows SDL lacks 'offscreen'; use the default driver
results=[]
for backend,args,marker in [('software',['--soft'],'CPU software rasterizer'),('cpu-ray',['--cpu-ray','--spp','1','--bounces','1'],'CPU BVH ray/path tracer'),('gl',[],'OpenGL')]:
  for name,quality,preference,on in [('auto-med',1,0,True),('balanced-low',0,2,True),('original-high',2,1,False),('auto-low',0,0,False)]:
    pixels=[];records=[]
    for explicit in [False,True]:
      work=root/(backend+'-'+name+('-explicit' if explicit else '-menu'));work.mkdir(exist_ok=True);link_assets(work)
      (work/'vaultline_settings.json').write_text(json.dumps({'quality':quality,'player_hair':preference}))
      selected=env.copy()
      if explicit:selected.update(FURY_PLAYER_SOURCE_HAIR='1' if on else '0',FURY_PLAYER_SHORT_TAIL='1' if on else '0',FURY_PLAYER_HAIR_DETAIL='512' if on else '0')
      command=[str(binary),*args,'--npc-view','PlayerBody','--npc-distance','1.0','--npc-orbit','110','--frames','1','--no-hud','--width','320','--height','240','--capture',str(work/'frame.ppm')]
      proc=subprocess.run(command,cwd=work,env=selected,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
      (work/'run.log').write_text(proc.stdout);assert proc.returncode==0,(backend,name,proc.stdout[-3000:])
      assert 'Texture loaded: assets/textures/asphalt.png' in proc.stdout and 'WARNING missing Meridian Mutual KIT' not in proc.stdout,'Fixture must load complete real-world assets'
      active=next(line for line in proc.stdout.splitlines() if 'Active renderer:' in line)
      assert marker.lower() in active.lower(),active
      assert ('active BALANCED' if on else 'active ORIGINAL') in proc.stdout,proc.stdout[:3000]
      image=Image.open(work/'frame.ppm');data=image.tobytes();pixels.append(data)
      image.save(work/'frame.png');assert Image.open(work/'frame.png').tobytes()==data;(work/'frame.ppm').unlink()
      records.append({'explicit':explicit,'renderer':active,'pixel_sha256':hashlib.sha256(data).hexdigest(),'command':command})
    assert pixels[0]==pixels[1],(backend,name,'menu pixels differ from explicit legacy bundle')
    results.append({'backend':backend,'case':name,'exact_pixel_match':True,'runs':records});print(backend,name,'exact pixels',flush=True)
# Real GL-window failure -> software fallback must be identified before freeze.
work=root/'fallback';work.mkdir(exist_ok=True);link_assets(work)
(work/'vaultline_settings.json').write_text('{"quality":1,"player_hair":0}')
p=subprocess.run([str(binary),'--frames','1','--width','160','--height','120'],cwd=work,env=env|{'SDL_VIDEODRIVER':'dummy'},text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(work/'run.log').write_text(p.stdout)
assert p.returncode==0 and 'OpenGL window failed; retrying without GL' in p.stdout and 'Player Hair requested AUTO | active BALANCED' in p.stdout,p.stdout[:4000]
assert p.stdout.index('Active renderer: CPU software rasterizer')<p.stdout.index('Player Hair requested AUTO | active BALANCED')
results.append({'case':'actual-gl-window-software-fallback','resolved_after_actual_backend':True})
(root/'SUMMARY.json').write_text(json.dumps(results,indent=2))
