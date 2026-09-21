#!/usr/bin/env python3
"""Author 5 Harbor Metro pedestrian OBJ+MTL meshes — Cycle-4 AAA characters.
Original assets only. Branding: Harbor Metro civilians — no third-party IP.

Cycle-4 upgrades (ChatGPT highest priority):
  proper proportions, facial topology, articulated hands, shoe construction,
  volumetric hair clumps, garment construction, skin/fabric/leather response,
  distinct silhouettes, natural idle/walk/conversation poses.
"""
from pathlib import Path
import math

OUT = Path("assets/meshes/harbor_metro/peds")
OUT.mkdir(parents=True, exist_ok=True)

# name, skin, shirt, pants, hair, shoes, style, height, pose
# pose: walk | idle | converse_a | converse_b | lean
PEDS = [
    ("hm_ped_rae",  (0.86, 0.68, 0.55), (0.16, 0.28, 0.52), (0.12, 0.14, 0.20),
     (0.10, 0.07, 0.05), (0.05, 0.05, 0.06), "jacket", 1.76, "walk"),
    ("hm_ped_dane", (0.50, 0.34, 0.24), (0.70, 0.18, 0.14), (0.10, 0.11, 0.14),
     (0.04, 0.03, 0.02), (0.09, 0.07, 0.05), "tee", 1.82, "idle"),
    ("hm_ped_suki", (0.92, 0.78, 0.68), (0.14, 0.48, 0.46), (0.20, 0.16, 0.30),
     (0.06, 0.04, 0.04), (0.78, 0.78, 0.80), "blouse", 1.64, "converse_a"),
    ("hm_ped_noah", (0.74, 0.54, 0.42), (0.26, 0.28, 0.34), (0.16, 0.20, 0.38),
     (0.12, 0.10, 0.08), (0.07, 0.07, 0.08), "hoodie", 1.74, "converse_b"),
    ("hm_ped_ivy",  (0.88, 0.72, 0.60), (0.68, 0.42, 0.18), (0.36, 0.22, 0.16),
     (0.48, 0.24, 0.12), (0.12, 0.06, 0.05), "coat", 1.68, "lean"),
]


def write_mtl(path, skin, shirt, pants, hair, shoes, style):
    def block(name, kd, ns, ks=0.12, emit=(0, 0, 0)):
        return (
            f"newmtl {name}\n"
            f"Ns {ns}\nKa 1 1 1\n"
            f"Kd {kd[0]:.4f} {kd[1]:.4f} {kd[2]:.4f}\n"
            f"Ks {ks:.3f} {ks:.3f} {ks:.3f}\n"
            f"Ke {emit[0]:.3f} {emit[1]:.3f} {emit[2]:.3f}\n"
            f"Ni 1.45\nd 1.0\nillum 2\n\n"
        )
    text = "# Harbor Metro original ped materials (Cycle-4)\n"
    # Skin: soft specular response
    text += block("Skin", skin, 28.0, ks=0.18)
    # Shirt/Jacket fabric
    fabric_ns = 8.0 if style in ("tee", "hoodie") else 14.0
    fabric_ks = 0.06 if style in ("tee", "hoodie") else 0.10
    text += block("Shirt", shirt, fabric_ns, ks=fabric_ks)
    text += block("Pants", pants, 12.0, ks=0.08)
    # Hair: anisotropic-ish higher Ns
    text += block("Hair", hair, 45.0, ks=0.22)
    # Shoes: leather / rubber
    shoe_ns = 55.0 if style == "blouse" else 35.0
    shoe_ks = 0.28 if style == "blouse" else 0.16
    text += block("Shoes", shoes, shoe_ns, ks=shoe_ks)
    # Secondary garment materials for construction reads
    if style == "jacket":
        text += block("Jacket", (shirt[0] * 0.85, shirt[1] * 0.9, shirt[2] * 1.05), 18.0, ks=0.14)
        text += block("Inner", (0.85, 0.85, 0.88), 10.0, ks=0.05)
    elif style == "coat":
        text += block("Jacket", (shirt[0] * 0.9, shirt[1] * 0.85, shirt[2] * 0.8), 22.0, ks=0.16)
    elif style == "hoodie":
        text += block("Jacket", (shirt[0] * 1.05, shirt[1] * 1.05, shirt[2] * 1.05), 7.0, ks=0.04)
    elif style == "blouse":
        text += block("Jacket", (shirt[0] * 1.1, shirt[1] * 1.05, shirt[2] * 0.95), 16.0, ks=0.12)
    else:
        text += block("Jacket", shirt, 9.0, ks=0.05)
    text += block("Belt", (0.08, 0.07, 0.06), 40.0, ks=0.25)
    text += block("EyeWhite", (0.92, 0.92, 0.94), 60.0, ks=0.35)
    text += block("Iris", (0.22, 0.28, 0.35), 80.0, ks=0.4)
    text += block("Lip", (skin[0] * 0.85, skin[1] * 0.55, skin[2] * 0.55), 35.0, ks=0.22)
    path.write_text(text)


