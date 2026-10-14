#!/usr/bin/env python3
"""Cycle-20 P0: build the HMPD cruiser asset from the CC-BY 4.0 'Police car' by Mateusz Wolinski (jeandiz).

blender -b --factory-startup -P scripts/build_hmpd_cruiser_c20.py
 -> assets/meshes/harbor_metro/hmpd_cruiser_c20/hmpd_cruiser_c20.blend (collection HMPD_Cruiser_C20, images packed)

* hierarchy flattened (rig/empties/helper icosphere removed), rescaled to a 2.91 m wheelbase,
  origin = ground centre between axles, front = -Y, width = X
* materials rebuilt: clear-coat HMPD navy/white paint from the repainted atlas
  (scripts/gen_hmpd_cruiser_livery_c20.py) + object-space projection decals (HMPD side word,
  HARBOR METRO POLICE sub-line + gold pinstripe, unit 27 / DIAL 911 quarters, hood crest, roof number),
  rocker road-grime, lamp lens masks with night emission, transmissive glass, textured cabin,
  emissive red/blue lightbar, rubber/steel wheel split
* marker empties for headlamps / tail lamps / lightbar so the beauty script can add real lights
Night/day is switched in the beauty script through materials' node named 'C20_NIGHT' (Value 0/1).
"""
import bpy, os
import math as pymath
from pathlib import Path
from mathutils import Vector, Matrix

SRC = Path("/workspace/vaultline-blender/refs/base_car/jeandiz_police_car_ccby/police_car_jeandiz.glb")
D = Path("/workspace/Fury/assets/meshes/harbor_metro/hmpd_cruiser_c20")
TEX = D / "tex"
OUT = D / "hmpd_cruiser_c20.blend"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(SRC))

# ---- flatten
meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.name != "Icosphere"]
for o in meshes:
    mw = o.matrix_world.copy()
    o.parent = None
    o.matrix_world = mw
for o in list(bpy.data.objects):
    if o not in meshes:
        bpy.data.objects.remove(o, do_unlink=True)
bpy.context.view_layer.update()

fl = bpy.data.objects["CrownVic.Wheel.Ft.L_0"]; bl = bpy.data.objects["CrownVic.Wheel.Bk.L_0"]
fr = bpy.data.objects["CrownVic.Wheel.Ft.R_0"]
def bbc(o):
    bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
    return sum(bb, Vector()) / 8, min(b.z for b in bb)
cfl, zfl = bbc(fl); cbl, _ = bbc(bl); cfr, _ = bbc(fr)
S = 2.91 / (cbl.y - cfl.y)
off = Vector(((cfl.x + cfr.x) / 2, (cfl.y + cbl.y) / 2, zfl - 0.008))
T = Matrix.Scale(S, 4) @ Matrix.Translation(-off)
for o in meshes:
    o.matrix_world = T @ o.matrix_world
bpy.context.view_layer.update()
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
# wheels keep their own origin at the hub (for rotation); others origin at car origin
for o in meshes:
    if "Wheel" not in o.name:
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True); bpy.context.view_layer.objects.active = o
        bpy.context.scene.cursor.location = (0, 0, 0)
        bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    else:
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True); bpy.context.view_layer.objects.active = o
        bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")

REN = {"CrownVic.Body_0": "C20CV_Body", "windows glass_0": "C20CV_Glass", "interior_0": "C20CV_Interior",
       "Roof light bar_0": "C20CV_Lightbar", "roof lights_0": "C20CV_LightbarHousing"}
for o in meshes:
    nm = REN.get(o.name, "C20CV_" + o.name.replace("CrownVic.", "").replace("_0", "").replace(".", "_"))
    o.name = nm; o.data.name = nm + "_mesh"
    for p in o.data.polygons:
        p.use_smooth = True
    if hasattr(o.data, "use_auto_smooth"):
        pass
    o["c20_cruiser"] = 1

