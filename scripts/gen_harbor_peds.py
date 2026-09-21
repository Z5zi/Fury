#!/usr/bin/env python3
"""Author 5 Harbor Metro pedestrian OBJ+MTL meshes — Cycle-3 AAA characters.
Original assets only. Branding: Harbor Metro civilians — no third-party IP.

Distinct silhouettes, anatomy (face/hands/shoes/hair), clothing construction,
idle/walk pose phases. Soft renderer judges these stills.
"""
from pathlib import Path
import math

OUT = Path("assets/meshes/harbor_metro/peds")
OUT.mkdir(parents=True, exist_ok=True)

# name, skin, shirt, pants, hair, shoes, style
# style: jacket|tee|coat|hoodie|blouse — drives silhouette extras
PEDS = [
    ("hm_ped_rae",   (0.86, 0.68, 0.55), (0.18, 0.32, 0.58), (0.14, 0.16, 0.22), (0.10, 0.07, 0.05), (0.06, 0.06, 0.07), "jacket"),
    ("hm_ped_dane",  (0.52, 0.36, 0.26), (0.68, 0.22, 0.18), (0.12, 0.13, 0.16), (0.04, 0.03, 0.02), (0.10, 0.08, 0.06), "tee"),
    ("hm_ped_suki",  (0.92, 0.78, 0.68), (0.16, 0.52, 0.48), (0.22, 0.18, 0.32), (0.06, 0.04, 0.04), (0.72, 0.72, 0.75), "blouse"),
    ("hm_ped_noah",  (0.76, 0.56, 0.44), (0.28, 0.30, 0.36), (0.18, 0.22, 0.40), (0.12, 0.10, 0.08), (0.08, 0.08, 0.09), "hoodie"),
    ("hm_ped_ivy",   (0.88, 0.72, 0.60), (0.72, 0.48, 0.22), (0.38, 0.24, 0.18), (0.48, 0.24, 0.12), (0.14, 0.07, 0.06), "coat"),
]

