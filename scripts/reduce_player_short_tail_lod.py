"""Offline Far companion to the CC0 shortened tied-tail experiment.

Run with Blender --background --python scripts/reduce_player_short_tail_lod.py.
Source UV corner coordinates survive collapse. Runtime never depends on Blender.
"""
from pathlib import Path
import bpy, json, math, hashlib

root = Path(__file__).resolve().parents[1]
assets = root / 'third_party/player_body_hair'
source = json.loads((assets / 'fit/fit.json').read_text())
keep = {i for c in source['components'] if c['min'][1] < .37 and c['vertices'] > 100 for i in c['indices']}
ids = sorted(keep)
remap = {v: i for i, v in enumerate(ids)}
faces = [f for f in source['faces'] if f[0][0] in keep]

def warp(p):
    x, y, z = p
    if y < .445:
        d = .445 - y
        y = .445 - d * (.36 + .64 * math.exp(-d / .025))
    return x, y, z

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
mesh = bpy.data.meshes.new('CC0_Player_short_tied_tail')
mesh.from_pydata([warp(source['vertices'][i]) for i in ids], [],
                   [[remap[i] for i, uv in f] for f in faces])
mesh.update()
obj = bpy.data.objects.new(mesh.name, mesh)
bpy.context.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
uvs = mesh.uv_layers.new(name='SourceUV')
for polygon, original in zip(mesh.polygons, faces):
    for loop, (_, uv) in zip(polygon.loop_indices, original):
        uvs.data[loop].uv = source['uvs'][uv]
    polygon.use_smooth = True
decimate = obj.modifiers.new('Far bound geometry reduction', 'DECIMATE')
decimate.ratio = .15
decimate.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier=decimate.name)
mesh = obj.data
mesh.calc_loop_triangles()
uvs = mesh.uv_layers.active.data
result = {
    'generator': 'Blender ' + bpy.app.version_string + '; core shells only; collapse ratio 0.15; deterministic pose-aware edge refinement to 448 source triangles',
    'fit_sha256': hashlib.sha256((assets / 'fit/fit.json').read_bytes()).hexdigest(),
    'positions': [list(v.co) for v in mesh.vertices],
    'triangles': [[[int(mesh.loops[loop].vertex_index), list(uvs[loop].uv)]
                   for loop in triangle.loops] for triangle in mesh.loop_triangles],
}
# Spend the Far budget on longitudinal deformation resolution rather than
# largely redundant rings around the tail. Shared edges are split on BOTH
# adjacent faces, preserving topology and source UV corner interpolation.
def head_weight(y):
    t = max(0.0, min(1.0, (.435-y)/.070))
    return 1-.40*t*t*(3-2*t)

while len(result['triangles']) < 448:
    edges = {}
    for index, face in enumerate(result['triangles']):
        for k in range(3):
            edge = tuple(sorted((face[k][0], face[(k+1)%3][0])))
            edges.setdefault(edge, []).append(index)
    def score(edge):
        a, b = [result['positions'][i] for i in edge]
        dy = abs(a[1]-b[1])
        return dy * abs(head_weight(a[1])-head_weight(b[1]))
    ranked = sorted(edges, key=lambda e: (score(e), e), reverse=True)
    edge = next((e for e in ranked if len(result['triangles'])+len(edges[e]) <= 448), None)
    if edge is None or score(edge) < 1e-10:
        break
    mid = len(result['positions'])
    result['positions'].append([(a+b)/2 for a,b in zip(result['positions'][edge[0]],result['positions'][edge[1]])])
    replace = set(edges[edge]); faces = []
    for index, face in enumerate(result['triangles']):
        if index not in replace:
            faces.append(face); continue
        k = next(k for k in range(3) if tuple(sorted((face[k][0],face[(k+1)%3][0]))) == edge)
        a,b,c = face[k],face[(k+1)%3],face[(k+2)%3]
        corner = [mid,[(x+y)/2 for x,y in zip(a[1],b[1])]]
        faces.extend([[a,corner,c],[corner,b,c]])
    result['triangles'] = faces

path = assets / 'short-tail-far.json'
path.write_text(json.dumps(result, separators=(',', ':')) + '\n')
print(path, len(result['positions']), 'vertices', len(result['triangles']), 'triangles')