old_images = {img.name: img for img in bpy.data.images}

def img(path, color=True):
    im = bpy.data.images.load(str(path), check_existing=True)
    if not color:
        im.colorspace_settings.name = "Non-Color"
    return im

def new_mat(name):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial"); out.location = (900, 0)
    b = nt.nodes.new("ShaderNodeBsdfPrincipled"); b.location = (600, 0)
    nt.links.new(b.outputs[0], out.inputs[0])
    night = nt.nodes.new("ShaderNodeValue"); night.name = "C20_NIGHT"; night.label = "C20_NIGHT"
    night.outputs[0].default_value = 0.0; night.location = (-900, 600)
    return m, nt, b, night

def tex(nt, image, loc, vec=None, ext="REPEAT"):
    t = nt.nodes.new("ShaderNodeTexImage"); t.image = image; t.location = loc; t.extension = ext
    if vec is not None:
        nt.links.new(vec, t.inputs[0])
    return t

def math(nt, op, a, b=None, loc=(0, 0), clamp=False):
    n = nt.nodes.new("ShaderNodeMath"); n.operation = op; n.location = loc; n.use_clamp = clamp
    for i, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[i].default_value = v
        else:
            nt.links.new(v, n.inputs[i])
    return n.outputs[0]

def mixc(nt, fac, a, b, loc=(0, 0)):
    n = nt.nodes.new("ShaderNodeMix"); n.data_type = "RGBA"; n.location = loc
    for sock, v in ((n.inputs[0], fac), (n.inputs[6], a), (n.inputs[7], b)):
        if isinstance(v, (int, float)):
            sock.default_value = v
        elif isinstance(v, tuple):
            sock.default_value = v
        else:
            nt.links.new(v, sock)
    return n.outputs[2]

# ---------------- body paint
body = bpy.data.objects["C20CV_Body"]
m, nt, b, night = new_mat("C20CV_Paint")
L = nt.links
uvb = tex(nt, img(TEX / "body_base_hmpd.png"), (-700, 300))
lm = tex(nt, img(TEX / "body_lights_mask.png", False), (-700, -350))
lsep = nt.nodes.new("ShaderNodeSeparateColor"); L.new(lm.outputs[0], lsep.inputs[0]); lsep.location = (-450, -350)
tc = nt.nodes.new("ShaderNodeTexCoord"); tc.location = (-1500, 0)
sep = nt.nodes.new("ShaderNodeSeparateXYZ"); L.new(tc.outputs["Object"], sep.inputs[0]); sep.location = (-1300, 0)
geo = nt.nodes.new("ShaderNodeNewGeometry"); geo.location = (-1500, -300)
vt = nt.nodes.new("ShaderNodeVectorTransform"); vt.vector_type = "NORMAL"; vt.convert_from = "WORLD"; vt.convert_to = "OBJECT"
L.new(geo.outputs["Normal"], vt.inputs[0]); vt.location = (-1300, -300)
nsep = nt.nodes.new("ShaderNodeSeparateXYZ"); L.new(vt.outputs[0], nsep.inputs[0]); nsep.location = (-1100, -300)
X, Y, Z = sep.outputs[0], sep.outputs[1], sep.outputs[2]
NX, NZ = nsep.outputs[0], nsep.outputs[2]

def box_decal(image, u_of, v_of, mask, loc):
    """u_of/v_of: sockets already in 0..1; mask: socket 0..1"""
    comb = nt.nodes.new("ShaderNodeCombineXYZ"); comb.location = (loc[0] - 200, loc[1])
    L.new(u_of, comb.inputs[0]); L.new(v_of, comb.inputs[1])
    t = tex(nt, image, loc, comb.outputs[0], ext="CLIP")
    a = math(nt, "MULTIPLY", t.outputs[1], mask, (loc[0] + 250, loc[1] - 100))
    return t.outputs[0], a

