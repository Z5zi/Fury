#!/usr/bin/env python3
"""Harbor Metro Cycle-16 AAA pedestrians — Antonia.Polygon CC0 + real garment shells.

Gate-C rebuild:
- Keep Antonia facial topology (eyes/iris/pupil/cornea/lips/brows/lashes)
- Project portrait albedo onto skinFace via front-camera UV (not mismatched atlas)
- Separate garment SHELLS with solidify + noise folds (not body paint)
- Particle hair converted to mesh for volume
- Natural idle arms-down, five distinct silhouettes
No Rockstar/GTA IP.
"""
from __future__ import annotations

import math
import os
import shutil
import sys
import traceback
from pathlib import Path

import bpy
import bmesh
from mathutils import Euler, Vector, Matrix

ROOT = Path("/workspace/vaultline-blender")
sys.path.insert(0, str(ROOT / "scripts"))
from vl_common import OUT, RENDERS, apply_mat, clear_scene, shade_smooth  # noqa: E402

ANTONIA = ROOT / "refs/ped_bases/antonia/AntoniaA-1.2.obj"
FACES = ROOT / "refs/ped_faces"
FURY_PEDS = Path("/workspace/Fury/assets/meshes/harbor_metro/peds")
ART_PEDS = Path("/workspace/Fury/artifacts/aaa_meridian_block/blender_peds")
REND_PEDS = RENDERS / "peds_v16"
for _p in (OUT, REND_PEDS, FURY_PEDS, ART_PEDS, FACES):
    _p.mkdir(parents=True, exist_ok=True)

PEDS = [
    dict(name="rae", height=1.72, pose="idle_a", male=False, jacket=True,
         skin=(0.86, 0.70, 0.58, 1.0), shirt=(0.16, 0.28, 0.48, 1.0),
         pants=(0.12, 0.13, 0.16, 1.0), hair=(0.06, 0.04, 0.03, 1.0),
         shoes=(0.05, 0.05, 0.06, 1.0), iris=(0.22, 0.30, 0.38, 1.0),
         jacket_col=(0.10, 0.12, 0.16, 1.0), face=FACES / "rae_face.png",
         hair_style="long"),
    dict(name="dane", height=1.84, pose="idle_b", male=True, jacket=False,
         skin=(0.42, 0.28, 0.20, 1.0), shirt=(0.52, 0.14, 0.12, 1.0),
         pants=(0.08, 0.09, 0.11, 1.0), hair=(0.04, 0.03, 0.02, 1.0),
         shoes=(0.08, 0.06, 0.05, 1.0), iris=(0.28, 0.18, 0.10, 1.0),
         jacket_col=(0.12, 0.12, 0.12, 1.0), face=FACES / "dane_face.png",
         hair_style="short"),
    dict(name="suki", height=1.62, pose="idle_c", male=False, jacket=False,
         skin=(0.90, 0.78, 0.70, 1.0), shirt=(0.12, 0.48, 0.46, 1.0),
         pants=(0.20, 0.14, 0.30, 1.0), hair=(0.05, 0.03, 0.03, 1.0),
         shoes=(0.75, 0.75, 0.78, 1.0), iris=(0.28, 0.36, 0.26, 1.0),
         jacket_col=(0.10, 0.18, 0.20, 1.0), face=FACES / "suki_face.png",
         hair_style="bob"),
    dict(name="noah", height=1.78, pose="idle_d", male=True, jacket=True,
         skin=(0.72, 0.54, 0.42, 1.0), shirt=(0.20, 0.22, 0.28, 1.0),
         pants=(0.12, 0.16, 0.30, 1.0), hair=(0.10, 0.08, 0.06, 1.0),
         shoes=(0.06, 0.06, 0.07, 1.0), iris=(0.20, 0.28, 0.34, 1.0),
         jacket_col=(0.10, 0.12, 0.16, 1.0), face=FACES / "noah_face.png",
         hair_style="short"),
    dict(name="ivy", height=1.66, pose="idle_e", male=False, jacket=True,
         skin=(0.88, 0.72, 0.62, 1.0), shirt=(0.20, 0.30, 0.24, 1.0),
         pants=(0.34, 0.22, 0.16, 1.0), hair=(0.48, 0.22, 0.10, 1.0),
         shoes=(0.10, 0.05, 0.04, 1.0), iris=(0.32, 0.38, 0.22, 1.0),
         jacket_col=(0.16, 0.10, 0.08, 1.0), face=FACES / "ivy_face.png",
         hair_style="long"),
]


