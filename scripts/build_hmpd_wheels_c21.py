#!/usr/bin/env python3
"""Cycle-21 P0 (Gate B): fully modelled police-spec wheel assemblies for the HMPD cruiser.

blender -b --factory-startup -P scripts/build_hmpd_wheels_c21.py
  in : assets/meshes/harbor_metro/hmpd_cruiser_c20/hmpd_cruiser_c20.blend (CC BY 4.0 base, see LICENSE.md)
  out: assets/meshes/harbor_metro/hmpd_cruiser_c21/hmpd_cruiser_c21.blend (collection HMPD_Cruiser_C21)

The C20 base-mesh wheels (stamped steel texture on a low-detail rim + 26-tri brake card) are deleted
and replaced at all four corners by an original, fully modelled assembly (no third-party geometry):
  * 17x7.5 five split-spoke alloy: concave dish (45 mm), round-bevelled window edges, machined
    bevelled outer lip, drop-centre barrel, 5 lug pockets on a 114.3 mm PCD, acorn lug nuts with
    washers, chrome centre cap with black insert, rubber valve stem
  * P235/50R17 pursuit tyre: lathed carcass with real sidewall bulge + rim protector rib, 5-rib tread
    of individually modelled bevelled blocks (slanted lateral grooves, centre sipes, shoulder blocks
    wrapping over the shoulder), raised moulded sidewall lettering, loaded contact patch (flattened,
    bulged sidewall), wear / road-grime / wet shading
  * 320 mm ventilated rotor: two friction plates + 40 curved vanes, cross-drilled outboard face
    (24 through holes), 6 shaded curved slots, concentric machining anisotropy, rusty edge, hat
  * fixed two-piston caliper (outboard body + bridge + inboard body, piston bulges, bridge bolts),
    trailing position behind the spokes; dust shield + knuckle stub behind the rotor
Frames: wheel-local axle = +X (outboard), origin = hub centre; car front = -Y, width = X.
"""
import bpy, bmesh, math
from pathlib import Path
from mathutils import Vector, Matrix

SRC = Path("/workspace/Fury/assets/meshes/harbor_metro/hmpd_cruiser_c20/hmpd_cruiser_c20.blend")
D = Path("/workspace/Fury/assets/meshes/harbor_metro/hmpd_cruiser_c21")
OUT = D / "hmpd_cruiser_c21.blend"
D.mkdir(parents=True, exist_ok=True)

bpy.ops.wm.open_mainfile(filepath=str(SRC))
coll = bpy.data.collections["HMPD_Cruiser_C20"]
coll.name = "HMPD_Cruiser_C21"
for o in list(bpy.data.objects):
    if o.name.startswith(("C20CV_Wheel_", "C20CV_WheelBrake_")):
        bpy.data.objects.remove(o, do_unlink=True)
for m in list(bpy.data.materials):
    if m.name in ("C20CV_Wheel", "C20CV_Brake") and m.users == 0:
        bpy.data.materials.remove(m)

# ---------------------------------------------------------------- dimensions (metres)
HUB_Z = 0.322           # loaded radius -> contact patch at car z = 0
R_OUT = 0.330           # unloaded tread radius
TRACK_X = 0.770
AXLE_Y = 1.455
R_BEAD = 0.222
PCD_R = 0.0572
CAL_PHI = math.radians(18.0)   # caliper centre: behind the axle (+Y car), 18 deg above horizontal
TAU = 2 * math.pi


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def to_mesh(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    return me


def lathe(bm, profile, n, mat=0, cyclic_profile=True, phi0=0.0, mat_fn=None):
    """profile: list of (a, r). Returns ring vertex grid."""
    rings = []
    for j in range(n):
        ph = phi0 + TAU * j / n
        c, s = math.cos(ph), math.sin(ph)
        rings.append([bm.verts.new((a, r * c, r * s)) for a, r in profile])
    P = len(profile)
    last = P if cyclic_profile else P - 1
    for j in range(n):
        A, B = rings[j], rings[(j + 1) % n]
        for i in range(last):
            i2 = (i + 1) % P
            f = bm.faces.new((A[i], A[i2], B[i2], B[i]))
            f.material_index = mat_fn(i) if mat_fn else mat
    return rings


def finish(bm):
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)


def curve2d_mesh(name, loops, extrude, bevel, bres=3):
    cu = bpy.data.curves.new(name + "_cu", "CURVE")
    cu.dimensions = "2D"
    cu.fill_mode = "BOTH"
    cu.extrude = extrude
    cu.bevel_depth = bevel
    cu.bevel_resolution = bres
    for pts in loops:
        sp = cu.splines.new("POLY")
        sp.points.add(len(pts) - 1)
        for i, (x, y) in enumerate(pts):
            sp.points[i].co = (x, y, 0, 1)
        sp.use_cyclic_u = True
    ob = bpy.data.objects.new(name + "_tmp", cu)
    bpy.context.scene.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.curves.remove(cu)
    me.name = name
    return me


def circle(cx, cy, r, n=48):
    return [(cx + r * math.cos(TAU * k / n), cy + r * math.sin(TAU * k / n)) for k in range(n)]


