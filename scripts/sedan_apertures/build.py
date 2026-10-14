#!/usr/bin/env python3
"""Offline, hash-pinned parked-sedan aperture derivative. Requires NumPy.

Subtract six authored-pane-local rectangular prisms only from Sedan_Body.
Reconstruct six full opaque pane faces and six gasket reveals from its exact
cut boundary loops to one shared inset pane perimeter in both LODs. Half-space
clipping preserves outside fragments and interpolates original attributes.
Clip the two existing vertical rain/grease marks at the same side apertures so
they remain on painted areas without opaque intrusion over the glazing.
Original binary payload is retained byte-for-byte; only fifteen primitive
accessor bindings change. No runtime CSG, material/interior edits, or shared
asset replacement. This is the third, explicitly bounded construction.
"""
from pathlib import Path
import copy
import hashlib
import json
import struct
import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ASSETS = ROOT / 'assets/meshes/harbor_metro'
SOURCES = {
    'hm_civ_sedan_v3.glb': '284a0944ed695ad092227fb2bf1626ce6ab7303af8b43a41fce3ef187f2c446f',
    'hm_civ_sedan_v3_lod1.glb': '74a035582144bb58ab1837838a90937c56523433f1b8e0b0cb5470019ef64741',
}
sha = lambda b: hashlib.sha256(b).hexdigest()