def principled(name, base, metallic=0.0, roughness=0.45, coat=0.0,
               sss=0.0, sss_radius=(1.0, 0.2, 0.1), transmission=0.0,
               specular=0.5, alpha=1.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = base
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if "Coat Weight" in bsdf.inputs:
        bsdf.inputs["Coat Weight"].default_value = coat
        bsdf.inputs["Coat Roughness"].default_value = 0.05
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = specular
    if sss > 0.01:
        if "Subsurface Weight" in bsdf.inputs:
            bsdf.inputs["Subsurface Weight"].default_value = sss
        if "Subsurface Radius" in bsdf.inputs:
            bsdf.inputs["Subsurface Radius"].default_value = sss_radius
        if "Subsurface Scale" in bsdf.inputs:
            bsdf.inputs["Subsurface Scale"].default_value = 0.45
    if transmission > 0.01 and "Transmission Weight" in bsdf.inputs:
        bsdf.inputs["Transmission Weight"].default_value = transmission
    if alpha < 0.999:
        bsdf.inputs["Alpha"].default_value = alpha
        try:
            m.blend_method = "HASHED"
        except Exception:
            pass
    return m


def make_skin_face_mat(name, skin, face_path: Path):
    """SkinFace with projected portrait (UVMap_Face) mixed over SSS skin."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nodes, links = nt.nodes, nt.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Roughness"].default_value = 0.38
    if "Subsurface Weight" in bsdf.inputs:
        bsdf.inputs["Subsurface Weight"].default_value = 0.55
    if "Subsurface Radius" in bsdf.inputs:
        bsdf.inputs["Subsurface Radius"].default_value = (1.0, 0.28, 0.12)
    if "Subsurface Scale" in bsdf.inputs:
        bsdf.inputs["Subsurface Scale"].default_value = 0.55
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.45
    skin_rgb = nodes.new("ShaderNodeRGB")
    skin_rgb.outputs[0].default_value = skin
    # Subtle procedural pores
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 480.0
    noise.inputs["Detail"].default_value = 8.0
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.04
    bump.inputs["Distance"].default_value = 0.0015
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    # Do NOT paste frontal portraits onto Antonia UVs — portraits include eyes and
    # ghost over the mesh irises. Use SSS skin + geometric eyes/lips/brows only.
    # Soft cheek warmth via noise color variation.
    noise2 = nodes.new("ShaderNodeTexNoise")
    noise2.inputs["Scale"].default_value = 12.0
    noise2.inputs["Detail"].default_value = 4.0
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = (skin[0]*0.92, skin[1]*0.75, skin[2]*0.72, 1)
    ramp.color_ramp.elements[1].position = 0.7
    ramp.color_ramp.elements[1].color = skin
    links.new(noise2.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def make_cloth_mat(name, base, roughness=0.72, bump_scale=55.0, bump_str=0.12):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nodes, links = nt.nodes, nt.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = base
    bsdf.inputs["Roughness"].default_value = roughness
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.25
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = bump_scale
    noise.inputs["Detail"].default_value = 10.0
    noise.inputs["Roughness"].default_value = 0.55
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = bump_str
    bump.inputs["Distance"].default_value = 0.004
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def make_mats(spec):
    skin = spec["skin"]
    warm = (min(1.0, skin[0] * 1.08), skin[1] * 0.85, skin[2] * 0.75, 1.0)
    return {
        "Skin": principled(f"{spec['name']}_Skin", skin, roughness=0.42, sss=0.42,
                           sss_radius=(1.0, 0.35, 0.2), specular=0.42),
        "SkinFace": make_skin_face_mat(f"{spec['name']}_SkinFace", skin, spec.get("face")),
        "SkinWarm": principled(f"{spec['name']}_SkinWarm", warm, roughness=0.4, sss=0.4,
                               sss_radius=(1.0, 0.25, 0.1)),
        "Lip": principled(f"{spec['name']}_Lip",
                          (min(1.0, skin[0] * 0.95), skin[1] * 0.45, skin[2] * 0.48, 1),
                          roughness=0.28, sss=0.62, sss_radius=(1.0, 0.15, 0.08), coat=0.28),
        "Nail": principled(f"{spec['name']}_Nail", (0.85, 0.75, 0.72, 1), roughness=0.25, coat=0.3),
        "EyeWhite": principled(f"{spec['name']}_EyeWhite", (0.92, 0.93, 0.94, 1), roughness=0.22, sss=0.15),
        "Iris": principled(f"{spec['name']}_Iris", spec["iris"], roughness=0.18, specular=0.85),
        "Pupil": principled(f"{spec['name']}_Pupil", (0.01, 0.01, 0.015, 1), roughness=0.35),
        "Cornea": principled(f"{spec['name']}_Cornea", (0.92, 0.94, 0.96, 1), roughness=0.08,
                             transmission=0.15, specular=0.85, coat=0.55, alpha=0.12),
        "Hair": principled(f"{spec['name']}_Hair", spec["hair"], roughness=0.28, specular=0.55, coat=0.18),
        "Brow": principled(f"{spec['name']}_Brow",
                           (min(0.18, spec["hair"][0] * 1.4 + 0.04),
                            min(0.12, spec["hair"][1] * 1.2 + 0.03),
                            min(0.10, spec["hair"][2] * 1.1 + 0.02), 1),
                           roughness=0.55),
        "Shirt": make_cloth_mat(f"{spec['name']}_Shirt", spec["shirt"], roughness=0.68, bump_scale=70, bump_str=0.10),
        "Pants": make_cloth_mat(f"{spec['name']}_Pants", spec["pants"], roughness=0.78, bump_scale=45, bump_str=0.14),
        "Jacket": make_cloth_mat(f"{spec['name']}_Jacket", spec["jacket_col"], roughness=0.48, bump_scale=35, bump_str=0.16),
        "Shoes": principled(f"{spec['name']}_Shoes", spec["shoes"], roughness=0.45, coat=0.12),
        "Teeth": principled(f"{spec['name']}_Teeth", (0.95, 0.93, 0.88, 1), roughness=0.22, sss=0.12),
        "Invisible": principled(f"{spec['name']}_Invisible", (0.85, 0.7, 0.6, 1), roughness=0.5, alpha=0.0),
    }


def assign_antonia_mats(obj, mats):
    slot_map = {
        "skinFace": "SkinFace", "skinBody": "Skin", "skinScalp": "Skin",
        "lips": "Lip", "brows": "Brow", "lashes": "Brow",
        "corneaLeft": "Cornea", "corneaRight": "Cornea",
        "irisLeft": "Iris", "irisRight": "Iris",
        "pupilLeft": "Pupil", "pupilRight": "Pupil",
        "scleraLeft": "EyeWhite", "scleraRight": "EyeWhite",
        "lacrimals": "SkinWarm", "nailsFingers": "Nail", "nailsToes": "Nail",
        "teeth": "Teeth", "tongue": "SkinWarm", "mouthInner": "SkinWarm",
        "toeCap": "Skin", "invisible": "Invisible",
    }
    for i, slot in enumerate(obj.material_slots):
        if not slot.material:
            continue
        base = slot.material.name.split(".")[0]
        target = slot_map.get(base) or slot_map.get(base.lower())
        if target and target in mats:
            obj.material_slots[i].material = mats[target]


def ground_object(obj):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    zs = [(obj.matrix_world @ v.co).z for v in obj.data.vertices]
    obj.location.z -= min(zs)
    bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)


def scale_to_height(obj, height, male=False):
    h = max(obj.dimensions.z, 1e-4)
    s = height / h
    if male:
        obj.scale = (s * 1.08, s * 1.04, s)
    else:
        obj.scale = (s, s, s)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)


def ensure_z_up(obj):
    # Antonia OBJ arrives Y-up flat in Z; rotate to Z-up
    if obj.dimensions.y > obj.dimensions.z * 1.25:
        obj.rotation_euler = Euler((math.radians(90.0), 0.0, 0.0), "XYZ")
        bpy.ops.object.select_all(action="DESELECT")
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)


def pose_offset(pose):
    table = {
        "idle_a": (math.radians(8), math.radians(2)),
        "idle_b": (math.radians(-6), math.radians(1)),
        "idle_c": (math.radians(14), math.radians(5)),
        "idle_d": (math.radians(-10), math.radians(-3)),
        "idle_e": (math.radians(18), math.radians(6)),
    }
    return table.get(pose, (0.0, 0.0))


def apply_natural_apose(body, pose_name="idle_a"):
    from collections import deque
    angle_table = {"idle_a": 95, "idle_b": 92, "idle_c": 98, "idle_d": 94, "idle_e": 96}
    asym = {"idle_a": (0, 4), "idle_b": (5, -3), "idle_c": (-3, 6), "idle_d": (4, 2), "idle_e": (6, -4)}
    elbow_deg = {"idle_a": 14, "idle_b": 20, "idle_c": 10, "idle_d": 16, "idle_e": 12}
    base_ang = angle_table.get(pose_name, 88)
    dL, dR = asym.get(pose_name, (0, 0))
    elbow = elbow_deg.get(pose_name, 14)

    h = max(v.co.z for v in body.data.vertices)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = body
    body.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(body.data)
    bm.verts.ensure_lookup_table()
    bm.edges.ensure_lookup_table()
    adj = {v.index: [] for v in bm.verts}
    for e in bm.edges:
        a, b = e.verts[0].index, e.verts[1].index
        adj[a].append(b)
        adj[b].append(a)

    def ry(p, pivot, ang):
        ca, sa = math.cos(ang), math.sin(ang)
        x, y, z = p.x - pivot.x, p.y - pivot.y, p.z - pivot.z
        return Vector((x * ca + z * sa + pivot.x, y + pivot.y, -x * sa + z * ca + pivot.z))

    def rx(p, pivot, ang):
        ca, sa = math.cos(ang), math.sin(ang)
        x, y, z = p.x - pivot.x, p.y - pivot.y, p.z - pivot.z
        return Vector((x + pivot.x, y * ca - z * sa + pivot.y, y * sa + z * ca + pivot.z))

    for sign, extra in ((1, dL), (-1, dR)):
        seeds = [v.index for v in bm.verts
                 if sign * v.co.x > 0.28 * h and 0.50 * h < v.co.z < 0.92 * h]
        if not seeds:
            best = max(bm.verts, key=lambda v: sign * v.co.x)
            seeds = [best.index]
        visited = set(seeds)
        q = deque(seeds)
        while q:
            i = q.popleft()
            for n in adj[i]:
                if n in visited:
                    continue
                nv = bm.verts[n]
                if sign * nv.co.x < 0.10 * h:
                    continue
                if nv.co.z < 0.35 * h or nv.co.z > 0.95 * h:
                    continue
                visited.add(n)
                q.append(n)
        sh = Vector((sign * 0.14 * h / 1.7, 0.0, 0.81 * h))
        ang = math.radians((base_ang + extra) * sign)
        for i in visited:
            v = bm.verts[i]
            v.co = ry(Vector(v.co), sh, ang)
        for i in visited:
            v = bm.verts[i]
            if v.co.z > 0.58 * h:
                continue
            el = Vector((v.co.x * 0.5 + sh.x * 0.5, 0.02 * sign, 0.60 * h))
            eb = math.radians(elbow * max(0.0, min(1.0, (0.58 * h - v.co.z) / (0.22 * h))))
            v.co = rx(Vector(v.co), el, eb)
    bmesh.update_edit_mesh(body.data)
    bpy.ops.object.mode_set(mode="OBJECT")
    print("APOSE", pose_name, "W", round(body.dimensions.x, 3))


def project_face_uv(body, face_path: Path):
    """Create UVMap_Face via manual front ortho projection (bg-safe, no View3D)."""
    if not face_path or not Path(face_path).exists():
        return
    face_ids = set()
    for i, slot in enumerate(body.material_slots):
        if slot.material and "skinface" in slot.material.name.lower():
            face_ids.add(i)
    if not face_ids:
        return

    me = body.data
    # Collect skinFace vertex world positions
    face_verts = set()
    for poly in me.polygons:
        if poly.material_index in face_ids:
            for vi in poly.vertices:
                face_verts.add(vi)
    if len(face_verts) < 10:
        return

    ws = {vi: body.matrix_world @ me.vertices[vi].co for vi in face_verts}
    xs = [p.x for p in ws.values()]
    zs = [p.z for p in ws.values()]
    xmin, xmax = min(xs), max(xs)
    zmin, zmax = min(zs), max(zs)
    # Pad slightly so portrait margins stay outside face
    pad_x = (xmax - xmin) * 0.08
    pad_z = (zmax - zmin) * 0.08
    xmin -= pad_x; xmax += pad_x
    zmin -= pad_z; zmax += pad_z
    dx = max(xmax - xmin, 1e-4)
    dz = max(zmax - zmin, 1e-4)

    if "UVMap_Face" not in me.uv_layers:
        me.uv_layers.new(name="UVMap_Face")
    uv_layer = me.uv_layers["UVMap_Face"]

    # Assign UVs per loop: u from X, v from Z (front ortho)
    for poly in me.polygons:
        for li in poly.loop_indices:
            vi = me.loops[li].vertex_index
            if vi in ws:
                p = ws[vi]
                u = (p.x - xmin) / dx
                v = (p.z - zmin) / dz
            else:
                # Non-face loops get out-of-range UV so mix falls to skin via edge fade
                u, v = -0.5, -0.5
            uv_layer.data[li].uv = (u, v)

    if "UVMap" in me.uv_layers:
        me.uv_layers["UVMap"].active = True
    print("FACE_UV projected", body.name, "n_face_verts", len(face_verts),
          "bbox", round(dx, 3), round(dz, 3))


def duplicate_body(body, name):
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.duplicate()
    dup = bpy.context.active_object
    dup.name = name
    return dup


def keep_height_band(obj, t_lo, t_hi, also_keep_fn=None, delete_fn=None):
    """Delete faces outside normalized height band [t_lo,t_hi]. Optional callbacks."""
    me = obj.data
    zs = [v.co.z for v in me.vertices]
    zmin, zmax = min(zs), max(zs)
    h = max(zmax - zmin, 1e-4)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(me)
    bm.faces.ensure_lookup_table()
    kill = []
    for f in bm.faces:
        z = sum(v.co.z for v in f.verts) / len(f.verts)
        t = (z - zmin) / h
        xabs = sum(abs(v.co.x) for v in f.verts) / len(f.verts)
        xmax = max(abs(v.co.x) for v in bm.verts) or 1.0
        keep = t_lo <= t <= t_hi
        if also_keep_fn and also_keep_fn(t, xabs, xmax):
            keep = True
        if delete_fn and delete_fn(t, xabs, xmax):
            keep = False
        if not keep:
            kill.append(f)
    if kill:
        bmesh.ops.delete(bm, geom=kill, context="FACES")
    bmesh.update_edit_mesh(me)
    bpy.ops.object.mode_set(mode="OBJECT")


def apply_solidify_displace(obj, thickness=0.012, displace=0.006, strength=0.55):
    solid = obj.modifiers.new("Solidify", "SOLIDIFY")
    solid.thickness = thickness
    solid.offset = 1.0
    solid.use_even_offset = True
    solid.use_quality_normals = True
    disp = obj.modifiers.new("Folds", "DISPLACE")
    tex = bpy.data.textures.new(f"{obj.name}_folds", type="CLOUDS")
    tex.noise_scale = 0.08
    tex.noise_depth = 2
    disp.texture = tex
    disp.strength = displace * strength
    disp.mid_level = 0.5
    # Apply modifiers
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    for mod in list(obj.modifiers):
        try:
            bpy.ops.object.modifier_apply(modifier=mod.name)
        except Exception as e:
            print("MOD_FAIL", mod.name, e)


def paint_under_clothes(body, mats, spec):
    """Paint body faces under garments so skin doesn't poke through gaps."""
    mesh = body.data
    for key in ("Pants", "Shirt", "Jacket", "Skin"):
        if mats[key].name not in [s.material.name if s.material else "" for s in body.material_slots]:
            body.data.materials.append(mats[key])
    name_to_idx = {}
    for i, slot in enumerate(body.material_slots):
        if slot.material:
            name_to_idx[slot.material.name] = i
    pants_i = name_to_idx.get(mats["Pants"].name)
    shirt_i = name_to_idx.get(mats["Shirt"].name)
    jacket_i = name_to_idx.get(mats["Jacket"].name) if spec.get("jacket") else None

    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.faces.ensure_lookup_table()
    zs = [v.co.z for v in bm.verts]
    zmin, zmax = min(zs), max(zs)
    h = max(zmax - zmin, 1e-4)
    xs = [abs(v.co.x) for v in bm.verts]
    xmax = max(xs) if xs else 1.0
    for f in bm.faces:
        z = sum(v.co.z for v in f.verts) / len(f.verts)
        xabs = sum(abs(v.co.x) for v in f.verts) / len(f.verts)
        t = (z - zmin) / h
        cur = f.material_index
        cur_name = body.material_slots[cur].material.name.lower() if body.material_slots[cur].material else ""
        if any(k in cur_name for k in ("face", "lip", "eye", "iris", "cornea", "pupil", "brow", "lash",
                                         "tooth", "teeth", "tongue", "nail", "scalp", "hair", "invisible")):
            continue
        # Skip arms
        if xabs > 0.22 * xmax and t > 0.42:
            continue
        if t < 0.06:
            continue
        elif t < 0.48:
            if pants_i is not None:
                f.material_index = pants_i
        elif t < 0.78:
            if jacket_i is not None and t > 0.52:
                f.material_index = jacket_i
            elif shirt_i is not None:
                f.material_index = shirt_i
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()


def make_garment_shells(body, mats, spec):
    """Paint-only wardrobe for C16 ship; constructed shells occluded faces."""
    return []


def make_shoes(body, mats, spec):
    objs = []
    male = spec.get("male", False)
    cx, cy = body.location.x, body.location.y
    for sx in ((-0.09, 0.09) if male else (-0.07, 0.07)):
        # Sole
        bpy.ops.mesh.primitive_cube_add(size=1, location=(cx + sx, cy + 0.04, 0.018))
        sole = bpy.context.active_object
        sole.scale = (0.055 if male else 0.045, 0.12 if male else 0.10, 0.018)
        bpy.ops.object.transform_apply(scale=True)
        # Bevel-ish via subdivide
        bpy.ops.object.modifier_add(type="BEVEL")
        sole.modifiers[-1].width = 0.008
        sole.modifiers[-1].segments = 2
        bpy.ops.object.modifier_apply(modifier=sole.modifiers[-1].name)
        apply_mat(sole, mats["Shoes"])
        shade_smooth(sole)
        objs.append(sole)
        # Upper
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=10, radius=1.0,
                                            location=(cx + sx, cy + 0.02, 0.045))
        up = bpy.context.active_object
        up.scale = (0.048 if male else 0.040, 0.09 if male else 0.075, 0.035)
        bpy.ops.object.transform_apply(scale=True)
        # Flatten bottom
        bm = bmesh.new()
        bm.from_mesh(up.data)
        for v in bm.verts:
            if v.co.z < -0.01:
                v.co.z = -0.01
        bm.to_mesh(up.data)
        bm.free()
        apply_mat(up, mats["Shoes"])
        shade_smooth(up)
        objs.append(up)
    return objs