def rounded_polar(theta_c, r_in, r_out, halfang, cr_m, n_side=26, n_corner=10):
    """Rounded rectangle in normalised (u,v), mapped to polar: r = r_in + v (r_out-r_in),
    phi = theta_c + u * halfang(r). cr_m = corner radius in metres."""
    hr = r_out - r_in
    rv = min(cr_m / hr, 0.49)
    def ru_at(v):
        rr = r_in + v * hr
        return min(cr_m / max(halfang(rr) * rr, 1e-4), 0.98)
    ru = ru_at(0.5)
    pts = []
    # walk CCW in (u, v): bottom edge (v=0) left->right, right edge up, top right->left, left down
    def corner(cu, cv, a0):
        for k in range(n_corner + 1):
            a = a0 + (math.pi / 2) * k / n_corner
            pts.append((cu + ru * math.cos(a), cv + rv * math.sin(a)))
    def edge(p0, p1):
        for k in range(1, n_side):
            t = k / n_side
            pts.append((p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t))
    corner(1 - ru, rv, -math.pi / 2)            # bottom-right
    edge((1, rv), (1, 1 - rv))
    corner(1 - ru, 1 - rv, 0.0)                 # top-right
    edge((1 - ru, 1), (-1 + ru, 1))
    corner(-1 + ru, 1 - rv, math.pi / 2)        # top-left
    edge((-1, 1 - rv), (-1, rv))
    corner(-1 + ru, rv, math.pi)                # bottom-left
    edge((-1 + ru, 0), (1 - ru, 0))
    out = []
    for u, v in pts:
        rr = r_in + v * hr
        ph = theta_c + u * halfang(rr)
        out.append((rr * math.cos(ph), rr * math.sin(ph)))
    return out


def map_plane(me, a_fn):
    """curve-local (x, y, z) -> wheel (a = a_fn(r) + z, Y = x, Z = y)."""
    for v in me.vertices:
        x, y, z = v.co
        r = math.hypot(x, y)
        v.co = (a_fn(r) + z, x, y)
    me.update()


