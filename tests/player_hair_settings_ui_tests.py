import os, pathlib, subprocess, sys, json, shutil, math
from PIL import Image

assets=pathlib.Path(__file__).resolve().parents[1]/'assets'
def link_assets(work):
    target=work/'assets'
    if target.exists():
        assert target.resolve()==assets.resolve(),('Unexpected fixture assets',target)
        return
    try:target.symlink_to(assets,target_is_directory=True)
    except OSError:
        def link_or_copy(source,destination):
            try:return os.link(source,destination)
            except OSError:return shutil.copy2(source,destination)
        shutil.copytree(assets,target,copy_function=link_or_copy)
ui_binary=pathlib.Path(sys.argv[1]).resolve(); production=pathlib.Path(sys.argv[2]).resolve()
root=pathlib.Path(sys.argv[3]).resolve();root.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
for key in ['FURY_PLAYER_SOURCE_HAIR','FURY_PLAYER_SHORT_TAIL','FURY_PLAYER_HAIR_DETAIL','FURY_QUALITY','FURY_SETTINGS_TEST_PAD']:
    env.pop(key,None)
env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='1',FURY_CPU_THREADS='1')
records=[]
for name,override in [('keyboard',{}),('controller',{'FURY_SETTINGS_TEST_PAD':'1'}),
                      ('hair-env',{'FURY_PLAYER_SOURCE_HAIR':'0'}),('quality-env',{'FURY_QUALITY':'high'})]:
    work=root/name;work.mkdir(exist_ok=True);link_assets(work)
    (work/'vaultline_settings.json').write_text(json.dumps({'quality':1,'player_hair':0,'future_ui':{'value':'preserve, me'}}))
    selected=env|override
    proc=subprocess.run([str(ui_binary),'--soft','--frames','51','--width','640','--height','480'],cwd=work,env=selected,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (work/'run.log').write_text(proc.stdout)
    assert proc.returncode==0,(name,proc.returncode,proc.stdout[-5000:])
    assert 'Texture loaded: assets/textures/asphalt.png' in proc.stdout and 'WARNING missing Meridian Mutual KIT' not in proc.stdout,'Fixture must load complete real-world assets'
    lines=[json.loads(line) for line in (work/'ui-trace.jsonl').read_text().splitlines()]
    assert len(lines)==51
    data=json.loads((work/'vaultline_settings.json').read_text());assert data['player_hair']==0 and data['quality']==1 and data['future_ui']=={'value':'preserve, me'},data
    for ppm in work.glob('ui-*.ppm'):
        image=Image.open(ppm);rgb=image.tobytes()
        frame=int(ppm.stem.split('-')[1]);width,height=image.size
        layout=json.loads((work/f'ui-{frame}-layout.json').read_text())
        footer_top=math.floor(layout['footer_y'])
        footer_bottom=math.ceil(layout['footer_y']+layout['footer_glyph_height'])
        assert 0 <= footer_top < footer_bottom <= height,(name,frame,layout)
        gold=(235,215,160)
        active_pixels=sum(image.getpixel((x,y))==gold for y in range(footer_top,footer_bottom) for x in range(width//2-270,width//2-140))
        assert active_pixels>30,(name,frame,'active status is not visible')
        notice_pixels=sum(image.getpixel((x,y))==gold for y in range(footer_top,footer_bottom) for x in range(width//2-80,width//2-20))
        expected=lines[frame]['pending'] or lines[frame]['environment_override']
        assert (notice_pixels>10)==bool(expected),(name,frame,'restart/override notice does not match actual UI state')
        png=ppm.with_suffix('.png');image.save(png);assert Image.open(png).tobytes()==rgb;ppm.unlink()
    # A fresh actual product process must resolve the saved requested preference.
    restart=work/'restart';restart.mkdir(exist_ok=True);link_assets(restart)
    shutil.copyfile(work/'saved-original.json',restart/'vaultline_settings.json')
    p=subprocess.run([str(production),'--soft','--frames','1','--width','160','--height','120'],cwd=restart,env=selected,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (restart/'run.log').write_text(p.stdout);assert p.returncode==0 and 'Player Hair requested ORIGINAL | active ORIGINAL' in p.stdout,p.stdout[-3000:]
    assert json.loads((restart/'vaultline_settings.json').read_text())['player_hair']==1
    records.append({'scenario':name,'frames':len(lines),'fresh_original_restart':True,'simulated_sdl_keyboard':True,'sdl_virtual_gamepad':name=='controller','physical_device_test':False,'renderer':'Software','resource_identity_unchanged':all(row['resource_identity_unchanged'] for row in lines)})
    print(name,'production UI + restart passed',flush=True)
(root/'SUMMARY.json').write_text(json.dumps(records,indent=2))
