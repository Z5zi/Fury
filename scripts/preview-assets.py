#!/usr/bin/env python3
"""Capture every shipped GLB/OBJ through the production CPU renderer and label sheets.

Requires a built fury_renderlab with --asset-only/--obj support and Pillow.
The photographs are production framebuffer captures; sheets only arrange/label them.
No optional assets are fetched. At most two processes/two CPU threads each; each
process times out at 60 seconds and uses a 4 GiB virtual-memory ceiling on POSIX.
"""
import argparse
import concurrent.futures
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import struct
import time

from PIL import Image, ImageDraw, ImageFont, ImageStat
if os.name == 'posix':
    import resource


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def memory_limit():
    resource.setrlimit(resource.RLIMIT_AS, (4*1024**3, 4*1024**3))


def make_sheets(records, destination, width, height):
    try:
        font = ImageFont.truetype('DejaVuSans.ttf', 12)
        title_font = ImageFont.truetype('DejaVuSans-Bold.ttf', 17)
    except OSError:
        font = title_font = ImageFont.load_default()
    sheets = []
    for kind in ('glb', 'obj'):
        group = [x for x in records if x['kind'] == kind]
        for page in range(math.ceil(len(group)/24)):
            subset = group[page*24:(page+1)*24]
            cols, rows, padding = 4, math.ceil(len(subset)/4), 6
            cell_width, cell_height = width+padding*2, height+48
            sheet = Image.new('RGB', (cols*cell_width, rows*cell_height+64), (20, 25, 32))
            draw = ImageDraw.Draw(sheet)
            title = f'{kind.upper()} runtime previews | page {page+1} | CPU BVH | 320x180 | 8 spp'
            draw.text((padding, 8), title, font=title_font, fill=(240, 244, 248))
            if subset and subset[0].get('gameplay_helper_filter'):
                title = 'Civilian closeups | exact gameplay helper filter | original geometry retained'
                draw.rectangle((0,0,sheet.width,32),fill=(20,25,32))
                draw.text((padding,8),title,font=title_font,fill=(240,244,248))
            subtitle = 'Authored core PBR factors; unsupported optional extensions fall back' if kind == 'glb' else 'Geometry-only OBJ import; neutral gray material; MTL intentionally unsupported'
            draw.text((padding, 33), subtitle, font=font, fill=(188, 199, 214))
            for i, item in enumerate(subset):
                x, y = (i%cols)*cell_width+padding, (i//cols)*cell_height+64
                if item['status'] == 'passed':
                    with Image.open(destination/item['image']) as source:
                        sheet.paste(source.convert('RGB'), (x, y))
                else:
                    draw.rectangle((x,y,x+width,y+height), fill=(88,28,28))
                    draw.text((x+8,y+height//2), 'CAPTURE FAILED: '+item['status'], font=font, fill='white')
                draw.text((x,y+height+4), Path(item['asset']).name, font=font, fill=(234,240,247))
                details = f"{item.get('triangles', 0):,} tris | {item.get('elapsed_seconds', 0):.2f}s"
                draw.text((x,y+height+22), details, font=font, fill=(158,177,198))
            path = destination/f'{kind}-contact-sheet-{page+1:02}.png'
            sheet.save(path)
            sheets.append(path.name)
    return sheets



def make_texture_sheet(root, destination):
    files = sorted(p for p in (root/'assets/textures').iterdir() if p.suffix in ('.png','.ppm'))
    sheet = Image.new('RGB',(1050,422),(20,25,32))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype('DejaVuSans.ttf',13)
    except OSError:
        font = ImageFont.load_default()
    draw.text((10,10),'Runtime textures: original 64x64 pixels at 2x nearest-neighbor scale',font=font,fill='white')
    equivalents = {}
    for i,path in enumerate(files):
        with Image.open(path) as raw:
            image = raw.convert('RGB')
            x,y = (i%5)*210+10,(i//5)*190+42
            sheet.paste(image.resize((128,128),Image.Resampling.NEAREST),(x,y))
            draw.text((x,y+135),path.name,font=font,fill='white')
            if path.suffix == '.png':
                with Image.open(path.with_suffix('.ppm')) as sibling:
                    equivalents[path.stem] = image.tobytes() == sibling.convert('RGB').tobytes()
    sheet.save(destination/'texture-contact-sheet.png')
    return {'contact_sheet':'texture-contact-sheet.png','png_ppm_pixel_equivalence':equivalents}



def filtered_civilian_wrapper(source, destination):
    # Match apps/vaultline/harbor_assets.cpp::is_helper_prim exactly. Removing
    # only mesh bindings preserves node transforms/children and binary geometry.
    filters = ('ground_walk','ground_curb','Shadow','SaltRing','xmem','StreetWalk')
    raw = source.read_bytes()
    if struct.unpack_from('<III',raw) != (0x46546c67,2,len(raw)):
        raise ValueError('Invalid source GLB')
    cursor,chunks,hidden = 12,[],[]
    while cursor < len(raw):
        size,kind = struct.unpack_from('<II',raw,cursor)
        data = raw[cursor+8:cursor+8+size]
        if kind == 0x4e4f534a:
            doc = json.loads(data)
            for node in doc.get('nodes',[]):
                if 'mesh' in node and any(f in node.get('name','') for f in filters):
                    hidden.append(node.get('name',''))
                    del node['mesh']
            data = json.dumps(doc,separators=(',',':')).encode()
            data += b' '*(-len(data)%4)
        chunks.append(struct.pack('<II',len(data),kind)+data)
        cursor += 8+size
    body = b''.join(chunks)
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(struct.pack('<III',0x46546c67,2,12+len(body))+body)
    return {'filter':'Harbor is_helper_prim exact substring filter; mesh bindings only',
            'substrings':list(filters),'hidden_helper_nodes':hidden,
            'wrapper_sha256':digest(destination),'original_sha256':digest(source)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--renderer', type=Path, default=Path('build/apps/renderlab/fury_renderlab'))
    parser.add_argument('--output', type=Path, default=Path('out/cpu-validation/asset-previews'))
    parser.add_argument('--jobs', type=int, choices=(1,2), default=2)
    parser.add_argument('--match', default='', help='Optional filename substring for bounded re-checks')
    parser.add_argument('--yaw', type=float, default=0, help='Asset-camera orbit in degrees')
    parser.add_argument('--sheets-only', action='store_true')
    parser.add_argument('--civilian-close', action='store_true', help='Six GLB LOD0/1 closeups with exact gameplay helper filter')
    args = parser.parse_args()
    root = args.root.resolve()
    destination = (root/args.output).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    if args.sheets_only:
        report = json.loads((destination/'index.json').read_text())
        report['contact_sheets'] = make_sheets(report['captures'], destination, 320, 180)
        (destination/'index.json').write_text(json.dumps(report, indent=2, sort_keys=True)+'\n')
        return 0
    renderer = (root/args.renderer).resolve()
    if not renderer.is_file():
        raise SystemExit('Renderer not found: '+str(renderer))
    renderer_sha = digest(renderer)
    assets = sorted(p for p in (root/'assets/meshes').rglob('*')
                    if p.suffix.lower() in ('.glb','.obj') and args.match in p.name)
    if args.civilian_close:
        assets = [p for p in assets if p.suffix=='.glb' and p.name.startswith('hm_civ_')]
    if not assets:
        raise SystemExit('No matching shipped assets')
    if not math.isfinite(args.yaw):
        raise SystemExit('Yaw must be finite')
    width, height = 320, 180
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', FURY_CPU_THREADS='2')
    started = time.monotonic()
    yaw = args.yaw
    civilian_close = args.civilian_close
    def capture(asset):
        kind = asset.suffix[1:]
        preview_input = asset
        filter_record = None
        if civilian_close:
            preview_input = destination/'filtered-inputs'/asset.name
            filter_record = filtered_civilian_wrapper(asset,preview_input)
        output = destination/kind/asset.stem
        output.parent.mkdir(parents=True, exist_ok=True)
        ppm, png, metrics, logfile = [output.with_suffix(suffix) for suffix in ('.ppm','.png','.json','.log')]
        args = [str(renderer), '--backend', 'cpu-ray', '--mode', 'path', '--asset-only',
                '--obj' if kind=='obj' else '--gltf', str(preview_input),
                '--width', str(width), '--height', str(height), '--asset-yaw', str(yaw),
                '--spp', '2', '--frames', '4',
                '--bounces', '2', '--warmup', '0', '--hidden', '--no-vsync',
                '--capture', str(ppm), '--report', str(metrics)]
        record = {'asset': str(asset.relative_to(root)), 'asset_sha256': digest(asset), 'kind': kind,
                  'command': [str(Path(a).relative_to(root)) if a.startswith(str(root)+'/') else a for a in args],
                  'log': str(logfile.relative_to(destination)), 'metrics': str(metrics.relative_to(destination)),
                  'image': str(png.relative_to(destination)), 'raw_framebuffer': str(ppm.relative_to(destination))}
        if filter_record:
            record['gameplay_helper_filter'] = filter_record
            record['preview_input'] = str(preview_input.relative_to(root))
        tick = time.monotonic()
        try:
            with logfile.open('w') as log:
                result = subprocess.run(args, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT,
                                        timeout=60, check=False, preexec_fn=memory_limit if os.name=='posix' else None)
            record['exit_code'] = result.returncode
            if result.returncode:
                raise ValueError('renderer exit '+str(result.returncode))
            m = json.loads(metrics.read_text())
            if not m.get('software_ray_tracing') or m.get('hardware_ray_tracing') or m.get('cpu_threads') != 2:
                raise ValueError('Runtime metadata did not confirm two-thread CPU ray tracing')
            if m.get('rays_traced',0) <= 0 or m.get('cpu_ms',{}).get('median',0) <= 0:
                raise ValueError('Runtime CPU timings/ray counts are missing or zero')
            if m.get('validation_errors') or m.get('triangles',0) <= 0:
                raise ValueError('Runtime reports empty/invalid geometry')
            record.update(triangles=m['triangles'], source_sha256=m.get('source_sha256'),
                          software_ray_tracing=m['software_ray_tracing'], cpu_threads=m['cpu_threads'],
                          rays_traced=m['rays_traced'], cpu_median_ms=m['cpu_ms']['median'],
                          accumulated_frames=m.get('accumulated_frames'), spp=m.get('spp'))
            with Image.open(ppm) as frame:
                if frame.size != (width,height):
                    raise ValueError('Unexpected framebuffer dimensions')
                frame.load()
                image = frame.convert('RGB')
                stats = ImageStat.Stat(image)
                record['rgb_mean'] = [round(c,3) for c in stats.mean]
                record['rgb_stddev'] = [round(c,3) for c in stats.stddev]
                image.save(png)
            record['image_sha256'] = digest(png)
            record['status'] = 'passed'
        except subprocess.TimeoutExpired:
            record['status'] = 'timeout'
        except (OSError, ValueError, KeyError) as exc:
            record['status'] = 'failed'
            record['error'] = str(exc)
        record['elapsed_seconds'] = round(time.monotonic()-tick,3)
        return record
    records = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(capture,p):p for p in assets}
        for future in concurrent.futures.as_completed(futures):
            record = future.result()
            records.append(record)
            print(f"[{len(records)}/{len(assets)}] {record['status']}: {record['asset']}", flush=True)
    records.sort(key=lambda x:x['asset'])
    sheets = make_sheets(records,destination,width,height)
    textures = make_texture_sheet(root,destination)
    failed = sum(x['status']!='passed' for x in records)
    report = {'schema': 1, 'renderer': str(renderer.relative_to(root)), 'renderer_sha256': renderer_sha,
              'method': 'Production framebuffer; isolated asset; neutral lighting; fitted camera; 2 spp x 4 accumulated frames; 2 bounces; 320x180',
              'asset_yaw_degrees': yaw, 'gameplay_filtered_civilian_closeups': civilian_close,
              'limits': {'simultaneous_processes': args.jobs, 'threads_per_process': 2, 'seconds_per_process': 60,
                         'virtual_memory_gib_per_process_posix': 4},
              'captures': records, 'contact_sheets': sheets, 'texture_review': textures,
              'summary': {'total':len(records),'passed':len(records)-failed,'failed':failed,
                          'elapsed_seconds':round(time.monotonic()-started,3)}}
    (destination/'index.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    print(json.dumps(report['summary'],indent=2))
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