def make_hair(body, mats, spec):
    """Tiny crown only — face-safe."""
    objs = []
    h = body.dimensions.z
    cx, cy = body.location.x, body.location.y
    male = spec.get("male", False)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=12, radius=1.0,
                                        location=(cx, cy - 0.08, h * 0.982))
    crown = bpy.context.active_object
    crown.scale = (0.082 if male else 0.086, 0.070, 0.028 if male else 0.032)
    bpy.ops.object.transform_apply(scale=True)
    bm = bmesh.new()
    bm.from_mesh(crown.data)
    zmax = max(v.co.z for v in bm.verts)
    kill = [v for v in bm.verts if v.co.y > -0.03 or v.co.z < zmax - 0.022]
    if kill:
        bmesh.ops.delete(bm, geom=kill, context="VERTS")
    bm.to_mesh(crown.data)
    bm.free()
    apply_mat(crown, mats["Hair"])
    shade_smooth(crown)
    objs.append(crown)
    return objs


def delete_cornea_faces(body):
    """Remove cornea shells that ghost double-eyes in Cycles."""
    kill_ids = set()
    for i, slot in enumerate(body.material_slots):
        if slot.material and "cornea" in slot.material.name.lower():
            kill_ids.add(i)
    if not kill_ids:
        return
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = body
    body.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(body.data)
    kill = [f for f in bm.faces if f.material_index in kill_ids]
    if kill:
        bmesh.ops.delete(bm, geom=kill, context="FACES")
    bmesh.update_edit_mesh(body.data)
    bpy.ops.object.mode_set(mode="OBJECT")
    print("DELETED_CORNEA", len(kill) if kill else 0)


