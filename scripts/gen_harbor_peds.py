#!/usr/bin/env python3
"""Author 5 Harbor Metro pedestrian OBJ+MTL meshes (original, non-box silhouettes).
Branding: Harbor Metro civilians only — no third-party IP.
"""
from pathlib import Path
import math

OUT = Path("assets/meshes/harbor_metro/peds")
OUT.mkdir(parents=True, exist_ok=True)

# name, skin, shirt, pants, hair, shoes
PEDS = [
    ("hm_ped_rae",   (0.86, 0.68, 0.55), (0.22, 0.38, 0.62), (0.18, 0.20, 0.28), (0.12, 0.09, 0.07), (0.08, 0.08, 0.09)),
    ("hm_ped_dane",  (0.55, 0.38, 0.28), (0.70, 0.28, 0.22), (0.15, 0.16, 0.20), (0.05, 0.04, 0.03), (0.12, 0.10, 0.08)),
    ("hm_ped_suki",  (0.92, 0.78, 0.68), (0.20, 0.55, 0.48), (0.25, 0.22, 0.35), (0.08, 0.06, 0.05), (0.70, 0.70, 0.72)),
    ("hm_ped_noah",  (0.78, 0.58, 0.45), (0.35, 0.35, 0.42), (0.22, 0.25, 0.45), (0.15, 0.12, 0.10), (0.10, 0.10, 0.11)),
    ("hm_ped_ivy",   (0.88, 0.72, 0.60), (0.75, 0.55, 0.30), (0.40, 0.28, 0.22), (0.45, 0.22, 0.12), (0.15, 0.08, 0.08)),
]

def write_mtl(path, skin, shirt, pants, hair, shoes):
    def block(name, kd, ns, metallic_hint=0.0, emit=(0,0,0)):
        return (
            f"newmtl {name}\n"
            f"Ns {ns}\nKa 1 1 1\n"
            f"Kd {kd[0]:.4f} {kd[1]:.4f} {kd[2]:.4f}\n"
            f"Ks {0.15+metallic_hint*0.5:.3f} {0.15+metallic_hint*0.5:.3f} {0.15+metallic_hint*0.5:.3f}\n"
            f"Ke {emit[0]:.3f} {emit[1]:.3f} {emit[2]:.3f}\n"
            f"Ni 1.45\nd 1.0\nillum 2\n\n"
        )
    text = "# Harbor Metro original ped materials\n"
    text += block("Skin", skin, 12.0)
    text += block("Shirt", shirt, 8.0)
    text += block("Pants", pants, 10.0)
    text += block("Hair", hair, 6.0)
    text += block("Shoes", shoes, 25.0, metallic_hint=0.05)
    path.write_text(text)

class MeshWriter:
    def __init__(self, mtl_name):
        self.mtl = mtl_name
        self.v = []
        self.vn = []
        self.vt = []
        self.groups = []  # (mat, faces) faces as list of (i0,i1,i2) 1-based
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
        # 8 corners
        corners = [
            (cx-hx, cy-hy, cz-hz), (cx+hx, cy-hy, cz-hz),
            (cx+hx, cy+hy, cz-hz), (cx-hx, cy+hy, cz-hz),
            (cx-hx, cy-hy, cz+hz), (cx+hx, cy-hy, cz+hz),
            (cx+hx, cy+hy, cz+hz), (cx-hx, cy+hy, cz+hz),
        ]
        base = len(self.v)
        for c in corners:
            self.v.append(c)
        # face normals + uvs shared simply
        faces_idx = [
            (0,1,2,3, (0,0,-1)),  # -Z
            (5,4,7,6, (0,0,1)),   # +Z
            (4,0,3,7, (-1,0,0)),  # -X
            (1,5,6,2, (1,0,0)),   # +X
            (3,2,6,7, (0,1,0)),   # +Y
            (4,5,1,0, (0,-1,0)),  # -Y
        ]
        for a,b,c,d,n in faces_idx:
            ni = len(self.vn)
            self.vn.append(n)
            u0 = len(self.vt); self.vt += [(0,0),(1,0),(1,1),(0,1)]
            ia, ib, ic, id_ = base+a+1, base+b+1, base+c+1, base+d+1
            # OBJ vn/vt are separate indices; use same ni+1 for all
            self.cur_faces.append((ia, u0+1, ni+1, ib, u0+2, ni+1, ic, u0+3, ni+1))
            self.cur_faces.append((ia, u0+1, ni+1, ic, u0+3, ni+1, id_, u0+4, ni+1))

    def add_ellipsoid(self, cx, cy, cz, rx, ry, rz, mat, segs=8, stacks=6):
        self.usemtl(mat)
        base_v = len(self.v)
        base_n = len(self.vn)
        base_t = len(self.vt)
        # generate rings
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
                # 1-based
                def trip(a,b,c):
                    # v/vt/vn all share local offset pattern
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
                # f is 9-tuple v/vt/vn * 3
                lines.append(
                    f"f {f[0]}/{f[1]}/{f[2]} {f[3]}/{f[4]}/{f[5]} {f[6]}/{f[7]}/{f[8]}"
                )
        path.write_text("\n".join(lines) + "\n")


