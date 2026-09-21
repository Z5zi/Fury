#!/usr/bin/env python3
"""Author 5 Harbor Metro pedestrian OBJ+MTL meshes — Cycle-5 AAA characters.
Original assets only. Branding: Harbor Metro civilians — no third-party IP.

Cycle-5 (pixel quality over count):
  capsule/ellipsoid limbs (no box toy look), readable facial topology at soft
  capture distance, articulated hands, shoe construction, hair masses,
  fabric/leather/rubber MTL response, SSS-ish skin, weight-shift poses.
"""
from pathlib import Path
import math

OUT = Path("assets/meshes/harbor_metro/peds")
OUT.mkdir(parents=True, exist_ok=True)

# name, skin, shirt, pants, hair, shoes, style, height, pose
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
    text = "# Harbor Metro original ped materials (Cycle-5)\n"
    # Skin: soft specular + slight warmth (SSS-ish via wrap in soft path)
    text += block("Skin", skin, 32.0, ks=0.22)
    fabric_ns = 6.0 if style in ("tee", "hoodie") else 12.0
    fabric_ks = 0.05 if style in ("tee", "hoodie") else 0.09
    text += block("Shirt", shirt, fabric_ns, ks=fabric_ks)
    text += block("Pants", pants, 10.0, ks=0.07)
    text += block("Hair", hair, 55.0, ks=0.28)
    # Leather / rubber shoes
    if style == "blouse":
        text += block("Shoes", shoes, 70.0, ks=0.35)  # patent leather
    elif style in ("tee", "hoodie"):
        text += block("Shoes", shoes, 25.0, ks=0.10)  # rubber sneakers
    else:
        text += block("Shoes", shoes, 42.0, ks=0.20)  # leather
    text += block("ShoeSole", (0.04, 0.04, 0.045), 8.0, ks=0.04)
    text += block("ShoeLace", (0.85, 0.85, 0.82), 15.0, ks=0.08)
    if style == "jacket":
        text += block("Jacket", (shirt[0] * 0.85, shirt[1] * 0.9, shirt[2] * 1.05), 20.0, ks=0.16)
        text += block("Inner", (0.85, 0.85, 0.88), 8.0, ks=0.04)
    elif style == "coat":
        text += block("Jacket", (shirt[0] * 0.9, shirt[1] * 0.85, shirt[2] * 0.8), 28.0, ks=0.18)
    elif style == "hoodie":
        text += block("Jacket", (shirt[0] * 1.05, shirt[1] * 1.05, shirt[2] * 1.05), 5.0, ks=0.03)
    elif style == "blouse":
        text += block("Jacket", (shirt[0] * 1.1, shirt[1] * 1.05, shirt[2] * 0.95), 18.0, ks=0.11)
    else:
        text += block("Jacket", shirt, 7.0, ks=0.04)
    text += block("Belt", (0.08, 0.07, 0.06), 48.0, ks=0.30)
    text += block("Buckle", (0.72, 0.68, 0.55), 90.0, ks=0.55)
    text += block("EyeWhite", (0.94, 0.94, 0.96), 80.0, ks=0.45)
    text += block("Iris", (0.18, 0.26, 0.34), 100.0, ks=0.5)
    text += block("Pupil", (0.02, 0.02, 0.03), 20.0, ks=0.05)
    text += block("Lip", (skin[0] * 0.82, skin[1] * 0.48, skin[2] * 0.50), 40.0, ks=0.28)
    text += block("Brow", (hair[0] * 0.9, hair[1] * 0.9, hair[2] * 0.9), 20.0, ks=0.08)
    text += block("Phone", (0.08, 0.08, 0.10), 60.0, ks=0.40)
    text += block("PhoneScreen", (0.15, 0.35, 0.55), 10.0, ks=0.05, emit=(0.25, 0.45, 0.70))
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

    def add_capsule(self, x0, y0, z0, x1, y1, z1, radius, mat, segs=12, yaw_extra=0.0):
        """Smooth oriented capsule (cylinder + hemisphere caps) — no bead stacking."""
        self.usemtl(mat)
        dx, dy, dz = x1 - x0, y1 - y0, z1 - z0
        length = math.sqrt(dx * dx + dy * dy + dz * dz) + 1e-6
        # Orthonormal basis: axis = u, plus n, b
        ux, uy, uz = dx / length, dy / length, dz / length
        # pick a helper not parallel to u
        if abs(uy) < 0.9:
            hx, hy, hz = 0.0, 1.0, 0.0
        else:
            hx, hy, hz = 1.0, 0.0, 0.0
        # n = normalize(helper × u)
        nx = hy * uz - hz * uy
        ny = hz * ux - hx * uz
        nz = hx * uy - hy * ux
        nl = math.sqrt(nx * nx + ny * ny + nz * nz) + 1e-6
        nx, ny, nz = nx / nl, ny / nl, nz / nl
        # b = u × n
        bx = uy * nz - uz * ny
        by = uz * nx - ux * nz
        bz = ux * ny - uy * nx
        # Rings along capsule: hemi start, shaft, hemi end
        # Parameter s in [0, length], with spherical rounding at ends
        rings = []  # list of (cx,cy,cz, ring_radius, ring_normal_blend)
        hemi_rings = max(4, segs // 3)
        shaft_rings = max(3, int(length / (radius * 0.85)))
        # start hemisphere (from pole to equator), center at x0
        for i in range(hemi_rings):
            a = (math.pi * 0.5) * (i / max(1, hemi_rings - 1))  # 0..pi/2
            # from pole (along -u) toward equator
            along = -math.cos(a) * radius
            rr = math.sin(a) * radius
            cx = x0 + ux * along
            cy = y0 + uy * along
            cz = z0 + uz * along
            rings.append((cx, cy, cz, rr, -math.cos(a), math.sin(a)))
        # shaft
        for i in range(1, shaft_rings):
            tpar = i / shaft_rings
            cx = x0 + dx * tpar
            cy = y0 + dy * tpar
            cz = z0 + dz * tpar
            rings.append((cx, cy, cz, radius, 0.0, 1.0))
        # end hemisphere
        for i in range(hemi_rings):
            a = (math.pi * 0.5) * (i / max(1, hemi_rings - 1))  # 0..pi/2
            along = math.sin(a) * radius  # wait: from equator to pole along +u
            # i=0 at equator (along=0), i=last at pole (along=radius)
            along = math.sin(a) * radius
            rr = math.cos(a) * radius
            cx = x1 + ux * along
            cy = y1 + uy * along
            cz = z1 + uz * along
            # actually better: equator at x1, pole beyond x1
            along = math.sin(a) * radius
            rr = math.cos(a) * radius
            cx = x1 + ux * along
            cy = y1 + uy * along
            cz = z1 + uz * along
            rings.append((cx, cy, cz, rr, math.sin(a), math.cos(a)))

        # Dedup / fix end hemi: rebuild cleanly
        rings = []
        # Start hemi: pole at x0 - u*r → equator at x0
        for i in range(hemi_rings):
            a = (math.pi * 0.5) * (i / max(1, hemi_rings - 1))
            # a=0 pole, a=pi/2 equator
            along = -math.cos(a) * radius  # -r .. 0 relative to x0
            rr = math.sin(a) * radius
            rings.append((x0 + ux * along, y0 + uy * along, z0 + uz * along, rr,
                          -math.cos(a), math.sin(a)))
        # Shaft x0 → x1
        for i in range(1, shaft_rings):
            tpar = i / shaft_rings
            rings.append((x0 + dx * tpar, y0 + dy * tpar, z0 + dz * tpar, radius, 0.0, 1.0))
        # End hemi: equator at x1 → pole at x1 + u*r
        for i in range(hemi_rings):
            a = (math.pi * 0.5) * (i / max(1, hemi_rings - 1))
            # a=0 equator, a=pi/2 pole
            along = math.sin(a) * radius
            rr = math.cos(a) * radius
            rings.append((x1 + ux * along, y1 + uy * along, z1 + uz * along, rr,
                          math.sin(a), math.cos(a)))

        # Emit ring vertices
        ring_base = []
        for (cx, cy, cz, rr, na, nr) in rings:
            base = len(self.v)
            ring_base.append(base)
            for j in range(segs):
                th = (j / segs) * 2 * math.pi
                ct, st = math.cos(th), math.sin(th)
                # radial direction in n-b plane
                rx = nx * ct + bx * st
                ry = ny * ct + by * st
                rz = nz * ct + bz * st
                self.v.append((cx + rx * rr, cy + ry * rr, cz + rz * rr))
                # normal = blend axis + radial
                ngx = ux * na + rx * nr
                ngy = uy * na + ry * nr
                ngz = uz * na + rz * nr
                gl = math.sqrt(ngx*ngx + ngy*ngy + ngz*ngz) + 1e-6
                self.vn.append((ngx/gl, ngy/gl, ngz/gl))
                self.vt.append((j / segs, 0.5))
        # Stitch quads between rings
        for ri in range(len(rings) - 1):
            b0 = ring_base[ri]
            b1 = ring_base[ri + 1]
            for j in range(segs):
                j2 = (j + 1) % segs
                i00, i01 = b0 + j, b0 + j2
                i10, i11 = b1 + j, b1 + j2
                def trip(a, b, c):
                    return (a+1, a+1, a+1, b+1, b+1, b+1, c+1, c+1, c+1)
                # use matching vn/vt indices (= vertex index since 1:1)
                def trip2(a, b, c):
                    # v/vt/vn all share offset from 0; indices are 1-based same
                    return (a+1, a+1, a+1, b+1, b+1, b+1, c+1, c+1, c+1)
                self.cur_faces.append(trip2(i00, i01, i11))
                self.cur_faces.append(trip2(i00, i11, i10))


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


def add_hand(w, cx, cy, cz, side, curl=0.15, yaw=0.0, hold_phone=False):
    """Palm + articulated fingers — readable at soft capture distance."""
    s = -1.0 if side < 0 else 1.0
    # palm (ellipsoid mass, not a box)
    w.add_ellipsoid(cx, cy, cz, 0.042, 0.028, 0.055, "Skin", segs=10, stacks=7, yaw=yaw)
    w.add_ellipsoid(cx, cy + 0.01, cz + 0.035, 0.038, 0.016, 0.022, "Skin", segs=8, stacks=5, yaw=yaw)
    for i, fx in enumerate([-0.028, -0.010, 0.010, 0.026]):
        flen = 0.052 - i * 0.003
        # proximal
        w.add_capsule(
            cx + fx * s, cy - 0.004 - curl * 0.015, cz + 0.055,
            cx + fx * s, cy - 0.010 - curl * 0.04, cz + 0.055 + flen * 0.55,
            0.009, "Skin", segs=6)
        # distal
        w.add_capsule(
            cx + fx * s, cy - 0.010 - curl * 0.04, cz + 0.055 + flen * 0.55,
            cx + fx * s + curl * 0.01 * s, cy - 0.018 - curl * 0.06, cz + 0.055 + flen,
            0.0075, "Skin", segs=6)
    # thumb
    w.add_capsule(
        cx + 0.040 * s, cy + 0.006, cz + 0.005,
        cx + 0.052 * s, cy - 0.002, cz + 0.038,
        0.010, "Skin", segs=6)
    if hold_phone:
        w.add_box(cx + 0.01 * s, cy + 0.02, cz + 0.08, 0.07, 0.012, 0.13, "Phone", yaw=yaw + 0.15 * s)
        w.add_box(cx + 0.01 * s, cy + 0.027, cz + 0.08, 0.055, 0.004, 0.10, "PhoneScreen", yaw=yaw + 0.15 * s)


def add_shoe(w, cx, cy, cz, style, scale=1.0):
    ss = scale
    # rubber/leather sole lip
    w.add_ellipsoid(cx, cy, cz + 0.02, 0.065 * ss, 0.022, 0.155 * ss, "ShoeSole", segs=10, stacks=6)
    # upper volume
    w.add_ellipsoid(cx, cy + 0.045, cz - 0.01, 0.060 * ss, 0.040, 0.12 * ss, "Shoes", segs=10, stacks=7)
    # toe box
    w.add_ellipsoid(cx, cy + 0.032, cz + 0.12 * ss, 0.052 * ss, 0.032, 0.048 * ss, "Shoes", segs=9, stacks=6)
    # heel counter
    w.add_ellipsoid(cx, cy + 0.035, cz - 0.10 * ss, 0.055 * ss, 0.038, 0.040 * ss, "Shoes", segs=8, stacks=5)
    if style == "blouse":
        w.add_ellipsoid(cx, cy - 0.005, cz - 0.10 * ss, 0.022, 0.028, 0.022, "Shoes", segs=6, stacks=5)
    elif style in ("tee", "hoodie"):
        w.add_ellipsoid(cx, cy + 0.075, cz + 0.02, 0.040 * ss, 0.022, 0.065 * ss, "Shoes", segs=7, stacks=5)
        w.add_box(cx + 0.052 * ss, cy + 0.048, cz + 0.01, 0.012, 0.035, 0.14 * ss, "ShoeLace")
        # lace stubs
        for lz in (-0.02, 0.02, 0.06):
            w.add_ellipsoid(cx, cy + 0.078, cz + lz, 0.012, 0.008, 0.010, "ShoeLace", segs=5, stacks=3)
    else:
        w.add_box(cx, cy + 0.072, cz + 0.02, 0.05 * ss, 0.016, 0.12 * ss, "Shoes")
        w.add_ellipsoid(cx, cy + 0.055, cz - 0.02, 0.02, 0.015, 0.02, "Buckle", segs=5, stacks=3)


def add_face(w, hx, hy, hz, style):
    """Exaggerated-but-believable facial forms for soft still readability."""
    # brow ridge + brows
    w.add_ellipsoid(hx, hy + 0.058, hz + 0.088, 0.072, 0.016, 0.028, "Skin", segs=10, stacks=5)
    for sx in (-1, 1):
        w.add_ellipsoid(hx + sx * 0.038, hy + 0.062, hz + 0.100, 0.028, 0.008, 0.012, "Brow", segs=6, stacks=3)
    # nose bridge + tip + nostrils
    w.add_ellipsoid(hx, hy + 0.018, hz + 0.118, 0.016, 0.040, 0.028, "Skin", segs=8, stacks=8)
    w.add_ellipsoid(hx, hy - 0.008, hz + 0.142, 0.020, 0.016, 0.024, "Skin", segs=8, stacks=6)
    w.add_ellipsoid(hx - 0.012, hy - 0.012, hz + 0.138, 0.008, 0.006, 0.008, "Skin", segs=5, stacks=3)
    w.add_ellipsoid(hx + 0.012, hy - 0.012, hz + 0.138, 0.008, 0.006, 0.008, "Skin", segs=5, stacks=3)
    # cheekbones
    w.add_ellipsoid(hx - 0.072, hy + 0.008, hz + 0.070, 0.038, 0.032, 0.032, "Skin", segs=8, stacks=6)
    w.add_ellipsoid(hx + 0.072, hy + 0.008, hz + 0.070, 0.038, 0.032, 0.032, "Skin", segs=8, stacks=6)
    # jaw / chin
    w.add_ellipsoid(hx, hy - 0.090, hz + 0.055, 0.055, 0.032, 0.045, "Skin", segs=10, stacks=6)
    w.add_ellipsoid(hx, hy - 0.115, hz + 0.075, 0.038, 0.022, 0.032, "Skin", segs=8, stacks=5)
    # lips (upper + lower)
    w.add_ellipsoid(hx, hy - 0.052, hz + 0.118, 0.032, 0.010, 0.014, "Lip", segs=8, stacks=4)
    w.add_ellipsoid(hx, hy - 0.068, hz + 0.116, 0.030, 0.011, 0.013, "Lip", segs=8, stacks=4)
    # eyes: socket, white, iris, pupil, lid
    for sx in (-1, 1):
        ex = hx + sx * 0.040
        ey = hy + 0.028
        ez = hz + 0.108
        w.add_ellipsoid(ex, ey + 0.012, ez - 0.005, 0.022, 0.010, 0.014, "Skin", segs=6, stacks=4)
        w.add_ellipsoid(ex, ey, ez + 0.008, 0.018, 0.013, 0.012, "EyeWhite", segs=8, stacks=6)
        w.add_ellipsoid(ex + sx * 0.002, ey, ez + 0.016, 0.010, 0.010, 0.008, "Iris", segs=7, stacks=5)
        w.add_ellipsoid(ex + sx * 0.002, ey, ez + 0.020, 0.005, 0.005, 0.004, "Pupil", segs=5, stacks=4)
        w.add_ellipsoid(ex, ey - 0.012, ez + 0.004, 0.020, 0.008, 0.012, "Skin", segs=6, stacks=3)
    # ears with lobe
    ear_y = 0.0
    for sx in (-1, 1):
        w.add_ellipsoid(hx + sx * 0.112, hy + ear_y, hz - 0.005, 0.024, 0.048, 0.020, "Skin", segs=9, stacks=7)
        w.add_ellipsoid(hx + sx * 0.118, hy + ear_y - 0.028, hz, 0.014, 0.018, 0.012, "Skin", segs=6, stacks=4)


def add_hair(w, hx, hy, hz, style):
    if style == "blouse":
        w.add_ellipsoid(hx, hy + 0.075, hz - 0.015, 0.130, 0.085, 0.135, "Hair", segs=16, stacks=12)
        for sx in (-1, 1):
            for i, dy in enumerate([0.02, -0.05, -0.12, -0.18]):
                w.add_ellipsoid(hx + sx * (0.095 + i * 0.008), hy + dy, hz - 0.02 + i * 0.01,
                                0.042, 0.065, 0.048, "Hair", segs=8, stacks=6)
        for fx in (-0.055, -0.022, 0.022, 0.055):
            w.add_ellipsoid(hx + fx, hy + 0.035, hz + 0.105, 0.020, 0.040, 0.018, "Hair", segs=6, stacks=4)
    elif style == "coat":
        w.add_ellipsoid(hx, hy + 0.065, hz - 0.02, 0.135, 0.095, 0.140, "Hair", segs=16, stacks=12)
        for sx in (-1, 1):
            for i, (dy, dz) in enumerate([(-0.02, -0.02), (-0.10, -0.01), (-0.18, 0.0), (-0.28, 0.02), (-0.36, 0.04)]):
                w.add_ellipsoid(hx + sx * (0.075 + i * 0.01), hy + dy, hz + dz,
                                0.048, 0.055, 0.042, "Hair", segs=7, stacks=5)
        w.add_ellipsoid(hx, hy - 0.16, hz - 0.04, 0.10, 0.14, 0.05, "Hair", segs=10, stacks=6)
        for fx in (-0.045, 0.0, 0.045):
            w.add_ellipsoid(hx + fx, hy + 0.045, hz + 0.115, 0.022, 0.038, 0.020, "Hair", segs=6, stacks=4)
    elif style == "tee":
        w.add_ellipsoid(hx, hy + 0.090, hz - 0.015, 0.112, 0.058, 0.118, "Hair", segs=14, stacks=10)
        for a in range(10):
            th = a * (math.pi * 2 / 10)
            w.add_ellipsoid(hx + math.cos(th) * 0.085, hy + 0.055, hz + math.sin(th) * 0.085 - 0.01,
                            0.028, 0.032, 0.028, "Hair", segs=6, stacks=4)
    elif style == "hoodie":
        w.add_ellipsoid(hx, hy + 0.078, hz - 0.01, 0.115, 0.062, 0.120, "Hair", segs=14, stacks=10)
        for fx in (-0.045, 0.0, 0.045):
            w.add_ellipsoid(hx + fx, hy + 0.038, hz + 0.105, 0.020, 0.032, 0.018, "Hair", segs=6, stacks=4)
    else:
        w.add_ellipsoid(hx, hy + 0.072, hz - 0.015, 0.125, 0.080, 0.132, "Hair", segs=16, stacks=12)
        w.add_ellipsoid(hx + 0.055, hy + 0.015, hz + 0.02, 0.055, 0.085, 0.060, "Hair", segs=10, stacks=7)
        for i, dy in enumerate([0.0, -0.07, -0.14]):
            w.add_ellipsoid(hx + 0.085, hy + dy, hz - 0.01, 0.038, 0.055, 0.042, "Hair", segs=7, stacks=5)
        for fx in (-0.05, -0.02, 0.02):
            w.add_ellipsoid(hx + fx, hy + 0.042, hz + 0.110, 0.020, 0.035, 0.018, "Hair", segs=6, stacks=4)


def pose_offsets(pose, phase):
    if pose == "walk":
        swing = math.sin(phase) * 0.55
        return swing, swing, 0.04, 0.08, math.sin(phase) * 0.035, 0.0, 0.0
    if pose == "idle":
        return 0.12, 0.08, 0.02, -0.12, 0.045, 0.05, -0.02
    if pose == "converse_a":
        return -0.15, 0.22, 0.06, 0.35, -0.055, 0.05, 0.35
    if pose == "converse_b":
        return 0.08, -0.12, -0.05, -0.40, 0.035, -0.12, -0.10
    if pose == "lean":
        return 0.20, 0.14, 0.12, 0.18, 0.06, 0.25, -0.08
    return 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0


def build_ped(name, skin, shirt, pants, hair, shoes, style, height, pose, phase=0.0):
    arm_sw, leg_sw, torso_lean, head_yaw, wshift, raise_L, raise_R = pose_offsets(pose, phase)
    idle = math.cos(phase * 0.5) * 0.018
    w = MeshWriter(name + ".mtl")
    h_scale = height / 1.72

    torso_w, shoulder, hip_w = 0.38, 0.42, 0.34
    if style == "tee":
        torso_w, shoulder, hip_w = 0.46, 0.50, 0.38
    elif style == "blouse":
        torso_w, shoulder, hip_w = 0.32, 0.36, 0.36
    elif style == "hoodie":
        torso_w, shoulder, hip_w = 0.42, 0.46, 0.36
    elif style == "coat":
        torso_w, shoulder, hip_w = 0.36, 0.42, 0.34
    elif style == "jacket":
        torso_w, shoulder, hip_w = 0.40, 0.44, 0.34

    # hips / belt (soft volumes)
    w.add_ellipsoid(wshift, 0.94 * h_scale, torso_lean * 0.5, hip_w * 0.52, 0.09, 0.13, "Pants", segs=12, stacks=8)
    w.add_ellipsoid(wshift, 1.04 * h_scale, 0.01 + torso_lean * 0.5, hip_w * 0.55, 0.028, 0.14, "Belt", segs=10, stacks=5)
    w.add_ellipsoid(wshift, 1.04 * h_scale, 0.13 + torso_lean * 0.5, 0.028, 0.022, 0.014, "Buckle", segs=6, stacks=4)

    torso_y = 1.26 * h_scale + idle
    # torso as ellipsoid (not a box)
    w.add_ellipsoid(wshift, torso_y, 0.01 + torso_lean, torso_w * 0.52, 0.26, 0.14, "Shirt", segs=14, stacks=12)

    if style == "jacket":
        w.add_ellipsoid(wshift, torso_y, 0.02 + torso_lean, torso_w * 0.38, 0.24, 0.10, "Inner", segs=10, stacks=8)
        for sx in (-1, 1):
            w.add_ellipsoid(wshift + sx * 0.14, torso_y - 0.02, 0.10 + torso_lean,
                            0.10, 0.26, 0.06, "Jacket", segs=10, stacks=8)
            w.add_ellipsoid(wshift + sx * 0.07, torso_y + 0.12, 0.13 + torso_lean,
                            0.055, 0.11, 0.035, "Jacket", segs=8, stacks=5)
        w.add_ellipsoid(wshift, torso_y - 0.22, 0.11 + torso_lean, torso_w * 0.50, 0.03, 0.05, "Jacket", segs=8, stacks=4)
        w.add_ellipsoid(wshift, 1.48 * h_scale + idle, 0.05 + torso_lean, 0.10, 0.035, 0.08, "Jacket", segs=8, stacks=5)
    elif style == "coat":
        w.add_ellipsoid(wshift, 0.88 * h_scale, 0.05 + torso_lean, torso_w * 0.58, 0.32, 0.16, "Jacket", segs=12, stacks=10)
        w.add_ellipsoid(wshift, torso_y + 0.02, 0.06 + torso_lean, shoulder * 0.55, 0.28, 0.17, "Jacket", segs=12, stacks=10)
        for sx in (-1, 1):
            w.add_ellipsoid(wshift + sx * 0.07, torso_y + 0.10, 0.15 + torso_lean,
                            0.06, 0.12, 0.04, "Jacket", segs=8, stacks=5)
        w.add_ellipsoid(wshift, 1.05 * h_scale, 0.15 + torso_lean, torso_w * 0.55, 0.03, 0.05, "Belt", segs=8, stacks=4)
        w.add_ellipsoid(wshift, 1.48 * h_scale + idle, 0.06 + torso_lean, 0.10, 0.04, 0.09, "Jacket", segs=8, stacks=5)
    elif style == "hoodie":
        w.add_ellipsoid(wshift, torso_y, 0.03 + torso_lean, torso_w * 0.55, 0.27, 0.16, "Jacket", segs=12, stacks=10)
        w.add_ellipsoid(wshift, 1.55 * h_scale + idle, -0.05 + torso_lean,
                        0.15, 0.12, 0.14, "Jacket", segs=12, stacks=8)
        w.add_ellipsoid(wshift, 1.12 * h_scale, 0.15 + torso_lean, 0.14, 0.08, 0.05, "Jacket", segs=8, stacks=5)
        w.add_ellipsoid(wshift, 1.02 * h_scale, 0.04 + torso_lean, torso_w * 0.56, 0.035, 0.15, "Jacket", segs=8, stacks=4)
    elif style == "blouse":
        w.add_ellipsoid(wshift, torso_y, 0.02 + torso_lean, torso_w * 0.52, 0.25, 0.13, "Shirt", segs=12, stacks=10)
        w.add_ellipsoid(wshift, 1.02 * h_scale, 0.04 + torso_lean, hip_w * 0.62, 0.09, 0.16, "Jacket", segs=10, stacks=6)
        w.add_ellipsoid(wshift, 1.47 * h_scale + idle, 0.04 + torso_lean, 0.085, 0.03, 0.07, "Shirt", segs=8, stacks=4)
        for sx in (-1, 1):
            w.add_ellipsoid(wshift + sx * 0.20, 1.40 * h_scale + idle, 0.02 + torso_lean,
                            0.085, 0.07, 0.08, "Shirt", segs=9, stacks=6)
    else:
        w.add_ellipsoid(wshift, torso_y, 0.01 + torso_lean, torso_w * 0.52, 0.25, 0.14, "Shirt", segs=12, stacks=10)
        w.add_ellipsoid(wshift, 1.48 * h_scale + idle, 0.02 + torso_lean,
                        0.095, 0.04, 0.08, "Shirt", segs=9, stacks=5)

    if style in ("jacket", "coat", "blouse"):
        for i in range(4):
            by = torso_y + 0.14 - i * 0.10
            w.add_ellipsoid(wshift, by, 0.14 + torso_lean, 0.012, 0.012, 0.01, "Buckle", segs=5, stacks=3)

    # neck
    w.add_capsule(wshift, 1.48 * h_scale + idle, 0.01 + torso_lean,
                  wshift, 1.56 * h_scale + idle, 0.01 + torso_lean, 0.048, "Skin", segs=8)
    hx = wshift + math.sin(head_yaw) * 0.02
    hy = 1.64 * h_scale + idle
    hz = 0.02 + torso_lean + math.cos(head_yaw) * 0.01
    w.add_ellipsoid(hx, hy, hz, 0.105, 0.125, 0.115, "Skin", segs=18, stacks=16, yaw=head_yaw)
    add_face(w, hx, hy, hz, style)
    add_hair(w, hx, hy, hz, style)

    # ---- Arms as capsules ----
    arm_mat = "Jacket" if style in ("jacket", "coat", "hoodie") else "Shirt"
    forearm_mat = "Skin" if style in ("tee",) else arm_mat
    if style == "blouse":
        forearm_mat = "Shirt"

    # shoulders
    for sx, raise_a, sw in ((-1, raise_L, arm_sw), (1, raise_R, -arm_sw)):
        sx_off = sx * (0.22 + shoulder * 0.15)
        sh_y = 1.42 * h_scale + idle + raise_a * 0.08
        sh_z = 0.02 + torso_lean
        w.add_ellipsoid(wshift + sx_off, sh_y, sh_z, 0.07, 0.07, 0.07, arm_mat, segs=9, stacks=7)
        # upper arm
        elbow_y = 1.12 * h_scale + idle + raise_a * 0.18
        elbow_z = 0.04 + sw * 0.10 + torso_lean
        w.add_capsule(wshift + sx_off, sh_y - 0.04, sh_z,
                      wshift + sx_off * 1.05, elbow_y, elbow_z, 0.048, arm_mat, segs=8)
        # forearm
        wrist_y = 0.88 * h_scale + raise_a * 0.30
        wrist_z = 0.10 + sw * 0.18 + torso_lean
        w.add_capsule(wshift + sx_off * 1.05, elbow_y, elbow_z,
                      wshift + sx_off * 1.08, wrist_y, wrist_z, 0.040, forearm_mat, segs=8)
        hold = (pose == "converse_a" and sx > 0)
        add_hand(w, wshift + sx_off * 1.08, wrist_y - 0.02, wrist_z + 0.02, sx,
                 curl=0.35 if hold else (0.2 if pose.startswith("converse") else 0.12),
                 hold_phone=hold)

    # ---- Legs as capsules ----
    for sx, lsw in ((-1, leg_sw), (1, -leg_sw)):
        hx_leg = wshift + sx * 0.10
        hip_y = 0.92 * h_scale
        knee_y = 0.52 * h_scale
        ankle_y = 0.12 * h_scale
        # thigh
        w.add_capsule(hx_leg, hip_y, torso_lean * 0.3,
                      hx_leg, knee_y, lsw * 0.14, 0.072, "Pants", segs=9)
        w.add_ellipsoid(hx_leg, knee_y, lsw * 0.14, 0.065, 0.05, 0.07, "Pants", segs=8, stacks=5)
        # shin
        w.add_capsule(hx_leg, knee_y, lsw * 0.14,
                      hx_leg, ankle_y, lsw * 0.20, 0.055, "Pants", segs=8)
        # cuff
        w.add_ellipsoid(hx_leg, ankle_y + 0.02, lsw * 0.20, 0.06, 0.03, 0.065, "Pants", segs=8, stacks=4)
        shoe_scale = 0.92 if style == "blouse" else (1.08 if style == "tee" else 1.0)
        add_shoe(w, hx_leg, 0.05 * h_scale, 0.04 + lsw * 0.22, style, shoe_scale)

    write_mtl(OUT / (name + ".mtl"), skin, shirt, pants, hair, shoes, style)
    w.write(OUT / (name + ".obj"))
    print("wrote", name, "style", style, "pose", pose, "verts", len(w.v), "h", round(height, 2))


for i, spec in enumerate(PEDS):
    build_ped(*spec, phase=i * 0.9 + 0.35)

print("done ->", OUT)