# ---------------------------------------------------------------- materials
def new_mat(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    out.location = (900, 0)
    b = nt.nodes.new("ShaderNodeBsdfPrincipled")
    b.location = (600, 0)
    nt.links.new(b.outputs[0], out.inputs[0])
    return m, nt, b


class G:
    """tiny node-graph helper"""
    def __init__(self, nt):
        self.nt = nt
        self.L = nt.links
        tc = nt.nodes.new("ShaderNodeTexCoord")
        self.obj = tc.outputs["Object"]
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        self.L.new(self.obj, sep.inputs[0])
        self.X, self.Y, self.Z = sep.outputs[0], sep.outputs[1], sep.outputs[2]
        self._r = None

    def m(self, op, a, b=None, clamp=False):
        n = self.nt.nodes.new("ShaderNodeMath")
        n.operation = op
        n.use_clamp = clamp
        for i, v in enumerate((a, b)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                self.L.new(v, n.inputs[i])
        return n.outputs[0]

    def mix(self, fac, a, b):
        n = self.nt.nodes.new("ShaderNodeMix")
        n.data_type = "RGBA"
        for sock, v in ((n.inputs[0], fac), (n.inputs[6], a), (n.inputs[7], b)):
            if isinstance(v, (int, float, tuple)):
                sock.default_value = v
            else:
                self.L.new(v, sock)
        return n.outputs[2]

    def mixf(self, fac, a, b):
        return self.m("ADD", self.m("MULTIPLY", self.m("SUBTRACT", 1.0, fac), a), self.m("MULTIPLY", fac, b))

    def sstep(self, x, e0, e1):
        n = self.nt.nodes.new("ShaderNodeMapRange")
        n.interpolation_type = "SMOOTHSTEP"
        self.L.new(x, n.inputs[0])
        n.inputs[1].default_value = e0
        n.inputs[2].default_value = e1
        n.inputs[3].default_value = 0.0
        n.inputs[4].default_value = 1.0
        return n.outputs[0]

    def noise(self, scale, detail=4.0, rough=0.55, vec=None):
        n = self.nt.nodes.new("ShaderNodeTexNoise")
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        self.L.new(vec or self.obj, n.inputs["Vector"])
        return n.outputs["Fac"]

    @property
    def r(self):
        if self._r is None:
            c = self.nt.nodes.new("ShaderNodeCombineXYZ")
            self.L.new(self.Y, c.inputs[1])
            self.L.new(self.Z, c.inputs[2])
            vm = self.nt.nodes.new("ShaderNodeVectorMath")
            vm.operation = "LENGTH"
            self.L.new(c.outputs[0], vm.inputs[0])
            self._r = vm.outputs["Value"]
        return self._r

    def bump(self, h, strength, dist, normal=None):
        n = self.nt.nodes.new("ShaderNodeBump")
        n.inputs["Strength"].default_value = strength
        n.inputs["Distance"].default_value = dist
        self.L.new(h, n.inputs["Height"])
        if normal is not None:
            self.L.new(normal, n.inputs["Normal"])
        return n.outputs[0]

    def radial_tangent(self, bsdf, aniso):
        t = self.nt.nodes.new("ShaderNodeTangent")
        t.direction_type = "RADIAL"
        t.axis = "X"
        self.L.new(t.outputs[0], bsdf.inputs["Tangent"])
        bsdf.inputs["Anisotropic"].default_value = aniso


def low_wet(g):
    """0 at hub height .. 1 near the road (object z < -0.2)"""
    return g.sstep(g.m("MULTIPLY", g.Z, -1.0), 0.16, 0.30)


# tyre rubber (sidewall / tread split by radius) with grime + wet lower third
m_tyre, nt, b = new_mat("C21_TyreRubber")
g = G(nt)
tread = g.sstep(g.r, 0.300, 0.312)
n1 = g.noise(38.0, 6.0, 0.6)
n2 = g.noise(420.0, 2.0, 0.5)
scuff = g.m("MULTIPLY", g.sstep(n1, 0.45, 0.75), tread)
col = g.mix(tread, (0.021, 0.021, 0.022, 1), (0.034, 0.033, 0.032, 1))
col = g.mix(g.m("MULTIPLY", scuff, 0.7), col, (0.060, 0.057, 0.053, 1))
wet = low_wet(g)
grime = g.m("MULTIPLY", g.sstep(g.noise(14.0, 5.0, 0.6), 0.40, 0.70), g.sstep(g.m("MULTIPLY", g.Z, -1.0), 0.0, 0.25))
col = g.mix(g.m("MULTIPLY", grime, 0.55), col, (0.055, 0.047, 0.038, 1))
col = g.mix(g.m("MULTIPLY", wet, 0.45), col, (0.010, 0.010, 0.011, 1))
g.L.new(col, b.inputs["Base Color"])
rough = g.mixf(tread, 0.60, 0.82)
rough = g.m("ADD", rough, g.m("MULTIPLY", g.m("SUBTRACT", n1, 0.5), 0.12))
rough = g.mixf(g.m("MULTIPLY", wet, 0.8), rough, 0.22)
g.L.new(rough, b.inputs["Roughness"])
b.inputs["Specular IOR Level"].default_value = 0.42
b.inputs["Coat Weight"].default_value = 0.0
g.L.new(g.m("MULTIPLY", wet, 0.55), b.inputs["Coat Weight"])
b.inputs["Coat Roughness"].default_value = 0.10
g.L.new(g.bump(g.m("ADD", g.m("MULTIPLY", n2, 0.6), g.m("MULTIPLY", n1, 0.4)), 0.12, 0.0015), b.inputs["Normal"])

m_letter, nt, b = new_mat("C21_TyreLetter")
g = G(nt)
wet = low_wet(g)
col = g.mix(g.m("MULTIPLY", wet, 0.5), (0.050, 0.049, 0.047, 1), (0.016, 0.016, 0.017, 1))
g.L.new(col, b.inputs["Base Color"])
g.L.new(g.mixf(wet, 0.48, 0.25), b.inputs["Roughness"])
b.inputs["Specular IOR Level"].default_value = 0.45

# painted silver alloy, clear-coated, brake dust in the barrel / lower half
m_alloy, nt, b = new_mat("C21_AlloySilver")
g = G(nt)
dust_ax = g.sstep(g.m("MULTIPLY", g.X, -1.0), -0.070, 0.02)   # deeper = dustier
nd = g.noise(60.0, 6.0, 0.6)
dust = g.m("ADD", g.m("MULTIPLY", dust_ax, 0.75), g.m("MULTIPLY", g.sstep(nd, 0.5, 0.8), 0.25), clamp=True)
lowd = g.m("MULTIPLY", g.sstep(g.m("MULTIPLY", g.Z, -1.0), 0.05, 0.25), g.sstep(nd, 0.35, 0.7))
dust = g.m("MAXIMUM", dust, g.m("MULTIPLY", lowd, 0.35))
ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
ao.only_local = True
ao.samples = 8
ao.inputs["Distance"].default_value = 0.025
crev = g.m("MULTIPLY", g.m("SUBTRACT", 1.0, ao.outputs["AO"]), 0.8)
dust = g.m("MAXIMUM", dust, crev)
col = g.mix(dust, (0.56, 0.57, 0.59, 1), (0.15, 0.135, 0.12, 1))
g.L.new(col, b.inputs["Base Color"])
b.inputs["Metallic"].default_value = 0.82
g.L.new(g.mixf(dust, 0.36, 0.66), b.inputs["Roughness"])
g.L.new(g.mixf(dust, 0.7, 0.12), b.inputs["Coat Weight"])
b.inputs["Coat Roughness"].default_value = 0.06
g.L.new(g.bump(nd, 0.05, 0.001), b.inputs["Normal"])

m_lip, nt, b = new_mat("C21_AlloyMachinedLip")
g = G(nt)
b.inputs["Base Color"].default_value = (0.80, 0.80, 0.81, 1)
b.inputs["Metallic"].default_value = 1.0
b.inputs["Roughness"].default_value = 0.14
g.radial_tangent(b, 0.65)
b.inputs["Coat Weight"].default_value = 1.0
b.inputs["Coat Roughness"].default_value = 0.03

m_chrome, nt, b = new_mat("C21_LugChrome")
b.inputs["Base Color"].default_value = (0.86, 0.86, 0.87, 1)
b.inputs["Metallic"].default_value = 1.0
b.inputs["Roughness"].default_value = 0.09

m_capblack, nt, b = new_mat("C21_CapInsert")
b.inputs["Base Color"].default_value = (0.012, 0.014, 0.022, 1)
b.inputs["Roughness"].default_value = 0.3
b.inputs["Coat Weight"].default_value = 1.0
b.inputs["Coat Roughness"].default_value = 0.05

m_valve, nt, b = new_mat("C21_ValveRubber")
b.inputs["Base Color"].default_value = (0.02, 0.02, 0.02, 1)
b.inputs["Roughness"].default_value = 0.55

# rotor: machined cast-iron friction face with concentric anisotropy, 6 curved slots, wear band, rusty rim
m_rotor, nt, b = new_mat("C21_RotorFace")
g = G(nt)
ang = g.m("ARCTAN2", g.Z, g.Y)
s = g.m("SUBTRACT", g.m("MULTIPLY", ang, 6.0 / TAU), g.m("MULTIPLY", g.r, 9.0))
fr = g.m("FRACT", s)
slot = g.m("MULTIPLY", g.m("LESS_THAN", fr, 0.035), g.m("MULTIPLY", g.sstep(g.r, 0.104, 0.108), g.m("SUBTRACT", 1.0, g.sstep(g.r, 0.150, 0.154))))
band = g.m("MULTIPLY", g.sstep(g.r, 0.100, 0.106), g.m("SUBTRACT", 1.0, g.sstep(g.r, 0.156, 0.160)))
edge_rust = g.sstep(g.r, 0.156, 0.161)
nr = g.noise(90.0, 6.0, 0.65)
rings = nt.nodes.new("ShaderNodeTexWave")
rings.wave_type = "RINGS"
rings.inputs["Scale"].default_value = 2.2
rings.inputs["Distortion"].default_value = 0.6
rings.inputs["Detail"].default_value = 3.0
g.L.new(g.obj, rings.inputs["Vector"])
col = g.mix(band, (0.20, 0.19, 0.18, 1), (0.48, 0.475, 0.47, 1))
col = g.mix(g.m("MULTIPLY", g.sstep(nr, 0.5, 0.8), 0.35), col, (0.32, 0.31, 0.30, 1))
col = g.mix(g.m("MAXIMUM", edge_rust, g.m("MULTIPLY", g.m("SUBTRACT", 1.0, band), 0.7)), col, (0.22, 0.12, 0.07, 1))
col = g.mix(slot, col, (0.05, 0.048, 0.046, 1))
g.L.new(col, b.inputs["Base Color"])
b.inputs["Metallic"].default_value = 0.9
rough = g.mixf(band, 0.62, 0.30)
rough = g.mixf(slot, rough, 0.7)
g.L.new(rough, b.inputs["Roughness"])
g.radial_tangent(b, 0.55)
h = g.m("SUBTRACT", g.m("MULTIPLY", rings.outputs["Fac"], 0.25), g.m("MULTIPLY", slot, 1.0))
g.L.new(g.bump(h, 0.35, 0.0015), b.inputs["Normal"])

m_iron, nt, b = new_mat("C21_CastIronHat")
g = G(nt)
nr = g.noise(70.0, 6.0, 0.65)
col = g.mix(g.sstep(nr, 0.45, 0.75), (0.085, 0.080, 0.075, 1), (0.20, 0.11, 0.06, 1))
g.L.new(col, b.inputs["Base Color"])
b.inputs["Metallic"].default_value = 0.65
b.inputs["Roughness"].default_value = 0.62
g.L.new(g.bump(nr, 0.25, 0.001), b.inputs["Normal"])

m_cal, nt, b = new_mat("C21_CaliperRed")
g = G(nt)
nc = g.noise(45.0, 4.0, 0.6)
col = g.mix(g.m("MULTIPLY", g.sstep(nc, 0.55, 0.8), 0.4), (0.36, 0.022, 0.018, 1), (0.16, 0.06, 0.04, 1))
g.L.new(col, b.inputs["Base Color"])
b.inputs["Roughness"].default_value = 0.34
b.inputs["Coat Weight"].default_value = 0.8
b.inputs["Coat Roughness"].default_value = 0.12

m_zinc, nt, b = new_mat("C21_ZincBolt")
b.inputs["Base Color"].default_value = (0.55, 0.55, 0.52, 1)
b.inputs["Metallic"].default_value = 1.0
b.inputs["Roughness"].default_value = 0.35

m_shield, nt, b = new_mat("C21_DustShield")
b.inputs["Base Color"].default_value = (0.018, 0.018, 0.019, 1)
b.inputs["Roughness"].default_value = 0.55
b.inputs["Metallic"].default_value = 0.3

# ---------------------------------------------------------------- tyre carcass
def r_top(a):
    return R_OUT - 0.5 * a * a - 55.0 * max(0.0, abs(a) - 0.100) ** 2

BLOCK_H = 0.0095
prof = []
# outboard bead -> sidewall -> (tread base) -> inboard sidewall -> bead -> under-rim closing
side_pts = [(0.097, 0.2235), (0.1005, 0.2285), (0.1065, 0.2330), (0.1095, 0.2365), (0.1080, 0.2400),
            (0.1135, 0.2480), (0.1195, 0.2580), (0.1228, 0.2700), (0.1232, 0.2790), (0.1210, 0.2880),
            (0.1185, 0.2945)]
prof += side_pts
NA = 40
for k in range(NA + 1):
    a = 0.116 - 0.232 * k / NA
    prof.append((a, r_top(a) - BLOCK_H + 0.0004))
prof += [(-a, r) for a, r in reversed(side_pts)]
prof += [(-0.060, 0.2225), (0.0, 0.2222), (0.060, 0.2225)]

bm = bmesh.new()
lathe(bm, prof, 256)
finish(bm)
me_carcass = to_mesh("C21_TyreCarcass", bm)

# tread blocks: 5 ribs x 64 pitches, slanted lateral grooves, sipes on the centre rib
RIBS = [(-0.118, -0.075, 0.0), (-0.064, -0.033, 0.5), (-0.022, 0.022, 0.25), (0.033, 0.064, 0.5), (0.075, 0.118, 0.0)]
PITCH = 64
bm = bmesh.new()
for (a0, a1, off) in RIBS:
    centre = abs(a0) < 0.03
    shoulder = abs(a0) > 0.1 or abs(a1) > 0.1
    gap = math.radians(0.32 if centre else 1.05)
    slant = math.radians(1.6) if not shoulder else math.radians(0.6)
    sgn = 1 if a0 < 0 else -1
    for k in range(PITCH):
        ph0 = TAU * (k + off) / PITCH
        ph1 = TAU * (k + 1 + off) / PITCH - gap
        NU, NV = 4, 4
        top, bot = [], []
        for iu in range(NU + 1):
            rowt, rowb = [], []
            for iv in range(NV + 1):
                t = iv / NV
                a = a0 + (a1 - a0) * t
                ph = ph0 + (ph1 - ph0) * iu / NU + slant * (t - 0.5) * sgn
                rt = r_top(a)
                rb = rt - BLOCK_H - 0.0015
                rowt.append(bm.verts.new((a, rt * math.cos(ph), rt * math.sin(ph))))
                rowb.append(bm.verts.new((a, rb * math.cos(ph), rb * math.sin(ph))))
            top.append(rowt)
            bot.append(rowb)
        for iu in range(NU):
            for iv in range(NV):
                bm.faces.new((top[iu][iv], top[iu + 1][iv], top[iu + 1][iv + 1], top[iu][iv + 1]))
                bm.faces.new((bot[iu][iv], bot[iu][iv + 1], bot[iu + 1][iv + 1], bot[iu + 1][iv]))
        ring_t = [top[iu][0] for iu in range(NU + 1)] + [top[NU][iv] for iv in range(1, NV + 1)] + \
                 [top[iu][NV] for iu in range(NU - 1, -1, -1)] + [top[0][iv] for iv in range(NV - 1, 0, -1)]
        ring_b = [bot[iu][0] for iu in range(NU + 1)] + [bot[NU][iv] for iv in range(1, NV + 1)] + \
                 [bot[iu][NV] for iu in range(NU - 1, -1, -1)] + [bot[0][iv] for iv in range(NV - 1, 0, -1)]
        nring = len(ring_t)
        for i in range(nring):
            j = (i + 1) % nring
            bm.faces.new((ring_t[i], ring_b[i], ring_b[j], ring_t[j]))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me_tread = to_mesh("C21_TyreTread", bm)

# raised sidewall lettering (outboard face), tops pointing outward like a real moulded tyre
side_a_r = sorted([(r, a) for a, r in side_pts])
def a_side(r):
    for (r0, a0), (r1, a1) in zip(side_a_r, side_a_r[1:]):
        if r0 <= r <= r1:
            t = (r - r0) / (r1 - r0)
            return a0 + (a1 - a0) * t
    return side_a_r[0][1] if r < side_a_r[0][0] else side_a_r[-1][1]

def letter_mesh(text, size, r_base, phi_c, extrude=0.0006):
    cu = bpy.data.curves.new("c21txt", "FONT")
    cu.body = text
    cu.size = size
    cu.extrude = extrude
    cu.bevel_depth = 0.00015
    cu.bevel_resolution = 1
    cu.align_x = "CENTER"
    cu.space_character = 1.12
    ob = bpy.data.objects.new("c21txt", cu)
    bpy.context.scene.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.curves.remove(cu)
    for v in me.vertices:
        x, y, z = v.co
        rr = r_base + y
        ph = phi_c - x / r_base
        v.co = (a_side(rr) + 0.00045 + z, rr * math.cos(ph), rr * math.sin(ph))
    return me

bm = bmesh.new()
# local phi 0 = screen-right (car front) seen from outboard; main markings sit in the lit lower half
for text, size, rb, phc in (("HARBOR PURSUIT", 0.0175, 0.2665, math.radians(232)),
                            ("P235/50R17  96V", 0.0130, 0.2700, math.radians(312)),
                            ("H.P.T. RADIAL", 0.0085, 0.2725, math.radians(50)),
                            ("TUBELESS  M+S", 0.0085, 0.2725, math.radians(130)),
                            ("MAX LOAD 710 kg  300 kPa", 0.0060, 0.2440, math.radians(180)),
                            ("DOT HM27 2626", 0.0060, 0.2440, math.radians(5))):
    me = letter_mesh(text, size, rb, phc)
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)
me_letters = to_mesh("C21_TyreLetters", bm)

def load_tyre(me):
    """loaded contact patch: flatten below the loaded radius, bulge lower sidewalls"""
    for v in me.vertices:
        a, y, z = v.co
        if z < -0.20 and abs(a) > 0.085:
            k = smooth((-z - 0.20) / 0.12)
            v.co.x = a * (1.0 + 0.045 * k)
        if v.co.z < -HUB_Z:
            v.co.z = -HUB_Z
    me.update()

for me in (me_carcass, me_tread, me_letters):
    load_tyre(me)

# ---------------------------------------------------------------- rim barrel (lathe, machined lip)
barrel = [(-0.1000, 0.2310), (-0.0965, 0.2250), (-0.0920, 0.2090), (-0.0800, 0.2015), (-0.0600, 0.2000),
          (-0.0500, 0.1930), (-0.0420, 0.1905), (0.0050, 0.1905), (0.0150, 0.1935), (0.0300, 0.1990),
          (0.0700, 0.2010), (0.0860, 0.2030),                               # inner barrel -> step
          (0.0960, 0.2052), (0.1005, 0.2082), (0.1022, 0.2130),              # bevel into lip face
          (0.1028, 0.2200), (0.1022, 0.2265), (0.1000, 0.2310), (0.0960, 0.2340), (0.0915, 0.2348),  # rounded lip
          (0.0880, 0.2320), (0.0870, 0.2235),                               # bead seat under tyre
          (0.0820, 0.2060), (0.0300, 0.2040), (-0.0350, 0.1960), (-0.0870, 0.2060), (-0.0900, 0.2240),
          (-0.0950, 0.2360)]
LIP_IDX = set(range(12, 19))
bm = bmesh.new()
lathe(bm, barrel, 192, mat_fn=lambda i: 1 if i in LIP_IDX else 0)
finish(bm)
me_barrel = to_mesh("C21_RimBarrel", bm)

# ---------------------------------------------------------------- spoke disc (2D curve with windows)
N_SP = 5
SP0 = math.radians(90)
def dish(r):
    return 0.050 + 0.032 * smooth((r - 0.098) / 0.105)

def spoke_hw(r):   # half spoke width (arc metres), tapers from the hub to the lip
    return 0.040 + (0.027 - 0.040) * (r - 0.100) / 0.090

loops = [circle(0, 0, 0.2035, 256)]
for k in range(N_SP):
    th = SP0 + TAU * k / N_SP
    thw = th + TAU / (2 * N_SP)
    loops.append(rounded_polar(thw, 0.1035, 0.1855, lambda r: (math.pi / N_SP) - spoke_hw(r) / r, 0.0125,
                               n_side=32, n_corner=12))
    loops.append(circle(PCD_R * math.cos(th), PCD_R * math.sin(th), 0.0140, 32))
loops.append(circle(0, 0, 0.0355, 64))
me_disc = curve2d_mesh("C21_RimSpokes", loops, 0.0050, 0.0030, 3)
# subdivide long fill triangles so the dish + spoke crown bend smoothly
bm = bmesh.new()
bm.from_mesh(me_disc)
for _ in range(2):
    long_e = [e for e in bm.edges if e.calc_length() > 0.012]
    if long_e:
        bmesh.ops.subdivide_edges(bm, edges=long_e, cuts=1, use_grid_fill=False)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
bm.to_mesh(me_disc)
bm.free()

def spoke_crown(y, z):
    r = math.hypot(y, z)
    ph = math.atan2(z, y)
    d = min(abs(((ph - SP0 - TAU * k / N_SP + math.pi) % TAU) - math.pi) for k in range(N_SP))
    u = (d * r) / spoke_hw(max(r, 0.1))          # 0 at the spoke ridge, 1 at the window edge
    fr = smooth((r - 0.100) / 0.018) * (1 - smooth((r - 0.178) / 0.014))
    return 0.0065 * max(0.0, 1.0 - u) ** 1.4 * fr  # V-ish ridge along each spoke

def disc_face(y, z):
    return dish(math.hypot(y, z)) + spoke_crown(y, z)

ZCAP = 0.0050 + 0.0030 - 1e-5
cap_n = {}
for v in me_disc.vertices:
    x, y, z = v.co
    r = math.hypot(x, y)
    if abs(z) > ZCAP:     # flat cap verts get the analytic normal of the dished / crowned surface
        e = 1e-4
        dfy = (disc_face(x + e, y) - disc_face(x - e, y)) / (2 * e)
        dfz = (disc_face(x, y + e) - disc_face(x, y - e)) / (2 * e)
        n = Vector((1.0, -dfy, -dfz)).normalized()
        cap_n[v.index] = n if z > 0 else -n
    v.co = (disc_face(x, y) + z, x, y)
me_disc.update()
DISC_CAP_N = cap_n

# hub mount face seen through the lug pockets / centre bore
bm = bmesh.new()
lathe(bm, [(0.040, 0.000), (0.040, 0.0995), (0.046, 0.0995), (0.046, 0.000)], 96, cyclic_profile=True)
finish(bm)
me_hubface = to_mesh("C21_HubFace", bm)

# ---------------------------------------------------------------- hardware: lug nuts, washers, cap, valve
bm = bmesh.new()
def add_prim(bm2, mat, M):
    for f in bm2.faces:
        f.material_index = mat
    me = bpy.data.meshes.new("tmp")
    bm2.to_mesh(me)
    bm2.free()
    me.transform(M)
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)

