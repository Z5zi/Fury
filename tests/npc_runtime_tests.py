#!/usr/bin/env python3
"""Run the real Vaultline NPC renderer and compare its actual gameplay state.

Uses isolated saves, CPU software rendering, production actor IDs, renderer-owned
PPM captures and the application's state sidecar. No source parsing or invented
NPC simulation. Visual differences prove an effect, not aesthetic quality.
"""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import socket
import subprocess
import sys
import tempfile
import time


REGULAR_IDS = {
    "NpcCivA", "NpcCivB", "NpcCivC", "NpcGuard", "NpcTeller", "NpcDeskGuard",
    "NpcBankCust", "NpcAlleyHmpd", "NpcCivAsh", "NpcCivD", "NpcCivE", "NpcFence",
}
CREW_IDS = {"CrewRook", "CrewSparrow"}
PRESENTATION_IDS = REGULAR_IDS | CREW_IDS | {"PlayerBody", "GhostLoop"}
DYNAMIC_ROLES = {"NpcExtraGuard": "security", "NpcEnforcer": "enforcer"}
EXPECTED_ROLES = {
    "NpcCivA": "commuter", "NpcGuard": "security", "NpcFence": "fence",
    "CrewRook": "crew_tech", "CrewSparrow": "crew_scout",
    "PlayerBody": "player", "GhostLoop": "ghost", **DYNAMIC_ROLES,
}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def strict_json(path):
    def reject(value):
        raise AssertionError(f"Nonfinite JSON constant in {path}: {value}")
    return json.loads(path.read_text(encoding="utf-8"), parse_constant=reject)


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def vector(value):
    return isinstance(value, list) and len(value) == 3 and all(finite(v) for v in value)


def read_ppm(path, width, height):
    parts = path.read_bytes().split(b"\n", 3)
    require(len(parts) == 4 and parts[0] == b"P6" and parts[2] == b"255", f"Invalid PPM {path}")
    require(tuple(map(int, parts[1].split())) == (width, height), f"Wrong capture dimensions {path}")
    pixels = parts[3]
    require(len(pixels) == width * height * 3, f"Incomplete renderer capture {path}")
    require(max(pixels) - min(pixels) > 8, f"Uniform/empty runtime capture {path}")
    require(len(set(zip(pixels[::3], pixels[1::3], pixels[2::3]))) > 32,
            f"Runtime capture lacks meaningful color detail {path}")
    return pixels