def build_ped(name, skin, shirt, pants, hair, shoes, phase=0.0):
    """Proportioned adult ~1.72m, clothing/hair/shoes/skin splits, slight walk pose."""
    h = 1.72
    swing = math.sin(phase) * 0.35
    w = MeshWriter(name + ".mtl")
    # pelvis / hips
    w.add_box(0, 0.95, 0, 0.34, 0.18, 0.22, "Pants")
    # torso / shirt
    w.add_box(0, 1.22, 0.01, 0.38, 0.42, 0.24, "Shirt")
    # neck
    w.add_box(0, 1.48, 0, 0.10, 0.10, 0.10, "Skin")
    # head (ellipsoid)
    w.add_ellipsoid(0, 1.60, 0.02, 0.11, 0.13, 0.12, "Skin", segs=10, stacks=8)
    # hair volume
    w.add_ellipsoid(0, 1.66, -0.01, 0.12, 0.08, 0.13, "Hair", segs=8, stacks=5)
    # upper arms
    w.add_box(-0.28, 1.28, 0.02 + swing*0.05, 0.10, 0.32, 0.10, "Shirt")
    w.add_box(0.28, 1.28, 0.02 - swing*0.05, 0.10, 0.32, 0.10, "Shirt")
    # forearms
    w.add_box(-0.30, 1.02, 0.06 + swing*0.12, 0.08, 0.28, 0.08, "Skin")
    w.add_box(0.30, 1.02, 0.06 - swing*0.12, 0.08, 0.28, 0.08, "Skin")
    # hands
    w.add_box(-0.30, 0.86, 0.10 + swing*0.15, 0.08, 0.08, 0.10, "Skin")
    w.add_box(0.30, 0.86, 0.10 - swing*0.15, 0.08, 0.08, 0.10, "Skin")
    # thighs
    w.add_box(-0.10, 0.70, swing*0.08, 0.14, 0.36, 0.14, "Pants")
    w.add_box(0.10, 0.70, -swing*0.08, 0.14, 0.36, 0.14, "Pants")
    # shins
    w.add_box(-0.10, 0.34, swing*0.14, 0.11, 0.34, 0.11, "Pants")
    w.add_box(0.10, 0.34, -swing*0.14, 0.11, 0.34, 0.11, "Pants")
    # shoes
    w.add_box(-0.10, 0.08, 0.04 + swing*0.16, 0.12, 0.10, 0.24, "Shoes")
    w.add_box(0.10, 0.08, 0.04 - swing*0.16, 0.12, 0.10, 0.24, "Shoes")
    write_mtl(OUT / (name + ".mtl"), skin, shirt, pants, hair, shoes)
    w.write(OUT / (name + ".obj"))
    print("wrote", name, "verts", len(w.v))

for i, spec in enumerate(PEDS):
    build_ped(*spec, phase=i * 0.7)

print("done ->", OUT)
