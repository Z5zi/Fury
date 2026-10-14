#!/usr/bin/env python3
"""Bake Jon Keel's distinct CC0 anatomical head from pinned graphical data.

This is Fury's own standard-library importer, reusing its original OBJ/math
helpers. No MPFB addon program logic is imported, copied, installed, or executed.
The reviewable recipe and separate manifest live in third_party/jon_anatomy;
the unchanged shared CC0 base OBJ, metadata and licenses are referenced in place.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import sys

from build_mira_anatomy import (
    array, bounds, cpp_float, cross_length_squared, finite, float32,
    group_ids, mean, read_obj, require, sha256,
)

REPO = Path(__file__).resolve().parents[1]
ASSETS = REPO / "third_party/jon_anatomy"
SHARED = REPO / "third_party/mpfb_assets"
HEADER = REPO / "engine/include/fury/generated/jon_anatomy.hpp"
REPORT = ASSETS / "GENERATED_STATS.json"
PINNED_COMMIT = "d0a32e57a7f915cb2f2b95410e2117648c7bbb7e"


def verified_inputs():
    manifest = json.loads((ASSETS / "MANIFEST.json").read_text())
    require(manifest["commit"] == PINNED_COMMIT, "Unexpected upstream commit")
    require(manifest["asset_license"] == "CC0-1.0", "Unexpected source-data license")
    seen = set()
    for category, root in (("shared_files", REPO), ("files", ASSETS)):
        for item in manifest[category]:
            path = (root / item["path"]).resolve()
            require(path.is_relative_to(root), f"Source path escapes asset root: {path}")
            require(path not in seen, f"Duplicate source: {path}")
            seen.add(path)
            require(sha256(path) == item["sha256"], f"Source hash mismatch: {path}")
            require(item["url"] == f'https://raw.githubusercontent.com/makehumancommunity/mpfb2/{PINNED_COMMIT}/{item["upstream_path"]}', f"Unpinned URL: {path}")
            if category == "files":
                data = path.read_bytes()
                require(len(data) == item["bytes"], f"Source size mismatch: {path}")
                blob_sha1 = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
                require(blob_sha1 == item["git_blob_sha1"], f"Git blob mismatch: {path}")
    required = {SHARED / name for name in ("base.obj", "LICENSE.md", "LICENSE.ASSETS.md", "metadata/basemesh_vertex_groups.json")}
    require(required.issubset(seen), "Shared inputs are not all pinned")
    return manifest


def apply_targets(base, recipe, manifest):
    positions = [list(v) for v in base]
    pinned_paths = {item["path"] for item in manifest["files"]}
    target_counts = {}
    for item in recipe["targets"]:
        name, weight = item["path"], float(item["weight"])
        require(name in pinned_paths, f"Unpinned target: {name}")
        require(name not in target_counts, f"Duplicate recipe target: {name}")
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


def surface_sections(positions, indices, heights):
    """Exact linear triangle/plane intersections, useful for collar/hair fitting."""
    sections = []
    for height in heights:
        intersections = set()
        for start in range(0, len(indices), 3):
            triangle = [positions[i] for i in indices[start:start + 3]]
            for a, b in zip(triangle, triangle[1:] + triangle[:1]):
                if (a[1] <= height < b[1]) or (b[1] <= height < a[1]):
                    t = (height - a[1]) / (b[1] - a[1])
                    point = tuple(a[k] + t * (b[k] - a[k]) for k in range(3))
                    intersections.add(point)
        require(intersections, f"No skin intersects fitting plane y={height}")
        low, high = bounds(intersections)
        sections.append({"y": height, "bounds_min": low, "bounds_max": high,
                         "width_x": high[0] - low[0], "depth_z": high[2] - low[2]})
    return sections


def build(recipe, manifest):
    base, source_uvs, faces = read_obj(SHARED / "base.obj")
    morphed, target_counts = apply_targets(base, recipe, manifest)
    body_ids = group_ids(faces, recipe["selection"]["obj_group"])
    metadata = json.loads((SHARED / "metadata/basemesh_vertex_groups.json").read_text())
    for group in ("body", "joint-l-eye", "joint-r-eye", "joint-mouth", "joint-neck", "joint-head", "joint-head-2",
                  "joint-jaw", "joint-l-upperlid", "joint-r-upperlid", "joint-l-lowerlid", "joint-r-lowerlid",
                  "helper-l-eye", "helper-r-eye"):
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
    eye_half_extents, eye_helper_bounds = [], []
    for side in ("l", "r"):
        lo, hi = bounds([normalized[i] for i in group_ids(faces, f"helper-{side}-eye")])
        eye_half_extents.append([(hi[a] - lo[a]) * 0.5 for a in range(3)])
        eye_helper_bounds.append({"side": side, "bounds_min": lo, "bounds_max": hi})

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
    require(degenerate_count == 0, "Pinned Jon recipe must retain every selected source triangle")
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
    edges, edge_orientation, neighbors = {}, {}, {i: set() for i in range(len(positions))}
    for i in range(0, len(indices), 3):
        a, b, c = indices[i:i + 3]
        for first, second in ((a, b), (b, c), (c, a)):
            edge = tuple(sorted((first, second)))
            edges[edge] = edges.get(edge, 0) + 1
            edge_orientation[edge] = edge_orientation.get(edge, 0) + (1 if first < second else -1)
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
    inconsistent_edges = sum(count == 2 and edge_orientation[edge] != 0 for edge, count in edges.items())
    require(inconsistent_edges == 0, "Inconsistently wound source triangles")
    boundary_neighbors = {}
    for (first, second), count in edges.items():
        if count == 1:
            boundary_neighbors.setdefault(first, set()).add(second)
            boundary_neighbors.setdefault(second, set()).add(first)
    require(all(len(n) == 2 for n in boundary_neighbors.values()), "Attachment boundary branches")
    boundary_unseen, boundary_loops = set(boundary_neighbors), 0
    while boundary_unseen:
        boundary_loops += 1
        todo = [boundary_unseen.pop()]
        while todo:
            for neighbor in boundary_neighbors[todo.pop()]:
                if neighbor in boundary_unseen:
                    boundary_unseen.remove(neighbor)
                    todo.append(neighbor)
    require(boundary_loops == 1, "Expected one continuous neck/upper-shoulder attachment seam")
    head_band = bounds([p for p in positions if p[1] >= 0.375])
    attachment_band = bounds([p for p in positions if p[1] < 0.375])
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
        "eye_helper_bounds": eye_helper_bounds,
        "head_band_y_ge_0_375": {"bounds_min": head_band[0], "bounds_max": head_band[1]},
        "neck_and_upper_shoulder_band_y_lt_0_375": {"bounds_min": attachment_band[0], "bounds_max": attachment_band[1]},
        "surface_cross_sections": surface_sections(positions, indices, (0.33, 0.34, 0.345, 0.35, 0.36, 0.375, 0.40, 0.43, 0.46, 0.485)),
        "boundary_edge_count": sum(count == 1 for count in edges.values()),
        "boundary_loop_count": boundary_loops,
        "inconsistently_wound_shared_edge_count": inconsistent_edges,
        "non_manifold_edge_count": sum(count > 2 for count in edges.values()),
        "connected_component_vertex_counts": component_sizes,
        "source_indices": "All source vertex/UV/face indices are zero based; face indices count every OBJ f line.",
        "lod": "Original retained face topology is shared. No simplification or detail-destroying decimation is applied.",
    }
    header = [
        "// Generated by scripts/build_jon_anatomy.py; do not edit by hand.",
        "// Geometry derived from pinned CC0-1.0 MPFB graphical assets, not addon code.",
        "// See docs/characters/jon/ANATOMY_PROVENANCE.md and third_party/jon_anatomy.",
        "#pragma once", "", '#include "fury/math.hpp"', "#include <array>", "#include <cstdint>", "",
        "namespace fury::jon_anatomy {", "",
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
                   "", "}  // namespace fury::jon_anatomy", ""])
    result = "\n".join(header)
    report["generated_header_sha256"] = hashlib.sha256(result.encode()).hexdigest()
    return result, json.dumps(report, indent=2) + "\n", report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify source hashes and exact generated outputs without writing")
    parser.add_argument("--recipe", type=Path, default=ASSETS / "jon_recipe.json")
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
    print(f"Jon anatomy {'verified' if args.check else 'generated'}: {report['vertex_count']} vertices, "
          f"{report['triangle_count']} triangles, {report['removed_degenerate_triangle_count']} degenerate triangles removed; "
          f"{report['boundary_edge_count']} boundary edges")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