def lin(sock, a, b, loc):
    """(sock - a) / (b - a)"""
    s1 = math(nt, "SUBTRACT", sock, a, loc)
    return math(nt, "DIVIDE", s1, b - a, (loc[0] + 150, loc[1]))

col = uvb.outputs[0]
DEC = {}
# side livery box (filled in from measured door band) — normalised car metres
SIDE = dict(y0=-1.22, y1=1.12, z0=0.44, z1=0.94)
QUARTER = dict(y0=1.30, y1=1.95, z0=0.62, z1=0.86)
ROOF = dict(x0=-0.30, x1=0.30, y0=0.50, y1=1.00)
HOOD = dict(x0=-0.55, x1=0.55, y0=-2.25, y1=-1.70)
y = 600
for side in (1, -1):
    # u: front->rear reading direction differs per side
    if side == 1:
        u = lin(Y, SIDE["y0"], SIDE["y1"], (-1000, y))
        uq = lin(Y, QUARTER["y0"], QUARTER["y1"], (-1000, y - 200))
    else:
        u = lin(Y, SIDE["y1"], SIDE["y0"], (-1000, y))
        uq = lin(Y, QUARTER["y1"], QUARTER["y0"], (-1000, y - 200))
    v = lin(Z, SIDE["z0"], SIDE["z1"], (-1000, y - 100))
    vq = lin(Z, QUARTER["z0"], QUARTER["z1"], (-1000, y - 300))
    nm = math(nt, "MULTIPLY", NX, float(side), (-800, y - 400))
    nm = math(nt, "GREATER_THAN", nm, 0.45, (-650, y - 400))
    c1, a1 = box_decal(img(TEX / "decal_side.png"), u, v, nm, (-450, y))
    col = mixc(nt, a1, col, c1, (-100, y))
    c2, a2 = box_decal(img(TEX / "decal_quarter.png"), uq, vq, nm, (-450, y - 250))
    col = mixc(nt, a2, col, c2, (50, y - 250))
    y -= 700
up = math(nt, "GREATER_THAN", NZ, 0.55, (-650, -900))
ur = lin(X, ROOF["x1"], ROOF["x0"], (-1000, -900)); vr = lin(Y, ROOF["y1"], ROOF["y0"], (-1000, -1000))
c3, a3 = box_decal(img(TEX / "decal_roof.png"), ur, vr, up, (-450, -950))
col = mixc(nt, a3, col, c3, (100, -950))
uh = lin(X, HOOD["x1"], HOOD["x0"], (-1000, -1200)); vh = lin(Y, HOOD["y1"], HOOD["y0"], (-1000, -1300))
c4, a4 = box_decal(img(TEX / "decal_hood.png"), uh, vh, up, (-450, -1250))
col = mixc(nt, a4, col, c4, (250, -1250))