def transform(n):
    if 'matrix' in n:
        return np.array(n['matrix']).reshape(4, 4).T
    x, y, z, w = n.get('rotation', [0, 0, 0, 1])
    r = np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                  [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                  [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
    m = np.eye(4)
    m[:3, :3] = r @ np.diag(n.get('scale', [1, 1, 1]))
    m[:3, 3] = n.get('translation', [0, 0, 0])
    return m

def read_glb(path):
    data = path.read_bytes()
    magic, version, length = struct.unpack_from('<III', data)
    assert (magic, version, length) == (0x46546c67, 2, len(data))
    size, typ = struct.unpack_from('<II', data, 12)
    assert typ == 0x4e4f534a
    g = json.loads(data[20:20+size])
    bs, bt = struct.unpack_from('<II', data, 20+size)
    assert bt == 0x004e4942 and 28+size+bs == len(data)
    return g, data[28+size:], data

def accessor(g, blob, i):
    a = g['accessors'][i]
    assert 'sparse' not in a and not a.get('normalized', False)
    v = g['bufferViews'][a['bufferView']]
    assert v['buffer'] == 0
    dt = np.dtype({5126:'<f4', 5125:'<u4', 5123:'<u2', 5121:'u1'}[a['componentType']])
    n = {'SCALAR':1, 'VEC2':2, 'VEC3':3, 'VEC4':4}[a['type']]
    return np.ndarray((a['count'], n), dtype=dt, buffer=blob,
        offset=v.get('byteOffset', 0)+a.get('byteOffset', 0),
        strides=(v.get('byteStride', dt.itemsize*n), dt.itemsize)).copy()

def split(poly, coordinates, axis, sign, limit):
    """Split a convex barycentric polygon at one plane without changing order."""
    inside, outside = [], []
    for a, b in zip(poly, poly[1:]+poly[:1]):
        da = sign * ((a @ coordinates)[axis] - limit)
        db = sign * ((b @ coordinates)[axis] - limit)
        ia, ib = da >= 0, db >= 0
        (inside if ia else outside).append(a)
        if ia != ib:
            p = a + (b-a) * (da/(da-db))
            inside.append(p)
            outside.append(p)
    return inside, outside

def subtract(poly, coords, lo, hi):
    # Reject a disjoint polygon before splitting, so untouched triangles retain
    # their exact vertex/index data and ordering.
    p = np.array(poly) @ coords
    if np.any(p.max(axis=0) < lo) or np.any(p.min(axis=0) > hi):
        return [poly], False
    current, fragments = poly, []
    for axis in range(3):
        for sign, limit in [(1, lo[axis]), (-1, hi[axis])]:
            current, outside = split(current, coords, axis, sign, limit)
            if len(outside) >= 3:
                fragments.append(outside)
            if len(current) < 3:
                return [poly], False
    # Final intersection area must be nonzero; mere plane contact is unchanged.
    p = np.array(current) @ coords
    area = sum(np.linalg.norm(np.cross(p[i]-p[0], p[i+1]-p[0]))
               for i in range(1, len(p)-1))
    return (fragments, True) if area > 1e-13 else ([poly], False)

def derive(filename, expected, specs):
    src = ASSETS / filename
    g, blob, source_data = read_glb(src)
    assert sha(source_data) == expected, f'Unsupported source: {src}'
    original = copy.deepcopy(g)
    assert len(g['buffers']) == 1 and 'uri' not in g['buffers'][0]
    out = bytearray(blob)
    nodes = {}
    def visit(i, parent):
        n = g['nodes'][i]
        m = parent @ transform(n)
        name = n.get('name', '').removeprefix('_LOD1tmp_')
        assert name not in nodes
        nodes[name] = (n, m)
        for child in n.get('children', []):
            visit(child, m)
    for i in g['scenes'][g.get('scene', 0)]['nodes']:
        visit(i, np.eye(4))

    def append_array(a, semantic):
        while len(out) % 4:
            out.append(0)
        a = np.ascontiguousarray(a)
        vi = len(g['bufferViews'])
        g['bufferViews'].append({'buffer':0, 'byteOffset':len(out),
             'byteLength':a.nbytes, 'target':34963 if semantic == 'INDEX' else 34962})
        out.extend(a.tobytes())
        ac = {'bufferView':vi, 'componentType':5125 if semantic == 'INDEX' else 5126,
              'count':len(a), 'type':{1:'SCALAR', 2:'VEC2', 3:'VEC3', 4:'VEC4'}[a.shape[1]]}
        if semantic == 'POSITION':
            ac.update(min=a.min(axis=0).tolist(), max=a.max(axis=0).tolist())
        index = len(g['accessors'])
        g['accessors'].append(ac)
        return index

    targets = {'Sedan_Body':specs}
    for sign in [-1,1]:
        targets[f'Sedan_rain_{sign}']=[next(s for s in specs if s['pane']==f'Sedan_swF_{sign}')]
    records = []
    for name, apertures in targets.items():
        node, model = nodes[name]
        mesh_index = node['mesh']
        assert sum(n.get('mesh') == mesh_index for n in g['nodes']) == 1
        mesh = g['meshes'][mesh_index]
        assert len(mesh['primitives']) == 1
        pr = mesh['primitives'][0]
        assert pr.get('mode', 4) == 4 and 'targets' not in pr
        assert set(pr['attributes']) == {'POSITION', 'NORMAL', 'TEXCOORD_0'}
        attrs = {key:accessor(g, blob, value) for key, value in pr['attributes'].items()}
        assert all(a.dtype == np.dtype('<f4') for a in attrs.values())
        indices = accessor(g, blob, pr['indices']).reshape(-1, 3).astype(np.uint32)
        position = attrs['POSITION'].astype(float)
        world = position @ model[:3, :3].T + model[:3, 3]
        frames = []
        for aperture in apertures:
            inv = np.linalg.inv(np.array(aperture['pane_to_asset']))
            frames.append((world @ inv[:3, :3].T + inv[:3, 3],
                           np.array(aperture['aperture_min']), np.array(aperture['aperture_max'])))
        added = {key:[] for key in attrs}
        new_indices, changed_ids = [], []
        removed_area = 0.0
        min_winding_dot = 1.0
        degenerate_fragments = 0
        for ordinal, ids in enumerate(indices):
            fragments, touched = [[np.eye(3)[i] for i in range(3)]], False
            for coords, lo, hi in frames:
                following = []
                for poly in fragments:
                    pieces, hit = subtract(poly, coords[ids], lo, hi)
                    following.extend(pieces)
                    touched |= hit
                fragments = following
            if not touched:
                new_indices.append(ids.tolist())
                continue
            changed_ids.append(ordinal)
            tri = position[ids]
            old_normal = np.cross(tri[1]-tri[0], tri[2]-tri[0])
            old_area = np.linalg.norm(old_normal)/2
            kept_area = 0.0
            # Deduplicate only the new vertices within this original triangle.
            cache = {}
            def vertex(bary):
                for i in range(3):
                    if np.array_equal(bary, np.eye(3)[i]):
                        return int(ids[i])
                key = tuple(bary)
                if key in cache:
                    return cache[key]
                index = len(position) + len(added['POSITION'])
                for semantic, values in attrs.items():
                    value = bary @ values[ids].astype(float)
                    if semantic == 'NORMAL':
                        length = np.linalg.norm(value)
                        assert length > 1e-10
                        value /= length
                    added[semantic].append(value)
                cache[key] = index
                return index
            for poly in fragments:
                for i in range(1, len(poly)-1):
                    bary = np.array([poly[0], poly[i], poly[i+1]])
                    p = (bary @ tri).astype(np.float32).astype(float)
                    normal = np.cross(p[1]-p[0], p[2]-p[0])
                    area = np.linalg.norm(normal)/2
                    if area < 1e-12:
                        degenerate_fragments += 1
                        continue
                    dot = float(np.dot(normal, old_normal)/(2*area*2*old_area))
                    assert dot > .999, (name, ordinal, dot)
                    min_winding_dot = min(min_winding_dot, dot)
                    kept_area += area
                    new_indices.append([vertex(b) for b in bary])
            removed_area += max(0., old_area-kept_area)
        for semantic, values in attrs.items():
            a = np.vstack([values, np.array(added[semantic], dtype='<f4')])
            assert np.isfinite(a).all() and a[:len(values)].tobytes() == values.tobytes()
            pr['attributes'][semantic] = append_array(a, semantic)
        pr['indices'] = append_array(np.array(new_indices, dtype='<u4').reshape(-1, 1), 'INDEX')
        records.append({'node':name, 'mesh_index':mesh_index,
                        'original_triangles':len(indices), 'result_triangles':len(new_indices),
                        'affected_original_triangles':len(changed_ids), 'affected_ordinals':changed_ids,
                        'added_vertices':len(added['POSITION']), 'removed_local_surface_area':removed_area,
                        'minimum_fragment_winding_dot':min_winding_dot,
                        'discarded_degenerate_fragments':degenerate_fragments})
        if name == 'Sedan_Body':
            original_body_position = position.copy()
            original_body_world = world.copy()
            original_body_indices = indices.copy()
            body_transform = model.copy()
            emitted_body_positions = accessor(g, bytes(out), pr['attributes']['POSITION'])

    # The original body has two thin skins. Obtain their exact cut curves by
    # intersecting the original triangles with the same six clipping planes.
    # Trace each closed loop independently; never bridge disconnected extremes.
    from collections import defaultdict, Counter
    for aperture in specs:
        pane_name = aperture['pane']
        thin = aperture['normal_axis']
        axes = [i for i in range(3) if i != thin]
        pane_frame = np.array(aperture['pane_to_asset'])
        inv_pane = np.linalg.inv(pane_frame)
        coords_all = original_body_world @ inv_pane[:3,:3].T + inv_pane[:3,3]
        lo, hi = np.array(aperture['aperture_min']), np.array(aperture['aperture_max'])
        inset = .018
        inner_lo, inner_hi = lo.copy(), hi.copy()
        inner_lo[axes] += inset
        inner_hi[axes] -= inset
        segments = []
        for ids in original_body_indices:
            coords = coords_all[ids]
            if np.any(coords.max(axis=0) < lo) or np.any(coords.min(axis=0) > hi):
                continue
            poly = list(np.eye(3))
            for axis in range(3):
                for sign, limit in [(1,lo[axis]),(-1,hi[axis])]:
                    if len(poly) < 3:
                        break
                    poly, _ = split(poly, coords, axis, sign, limit)
            if len(poly) < 3:
                continue
            local = np.array(poly) @ coords
            for i in range(len(poly)):
                j = (i+1) % len(poly)
                pa, pb = local[i], local[j]
                if np.linalg.norm(pa-pb) < 1e-9:
                    continue
                if not any(max(abs(pa[ax]-limit),abs(pb[ax]-limit)) < 1e-8
                           for ax in axes for limit in [lo[ax],hi[ax]]):
                    continue
                # Cast in body-local coordinates exactly as the body writer did.
                ba = (poly[i] @ original_body_position[ids]).astype('<f4')
                bb = (poly[j] @ original_body_position[ids]).astype('<f4')
                segments.append((pa,pb,ba,bb))
        key = lambda v: tuple(np.round(v,6))
        adjacency = defaultdict(set)
        degree = Counter()
        representatives = defaultdict(list)
        for pa,pb,ba,bb in segments:
            ka,kb=key(pa),key(pb)
            assert ka != kb
            adjacency[ka].add(kb);adjacency[kb].add(ka)
            degree[ka]+=1;degree[kb]+=1
            representatives[ka].append(ba);representatives[kb].append(bb)
        assert set(degree.values()) == {2}, (pane_name,Counter(degree.values()))
        left=set(adjacency);loops=[]
        while left:
            pending={left.pop()};loop=set()
            while pending:
                v=pending.pop();loop.add(v);pending |= adjacency[v]-loop
            left -= loop
            loops.append(loop)
        assert len(loops)==2, (pane_name,'unexpected body cut loop count',len(loops))
        seam_gap=max(np.linalg.norm(np.asarray(vs)-vs[0],axis=1).max()
                     for vs in representatives.values())
        assert seam_gap < 5e-7, (pane_name,seam_gap)
        # Every ring start must coincide with an actually emitted body vertex.
        emitted_keys={tuple(p) for p in emitted_body_positions}
        missing=[p for s in segments for p in s[2:] if tuple(p) not in emitted_keys]
        maximum_body_distance=0.
        for p in missing:
            distance=float(np.linalg.norm(emitted_body_positions-p,axis=1).min())
            maximum_body_distance=max(maximum_body_distance,distance)
        assert maximum_body_distance < 5e-7, (pane_name,maximum_body_distance)

        def project_inner(point):
            q=point.copy()
            for axis in axes:
                q[axis]=inner_lo[axis]+(point[axis]-lo[axis])/(hi[axis]-lo[axis])*(inner_hi[axis]-inner_lo[axis])
            q[thin]=0.
            return q

        gasket_node,gasket_frame=nodes[pane_name+'_gasket']
        inv_gasket=np.linalg.inv(gasket_frame)
        triangles=[]
        for pa,pb,ba,bb in segments:
            wa=body_transform[:3,:3] @ ba.astype(float)+body_transform[:3,3]
            wb=body_transform[:3,:3] @ bb.astype(float)+body_transform[:3,3]
            qa,qb=project_inner(pa),project_inner(pb)
            wqa=pane_frame[:3,:3] @ qa+pane_frame[:3,3]
            wqb=pane_frame[:3,:3] @ qb+pane_frame[:3,3]
            quad=np.array([wa,wb,wqb,wqa]) @ inv_gasket[:3,:3].T+inv_gasket[:3,3]
            triangles.extend([quad[[0,1,2]],quad[[0,2,3]]])
        positions=[];normals=[];uvs=[]
        for tri in triangles:
            tri=tri.astype('<f4')
            n=np.cross(tri[1].astype(float)-tri[0],tri[2].astype(float)-tri[0])
            assert np.linalg.norm(n)>1e-10, (pane_name,'degenerate reveal')
            n/=np.linalg.norm(n)
            for v in tri:
                positions.append(v);normals.append(n)
                w=gasket_frame[:3,:3] @ v.astype(float)+gasket_frame[:3,3]
                p=inv_pane[:3,:3] @ w+inv_pane[:3,3]
                uvs.append([(p[ax]-lo[ax])/(hi[ax]-lo[ax]) for ax in axes])
        gp=g['meshes'][gasket_node['mesh']]['primitives'][0]
        old_gasket_triangles=accessor(original,blob,gp['indices']).size//3
        for semantic,data in [('POSITION',positions),('NORMAL',normals),('TEXCOORD_0',uvs)]:
            gp['attributes'][semantic]=append_array(np.array(data,dtype='<f4'),semantic)
        gp['indices']=append_array(np.arange(len(positions),dtype='<u4').reshape(-1,1),'INDEX')
        records.append({'node':pane_name+'_gasket','mesh_index':gasket_node['mesh'],
            'original_triangles':old_gasket_triangles,'result_triangles':len(triangles),
            'construction':'Two independently traced closed body-skin loops, each connected by exact cut segments to the same inset pane perimeter; no arbitrary endpoint bridging.',
            'body_boundary_loops':len(loops),'body_loop_vertices':sorted(map(len,loops)),
            'body_boundary_segments':len(segments),'maximum_matched_cut_vertex_gap_m':float(seam_gap),
            'maximum_cut_to_emitted_body_vertex_gap_m':maximum_body_distance,
            'geometrically_coincident_not_index_welded':True,'pane_border_inset_m':inset})

        # Full planar opaque presentation under the existing merged material
        # path. Preserve each pane's source material and node transform.
        pn,pm=nodes[pane_name];pinv=np.linalg.inv(pm)
        verts=[]
        for u,v in [(0,0),(1,0),(1,1),(0,1)]:
            q=np.zeros(3)
            q[axes[0]]=inner_hi[axes[0]] if u else inner_lo[axes[0]]
            q[axes[1]]=inner_hi[axes[1]] if v else inner_lo[axes[1]]
            # 0.5mm overlap hides float-rounding seams under the dark reveal.
            q[axes[0]] += .0005 if u else -.0005
            q[axes[1]] += .0005 if v else -.0005
            w=pane_frame[:3,:3] @ q+pane_frame[:3,3]
            verts.append(pinv[:3,:3] @ w+pinv[:3,3])
        verts=np.array(verts,dtype='<f4')
        normal=np.zeros(3);normal[thin]=-1 if pane_name in ('Sedan_rg','Sedan_swF_-1','Sedan_swR_-1') else 1
        pane_indices=np.array([[0,1,2],[0,2,3]],dtype='<u4')
        face=np.cross(verts[1]-verts[0],verts[2]-verts[0])
        if np.dot(face,normal)<0:pane_indices=pane_indices[:,[0,2,1]]
        pp=g['meshes'][pn['mesh']]['primitives'][0]
        old_pane_triangles=accessor(original,blob,pp['indices']).size//3
        pp['attributes']['POSITION']=append_array(verts,'POSITION')
        pp['attributes']['NORMAL']=append_array(np.tile(normal,(4,1)).astype('<f4'),'NORMAL')
        pp['attributes']['TEXCOORD_0']=append_array(np.array([[0,0],[1,0],[1,1],[0,1]],dtype='<f4'),'TEXCOORD_0')
        pp['indices']=append_array(pane_indices.reshape(-1,1),'INDEX')
        records.append({'node':pane_name,'mesh_index':pn['mesh'],
            'original_triangles':old_pane_triangles,'result_triangles':2,
            'construction':'Shared full rectangular pane perimeter for both LODs; source material and transform unchanged; 0.5mm under-reveal overlap.',
            'pane_inner_min':inner_lo[axes].tolist(),'pane_inner_max':inner_hi[axes].tolist()})
    # Explicit exact non-target JSON preservation and unchanged binary prefix.
    edited_meshes = {r['mesh_index'] for r in records}
    for i, mesh in enumerate(g['meshes']):
        if i not in edited_meshes:
            assert mesh == original['meshes'][i]
    for key in original:
        if key not in {'buffers', 'bufferViews', 'accessors', 'meshes'}:
            assert g[key] == original[key], key
    assert g['bufferViews'][:len(original['bufferViews'])] == original['bufferViews']
    assert g['accessors'][:len(original['accessors'])] == original['accessors']
    assert bytes(out[:len(blob)]) == blob
    g['buffers'][0]['byteLength'] = len(out)
    while len(out) % 4:
        out.append(0)
    js = json.dumps(g, separators=(',', ':'), ensure_ascii=False).encode()
    js += b' ' * ((-len(js)) % 4)
    output = (struct.pack('<III', 0x46546c67, 2, 28+len(js)+len(out)) +
              struct.pack('<II', len(js), 0x4e4f534a) + js +
              struct.pack('<II', len(out), 0x004e4942) + out)
    dest = ASSETS / filename.replace('hm_civ_sedan_v3', 'hm_parked_sedan_aperture_v1')
    dest.write_bytes(output)
    return {'source':filename, 'source_sha256':expected, 'source_bytes':len(source_data),
            'output':dest.name, 'output_sha256':sha(output), 'output_bytes':len(output),
            'original_binary_prefix_unchanged':True, 'materials_nodes_non_target_meshes_exact':True,
            'records':records}

if __name__ == '__main__':
    construction = json.loads((HERE/'construction.json').read_text())
    specs = construction['aperture_specs']
    result = {'method':__doc__, 'script_sha256':sha(Path(__file__).read_bytes()),
              'construction_sha256':sha((HERE/'construction.json').read_bytes()),
              'assets':[derive(name, digest, specs) for name, digest in SOURCES.items()]}
    (HERE/'GENERATED.json').write_text(json.dumps(result, indent=2)+'\n')
    for a in result['assets']:
        print(a['output'], a['output_sha256'], a['output_bytes'])
        for r in a['records']:
            print(r['node'], r['original_triangles'], '->', r['result_triangles'],
                  'affected', r.get('affected_original_triangles', 'reconstructed'))