ROT = Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1)))   # (x,y,z)->(z,x,y)
for k in range(N_SP):
    th = SP0 + TAU * k / N_SP
    cy, cz = PCD_R * math.cos(th), PCD_R * math.sin(th)
    T = Matrix.Translation((0, cy, cz))
    b2 = bmesh.new()   # washer/cone seat
    bmesh.ops.create_cone(b2, cap_ends=True, segments=32, radius1=0.0128, radius2=0.0112, depth=0.004)
    add_prim(b2, 0, T @ Matrix.Translation((0.048, 0, 0)) @ ROT)
    b2 = bmesh.new()   # hex
    bmesh.ops.create_cone(b2, cap_ends=True, segments=6, radius1=0.0110, radius2=0.0110, depth=0.015)
    bmesh.ops.bevel(b2, geom=list(b2.edges), offset=0.0009, segments=2, affect="EDGES", profile=0.5)
    add_prim(b2, 0, T @ Matrix.Translation((0.0575, 0, 0)) @ ROT @ Matrix.Rotation(th, 4, "Z"))
    b2 = bmesh.new()   # acorn dome
    bmesh.ops.create_uvsphere(b2, u_segments=24, v_segments=12, radius=0.0086)
    add_prim(b2, 0, T @ Matrix.Translation((0.0650, 0, 0)) @ Matrix.Scale(0.9, 4, (1, 0, 0)))