def write_mtl(path, skin, shirt, pants, hair, shoes):
    def block(name, kd, ns, metallic_hint=0.0, emit=(0,0,0)):
        return (
            f"newmtl {name}\n"
            f"Ns {ns}\nKa 1 1 1\n"
            f"Kd {kd[0]:.4f} {kd[1]:.4f} {kd[2]:.4f}\n"
            f"Ks {0.12+metallic_hint*0.55:.3f} {0.12+metallic_hint*0.55:.3f} {0.12+metallic_hint*0.55:.3f}\n"
            f"Ke {emit[0]:.3f} {emit[1]:.3f} {emit[2]:.3f}\n"
            f"Ni 1.45\nd 1.0\nillum 2\n\n"
        )
    text = "# Harbor Metro original ped materials (Cycle-3)\n"
    text += block("Skin", skin, 18.0)
    text += block("Shirt", shirt, 9.0)
    text += block("Pants", pants, 11.0)
    text += block("Hair", hair, 22.0, metallic_hint=0.04)
    text += block("Shoes", shoes, 40.0, metallic_hint=0.12)
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

    def add_box(self, cx, cy, cz, sx, sy, sz, mat):
        self.usemtl(mat)
        hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
        corners = [
            (cx-hx, cy-hy, cz-hz), (cx+hx, cy-hy, cz-hz),
            (cx+hx, cy+hy, cz-hz), (cx-hx, cy+hy, cz-hz),
            (cx-hx, cy-hy, cz+hz), (cx+hx, cy-hy, cz+hz),
            (cx+hx, cy+hy, cz+hz), (cx-hx, cy+hy, cz+hz),
        ]
        base = len(self.v)
        for c in corners:
            self.v.append(c)
        faces_idx = [
            (0,1,2,3, (0,0,-1)),
            (5,4,7,6, (0,0,1)),
            (4,0,3,7, (-1,0,0)),
            (1,5,6,2, (1,0,0)),
            (3,2,6,7, (0,1,0)),
            (4,5,1,0, (0,-1,0)),
        ]
        for a,b,c,d,n in faces_idx:
            ni = len(self.vn)
            self.vn.append(n)
            u0 = len(self.vt); self.vt += [(0,0),(1,0),(1,1),(0,1)]
            ia, ib, ic, id_ = base+a+1, base+b+1, base+c+1, base+d+1
            self.cur_faces.append((ia, u0+1, ni+1, ib, u0+2, ni+1, ic, u0+3, ni+1))
            self.cur_faces.append((ia, u0+1, ni+1, ic, u0+3, ni+1, id_, u0+4, ni+1))

    def add_ellipsoid(self, cx, cy, cz, rx, ry, rz, mat, segs=10, stacks=8):
        self.usemtl(mat)
        base_v = len(self.v)
        base_n = len(self.vn)
        base_t = len(self.vt)
        for i in range(stacks+1):
            v = i / stacks
            phi = v * math.pi
            for j in range(segs):
                u = j / segs
                th = u * 2 * math.pi
                x = cx + rx * math.sin(phi) * math.cos(th)
                y = cy + ry * math.cos(phi)
                z = cz + rz * math.sin(phi) * math.sin(th)
                nx = math.sin(phi) * math.cos(th)
                ny = math.cos(phi)
                nz = math.sin(phi) * math.sin(th)
                self.v.append((x,y,z))
                self.vn.append((nx,ny,nz))
                self.vt.append((u,v))
        for i in range(stacks):
            for j in range(segs):
                j2 = (j+1) % segs
                i0 = base_v + i*segs + j
                i1 = base_v + i*segs + j2
                i2 = base_v + (i+1)*segs + j2
                i3 = base_v + (i+1)*segs + j
                def trip(a,b,c):
                    return (a+1, base_t+(a-base_v)+1, base_n+(a-base_v)+1,
                            b+1, base_t+(b-base_v)+1, base_n+(b-base_v)+1,
                            c+1, base_t+(c-base_v)+1, base_n+(c-base_v)+1)
                self.cur_faces.append(trip(i0,i1,i2))
                self.cur_faces.append(trip(i0,i2,i3))

    def finish(self):
        if self.cur is not None:
            self.groups.append((self.cur, self.cur_faces))

    def write(self, path):
        self.finish()
        lines = [f"mtllib {self.mtl}", "o Ped"]
        for x,y,z in self.v:
            lines.append(f"v {x:.5f} {y:.5f} {z:.5f}")
        for x,y,z in self.vn:
            lines.append(f"vn {x:.5f} {y:.5f} {z:.5f}")
        for u,v in self.vt:
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


def add_hand(w, cx, cy, cz, side, swing):
    """Palm + 4 finger stubs + thumb — readable at capture distance."""
    s = 1.0 if side < 0 else -1.0
    w.add_box(cx, cy, cz, 0.075, 0.055, 0.11, "Skin")  # palm
    # fingers
    for i, fx in enumerate([-0.028, -0.010, 0.010, 0.028]):
        w.add_box(cx + fx * s, cy - 0.01, cz + 0.07, 0.018, 0.022, 0.055, "Skin")
    # thumb
    w.add_box(cx + 0.045 * s, cy + 0.01, cz + 0.02, 0.022, 0.028, 0.045, "Skin")


def add_shoe(w, cx, cy, cz, style_scale=1.0):
    # sole + upper + heel
    w.add_box(cx, cy, cz + 0.02, 0.13 * style_scale, 0.055, 0.28 * style_scale, "Shoes")
    w.add_box(cx, cy + 0.04, cz - 0.02, 0.12 * style_scale, 0.06, 0.18 * style_scale, "Shoes")
    w.add_box(cx, cy + 0.01, cz - 0.10, 0.11 * style_scale, 0.05, 0.08 * style_scale, "Shoes")


