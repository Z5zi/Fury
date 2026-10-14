#!/usr/bin/env python3
"""Reproducible Vale diagnostic captures. These are not game-world evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
ANGLES = [(0, 'front'), (45, 'quarter'), (90, 'right'), (135, 'rear-quarter'),
          (180, 'back'), (-135, 'rear-other'), (-90, 'left'), (-45, 'opposite')]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def cases(poses):
    if not poses:
        return [(f'{framing}-{label}', framing, yaw, 'bind', 0, [])
                for framing in ['body', 'face'] for yaw, label in ANGLES]
    result = [(f'{pose}-{phase}', 'body', 45, pose, phase, [])
              for pose in ['idle', 'walk', 'run', 'crouch', 'crouch-walk', 'talk']
              for phase in [0, .25, .5, .75]]
    for label, yaw, pitch in [('yaw-minus', -.9, 0), ('yaw-plus', .9, 0),
                               ('pitch-minus', 0, -.4), ('pitch-plus', 0, .4)]:
        for angle in [0, 90, 180]:
            result.append((f'attention-{label}-{angle}', 'face', angle, 'talk', .125,
                           ['--attention-yaw', str(yaw), '--attention-pitch', str(pitch)]))
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--exe', type=Path, default=ROOT/'build/apps/characterlab/fury_characterlab')
    parser.add_argument('--lod', choices=['near', 'far'], default='near')
    parser.add_argument('--backend', choices=['soft', 'cpu-ray', 'gl'], default='gl')
    parser.add_argument('--size', type=int, default=720)
    parser.add_argument('--spp', type=int, default=1)
    parser.add_argument('--frames', type=int, default=1)
    parser.add_argument('--trace', choices=['ray', 'path'], default='ray')
    parser.add_argument('--poses', action='store_true')
    parser.add_argument('--clay', action='store_true')
    parser.add_argument('--baseline', action='store_true', help='Expect original generic profile from an explicitly selected baseline executable')
    args = parser.parse_args()
    out, exe = args.output.resolve(), args.exe.resolve()
    out.mkdir(parents=True, exist_ok=False)
    overrides = {'FURY_CPU_THREADS': '2', 'LP_NUM_THREADS': '2', 'OMP_NUM_THREADS': '2',
                 'SDL_VIDEODRIVER': 'offscreen' if args.backend == 'gl' else 'dummy',
                 'SDL_AUDIODRIVER': 'dummy', 'FURY_AUDIO_BACKEND': 'null',
                 'LIBGL_ALWAYS_SOFTWARE': '1', 'MESA_SHADER_CACHE_DIR': str(ROOT/'out/vale/mesa-cache'),
                 'FURY_TRACE_MODE': args.trace}
    env = {key: value for key, value in os.environ.items() if not key.startswith('FURY_')}
    env.update(overrides)
    records = []
    manifest = {'capture_kind': 'diagnostic_engine_stage', 'game_world_proof': False,
                'binary_sha256': sha(exe), 'script_sha256': sha(Path(__file__)),
                'environment_overrides': overrides, 'cpu_affinity': sorted(os.sched_getaffinity(0)),
                'options': vars(args), 'captures': records}
    for name, framing, yaw, pose, phase, extra in cases(args.poses):
        prefix = out/name
        command = [str(exe), '--id', 'NpcAlleyHmpd', '--role', 'Security', '--height', '1.86',
                   '--width', str(args.size), '--image-height', str(args.size*4//3),
                   '--frames', str(args.frames), '--spp', str(args.spp), '--bounces', '4',
                   '--pose', pose, '--phase', str(phase), '--yaw', str(yaw), '--lod', args.lod,
                   '--capture', str(prefix)+'.ppm', '--report', str(prefix)+'.json', *extra]
        if args.backend != 'gl': command.append('--'+args.backend)
        if framing == 'face': command += ['--face', '--distance', '1.40']
        if args.clay: command.append('--clay')
        start = time.monotonic()
        run = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
        prefix.with_suffix('.log').write_text(run.stdout)
        run.check_returncode()
        report = json.loads(prefix.with_suffix('.json').read_text())
        assert report['identity'] == 'NpcAlleyHmpd' and report['role'] == 'security'
        assert report['individualized'] != args.baseline
        assert report['validation_errors'] == 0 and report['exit_code'] == 0
        assert report['frames_submitted'] == args.frames
        expected = {'gl': 'OpenGL', 'soft': 'CPU software rasterizer', 'cpu-ray': 'CPU BVH ray/path tracer'}[args.backend]
        assert report['backend'].startswith(expected), (expected, report['backend'])
        records.append({'name': name, 'command': command, 'pixel_sha256': sha(prefix.with_suffix('.ppm')),
                        'report_sha256': sha(prefix.with_suffix('.json')),
                        'process_seconds': time.monotonic()-start})
        (out/'CAPTURE_INPUT.json').write_text(json.dumps(manifest, indent=2, default=str)+'\n')
        print(name, flush=True)

if __name__ == '__main__':
    main()