# road grime on rockers / lower bumpers: noise * (1 - smoothstep(z))
noi = nt.nodes.new("ShaderNodeTexNoise"); noi.inputs["Scale"].default_value = 9.0; noi.inputs["Detail"].default_value = 8
L.new(tc.outputs["Object"], noi.inputs[0]); noi.location = (-700, -1550)
zr = nt.nodes.new("ShaderNodeMapRange"); zr.location = (-500, -1500)
L.new(Z, zr.inputs[0]); zr.inputs[1].default_value = 0.18; zr.inputs[2].default_value = 0.55
zr.inputs[3].default_value = 1.0; zr.inputs[4].default_value = 0.0
g = math(nt, "MULTIPLY", zr.outputs[0], noi.outputs[0], (-300, -1500))
g = math(nt, "MULTIPLY", g, 0.85, (-150, -1500), clamp=True)
col = mixc(nt, g, col, (0.13, 0.11, 0.09, 1.0), (400, -900))
L.new(col, b.inputs["Base Color"])
# lens areas: smooth + emissive at night
lens = math(nt, "MAXIMUM", lsep.outputs[0], lsep.outputs[1], (-250, -350))
rough = math(nt, "MULTIPLY", g, 0.45, (0, -1500))
rough = math(nt, "ADD", rough, 0.42, (150, -1500))
rl = mixc(nt, lens, (0.26, 0.26, 0.26, 1), (0.04, 0.04, 0.04, 1), (300, -600))
rr = math(nt, "ADD", rough, 0.0, (300, -1500))
rfin = math(nt, "MULTIPLY", rr, math(nt, "SUBTRACT", 1.0, lens, (200, -1650)), (450, -1550))
rfin = math(nt, "ADD", rfin, math(nt, "MULTIPLY", lens, 0.05, (300, -1700)), (500, -1650))
L.new(rfin, b.inputs["Roughness"])
b.inputs["Metallic"].default_value = 0.0
b.inputs["Specular IOR Level"].default_value = 0.25
b.inputs["Coat Weight"].default_value = 1.0
b.inputs["Coat Roughness"].default_value = 0.035
b.inputs["Coat IOR"].default_value = 1.5
# emission: headlamp warm white, tail red, both * night
ec = mixc(nt, lsep.outputs[1], (1.0, 0.92, 0.80, 1), (1.0, 0.04, 0.02, 1), (300, -300))
L.new(ec, b.inputs["Emission Color"])
es = math(nt, "MULTIPLY", lens, night.outputs[0], (300, -450))
es = math(nt, "MULTIPLY", es, math(nt, "ADD", math(nt, "MULTIPLY", lsep.outputs[0], 2.2, (200, -550)), 2.0, (300, -550)), (450, -450))
L.new(es, b.inputs["Emission Strength"])
body.data.materials.clear(); body.data.materials.append(m)

# ---------------- glass
m, nt, b, night = new_mat("C20CV_Glass")
b.inputs["Base Color"].default_value = (0.78, 0.84, 0.82, 1)
b.inputs["Roughness"].default_value = 0.015
b.inputs["IOR"].default_value = 1.52
b.inputs["Transmission Weight"].default_value = 1.0
b.inputs["Metallic"].default_value = 0.0
m.blend_method = "HASHED" if hasattr(m, "blend_method") else None
o = bpy.data.objects["C20CV_Glass"]; o.data.materials.clear(); o.data.materials.append(m)

# ---------------- interior
m, nt, b, night = new_mat("C20CV_Interior")
t2 = tex(nt, old_images["Image_2"], (-500, 100)); t3 = tex(nt, old_images["Image_3"], (-500, -200))
nt.links.new(t2.outputs[0], b.inputs["Base Color"])
b.inputs["Roughness"].default_value = 0.72; b.inputs["Metallic"].default_value = 0.0
nt.links.new(t3.outputs[0], b.inputs["Emission Color"])
es = math(nt, "MULTIPLY", night.outputs[0], 1.2, (0, -300)); nt.links.new(es, b.inputs["Emission Strength"])
o = bpy.data.objects["C20CV_Interior"]; o.data.materials.clear(); o.data.materials.append(m)

# ---------------- lightbar lenses
m, nt, b, night = new_mat("C20CV_LightbarLens")
t4 = tex(nt, old_images["Image_4"], (-600, 200)); t5 = tex(nt, old_images["Image_5"], (-600, -100), None)
t6 = tex(nt, old_images["Image_6"], (-600, -400))
for t in (t5, t6):
    t.image.colorspace_settings.name = "Non-Color"