def image_difference(first, second, width, height):
    require(len(first) == len(second), "Image dimensions do not agree")
    changed = sum(first[i:i + 3] != second[i:i + 3] for i in range(0, len(first), 3))
    central = 0
    for y in range(height // 4, height * 3 // 4):
        for x in range(width // 4, width * 3 // 4):
            i = (y * width + x) * 3
            central += first[i:i + 3] != second[i:i + 3]
    return {"changed_pixels": changed, "central_changed_pixels": central,
            "changed_pixel_fraction": changed / (width * height),
            "mean_absolute_rgb_difference": sum(abs(a - b) for a, b in zip(first, second)) / len(first)}


def verify_camera(camera, elevation, orbit):
    require(isinstance(camera, dict) and vector(camera.get("eye")) and vector(camera.get("target")),
            "Missing/nonfinite actual capture camera geometry")
    require(camera.get("elevation_degrees") == elevation and camera.get("orbit_degrees") == orbit,
            "Capture camera did not record the requested angles")
    offset = [eye - target for eye, target in zip(camera["eye"], camera["target"])]
    radius = math.sqrt(sum(value * value for value in offset))
    require(radius > 0 and finite(camera.get("yaw")) and finite(camera.get("pitch")),
            "Invalid capture camera radius/orientation")
    direction = [-value / radius for value in offset]
    forward = [math.cos(camera["pitch"]) * math.cos(camera["yaw"]), math.sin(camera["pitch"]),
               math.cos(camera["pitch"]) * math.sin(camera["yaw"])]
    require(max(abs(a - b) for a, b in zip(direction, forward)) < 2e-5,
            "Actual capture camera does not look at its recorded target")
    return offset, radius


def verify_elevation(base, raised, degrees):
    before, radius_before = verify_camera(base["camera"], 0, base["camera"]["orbit_degrees"])
    after, radius_after = verify_camera(raised["camera"], degrees, raised["camera"]["orbit_degrees"])
    require(abs(radius_before - radius_after) < 2e-5, "Elevation changed the camera orbit radius")
    angle_before = math.atan2(before[1], math.hypot(before[0], before[2]))
    angle_after = math.atan2(after[1], math.hypot(after[0], after[2]))
    require(abs(angle_after - angle_before - math.radians(degrees)) < 2e-5,
            "Camera eye did not rise/fall by the requested spherical angle")
    require(abs(math.atan2(after[0], after[2]) - math.atan2(before[0], before[2])) < 2e-5,
            "Elevation changed the camera azimuth")


def verify_state(state, selected, frozen):
    require(type(state.get("heist_phase")) is int and 0 <= state["heist_phase"] <= 6,
            "Invalid/missing actual heist state")
    expected_ids = REGULAR_IDS | ({selected} if selected in DYNAMIC_ROLES else set())
    agents, crew = state.get("agents"), state.get("crew")
    require(isinstance(agents, list) and len(agents) == len(expected_ids), "Wrong real NPC roster count")
    require({a.get("id") for a in agents} == expected_ids, "Actual NPC IDs changed or duplicated")
    require(isinstance(crew, list) and len(crew) == 2 and {c.get("id") for c in crew} == CREW_IDS,
            "Actual crew IDs changed or duplicated")
    for actor in agents + crew:
        require(vector(actor.get("position")) and finite(actor.get("yaw")),
                f"Nonfinite actor transform {actor.get('id')}")
        require(finite(actor.get("travel_distance")) and actor["travel_distance"] >= 0,
                f"Invalid actual travel {actor['id']}")
        if frozen:
            require(actor["travel_distance"] == 0, f"Frozen portrait advances AI travel for {actor['id']}")
    for actor in agents:
        require(type(actor.get("on_duty")) is bool and type(actor.get("chasing")) is bool,
                f"Missing actual duty/chase state {actor['id']}")
        require(finite(actor.get("actual_speed")) and actor["actual_speed"] >= 0,
                f"Invalid measured speed {actor['id']}")
        waypoints = actor.get("waypoints")
        require(isinstance(waypoints, list) and waypoints and all(vector(w) for w in waypoints),
                f"Invalid authored route {actor['id']}")
        require(type(actor.get("waypoint_index")) is int and 0 <= actor["waypoint_index"] < len(waypoints),
                f"Invalid actual target index {actor['id']}")
    for actor in crew:
        require(type(actor.get("active")) is bool, f"Missing crew active state {actor['id']}")
    return {actor["id"]: actor for actor in agents + crew}


def verify_profile(profile, selected, enabled, moving):
    require(profile.get("enabled") is enabled, "FURY_NPC_DETAIL was not honored")
    for key in ("actors", "near_triangles", "far_triangles", "pose_updates", "posed_vertices", "deferred_updates"):
        require(type(profile.get(key)) is int and profile[key] >= 0, f"Invalid presentation counter {key}")
    entries = profile.get("profiles")
    require(isinstance(entries, list), "Missing presentation profile list")
    if not enabled:
        require(not entries and not any(profile[key] for key in
                ("actors", "near_triangles", "far_triangles", "pose_updates", "posed_vertices", "deferred_updates")),
                "Disabled detail still installs or updates detailed characters")
        return
    expected_ids = PRESENTATION_IDS | ({selected} if selected in DYNAMIC_ROLES else set())
    require(profile["actors"] == len(expected_ids) and len(entries) == len(expected_ids),
            "Detailed presentation omits or duplicates a real actor")
    require({entry.get("id") for entry in entries} == expected_ids, "Detailed actor identity mismatch")
    near_total = far_total = 0
    for entry in entries:
        require(finite(entry.get("height")) and 1 <= entry["height"] <= 2.5, "Invalid original actor height")
        require(type(entry.get("seed")) is int and entry["seed"] >= 0, "Invalid stable actor seed")
        require(type(entry.get("near_triangles")) is int and type(entry.get("far_triangles")) is int and
                0 < entry["far_triangles"] < entry["near_triangles"], "Missing distinct near/far actor geometry")
        if entry["id"] in EXPECTED_ROLES:
            require(entry.get("role") == EXPECTED_ROLES[entry["id"]], f"Wrong role for {entry['id']}")
        near_total += entry["near_triangles"]
        far_total += entry["far_triangles"]
    require(profile["near_triangles"] == near_total and profile["far_triangles"] == far_total,
            "Actor geometry counters disagree with profile entries")
    if moving:
        require(profile["pose_updates"] > 0 and profile["posed_vertices"] > 0,
                "Live simulation never applied a character pose")


class Runtime:
    def __init__(self, args, output):
        self.args, self.output, self.records = args, output, []
        self.used_ports = set()
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("FURY_")}
        self.env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", FURY_AUDIO_BACKEND="null",
                        FURY_CPU_THREADS="2", FURY_QUALITY="high", FURY_WORLD_ART="1",
                        FURY_SURFACE_DETAIL="1", FURY_NET="embedded", OMP_NUM_THREADS="2",
                        OPENBLAS_NUM_THREADS="2", LP_NUM_THREADS="2")

    def run(self, name, arguments, detail="1", expected_success=True):
        # Every process has a fresh settings/save directory. It cannot load,
        # rewrite, or delete the user's session files in the source/build tree.
        work = self.output / "work" / name
        work.mkdir(parents=True)
        try:
            (work / "assets").symlink_to(self.args.source_root / "assets", target_is_directory=True)
        except OSError:
            def link_or_copy(source, destination):
                try:
                    return os.link(source, destination)
                except OSError:
                    return shutil.copy2(source, destination)
            shutil.copytree(self.args.source_root / "assets", work / "assets", copy_function=link_or_copy)
        env = dict(self.env)
        # Concurrent CTest/capture processes must never share the default
        # embedded host. Reserve an ephemeral localhost UDP port, then release
        # it immediately before launching the application's own server.
        while True:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
                reservation.bind(("127.0.0.1", 0))
                net_port = reservation.getsockname()[1]
            if net_port not in self.used_ports:
                self.used_ports.add(net_port)
                break
        env["FURY_NET_PORT"] = str(net_port)
        if detail is not None:
            env["FURY_NPC_DETAIL"] = detail
        command = [str(self.args.vaultline), *map(str, arguments)]
        started = time.perf_counter()
        try:
            result = subprocess.run(command, cwd=work, env=env, text=True, errors="replace",
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=self.args.timeout)
            log, code = result.stdout, result.returncode
        except subprocess.TimeoutExpired as error:
            log = error.stdout or ""
            if isinstance(log, bytes):
                log = log.decode("utf-8", errors="replace")
            (self.output / (name + ".log")).write_text(log, encoding="utf-8")
            raise AssertionError(f"{name} exceeded {self.args.timeout:g}s runtime bound") from error
        elapsed = time.perf_counter() - started
        (self.output / (name + ".log")).write_text(log, encoding="utf-8")
        record = {"case": name, "command": command, "detail": detail, "returncode": code,
                  "embedded_udp_port": net_port,
                  "process_wall_seconds": elapsed, "log": name + ".log"}
        self.records.append(record)
        require(code == 0 if expected_success else code != 0,
                f"{name}: unexpected exit {code}; see {name}.log\n{log[-1800:]}")
        return log, record

    def capture(self, selected, detail, mode="view", suffix="", talk=False, sequence=False, orbit=None, elevation=None):
        label = "default" if detail is None else detail
        name = f"{selected}-{mode}-detail{label}{suffix}"
        audit, ppm = self.output / (name + ".json"), self.output / (name + ".ppm")
        frames = 4 if mode == "view" else 8
        arguments = ["--soft", "--npc-" + mode, selected, "--frames", frames, "--capture-fps", 12,
                     "--no-hud", "--width", self.args.width, "--height", self.args.height,
                     "--capture", ppm, "--npc-audit", audit]
        if talk:
            arguments.append("--npc-talk")
        if orbit is not None:
            arguments += ["--npc-orbit", orbit]
        if elevation is not None:
            arguments += ["--npc-elevation", elevation]
        sequence_directory = self.output / (name + "-sequence")
        if sequence:
            arguments += ["--capture-sequence", sequence_directory]
        log, record = self.run(name, arguments, detail)
        require("Renderer backend: CPU software rasterizer" in log, "Requested GPU-free rasterizer did not initialize")
        require(f"port={record['embedded_udp_port']} (threaded)" in log and
                f"NetClient UDP connected to 127.0.0.1:{record['embedded_udp_port']} " in log and
                "(embedded host)" in log,
                "Runtime did not establish its own isolated embedded UDP host/client")
        expected_mode = "frozen portrait" if mode == "view" else "live AI/animation"
        require(f"NPC capture: {selected} {expected_mode}" in log, "Application did not select requested real actor/mode")
        require(audit.is_file() and Path(str(audit) + ".state.json").is_file(), "Missing real profile/state sidecar")
        profile, state = strict_json(audit), strict_json(Path(str(audit) + ".state.json"))
        renderer = strict_json(Path(str(audit) + ".render.json"))
        require(renderer.get("frames") == frames and renderer.get("validation_errors") == 0,
                "Renderer did not complete exactly the requested error-free frames")
        require("CPU software rasterizer" in renderer.get("backend", "") and
                type(renderer.get("triangles")) is int and renderer["triangles"] > 0 and
                type(renderer.get("instances")) is int and renderer["instances"] > 0 and
                finite(renderer.get("cpu_frame_ms")) and renderer["cpu_frame_ms"] >= 0,
                "Missing actual renderer-owned frame/geometry measurements")
        record["renderer"] = renderer
        camera = renderer.get("camera")
        verify_camera(camera, elevation or 0, orbit or 0)
        actors = verify_state(state, selected, mode == "view")
        verify_profile(profile, selected, detail != "0", mode == "motion")
        pixels = read_ppm(ppm, self.args.width, self.args.height)
        record.update(capture=ppm.name, capture_sha256=hashlib.sha256(ppm.read_bytes()).hexdigest(),
                      audit=audit.name, state_sidecar=audit.name + ".state.json", frames=frames,
                      width=self.args.width, height=self.args.height)
        sequences = []
        if sequence:
            paths = sorted(sequence_directory.glob("*.ppm"))
            expected_names = [f"frame_{frame:06d}.ppm" for frame in range(1, frames + 1)]
            require([p.name for p in paths] == expected_names, "Motion sequence has missing/extra/misnumbered frames")
            sequences = [read_ppm(path, self.args.width, self.args.height) for path in paths]
            require(sequences[-1] == pixels, "Last-frame capture does not match actual final sequence frame")
            record["sequence_sha256"] = [hashlib.sha256(path.read_bytes()).hexdigest() for path in paths]
        print(f"PASS {name}", flush=True)
        return {"profile": profile, "state": state, "actors": actors, "pixels": pixels, "sequence": sequences,
                "camera": camera}


