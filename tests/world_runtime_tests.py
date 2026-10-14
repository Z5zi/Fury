#!/usr/bin/env python3
"""Exercise Vaultline's real six-district builder and GPU-free render paths.

This is a process-level regression, not a substitute scene or source-text test.
No third-party Python modules are required. See docs/WORLD_TEST_PLAN.md.
"""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time


DISTRICTS = ("metro", "ridge", "ashcourt", "depot", "loft", "quay")
CATEGORIES = ("architecture", "ground", "life", "water")
VIEWS = ("metro-wide", "metro-street", "ridge", "ridge-street", "ashcourt",
         "market-street", "depot", "loft", "quay", "waterfront", "world-overview")
COUNT_KEYS = ("entities", "visible_entities", "triangles", "unique_mesh_triangles",
              "solids", "tagged_entities", "invalid_indices", "nonfinite_vertices",
              "degenerate_triangles", "zero_area_triangles", "nonfinite_instances")
VALIDATION = re.compile(
    r"Rendering validation: frames=(\d+) provider=(.*?) triangles=(\d+) "
    r"cpu_ms=([0-9.eE+-]+) accumulated=(\d+) errors=(\d+)")


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def strict_json(path):
    def reject(value):
        raise AssertionError(f"Nonfinite JSON constant in {path}: {value}")
    return json.loads(path.read_text(encoding="utf-8"), parse_constant=reject)


def integer(value):
    return type(value) is int and value >= 0


def verify_audit(audit, enabled):
    require(audit.get("schema") == 1, "Unsupported/missing world audit schema")
    require(audit.get("enabled") is enabled, "FURY_WORLD_ART state was not honored")
    for stage in ("before", "after"):
        fingerprint = audit.get(stage + "_geometry_fingerprint")
        require(isinstance(fingerprint, str) and re.fullmatch(r"[0-9a-fA-F]{16}", fingerprint),
                f"Missing deterministic {stage} geometry/layout fingerprint")
    for stage in ("before", "after"):
        counts = audit[stage]
        for key in COUNT_KEYS:
            require(integer(counts.get(key)), f"{stage}.{key} must be a nonnegative integer")
        require(counts["entities"] >= 100, "Audit did not build the actual populated world")
        require(0 < counts["visible_entities"] <= counts["entities"], "Invalid visibility count")
        require(0 < counts["unique_mesh_triangles"] <= counts["triangles"], "Invalid mesh counts")
        require(0 < counts["solids"] <= counts["entities"], "Missing original collision world")
        require(counts["tagged_entities"] <= counts["entities"], "Invalid tag count")
        require(counts["zero_area_triangles"] <= counts["degenerate_triangles"],
                "Exact-zero triangle count exceeds the near-zero diagnostic count")
    require(audit.get("original_entities_preserved") is True,
            "World art changed original names/tags/transforms/colliders")
    require(audit.get("added_solids") == 0, "Decorative art introduced gameplay collision")
    protected = audit["protected_entities"]
    require(len(protected) == audit["before"]["entities"], "Protection list omits original entities")
    for entity in protected:
        require(isinstance(entity["name"], str) and isinstance(entity["tag"], str), "Invalid identity")
        require(type(entity["solid"]) is bool, "Invalid solid flag")
        for key in ("position", "rotation", "scale", "collider_center", "collider_half_extents"):
            vector = entity[key]
            require(isinstance(vector, list) and len(vector) == 3 and
                    all(isinstance(v, (int, float)) and math.isfinite(v) for v in vector),
                    f"Invalid protected transform/collider: {entity['name']}.{key}")
    # These anchors distinguish the real complete world from a small audit fixture.
    names = {entity["name"] for entity in protected}
    for anchor in ("StreetGrid", "BankWallN", "MetroBridge", "AshcourtPlaza",
                   "DepotYard", "LoftWallN", "NorthQuayPlaza", "NQWarehouse0"):
        require(anchor in names, f"Full-world audit lacks {anchor}")
    require(sum(e["solid"] for e in protected) == audit["before"]["solids"],
            "Protected collider count does not match the original world")
    coverage = audit["coverage"]
    for category in CATEGORIES:
        require(category in coverage and isinstance(coverage[category], dict),
                f"Missing coverage category {category}")
        require(all(integer(value) for value in coverage[category].values()),
                f"Invalid coverage values in {category}")
        if enabled:
            require(sum(coverage[category].values()) > 0, f"No {category} art was built")
        else:
            require(not any(coverage[category].values()), f"Disabled {category} still adds art")
    if enabled:
        for district in DISTRICTS:
            require(sum(coverage[category].get(district, 0) for category in CATEGORIES) > 0,
                    f"No new art coverage in district {district}")
        water = coverage["water"]
        require(water.get("updated_surfaces") == 3, "Expected all three existing water surfaces to be upgraded")
        require(water.get("added_triangles") == 0, "Water-map upgrade unexpectedly adds geometry")
        require(0 < water.get("shared_map_bytes", 0) <= 2 * 1024 * 1024, "Water maps exceed their shared memory budget")
        for district in ("metro", "ridge", "quay"):
            require(water.get(district, 0) > 0, f"Water upgrade missing in {district}")
    before, after = audit["before"], audit["after"]
    for key in ("solids", "invalid_indices", "nonfinite_vertices", "degenerate_triangles",
                "zero_area_triangles", "nonfinite_instances"):
        require(after[key] == before[key], f"New world art regressed {key}: {before[key]} -> {after[key]}")
    if not enabled:
        require(before == after, "Disabled world-art build changed world statistics")
        require(audit["before_geometry_fingerprint"] == audit["after_geometry_fingerprint"],
                "Disabled world art changed geometry/layout despite equal counters")
    else:
        require(audit["before_geometry_fingerprint"] != audit["after_geometry_fingerprint"],
                "Enabled world art did not change the scene fingerprint")
        added_entities = after["entities"] - before["entities"]
        added_triangles = after["triangles"] - before["triangles"]
        require(0 < added_entities <= 1200, f"Entity budget exceeded: {added_entities} added, cap 1200")
        require(0 < added_triangles <= 125000,
                f"Triangle budget exceeded: {added_triangles} added, cap 125000")
        require(after["visible_entities"] > before["visible_entities"], "New art is entirely hidden")
        require(after["unique_mesh_triangles"] >= before["unique_mesh_triangles"],
                "World art unexpectedly removed original meshes")