s5 = nt.nodes.new("ShaderNodeSeparateColor"); nt.links.new(t5.outputs[0], s5.inputs[0])
s6 = nt.nodes.new("ShaderNodeSeparateColor"); nt.links.new(t6.outputs[0], s6.inputs[0])
nt.links.new(t4.outputs[0], b.inputs["Base Color"])
nt.links.new(s5.outputs[1], b.inputs["Roughness"])
nt.links.new(s5.outputs[2], b.inputs["Metallic"])
tw = math(nt, "MULTIPLY", s6.outputs[0], 0.6, (-200, -400)); nt.links.new(tw, b.inputs["Transmission Weight"])
# saturated lens colour -> emission (red / blue), white segments stay dark (steady-burn off)
hsv = nt.nodes.new("ShaderNodeSeparateColor"); hsv.mode = "HSV"; nt.links.new(t4.outputs[0], hsv.inputs[0])
satm = math(nt, "GREATER_THAN", hsv.outputs[1], 0.45, (-200, 0))
day = math(nt, "MULTIPLY", satm, 0.6, (0, 50))
nn = math(nt, "MULTIPLY", satm, night.outputs[0], (0, -50)); nn = math(nt, "MULTIPLY", nn, 16.0, (150, -50))
es = math(nt, "ADD", day, nn, (300, 0))
rgb = nt.nodes.new("ShaderNodeSeparateColor"); nt.links.new(t4.outputs[0], rgb.inputs[0])
isred = math(nt, "GREATER_THAN", math(nt, "SUBTRACT", rgb.outputs[0], rgb.outputs[2], (-100, 300)), 0.0, (50, 300))
ecol = mixc(nt, isred, (0.03, 0.18, 1.0, 1), (1.0, 0.02, 0.01, 1), (200, 300))
nt.links.new(ecol, b.inputs["Emission Color"]); nt.links.new(es, b.inputs["Emission Strength"])
o = bpy.data.objects["C20CV_Lightbar"]; o.data.materials.clear(); o.data.materials.append(m)

# housing (chrome/black)
m, nt, b, night = new_mat("C20CV_LightbarHousing")
b.inputs["Base Color"].default_value = (0.05, 0.05, 0.055, 1); b.inputs["Metallic"].default_value = 0.6
b.inputs["Roughness"].default_value = 0.32
o = bpy.data.objects["C20CV_LightbarHousing"]; o.data.materials.clear(); o.data.materials.append(m)