# centre cap: chrome dome ring + black insert
cap = [(0.0480, 0.0000), (0.0480, 0.0345), (0.0560, 0.0350), (0.0610, 0.0330), (0.0640, 0.0290),
       (0.0650, 0.0240), (0.0645, 0.0000)]
b2 = bmesh.new()
lathe(b2, cap, 64, cyclic_profile=True)
finish(b2)
add_prim(b2, 0, Matrix.Identity(4))
b2 = bmesh.new()
bmesh.ops.create_cone(b2, cap_ends=True, segments=64, radius1=0.0225, radius2=0.0215, depth=0.0016)
add_prim(b2, 1, Matrix.Translation((0.0652, 0, 0)) @ ROT)
# valve stem in a window (between spokes), from the barrel step angled outboard
thv = SP0 + TAU / (2 * N_SP) + math.radians(14)
vy, vz = 0.193 * math.cos(thv), 0.193 * math.sin(thv)
dirv = Vector((1.0, -0.35 * math.cos(thv), -0.35 * math.sin(thv))).normalized()
Rv = Vector((0, 0, 1)).rotation_difference(dirv).to_matrix().to_4x4()
b2 = bmesh.new()
bmesh.ops.create_cone(b2, cap_ends=True, segments=16, radius1=0.0055, radius2=0.0040, depth=0.022)
add_prim(b2, 2, Matrix.Translation(Vector((0.080, vy, vz)) + dirv * 0.011) @ Rv)
b2 = bmesh.new()
bmesh.ops.create_cone(b2, cap_ends=True, segments=16, radius1=0.0036, radius2=0.0036, depth=0.008)
add_prim(b2, 0, Matrix.Translation(Vector((0.080, vy, vz)) + dirv * 0.026) @ Rv)
me_hw = to_mesh("C21_WheelHardware", bm)