class MeshWriter:
    def __init__(self, mtl_name):
        self.mtl = mtl_name
        self.v = []
        self.vn = []
        self.vt = []
        self.groups = []
        self.cur = None
        self.cur_faces = []

    def usemtl(self, name):
        if self.cur is not None:
            self.groups.append((self.cur, self.cur_faces))
        self.cur = name
        self.cur_faces = []

    def add_box(self, cx, cy, cz, sx, sy, sz, mat, yaw=0.0):
        self.usemtl(mat)
        hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
        corners = [
            (-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
            (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz),
        ]
        c, s = math.cos(yaw), math.sin(yaw)
        base = len(self.v)
        for x, y, z in corners:
            rx = x * c - z * s
            rz = x * s + z * c
            self.v.append((cx + rx, cy + y, cz + rz))
        faces_idx = [
            (0, 1, 2, 3, (0, 0, -1)),
            (5, 4, 7, 6, (0, 0, 1)),
            (4, 0, 3, 7, (-1, 0, 0)),
            (1, 5, 6, 2, (1, 0, 0)),
            (3, 2, 6, 7, (0, 1, 0)),
            (4, 5, 1, 0, (0, -1, 0)),
        ]
        for a, b, cidx, d, n in faces_idx:
            # rotate normal in XZ
            nx = n[0] * c - n[2] * s
            nz = n[0] * s + n[2] * c
            ni = len(self.vn)
            self.vn.append((nx, n[1], nz))
            u0 = len(self.vt)
            self.vt += [(0, 0), (1, 0), (1, 1), (0, 1)]
            ia, ib, ic, id_ = base + a + 1, base + b + 1, base + cidx + 1, base + d + 1
            self.cur_faces.append((ia, u0 + 1, ni + 1, ib, u0 + 2, ni + 1, ic, u0 + 3, ni + 1))
            self.cur_faces.append((ia, u0 + 1, ni + 1, ic, u0 + 3, ni + 1, id_, u0 + 4, ni + 1))

    def add_ellipsoid(self, cx, cy, cz, rx, ry, rz, mat, segs=12, stacks=10, yaw=0.0):
        self.usemtl(mat)
        base_v = len(self.v)
        base_n = len(self.vn)
        base_t = len(self.vt)
        cyaw, syaw = math.cos(yaw), math.sin(yaw)
        for i in range(stacks + 1):
            v = i / stacks
            phi = v * math.pi
            for j in range(segs):
                u = j / segs
                th = u * 2 * math.pi
                lx = rx * math.sin(phi) * math.cos(th)
                ly = ry * math.cos(phi)
                lz = rz * math.sin(phi) * math.sin(th)
                x = cx + lx * cyaw - lz * syaw
                z = cz + lx * syaw + lz * cyaw
                y = cy + ly
                nx = math.sin(phi) * math.cos(th)
                ny = math.cos(phi)
                nz = math.sin(phi) * math.sin(th)
                nrx = nx * cyaw - nz * syaw
                nrz = nx * syaw + nz * cyaw
                self.v.append((x, y, z))
                self.vn.append((nrx, ny, nrz))
                self.vt.append((u, v))
        for i in range(stacks):
            for j in range(segs):
                j2 = (j + 1) % segs
                i0 = base_v + i * segs + j
                i1 = base_v + i * segs + j2
                i2 = base_v + (i + 1) * segs + j2
                i3 = base_v + (i + 1) * segs + j
                def trip(a, b, c):
                    return (a + 1, base_t + (a - base_v) + 1, base_n + (a - base_v) + 1,
                            b + 1, base_t + (b - base_v) + 1, base_n + (b - base_v) + 1,
                            c + 1, base_t + (c - base_v) + 1, base_n + (c - base_v) + 1)
                self.cur_faces.append(trip(i0, i1, i2))
                self.cur_faces.append(trip(i0, i2, i3))

    def finish(self):
        if self.cur is not None:
            self.groups.append((self.cur, self.cur_faces))

    def write(self, path):
        self.finish()
        lines = [f"mtllib {self.mtl}", "o Ped"]
        for x, y, z in self.v:
            lines.append(f"v {x:.5f} {y:.5f} {z:.5f}")
        for x, y, z in self.vn:
            lines.append(f"vn {x:.5f} {y:.5f} {z:.5f}")
        for u, v in self.vt:
            lines.append(f"vt {u:.5f} {v:.5f}")
        for mat, faces in self.groups:
            if not faces:
                continue
            lines.append(f"usemtl {mat}")
            for f in faces:
                lines.append(
                    f"f {f[0]}/{f[1]}/{f[2]} {f[3]}/{f[4]}/{f[5]} {f[6]}/{f[7]}/{f[8]}"
                )
        path.write_text("\n".join(lines) + "\n")


def add_hand(w, cx, cy, cz, side, curl=0.15, yaw=0.0):
    """Palm + 4 articulated finger segments + thumb — readable AAA soft still."""
    s = -1.0 if side < 0 else 1.0
    # palm
    w.add_box(cx, cy, cz, 0.078, 0.048, 0.105, "Skin", yaw=yaw)
    # knuckle ridge
    w.add_box(cx, cy + 0.012, cz + 0.04, 0.075, 0.022, 0.03, "Skin", yaw=yaw)
    # fingers: proximal + distal
    for i, fx in enumerate([-0.030, -0.010, 0.010, 0.028]):
        flen = 0.048 - i * 0.002
        w.add_box(cx + fx * s, cy - 0.005 - curl * 0.02, cz + 0.075,
                  0.016, 0.018, flen, "Skin", yaw=yaw)
        w.add_box(cx + fx * s, cy - 0.012 - curl * 0.05, cz + 0.075 + flen * 0.85,
                  0.014, 0.015, flen * 0.7, "Skin", yaw=yaw + curl * 0.3 * s)
    # thumb
    w.add_box(cx + 0.048 * s, cy + 0.008, cz + 0.01, 0.020, 0.024, 0.042, "Skin",
              yaw=yaw + 0.4 * s)
    w.add_box(cx + 0.055 * s, cy + 0.002, cz + 0.04, 0.016, 0.018, 0.032, "Skin",
              yaw=yaw + 0.55 * s)


def add_shoe(w, cx, cy, cz, style, scale=1.0):
    """Sole + upper + heel + toe — leather/sneaker silhouette."""
    ss = scale
    # sole (darker via Shoes mat — slight extension)
    w.add_box(cx, cy, cz + 0.02, 0.125 * ss, 0.035, 0.30 * ss, "Shoes")
    # upper
    w.add_box(cx, cy + 0.045, cz - 0.01, 0.118 * ss, 0.07, 0.22 * ss, "Shoes")
    # toe box
    w.add_ellipsoid(cx, cy + 0.035, cz + 0.12 * ss, 0.055 * ss, 0.035, 0.05 * ss,
                    "Shoes", segs=8, stacks=5)
    # heel
    w.add_box(cx, cy + 0.02, cz - 0.11 * ss, 0.11 * ss, 0.055, 0.07 * ss, "Shoes")
    if style == "blouse":
        # slim heel lift
        w.add_box(cx, cy - 0.01, cz - 0.11 * ss, 0.04, 0.04, 0.04, "Shoes")
    elif style in ("tee", "hoodie"):
        # sneaker tongue + side stripe volume
        w.add_box(cx, cy + 0.08, cz + 0.02, 0.08 * ss, 0.03, 0.12 * ss, "Shoes")
        w.add_box(cx + 0.055 * ss, cy + 0.05, cz, 0.015, 0.04, 0.16 * ss, "Inner"
                  if style == "tee" else "Shirt")
    else:
        # lace ridge
        w.add_box(cx, cy + 0.075, cz + 0.02, 0.06, 0.02, 0.14 * ss, "Shoes")


def add_face(w, hx, hy, hz, style):
    """Facial topology: brow, nose bridge/tip, cheekbones, chin, lips, eyes, ears."""
    # brow ridge
    w.add_box(hx, hy + 0.055, hz + 0.085, 0.13, 0.022, 0.035, "Skin")
    # nose bridge + tip
    w.add_box(hx, hy + 0.02, hz + 0.115, 0.022, 0.055, 0.035, "Skin")
    w.add_ellipsoid(hx, hy - 0.005, hz + 0.135, 0.018, 0.016, 0.022, "Skin", segs=6, stacks=4)
    # cheekbones
    w.add_ellipsoid(hx - 0.07, hy + 0.01, hz + 0.06, 0.035, 0.028, 0.03, "Skin", segs=6, stacks=4)
    w.add_ellipsoid(hx + 0.07, hy + 0.01, hz + 0.06, 0.035, 0.028, 0.03, "Skin", segs=6, stacks=4)
    # jaw / chin
    w.add_box(hx, hy - 0.085, hz + 0.055, 0.09, 0.045, 0.07, "Skin")
    w.add_ellipsoid(hx, hy - 0.11, hz + 0.07, 0.04, 0.025, 0.035, "Skin", segs=6, stacks=4)
    # lips
    w.add_box(hx, hy - 0.055, hz + 0.11, 0.055, 0.014, 0.02, "Lip")
    w.add_box(hx, hy - 0.068, hz + 0.108, 0.05, 0.012, 0.018, "Lip")
    # eye sockets (indent via darker lids) + whites + iris
    for sx in (-1, 1):
        w.add_box(hx + sx * 0.038, hy + 0.03, hz + 0.10, 0.038, 0.018, 0.02, "Skin")
        w.add_ellipsoid(hx + sx * 0.038, hy + 0.028, hz + 0.112,
                        0.016, 0.011, 0.01, "EyeWhite", segs=6, stacks=4)
        w.add_ellipsoid(hx + sx * 0.038, hy + 0.028, hz + 0.118,
                        0.008, 0.008, 0.006, "Iris", segs=5, stacks=4)
    # ears
    ear_y = 0.005 if style != "blouse" else 0.0
    w.add_ellipsoid(hx - 0.105, hy + ear_y, hz, 0.022, 0.045, 0.018, "Skin", segs=7, stacks=5)
    w.add_ellipsoid(hx + 0.105, hy + ear_y, hz, 0.022, 0.045, 0.018, "Skin", segs=7, stacks=5)


def add_hair(w, hx, hy, hz, style):
    """Volumetric hair: scalp cap + strand clumps (not a single ellipsoid)."""
    if style == "blouse":
        # bob: scalp + side curtain clumps + fringe
        w.add_ellipsoid(hx, hy + 0.07, hz - 0.01, 0.125, 0.08, 0.13, "Hair", segs=14, stacks=10)
        for sx in (-1, 1):
            for i, dy in enumerate([0.02, -0.04, -0.10]):
                w.add_ellipsoid(hx + sx * (0.09 + i * 0.01), hy + dy, hz - 0.02,
                                0.04, 0.07, 0.05, "Hair", segs=7, stacks=5)
        # fringe
        for fx in (-0.05, -0.02, 0.02, 0.05):
            w.add_box(hx + fx, hy + 0.04, hz + 0.10, 0.03, 0.06, 0.025, "Hair")
    elif style == "coat":
        # long wavy: scalp + cascading strand boxes/ellipsoids
        w.add_ellipsoid(hx, hy + 0.06, hz - 0.02, 0.13, 0.09, 0.135, "Hair", segs=14, stacks=10)
        for sx in (-1, 1):
            for i, (dy, dz) in enumerate([(-0.02, -0.02), (-0.10, -0.01), (-0.18, 0.0), (-0.26, 0.02)]):
                w.add_ellipsoid(hx + sx * (0.07 + i * 0.008), hy + dy, hz + dz,
                                0.045, 0.06, 0.04, "Hair", segs=6, stacks=4)
        w.add_box(hx, hy - 0.14, hz - 0.05, 0.16, 0.22, 0.06, "Hair")
        # fringe
        for fx in (-0.04, 0.0, 0.04):
            w.add_box(hx + fx, hy + 0.05, hz + 0.11, 0.035, 0.05, 0.03, "Hair")
    elif style == "tee":
        # short fade crop — tight scalp clumps
        w.add_ellipsoid(hx, hy + 0.085, hz - 0.015, 0.108, 0.055, 0.115, "Hair", segs=12, stacks=8)
        for a in range(8):
            th = a * (math.pi * 2 / 8)
            w.add_ellipsoid(hx + math.cos(th) * 0.08, hy + 0.06, hz + math.sin(th) * 0.08 - 0.01,
                            0.03, 0.035, 0.03, "Hair", segs=5, stacks=4)
    elif style == "hoodie":
        # short under-hood + visible fringe at forehead
        w.add_ellipsoid(hx, hy + 0.075, hz - 0.01, 0.112, 0.06, 0.118, "Hair", segs=12, stacks=8)
        for fx in (-0.04, 0.0, 0.04):
            w.add_box(hx + fx, hy + 0.04, hz + 0.10, 0.03, 0.04, 0.025, "Hair")
    else:
        # Rae: side-part medium with volume
        w.add_ellipsoid(hx, hy + 0.07, hz - 0.015, 0.122, 0.075, 0.128, "Hair", segs=14, stacks=10)
        w.add_ellipsoid(hx + 0.05, hy + 0.02, hz + 0.02, 0.05, 0.08, 0.06, "Hair", segs=8, stacks=5)
        for i, dy in enumerate([0.0, -0.06, -0.12]):
            w.add_ellipsoid(hx + 0.08, hy + dy, hz - 0.01, 0.035, 0.05, 0.04, "Hair", segs=6, stacks=4)
        for fx in (-0.05, -0.02, 0.02):
            w.add_box(hx + fx, hy + 0.045, hz + 0.105, 0.03, 0.045, 0.025, "Hair")


def pose_offsets(pose, phase):
    """Return (arm_swing, leg_swing, torso_lean, head_yaw, weight_shift, arm_raise_L, arm_raise_R)."""
    if pose == "walk":
        swing = math.sin(phase) * 0.48
        return swing, swing, 0.02, 0.05, math.sin(phase) * 0.02, 0.0, 0.0
    if pose == "idle":
        return 0.08, 0.05, 0.0, -0.1, 0.03, 0.0, 0.0
    if pose == "converse_a":
        # Suki: weight on one leg, gesturing right hand
        return -0.12, 0.15, 0.04, 0.25, -0.04, 0.0, 0.22
    if pose == "converse_b":
        # Noah: facing Suki, hands in hoodie pocket-ish, listening lean
        return 0.05, -0.08, -0.03, -0.35, 0.02, -0.08, -0.08
    if pose == "lean":
        # Ivy: slight lean / waiting posture
        return 0.15, 0.1, 0.08, 0.15, 0.05, 0.18, -0.05
    return 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0


def build_ped(name, skin, shirt, pants, hair, shoes, style, height, pose, phase=0.0):
    arm_sw, leg_sw, torso_lean, head_yaw, wshift, raise_L, raise_R = pose_offsets(pose, phase)
    idle = math.cos(phase * 0.5) * 0.015
    w = MeshWriter(name + ".mtl")
    h_scale = height / 1.72

    # Body proportions — 7.5–8 head canon-ish, style-driven silhouettes
    torso_w, shoulder, hip_w = 0.38, 0.42, 0.34
    if style == "tee":
        torso_w, shoulder, hip_w = 0.46, 0.50, 0.38  # stocky Dane
    elif style == "blouse":
        torso_w, shoulder, hip_w = 0.32, 0.36, 0.36
    elif style == "hoodie":
        torso_w, shoulder, hip_w = 0.42, 0.46, 0.36
    elif style == "coat":
        torso_w, shoulder, hip_w = 0.36, 0.42, 0.34
    elif style == "jacket":
        torso_w, shoulder, hip_w = 0.40, 0.44, 0.34

    # --- Lower body ---
    w.add_box(wshift, 0.94 * h_scale, torso_lean * 0.5, hip_w, 0.16, 0.22, "Pants")
    w.add_box(wshift, 1.04 * h_scale, 0.01 + torso_lean * 0.5, hip_w * 1.02, 0.045, 0.24, "Belt")
    # belt buckle
    w.add_box(wshift, 1.04 * h_scale, 0.12 + torso_lean * 0.5, 0.05, 0.04, 0.02, "Belt")

    # --- Torso / garment construction ---
    torso_y = 1.26 * h_scale + idle
    w.add_box(wshift, torso_y, 0.01 + torso_lean, torso_w, 0.46, 0.24, "Shirt")

    if style == "jacket":
        # open jacket panels + lapels + inner tee
        w.add_box(wshift, torso_y, 0.02 + torso_lean, torso_w * 0.7, 0.42, 0.18, "Inner")
        for sx in (-1, 1):
            w.add_box(wshift + sx * 0.15, torso_y - 0.02, 0.12 + torso_lean,
                      0.15, 0.48, 0.08, "Jacket")
            # lapel
            w.add_box(wshift + sx * 0.08, torso_y + 0.14, 0.14 + torso_lean,
                      0.08, 0.18, 0.04, "Jacket")
        # cuff tabs at hem
        w.add_box(wshift, torso_y - 0.22, 0.13 + torso_lean, torso_w * 0.95, 0.04, 0.06, "Jacket")
        # collar
        w.add_box(wshift, 1.48 * h_scale + idle, 0.06 + torso_lean, 0.18, 0.05, 0.12, "Jacket")
    elif style == "coat":
        # long coat body + lapels + belt
        w.add_box(wshift, 0.88 * h_scale, 0.05 + torso_lean, torso_w * 1.08, 0.58, 0.28, "Jacket")
        w.add_box(wshift, torso_y + 0.02, 0.08 + torso_lean, shoulder, 0.50, 0.30, "Jacket")
        for sx in (-1, 1):
            w.add_box(wshift + sx * 0.08, torso_y + 0.12, 0.16 + torso_lean,
                      0.09, 0.20, 0.05, "Jacket")
        w.add_box(wshift, 1.05 * h_scale, 0.16 + torso_lean, torso_w * 1.05, 0.05, 0.06, "Belt")
        w.add_box(wshift, 1.48 * h_scale + idle, 0.08 + torso_lean, 0.18, 0.06, 0.14, "Jacket")
    elif style == "hoodie":
        w.add_box(wshift, torso_y, 0.03 + torso_lean, torso_w * 1.05, 0.48, 0.28, "Jacket")
        # hood volume (behind head)
        w.add_ellipsoid(wshift, 1.55 * h_scale + idle, -0.06 + torso_lean,
                        0.15, 0.11, 0.14, "Jacket", segs=10, stacks=7)
        # kangaroo pocket
        w.add_box(wshift, 1.12 * h_scale, 0.16 + torso_lean, 0.24, 0.14, 0.07, "Jacket")
        # cuff ribbing
        for sx in (-1, 1):
            w.add_box(wshift + sx * 0.28, 1.08 * h_scale, 0.04, 0.11, 0.06, 0.11, "Jacket")
        # hem ribbing
        w.add_box(wshift, 1.02 * h_scale, 0.04 + torso_lean, torso_w * 1.08, 0.05, 0.26, "Jacket")
    elif style == "blouse":
        w.add_box(wshift, torso_y, 0.02 + torso_lean, torso_w, 0.44, 0.22, "Shirt")
        # peplum
        w.add_box(wshift, 1.02 * h_scale, 0.04 + torso_lean, hip_w * 1.15, 0.14, 0.28, "Jacket")
        # collar / neckline
        w.add_box(wshift, 1.47 * h_scale + idle, 0.05 + torso_lean, 0.14, 0.05, 0.10, "Shirt")
        # sleeve puff hint at shoulder
        for sx in (-1, 1):
            w.add_ellipsoid(wshift + sx * 0.20, 1.40 * h_scale + idle, 0.02 + torso_lean,
                            0.08, 0.07, 0.08, "Shirt", segs=8, stacks=5)
    else:  # tee
        w.add_box(wshift, torso_y, 0.01 + torso_lean, torso_w, 0.44, 0.24, "Shirt")
        # crew collar
        w.add_ellipsoid(wshift, 1.48 * h_scale + idle, 0.02 + torso_lean,
                        0.09, 0.04, 0.08, "Shirt", segs=8, stacks=4)
        # short sleeve cuffs
        for sx in (-1, 1):
            w.add_box(wshift + sx * 0.28, 1.22 * h_scale + idle, 0.02,
                      0.12, 0.08, 0.12, "Shirt")

    # buttons / snaps (visible construction)
    if style in ("jacket", "coat", "blouse"):
        for i in range(4):
            by = torso_y + 0.14 - i * 0.10
            w.add_ellipsoid(wshift, by, 0.14 + torso_lean, 0.012, 0.012, 0.01, "Belt", segs=5, stacks=3)

    # --- Neck / head ---
    w.add_box(wshift, 1.51 * h_scale + idle, 0.01 + torso_lean, 0.095, 0.11, 0.095, "Skin")
    hx = wshift + math.sin(head_yaw) * 0.02
    hy = 1.63 * h_scale + idle
    hz = 0.02 + torso_lean + math.cos(head_yaw) * 0.01
    # higher-res skull
    w.add_ellipsoid(hx, hy, hz, 0.102, 0.122, 0.112, "Skin", segs=16, stacks=14, yaw=head_yaw)
    add_face(w, hx, hy, hz, style)
    add_hair(w, hx, hy, hz, style)

    # --- Arms ---
    arm_mat = "Jacket" if style in ("jacket", "coat", "hoodie") else "Shirt"
    # upper arms
    w.add_box(wshift - 0.27, 1.30 * h_scale + idle + raise_L * 0.15,
              0.02 + arm_sw * 0.06 + torso_lean, 0.095, 0.36, 0.095, arm_mat)
    w.add_box(wshift + 0.27, 1.30 * h_scale + idle + raise_R * 0.15,
              0.02 - arm_sw * 0.06 + torso_lean, 0.095, 0.36, 0.095, arm_mat)
    # forearms (skin or sleeve)
    forearm_mat = "Skin" if style in ("tee", "blouse") else arm_mat
    if style == "blouse":
        forearm_mat = "Shirt"
    w.add_box(wshift - 0.29, 1.00 * h_scale + raise_L * 0.25,
              0.06 + arm_sw * 0.16 + torso_lean, 0.08, 0.30, 0.08, forearm_mat)
    w.add_box(wshift + 0.29, 1.00 * h_scale + raise_R * 0.25,
              0.06 - arm_sw * 0.16 + torso_lean, 0.08, 0.30, 0.08, forearm_mat)
    # hands
    add_hand(w, wshift - 0.29, 0.84 * h_scale + raise_L * 0.35,
             0.12 + arm_sw * 0.20, -1, curl=0.2 if pose.startswith("converse") else 0.1)
    add_hand(w, wshift + 0.29, 0.84 * h_scale + raise_R * 0.35,
             0.12 - arm_sw * 0.20, 1, curl=0.35 if pose == "converse_a" else 0.1)

    # --- Legs ---
    thigh_mat = "Pants"
    w.add_box(wshift - 0.10, 0.70 * h_scale, leg_sw * 0.12, 0.14, 0.40, 0.14, thigh_mat)
    w.add_box(wshift + 0.10, 0.70 * h_scale, -leg_sw * 0.12, 0.14, 0.40, 0.14, thigh_mat)
    # knees
    w.add_ellipsoid(wshift - 0.10, 0.50 * h_scale, leg_sw * 0.14,
                    0.07, 0.055, 0.075, thigh_mat, segs=6, stacks=4)
    w.add_ellipsoid(wshift + 0.10, 0.50 * h_scale, -leg_sw * 0.14,
                    0.07, 0.055, 0.075, thigh_mat, segs=6, stacks=4)
    # shins
    w.add_box(wshift - 0.10, 0.30 * h_scale, leg_sw * 0.18, 0.11, 0.36, 0.11, thigh_mat)
    w.add_box(wshift + 0.10, 0.30 * h_scale, -leg_sw * 0.18, 0.11, 0.36, 0.11, thigh_mat)
    # pant cuffs / break
    w.add_box(wshift - 0.10, 0.12 * h_scale, leg_sw * 0.18, 0.12, 0.05, 0.125, thigh_mat)
    w.add_box(wshift + 0.10, 0.12 * h_scale, -leg_sw * 0.18, 0.12, 0.05, 0.125, thigh_mat)

    # shoes
    shoe_scale = 0.92 if style == "blouse" else (1.08 if style == "tee" else 1.0)
    add_shoe(w, wshift - 0.10, 0.055 * h_scale, 0.04 + leg_sw * 0.20, style, shoe_scale)
    add_shoe(w, wshift + 0.10, 0.055 * h_scale, 0.04 - leg_sw * 0.20, style, shoe_scale)

    write_mtl(OUT / (name + ".mtl"), skin, shirt, pants, hair, shoes, style)
    w.write(OUT / (name + ".obj"))
    print("wrote", name, "style", style, "pose", pose, "verts", len(w.v), "h", round(height, 2))


for i, spec in enumerate(PEDS):
    build_ped(*spec, phase=i * 0.9 + 0.35)

print("done ->", OUT)
