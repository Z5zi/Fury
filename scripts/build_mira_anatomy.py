#!/usr/bin/env python3
"""Bake Mira's CC0 anatomical head with an original, standard-library-only importer.

No MPFB program code is imported or executed. The pinned mesh and morph deltas
are data; their hashes and the reviewable recipe live in third_party/mpfb_assets.
Run from any directory, or use --check to verify the committed generated output.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import math
from pathlib import Path
import struct
import sys


REPO = Path(__file__).resolve().parents[1]
ASSETS = REPO / "third_party/mpfb_assets"
HEADER = REPO / "engine/include/fury/generated/mira_anatomy.hpp"
REPORT = ASSETS / "GENERATED_STATS.json"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verified_inputs():
    manifest = json.loads((ASSETS / "MANIFEST.json").read_text())
    for item in manifest["files"]:
        path = ASSETS / item["path"]
        require(sha256(path) == item["sha256"], f"Source hash mismatch: {path}")
    return manifest


def finite(values, context):
    require(all(math.isfinite(x) for x in values), f"Non-finite value: {context}")


def read_obj(path):
    positions, uv, faces = [], [], []
    groups = ()
    for line_number, line in enumerate(path.read_text().splitlines(), 1):
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        command, *args = fields
        if command in ("v", "vt"):
            count = 3 if command == "v" else 2
            require(len(args) >= count, f"Incomplete {command}, line {line_number}")
            values = tuple(float(x) for x in args[:count])
            finite(values, f"OBJ line {line_number}")
            (positions if command == "v" else uv).append(values)
        elif command == "g":
            groups = tuple(args)
        elif command == "f":
            # The pinned file contains quads; support triangles too, but do not
            # silently fan-triangulate potentially concave larger polygons.
            require(len(args) in (3, 4), f"Unsupported polygon, line {line_number}")
            corners = []
            for arg in args:
                parts = arg.split("/")
                require(len(parts) >= 2 and parts[1], f"Missing UV, line {line_number}")
                vertex, texture = int(parts[0]), int(parts[1])
                vertex = vertex - 1 if vertex > 0 else len(positions) + vertex
                texture = texture - 1 if texture > 0 else len(uv) + texture
                require(0 <= vertex < len(positions), f"Invalid vertex, line {line_number}")
                require(0 <= texture < len(uv), f"Invalid UV, line {line_number}")
                corners.append((vertex, texture))
            faces.append((groups, tuple(corners)))
    require(positions and uv and faces, "Empty OBJ")
    return positions, uv, faces


def group_ids(faces, name):
    result = sorted({v for groups, corners in faces if name in groups for v, _ in corners})
    require(result, f"Missing OBJ group: {name}")
    return result


def apply_targets(base, recipe, manifest):
    positions = [list(v) for v in base]
    pinned_paths = {item["path"] for item in manifest["files"]}
    target_counts = {}
    for item in recipe["targets"]:
        name, weight = item["path"], float(item["weight"])
        require(name in pinned_paths, f"Unpinned target: {name}")
        finite([weight], name)
        seen = set()
        with gzip.open(ASSETS / name, "rt", encoding="utf-8") as source:
            for line_number, line in enumerate(source, 1):
                fields = line.split("#", 1)[0].split()
                if not fields:
                    continue
                require(len(fields) == 4, f"Malformed target: {name}:{line_number}")
                index, *delta = [int(fields[0]), *map(float, fields[1:])]
                require(0 <= index < len(positions), f"Invalid target vertex: {name}:{line_number}")
                require(index not in seen, f"Duplicate target vertex: {name}:{line_number}")
                finite(delta, f"{name}:{line_number}")
                seen.add(index)
                for axis in range(3):
                    positions[index][axis] += weight * delta[axis]
        target_counts[name] = len(seen)
    return positions, target_counts


def bounds(points):
    return ([min(p[a] for p in points) for a in range(3)],
            [max(p[a] for p in points) for a in range(3)])


def mean(points):
    return [sum(p[a] for p in points) / len(points) for a in range(3)]


def cross_length_squared(a, b, c):
    u, v = [b[i] - a[i] for i in range(3)], [c[i] - a[i] for i in range(3)]
    cross = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    return sum(x * x for x in cross)


def float32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def cpp_float(x):
    # Nine significant digits round-trip a binary32; sanitize negative zero.
    value = format(float32(x) if x != 0 else 0, ".9g")
    if "." not in value and "e" not in value:
        value += ".0"
    return value + "f"


def array(name, type_name, values, vector=False):
    qualifier = "const" if type_name in ("Vec3", "Vec2") else "constexpr"
    lines = [f"inline {qualifier} std::array<{type_name}, {len(values)}> {name} = {{{{"]
    if vector:
        lines.extend("  {" + ", ".join(cpp_float(x) for x in v) + "}," for v in values)
    else:
        for start in range(0, len(values), 18):
            lines.append("  " + ", ".join(str(v) + "u" for v in values[start:start + 18]) + ",")
    lines.append("}};")
    return "\n".join(lines)


def build(recipe, manifest):
    base, source_uvs, faces = read_obj(ASSETS / "base.obj")
    morphed, target_counts = apply_targets(base, recipe, manifest)
    body_ids = group_ids(faces, recipe["selection"]["obj_group"])
    metadata = json.loads((ASSETS / "metadata/basemesh_vertex_groups.json").read_text())
    for group in ("body", "joint-l-eye", "joint-r-eye", "joint-mouth", "joint-neck", "joint-head", "joint-head-2"):
        metadata_ids = sorted({i for first, last in metadata[group] for i in range(first, last + 1)})
        require(metadata_ids == group_ids(faces, group), f"OBJ/metadata disagreement: {group}")
    source_min, source_max = bounds([morphed[i] for i in body_ids])
    normalization = recipe["normalization"]
    feet, crown = normalization["body_feet_y"], normalization["body_crown_y"]
    finite([feet, crown, normalization["center_x"], normalization["center_z"]], "normalization")
    require(crown > feet and source_max[1] > source_min[1], "Invalid height normalization")
    scale = (crown - feet) / (source_max[1] - source_min[1])
    # A single uniform scale preserves anatomical proportions. MPFB's source
    # axes already match Fury: +X anatomical left, +Y up, +Z face/front.
    normalized = [((p[0] - normalization["center_x"]) * scale,
                   (p[1] - source_min[1]) * scale + feet,
                   (p[2] - normalization["center_z"]) * scale) for p in morphed]
    landmarks = {}
    for name, group in (("left_eye", "joint-l-eye"), ("right_eye", "joint-r-eye"),
                        ("mouth", "joint-mouth"), ("neck", "joint-neck"),
                        ("head", "joint-head"), ("crown_joint", "joint-head-2"),
                        ("jaw", "joint-jaw"), ("left_upper_lid", "joint-l-upperlid"),
                        ("left_lower_lid", "joint-l-lowerlid"),
                        ("right_upper_lid", "joint-r-upperlid"),
                        ("right_lower_lid", "joint-r-lowerlid")):
        landmarks[name] = mean([normalized[i] for i in group_ids(faces, group)])
    eye_half_extents = []
    for side in ("l", "r"):
        lo, hi = bounds([normalized[i] for i in group_ids(faces, f"helper-{side}-eye")])
        eye_half_extents.append([(hi[a] - lo[a]) * 0.5 for a in range(3)])

    cutoff = float(recipe["selection"]["minimum_face_y"])
    epsilon = float(recipe["degenerate_cross_length_squared_max"])
    finite([cutoff, epsilon], "selection")
    require(epsilon >= 0, "Negative degenerate threshold")
    triangles, source_faces, degenerate_count, selected_faces, candidate_count = [], [], 0, 0, 0
    for face_index, (groups, corners) in enumerate(faces):
        if recipe["selection"]["obj_group"] not in groups:
            continue
        if any(normalized[v][1] < cutoff for v, _ in corners):
            continue
        selected_faces += 1
        for offset in range(1, len(corners) - 1):
            triangle = (corners[0], corners[offset], corners[offset + 1])
            candidate_count += 1
            # Check the actual binary32 vertices emitted to the runtime too.
            points = [[float32(x) for x in normalized[v]] for v, _ in triangle]
            if cross_length_squared(*points) <= epsilon:
                degenerate_count += 1
                continue
            triangles.append(triangle)
            source_faces.append(face_index)
    require(triangles, "Selection contains no triangles")
    vertex_ids = sorted({v for triangle in triangles for v, _ in triangle})
    uv_ids = sorted({uv for triangle in triangles for _, uv in triangle})
    vertex_map, uv_map = {v: i for i, v in enumerate(vertex_ids)}, {v: i for i, v in enumerate(uv_ids)}
    positions = [normalized[v] for v in vertex_ids]
    indices = [vertex_map[v] for triangle in triangles for v, _ in triangle]
    uvs = [source_uvs[i] for i in uv_ids]
    uv_indices = [uv_map[uv] for triangle in triangles for _, uv in triangle]
    low, high = bounds(positions)
    # The face plane cut intentionally leaves an open attachment seam; count
    # edges and components so any later accidental topology change is visible.
    edges, neighbors = {}, {i: set() for i in range(len(positions))}
    for i in range(0, len(indices), 3):
        a, b, c = indices[i:i + 3]
        for first, second in ((a, b), (b, c), (c, a)):
            edge = tuple(sorted((first, second)))
            edges[edge] = edges.get(edge, 0) + 1
            neighbors[first].add(second)
            neighbors[second].add(first)
    unseen, component_sizes = set(neighbors), []
    while unseen:
        todo, count = [min(unseen)], 0
        unseen.remove(todo[0])
        while todo:
            current = todo.pop()
            count += 1
            for neighbor in neighbors[current]:
                if neighbor in unseen:
                    unseen.remove(neighbor)
                    todo.append(neighbor)
        component_sizes.append(count)
    require(len(component_sizes) == 1, "Head selection unexpectedly has disconnected components")
    require(all(count <= 2 for count in edges.values()), "Non-manifold generated edge")
    report = {
        "upstream_commit": manifest["commit"],
        "recipe": recipe,
        "source_vertex_count": len(base), "source_face_count": len(faces),
        "source_body_vertex_count": len(body_ids), "target_delta_counts": target_counts,
        "source_morphed_body_bounds_min": source_min, "source_morphed_body_bounds_max": source_max,
        "uniform_scale": scale, "selected_source_face_count": selected_faces,
        "candidate_triangle_count": candidate_count, "removed_degenerate_triangle_count": degenerate_count,
        "vertex_count": len(positions), "triangle_count": len(triangles), "uv_count": len(uvs),
        "bounds_min": low, "bounds_max": high, "landmarks": landmarks,
        "eye_helper_half_extents": eye_half_extents,
        "boundary_edge_count": sum(count == 1 for count in edges.values()),
        "non_manifold_edge_count": sum(count > 2 for count in edges.values()),
        "connected_component_vertex_counts": component_sizes,
        "source_indices": "All source vertex/UV/face indices are zero based; face indices count every OBJ f line.",
        "lod": "Original retained face topology is shared. No simplification or detail-destroying decimation is applied.",
    }
    header = [
        "// Generated by scripts/build_mira_anatomy.py; do not edit by hand.",
        "// Geometry derived from pinned CC0-1.0 MPFB graphical assets, not addon code.",
        "// See docs/characters/mira/ANATOMY_PROVENANCE.md and third_party/mpfb_assets.",
        "#pragma once", "", '#include "fury/math.hpp"', "#include <array>", "#include <cstdint>", "",
        "namespace fury::mira_anatomy {", "",
        f'inline constexpr char source_commit[] = "{manifest["commit"]}";',
        "// Apply all weighted target deltas to the OBJ before normalization.",
        "// Original axes: +X anatomical left, +Y up, +Z face/front.",
        f"// Morphed body foot floor={source_min[1]:.12g}; crown={source_max[1]:.12g}; height={source_max[1] - source_min[1]:.12g}.",
        f"// s=({crown:.12g}-({feet:.12g})) / ({source_max[1]:.12g}-({source_min[1]:.12g}))={scale:.17g}.",
        f"// Fury position=((source.x-({normalization['center_x']:.12g}))*s, (source.y-({source_min[1]:.12g}))*s+({feet:.12g}), (source.z-({normalization['center_z']:.12g}))*s).",
        "// Full normalized body feet=-0.5 and crown=0.493; hair reserve=0.007.",
        f"inline constexpr float source_body_floor_y = {cpp_float(source_min[1])};",
        f"inline constexpr float source_body_height = {cpp_float(source_max[1] - source_min[1])};",
        f"inline constexpr float normalization_scale = {cpp_float(scale)};",
        f"inline constexpr std::uint32_t vertex_count = {len(positions)}u;",
        f"inline constexpr std::uint32_t triangle_count = {len(triangles)}u;",
        f"inline constexpr std::uint32_t removed_degenerate_triangle_count = {degenerate_count}u;",
        f"inline constexpr float neck_cutoff_y = {cpp_float(cutoff)};", "",
        array("positions", "Vec3", positions, True), "", array("indices", "std::uint32_t", indices), "",
        "// Source OBJ UV seams remain distinct; each triangle corner uses its own UV index.",
        array("uv_coordinates", "Vec2", uvs, True), "", array("uv_indices", "std::uint32_t", uv_indices), "",
        array("source_vertex_indices", "std::uint32_t", vertex_ids), "",
        array("source_uv_indices", "std::uint32_t", uv_ids), "",
        array("triangle_source_faces", "std::uint32_t", source_faces), "",
        "// Landmark means are calculated after all morphs, before helpers are discarded.",
        "// Eye order: anatomical left (+X), anatomical right (-X).",
        array("eye_centers", "Vec3", [landmarks["left_eye"], landmarks["right_eye"]], True),
        array("eye_helper_half_extents", "Vec3", eye_half_extents, True),
        "// The mouth joint is internal: it is not a lip surface or lipstick center.",
    ]
    for cpp_name, value in (("mouth_center", landmarks["mouth"]), ("neck_center", landmarks["neck"]),
                            ("head_center", landmarks["head"]), ("crown_joint", landmarks["crown_joint"]),
                            ("jaw_center", landmarks["jaw"]), ("bounds_min", low), ("bounds_max", high)):
        header.append(f"inline const Vec3 {cpp_name}{{" + ", ".join(cpp_float(v) for v in value) + "};")
    header.extend([array("upper_lid_centers", "Vec3", [landmarks["left_upper_lid"], landmarks["right_upper_lid"]], True),
                   array("lower_lid_centers", "Vec3", [landmarks["left_lower_lid"], landmarks["right_lower_lid"]], True),
                   "", "}  // namespace fury::mira_anatomy", ""])
    result = "\n".join(header)
    report["generated_header_sha256"] = hashlib.sha256(result.encode()).hexdigest()
    return result, json.dumps(report, indent=2) + "\n", report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify source hashes and exact generated outputs without writing")
    parser.add_argument("--recipe", type=Path, default=ASSETS / "mira_recipe.json")
    args = parser.parse_args()
    manifest = verified_inputs()
    recipe = json.loads(args.recipe.read_text())
    header, report_text, report = build(recipe, manifest)
    for path, expected in ((HEADER, header), (REPORT, report_text)):
        if args.check:
            require(path.exists() and path.read_text() == expected, f"Stale generated output: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(expected)
    print(f"Mira anatomy {'verified' if args.check else 'generated'}: {report['vertex_count']} vertices, "
          f"{report['triangle_count']} triangles, {report['removed_degenerate_triangle_count']} degenerate triangles removed; "
          f"{report['boundary_edge_count']} boundary edges")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