# ---------------------------------------------------------------- ventilated rotor
R_RI, R_RO = 0.096, 0.160
holes = []
for k in range(6):
    base = TAU * (k + 0.5) / 6
    for i, rr in enumerate((0.112, 0.126, 0.140, 0.153)):
        ph = base + (rr * 9.0) * TAU / 6      # on the slot spiral, half a pitch between slots
        holes.append(circle(rr * math.cos(ph), rr * math.sin(ph), 0.0036, 20))
me_plate_o = curve2d_mesh("C21_RotorPlateOut", [circle(0, 0, R_RO - 0.001, 192), circle(0, 0, R_RI + 0.001, 128)] + holes,
                          0.0035, 0.0010, 2)
map_plane(me_plate_o, lambda r: 0.0105)
me_plate_i = curve2d_mesh("C21_RotorPlateIn", [circle(0, 0, R_RO - 0.001, 192), circle(0, 0, R_RI + 0.001, 128)],
                          0.0035, 0.0010, 2)
map_plane(me_plate_i, lambda r: -0.0105)
bm = bmesh.new()
for k in range(40):           # curved cooling vanes between the plates
    ph0 = TAU * k / 40
    pts = []
    for i in range(7):
        rr = 0.099 + (0.158 - 0.099) * i / 6
        ph = ph0 + 0.35 * (rr - 0.099) / 0.06
        pts.append((rr, ph))
    vs_top, vs_bot = [], []
    for (rr, ph) in pts:
        tdir = Vector((0, -math.sin(ph), math.cos(ph)))
        p = Vector((0, rr * math.cos(ph), rr * math.sin(ph)))
        for side, lst in ((1, vs_top), (-1, vs_bot)):
            q = p + tdir * 0.0022 * side
            lst.append((bm.verts.new((0.0062, q.y, q.z)), bm.verts.new((-0.0062, q.y, q.z))))
    for i in range(6):
        a0, a1 = vs_top[i], vs_top[i + 1]
        b0, b1 = vs_bot[i], vs_bot[i + 1]
        bm.faces.new((a0[0], a1[0], a1[1], a0[1]))
        bm.faces.new((b0[0], b0[1], b1[1], b1[0]))
        bm.faces.new((a0[0], b0[0], b1[0], a1[0]))
        bm.faces.new((a0[1], a1[1], b1[1], b0[1]))
    bm.faces.new((vs_top[0][0], vs_top[0][1], vs_bot[0][1], vs_bot[0][0]))
    bm.faces.new((vs_top[6][0], vs_bot[6][0], vs_bot[6][1], vs_top[6][1]))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me_vanes = to_mesh("C21_RotorVanes", bm)