def read_ppm(path, width, height):
    data = path.read_bytes()
    # The application writes this exact simple P6 form; do not trim pixel bytes.
    parts = data.split(b"\n", 3)
    require(len(parts) == 4 and parts[0] == b"P6" and parts[2] == b"255", f"Invalid PPM: {path}")
    require(tuple(map(int, parts[1].split())) == (width, height), f"Wrong capture dimensions: {path}")
    pixels = parts[3]
    require(len(pixels) == width * height * 3, f"Incomplete PPM: {path}")
    require(max(pixels) - min(pixels) > 8, f"Blank/uniform runtime capture: {path}")
    require(len(set(zip(pixels[::3], pixels[1::3], pixels[2::3]))) > 32,
            f"Runtime capture has no useful image detail: {path}")
    return pixels


def image_difference(first, second):
    require(len(first) == len(second), "Off/on image sizes differ")
    changed = sum(first[i:i+3] != second[i:i+3] for i in range(0, len(first), 3))
    return {"changed_pixels": changed, "changed_pixel_fraction": changed / (len(first) / 3),
            "mean_absolute_rgb_difference": sum(abs(a-b) for a, b in zip(first, second)) / len(first)}


class Runtime:
    def __init__(self, args, output):
        self.args, self.output = args, output
        self.work = output / "work"
        self.work.mkdir(parents=True, exist_ok=True)
        assets = self.work / "assets"
        if not assets.exists():
            try:
                assets.symlink_to(args.source_root / "assets", target_is_directory=True)
            except OSError:
                # Windows without symlink permission: hardlink where possible,
                # otherwise copy only the fixture assets into the isolated workdir.
                def link_or_copy(source, destination):
                    try:
                        return os.link(source, destination)
                    except OSError:
                        return shutil.copy2(source, destination)
                shutil.copytree(args.source_root / "assets", assets, copy_function=link_or_copy)
        self.base_env = {key: value for key, value in os.environ.items() if not key.startswith("FURY_")}
        self.base_env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                             FURY_AUDIO_BACKEND="null", FURY_CPU_THREADS="2", FURY_QUALITY="high",
                             FURY_SURFACE_DETAIL="1", FURY_TRACE_MODE="ray", FURY_NET="embedded",
                             OMP_NUM_THREADS="2", OPENBLAS_NUM_THREADS="2", LP_NUM_THREADS="2")
        self.records = []

    def run(self, name, arguments, toggle="1", expected_success=True, overrides=None):
        env = dict(self.base_env)
        if toggle is not None:
            env["FURY_WORLD_ART"] = toggle
        env.update(overrides or {})
        # Each launch gets identical defaults and cannot touch the player's saves.
        for pattern in ("vaultline_settings.json", "vaultline_session*.json"):
            for path in self.work.glob(pattern):
                path.unlink()
        command = [str(self.args.vaultline), *map(str, arguments)]
        start = time.perf_counter()
        try:
            result = subprocess.run(command, cwd=self.work, env=env, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=self.args.timeout, errors="replace")
            log, code = result.stdout, result.returncode
        except subprocess.TimeoutExpired as error:
            log = error.stdout or ""
            if isinstance(log, bytes):
                log = log.decode("utf-8", errors="replace")
            (self.output / (name + ".log")).write_text(log, encoding="utf-8")
            raise AssertionError(f"{name} exceeded {self.args.timeout:g}s CPU runtime bound") from error
        elapsed = time.perf_counter() - start
        (self.output / (name + ".log")).write_text(log, encoding="utf-8")
        record = {"case": name, "command": command, "world_art": toggle, "returncode": code,
                  "process_wall_seconds": elapsed, "log": name + ".log"}
        self.records.append(record)
        require((code == 0) if expected_success else (code != 0),
                f"{name}: unexpected exit {code}; see {self.output / (name + '.log')}\n{log[-2000:]}")
        require(elapsed <= self.args.timeout, f"{name} exceeded its wall-time budget")
        return log, record

    def audit(self, name, toggle):
        path = self.output / (name + ".json")
        if path.exists():
            path.unlink()
        self.run(name, ["--world-audit", path], toggle)
        require(path.is_file(), f"{name}: runtime did not write the requested world audit")
        return strict_json(path)

    def capture(self, backend, toggle, view, width, height, frames, mode="ray", suffix="", spp=None):
        name = f"{view}-{backend}-{mode}-art{toggle}{suffix}"
        path = self.output / (name + ".ppm")
        if spp is None:
            spp = self.args.ray_spp if backend == "cpu-ray" and self.args.ray_spp is not None else self.args.spp
        if path.exists():
            path.unlink()
        arguments = ["--photo", "--view", view, "--no-hud", "--width", width,
                     "--height", height, "--frames", frames, "--spp", spp,
                     "--bounces", 2, "--capture", path]
        overrides = {"FURY_TRACE_MODE": mode}
        if backend == "cpu-ray":
            arguments.insert(0, "--cpu-ray")
        elif backend == "software":
            arguments.insert(0, "--soft")
        elif backend == "gl":
            overrides["SDL_VIDEODRIVER"] = os.environ.get("SDL_VIDEODRIVER", "x11")
        else:
            raise AssertionError(f"Unknown backend {backend}")
        log, record = self.run(name, arguments, str(toggle), overrides=overrides)
        expected = {"software": "Renderer backend: CPU software rasterizer",
                    "cpu-ray": "Renderer backend: CPU BVH ray/path tracer"}
        if backend in expected:
            require(expected[backend] in log, f"{name}: requested CPU renderer did not initialize")
        else:
            require("falling back to software" not in log and "OpenGL" in log,
                    f"{name}: OpenGL smoke fell back rather than testing GL")
        match = VALIDATION.search(log)
        if backend == "cpu-ray":
            require(match is not None, f"{name}: missing actual CPU renderer validation counters")
        if match:
            actual_frames, provider, triangles, cpu_ms, accumulated, errors = match.groups()
            record["renderer"] = {"frames": int(actual_frames), "provider": provider,
                                  "triangles": int(triangles), "last_cpu_frame_ms": float(cpu_ms),
                                  "accumulated_frames": int(accumulated), "validation_errors": int(errors)}
            require(int(actual_frames) == frames and int(errors) == 0 and int(triangles) > 0,
                    f"{name}: invalid renderer frame/geometry/error counters")
            require(math.isfinite(float(cpu_ms)) and float(cpu_ms) > 0,
                    f"{name}: invalid measured CPU frame time")
        pixels = read_ppm(path, width, height)
        record.update(capture=path.name, width=width, height=height,
                      requested_frames=frames, samples_per_pixel=spp, trace_mode=mode,
                      capture_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        return pixels


def collider_bounds(entity):
    # Match Scene::collect_solids: local center offset and scaled half extents.
    center = [p + c for p, c in zip(entity["position"], entity["collider_center"])]
    half = [abs(h * s) for h, s in zip(entity["collider_half_extents"], entity["scale"])]
    return ([c - h for c, h in zip(center, half)],
            [c + h for c, h in zip(center, half)])


def segment_hits_expanded_collider(start, end, bounds, radius):
    """Continuous XZ slab test, including interiors between route waypoints."""
    low, high = bounds
    enter, leave = 0.0, 1.0
    for axis in (0, 2):
        minimum, maximum = low[axis] - radius, high[axis] + radius
        delta = end[axis] - start[axis]
        if abs(delta) < 1e-12:
            if not minimum <= start[axis] <= maximum:
                return False
        else:
            first, second = sorted(((minimum - start[axis]) / delta,
                                    (maximum - start[axis]) / delta))
            enter, leave = max(enter, first), min(leave, second)
            if enter > leave:
                return False
    return enter <= leave


def rounded_body_clearance(start, end, bounds, radius):
    """Exact XZ distance between a closed segment and rectangle, minus radius.

    Endpoint-to-rectangle and corner-to-segment distances cover separated
    convex shapes; the slab test handles crossings whose endpoints are clear.
    Unlike a swept bounding box or square-expanded corners, this tests the
    circular body continuously along the segment, including a zero-length spawn.
    """
    if segment_hits_expanded_collider(start, end, bounds, 0.0):
        return -radius
    low, high = bounds

    def point_rectangle(point):
        return math.hypot(*(max(low[axis] - point[axis], 0.0,
                               point[axis] - high[axis]) for axis in (0, 2)))

    dx, dz = end[0] - start[0], end[2] - start[2]
    length_squared = dx * dx + dz * dz

    def corner_segment(x, z):
        t = ((x - start[0]) * dx + (z - start[2]) * dz) / length_squared if length_squared else 0.0
        t = max(0.0, min(1.0, t))
        return math.hypot(x - start[0] - t * dx, z - start[2] - t * dz)

    distance = min(point_rectangle(start), point_rectangle(end),
                   *(corner_segment(x, z) for x in (low[0], high[0])
                     for z in (low[2], high[2])))
    return distance - radius


def verify_owen_lobby_clearance(audit, state):
    """Use the real world and roster audits, with no duplicated chair layout."""
    entities = {entity["name"]: entity for entity in audit["protected_entities"]}
    owen = next(a for a in state["agents"] if a["id"] == "NpcBankCust")
    route, radius = owen["waypoints"], owen["radius"]
    require(len(route) >= 3 and math.isfinite(radius) and radius > 0,
            "Owen audit must include the actual authored route and body radius")
    segments = list(zip(route, route[1:] + route[:1]))
    # Also protect startup-to-route travel if a future authored spawn moves.
    segments.append((owen["position"], route[0]))
    chairs = [entities[f"MMChair{i}"] for i in range(4)]
    aisle = (entities["RouteEntrance"]["position"], entities["RouteLobby"]["position"])
    for chair in chairs:
        require(chair["solid"], f"{chair['name']}: route clearance must not disable collision")
        bounds = collider_bounds(chair)
        for segment, (start, end) in enumerate(segments):
            require(not segment_hits_expanded_collider(start, end, bounds, radius),
                    f"{chair['name']} intersects Owen's swept route segment {segment} (radius {radius})")
        require(not segment_hits_expanded_collider(*aisle, bounds, radius),
                f"{chair['name']} blocks the authored entrance-to-lobby aisle")
        # Repositioned seats must not replace route clipping with another solid
        # overlap (entrance stanchions, doorway posts or the other chairs).
        for other in entities.values():
            if not other["solid"] or other["name"] == chair["name"]:
                continue
            other_bounds = collider_bounds(other)
            overlap = all(max(bounds[0][axis], other_bounds[0][axis]) <
                          min(bounds[1][axis], other_bounds[1][axis]) - 1e-6
                          for axis in range(3))
            require(not overlap, f"{chair['name']} overlaps solid {other['name']}")
    return {"actor": owen["id"], "radius": radius, "route": route,
            "segments_checked": len(segments), "chairs_checked": len(chairs),
            "aisle_anchors": ["RouteEntrance", "RouteLobby"]}


def run_owen_lobby_clearance(runtime, audit):
    path = runtime.output / "owen-lobby-route.json"
    runtime.run("owen-lobby-route", ["--soft", "--npc-view", "NpcBankCust",
                "--width", 64, "--height", 64, "--frames", 1, "--no-hud",
                "--npc-audit", path], overrides={"FURY_NET_PORT": "18432"})
    state = strict_json(Path(str(path) + ".state.json"))
    result = verify_owen_lobby_clearance(audit, state)
    print("PASS actual Owen route radius, all swept segments, lobby seats and entrance aisle", flush=True)
    return result


def verify_nell_market_clearance(audit, state):
    """Guard Nell using actual early-world colliders and the full roster sidecar.

    --world-audit exits before late Meridian mission solids are spawned. This
    ordinary CI gate covers every solid available in that snapshot. The source-
    pinned Nell evidence separately certifies all 343 startup solids, eight
    non-solid visual obstructions and actual motion at multiple update cadences.
    """
    entities = audit["protected_entities"]
    actors = [actor for actor in state["agents"] if actor["id"] == "NpcCivAsh"]
    require(len(actors) == 1, "Nell must retain exactly one actual roster identity")
    nell = actors[0]
    route, radius = nell["waypoints"], nell["radius"]
    require(math.isfinite(radius) and abs(radius - 0.4) < 1e-6,
            "Nell must retain her actual 0.4 m body radius; do not shrink it to clear fixtures")
    require(len(route) >= 3 and all(len(point) == 3 and all(math.isfinite(v) for v in point)
                                   for point in route), "Nell audit lacks a finite authored route")
    require(max(abs(a - b) for a, b in zip(nell["position"], (-88.0, 0.875, 42.0))) < 1e-5
            and max(abs(a - b) for a, b in zip(route[0], (-88.0, 0.0, 42.0))) < 1e-5,
            "Nell's startup position and first/home waypoint must stay fixed")
    segments = list(zip(route, route[1:] + route[:1]))
    segments.append((nell["position"], route[0]))
    solids = [(index, entity, collider_bounds(entity)) for index, entity in enumerate(entities)
              if entity["solid"]]
    require(solids, "Nell clearance needs the populated actual collision world")
    minimum = math.inf
    for segment, (start, end) in enumerate(segments):
        for index, entity, bounds in solids:
            clearance = rounded_body_clearance(start, end, bounds, radius)
            minimum = min(minimum, clearance)
            require(clearance > 1e-5,
                    f"Nell swept segment {segment} contacts solid #{index} {entity['name']} "
                    f"(body clearance {clearance:.6f} m, radius {radius:.6f})")

    # Duplicate MidCrate/MidTrash names occur across districts: pin the two
    # original entity indices and their approved transforms/collider dimensions.
    storage = ((1825, "MidCrate", (-89.0, 0.55, 39.8), (0.5, 0.5, 0.5)),
               (1826, "MidTrash", (-87.6, 0.55, 40.1), (0.325, 0.525, 0.325)))
    for index, name, position, half_extents in storage:
        require(index < len(entities), f"Missing Nell storage entity #{index}")
        entity = entities[index]
        require(entity["name"] == name and entity["solid"],
                f"Nell storage #{index} {name} must retain its identity and solid collider")
        for field, expected in (("position", position), ("collider_center", (0, 0, 0)),
                                ("collider_half_extents", half_extents), ("scale", (1, 1, 1))):
            require(max(abs(a - b) for a, b in zip(entity[field], expected)) < 1e-5,
                    f"Nell storage #{index} {name}: unexpected {field}")
        bounds = collider_bounds(entity)
        for other_index, other, other_bounds in solids:
            if other_index == index:
                continue
            overlap = all(max(bounds[0][axis], other_bounds[0][axis]) <
                          min(bounds[1][axis], other_bounds[1][axis]) - 1e-6
                          for axis in range(3))
            require(not overlap, f"Nell storage #{index} overlaps solid #{other_index} {other['name']}")

    # Local, walkable approach witnesses preserve both stall service areas,
    # the fence counter, ATM approach and the passage below the fixed hydrant.
    # Use the player's 0.45 m square-expanded collision convention here, which
    # is stricter than the rounded NPC-body calculation at rectangle corners.
    player_radius = 0.45
    access = {
        "home_stall_A_fence": ((-86.7, 38.5), (-86.7, 42), (-88, 42),
                              (-87.45, 42), (-87.45, 44), (-87.45, 45.7),
                              (-88.95, 45.7), (-88.95, 50.65), (-86, 50.65)),
        "stall_B": ((-86.7, 42), (-84, 41.5)),
        "ATM": ((-86.7, 38.5), (-90, 32), (-90, 30.85)),
        "fixed_hydrant_passage": ((-90.5, 40.85), (-88.6, 40.85)),
    }
    for name, path in access.items():
        points = [(x, 0.0, z) for x, z in path]
        for segment, (start, end) in enumerate(zip(points, points[1:])):
            for index, entity, bounds in solids:
                require(not segment_hits_expanded_collider(start, end, bounds, player_radius),
                        f"Nell player access {name}/{segment} blocked by solid #{index} {entity['name']}")
    return {"actor": nell["id"], "radius": radius, "route": route,
            "segments_checked": len(segments), "solid_colliders_checked": len(solids),
            "minimum_rounded_body_clearance": minimum, "storage_indices": [1825, 1826],
            "player_radius": player_radius, "player_access_paths": access,
            "scope": "actual world-audit snapshot before late Meridian mission additions"}


def run_regression(runtime):
    off = runtime.audit("world-off", "0")
    on = runtime.audit("world-on", "1")
    repeat = runtime.audit("world-on-repeat", "1")
    default = runtime.audit("world-default", None)
    for audit, enabled in ((off, False), (on, True), (repeat, True), (default, True)):
        verify_audit(audit, enabled)
    require(off["after"] == on["before"], "Off/on baseline worlds differ")
    require(off["after_geometry_fingerprint"] == on["before_geometry_fingerprint"],
            "Off/on baseline geometry/layout fingerprints differ")
    require(off["protected_entities"] == on["protected_entities"],
            "Original ordered identities/transforms/colliders differ across real off/on builds")
    require(on == repeat, "World build is nondeterministic across fresh processes")
    require(on == default, "Unset FURY_WORLD_ART differs from the enabled default")
    lobby_clearance = run_owen_lobby_clearance(runtime, on)
    # Owen's existing frozen capture includes every regular NPC. Reuse that
    # actual sidecar so Nell's route gate needs no additional game launch.
    roster_state = strict_json(runtime.output / "owen-lobby-route.json.state.json")
    nell_clearance = verify_nell_market_clearance(on, roster_state)
    print("PASS actual Nell radius, complete swept loop/spawn, solid storage and market access", flush=True)
    for value in ("invalid", "2", "-1", "true", "01"):
        name = "invalid-toggle-" + value.replace("-", "minus")
        path = runtime.output / (name + ".json")
        if path.exists():
            path.unlink()
        log, _ = runtime.run(name, ["--world-audit", path], value, expected_success=False)
        require("FURY_WORLD_ART" in log and not path.exists(), "Invalid toggle lacked a clear rejection")
    runtime.run("missing-audit-path", ["--world-audit"], expected_success=False)
    runtime.run("invalid-view", ["--view", "not-a-district"], expected_success=False)
    differences = []
    for backend in ("software", "cpu-ray"):
        # Keep these tiny regression files separate from the later evidence
        # matrix, so every record's dimensions/hash still match its saved PPM.
        first = runtime.capture(backend, 0, "metro-wide", 160, 90, 2, suffix="-regression", spp=1)
        second = runtime.capture(backend, 1, "metro-wide", 160, 90, 2, suffix="-regression", spp=1)
        difference = image_difference(first, second)
        require(difference["changed_pixels"] > 8, f"{backend}: world-art toggle has no visible runtime effect")
        differences.append({"view": "metro-wide", "backend": backend, **difference})
        if backend == "cpu-ray":
            repeated_pixels = runtime.capture(backend, 1, "metro-wide", 160, 90, 2,
                                              suffix="-regression-repeat", spp=1)
            require(second == repeated_pixels, "Frozen CPU-ray capture changed across identical fresh launches")
    return {"before": on["before"], "after": on["after"], "coverage": on["coverage"],
            "baseline_geometry_diagnostics": {key: off["after"][key] for key in
                                           ("invalid_indices", "nonfinite_vertices", "degenerate_triangles",
                                            "zero_area_triangles", "nonfinite_instances")},
            "image_comparisons": differences, "owen_lobby_clearance": lobby_clearance,
            "nell_market_clearance": nell_clearance}


def run_captures(runtime):
    comparisons = []
    for view in VIEWS:
        for backend in ("software", "cpu-ray"):
            frames = runtime.args.ray_frames if backend == "cpu-ray" and runtime.args.ray_frames is not None else runtime.args.frames
            pixels = [runtime.capture(backend, toggle, view, runtime.args.width,
                                      runtime.args.height, frames) for toggle in (0, 1)]
            difference = image_difference(*pixels)
            require(difference["changed_pixels"] > 8, f"{view}/{backend}: art off/on looks identical")
            comparisons.append({"view": view, "backend": backend, **difference})
            print(f"PASS {view}/{backend}: {difference['changed_pixel_fraction']:.1%} changed pixels", flush=True)
    if runtime.args.path_smoke:
        runtime.capture("cpu-ray", 1, "waterfront", 160, 90, 2, mode="path", spp=1)
    if runtime.args.gl_smoke:
        runtime.capture("gl", 1, "metro-wide", 320, 180, 2)
    return comparisons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vaultline", required=True, type=Path)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--captures", action="store_true", help="Also save all six districts and overview off/on")
    parser.add_argument("--captures-only", action="store_true", help="Capture matrix only; audits must be tested separately")
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=360)
    parser.add_argument("--frames", type=int, default=3)
    parser.add_argument("--spp", type=int, default=1)
    parser.add_argument("--ray-frames", type=int, help="Override matrix frame count only for CPU ray captures")
    parser.add_argument("--ray-spp", type=int, help="Override matrix samples per pixel only for CPU ray captures")
    parser.add_argument("--timeout", type=float, default=45, help="Per-process seconds, including startup")
    parser.add_argument("--path-smoke", action="store_true")
    parser.add_argument("--gl-smoke", action="store_true", help="Requires a working desktop/display or xvfb-run")
    args = parser.parse_args()
    args.vaultline, args.source_root = args.vaultline.resolve(), args.source_root.resolve()
    require(args.vaultline.is_file(), f"Vaultline executable missing: {args.vaultline}")
    require((args.source_root / "assets").is_dir(), "Source assets are required for a real-world test")
    require(64 <= args.width <= 7680 and 64 <= args.height <= 4320, "Invalid capture dimensions")
    require(1 <= args.frames <= 1000 and 1 <= args.spp <= 64 and args.timeout > 0, "Invalid runtime bounds")
    require(args.ray_frames is None or 1 <= args.ray_frames <= 1000, "Invalid CPU ray frame count")
    require(args.ray_spp is None or 1 <= args.ray_spp <= 64, "Invalid CPU ray sample count")
    # Resource guard applies to this process and descendants, never the caller.
    if hasattr(os, "sched_getaffinity") and hasattr(os, "sched_setaffinity"):
        available = sorted(os.sched_getaffinity(0))
        os.sched_setaffinity(0, available[:2])
    temporary = None
    if args.output_dir is None:
        temporary = tempfile.TemporaryDirectory(prefix="fury-world-regression-")
        output = Path(temporary.name)
    else:
        output = args.output_dir.resolve()
        output.mkdir(parents=True, exist_ok=True)
    runtime = Runtime(args, output)
    summary = {"schema": 1, "test_kind": "actual-vaultline-full-world-runtime",
               "vaultline_executable": str(args.vaultline),
               "vaultline_sha256": hashlib.sha256(args.vaultline.read_bytes()).hexdigest(),
               "platform": platform.platform(), "python_version": platform.python_version(),
               "process_timeout_seconds": args.timeout, "max_cpu_workers": 2,
               "notes": ["Process wall time includes startup, asset loading and shutdown; it is not FPS.",
                         "last_cpu_frame_ms is reported only when present in renderer-owned logs.",
                         "degenerate_triangles uses squared local-space cross-product area <1e-14; microscopic facets are included, not only exact zero-area triangles.",
                         "Image differences establish a visible toggle effect, not subjective visual quality."]}
    try:
        if not args.captures_only:
            summary["regression"] = run_regression(runtime)
        if args.captures or args.captures_only:
            summary["capture_comparisons"] = run_captures(runtime)
        require(summary["vaultline_sha256"] == hashlib.sha256(args.vaultline.read_bytes()).hexdigest(),
                "Vaultline executable changed during validation; rerun against one unchanged build")
        for record in runtime.records:
            if "capture" in record:
                saved = output / record["capture"]
                require(hashlib.sha256(saved.read_bytes()).hexdigest() == record["capture_sha256"],
                        f"Saved capture no longer matches its runtime record: {saved}")
        summary["passed"] = True
        print("PASS actual six-district capture matrix" if args.captures_only else
              "PASS actual six-district world preservation, geometry, coverage, budgets and CPU runtime", flush=True)
    except Exception as error:
        summary.update(passed=False, error=str(error))
        print(f"FAIL {error}\nEvidence: {output}", file=sys.stderr, flush=True)
        return 1
    finally:
        summary["runs"] = runtime.records
        (output / "world-validation-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        # Retain failed evidence; successful CTest runs need no generated files.
        if temporary is not None:
            if not summary.get("passed"):
                retained = Path(tempfile.mkdtemp(prefix="fury-world-failure-"))
                for path in output.iterdir():
                    if path.is_file():
                        shutil.copy2(path, retained / path.name)
                print(f"Failure logs and audit JSON retained at {retained}", file=sys.stderr)
            temporary.cleanup()
    return 0


if __name__ == "__main__":
    sys.exit(main())
