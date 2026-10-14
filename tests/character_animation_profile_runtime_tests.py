"""Validate real application reads of asset motion metadata, including v4 audits.

This is dispatch/serialization coverage, not a Player geometry acceptance gate.
"""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import socket
import tempfile

def _link_dir(link, target):
    """Symlink a directory; on Windows without symlink privilege use a junction, then a copy."""
    import os as _os, shutil as _shutil, subprocess as _subprocess
    from pathlib import Path as _Path
    link, target = _Path(link), _Path(target)
    try:
        link.symlink_to(target, target_is_directory=True)
        return
    except OSError:
        if _os.name != 'nt':
            raise
    if _subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(target)],
                       capture_output=True).returncode == 0:
        return
    _shutil.copytree(target, link)


p = argparse.ArgumentParser()
p.add_argument('--characterlab', required=True)
p.add_argument('--vaultline', required=True)
p.add_argument('--output-dir', required=True)
p.add_argument('--expected-player-profile', required=True)
a = p.parse_args()
out = Path(a.output_dir).resolve()
out.mkdir(parents=True, exist_ok=True)
env = {key: value for key, value in os.environ.items() if not key.startswith('FURY_')}
env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy',
           LIBGL_ALWAYS_SOFTWARE='1', LP_NUM_THREADS='1', FURY_CPU_THREADS='1',
           FURY_AUDIO_BACKEND='null')
if os.name == 'nt':
    env.pop('SDL_VIDEODRIVER', None)  # Windows SDL lacks 'offscreen'; use the default driver
source_root = Path(__file__).resolve().parents[1]

def run(command, name):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as port:
        port.bind(('127.0.0.1', 0))
        env['FURY_NET_PORT'] = str(port.getsockname()[1])
    with tempfile.TemporaryDirectory(prefix='fresh-' + name + '-', dir=out) as directory:
        work = Path(directory)
        _link_dir((work / 'assets'),source_root / 'assets')
        with (out / (name + '.log')).open('w') as log:
            subprocess.run(command, cwd=work, env=env, check=True, timeout=150,
                           stdout=log, stderr=subprocess.STDOUT)

for name, identity, role, legacy, expected in [
    ('player', 'PlayerBody', 'Player', False, a.expected_player_profile),
    ('player-legacy', 'PlayerBody', 'Player', True, 'generic'),
    ('rook', 'CrewRook', 'CrewTech', False, 'generic'),
    ('ghost', 'GhostLoop', 'Ghost', False, 'generic'),
]:
    for lod in ['near', 'far']:
        key = name + '-' + lod
        report = out / (key + '.json')
        command = [a.characterlab, '--id', identity, '--role', role, '--height', '1.8',
                   '--pose', 'crouch', '--lod', lod, '--frames', '1', '--width', '160',
                   '--image-height', '200', '--report', str(report)]
        if legacy:
            command += ['--legacy-profile']
        run(command, key)
        data = json.loads(report.read_text())
        assert data['animation_profile'] == expected, (key, data['animation_profile'])
        assert data['frames_submitted'] == 1 and data['validation_errors'] == 0
        assert data['backend'].startswith('OpenGL'), data['backend']

trace = out / 'player-input-audit.json'
audit = out / 'npc-profiles.json'
run([a.vaultline, '--width', '160', '--height', '120', '--frames', '300',
     '--player-input-audit', str(trace), '--npc-audit', str(audit)], 'player-world')
data = json.loads(trace.read_text())
assert len(data['frames']) == 300
assert 'Renderer backend: OpenGL 3.3' in (out / 'player-world.log').read_text()
# Prove the runtime consumed the policy numerically, not just in its labels.
transition = data['frames'][115]
assert transition['body_visible'] and transition['crouching']
assert data['frames'][114]['animation']['crouch_weight'] == 0
response = 9 if a.expected_player_profile == 'generic' else 6
assert abs(transition['animation']['crouch_weight'] + math.expm1(-response / 30)) < 1e-6
seen = set()
for frame in data['frames']:
    state = frame['animation']
    assert state['available']
    assert state['animation_profile'] == a.expected_player_profile
    assert state['state_version'] == 4
    fields = state['state_extension_v4'].split()
    assert fields[0] == '4' and len(fields) == 29, fields
    assert int(fields[-1]) == state['animation_profile_id']
    assert state['animation_profile_id'] == (a.expected_player_profile != 'generic')
    assert float(fields[-2]) == state['recovery_root_z']
    assert frame['standing_rig_height_preserved'] and frame['mesh_storage_stable']
    seen.add(frame['body_visible'])
assert seen == {0, 1}, seen
profiles = json.loads(audit.read_text())['profiles']
assert next(x for x in profiles if x['id'] == 'PlayerBody')['animation_profile'] == a.expected_player_profile
assert all(x['animation_profile'] == 'generic' for x in profiles if x['id'] != 'PlayerBody')
print('Application profile reads verified: 8 characterlab LOD cases; 300 production input frames; v4 profile/recovery metadata; non-Player defaults.')