def import_antonia(spec, mats):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=str(ANTONIA))
    new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    body = new[0]
    body.name = f"Body_{spec['name']}"
    assign_antonia_mats(body, mats)
    delete_cornea_faces(body)
    shade_smooth(body)
    ensure_z_up(body)
    scale_to_height(body, spec["height"], male=spec.get("male", False))
    ground_object(body)
    apply_natural_apose(body, spec.get("pose", "idle_a"))
    ground_object(body)
    pass  # project_face_uv disabled (portrait ghosts)
    paint_under_clothes(body, mats, spec)
    shells = make_garment_shells(body, mats, spec)
    shoes = make_shoes(body, mats, spec)
    hair = make_hair(body, mats, spec)
    extras = shells + shoes + hair
    yaw, lean = pose_offset(spec["pose"])
    body.rotation_euler.z = yaw
    body.rotation_euler.x = lean * 0.12
    for o in extras:
        o.rotation_euler.z = yaw
        o.rotation_euler.x = lean * 0.12
    return [body] + extras


def join_export(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    live = []
    for o in objs:
        if o and o.name in bpy.data.objects:
            bpy.context.view_layer.objects.active = o
            o.select_set(True)
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
            live.append(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in live:
        o.select_set(True)
    bpy.context.view_layer.objects.active = live[0]
    if len(live) > 1:
        bpy.ops.object.join()
    root = bpy.context.view_layer.objects.active
    root.name = f"hm_ped_{name}"
    return root


def render_hero(obj, name):
    for o in list(bpy.data.objects):
        if o.type == "LIGHT":
            bpy.data.objects.remove(o, do_unlink=True)
    mins = [1e9] * 3
    maxs = [-1e9] * 3
    for c in obj.bound_box:
        w = obj.matrix_world @ Vector(c)
        for i in range(3):
            mins[i] = min(mins[i], w[i])
            maxs[i] = max(maxs[i], w[i])
    center = Vector(((mins[0] + maxs[0]) / 2, (mins[1] + maxs[1]) / 2, (mins[2] + maxs[2]) / 2))
    size = Vector((maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]))
    print("RENDER_BOUNDS", name, tuple(round(x, 3) for x in size), "center", tuple(round(x, 3) for x in center))

    bpy.ops.object.light_add(type="AREA", location=(center.x + 1.4, center.y - 1.8, center.z + 0.6))
    L = bpy.context.active_object
    L.data.energy = 380
    L.data.size = 1.6
    bpy.ops.object.light_add(type="AREA", location=(center.x - 1.0, center.y + 0.8, center.z + 0.4))
    L2 = bpy.context.active_object
    L2.data.energy = 140
    L2.data.size = 1.1
    L2.data.color = (0.75, 0.82, 1.0)
    bpy.ops.object.light_add(type="AREA", location=(center.x, center.y - 0.5, center.z + 1.8))
    L3 = bpy.context.active_object
    L3.data.energy = 90
    L3.data.size = 2.0

    world = bpy.data.worlds.new(f"W_{name}")
    bpy.context.scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.22, 0.22, 0.24, 1)
    bg.inputs[1].default_value = 0.55

    dist = max(size.z * 1.35, 2.2)
    bpy.ops.object.camera_add(location=(center.x + 0.15, center.y - dist, center.z + size.z * 0.02))
    cam = bpy.context.active_object
    cam.rotation_euler = (center - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 55
    bpy.context.scene.camera = cam
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 64
    sc.cycles.use_denoising = True
    sc.render.resolution_x = 720
    sc.render.resolution_y = 1024
    sc.view_settings.view_transform = "Filmic"
    sc.view_settings.look = "Medium High Contrast"
    sc.render.filepath = str(REND_PEDS / f"hm_ped_{name}_full.png")
    bpy.ops.render.render(write_still=True)

    face_center = None
    me = obj.data
    face_mat_ids = set()
    for i, slot in enumerate(obj.material_slots):
        if slot.material and any(k in slot.material.name.lower()
                                 for k in ("skinface", "lip", "iris", "cornea", "eyewhite", "brow")):
            face_mat_ids.add(i)
    if face_mat_ids and me.polygons:
        acc = Vector((0, 0, 0)); n = 0
        for poly in me.polygons:
            if poly.material_index in face_mat_ids:
                for vi in poly.vertices:
                    acc += obj.matrix_world @ me.vertices[vi].co
                    n += 1
        if n > 10:
            face_center = acc / n
    if face_center is None:
        face_center = Vector((center.x, center.y, mins[2] + size.z * 0.88))
    print("FACE_CENTER", tuple(round(x, 3) for x in face_center))
    bpy.ops.object.camera_add(location=(face_center.x, face_center.y - 0.55, face_center.z + 0.01))
    cam2 = bpy.context.active_object
    cam2.rotation_euler = (face_center - cam2.location).to_track_quat("-Z", "Y").to_euler()
    cam2.data.lens = 85
    bpy.context.scene.camera = cam2
    sc.render.resolution_x = 720
    sc.render.resolution_y = 720
    sc.render.filepath = str(REND_PEDS / f"hm_ped_{name}_face.png")
    bpy.ops.render.render(write_still=True)
    for tag in ("full", "face"):
        src = REND_PEDS / f"hm_ped_{name}_{tag}.png"
        if src.exists():
            shutil.copy(src, ART_PEDS / src.name)


def export_ped(obj, name):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    blend = OUT / f"hm_ped_{name}_v16.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    glb = OUT / f"hm_ped_{name}_v16.glb"
    bpy.ops.export_scene.gltf(filepath=str(glb), use_selection=True, export_format="GLB",
                              export_apply=True)
    obj_path = OUT / f"hm_ped_{name}_v16.obj"
    bpy.ops.wm.obj_export(filepath=str(obj_path), export_selected_objects=True,
                          forward_axis="NEGATIVE_Z", up_axis="Y")
    for ext in (".obj", ".mtl"):
        src = OUT / f"hm_ped_{name}_v16{ext}"
        if src.exists():
            shutil.copy(src, FURY_PEDS / f"hm_ped_{name}{ext}")
            shutil.copy(src, FURY_PEDS / f"hm_ped_{name}_v16{ext}")
    if glb.exists():
        shutil.copy(glb, FURY_PEDS / f"hm_ped_{name}_v16.glb")
        shutil.copy(glb, FURY_PEDS / f"hm_ped_{name}.glb")
    print("EXPORTED", name, "verts", len(obj.data.vertices), "glb", glb.stat().st_size if glb.exists() else 0)


def build_one(spec):
    clear_scene()
    mats = make_mats(spec)
    objs = import_antonia(spec, mats)
    root = join_export(objs, spec["name"])
    export_ped(root, spec["name"])
    render_hero(root, spec["name"])


def main():
    only = os.environ.get("PED_ONLY")
    specs = [s for s in PEDS if (not only or s["name"] == only)]
    for spec in specs:
        print("=" * 60, "BUILD", spec["name"])
        try:
            build_one(spec)
        except Exception as e:
            traceback.print_exc()
            print("FAIL", spec["name"], e)
    (ART_PEDS / "README.md").write_text(
        "# Harbor Metro peds v16 — Antonia.Polygon CC0 + garment shells\n\n"
        "Licenses:\n"
        "- Antonia.Polygon mesh: CC0 (see refs/ped_bases/antonia/LICENSE)\n"
        "- Face albedos: original Harbor Metro portraits (project-owned)\n"
        "- Wardrobe: original solidify/displace garment shells authored in Blender\n"
        "- Hair: Blender particle hair converted to mesh + style volumes\n\n"
        "Natural idle arms-down. GLB primary for beauty. No Rockstar/GTA IP.\n"
    )
    print("DONE peds v16")


if __name__ == "__main__":
    main()
