#!/usr/bin/env python3
"""Independently reconstruct Vale hand source corners and verify Far boundaries.

This checker reads the raw OBJ and gzip numeric targets directly. It does not
call either anatomy builder or the simplifier. The only reused utility parses
literal generated C++ arrays; all geometry is independently reconstructed.
"""
import argparse
from collections import Counter, defaultdict
import gzip
import hashlib
import json
import math
from pathlib import Path
import struct
from audit_vale_source import array
ROOT = Path(__file__).resolve().parents[1]

def check(value, message):
    if not value: raise AssertionError(message)

def binary32(values):
    return tuple(struct.unpack('f', struct.pack('f', value))[0] for value in values)

def minus(a, b):
    return tuple(x-y for x, y in zip(a, b))

def dot(a, b):
    return sum(x*y for x, y in zip(a, b))

def unit(v):
    norm = math.sqrt(dot(v, v))
    check(norm > 0 and math.isfinite(norm), 'Finite nonzero source basis')
    return tuple(x/norm for x in v)

def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])

def topology(indices, expected_boundary=None):
    edges, directions, neighbors = Counter(), Counter(), defaultdict(set)
    triangles = [tuple(indices[i:i+3]) for i in range(0, len(indices), 3)]
    check(len(indices) % 3 == 0 and len(set(triangles)) == len(triangles), 'Unique complete triangles')
    for triangle in triangles:
        check(len(set(triangle)) == 3, 'Three different triangle corners')
        for a, b in zip(triangle, triangle[1:]+triangle[:1]):
            edge = tuple(sorted((a, b)))
            edges[edge] += 1
            directions[edge] += 1 if a < b else -1
            neighbors[a].add(b); neighbors[b].add(a)
    check(all(count in [1, 2] for count in edges.values()), 'Manifold hand edge counts')
    check(all(directions[edge] == 0 for edge, count in edges.items() if count == 2), 'Consistent hand edge orientation')
    boundary = {edge for edge, count in edges.items() if count == 1}
    if expected_boundary is not None: check(boundary == expected_boundary, 'Exact locked wrist edges')
    remaining = set(neighbors); components = 0
    while remaining:
        components += 1; pending = [remaining.pop()]
        while pending:
            for neighbor in neighbors[pending.pop()]:
                if neighbor in remaining:
                    remaining.remove(neighbor); pending.append(neighbor)
    check(components == 1, 'One connected anatomical hand')
    euler = len(neighbors)-len(edges)+len(triangles)
    check(euler == 1, 'One disk with only the wrist boundary')
    return boundary, {'vertices': len(neighbors), 'triangles': len(triangles), 'boundary_edges': len(boundary),
                      'components': components, 'euler_characteristic': euler}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT/'docs/characters/vale/HAND_SOURCE_AUDIT.json')
    args = parser.parse_args()
    points, texcoords, faces, groups = [], [], [], set()
    for text in (ROOT/'third_party/mpfb_assets/base.obj').read_text().splitlines():
        fields = text.split('#', 1)[0].split()
        if not fields: continue
        if fields[0] == 'v': points.append(list(map(float, fields[1:4])))
        elif fields[0] == 'vt': texcoords.append(tuple(map(float, fields[1:3])))
        elif fields[0] == 'g': groups = set(fields[1:])
        elif fields[0] == 'f':
            corners = [tuple(int(number)-1 for number in item.split('/')[:2]) for item in fields[1:]]
            faces.append((set(groups), corners))
    recipe_path = ROOT/'third_party/vale_anatomy/vale_recipe.json'
    recipe = json.loads(recipe_path.read_text())
    for target in recipe['targets']:
        with gzip.open(ROOT/target['path'], 'rt') as data:
            for text in data:
                fields = text.split('#', 1)[0].split()
                if not fields: continue
                index = int(fields[0]); delta = list(map(float, fields[1:]))
                check(len(delta) == 3, 'Three numeric source target coordinates')
                for axis in range(3): points[index][axis] += target['weight']*delta[axis]
    def group_ids(name):
        result = sorted({vertex for names, corners in faces if name in names for vertex, _ in corners})
        check(result, f'Present source helper group {name}')
        return result
    def center(name):
        indices = group_ids(name)
        return tuple(sum(points[index][axis] for index in indices)/len(indices) for axis in range(3))
    body = group_ids('body')
    scale = .989/(max(points[index][1] for index in body)-min(points[index][1] for index in body))
    wrist = center('joint-l-hand')
    down = unit(minus(center('joint-l-finger-3-1'), wrist))
    spread = minus(center('joint-l-finger-5-1'), center('joint-l-finger-2-1'))
    width = unit(tuple(x-y*dot(spread, down) for x, y in zip(spread, down)))
    up = tuple(-x for x in down)
    normal = unit(cross(width, up))
    selected, provenance = [], []
    for number, (names, corners) in enumerate(faces):
        if 'body' not in names: continue
        if not all(points[index][0] > 3.5 and dot(minus(points[index], wrist), down) > -.23 for index, _ in corners): continue
        for k in range(1, len(corners)-1):
            selected.append((corners[0], corners[k], corners[k+1])); provenance.append(number)
    original_vertices = sorted({index for triangle in selected for index, _ in triangle})
    original_uvs = sorted({uv for triangle in selected for _, uv in triangle})
    lookup = {value: index for index, value in enumerate(original_vertices)}
    uv_lookup = {value: index for index, value in enumerate(original_uvs)}
    frame = [tuple(dot(minus(points[index], wrist), axis)*scale for axis in [width, up, normal])
             for index in original_vertices]
    expected = []
    for x, y, z in frame:
        t = max(0, min(1, (-y-.025)/.080)); t = t*t*(3-2*t)
        expected.append((x*(1-.14*t)+.004*max(0, min(1, (-x-.015)/.025)), y+.009*t, z-.019*t))
    header = ROOT/'engine/include/fury/generated/vale_hands.hpp'
    values = {name: array(header, name) for name in ['positions', 'source_frame_positions', 'near_indices',
              'far_indices', 'uv_coordinates', 'near_uv_indices', 'far_uv_indices', 'source_vertex_indices',
              'source_uv_indices', 'source_triangle_faces', 'wrist_boundary_indices']}
    check(values['source_vertex_indices'] == original_vertices, 'Every raw source vertex ID')
    check(values['source_uv_indices'] == original_uvs, 'Every raw source UV ID')
    check(values['source_triangle_faces'] == provenance, 'Every original source face ID')
    for name, expected_points in [('source_frame_positions', frame), ('positions', expected),
                                  ('uv_coordinates', [texcoords[index] for index in original_uvs])]:
        check([binary32(point) for point in values[name]] == [binary32(point) for point in expected_points],
              f'Every independently reconstructed binary32 {name}')
    near = [lookup[index] for triangle in selected for index, _ in triangle]
    near_uv = [uv_lookup[uv] for triangle in selected for _, uv in triangle]
    check(values['near_indices'] == near and values['near_uv_indices'] == near_uv, 'Every original hand triangle and UV corner')
    boundary, near_topology = topology(near)
    loop = values['wrist_boundary_indices']
    check(len(loop) == len(set(loop)) == 20, 'Exact simple 20-point wrist cycle')
    check({tuple(sorted(pair)) for pair in zip(loop, loop[1:]+loop[:1])} == boundary, 'Complete ordered wrist cycle')
    _, far_topology = topology(values['far_indices'], boundary)
    check(max(values['far_indices']) < len(expected), 'Far only uses retained source endpoints')
    first_uv = {}
    for index, uv in zip(near, near_uv): first_uv.setdefault(index, uv)
    check(values['far_uv_indices'] == [first_uv[index] for index in values['far_indices']], 'Every Far endpoint retains its source UV')
    controls = []
    for label, corrupt in [('duplicate_far_triangle', values['far_indices']+values['far_indices'][:3]),
                            ('reversed_near_triangle', [near[0], near[2], near[1]]+near[3:]),
                            ('missing_far_triangle', values['far_indices'][3:])]:
        try: topology(corrupt, boundary)
        except AssertionError: controls.append({'case': label, 'rejected': True})
        else: raise AssertionError('Checker accepted '+label)
    report = {'result': 'passed', 'method': __doc__, 'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'recipe_sha256': hashlib.sha256(recipe_path.read_bytes()).hexdigest(),
              'header_sha256': hashlib.sha256(header.read_bytes()).hexdigest(),
              'source_positions': len(expected), 'source_uvs': len(original_uvs),
              'original_triangles_reconstructed': len(selected), 'original_uv_corners_reconstructed': len(near_uv),
              'near_topology': near_topology, 'far_topology': far_topology,
              'negative_controls': controls, 'finger_animation': False}
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({key: report[key] for key in ['result', 'source_positions', 'original_triangles_reconstructed', 'original_uv_corners_reconstructed']}))
if __name__ == '__main__':
    main()