# measured tyre geometry (normalised): outer radius ~0.322, rim flange ~0.215
SW_R0, SW_R1 = 0.248, 0.300
# ---------------- wheels: texture lum < 0.3 = rubber, else black-painted steel; bright = chrome cap
m, nt, b, night = new_mat("C20CV_Wheel")
t7 = tex(nt, old_images["Image_7"], (-700, 0))
hsv = nt.nodes.new("ShaderNodeSeparateColor"); hsv.mode = "HSV"; nt.links.new(t7.outputs[0], hsv.inputs[0])
val = hsv.outputs[2]
chrome = math(nt, "GREATER_THAN", val, 0.6, (-300, 200))
rub = math(nt, "LESS_THAN", val, 0.30, (-300, 0))
noi = nt.nodes.new("ShaderNodeTexNoise"); noi.inputs["Scale"].default_value = 60.0
c = mixc(nt, rub, (0.022, 0.022, 0.024, 1), (0.011, 0.011, 0.012, 1), (0, 100))
c = mixc(nt, chrome, c, (0.75, 0.76, 0.78, 1), (150, 100))
nt.links.new(c, b.inputs["Base Color"])
met = math(nt, "MAXIMUM", math(nt, "MULTIPLY", math(nt, "SUBTRACT", 1.0, rub, (0, -100)), 0.35, (100, -100)), chrome, (250, -100))
nt.links.new(met, b.inputs["Metallic"])
r = math(nt, "ADD", math(nt, "MULTIPLY", rub, 0.40, (0, -250)), 0.34, (150, -250))
r = math(nt, "SUBTRACT", r, math(nt, "MULTIPLY", chrome, 0.28, (150, -350)), (300, -300))
nt.links.new(r, b.inputs["Roughness"])
# sidewall lettering: polar mapping in wheel object space (axle = X, origin = hub)
wtc = nt.nodes.new("ShaderNodeTexCoord"); wsep = nt.nodes.new("ShaderNodeSeparateXYZ")
nt.links.new(wtc.outputs["Object"], wsep.inputs[0])
ang = math(nt, "ARCTAN2", wsep.outputs[2], wsep.outputs[1], (-900, -500))
uu = math(nt, "ADD", math(nt, "DIVIDE", ang, 6.28318, (-750, -500)), 0.5, (-600, -500))
rad = nt.nodes.new("ShaderNodeVectorMath"); rad.operation = "LENGTH"
cmb = nt.nodes.new("ShaderNodeCombineXYZ"); nt.links.new(wsep.outputs[1], cmb.inputs[1]); nt.links.new(wsep.outputs[2], cmb.inputs[2])
nt.links.new(cmb.outputs[0], rad.inputs[0])
vv = math(nt, "DIVIDE", math(nt, "SUBTRACT", rad.outputs["Value"], SW_R0, (-750, -650)), SW_R1 - SW_R0, (-600, -650))
uvc = nt.nodes.new("ShaderNodeCombineXYZ"); nt.links.new(uu, uvc.inputs[0]); nt.links.new(vv, uvc.inputs[1])
swt = tex(nt, img(TEX / "tyre_sidewall.png", False), (-400, -600), uvc.outputs[0], ext="CLIP")
swh = math(nt, "MULTIPLY", swt.outputs[0], rub, (-150, -600))
hh = math(nt, "ADD", math(nt, "MULTIPLY", noi.outputs[0], 0.15, (-150, -750)), swh, (0, -700))
bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.35; bump.inputs["Distance"].default_value = 0.002
nt.links.new(hh, bump.inputs["Height"]); nt.links.new(bump.outputs[0], b.inputs["Normal"])
c = mixc(nt, math(nt, "MULTIPLY", swh, 0.35, (150, -600)), c, (0.06, 0.06, 0.065, 1), (300, 100))
nt.links.new(c, b.inputs["Base Color"])
for o in bpy.data.objects:
    if o.name.startswith("C20CV_Wheel_"):
        o.data.materials.clear(); o.data.materials.append(m)

m, nt, b, night = new_mat("C20CV_Brake")
t8 = tex(nt, old_images["Image_8"], (-500, 0)); nt.links.new(t8.outputs[0], b.inputs["Base Color"])
b.inputs["Metallic"].default_value = 0.85; b.inputs["Roughness"].default_value = 0.38
for o in bpy.data.objects:
    if o.name.startswith("C20CV_WheelBrake"):
        o.data.materials.clear(); o.data.materials.append(m)