def run_checks(runtime):
    differences = []
    # Independent role portraits come from the real scene builder and exact
    # production identities. Dynamic Enforcer invokes the application's spawn.
    portraits = {}
    for selected in ("NpcCivA", "NpcGuard", "NpcFence", "CrewRook", "NpcEnforcer"):
        off, on = (runtime.capture(selected, detail) for detail in ("0", "1"))
        require(off["state"] == on["state"], f"{selected}: character-art toggle changed actual gameplay state")
        difference = image_difference(off["pixels"], on["pixels"], runtime.args.width, runtime.args.height)
        require(difference["changed_pixels"] > 24 and difference["central_changed_pixels"] > 8,
                f"{selected}: detailed character has no visible central portrait effect")
        differences.append({"actor": selected, "case": "frozen-detail-toggle", **difference})
        portraits[selected] = on
    repeated = runtime.capture("NpcGuard", "1", suffix="-repeat")
    default = runtime.capture("NpcGuard", None)
    for other in (repeated, default):
        require(other["state"] == portraits["NpcGuard"]["state"] and
                other["profile"] == portraits["NpcGuard"]["profile"] and
                other["pixels"] == portraits["NpcGuard"]["pixels"],
                "Frozen actor capture/default detail is not deterministic across fresh processes")
    orbited = runtime.capture("NpcGuard", "1", suffix="-orbit", orbit=45)
    require(orbited["state"] == portraits["NpcGuard"]["state"] and
            orbited["profile"] == portraits["NpcGuard"]["profile"],
            "Camera orbit changed physical NPC/gameplay or character-profile state")
    require(orbited["pixels"] != portraits["NpcGuard"]["pixels"], "Requested camera orbit did not change framing")
    zero_elevation = runtime.capture("NpcGuard", "1", suffix="-zero-elevation", elevation=0)
    require(zero_elevation == portraits["NpcGuard"], "Explicit zero elevation changed default pixels/camera/state")
    legacy_offset, _ = verify_camera(zero_elevation["camera"], 0, 0)
    require(max(abs(a - b) for a, b in zip(legacy_offset, (3.2 * .28, .18, 3.2))) < 2e-5,
            "Default portrait camera no longer uses its exact established framing")
    for elevation, orbit in ((-60, 0), (30, 45), (60, 0)):
        elevated = runtime.capture("NpcGuard", "1", suffix=f"-elevation{elevation}",
                                   elevation=elevation, orbit=orbit)
        base = orbited if orbit else portraits["NpcGuard"]
        verify_elevation(base, elevated, elevation)
        require(elevated["state"] == base["state"] and elevated["profile"] == base["profile"],
                "Frozen elevation changed NPC/gameplay or character-profile state")
        require(elevated["camera"]["target"] == base["camera"]["target"], "Elevation moved the look-at target")
        require(elevated["pixels"] != base["pixels"], "Requested elevation did not change captured framing")
    for selected in ("NpcExtraGuard", "PlayerBody", "GhostLoop"):
        result = runtime.capture(selected, "1")
        if selected in DYNAMIC_ROLES:
            require(selected in result["actors"], "Dynamic capture did not spawn the requested real threat")

    for selected in ("NpcGuard", "NpcCivA"):
        off = runtime.capture(selected, "0", mode="motion", sequence=selected == "NpcGuard")
        on = runtime.capture(selected, "1", mode="motion", sequence=selected == "NpcGuard")
        require(off["state"] == on["state"], f"{selected}: detailed rendering changed live AI/schedule/mission state")
        actor = on["actors"][selected]
        require(actor["travel_distance"] > .001 and actor["actual_speed"] > 0,
                f"{selected}: motion CLI did not run real patrol locomotion")
        require(actor["position"] != portraits[selected]["actors"][selected]["position"],
                f"{selected}: live actor never left its production spawn")
        require(actor["waypoints"] == portraits[selected]["actors"][selected]["waypoints"],
                f"{selected}: presentation mode rewrote the authored patrol")
        difference = image_difference(off["pixels"], on["pixels"], runtime.args.width, runtime.args.height)
        require(difference["changed_pixels"] > 24, "Live baseline/detail frames unexpectedly match")
        differences.append({"actor": selected, "case": "live-detail-toggle", **difference})
        if selected == "NpcGuard":
            require(len({hashlib.sha256(frame).hexdigest() for frame in on["sequence"]}) > 1,
                    "Actual live guard sequence contains no temporal pixel change")
            require(on["sequence"][0] != on["sequence"][-1], "Guard first/last motion frames are identical")

    idle = runtime.capture("CrewRook", "1", mode="motion", suffix="-idle")
    talking = runtime.capture("CrewRook", "1", mode="motion", suffix="-talk", talk=True)
    require(idle["state"] == talking["state"], "Presentation-only talk gesture changed gameplay state")
    require(talking["actors"]["CrewRook"]["travel_distance"] == 0,
            "Idle pre-heist crew should not pretend to follow during gesture capture")
    gesture = image_difference(idle["pixels"], talking["pixels"], runtime.args.width, runtime.args.height)
    require(gesture["central_changed_pixels"] > 4, "Requested talk gesture has no captured central pixel effect")
    differences.append({"actor": "CrewRook", "case": "presentation-talk-gesture", **gesture})

    live_base = runtime.capture("NpcBankCust", "1", mode="motion", sequence=True)
    live_zero = runtime.capture("NpcBankCust", "1", mode="motion", suffix="-zero-elevation", elevation=0,
                                sequence=True)
    require(live_base == live_zero, "Zero elevation changed live pixels, sequence or actual gameplay state")
    live_elevated = runtime.capture("NpcBankCust", "1", mode="motion", suffix="-elevated", elevation=30,
                                    sequence=True)
    verify_elevation(live_base, live_elevated, 30)
    require(live_base["actors"]["NpcBankCust"] == live_elevated["actors"]["NpcBankCust"] and
            live_elevated["actors"]["NpcBankCust"]["travel_distance"] > .001,
            "Elevated live inspection changed or froze the selected civilian's real patrol")
    require(live_base["state"]["heist_phase"] == live_elevated["state"]["heist_phase"] and
            live_base["profile"]["profiles"] == live_elevated["profile"]["profiles"],
            "Elevated inspection changed mission or character identity")
    # The camera is also the real player position. Guards can react to that
    # changed position; compare immutable routes/identities, not chase outcomes.
    for actor_id, before in live_base["actors"].items():
        after = live_elevated["actors"][actor_id]
        for field in ("id", "radius", "waypoints"):
            require(before.get(field) == after.get(field), f"Elevation changed {actor_id} {field}")
    require(live_base["pixels"] != live_elevated["pixels"] and
            live_elevated["sequence"][0] != live_elevated["sequence"][-1],
            "Elevated live camera did not capture changing gameplay")

    invalid = [
        ("missing-npc-id", ["--npc-view"], "Missing value"),
        ("unbounded-npc-view", ["--npc-view", "NpcGuard"], "requires --frames"),
        ("unknown-npc", ["--soft", "--npc-view", "NotAnActor", "--frames", 1], "Unknown NPC capture ID"),
        ("unsupported-player-motion", ["--soft", "--npc-motion", "PlayerBody", "--frames", 1], "motion needs live"),
        ("talk-without-motion", ["--npc-view", "NpcGuard", "--frames", 1, "--npc-talk"], "--npc-talk requires"),
        ("frozen-live-motion", ["--npc-motion", "NpcGuard", "--frames", 1, "--photo"], "--photo"),
        ("conflicting-world-view", ["--npc-view", "NpcGuard", "--view", "metro-wide", "--frames", 1], "cannot combine"),
        ("invalid-capture-fps", ["--capture-fps", "nan"], "Invalid value for --capture-fps"),
        ("invalid-capture-distance", ["--npc-distance", "0"], "Invalid value for --npc-distance"),
        ("invalid-capture-orbit", ["--npc-orbit", "nan"], "Invalid value for --npc-orbit"),
        ("out-of-range-capture-orbit", ["--npc-orbit", "181"], "Invalid value for --npc-orbit"),
        ("missing-capture-elevation", ["--npc-elevation"], "Missing value for --npc-elevation"),
    ]
    invalid += [(f"invalid-capture-elevation-{index}", ["--npc-elevation", value],
                 "Invalid value for --npc-elevation")
                for index, value in enumerate(("nan", "inf", "-inf", "60.01", "-60.01", "30degrees", "", "1e999"))]
    for name, args, diagnostic in invalid:
        log, _ = runtime.run(name, args, expected_success=False)
        require(diagnostic in log, f"{name}: rejected without the expected actionable diagnostic")
    for value in ("2", "-1", "true", "invalid", "01"):
        log, _ = runtime.run("invalid-detail-" + value.replace("-", "minus"),
                             ["--soft", "--npc-view", "NpcGuard", "--frames", 1],
                             detail=value, expected_success=False)
        require("FURY_NPC_DETAIL must be 0 or 1" in log, "Invalid detail toggle did not explain valid values")
    return differences


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vaultline", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--timeout", type=float, default=45, help="Seconds per application process")
    args = parser.parse_args()
    args.vaultline, args.source_root = args.vaultline.resolve(), args.source_root.resolve()
    require(args.vaultline.is_file(), "Missing Vaultline executable")
    require((args.source_root / "assets").is_dir(), "Real production assets are required")
    require(64 <= args.width <= 640 and 64 <= args.height <= 360 and args.timeout > 0,
            "Runtime-test dimensions/time bounds are invalid")
    if hasattr(os, "sched_getaffinity") and hasattr(os, "sched_setaffinity"):
        os.sched_setaffinity(0, sorted(os.sched_getaffinity(0))[:2])
    temporary = None
    if args.output_dir is None:
        temporary = tempfile.TemporaryDirectory(prefix="fury-npc-runtime-")
        output = Path(temporary.name)
    else:
        output = args.output_dir.resolve()
        output.mkdir(parents=True, exist_ok=True)
        require(not any(output.iterdir()), "Choose an empty output directory to preserve earlier evidence")
    runtime = Runtime(args, output)
    summary = {"schema": 1, "test_kind": "actual-vaultline-npc-runtime", "passed": False,
               "executable": str(args.vaultline), "executable_sha256": hashlib.sha256(args.vaultline.read_bytes()).hexdigest(),
               "test_script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
               "platform": platform.platform(), "max_cpu_workers": 2,
               "notes": ["State comparisons use the application's actual agent/crew/heist sidecar.",
                         "Capture duration is offline simulation time, not measured real-time performance.",
                         "Talk capture exercises a presentation gesture, not SDL Q input or dialogue UI.",
                         "Pixel differences establish a visible effect; they do not establish visual quality."]}
    status = 0
    try:
        summary["comparisons"] = run_checks(runtime)
        require(summary["executable_sha256"] == hashlib.sha256(args.vaultline.read_bytes()).hexdigest(),
                "Executable changed during validation; rerun against one finished build")
        require(summary["test_script_sha256"] == hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "Runtime test script changed during validation; rerun one unchanged test revision")
        summary["passed"] = True
        print("PASS actual NPC runtime: role portraits, unchanged gameplay state, live patrols, gesture and CLI validation", flush=True)
    except Exception as error:
        summary["error"] = str(error)
        print(f"FAIL {error}\nEvidence: {output}", file=sys.stderr, flush=True)
        status = 1
    finally:
        summary["runs"] = runtime.records
        (output / "npc-runtime-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        if temporary is not None:
            if status:
                retained = Path(tempfile.mkdtemp(prefix="fury-npc-runtime-failure-"))
                for path in output.iterdir():
                    if path.name == "work":
                        continue
                    if path.is_dir():
                        shutil.copytree(path, retained / path.name)
                    else:
                        shutil.copy2(path, retained / path.name)
                print(f"Failure evidence retained at {retained}", file=sys.stderr, flush=True)
            temporary.cleanup()
    return status


if __name__ == "__main__":
    sys.exit(main())
