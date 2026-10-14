#!/usr/bin/env python3
"""Read-only lip-seam/weight diagnostic; only Vale has reviewed pixel proof here."""
from pathlib import Path
import argparse
import hashlib
import json
import re
from audit_vale_source import array
ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def measure(label, source, start=None, span=None):
    actor = label.split('-')[0]
    header = ROOT / f'engine/include/fury/generated/{actor}_anatomy.hpp'
    points = array(header, 'positions')
    uv = array(header, 'uv_coordinates')
    indices = array(header, 'indices')
    uv_indices = array(header, 'uv_indices')
    source_indices = array(header, 'source_vertex_indices')
    if start is None:
        match = re.search(r'wt\(B::Head, B::Neck, smooth\(\((?:position\.y|y) - ([.0-9]+)f\) / ([.0-9]+)f\)\)', source.read_text())
        if not match:
            raise ValueError(f'Unrecognized current weight expression: {source}')
        start, span = map(float, match.groups())
    external, interior = set(), set()
    for offset in range(0, len(indices), 3):
        tex = uv[uv_indices[offset]]
        if tex[0] > .64 and .14869 <= tex[1] <= .81764:
            external.update(indices[offset:offset+3])
        if .75 < tex[0] < .93 and tex[1] < .13645:
            interior.update(indices[offset:offset+3])
    seam = sorted(external & interior)
    if len(seam) != 38:
        raise ValueError(f'Unexpected source mouth seam count: {actor}: {len(seam)}')
    vertices = []
    for index in seam:
        t = max(0., min(1., (points[index][1] - start) / span))
        vertices.append({'generated_position_index': index,
                         'original_obj_vertex_index': source_indices[index],
                         'position': points[index], 'head_weight': t*t*(3-2*t)})
    weights = [v['head_weight'] for v in vertices]
    return {'actor': label, 'source_header': str(header.relative_to(ROOT)),
            'source_header_sha256': sha(header),
            'runtime_source': str(source.relative_to(ROOT)), 'runtime_source_sha256': sha(source),
            'lip_source_vertex_count': len(vertices),
            'head_weight_transition': [start, start+span],
            'head_weight_range': [min(weights), max(weights)],
            'lip_vertices_below_full_head_weight': sum(w < .999999 for w in weights),
            'vertices': vertices}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT/'docs/characters/vale/ATTENTION_SOURCE_RISK.json')
    args = parser.parse_args()
    rows = [measure(name, ROOT/f'engine/src/character_{name}.cpp')
            for name in ['mira', 'jon', 'tessa', 'owen', 'lia']]
    previous = ROOT/'out/vale/recovery-snapshot/character_vale.cpp'
    if previous.exists():
        rows.append(measure('vale-before', previous))
    rows.append(measure('vale-after', ROOT/'engine/src/character_vale.cpp'))
    report = {'method': 'Read-only exact canonical seam intersection between external-head and mouth-interior charts; threshold parsed from each recorded runtime source. Graphical indices are zero based. Only Vale has an observed rendered defect in this report. Peer weights identify review candidates, not visual defect severity. No other actor modified.',
              'script_sha256': sha(Path(__file__)), 'rows': rows}
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    for row in rows:
        print(row['actor'], row['head_weight_range'], row['lip_vertices_below_full_head_weight'])
if __name__ == '__main__':
    main()
