#!/usr/bin/env python3
"""Reproducible, dependency-free audit of every shipped asset and visual evidence file.

Does not fetch optional assets or treat a filename/reference as proof of rendering.
Exit 1 means malformed data, invalid indices/nonfinite geometry, or a lock mismatch;
degenerate/open/nonmanifold surfaces are reported (decals and kits can be open).
"""
import argparse
import collections
import hashlib
import json
import math
import pathlib
import re
import struct
import wave
import zlib

COMPONENTS = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}
FORMATS = {5120: 'b', 5121: 'B', 5122: 'h', 5123: 'H', 5125: 'I', 5126: 'f'}
SUPPORTED_EXTENSIONS = {'KHR_materials_transmission', 'KHR_materials_ior',
                        'KHR_materials_emissive_strength', 'KHR_texture_transform'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def finite_tree(obj):
    if isinstance(obj, float):
        return math.isfinite(obj)
    if isinstance(obj, list):
        return all(map(finite_tree, obj))
    if isinstance(obj, dict):
        return all(map(finite_tree, obj.values()))
    return True


def geometry(positions, triangles):
    """Topology uses source indices, not welding: split seams count as boundaries."""
    edges = collections.Counter()
    invalid = degenerate = 0
    finite = all(math.isfinite(c) for p in positions for c in p)
    bounds = None
    if positions and finite:
        bounds = [[min(p[c] for p in positions) for c in range(3)],
                  [max(p[c] for p in positions) for c in range(3)]]
    for tri in triangles:
        if any(i < 0 or i >= len(positions) for i in tri):
            invalid += 1
            continue
        a, b, c = [positions[i] for i in tri]
        u, v = [b[i] - a[i] for i in range(3)], [c[i] - a[i] for i in range(3)]
        cross = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        if sum(x*x for x in cross) <= 1e-20:
            degenerate += 1
        for i, j in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
            edges[tuple(sorted((i, j)))] += 1
    return {'vertices': len(positions), 'triangles': len(triangles),
            'finite_positions': finite, 'invalid_triangles': invalid,
            'degenerate_triangles': degenerate,
            'index_boundary_edges': sum(n == 1 for n in edges.values()),
            'index_nonmanifold_edges': sum(n > 2 for n in edges.values()),
            'local_bounds': bounds}


def audit_obj(path):
    positions, triangles, material_libraries, materials = [], [], [], set()
    normals = uvs = 0
    finite_attributes = True
    polygons = 0
    for line in path.read_text(errors='strict').splitlines():
        p = line.split()
        if not p or p[0].startswith('#'):
            continue
        if p[0] in ('v', 'vn', 'vt'):
            values = list(map(float, p[1:]))
            finite_attributes &= all(map(math.isfinite, values))
            if p[0] == 'v':
                if len(values) < 3:
                    raise ValueError('OBJ position has fewer than 3 components')
                positions.append(values[:3])
            elif p[0] == 'vn':
                normals += 1
            else:
                uvs += 1
        elif p[0] == 'f':
            polygons += 1
            ids = []
            for v in p[1:]:
                index = int(v.split('/')[0])
                ids.append(index - 1 if index > 0 else len(positions) + index)
            for j in range(1, len(ids)-1):
                triangles.append((ids[0], ids[j], ids[j+1]))
        elif p[0] == 'mtllib':
            material_libraries.extend(p[1:])
        elif p[0] == 'usemtl':
            materials.add(' '.join(p[1:]))
    return dict(geometry(positions, triangles), format='OBJ', polygons=polygons,
                normals=normals, uvs=uvs, finite_attributes=finite_attributes,
                materials=len(materials), material_libraries=material_libraries,
                missing_material_libraries=[s for s in material_libraries if not (path.parent/s).is_file()],
                runtime_material_support='OBJ loader ignores MTL; geometry-only fallback')


def audit_glb(path):
    raw = path.read_bytes()
    magic, version, size = struct.unpack_from('<III', raw)
    if (magic, version, size) != (0x46546c67, 2, len(raw)):
        raise ValueError('Invalid GLB header/length')
    doc, binary, cursor = None, b'', 12
    while cursor < size:
        length, kind = struct.unpack_from('<II', raw, cursor)
        cursor += 8
        if cursor + length > size:
            raise ValueError('GLB chunk extends past file')
        if kind == 0x4e4f534a:
            doc = json.loads(raw[cursor:cursor+length])
        elif kind == 0x004e4942:
            binary = raw[cursor:cursor+length]
        cursor += length
    if not doc:
        raise ValueError('Missing GLB JSON')
    buffers = []
    for b in doc.get('buffers', []):
        if b.get('uri'):
            raise ValueError('External/data URI GLB buffer requires explicit auditor support')
        if b['byteLength'] > len(binary):
            raise ValueError('Truncated GLB binary')
        buffers.append(binary)
    accessors = {}
    def accessor(index):
        if index in accessors:
            return accessors[index]
        a = doc['accessors'][index]
        if 'sparse' in a:
            raise ValueError('Sparse accessor requires explicit auditor support')
        view = doc['bufferViews'][a['bufferView']]
        n = COMPONENTS[a['type']]
        fmt = '<' + FORMATS[a['componentType']] * n
        width = struct.calcsize(fmt)
        stride = view.get('byteStride', width)
        start = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        end = start + max(0, a['count']-1)*stride + (width if a['count'] else 0)
        if stride < width or end > view.get('byteOffset', 0) + view['byteLength']:
            raise ValueError('Accessor exceeds buffer view')
        values = [struct.unpack_from(fmt, buffers[view['buffer']], start+i*stride) for i in range(a['count'])]
        accessors[index] = values
        return values
    aggregate = collections.Counter()
    finite_attributes = finite_tree(doc)
    primitive_details = []
    for mesh in doc.get('meshes', []):
        for primitive in mesh['primitives']:
            if primitive.get('mode', 4) != 4:
                raise ValueError('Non-triangle-list primitive')
            attributes = primitive['attributes']
            pos = accessor(attributes['POSITION'])
            for ai in attributes.values():
                finite_attributes &= all(math.isfinite(c) for p in accessor(ai) for c in p)
            ids = [v[0] for v in accessor(primitive['indices'])] if 'indices' in primitive else list(range(len(pos)))
            if len(ids) % 3:
                raise ValueError('Triangle-list count is not divisible by three')
            g = geometry(pos, list(zip(ids[::3], ids[1::3], ids[2::3])))
            for name in ('vertices', 'triangles', 'invalid_triangles', 'degenerate_triangles', 'index_boundary_edges', 'index_nonmanifold_edges'):
                aggregate[name] += g[name]
            aggregate['primitives'] += 1
            aggregate['primitives_with_uv'] += 'TEXCOORD_0' in attributes
            aggregate['primitives_with_normals'] += 'NORMAL' in attributes
            aggregate['primitives_with_tangents'] += 'TANGENT' in attributes
            finite_attributes &= g['finite_positions']
            if g['invalid_triangles'] or g['degenerate_triangles']:
                primitive_details.append({'mesh': mesh.get('name', ''), **g})
    for index in range(len(doc.get('accessors', []))):
        finite_attributes &= all(math.isfinite(c) for p in accessor(index) for c in p)
    # Compute actual default-scene bounds after node transforms. Asset files can
    # contain widely offset staging props despite an 'individual building' name.
    nodes = doc.get('nodes', [])
    parents = {}
    for ni, node in enumerate(nodes):
        for child in node.get('children', []):
            if child in parents:
                raise ValueError('glTF node has multiple parents')
            parents[child] = ni
    def local(node):
        if 'matrix' in node:
            return node['matrix']
        x, y, z, w = node.get('rotation', [0, 0, 0, 1])
        sx, sy, sz = node.get('scale', [1, 1, 1])
        tx, ty, tz = node.get('translation', [0, 0, 0])
        return [(1-2*y*y-2*z*z)*sx, (2*x*y+2*z*w)*sx, (2*x*z-2*y*w)*sx, 0,
                (2*x*y-2*z*w)*sy, (1-2*x*x-2*z*z)*sy, (2*y*z+2*x*w)*sy, 0,
                (2*x*z+2*y*w)*sz, (2*y*z-2*x*w)*sz, (1-2*x*x-2*y*y)*sz, 0,
                tx, ty, tz, 1]
    worlds = {}
    def world(index, chain=()):
        if index in chain:
            raise ValueError('glTF node cycle')
        if index not in worlds:
            a = local(nodes[index])
            if index in parents:
                b = world(parents[index], chain+(index,))
                a = [sum(b[k*4+r]*a[c*4+k] for k in range(4)) for c in range(4) for r in range(4)]
            worlds[index] = a
        return worlds[index]
    if 'scene' in doc:
        todo = list(doc['scenes'][doc['scene']]['nodes'])
    else:
        todo = [i for i in range(len(nodes)) if i not in parents]
    scene_lo, scene_hi, visited = [math.inf]*3, [-math.inf]*3, set()
    while todo:
        ni = todo.pop()
        if ni in visited:
            raise ValueError('Duplicate node in default glTF scene')
        visited.add(ni)
        node = nodes[ni]
        todo.extend(node.get('children', []))
        if 'mesh' not in node:
            continue
        m = world(ni)
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            for p in accessor(primitive['attributes']['POSITION']):
                for axis in range(3):
                    value = m[axis]*p[0]+m[4+axis]*p[1]+m[8+axis]*p[2]+m[12+axis]
                    finite_attributes &= math.isfinite(value)
                    scene_lo[axis], scene_hi[axis] = min(scene_lo[axis], value), max(scene_hi[axis], value)
    scene_bounds = [scene_lo, scene_hi] if all(map(math.isfinite, scene_lo+scene_hi)) else None
    mats = doc.get('materials', [])
    return dict(aggregate, scene_bounds=scene_bounds, format='GLB 2.0', finite_attributes=finite_attributes,
                meshes=len(doc.get('meshes', [])), nodes=len(doc.get('nodes', [])),
                materials=len(mats), embedded_images=len(doc.get('images', [])),
                animations=len(doc.get('animations', [])), skins=len(doc.get('skins', [])),
                generator=doc.get('asset', {}).get('generator'),
                alpha_modes=dict(collections.Counter(m.get('alphaMode', 'OPAQUE') for m in mats)),
                extensions_used=doc.get('extensionsUsed', []),
                extensions_ignored=sorted(set(doc.get('extensionsUsed', []))-SUPPORTED_EXTENSIONS),
                unsupported_required=sorted(set(doc.get('extensionsRequired', []))-SUPPORTED_EXTENSIONS),
                geometry_findings=primitive_details)


def audit_image(path):
    b = path.read_bytes()
    if b.startswith(b'\x89PNG\r\n\x1a\n'):
        w, h, depth, color, _, _, interlace = struct.unpack_from('>IIBBBBB', b, 16)
        channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color)
        if not channels or min(w, h) <= 0 or w*h > 100_000_000:
            raise ValueError('Invalid/oversized PNG image')
        cursor, compressed, ended = 8, bytearray(), False
        while cursor+12 <= len(b):
            length = struct.unpack_from('>I', b, cursor)[0]
            kind = b[cursor+4:cursor+8]
            end = cursor+8+length
            if end+4 > len(b):
                raise ValueError('PNG chunk is truncated')
            if zlib.crc32(b[cursor+4:end]) & 0xffffffff != struct.unpack_from('>I', b, end)[0]:
                raise ValueError('PNG chunk checksum failed')
            if kind == b'IDAT':
                compressed.extend(b[cursor+8:end])
            cursor = end+4
            if kind == b'IEND':
                ended = True
                break
        if not ended:
            raise ValueError('PNG has no end chunk')
        decompressor = zlib.decompressobj()
        expected = ((w*channels*depth+7)//8+1)*h
        decoded = decompressor.decompress(compressed, expected+1)
        if interlace:
            raise ValueError('Interlaced PNG needs explicit auditor support')
        if len(decoded) != expected or not decompressor.eof:
            raise ValueError('PNG raster length is invalid')
        return {'format': 'PNG', 'width': w, 'height': h, 'bit_depth': depth,
                'color_type': color, 'interlaced': bool(interlace),
                'chunk_checksums_valid': True, 'raster_bytes_valid': True}
    if b[:2] in (b'P3', b'P6'):
        tokens = re.finditer(rb'#[^\r\n]*|\S+', b)
        head = []
        for token in tokens:
            if token.group().startswith(b'#'):
                continue
            head.append(token.group())
            if len(head) == 4:
                end = token.end()
                break
        magic, w, h, maximum = head
        w, h, maximum = int(w), int(h), int(maximum)
        if min(w, h, maximum) <= 0:
            raise ValueError('Invalid PPM dimensions/range')
        if magic == b'P6':
            expected = w*h*3*(2 if maximum > 255 else 1)
            if len(b)-end-1 < expected:
                raise ValueError('Truncated PPM payload')
        else:
            samples = [int(t.group()) for t in tokens if not t.group().startswith(b'#')]
            if len(samples) != w*h*3 or any(c < 0 or c > maximum for c in samples):
                raise ValueError('Invalid P3 PPM payload')
        return {'format': magic.decode()+' PPM', 'width': w, 'height': h, 'maximum': maximum}
    raise ValueError('Unrecognized image (auditor supports shipped PNG/PPM)')


def audit_wav(path):
    with wave.open(str(path), 'rb') as wav:
        channels, sample_width, rate, frames, compression, _ = wav.getparams()
        data = wav.readframes(frames)
    if compression != 'NONE' or sample_width != 2:
        raise ValueError('Auditor requires shipped PCM16 audio')
    samples = [x[0] for x in struct.iter_unpack('<h', data)]
    if len(samples) != frames*channels:
        raise ValueError('Truncated WAV data')
    peak = max(map(abs, samples), default=0)/32768
    rms = math.sqrt(sum((x/32768)**2 for x in samples)/max(1, len(samples)))
    return {'format': 'PCM WAV', 'channels': channels, 'sample_bits': sample_width*8,
            'sample_rate_hz': rate, 'frames': frames, 'duration_seconds': round(frames/rate, 6),
            'peak_dbfs': round(20*math.log10(peak), 3) if peak else None,
            'rms_dbfs': round(20*math.log10(rms), 3) if rms else None,
            'clipped_samples': sum(x in (-32768, 32767) for x in samples),
            'loop_endpoint_delta': abs(samples[-1]-samples[0])/32768 if samples else 0}


def provenance(path):
    s = path.as_posix()
    if s.startswith('assets/meshes/harbor_metro/'):
        if path.name.startswith(('hm_civ_', 'hmpd_')):
            return {'claim': 'MrJaneLAB sedan base CC0; Harbor Metro additions original',
                    'evidence': ['assets/meshes/harbor_metro/HMPD_CRUISER_BASE_LICENSE.txt', 'assets/meshes/harbor_metro/README.md'],
                    'gap': 'Base asset source URL, downloaded original, exact license text/hash and derivative lineage are not recorded'}
        return {'claim': 'Original Harbor Metro content; repository MIT license',
                'evidence': ['assets/meshes/harbor_metro/README.md', 'assets/meshes/harbor_metro/PLAN_PACK_README.md', 'LICENSE'],
                'gap': 'Blender source/generation files referenced outside repository are not shipped'}
    if s.startswith('assets/textures/'):
        return {'claim': 'Original procedural textures; repository MIT license', 'evidence': ['assets/textures/README.md', 'LICENSE']}
    if s.startswith('assets/audio/'):
        return {'claim': 'Authored Meridian audio; repository MIT license', 'evidence': ['docs/MERIDIAN_AUDIO.md', 'LICENSE'],
                'gap': 'Audio generation/session sources and per-clip provenance are not shipped'}
    return {'claim': 'Repository content; MIT license (no stronger independent provenance assertion)', 'evidence': ['LICENSE']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=pathlib.Path, default=None)
    args = parser.parse_args()
    root = args.root.resolve()
    source_files = []
    for folder in ('engine/src', 'engine/include', 'apps'):
        for p in (root/folder).rglob('*'):
            if p.suffix in ('.cpp', '.hpp', '.h', '.txt') and p.is_file():
                source_files.append((p.relative_to(root).as_posix(), p.read_text(errors='replace').splitlines()))
    def references(name):
        return [{'path': p, 'line': n, 'text': line.strip()[:220]}
                for p, lines in source_files for n, line in enumerate(lines, 1) if name in line]
    # Resolve registry paths to their logical names, then look outside the
    # registry for actual calls/dispatch tables. Registration alone is not use.
    harbor_source = (root/'apps/vaultline/harbor_assets.cpp').read_text()
    registry_start = harbor_source.index('const HarborAssetDesc kAssets[]')
    registry_end = harbor_source.index('};', registry_start)+2
    registry = harbor_source[registry_start:registry_end]
    descriptors = {}
    for block in re.findall(r'\{([^{}]+)\}', registry):
        values = re.findall(r'"([^"\n]+)"|\b(nullptr)\b', block)
        values = [name if name else None for name, null in values]
        if len(values) != 4:
            continue
        name, *paths = values
        usage = references('"'+name+'"')
        usage = [r for r in usage if not (r['path'] == 'apps/vaultline/harbor_assets.cpp' and
                 registry_start <= sum(len(x)+1 for x in harbor_source.splitlines()[:r['line']-1]) < registry_end)]
        for role, path in zip(('primary_glb', 'lod_glb', 'fallback_obj'), paths):
            if path:
                descriptors['assets/meshes/'+path] = {'logical_name': name, 'role': role,
                    'dispatch_references': usage,
                    'status': 'dispatch/call evidence; runtime loading still requires verification' if usage else 'registry only; no gameplay dispatch/call found'}
    files, errors = [], []
    candidates = list((root/'assets').rglob('*'))
    # Capture images are evidence, never runtime textures/sprites.
    candidates += [p for folder in ('artifacts', 'docs/images') for p in (root/folder).rglob('*')
                   if p.suffix.lower() in ('.png', '.ppm', '.jpg', '.jpeg', '.blend')]
    for path in sorted(p for p in candidates if p.is_file()):
        relative = path.relative_to(root)
        record = {'path': relative.as_posix(), 'bytes': path.stat().st_size,
                  'sha256': sha(path), 'provenance': provenance(relative),
                  'source_references': references(path.name)}
        if relative.as_posix() in descriptors:
            record['harbor_registry'] = descriptors[relative.as_posix()]
        ext = path.suffix.lower()
        if relative.parts[0] != 'assets':
            record['usage'] = 'visual-evidence-only; not a runtime asset'
        elif record['source_references']:
            record['usage'] = 'source-referenced; loading/rendering requires runtime verification'
        elif ext == '.mtl':
            record['usage'] = 'authoring/OBJ companion; runtime OBJ loader ignores MTL'
        elif ext == '.ppm':
            record['usage'] = 'texture-slot sibling fallback (extension constructed at runtime)'
        elif ext in ('.md', '.txt', '.json'):
            record['usage'] = 'documentation/license/optional-download manifest'
        else:
            record['usage'] = 'no literal source reference; inspect dynamic names and kit embedding'
        try:
            if ext == '.glb': record['details'] = audit_glb(path)
            elif ext == '.obj': record['details'] = audit_obj(path)
            elif ext in ('.png', '.ppm'): record['details'] = audit_image(path)
            elif ext == '.wav': record['details'] = audit_wav(path)
            elif ext == '.mtl':
                lines = path.read_text().splitlines()
                record['details'] = {'format': 'Wavefront MTL', 'materials': sum(x.startswith('newmtl ') for x in lines),
                                     'texture_maps': [x for x in lines if x.startswith(('map_', 'bump ', 'norm '))]}
            details = record.get('details', {})
            if details.get('invalid_triangles') or not details.get('finite_positions', True) or not details.get('finite_attributes', True):
                raise ValueError('Invalid/nonfinite geometry')
        except (ValueError, KeyError, IndexError, struct.error, OSError, wave.Error, zlib.error) as e:
            record['error'] = str(e)
            errors.append({'path': str(relative), 'error': str(e)})
        files.append(record)
    lock = json.loads((root/'assets/render-assets.lock.json').read_text())
    optional = []
    for asset in lock['assets']:
        entry = {'id': asset['id'], 'source': asset['source'], 'license': asset['license'], 'files': []}
        for f in asset['files']:
            p = root/'out/assets'/f['path']
            result = {'path': f['path'], 'expected_bytes': f['bytes'], 'expected_sha256': f['sha256'], 'present': p.is_file()}
            if p.is_file():
                result['lock_matches'] = p.stat().st_size == f['bytes'] and sha(p) == f['sha256']
                if not result['lock_matches']:
                    errors.append({'path': str(p.relative_to(root)), 'error': 'Optional asset differs from lock'})
            entry['files'].append(result)
        optional.append(entry)
    missing = []
    for match in re.finditer(r'"(harbor_metro/[^"\n]+\.(?:glb|obj))"', (root/'apps/vaultline/harbor_assets.cpp').read_text()):
        if not (root/'assets/meshes'/match.group(1)).is_file():
            missing.append(match.group(1))
    extension_counts = collections.Counter(pathlib.Path(x['path']).suffix.lower() for x in files)
    report = {'schema': 1, 'method': 'stdlib parser; raw local primitive topology (no positional welding); all accessor attributes finite-checked; source references are not proof of rendering',
              'summary': {'files': len(files), 'bytes': sum(x['bytes'] for x in files), 'extensions': dict(sorted(extension_counts.items())),
                          'sprite_named_files': sum('sprite' in x['path'].lower() for x in files), 'missing_harbor_descriptor_paths': sorted(set(missing)), 'errors': len(errors)},
              'files': files, 'optional_assets': optional, 'errors': errors}
    output = args.output or root/'docs/validation/asset-audit.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True)+'\n')
    print(json.dumps(report['summary'], indent=2))
    print('Report:', output)
    return 1 if errors else 0


if __name__ == '__main__':
    raise SystemExit(main())