# hat (top hat between friction ring and hub face), 5 studs
hat = [(-0.0060, 0.0990), (0.0380, 0.0890), (0.0395, 0.0860), (0.0395, 0.0360), (0.0345, 0.0360),
       (0.0345, 0.0830), (-0.0060, 0.0930)]
bm = bmesh.new()
lathe(bm, hat, 128, cyclic_profile=True)
finish(bm)
me_hat = to_mesh("C21_RotorHat", bm)

# ---------------------------------------------------------------- caliper (wheel-local angle CAL_PHI)
def cal_halfang(span_deg):
    return lambda r: math.radians(span_deg)

body_out = rounded_polar(CAL_PHI, 0.1130, 0.1830, cal_halfang(25), 0.016)
bridge = rounded_polar(CAL_PHI, 0.1640, 0.1845, cal_halfang(19), 0.006)
body_in = rounded_polar(CAL_PHI, 0.1080, 0.1830, cal_halfang(27), 0.016)
me_c1 = curve2d_mesh("c1", [body_out], 0.0080, 0.0035, 3)
map_plane(me_c1, lambda r: 0.0265)
me_c2 = curve2d_mesh("c2", [bridge], 0.0330, 0.0030, 3)
map_plane(me_c2, lambda r: 0.0)
me_c3 = curve2d_mesh("c3", [body_in], 0.0100, 0.0035, 3)
map_plane(me_c3, lambda r: -0.0300)
bm = bmesh.new()
for me in (me_c1, me_c2, me_c3):
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)
for f in bm.faces:
    f.material_index = 0