def add_face(w, hx, hy, hz):
    # nose
    w.add_box(hx, hy + 0.02, hz + 0.11, 0.028, 0.04, 0.04, "Skin")
    # brow ridge
    w.add_box(hx, hy + 0.06, hz + 0.09, 0.12, 0.025, 0.03, "Skin")
    # ears
    w.add_ellipsoid(hx - 0.11, hy + 0.01, hz, 0.025, 0.04, 0.02, "Skin", segs=6, stacks=4)
    w.add_ellipsoid(hx + 0.11, hy + 0.01, hz, 0.025, 0.04, 0.02, "Skin", segs=6, stacks=4)
    # chin
    w.add_box(hx, hy - 0.08, hz + 0.06, 0.07, 0.04, 0.05, "Skin")


def build_ped(name, skin, shirt, pants, hair, shoes, style, phase=0.0, height=1.72):
    """Proportioned adult with clothing construction + face/hands/shoes/hair."""
    swing = math.sin(phase) * 0.42
    idle = math.cos(phase * 0.5) * 0.03
    w = MeshWriter(name + ".mtl")
    h_scale = height / 1.72

    # Body proportions by style
    torso_w = 0.40
    shoulder = 0.42
    hip_w = 0.34
    if style == "tee":
        torso_w, shoulder = 0.44, 0.48  # stockier Dane
    elif style == "blouse":
        torso_w, shoulder, hip_w = 0.34, 0.36, 0.36  # Suki
        height = 1.64
        h_scale = height / 1.72
    elif style == "hoodie":
        torso_w, shoulder = 0.42, 0.46
    elif style == "coat":
        torso_w, shoulder = 0.38, 0.44
        height = 1.68
        h_scale = height / 1.72

    # pelvis / hips
    w.add_box(0, 0.95 * h_scale, 0, hip_w, 0.18, 0.22, "Pants")
    # belt
    w.add_box(0, 1.05 * h_scale, 0.01, hip_w * 1.02, 0.05, 0.24, "Pants")

    # torso / shirt
    w.add_box(0, 1.24 * h_scale + idle, 0.01, torso_w, 0.44, 0.24, "Shirt")
    # collar
    w.add_box(0, 1.46 * h_scale + idle, 0.04, 0.16, 0.06, 0.12, "Shirt")

    if style == "jacket":
        # open jacket panels
        w.add_box(-0.14, 1.22 * h_scale, 0.10, 0.16, 0.46, 0.08, "Shirt")
        w.add_box(0.14, 1.22 * h_scale, 0.10, 0.16, 0.46, 0.08, "Shirt")
        # inner tee slightly different via pants? keep shirt
    elif style == "coat":
        # long coat hem
        w.add_box(0, 0.85 * h_scale, 0.06, torso_w * 1.05, 0.55, 0.28, "Shirt")
        w.add_box(0, 1.30 * h_scale, 0.08, shoulder, 0.50, 0.30, "Shirt")
    elif style == "hoodie":
        # hood volume
        w.add_ellipsoid(0, 1.55 * h_scale, -0.04, 0.14, 0.10, 0.14, "Shirt", segs=8, stacks=5)
        # kangaroo pocket
        w.add_box(0, 1.12 * h_scale, 0.14, 0.22, 0.12, 0.06, "Shirt")
    elif style == "blouse":
        # slight peplum
        w.add_box(0, 1.02 * h_scale, 0.04, hip_w * 1.1, 0.12, 0.26, "Shirt")

    # neck
    w.add_box(0, 1.50 * h_scale + idle, 0.01, 0.10, 0.12, 0.10, "Skin")
    # head
    hx, hy, hz = 0.0, 1.62 * h_scale + idle, 0.02
    w.add_ellipsoid(hx, hy, hz, 0.105, 0.125, 0.115, "Skin", segs=14, stacks=12)
    add_face(w, hx, hy, hz)

    # hair styles
    if style == "blouse":
        # bob / side volume
        w.add_ellipsoid(hx, hy + 0.06, hz - 0.02, 0.13, 0.09, 0.14, "Hair", segs=12, stacks=8)
        w.add_ellipsoid(hx - 0.08, hy - 0.02, hz - 0.04, 0.05, 0.10, 0.06, "Hair", segs=6, stacks=4)
        w.add_ellipsoid(hx + 0.08, hy - 0.02, hz - 0.04, 0.05, 0.10, 0.06, "Hair", segs=6, stacks=4)
    elif style == "coat":
        # longer wavy
        w.add_ellipsoid(hx, hy + 0.05, hz - 0.03, 0.13, 0.10, 0.14, "Hair", segs=12, stacks=8)
        w.add_box(hx - 0.08, hy - 0.08, hz - 0.02, 0.06, 0.22, 0.08, "Hair")
        w.add_box(hx + 0.08, hy - 0.08, hz - 0.02, 0.06, 0.22, 0.08, "Hair")
        w.add_box(hx, hy - 0.12, hz - 0.06, 0.14, 0.18, 0.06, "Hair")
    elif style == "tee":
        # short crop
        w.add_ellipsoid(hx, hy + 0.08, hz - 0.02, 0.11, 0.06, 0.12, "Hair", segs=8, stacks=5)
    elif style == "hoodie":
        # under-hood short
        w.add_ellipsoid(hx, hy + 0.07, hz - 0.01, 0.115, 0.07, 0.12, "Hair", segs=8, stacks=5)
    else:
        # side-part medium
        w.add_ellipsoid(hx, hy + 0.07, hz - 0.02, 0.125, 0.08, 0.135, "Hair", segs=12, stacks=8)
        w.add_box(hx + 0.06, hy - 0.02, hz + 0.02, 0.05, 0.12, 0.08, "Hair")

    # upper arms
    arm_mat = "Shirt"
    w.add_box(-0.28, 1.28 * h_scale + idle, 0.02 + swing * 0.05, 0.10, 0.34, 0.10, arm_mat)
    w.add_box(0.28, 1.28 * h_scale + idle, 0.02 - swing * 0.05, 0.10, 0.34, 0.10, arm_mat)
    # forearms
    w.add_box(-0.30, 1.00 * h_scale, 0.06 + swing * 0.14, 0.085, 0.30, 0.085, "Skin")
    w.add_box(0.30, 1.00 * h_scale, 0.06 - swing * 0.14, 0.085, 0.30, 0.085, "Skin")
    # hands
    add_hand(w, -0.30, 0.84 * h_scale, 0.12 + swing * 0.18, -1, swing)
    add_hand(w, 0.30, 0.84 * h_scale, 0.12 - swing * 0.18, 1, swing)

    # thighs
    w.add_box(-0.10, 0.70 * h_scale, swing * 0.10, 0.145, 0.38, 0.145, "Pants")
    w.add_box(0.10, 0.70 * h_scale, -swing * 0.10, 0.145, 0.38, 0.145, "Pants")
    # knees
    w.add_box(-0.10, 0.50 * h_scale, swing * 0.12, 0.12, 0.08, 0.13, "Pants")
    w.add_box(0.10, 0.50 * h_scale, -swing * 0.12, 0.12, 0.08, 0.13, "Pants")
    # shins
    w.add_box(-0.10, 0.32 * h_scale, swing * 0.16, 0.115, 0.34, 0.115, "Pants")
    w.add_box(0.10, 0.32 * h_scale, -swing * 0.16, 0.115, 0.34, 0.115, "Pants")

    # shoes
    add_shoe(w, -0.10, 0.07 * h_scale, 0.04 + swing * 0.18)
    add_shoe(w, 0.10, 0.07 * h_scale, 0.04 - swing * 0.18)

    write_mtl(OUT / (name + ".mtl"), skin, shirt, pants, hair, shoes)
    w.write(OUT / (name + ".obj"))
    print("wrote", name, "style", style, "verts", len(w.v), "h", round(height, 2))

for i, spec in enumerate(PEDS):
    build_ped(*spec, phase=i * 0.85 + 0.2)

print("done ->", OUT)