# rear licence plate: raycast onto the trunk/bumper face
dg = bpy.context.evaluated_depsgraph_get()
hit, loc, nrm, idx, ob, mtx = bpy.context.scene.ray_cast(dg, Vector((0, 4.0, 0.775)), Vector((0, -1, 0)))
print("PLATE_HIT", hit, loc, nrm)
if hit:
    bpy.ops.mesh.primitive_plane_add(size=1, location=loc + Vector((0, 0.008, 0)))
    pl = bpy.context.active_object; pl.name = "C20CV_PlateRear"
    pl.scale = (0.305, 0.153, 1)
    pl.rotation_euler = (pymath.radians(85), 0, pymath.radians(180))
    bpy.ops.object.transform_apply(scale=True)
    m, nt, b, night = new_mat("C20CV_Plate")
    tp = tex(nt, img(TEX / "plate_rear.png"), (-400, 0)); nt.links.new(tp.outputs[0], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.35; b.inputs["Metallic"].default_value = 0.3
    pl.data.materials.append(m); pl["c20_cruiser"] = 1
    meshes.append(pl)

# black powder-coat trim: push bar + door handles (loose parts of the body mesh)
import bmesh
m, nt, b, night = new_mat("C20CV_Trim")
b.inputs["Base Color"].default_value = (0.012, 0.012, 0.013, 1)
b.inputs["Roughness"].default_value = 0.42; b.inputs["Metallic"].default_value = 0.15
b.inputs["Coat Weight"].default_value = 0.3; b.inputs["Coat Roughness"].default_value = 0.2
body.data.materials.append(m)
bm = bmesh.new(); bm.from_mesh(body.data); bm.faces.ensure_lookup_table()
seen = set(); ntrim = 0
for f in bm.faces:
    if f.index in seen:
        continue
    stack = [f]; comp = []; seen.add(f.index)
    while stack:
        g = stack.pop(); comp.append(g)
        for e in g.edges:
            for h in e.link_faces:
                if h.index not in seen:
                    seen.add(h.index); stack.append(h)
    vs = [v.co for g in comp for v in g.verts]
    mn = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    mx = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    sz = mx - mn
    pushbar = max(abs(mn.x), abs(mx.x)) < 0.40 and mn.y < -2.27 and mx.z < 0.9
    handle = min(abs(mn.x), abs(mx.x)) > 0.84 and sz.x < 0.05 and 0.15 < sz.y < 0.22 and sz.z < 0.08
    if pushbar or handle:
        for g in comp:
            g.material_index = 1
        ntrim += 1
bm.to_mesh(body.data); bm.free()
print("TRIM_ISLANDS", ntrim)

# drop unused glTF materials
for mat in list(bpy.data.materials):
    if not mat.name.startswith("C20CV_"):
        bpy.data.materials.remove(mat)

# ---------------- light markers (from lens mask on body faces)
import numpy as np
mimg = bpy.data.images.load(str(TEX / "body_lights_mask.png"), check_existing=True)
W, H = mimg.size; px = np.array(mimg.pixels[:]).reshape(H, W, 4)
me = body.data; uvl = me.uv_layers.active.data
acc = {"HeadL": [], "TailL": []}
for p in me.polygons:
    uu = sum(uvl[i].uv.x for i in p.loop_indices) / p.loop_total
    vv = sum(uvl[i].uv.y for i in p.loop_indices) / p.loop_total
    q = px[int(min(max(vv, 0), 0.999) * H), int(min(max(uu, 0), 0.999) * W)]
    if q[0] > 0.5:
        acc["HeadL"].append(p.center.copy())
    elif q[1] > 0.5:
        acc["TailL"].append(p.center.copy())
coll = bpy.data.collections.new("HMPD_Cruiser_C20")
bpy.context.scene.collection.children.link(coll)
for k, pts in acc.items():
    for sgn, sname in ((1, "R"), (-1, "L")):
        sel = [p for p in pts if p.x * sgn > 0.2]
        if not sel:
            print("no marker", k, sname); continue
        c = sum(sel, Vector()) / len(sel)
        e = bpy.data.objects.new(f"C20CV_Mk_{k}_{sname}", None); e.location = c
        coll.objects.link(e); print("MARKER", e.name, tuple(round(x, 3) for x in c), len(sel))
lb = bpy.data.objects["C20CV_Lightbar"]
bb = [lb.matrix_world @ Vector(cc) for cc in lb.bound_box]
e = bpy.data.objects.new("C20CV_Mk_Lightbar", None); e.location = sum(bb, Vector()) / 8; coll.objects.link(e)
print("LIGHTBAR", tuple(round(x, 3) for x in e.location), "dims", tuple(round(x, 3) for x in lb.dimensions))

for o in meshes:
    for c in o.users_collection:
        c.objects.unlink(o)
    coll.objects.link(o)
bb = [o.matrix_world @ Vector(cc) for o in meshes for cc in o.bound_box]
print("CAR_BBOX", tuple(round(min(v[i] for v in bb), 3) for i in range(3)), tuple(round(max(v[i] for v in bb), 3) for i in range(3)))
print("TRIS", sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes))
for im in bpy.data.images:
    if im.users == 0:
        bpy.data.images.remove(im)
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT), compress=True)
print("SAVED", OUT)