def add_cal(b2, mat, M):
    for f in b2.faces:
        f.material_index = mat
    me = bpy.data.meshes.new("tmp")
    b2.to_mesh(me)
    b2.free()
    me.transform(M)
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)
for dph in (-11.5, 11.5):     # piston bore bulges on the outboard body
    ph = CAL_PHI + math.radians(dph)
    rr = 0.1440
    b2 = bmesh.new()
    bmesh.ops.create_cone(b2, cap_ends=True, segments=48, radius1=0.0185, radius2=0.0165, depth=0.006)
    bmesh.ops.bevel(b2, geom=[e for e in b2.edges if len(e.link_faces) == 2 and e.calc_face_angle(0) > 0.5],
                    offset=0.0022, segments=3, affect="EDGES", profile=0.5)
    add_cal(b2, 0, Matrix.Translation((0.0375, rr * math.cos(ph), rr * math.sin(ph))) @ ROT)
for dph in (-21.0, 21.0):     # bridge bolts
    ph = CAL_PHI + math.radians(dph)
    rr = 0.1745
    b2 = bmesh.new()
    bmesh.ops.create_cone(b2, cap_ends=True, segments=6, radius1=0.0062, radius2=0.0062, depth=0.006)
    add_cal(b2, 1, Matrix.Translation((0.0370, rr * math.cos(ph), rr * math.sin(ph))) @ ROT)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me_cal = to_mesh("C21_Caliper", bm)

# dust shield + knuckle stub
shield = [(-0.0270, 0.0500), (-0.0270, 0.1700), (-0.0200, 0.1760), (-0.0175, 0.1745), (-0.0240, 0.1680),
          (-0.0240, 0.0500)]
bm = bmesh.new()
lathe(bm, shield, 96, cyclic_profile=True)
bmesh.ops.create_cone(bm, cap_ends=True, segments=32, radius1=0.055, radius2=0.050, depth=0.06,
                      matrix=Matrix.Translation((-0.060, 0, 0)) @ ROT)
finish(bm)
me_shield = to_mesh("C21_DustShield", bm)

# ---------------------------------------------------------------- materials -> meshes, sharp edges
def setmats(me, mats, sharp=35.0):
    me.materials.clear()
    for m in mats:
        me.materials.append(m)
    for p in me.polygons:
        p.use_smooth = True
    try:
        me.set_sharp_from_angle(angle=math.radians(sharp))
    except Exception as e:
        print("sharp fail", me.name, e)

setmats(me_carcass, [m_tyre], 60)
setmats(me_tread, [m_tyre], 50)
setmats(me_letters, [m_letter], 50)
setmats(me_barrel, [m_alloy, m_lip], 45)
setmats(me_disc, [m_alloy], 40)
for e in me_disc.edges:
    e.use_edge_sharp = False
vn = [Vector(v.normal) for v in me_disc.vertices]
for i, n in DISC_CAP_N.items():
    vn[i] = n
me_disc.normals_split_custom_set_from_vertices(vn)
setmats(me_hubface, [m_iron], 40)
setmats(me_hw, [m_chrome, m_capblack, m_valve], 40)
setmats(me_plate_o, [m_rotor], 50)
setmats(me_plate_i, [m_rotor], 50)
setmats(me_vanes, [m_iron], 50)
setmats(me_hat, [m_iron], 40)
setmats(me_cal, [m_cal, m_zinc], 40)
setmats(me_shield, [m_shield], 40)

PARTS = [("Tyre", me_carcass), ("Tread", me_tread), ("Letters", me_letters), ("RimBarrel", me_barrel),
         ("RimSpokes", me_disc), ("HubFace", me_hubface), ("Hardware", me_hw), ("RotorOut", me_plate_o),
         ("RotorIn", me_plate_i), ("RotorVanes", me_vanes), ("RotorHat", me_hat), ("Caliper", me_cal),
         ("Shield", me_shield)]

ntri = 0
for corner, side, ysgn in (("Ft_L", 1, -1), ("Ft_R", -1, -1), ("Bk_L", 1, 1), ("Bk_R", -1, 1)):
    hub = Vector((side * TRACK_X, ysgn * AXLE_Y, HUB_Z))
    base = Matrix.Translation(hub)
    if side < 0:
        base = base @ Matrix.Rotation(math.pi, 4, "Z")
    for nm, me in PARTS:
        o = bpy.data.objects.new(f"C21CV_Wheel_{corner}_{nm}", me)
        M = base
        if nm == "Caliper" and side < 0:
            M = base @ Matrix.Rotation(math.pi - 2 * CAL_PHI, 4, "X")
        o.matrix_world = M
        o["c20_cruiser"] = 1
        o["c21_wheel"] = 1
        coll.objects.link(o)
        if nm == "Tread":
            bv = o.modifiers.new("c21_bevel", "BEVEL")
            bv.width = 0.0011
            bv.segments = 2
            bv.limit_method = "ANGLE"
            bv.angle_limit = math.radians(40)
        ntri += sum(len(p.vertices) - 2 for p in me.polygons)
print("C21_WHEEL_TRIS_PER_CORNER", ntri // 4)

for m in list(bpy.data.materials):
    if m.users == 0:
        bpy.data.materials.remove(m)
bpy.ops.file.pack_all()
bpy.context.preferences.filepaths.save_version = 0   # no .blend1 next to the asset
bpy.ops.wm.save_as_mainfile(filepath=str(OUT), compress=True)
print("SAVED", OUT)
