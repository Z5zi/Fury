#!/usr/bin/env python3
"""Capture original Vale world views without changing game or navigation logic."""
from pathlib import Path
import os, subprocess, argparse, json, hashlib, socket, time
from PIL import Image
p = argparse.ArgumentParser()
p.add_argument('name')
p.add_argument('--exe', type=Path)
p.add_argument('--mode', choices=['sides', 'motion', 'night', 'deep-night', 'backend', 'benchmark'], default='sides')
p.add_argument('--backend', choices=['soft', 'cpu-ray', 'gl'], default='soft')
p.add_argument('--size', type=int, default=480)
p.add_argument('--trace', default='ray')
o = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = (root / 'out/vale' / o.name).resolve()
out.mkdir(parents=True, exist_ok=True)
exe = (o.exe or root / 'build/apps/vaultline/vaultline').resolve()
env = {k: v for k, v in os.environ.items() if not k.startswith('FURY_')}
env.update(FURY_CPU_THREADS='2', LP_NUM_THREADS='2', OMP_NUM_THREADS='2', OPENBLAS_NUM_THREADS='2', SDL_VIDEODRIVER='offscreen' if o.backend == 'gl' else 'dummy', SDL_AUDIODRIVER='dummy', FURY_AUDIO_BACKEND='null', LIBGL_ALWAYS_SOFTWARE='1', MESA_SHADER_CACHE_DIR=str(root / 'out/vale/mesa-cache'), FURY_TRACE_MODE=o.trace, FURY_WORLD_ART='1', FURY_SURFACE_DETAIL='1', FURY_NPC_DETAIL='1', FURY_NET='embedded')
cases = []
if o.mode == 'sides':
    for distance, framing in [(3.2, 'body'), (1.1, 'face')]:
        for yaw in [0, 45, 90, 135, 180, -135, -90, -45]:
            cases.append((f'{framing}-{yaw}', yaw, distance, 2, 12, False, False))
elif o.mode == 'motion':
    cases = [('patrol-chase', -90, 3.2, 192, 12, True, False), ('talk-presentation', 45, 2.5, 36, 12, True, True)]
elif o.mode == 'deep-night':
    cases = [('deep-night-minus90', -90, 3.2, 384, 4, True, False), ('deep-night-minus45', -45, 3.2, 384, 4, True, False), ('deep-night-face', -45, 1.1, 384, 4, True, False)]
elif o.mode == 'night':
    cases = [('night-alley-minus90', -90, 3.2, 312, 4, True, False), ('night-alley-plus90', 90, 3.2, 312, 4, True, False)]
elif o.mode == 'backend':
    cases = [('face', -45, 1.1, 2, 12, False, False), ('body', 90, 3.2, 2, 12, False, False)]
else:
    cases = [(f'sample-{k}', -90, 3.2, 8, 12, True, False) for k in range(3)]
records = []
for name, yaw, distance, frames, fps, motion, talk in cases:
    case = out / name
    case.mkdir()
    work = case / 'work'
    work.mkdir()
    (work / 'assets').symlink_to(root / 'assets', target_is_directory=True)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    cmd = [str(exe), '--npc-motion' if motion else '--npc-view', 'NpcAlleyHmpd', '--npc-orbit', str(yaw), '--npc-distance', str(distance), '--width', str(o.size), '--height', str(o.size * 3 // 4 if o.mode == 'motion' else o.size * 4 // 3), '--frames', str(frames), '--capture-fps', str(fps), '--no-hud', '--capture', str(case / 'frame.ppm'), '--npc-audit', str(case / 'audit.json')]
    if o.backend != 'gl':
        cmd += ['--' + o.backend]
    if o.backend == 'cpu-ray':
        cmd += ['--spp', '2', '--bounces', '2']
    if talk:
        cmd += ['--npc-talk']
    if o.mode == 'motion':
        cmd += ['--capture-sequence', str(case / 'frames')]
    t = time.monotonic()
    r = subprocess.run(cmd, cwd=work, env=env | {'FURY_NET_PORT': str(port)}, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    elapsed = time.monotonic() - t
    (case / 'runtime.log').write_text(r.stdout)
    r.check_returncode()
    Image.open(case / 'frame.ppm').save(case / 'frame.png')
    state = json.loads((case / 'audit.json.state.json').read_text())
    metrics = json.loads((case / 'audit.json.render.json').read_text())
    assert metrics['validation_errors'] == 0
    record = {'name': name, 'command': cmd, 'process_seconds': elapsed, 'renderer': metrics, 'selected_actor': next((x for x in state['agents'] if x['id'] == 'NpcAlleyHmpd')), 'final_image_sha256': hashlib.sha256((case / 'frame.ppm').read_bytes()).hexdigest(), 'frames': frames, 'simulated_fps': fps, 'capture_mode': o.mode}
    records.append(record)
    print(name, record['selected_actor'], flush=True)
(out / 'CAPTURE_INPUT.json').write_text(json.dumps({'actor': 'NpcAlleyHmpd', 'binary_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(), 'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'environment': {k: v for k, v in env.items() if k in ['FURY_CPU_THREADS', 'LP_NUM_THREADS', 'OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'SDL_VIDEODRIVER', 'SDL_AUDIODRIVER', 'FURY_AUDIO_BACKEND', 'LIBGL_ALWAYS_SOFTWARE', 'FURY_TRACE_MODE', 'FURY_WORLD_ART', 'FURY_SURFACE_DETAIL', 'FURY_NPC_DETAIL', 'FURY_NET']}, 'cases': records, 'cpu_affinity': sorted(os.sched_getaffinity(0)), 'note': 'Actual game scene and player camera. Scenery and guard responses remain active. Night cases advance the real 160-second day cycle for 78 simulated seconds; deep-night uses 96 seconds. No custom time or AI override. Static --npc-view freezes the original world; --npc-motion retains live guard logic. Camera occlusion is retained. A case label does not certify chase behavior; inspect its actual saved state.'}, indent=2) + '\n')
