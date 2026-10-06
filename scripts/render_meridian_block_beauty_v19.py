#!/usr/bin/env python3
"""Cycle-19 PRIMARY ChatGPT evidence: Blender Cycles beauty — Meridian Mutual block.

C18 judged 6.1 NO-SHIP, Gate A (asphalt) FAIL: smooth uniform material. Cycle-19 = P0 asphalt rebuild
(build_road_c19 / make_road_material_c19 + scripts/gen_road_masks_v19.py), parked cars, lamp poles off
the carriageway, 03 reframe + native wheel close-up, native day/night asphalt close-ups.
Peds (C18 MPFB2 pipeline) untouched.

--- cycle-18 notes ---

C17 judged 4.4 NO-SHIP: Gate C (peds) regressed with spiky garment-shell / hair-card
artifacts at beauty distance. Cycle-18 changes ONLY the ped pipeline + 04 hero:
1. Peds P0 — hand-built Antonia shells/cards removed entirely. Peds rebuilt from the
   MakeHuman CC0 system assets via MPFB2 (scripts/build_harbor_peds_v18.py): fitted
   textured garments, alpha-textured hair meshes, ENHANCED_SSS skin, procedural eyes,
   default rig + authored idle poses. Appended from packed .blend (full shader fidelity).
2. 04 hero reframed: small group on a new south-side pavement in front of the
   Meridian Mutual stone facade (no more void background); dedicated key/rim/fill.
3. Asphalt, wheels, architecture, lobby, night unchanged from C17 (gates A/B/D/E).

Harbor Metro / HMPD / Meridian Mutual ONLY — no Rockstar/GTA IP.
"""
from __future__ import annotations

import math
import os
import sys
from pathlib import Path

import bpy
from mathutils import Vector, Euler

ROOT = Path("/workspace/vaultline-blender")
sys.path.insert(0, str(ROOT / "scripts"))
from vl_common import clear_scene  # noqa: E402

MESH = Path("/workspace/Fury/assets/meshes/harbor_metro")
PEDS = MESH / "peds"
OUT = Path(os.environ.get("BEAUTY_OUT", "/workspace/Fury/artifacts/aaa_meridian_block/blender_scene_beauty"))
OUT.mkdir(parents=True, exist_ok=True)
TEX = ROOT / "refs/textures/polyhaven"
WHEEL_GLB = ROOT / "refs/wheels/kenney_police_wheels.glb"


def fury_to_b(x, y, z):
    return Vector((x, -z, y))


def shade_smooth(obj):
    try:
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.shade_smooth()
    except Exception:
        pass


def import_obj(path: Path):
    if not path.exists():
        print("MISSING", path)
        return []
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis="NEGATIVE_Z", up_axis="Y")
    return [o for o in bpy.data.objects if o not in before]


def get_bsdf(mat):
    if mat is None:
        return None
    if not mat.use_nodes:
        mat.use_nodes = True
    for n in mat.node_tree.nodes:
        if n.type == "BSDF_PRINCIPLED":
            return n
    return None


def load_img(path: Path):
    path = Path(path)
    if not path.exists():
        return None
    img = bpy.data.images.load(str(path), check_existing=True)
    img.colorspace_settings.name = "sRGB" if "diff" in path.name else "Non-Color"
    return img


def make_ph_mat(name, folder, scale=4.0, roughness_boost=0.0, darken=0.0):
    """Poly Haven CC0 PBR material (diff/nor/rough)."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nodes, links = nt.nodes, nt.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    tex = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (scale, scale, scale)
    links.new(tex.outputs["UV"], mapping.inputs["Vector"])

    base = TEX / folder
    diff = load_img(base / f"{folder}_diff_2k.jpg")
    nor = load_img(base / f"{folder}_nor_gl_2k.jpg")
    rough = load_img(base / f"{folder}_rough_2k.jpg")

    if diff:
        n = nodes.new("ShaderNodeTexImage")
        n.image = diff
        links.new(mapping.outputs["Vector"], n.inputs["Vector"])
        if darken > 0.01:
            mix = nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.inputs["Factor"].default_value = darken
            dark = nodes.new("ShaderNodeRGB")
            dark.outputs[0].default_value = (0.02, 0.02, 0.02, 1)
            links.new(n.outputs["Color"], mix.inputs["A"])
            links.new(dark.outputs[0], mix.inputs["B"])
            links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
        else:
            links.new(n.outputs["Color"], bsdf.inputs["Base Color"])
    else:
        bsdf.inputs["Base Color"].default_value = (0.35, 0.32, 0.28, 1)

    if rough:
        n = nodes.new("ShaderNodeTexImage")
        n.image = rough
        n.image.colorspace_settings.name = "Non-Color"
        links.new(mapping.outputs["Vector"], n.inputs["Vector"])
        if roughness_boost > 0.01:
            add = nodes.new("ShaderNodeMath")
            add.operation = "ADD"
            add.inputs[1].default_value = roughness_boost
            links.new(n.outputs["Color"], add.inputs[0])
            clamp = nodes.new("ShaderNodeMath")
            clamp.operation = "MINIMUM"
            clamp.inputs[1].default_value = 0.95
            links.new(add.outputs[0], clamp.inputs[0])
            links.new(clamp.outputs[0], bsdf.inputs["Roughness"])
        else:
            links.new(n.outputs["Color"], bsdf.inputs["Roughness"])
    else:
        bsdf.inputs["Roughness"].default_value = 0.65

    if nor:
        n = nodes.new("ShaderNodeTexImage")
        n.image = nor
        n.image.colorspace_settings.name = "Non-Color"
        links.new(mapping.outputs["Vector"], n.inputs["Vector"])
        nrm = nodes.new("ShaderNodeNormalMap")
        nrm.inputs["Strength"].default_value = 0.85
        links.new(n.outputs["Color"], nrm.inputs["Color"])
        links.new(nrm.outputs["Normal"], bsdf.inputs["Normal"])

    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def make_glass_mat(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = get_bsdf(mat)
    bsdf.inputs["Base Color"].default_value = (0.12, 0.18, 0.24, 1)
    bsdf.inputs["Roughness"].default_value = 0.05
    bsdf.inputs["Metallic"].default_value = 0.0
    if "Transmission Weight" in bsdf.inputs:
        bsdf.inputs["Transmission Weight"].default_value = 0.92
    if "IOR" in bsdf.inputs:
        bsdf.inputs["IOR"].default_value = 1.52
    try:
        mat.blend_method = "HASHED"
    except Exception:
        pass
    return mat


def make_metal_mat(name, col=(0.1, 0.1, 0.11, 1), rough=0.3):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = get_bsdf(mat)
    bsdf.inputs["Base Color"].default_value = col
    bsdf.inputs["Metallic"].default_value = 1.0
    bsdf.inputs["Roughness"].default_value = rough
    return mat


def ensure_uv(obj, scale=1.0):
    if obj.type != "MESH":
        return
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    if not obj.data.uv_layers:
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        try:
            bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
        except Exception:
            bpy.ops.uv.unwrap(method="ANGLE_BASED")
        bpy.ops.object.mode_set(mode="OBJECT")
    # scale UV via material mapping; also object-level
    obj.select_set(False)


def hide_junk():
    for o in list(bpy.data.objects):
        n = o.name.lower()
        if n.startswith("hm_ped_"):
            continue
        if any(k in n for k in (
            "lod1", "cage", "proxy", "helper", "collision", "wire",
            "sensor", "lidar", "rig_pole", "calib", "occ", "guide_",
            "debug_", "measure_", "float_", "v11_grime", "v12_chassis",
            "v12_ub_", "v12_well_", "v11_roadgrime", "dirt_brakedust",
            "v12_grimeglass", "v12_susp", "facadecard", "midglass",
            "envband", "banner", "awning", "cable", "antenna",
            "v11_cage", "steer_", "mdt", "interior", "cabin",
            "c13_facade", "c13_winglass", "c13_frame", "c13_midglass",
            "c13_midframe", "c13_heropanel", "c13_heroframe",
            "_room", "_ash_", "ashrev", "_rev", "_pane", "bunting",
        )):
            o.hide_render = True
            o.hide_viewport = True
        # Hard-delete ash/rev/pane façade cards and thin floating panels.
        if o.type == "MESH" and any(k in n for k in ("_ash_", "ashrev", "_rev", "_pane", "_room", "_fw_", "facadecard", "reveal", "cheek", "conduit", "dent_")):
            if not any(k in n for k in ("c14_", "lobby", "glassdoor")):
                print("DELETE_FAÇADE", o.name)
                bpy.data.objects.remove(o, do_unlink=True)
                continue
        if o.type == "MESH":
            d = o.dimensions
            sorted_d = sorted([d.x, d.y, d.z])
            if sorted_d[0] < 0.35 and sorted_d[1] > 0.9 and sorted_d[2] > 1.2:
                if abs(o.location.y) > 0.4 or o.location.z > 0.8:
                    if not any(k in n for k in ("c14_", "wetasphalt", "lobby", "sign", "lamp", "desk", "chair", "glassdoor", "col_", "base_", "cap_", "cheek")):
                        print("DELETE_FLOAT_CARD", o.name, tuple(round(x, 2) for x in d))
                        bpy.data.objects.remove(o, do_unlink=True)


def hide_imported_grounds():
    for o in list(bpy.data.objects):
        n = o.name.lower()
        if o.type != "MESH":
            continue
        if any(k in n for k in ("sidewalk", "asphalt", "road", "ground", "plaza", "pavement", "curb")):
            if "wetasphalt" in n or "c14_" in n or "c15_" in n or "c16_" in n or "authored" in n:
                continue
            o.hide_render = True
            o.hide_viewport = True
        dims = o.dimensions
        if dims.x > 8 and dims.y > 8 and dims.z < 0.4 and abs(o.location.z) < 0.5:
            if "wetasphalt" not in n and "c14_" not in n and "lane" not in n:
                if any(k in n for k in ("plane", "floor", "slab", "hm_", "concrete")):
                    o.hide_render = True
                    o.hide_viewport = True



def make_wet_asphalt_ph():
    """Matte aggregate asphalt + VISIBLE shallow puddle islands (mesh).
    Answers: looks like real wet asphalt? Keep dry matte; wet only localized.
    """
    bpy.ops.mesh.primitive_plane_add(size=1, location=(18, -12, 0.012))
    ground = bpy.context.active_object
    ground.name = "C16_RoadAsphalt"
    ground.scale = (110, 80, 1)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.subdivide(number_cuts=24)
    bpy.ops.uv.reset()
    bpy.ops.object.mode_set(mode="OBJECT")
    ensure_uv(ground)

    mat = bpy.data.materials.new("C16_RoadAsphalt")
    mat.use_nodes = True
    nt = mat.node_tree
    nodes, links = nt.nodes, nt.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Metallic"].default_value = 0.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.32
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.32

    tex = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    # Dense tile so 4k aggregate grain reads at hero crop
    mapping.inputs["Scale"].default_value = (48.0, 38.0, 1.0)
    links.new(tex.outputs["Object"], mapping.inputs["Vector"])

    folder = "asphalt_02"
    # Prefer 4k maps when present
    def _pick(stem):
        for res in ("4k", "2k"):
            for ext in ("jpg", "png"):
                p = TEX / folder / f"{folder}_{stem}_{res}.{ext}"
                if p.exists():
                    return load_img(p)
        return None

    diff = _pick("diff")
    nor = _pick("nor_gl")
    rough_img = _pick("rough")
    disp = _pick("disp")

    diff_n = nodes.new("ShaderNodeTexImage")
    diff_n.image = diff
    links.new(mapping.outputs["Vector"], diff_n.inputs["Vector"])
    # Mild darken only — keep aggregate readable
    darken = nodes.new("ShaderNodeMix")
    darken.data_type = "RGBA"
    darken.inputs["Factor"].default_value = 0.28
    dark_col = nodes.new("ShaderNodeRGB")
    dark_col.outputs[0].default_value = (0.04, 0.04, 0.042, 1)
    links.new(diff_n.outputs["Color"], darken.inputs["A"])
    links.new(dark_col.outputs[0], darken.inputs["B"])
    # Multi-scale tonal breakup (patches / tire polish)
    col_noise = nodes.new("ShaderNodeTexNoise")
    col_noise.inputs["Scale"].default_value = 2.2
    col_noise.inputs["Detail"].default_value = 8.0
    col_noise.inputs["Roughness"].default_value = 0.55
    links.new(tex.outputs["Object"], col_noise.inputs["Vector"])
    col_mul = nodes.new("ShaderNodeMix")
    col_mul.data_type = "RGBA"
    grey = nodes.new("ShaderNodeRGB")
    grey.outputs[0].default_value = (0.06, 0.06, 0.062, 1)
    links.new(darken.outputs["Result"], col_mul.inputs["A"])
    links.new(grey.outputs[0], col_mul.inputs["B"])
    cr = nodes.new("ShaderNodeMapRange")
    cr.inputs["From Min"].default_value = 0.30
    cr.inputs["From Max"].default_value = 0.70
    cr.inputs["To Min"].default_value = 0.0
    cr.inputs["To Max"].default_value = 0.28
    links.new(col_noise.outputs["Fac"], cr.inputs["Value"])
    links.new(cr.outputs["Result"], col_mul.inputs["Factor"])
    links.new(col_mul.outputs["Result"], bsdf.inputs["Base Color"])

    # Dry roughness high; keep base matte
    rough_tex = nodes.new("ShaderNodeTexImage")
    rough_tex.image = rough_img
    if rough_img:
        rough_img.colorspace_settings.name = "Non-Color"
    links.new(mapping.outputs["Vector"], rough_tex.inputs["Vector"])
    dry_boost = nodes.new("ShaderNodeMath")
    dry_boost.operation = "ADD"
    dry_boost.inputs[1].default_value = 0.22
    links.new(rough_tex.outputs["Color"], dry_boost.inputs[0])
    dry_clamp = nodes.new("ShaderNodeMath")
    dry_clamp.operation = "MINIMUM"
    dry_clamp.inputs[1].default_value = 0.96
    links.new(dry_boost.outputs[0], dry_clamp.inputs[0])
    dry_max = nodes.new("ShaderNodeMath")
    dry_max.operation = "MAXIMUM"
    dry_max.inputs[1].default_value = 0.72
    links.new(dry_clamp.outputs[0], dry_max.inputs[0])
    links.new(dry_max.outputs[0], bsdf.inputs["Roughness"])

    if nor:
        nor_n = nodes.new("ShaderNodeTexImage")
        nor_n.image = nor
        nor.colorspace_settings.name = "Non-Color"
        links.new(mapping.outputs["Vector"], nor_n.inputs["Vector"])
        # Extra micro-noise layered into normal for aggregate sparkle
        micro = nodes.new("ShaderNodeTexNoise")
        micro.inputs["Scale"].default_value = 85.0
        micro.inputs["Detail"].default_value = 12.0
        micro.inputs["Roughness"].default_value = 0.7
        links.new(tex.outputs["Object"], micro.inputs["Vector"])
        nrm = nodes.new("ShaderNodeNormalMap")
        nrm.inputs["Strength"].default_value = 2.15
        links.new(nor_n.outputs["Color"], nrm.inputs["Color"])
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.35
        bump.inputs["Distance"].default_value = 0.015
        links.new(micro.outputs["Fac"], bump.inputs["Height"])
        links.new(nrm.outputs["Normal"], bump.inputs["Normal"])
        links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    if disp:
        disp_n = nodes.new("ShaderNodeTexImage")
        disp_n.image = disp
        disp.colorspace_settings.name = "Non-Color"
        links.new(mapping.outputs["Vector"], disp_n.inputs["Vector"])
        # Bump from displacement as additional micro-relief
        bump2 = nodes.new("ShaderNodeBump")
        bump2.inputs["Strength"].default_value = 0.45
        bump2.inputs["Distance"].default_value = 0.02
        links.new(disp_n.outputs["Color"], bump2.inputs["Height"])
        if nor:
            links.new(bump.outputs["Normal"], bump2.inputs["Normal"])
            links.new(bump2.outputs["Normal"], bsdf.inputs["Normal"])
        else:
            links.new(bump2.outputs["Normal"], bsdf.inputs["Normal"])

    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    ground.data.materials.append(mat)

    # Sidewalk
    bpy.ops.mesh.primitive_cube_add(size=1, location=(18, 4.5, 0.08))
    walk = bpy.context.active_object
    walk.scale = (70, 6.0, 0.12)
    bpy.ops.object.transform_apply(scale=True)
    walk.name = "C16_Sidewalk"
    ensure_uv(walk)
    wmat = make_ph_mat("C16_Sidewalk", "concrete_floor_worn_001" if (TEX / "concrete_floor_worn_001" / "concrete_floor_worn_001_diff_2k.jpg").exists() else "concrete_wall_008", scale=6.0, roughness_boost=0.15)
    walk.data.materials.append(wmat)

    # Lane markings
    for i, x in enumerate((6.0, 16.0, 26.0, 36.0)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, -10.0, 0.035))
        mark = bpy.context.active_object
        mark.scale = (2.2, 0.12, 0.008)
        bpy.ops.object.transform_apply(scale=True)
        mark.name = f"C16_Lane_{i}"
        mm = bpy.data.materials.new(f"C16_Lane_{i}")
        mm.use_nodes = True
        get_bsdf(mm).inputs["Base Color"].default_value = (0.72, 0.68, 0.38, 1)
        get_bsdf(mm).inputs["Roughness"].default_value = 0.72
        mark.data.materials.append(mm)

    # --- VISIBLE shallow puddle meshes (localized only) ---
    wet = bpy.data.materials.new("C16_PuddleWet")
    wet.use_nodes = True
    wbsdf = get_bsdf(wet)
    wbsdf.inputs["Base Color"].default_value = (0.04, 0.045, 0.05, 1)
    wbsdf.inputs["Roughness"].default_value = 0.12
    wbsdf.inputs["Metallic"].default_value = 0.0
    if "Coat Weight" in wbsdf.inputs:
        wbsdf.inputs["Coat Weight"].default_value = 0.55
        wbsdf.inputs["Coat Roughness"].default_value = 0.08
    if "Specular IOR Level" in wbsdf.inputs:
        wbsdf.inputs["Specular IOR Level"].default_value = 0.65
    edge = bpy.data.materials.new("C16_PuddleEdge")
    edge.use_nodes = True
    ebsdf = get_bsdf(edge)
    ebsdf.inputs["Base Color"].default_value = (0.05, 0.05, 0.055, 1)
    ebsdf.inputs["Roughness"].default_value = 0.35
    if "Coat Weight" in ebsdf.inputs:
        ebsdf.inputs["Coat Weight"].default_value = 0.25
        ebsdf.inputs["Coat Roughness"].default_value = 0.18

    puddles = [
        (8.5, -9.5, 1.8, 1.1),
        (14.0, -11.2, 2.4, 1.4),
        (19.5, -8.8, 1.5, 0.9),
        (24.0, -12.5, 2.0, 1.2),
        (11.0, -13.0, 1.2, 0.8),
        (28.5, -9.0, 1.6, 1.0),
        (16.5, -14.5, 1.9, 1.1),
    ]
    for i, (x, y, sx, sy) in enumerate(puddles):
        bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=1.0, depth=0.008,
                                           location=(x, y, 0.018))
        pud = bpy.context.active_object
        pud.scale = (sx, sy, 1.0)
        bpy.ops.object.transform_apply(scale=True)
        pud.name = f"C16_Puddle_{i}"
        pud.data.materials.append(wet)
        # Soft edge ring
        bpy.ops.mesh.primitive_torus_add(major_radius=0.92, minor_radius=0.08,
                                        location=(x, y, 0.016))
        ring = bpy.context.active_object
        ring.scale = (sx, sy, 0.4)
        bpy.ops.object.transform_apply(scale=True)
        ring.name = f"C16_PuddleEdge_{i}"
        ring.data.materials.append(edge)

    print("C16_ASPHALT_BUILT 4k-aggregate + micro-normal + localized puddle meshes")



# ============================================================================
# Cycle-19 P0 ASPHALT — world-space authored masks + layered Poly Haven CC0 stack
# ============================================================================
ROADGEN = ROOT / "refs/textures/generated/c19_road"
RX0, RX1, RY0, RY1 = -15.0, 65.0, -27.0, 2.0
RYS, RYN, RYC = -22.0, -9.0, -15.5
RXW, RXE = 9.5, 17.6
PAV_TOP = 0.12


def _crown_at(x, y):
    """Road crown height (m) at Blender (x, y) — matches gen_road_masks_v19.py."""
    import numpy as np
    c = _crown_at.grid
    if c is None:
        c = _crown_at.grid = np.load(str(ROADGEN / "c19_crown_0p2m.npy"))
    j = (x - RX0) / 0.2
    i = (RY1 - y) / 0.2
    i0, j0 = int(max(0, min(c.shape[0] - 2, math.floor(i)))), int(max(0, min(c.shape[1] - 2, math.floor(j))))
    fi, fj = min(1, max(0, i - i0)), min(1, max(0, j - j0))
    v = (c[i0, j0] * (1 - fi) * (1 - fj) + c[i0 + 1, j0] * fi * (1 - fj) +
         c[i0, j0 + 1] * (1 - fi) * fj + c[i0 + 1, j0 + 1] * fi * fj)
    return float(v)


_crown_at.grid = None


def _img_node(nodes, path, interp="Linear", ext="CLIP", color=False):
    n = nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.load(str(path), check_existing=True)
    img.colorspace_settings.name = "sRGB" if color else "Non-Color"
    n.image = img
    n.interpolation = interp
    n.extension = ext
    return n


def _ph(folder, stem):
    for res in ("4k", "2k"):
        p = TEX / folder / f"{folder}_{stem}_{res}.jpg"
        if p.exists():
            return p
    return None


def make_road_material_c19(night=False):
    mat = bpy.data.materials.new("c19_RoadAsphalt")
    mat.use_nodes = True
    nt = mat.node_tree
    N, L = nt.nodes, nt.links
    N.clear()
    out = N.new("ShaderNodeOutputMaterial")
    bsdf = N.new("ShaderNodeBsdfPrincipled")
    L.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    tc = N.new("ShaderNodeTexCoord")
    geo = N.new("ShaderNodeNewGeometry")

    def math_(op, a=None, b=None, clamp=False):
        m = N.new("ShaderNodeMath")
        m.operation = op
        m.use_clamp = clamp
        for idx, v in enumerate((a, b)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                m.inputs[idx].default_value = v
            else:
                L.new(v, m.inputs[idx])
        return m.outputs[0]

    def mixc(fac, a, b, blend="MIX"):
        m = N.new("ShaderNodeMix")
        m.data_type = "RGBA"
        m.blend_type = blend
        m.clamp_result = True
        if isinstance(fac, (int, float)):
            m.inputs["Factor"].default_value = fac
        else:
            L.new(fac, m.inputs["Factor"])
        for sock, v in ((m.inputs[6], a), (m.inputs[7], b)):
            if isinstance(v, tuple):
                sock.default_value = v
            else:
                L.new(v, sock)
        return m.outputs[2]

    def mixf(fac, a, b):
        m = N.new("ShaderNodeMix")
        m.data_type = "FLOAT"
        m.clamp_factor = True
        for sock, v in ((m.inputs["Factor"], fac), (m.inputs[2], a), (m.inputs[3], b)):
            if isinstance(v, (int, float)):
                sock.default_value = v
            else:
                L.new(v, sock)
        return m.outputs[0]

    def sstep(x, e0, e1):
        mr = N.new("ShaderNodeMapRange")
        mr.interpolation_type = "SMOOTHSTEP"
        mr.inputs["From Min"].default_value = e0
        mr.inputs["From Max"].default_value = e1
        L.new(x, mr.inputs["Value"])
        return mr.outputs["Result"]

    def mapping(scale, rot=0.0, loc=(0, 0, 0)):
        mp = N.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = scale
        mp.inputs["Rotation"].default_value = (0, 0, rot)
        mp.inputs["Location"].default_value = loc
        L.new(tc.outputs["Object"], mp.inputs["Vector"])
        return mp.outputs["Vector"]

    def noise(scale, detail=4.0, rough=0.55, seed_off=(0, 0, 0)):
        nz = N.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = scale
        nz.inputs["Detail"].default_value = detail
        nz.inputs["Roughness"].default_value = rough
        mp = mapping((1, 1, 1), 0.0, seed_off)
        L.new(mp, nz.inputs["Vector"])
        return nz.outputs["Fac"]

    # ---- authored world-space masks ----
    muv = mapping((1.0 / (RX1 - RX0), 1.0 / (RY1 - RY0), 1.0), 0.0,
                  (-RX0 / (RX1 - RX0), -RY0 / (RY1 - RY0), 0.0))
    H_ = _img_node(N, ROADGEN / "c19_road_height.png")
    m1 = _img_node(N, ROADGEN / "c19_road_m1.png")
    m2 = _img_node(N, ROADGEN / "c19_road_m2.png")
    m3 = _img_node(N, ROADGEN / "c19_road_m3.png")
    m4 = _img_node(N, ROADGEN / "c19_road_m4.png")
    for n in (H_, m1, m2, m3, m4):
        L.new(muv, n.inputs["Vector"])

    def sep(n):
        s = N.new("ShaderNodeSeparateColor")
        L.new(n.outputs["Color"], s.inputs["Color"])
        return s.outputs[0], s.outputs[1], s.outputs[2]

    wet_lvl, patch_id, seal = sep(m1)
    paint_w, paint_y, crack = sep(m2)
    wear, oil, grime = sep(m3)
    gutter, man, drain = sep(m4)
    # outside the authored rect -> base damp level (CLIP gives 0)
    wet_lvl = math_("MAXIMUM", wet_lvl, 0.18)

    # ---- Poly Haven CC0 layers (true-to-scale tiling) ----
    uvA = mapping((1 / 3.0, 1 / 3.0, 1))                              # asphalt_02, 3 m
    uvA2 = mapping((1 / 3.0, 1 / 3.0, 1), 1.9, (0.37, 0.61, 0))      # rotated/offset anti-tile copy
    uvB = mapping((1 / 4.04, 1 / 4.04, 1), 0.7, (0.13, 0.42, 0))     # asphalt_04, 4 m
    uvP = mapping((1 / 2.0, 1 / 2.0, 1), 0.3)                         # asphalt_track (fresh patch), 2 m
    uvG = mapping((1 / 2.0, 1 / 2.0, 1))                              # concrete gutter

    def ph_set(folder, uv, with_disp=False):
        d = _img_node(N, _ph(folder, "diff"), ext="REPEAT", color=True)
        r = _img_node(N, _ph(folder, "rough"), ext="REPEAT")
        nn = _img_node(N, _ph(folder, "nor_gl"), ext="REPEAT")
        outs = [d, r, nn]
        if with_disp and _ph(folder, "disp"):
            outs.append(_img_node(N, _ph(folder, "disp"), ext="REPEAT"))
        for n in outs:
            L.new(uv, n.inputs["Vector"])
        return outs

    A = ph_set("asphalt_02", uvA, True)
    A2 = ph_set("asphalt_02", uvA2, True)
    B = ph_set("asphalt_04", uvB)
    P = ph_set("asphalt_track", uvP)
    G = ph_set("concrete_floor_worn_02", uvG)

    n_tile = sstep(noise(0.22, 3.0, 0.5, (11, 3, 0)), 0.42, 0.58)       # A/A2 anti-tile blend
    n_B = sstep(noise(0.07, 4.0, 0.6, (5, 17, 0)), 0.50, 0.66)          # older, greyer, finer zones
    n_tone = noise(0.045, 5.0, 0.6, (23, 7, 0))                         # very large tonal drift
    n_blot = noise(0.55, 6.0, 0.62, (2, 29, 0))                         # blotchy mid-scale

    col = mixc(n_tile, A[0].outputs["Color"], A2[0].outputs["Color"])
    colB = mixc(1.0, B[0].outputs["Color"], (0.50, 0.50, 0.50, 1), "MULTIPLY")
    col = mixc(math_("MULTIPLY", n_B, 0.85), col, colB)
    agg = mixf(n_tile, A[3].outputs["Color"], A2[3].outputs["Color"])  # aggregate peaks (0..1)
    # patches: new (id 1) = dark fresh asphalt_track; old (id 0.5) = darker re-tile of A
    p_new = sstep(patch_id, 0.7, 0.8)
    p_old = math_("MULTIPLY", sstep(patch_id, 0.3, 0.4), math_("SUBTRACT", 1.0, p_new), True)
    colP = mixc(1.0, P[0].outputs["Color"], (3.2, 3.2, 3.3, 1), "MULTIPLY")
    col = mixc(p_new, col, colP)
    col = mixc(math_("MULTIPLY", p_old, 0.9), col, mixc(1.0, A2[0].outputs["Color"], (0.66, 0.66, 0.68, 1), "MULTIPLY"))
    tone = math_("ADD", math_("MULTIPLY", n_tone, 0.9), 0.55)           # 0.55..1.45
    tone = math_("MULTIPLY", tone, math_("ADD", math_("MULTIPLY", n_blot, 0.55), 0.72))
    tone = math_("MULTIPLY", tone, 0.46)                                # C19: city asphalt albedo, not concrete
    toneC = N.new("ShaderNodeCombineColor")
    for k in range(3):
        L.new(tone, toneC.inputs[k])
    col = mixc(1.0, col, toneC.outputs[0], "MULTIPLY")
    # subtle desaturate toward neutral-cool city grey
    hsv = N.new("ShaderNodeHueSaturation")
    hsv.inputs["Saturation"].default_value = 0.55
    hsv.inputs["Value"].default_value = 1.0
    L.new(col, hsv.inputs["Color"])
    col = hsv.outputs["Color"]
    # tyre polish: lighter, bleached aggregate in the wheel paths
    col = mixc(math_("MULTIPLY", wear, 0.55), col, mixc(1.0, col, (1.35, 1.33, 1.3, 1), "MULTIPLY"))
    # oil / drips: darker, faintly brown
    col = mixc(math_("MULTIPLY", oil, 0.75), col, mixc(1.0, col, (0.32, 0.30, 0.28, 1), "MULTIPLY"))
    # kerb/gutter grime
    col = mixc(math_("MULTIPLY", grime, 0.5), col, (0.045, 0.040, 0.034, 1))
    # concrete gutter band
    colG = mixc(math_("MULTIPLY", grime, 0.6), mixc(1.0, G[0].outputs["Color"], (0.78, 0.77, 0.74, 1), "MULTIPLY"),
                (0.08, 0.075, 0.065, 1))
    col = mixc(gutter, col, colG)
    # manhole iron
    col = mixc(man, col, (0.032, 0.030, 0.028, 1))
    # worn road paint: sits in the crevices, aggregate peaks punch through
    peaks = sstep(agg, 0.45, 0.8)
    pw = math_("MULTIPLY", paint_w, math_("SUBTRACT", 1.0, math_("MULTIPLY", peaks, 0.55)), True)
    py = math_("MULTIPLY", paint_y, math_("SUBTRACT", 1.0, math_("MULTIPLY", peaks, 0.45)), True)
    col = mixc(pw, col, (0.43, 0.43, 0.41, 1))
    col = mixc(py, col, (0.46, 0.31, 0.05, 1))
    # tar sealant & cracks
    col = mixc(math_("MULTIPLY", seal, 0.85), col, (0.026, 0.026, 0.028, 1))
    col = mixc(math_("MULTIPLY", crack, 0.8), col, (0.008, 0.008, 0.009, 1))

    # ---- wetness ----
    wet = math_("SUBTRACT", math_("ADD", math_("MULTIPLY", wet_lvl, 1.1), 0.30), math_("MULTIPLY", agg, 0.42), True)
    # ragged shallow edges: aggregate peaks break the water surface near the rim
    pud = sstep(math_("SUBTRACT", wet_lvl, math_("MULTIPLY", agg, 0.07)), 0.735, 0.765)
    dark = math_("SUBTRACT", 1.0, math_("MULTIPLY", wet, 0.58))
    darkC = N.new("ShaderNodeCombineColor")
    for k in range(3):
        L.new(dark, darkC.inputs[k])
    col = mixc(1.0, col, darkC.outputs[0], "MULTIPLY")
    # night grade: submerged asphalt stays visible under lamp light (no black holes where water mirrors the dark sky)
    pt = (0.66, 0.67, 0.70, 1) if night else (0.30, 0.31, 0.33, 1)
    col = mixc(math_("MULTIPLY", pud, 0.85), col, mixc(1.0, col, pt, "MULTIPLY"))
    L.new(col, bsdf.inputs["Base Color"])

    # ---- roughness ----
    rA = mixf(n_tile, A[1].outputs["Color"], A2[1].outputs["Color"])
    r = math_("ADD", math_("MULTIPLY", rA, 0.35), 0.62)                 # dry 0.62..0.97
    r = mixf(n_B, r, math_("ADD", math_("MULTIPLY", B[1].outputs["Color"], 0.3), 0.6))
    r = mixf(p_new, r, math_("ADD", math_("MULTIPLY", P[1].outputs["Color"], 0.3), 0.55))
    r = math_("SUBTRACT", r, math_("MULTIPLY", wear, 0.16))
    r = mixf(math_("MULTIPLY", oil, 0.7), r, 0.34)
    r = mixf(seal, r, 0.62)
    r = mixf(math_("MAXIMUM", pw, py), r, 0.5)
    r = mixf(gutter, r, math_("ADD", math_("MULTIPLY", G[1].outputs["Color"], 0.3), 0.55))
    r = mixf(man, r, 0.42)
    r_wet = math_("ADD", 0.13, math_("MULTIPLY", agg, 0.42))           # damp film, dry peaks
    r = mixf(wet, r, r_wet)
    r = mixf(pud, r, 0.3)
    L.new(math_("MAXIMUM", r, 0.04), bsdf.inputs["Roughness"])

    # ---- normals: PH normal (anti-tiled) -> authored relief bump ----
    ncol = mixc(n_tile, A[2].outputs["Color"], A2[2].outputs["Color"])
    ncol = mixc(math_("MULTIPLY", n_B, 0.85), ncol, B[2].outputs["Color"])
    ncol = mixc(p_new, ncol, P[2].outputs["Color"])
    ncol = mixc(gutter, ncol, G[2].outputs["Color"])
    nmap = N.new("ShaderNodeNormalMap")
    nmap.inputs["Strength"].default_value = 1.35
    L.new(ncol, nmap.inputs["Color"])
    bump = N.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 1.0
    bump.inputs["Distance"].default_value = 0.06
    L.new(H_.outputs["Color"], bump.inputs["Height"])
    L.new(nmap.outputs["Normal"], bump.inputs["Normal"])
    L.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    # ---- water film: coat (flat in standing water, follows relief when merely damp) ----
    coat_w = math_("MAXIMUM", pud, math_("MULTIPLY", math_("MULTIPLY", wet, 0.42),
                                              math_("SUBTRACT", 1.0, math_("MULTIPLY", seal, 0.75))))
    L.new(coat_w, bsdf.inputs["Coat Weight"])
    L.new(mixf(pud, 0.2, 0.012), bsdf.inputs["Coat Roughness"])
    bsdf.inputs["Coat IOR"].default_value = 1.33
    cn = N.new("ShaderNodeMix")
    cn.data_type = "VECTOR"
    L.new(pud, cn.inputs["Factor"])
    L.new(bump.outputs["Normal"], cn.inputs[4])
    rip = N.new("ShaderNodeTexNoise")
    rip.inputs["Scale"].default_value = 9.0
    rip.inputs["Detail"].default_value = 2.0
    L.new(tc.outputs["Object"], rip.inputs["Vector"])
    ripb = N.new("ShaderNodeBump")
    ripb.inputs["Strength"].default_value = 0.035
    ripb.inputs["Distance"].default_value = 0.01
    L.new(rip.outputs["Fac"], ripb.inputs["Height"])
    L.new(geo.outputs["Normal"], ripb.inputs["Normal"])
    L.new(ripb.outputs["Normal"], cn.inputs[5])
    cnn = N.new("ShaderNodeVectorMath")
    cnn.operation = "NORMALIZE"
    L.new(cn.outputs[1], cnn.inputs[0])
    L.new(cnn.outputs["Vector"], bsdf.inputs["Coat Normal"])
    bsdf.inputs["Coat Tint"].default_value = (0.9, 0.9, 0.9, 1)
    return mat


def _box(name, x0, y0, x1, y1, z0, z1, mat=None, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    o = bpy.context.active_object
    o.name = name
    o.scale = (x1 - x0, y1 - y0, z1 - z0)
    bpy.ops.object.transform_apply(scale=True)
    if bevel > 0:
        b = o.modifiers.new("bev", "BEVEL")
        b.width = bevel
        b.segments = 2
    ensure_uv(o)
    if mat:
        o.data.materials.append(mat)
    return o


def make_kerb_material_c19():
    """Granite kerb: PH stone, grime + wet line rising from the gutter."""
    m = make_ph_mat("c19_KerbGranite", "concrete_floor_worn_02", scale=1.0, roughness_boost=0.05)
    nt = m.node_tree
    bsdf = get_bsdf(m)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (0.9, 0.9, 0.9)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    for n in nt.nodes:
        if n.type == "TEX_IMAGE":
            nt.links.new(mp.outputs["Vector"], n.inputs["Vector"])
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sz = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sz.inputs["Vector"])
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.interpolation_type = "SMOOTHSTEP"
    mr.inputs["From Min"].default_value = 0.10
    mr.inputs["From Max"].default_value = 0.0
    nt.links.new(sz.outputs["Z"], mr.inputs["Value"])
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 3.0
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    fac = nt.nodes.new("ShaderNodeMath")
    fac.operation = "MULTIPLY"
    nt.links.new(mr.outputs["Result"], fac.inputs[0])
    nt.links.new(nz.outputs["Fac"], fac.inputs[1])
    fac2 = nt.nodes.new("ShaderNodeMath")
    fac2.operation = "MULTIPLY"
    fac2.use_clamp = True
    fac2.inputs[1].default_value = 1.8
    nt.links.new(fac.outputs[0], fac2.inputs[0])
    bc = bsdf.inputs["Base Color"]
    src = bc.links[0].from_socket
    tint = nt.nodes.new("ShaderNodeMix")
    tint.data_type = "RGBA"
    tint.blend_type = "MULTIPLY"
    tint.inputs[0].default_value = 1.0
    nt.links.new(src, tint.inputs[6])
    tint.inputs[7].default_value = (0.62, 0.62, 0.63, 1)
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(fac2.outputs[0], mix.inputs[0])
    nt.links.new(tint.outputs[2], mix.inputs[6])
    mix.inputs[7].default_value = (0.03, 0.028, 0.024, 1)
    nt.links.new(mix.outputs[2], bc)
    rr = bsdf.inputs["Roughness"]
    rsrc = rr.links[0].from_socket
    rmix = nt.nodes.new("ShaderNodeMix")
    rmix.data_type = "FLOAT"
    nt.links.new(fac2.outputs[0], rmix.inputs[0])
    nt.links.new(rsrc, rmix.inputs[2])
    rmix.inputs[3].default_value = 0.28
    nt.links.new(rmix.outputs[0], rr)
    return m


PARKED = [  # file, x, y, heading(+1 = faces +x), paint
    ("hm_civ_sedan_v1.obj", 33.2, -10.35, -1, (0.10, 0.11, 0.12, 1)),
    ("hm_civ_van_v1.obj", 45.5, -10.45, -1, (0.55, 0.56, 0.57, 1)),
    ("hm_civ_hatch_v1.obj", 38.8, -20.62, 1, (0.30, 0.03, 0.03, 1)),
]


def add_parked_cars_c19():
    """Kerb-side parked Harbor Metro civilian cars (density); lamps off, clearcoat paint."""
    for k, (fn, x, y, hd, paint) in enumerate(PARKED):
        objs = import_obj(MESH / fn)
        if not objs:
            continue
        meshes = [o for o in objs if o.type == "MESH"]
        bpy.context.view_layer.update()
        from mathutils import Vector as V
        pts = [o.matrix_world @ V(c) for o in meshes for c in o.bound_box]
        mnx, mxx = min(p.x for p in pts), max(p.x for p in pts)
        mny, mxy = min(p.y for p in pts), max(p.y for p in pts)
        mnz = min(p.z for p in pts)
        cx, cy = (mnx + mxx) / 2, (mny + mxy) / 2
        bpy.ops.object.empty_add(type="PLAIN_AXES", location=(cx, cy, mnz))
        e = bpy.context.active_object
        e.name = f"c19_Parked_{k}"
        for o in objs:
            if o.parent is None:
                o.parent = e
                o.matrix_parent_inverse = e.matrix_world.inverted()
        along_x = (mxx - mnx) >= (mxy - mny)
        base_rot = 0.0 if along_x else math.radians(90)
        e.rotation_euler = (0, 0, base_rot + (0 if hd > 0 else math.pi))
        e.location = (x, y, 0.012 + _crown_at(x, y))
        for o in meshes:
            shade_smooth(o)
            for slot in o.material_slots:
                m = slot.material
                b = get_bsdf(m)
                if not b:
                    continue
                if "Emission Strength" in b.inputs:
                    b.inputs["Emission Strength"].default_value = 0.0
                mn = m.name.lower()
                if "paint" in mn:
                    b.inputs["Base Color"].default_value = paint
                    b.inputs["Roughness"].default_value = 0.32
                    b.inputs["Metallic"].default_value = 0.35
                    b.inputs["Coat Weight"].default_value = 1.0
                    b.inputs["Coat Roughness"].default_value = 0.04
                elif "glass" in mn:
                    b.inputs["Roughness"].default_value = 0.03
                    b.inputs["Transmission Weight"].default_value = 0.0
                    b.inputs["Coat Weight"].default_value = 1.0
                elif any(t in mn for t in ("tire", "rubber")):
                    b.inputs["Roughness"].default_value = 0.8
        # sanity: which way did we end up (print footprint)
        print("C19_PARKED", fn, round(x, 1), round(y, 1), "along_x", along_x, "dims",
              round(mxx - mnx, 2), round(mxy - mny, 2))


def build_road_c19(night=False):
    """Replace C16 flat plane + disc puddles with the authored crowned carriageway."""
    import numpy as np
    for o in list(bpy.data.objects):
        n = o.name
        if n.startswith(("C16_Puddle", "C16_PuddleEdge", "C16_Lane_", "C16_RoadAsphalt")):
            bpy.data.objects.remove(o, do_unlink=True)
    mat = make_road_material_c19(night)
    # crowned carriageway mesh from the authored crown grid (0.2 m)
    c = np.load(str(ROADGEN / "c19_crown_0p2m.npy"))
    rows, cols = c.shape
    verts, faces = [], []
    for i in range(rows):
        y = RY1 - (i * 0.2 + 0.0075)
        for j in range(cols):
            x = RX0 + (j * 0.2 + 0.0075)
            verts.append((x, y, 0.012 + float(c[i, j])))
    for i in range(rows - 1):
        for j in range(cols - 1):
            a = i * cols + j
            faces.append((a, a + cols, a + cols + 1, a + 1))
    me = bpy.data.meshes.new("c19_RoadCarriageway")
    me.from_pydata(verts, [], faces)
    me.update()
    road = bpy.data.objects.new("c19_RoadCarriageway", me)
    bpy.context.collection.objects.link(road)
    road.data.materials.append(mat)
    for p in me.polygons:
        p.use_smooth = True
    # far ground (same stack; masks clip to damp base)
    bpy.ops.mesh.primitive_plane_add(size=1, location=(18, -12, 0.006))
    g = bpy.context.active_object
    g.name = "c19_RoadFar"
    g.scale = (130, 100, 1)
    bpy.ops.object.transform_apply(location=True, scale=True)
    g.data.materials.append(mat)

    # pavements + granite kerbs (street now has two real kerbs + N-S stub)
    conc = bpy.data.materials.get("C14_Concrete")
    pav = make_paving_mat(conc) if conc else None
    if pav:
        pav.name = "c19_PavingFlags"
    kerb = make_kerb_material_c19()
    kw, kz = 0.16, 0.15
    _box("c19_PavBankSouth", 22.0, RYN, 65.0, -5.4, 0.0, PAV_TOP, pav)
    _box("c19_KerbBankSouth", RXE, RYN, 65.0, RYN + kw, 0.0, kz, kerb, 0.012)
    _box("c19_PavMidSouth", -15.0, RYN, RXW, -5.9, 0.0, PAV_TOP, pav)
    _box("c19_KerbMidSouth", -15.0, RYN, RXW, RYN + kw, 0.0, kz, kerb, 0.012)
    _box("c19_PavMidEast", 7.0, -5.9, RXW, 1.5, 0.0, PAV_TOP, pav)
    _box("c19_KerbMidEast", RXW - kw, RYN + kw, RXW, 1.5, 0.0, kz, kerb, 0.012)
    _box("c19_PavSouth", -15.0, -27.0, 65.0, RYS, 0.0, PAV_TOP, pav)
    _box("c19_KerbSouth", -15.0, RYS - kw, 65.0, RYS, 0.0, kz, kerb, 0.012)
    # extend the C18 west pavement / kerb south to meet the new kerb line
    wp = bpy.data.objects.get("C18_WestPavement")
    if wp:
        _box("c19_WestPavExt", RXE, RYN, 22.0, -8.0, 0.0, PAV_TOP, pav)
    _box("c19_KerbWestExt", RXE - 0.09, RYN + kw, RXE + 0.09, -7.9, 0.0, 0.15, kerb, 0.012)
    wk = bpy.data.objects.get("C18_WestKerb")
    if wk:
        wk.data.materials.clear()
        wk.data.materials.append(kerb)

    # drain grates (PH rusty grate) in the gutter, recessed
    grate = make_ph_mat("c19_DrainGrate", "metal_grate_rusty", scale=2.0, darken=0.35)
    gb = get_bsdf(grate)
    gb.inputs["Metallic"].default_value = 0.6
    hole = bpy.data.materials.new("c19_DrainHole")
    hole.use_nodes = True
    get_bsdf(hole).inputs["Base Color"].default_value = (0.004, 0.004, 0.004, 1)
    get_bsdf(hole).inputs["Roughness"].default_value = 0.9
    lay = (ROADGEN / "layout.txt").read_text()
    drains = eval(lay.split("drains=")[1].strip())
    for i, (x, y, rot) in enumerate(drains):
        sx, sy = (0.45, 0.2) if rot == 0 else (0.2, 0.45)
        _box(f"c19_DrainHole_{i}", x - sx, y - sy, x + sx, y + sy, -0.25, 0.004, hole)
        gr = _box(f"c19_DrainGrate_{i}", x - sx + 0.02, y - sy + 0.02, x + sx - 0.02, y + sy - 0.02, 0.002, 0.012, grate)
        for poly in gr.data.polygons:
            poly.use_smooth = False
    # old C16 drain cubes sat on flat asphalt at y=1.5 (hidden behind buildings) -> remove
    for o in list(bpy.data.objects):
        if o.name.startswith("C16_Drain_"):
            bpy.data.objects.remove(o, do_unlink=True)

    add_parked_cars_c19()

    # cruiser sits on the crown
    cr = bpy.data.objects.get("HMPD_Cruiser")
    if cr:
        cr.location.z += _crown_at(cr.location.x, cr.location.y)
    print("C19_ROAD_BUILT authored masks + crowned mesh + kerbs/gutters/drains")


def force_architecture_ph():
    """Apply Poly Haven stone/concrete/plaster — break white monolith."""
    stone = make_ph_mat("C14_Stone", "stone_wall_02", scale=3.5)
    plaster = make_ph_mat("C14_Plaster", "plastered_wall_02", scale=2.5)
    concrete = make_ph_mat("C14_Concrete", "concrete_wall_008", scale=4.0)
    rock = make_ph_mat("C14_Rock", "rock_wall_02", scale=3.0)
    glass = make_glass_mat("C14_Glass")
    metal = make_metal_mat("C14_Metal")
    frame = make_metal_mat("C14_Frame", (0.04, 0.04, 0.045, 1), 0.35)

    for o in bpy.data.objects:
        if o.type != "MESH":
            continue
        n = o.name.lower()
        if any(k in n for k in ("c14_wet", "c15_road", "c16_road", "c14_puddle", "c16_puddle", "c14_lane", "c15_lane", "c16_lane", "c14_sidewalk", "c15_sidewalk", "c16_sidewalk",
                                  "c14w_", "c14_cc0", "kenney", "hmpd", "ped_", "body_", "hm_ped",
                                  "c14_lobby", "c16_", "c14_desk", "c14_chair", "c14_hmpd", "lamp",
                                  "wheel", "tire", "rim", "rotor", "caliper")):
            continue
        ensure_uv(o)
        for i, slot in enumerate(list(o.material_slots)):
            if not slot.material:
                continue
            mn = slot.material.name.lower()
            if any(k in mn for k in ("skin", "hair", "tire", "alloy", "wetasphalt",
                                       "blue_metallic", "bodylivery", "c14_", "hmpd",
                                       "kenney", "wheel", "rubber", "rim")):
                continue
            if any(k in mn for k in ("glass", "window", "glaze")):
                o.material_slots[i].material = glass
            elif any(k in mn for k in ("frame", "rail", "mullion", "metal", "steel", "chrome")):
                o.material_slots[i].material = frame if "frame" in mn or "rail" in mn else metal
            elif any(k in mn for k in ("concrete", "sidewalk", "curb")):
                o.material_slots[i].material = concrete
            elif any(k in mn for k in ("stone", "limestone", "granite", "brick", "rock")):
                o.material_slots[i].material = stone if "rock" not in mn else rock
            elif any(k in mn for k in ("plaster", "paint", "stucco", "sign")):
                o.material_slots[i].material = plaster
            else:
                # Crush white clay → alternate stone/plaster by object
                bsdf = get_bsdf(slot.material)
                base = list(bsdf.inputs["Base Color"].default_value) if bsdf else [0.8] * 3
                if base[0] > 0.45 and base[1] > 0.45:
                    o.material_slots[i].material = stone if (hash(o.name) % 2 == 0) else plaster

    # Also assign by object name heuristics for kits without useful mat names
    for o in bpy.data.objects:
        if o.type != "MESH" or not o.data.materials:
            continue
        n = o.name.lower()
        if any(k in n for k in ("c14_", "ped", "wheel", "tire", "cruiser", "hmpd")):
            continue
        if "glass" in n or "window" in n:
            o.data.materials[0] = glass
        elif "frame" in n:
            o.data.materials[0] = frame



def lobby_furniture_rebuild():
    """C16: replace blockout desk/chairs/screens with wood/stone/metal construction."""
    ox, oy = 34.0, 0.5
    wood = make_ph_mat("C16_DeskWood", "wood_table_001", scale=2.2, darken=0.05) if (TEX / "wood_table_001" / "wood_table_001_diff_2k.jpg").exists() else None
    if wood is None:
        wood = bpy.data.materials.new("C16_DeskWood")
        wood.use_nodes = True
        get_bsdf(wood).inputs["Base Color"].default_value = (0.28, 0.16, 0.08, 1)
        get_bsdf(wood).inputs["Roughness"].default_value = 0.42
    stone = make_ph_mat("C16_DeskStone", "stone_wall_02", scale=2.0)
    metal = make_metal_mat("C16_DeskMetal", (0.18, 0.18, 0.2, 1), 0.32)
    brass = make_metal_mat("C16_DeskBrass", (0.48, 0.34, 0.14, 1), 0.28)
    leather = bpy.data.materials.new("C16_ChairLeather")
    leather.use_nodes = True
    get_bsdf(leather).inputs["Base Color"].default_value = (0.10, 0.05, 0.035, 1)
    get_bsdf(leather).inputs["Roughness"].default_value = 0.52
    if "Coat Weight" in get_bsdf(leather).inputs:
        get_bsdf(leather).inputs["Coat Weight"].default_value = 0.15

    # Hide prior blockout desk/chairs (keep walls/sign)
    for o in list(bpy.data.objects):
        n = o.name.lower()
        if any(k in n for k in ("c14_lobbydesk", "c14_lobbydesktop", "c14_lobbychair",
                                  "c14_lobbymonitor", "c15_waitseat", "c15_waitback",
                                  "c15_waitleg")):
            o.hide_render = True
            o.hide_viewport = True

    # Reception desk: stone base + wood counter + metal edge
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 5.0, 0.45))
    base = bpy.context.active_object
    base.name = "C16_DeskBase"
    base.scale = (3.8, 1.05, 0.9)
    bpy.ops.object.transform_apply(scale=True)
    ensure_uv(base)
    base.data.materials.append(stone)

    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 5.05, 0.98))
    counter = bpy.context.active_object
    counter.name = "C16_DeskCounter"
    counter.scale = (4.0, 1.25, 0.07)
    bpy.ops.object.transform_apply(scale=True)
    ensure_uv(counter)
    counter.data.materials.append(wood)

    # Front fascia wood panel
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 4.55, 0.45))
    fascia = bpy.context.active_object
    fascia.name = "C16_DeskFascia"
    fascia.scale = (3.7, 0.08, 0.75)
    bpy.ops.object.transform_apply(scale=True)
    ensure_uv(fascia)
    fascia.data.materials.append(wood)

    # Metal edge banding
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 5.05, 1.03))
    edge = bpy.context.active_object
    edge.name = "C16_DeskEdge"
    edge.scale = (4.05, 1.28, 0.015)
    bpy.ops.object.transform_apply(scale=True)
    edge.data.materials.append(metal)

    # Monitor stands + bezels + screens
    screen_mat = bpy.data.materials.new("C16_LobbyScreen")
    screen_mat.use_nodes = True
    sb = get_bsdf(screen_mat)
    sb.inputs["Base Color"].default_value = (0.02, 0.06, 0.12, 1)
    sb.inputs["Emission Color"].default_value = (0.25, 0.55, 0.9, 1)
    sb.inputs["Emission Strength"].default_value = 3.5
    sb.inputs["Roughness"].default_value = 0.15
    for i, x in enumerate((ox - 1.35, ox + 1.35)):
        # Stand base
        bpy.ops.mesh.primitive_cylinder_add(radius=0.12, depth=0.03, location=(x, oy + 4.85, 1.05))
        stand = bpy.context.active_object
        stand.name = f"C16_MonStand_{i}"
        stand.data.materials.append(metal)
        # Neck
        bpy.ops.mesh.primitive_cylinder_add(radius=0.025, depth=0.28, location=(x, oy + 4.85, 1.22))
        neck = bpy.context.active_object
        neck.name = f"C16_MonNeck_{i}"
        neck.data.materials.append(metal)
        # Bezel
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 4.78, 1.55))
        bez = bpy.context.active_object
        bez.name = f"C16_MonBezel_{i}"
        bez.scale = (0.55, 0.04, 0.36)
        bpy.ops.object.transform_apply(scale=True)
        bez.data.materials.append(metal)
        # Screen glass
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 4.755, 1.55))
        scr = bpy.context.active_object
        scr.name = f"C16_MonScreen_{i}"
        scr.scale = (0.48, 0.015, 0.30)
        bpy.ops.object.transform_apply(scale=True)
        scr.data.materials.append(screen_mat)
        # Keyboard
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 4.95, 1.08))
        kb = bpy.context.active_object
        kb.name = f"C16_Keyboard_{i}"
        kb.scale = (0.38, 0.14, 0.015)
        bpy.ops.object.transform_apply(scale=True)
        kb.data.materials.append(metal)

    # Waiting chairs with cushions + metal legs + arms
    for i, x in enumerate((ox - 3.3, ox - 1.7, ox + 1.7, ox + 3.3)):
        y = oy + 2.15
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.38))
        seat = bpy.context.active_object
        seat.name = f"C16_ChairSeat_{i}"
        seat.scale = (0.52, 0.52, 0.07)
        bpy.ops.object.transform_apply(scale=True)
        seat.data.materials.append(leather)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y + 0.22, 0.72))
        back = bpy.context.active_object
        back.name = f"C16_ChairBack_{i}"
        back.scale = (0.52, 0.07, 0.48)
        bpy.ops.object.transform_apply(scale=True)
        back.data.materials.append(leather)
        # Cushion pad
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.44))
        cush = bpy.context.active_object
        cush.name = f"C16_ChairCush_{i}"
        cush.scale = (0.48, 0.48, 0.04)
        bpy.ops.object.transform_apply(scale=True)
        cush.data.materials.append(leather)
        # Arms
        for sx in (-0.28, 0.28):
            bpy.ops.mesh.primitive_cube_add(size=1, location=(x + sx, y, 0.58))
            arm = bpy.context.active_object
            arm.name = f"C16_ChairArm_{i}_{sx}"
            arm.scale = (0.04, 0.42, 0.04)
            bpy.ops.object.transform_apply(scale=True)
            arm.data.materials.append(metal)
        # Legs
        for sx, sy in [(-0.22, -0.22), (0.22, -0.22), (-0.22, 0.22), (0.22, 0.22)]:
            bpy.ops.mesh.primitive_cylinder_add(radius=0.018, depth=0.36, location=(x + sx, y + sy, 0.18))
            leg = bpy.context.active_object
            leg.name = f"C16_ChairLeg_{i}_{sx}_{sy}"
            leg.data.materials.append(metal)

    # Desk nameplate
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 4.55, 0.85))
    plate = bpy.context.active_object
    plate.name = "C16_DeskNameplate"
    plate.scale = (0.55, 0.03, 0.08)
    bpy.ops.object.transform_apply(scale=True)
    plate.data.materials.append(brass)

    # Floor runner / mat
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 3.2, 0.04))
    mato = bpy.context.active_object
    mato.name = "C16_LobbyRunner"
    mato.scale = (2.2, 3.5, 0.02)
    bpy.ops.object.transform_apply(scale=True)
    ensure_uv(mato)
    runner = bpy.data.materials.new("C16_RunnerMat")
    runner.use_nodes = True
    get_bsdf(runner).inputs["Base Color"].default_value = (0.12, 0.08, 0.05, 1)
    get_bsdf(runner).inputs["Roughness"].default_value = 0.9
    mato.data.materials.append(runner)
    print("C16_LOBBY_FURNITURE_REBUILT")


def build_lobby_interior():
    """Authored Meridian Mutual lobby — depth, desk, glass, stone, furniture."""
    stone = make_ph_mat("C14_LobbyStone", "stone_wall_02", scale=2.8)
    plaster = make_ph_mat("C14_LobbyPlaster", "plastered_wall_02", scale=2.0)
    concrete = make_ph_mat("C14_LobbyFloor", "concrete_wall_008", scale=5.0, darken=0.15)
    glass = make_glass_mat("C14_LobbyGlass")
    metal = make_metal_mat("C14_LobbyMetal", (0.08, 0.08, 0.1, 1), 0.28)
    wood = bpy.data.materials.new("C14_DeskWood")
    wood.use_nodes = True
    get_bsdf(wood).inputs["Base Color"].default_value = (0.22, 0.12, 0.06, 1)
    get_bsdf(wood).inputs["Roughness"].default_value = 0.45

    # Place lobby volume near bank annex street face (readable from camera)
    # Blender coords: x along street, y toward/away building, z up
    ox, oy, oz = 34.0, 0.5, 0.0  # lobby origin (just inside façade)

    # Floor
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 3.5, 0.05))
    floor = bpy.context.active_object
    floor.scale = (10, 7, 0.1)
    bpy.ops.object.transform_apply(scale=True)
    floor.name = "C14_LobbyFloor"
    ensure_uv(floor)
    floor.data.materials.append(concrete)

    # Back wall
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 7.0, 2.4))
    bw = bpy.context.active_object
    bw.scale = (10, 0.25, 4.8)
    bpy.ops.object.transform_apply(scale=True)
    bw.name = "C14_LobbyBack"
    ensure_uv(bw)
    bw.data.materials.append(stone)

    # Side walls
    for i, x in enumerate((ox - 5.0, ox + 5.0)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 3.5, 2.4))
        w = bpy.context.active_object
        w.scale = (0.25, 7.0, 4.8)
        bpy.ops.object.transform_apply(scale=True)
        w.name = f"C14_LobbySide_{i}"
        ensure_uv(w)
        w.data.materials.append(plaster)

    # Ceiling
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 3.5, 4.7))
    ceil = bpy.context.active_object
    ceil.scale = (10, 7, 0.15)
    bpy.ops.object.transform_apply(scale=True)
    ceil.name = "C14_LobbyCeil"
    ensure_uv(ceil)
    ceil.data.materials.append(plaster)

    # Glass entry (street-facing, y small)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 0.15, 2.0))
    g = bpy.context.active_object
    g.scale = (4.5, 0.06, 3.6)
    bpy.ops.object.transform_apply(scale=True)
    g.name = "C14_LobbyGlassDoor"
    g.data.materials.append(glass)
    # Metal frame around glass
    for j, (sx, sz, lx, lz) in enumerate([
        (4.7, 0.12, 0, 1.9), (4.7, 0.12, 0, -1.9),
        (0.12, 3.8, 2.3, 0), (0.12, 3.8, -2.3, 0),
    ]):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(ox + lx, oy + 0.2, 2.0 + lz))
        f = bpy.context.active_object
        f.scale = (sx, 0.1, sz)
        bpy.ops.object.transform_apply(scale=True)
        f.name = f"C14_LobbyDoorFrame_{j}"
        f.data.materials.append(metal)

    # Stone wainscot / reception backdrop panels
    for i, x in enumerate((ox - 2.5, ox, ox + 2.5)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 6.7, 1.4))
        p = bpy.context.active_object
        p.scale = (2.2, 0.2, 2.4)
        bpy.ops.object.transform_apply(scale=True)
        p.name = f"C14_LobbyPanel_{i}"
        ensure_uv(p)
        p.data.materials.append(stone if i != 1 else plaster)

    # Reception desk
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 5.0, 0.55))
    desk = bpy.context.active_object
    desk.scale = (3.6, 1.0, 1.05)
    bpy.ops.object.transform_apply(scale=True)
    desk.name = "C14_LobbyDesk"
    desk.data.materials.append(wood)
    # Desk top counter
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 5.0, 1.15))
    top = bpy.context.active_object
    top.scale = (3.8, 1.15, 0.08)
    bpy.ops.object.transform_apply(scale=True)
    top.name = "C14_LobbyDeskTop"
    top.data.materials.append(metal)

    # Chairs
    for i, x in enumerate((ox - 2.8, ox + 2.8)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 2.8, 0.45))
        ch = bpy.context.active_object
        ch.scale = (0.55, 0.55, 0.9)
        bpy.ops.object.transform_apply(scale=True)
        ch.name = f"C14_LobbyChair_{i}"
        ch.data.materials.append(wood)

    # Meridian sign board inside
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, oy + 6.85, 3.6))
    sign = bpy.context.active_object
    sign.scale = (3.5, 0.08, 0.55)
    bpy.ops.object.transform_apply(scale=True)
    sign.name = "C14_LobbySign"
    sm = bpy.data.materials.new("C14_LobbySignMat")
    sm.use_nodes = True
    sbsdf = get_bsdf(sm)
    sbsdf.inputs["Base Color"].default_value = (0.05, 0.12, 0.35, 1)
    sbsdf.inputs["Emission Color"].default_value = (0.3, 0.5, 1.0, 1)
    sbsdf.inputs["Emission Strength"].default_value = 6.0
    sign.data.materials.append(sm)

    # Readable dimensional wordmark on the back wall (not a floating facade card).
    logo_mat = bpy.data.materials.new("C14_MeridianWordmark")
    logo_mat.use_nodes = True
    lbsdf = get_bsdf(logo_mat)
    lbsdf.inputs["Base Color"].default_value = (0.75, 0.82, 0.95, 1)
    lbsdf.inputs["Metallic"].default_value = 0.65
    lbsdf.inputs["Roughness"].default_value = 0.24
    bpy.ops.object.text_add(location=(ox - 3.15, oy + 6.65, 3.45),
                            rotation=(math.radians(90), 0, 0))
    logo = bpy.context.active_object
    logo.name = "C14_LobbyMeridianMutualWordmark"
    logo.data.body = "MERIDIAN MUTUAL"
    logo.data.align_x = "LEFT"
    logo.data.size = 0.58
    logo.data.extrude = 0.018
    logo.data.bevel_depth = 0.006
    logo.data.materials.append(logo_mat)

    # Reception monitors and warm table lamps create recognizable lobby function.
    screen_mat = bpy.data.materials.new("C14_LobbyScreen")
    screen_mat.use_nodes = True
    sb = get_bsdf(screen_mat)
    sb.inputs["Base Color"].default_value = (0.02, 0.08, 0.14, 1)
    sb.inputs["Emission Color"].default_value = (0.18, 0.48, 0.85, 1)
    sb.inputs["Emission Strength"].default_value = 2.0
    for i, x in enumerate((ox - 1.25, ox + 1.25)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 4.72, 1.65))
        mon = bpy.context.active_object
        mon.name = f"C14_LobbyMonitor_{i}"
        mon.scale = (0.52, 0.06, 0.34)
        bpy.ops.object.transform_apply(scale=True)
        mon.data.materials.append(screen_mat)

    # Ceiling practicals
    for i, x in enumerate((ox - 3.0, ox, ox + 3.0)):
        bpy.ops.object.light_add(type="AREA", location=(x, oy + 3.5, 4.3))
        L = bpy.context.active_object
        L.data.energy = 450
        L.data.size = 1.2
        L.data.color = (1.0, 0.92, 0.8)
        L.rotation_euler = (0, 0, 0)
        L.name = f"C14_LobbyLight_{i}"
        # Visible fixture
        bpy.ops.mesh.primitive_cylinder_add(radius=0.25, depth=0.06, location=(x, oy + 3.5, 4.45))
        fix = bpy.context.active_object
        fix.name = f"C14_LobbyFixture_{i}"
        fm = bpy.data.materials.new(f"C14_Fix_{i}")
        fm.use_nodes = True
        get_bsdf(fm).inputs["Emission Color"].default_value = (1.0, 0.95, 0.85, 1)
        get_bsdf(fm).inputs["Emission Strength"].default_value = 12.0
        fix.data.materials.append(fm)

    # Warm fill from glass entry
    bpy.ops.object.light_add(type="AREA", location=(ox, oy - 1.5, 2.5))
    Lf = bpy.context.active_object
    Lf.data.energy = 200
    Lf.data.size = 4.0
    Lf.data.color = (0.7, 0.85, 1.0)
    Lf.rotation_euler = (math.radians(90), 0, 0)

    # --- C15 denser lobby props: wood/stone/metal separation ---
    brass = make_metal_mat("C16_LobbyBrass", (0.45, 0.32, 0.12, 1), 0.28)
    leather = bpy.data.materials.new("C16_LobbyLeather")
    leather.use_nodes = True
    get_bsdf(leather).inputs["Base Color"].default_value = (0.12, 0.06, 0.04, 1)
    get_bsdf(leather).inputs["Roughness"].default_value = 0.55
    # Waiting chairs (leather + metal legs)
    for i, x in enumerate((ox - 3.2, ox - 1.6, ox + 1.6, ox + 3.2)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 2.2, 0.42))
        seat = bpy.context.active_object
        seat.scale = (0.5, 0.5, 0.08)
        bpy.ops.object.transform_apply(scale=True)
        seat.name = f"C16_WaitSeat_{i}"
        seat.data.materials.append(leather)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 2.45, 0.72))
        back = bpy.context.active_object
        back.scale = (0.5, 0.08, 0.45)
        bpy.ops.object.transform_apply(scale=True)
        back.name = f"C16_WaitBack_{i}"
        back.data.materials.append(leather)
        for sx in (-0.18, 0.18):
            for sy in (-0.18, 0.18):
                bpy.ops.mesh.primitive_cylinder_add(radius=0.03, depth=0.4, location=(x + sx, oy + 2.2 + sy, 0.2))
                leg = bpy.context.active_object
                leg.name = f"C16_WaitLeg_{i}_{sx}_{sy}"
                leg.data.materials.append(brass)
    # Reception keyboard + phone + nameplate
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox - 0.6, oy + 4.85, 1.22))
    kb = bpy.context.active_object
    kb.scale = (0.35, 0.14, 0.02)
    bpy.ops.object.transform_apply(scale=True)
    kb.name = "C16_Keyboard"
    kb.data.materials.append(make_metal_mat("C16_KB", (0.05, 0.05, 0.06, 1), 0.35))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox + 0.9, oy + 4.85, 1.25))
    phone = bpy.context.active_object
    phone.scale = (0.12, 0.18, 0.06)
    bpy.ops.object.transform_apply(scale=True)
    phone.name = "C16_Phone"
    phone.data.materials.append(brass)
    # Baseboard trim
    for y in (oy + 0.4, oy + 6.85):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(ox, y, 0.12))
        trim = bpy.context.active_object
        trim.scale = (9.6, 0.08, 0.18)
        bpy.ops.object.transform_apply(scale=True)
        trim.name = f"C16_Baseboard_{y}"
        trim.data.materials.append(brass)
    # Ceiling coffer beams
    for i, x in enumerate((ox - 2.5, ox, ox + 2.5)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, oy + 3.5, 4.55))
        beam = bpy.context.active_object
        beam.scale = (0.2, 6.5, 0.12)
        bpy.ops.object.transform_apply(scale=True)
        beam.name = f"C16_Coffer_{i}"
        beam.data.materials.append(make_ph_mat(f"C16_CofferMat_{i}", "plastered_wall_02", scale=1.5))

    print("LOBBY_BUILT")
    lobby_furniture_rebuild()


def add_street_dressing():
    """Background mass + street furniture so night/day aren't voids."""
    plaster = make_ph_mat("C14_BgPlaster", "plastered_wall_02", scale=2.0)
    stone = make_ph_mat("C14_BgStone", "stone_wall_02", scale=2.5)
    concrete = make_ph_mat("C14_BgConcrete", "concrete_wall_008", scale=3.0)
    # Distant building blocks (real volumes, not floating cards)
    specs = [
        (-5, 8, 0, 8, 6, 14, plaster),
        (50, 6, 0, 10, 7, 18, stone),
        (55, -5, 0, 6, 5, 10, concrete),
        (-8, -5, 0, 7, 5, 12, stone),
        (44.5, -0.2, 0, 11, 10.6, 16, stone),  # C19: out of the carriageway, fills the north street wall
    ]
    for i, (x, y, z, sx, sy, sz, mat) in enumerate(specs):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, sz / 2))
        o = bpy.context.active_object
        o.scale = (sx, sy, sz)
        bpy.ops.object.transform_apply(scale=True)
        o.name = f"C14_BgBldg_{i}"
        ensure_uv(o)
        o.data.materials.append(mat)
        # Window glow strips for night (emissive panels embedded)
        for wi in range(3):
            bpy.ops.mesh.primitive_cube_add(
                size=1, location=(x - sx * 0.4 + wi * sx * 0.35, y - sy * 0.51, 3 + wi * 3))
            w = bpy.context.active_object
            w.scale = (sx * 0.25, 0.08, 1.4)
            bpy.ops.object.transform_apply(scale=True)
            w.name = f"C14_BgWin_{i}_{wi}"
            wm = bpy.data.materials.new(f"C14_BgWinMat_{i}_{wi}")
            wm.use_nodes = True
            wbsdf = get_bsdf(wm)
            wbsdf.inputs["Base Color"].default_value = (0.9, 0.75, 0.4, 1)
            wbsdf.inputs["Emission Color"].default_value = (1.0, 0.85, 0.5, 1)
            wbsdf.inputs["Emission Strength"].default_value = 0.8  # day; boosted at night
            w.data.materials.append(wm)

    # Bollards / planters
    metal = make_metal_mat("C14_Bollard")
    for i, x in enumerate((10, 14, 22, 30, 38)):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.12, depth=1.0, location=(x, 1.2, 0.5))
        b = bpy.context.active_object
        b.name = f"C14_Bollard_{i}"
        b.data.materials.append(metal)


def add_street_lamps(night: bool):
    energy = 3200 if night else 500
    em = 80.0 if night else 6.0
    positions = [
        # C19: all poles on pavements (north kerb row y=-8.5, south kerb row y=-22.6)
        (8, -8.5, 5.2), (18, -8.5, 5.2), (28, -8.5, 5.2), (38, -8.5, 5.2),
        (12, -22.6, 5.2), (24, -22.6, 5.2), (36, -22.6, 5.2),
        (35, -6.0, 4.8),
    ]
    pole_m = make_metal_mat("C14_LampPole", (0.07, 0.07, 0.08, 1), 0.4)
    for i, loc in enumerate(positions):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.07, depth=5.0, location=(loc[0], loc[1], 2.5))
        pole = bpy.context.active_object
        pole.name = f"C14_LampPole_{i}"
        pole.data.materials.append(pole_m)
        bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
        fix = bpy.context.active_object
        fix.scale = (0.45, 0.35, 0.12)
        bpy.ops.object.transform_apply(scale=True)
        fix.name = f"C14_LampHead_{i}"
        fmat = bpy.data.materials.new(f"C14_LampHeadMat_{i}")
        fmat.use_nodes = True
        fbsdf = get_bsdf(fmat)
        fbsdf.inputs["Base Color"].default_value = (1.0, 0.85, 0.55, 1)
        fbsdf.inputs["Emission Color"].default_value = (1.0, 0.82, 0.55, 1)
        fbsdf.inputs["Emission Strength"].default_value = em
        fix.data.materials.append(fmat)
        bpy.ops.object.light_add(type="AREA", location=(loc[0], loc[1], loc[2] - 0.25))
        L = bpy.context.active_object
        L.data.energy = energy
        L.data.size = 0.9
        L.data.color = (1.0, 0.85, 0.55)
        L.rotation_euler = (math.radians(90), 0, 0)
        # C19: downward pool onto the wet carriageway (streaked reflections at night)
        bpy.ops.object.light_add(type="SPOT", location=(loc[0], loc[1] + (-0.6 if loc[1] > -15 else 0.6), loc[2] - 0.2))
        S = bpy.context.active_object
        S.name = f"c19_LampPool_{i}"
        S.data.energy = 2600 if night else 0.0
        S.data.spot_size = math.radians(115)
        S.data.spot_blend = 0.6
        S.data.shadow_soft_size = 0.25
        S.data.color = (1.0, 0.8, 0.52)
        S.rotation_euler = (0, 0, 0)
        if not night:
            S.hide_render = True


def add_meridian_sign():
    bpy.ops.mesh.primitive_cube_add(size=1, location=(35, 3.6, 5.2))
    sign = bpy.context.active_object
    sign.scale = (5.5, 0.16, 0.9)
    bpy.ops.object.transform_apply(scale=True)
    sign.name = "C14_MeridianSign"
    mat = bpy.data.materials.new("C14_MeridianSignMat")
    mat.use_nodes = True
    bsdf = get_bsdf(mat)
    bsdf.inputs["Base Color"].default_value = (0.05, 0.12, 0.35, 1)
    bsdf.inputs["Metallic"].default_value = 0.4
    bsdf.inputs["Roughness"].default_value = 0.3
    bsdf.inputs["Emission Color"].default_value = (0.25, 0.45, 1.0, 1)
    bsdf.inputs["Emission Strength"].default_value = 5.0
    sign.data.materials.append(mat)


def setup_world(night: bool):
    world = bpy.data.worlds.new("MeridianWorld")
    bpy.context.scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    nodes, links = nt.nodes, nt.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputWorld")
    bg = nodes.new("ShaderNodeBackground")
    if night:
        # C19: faint urban sky-glow so night puddles mirror a dim sky instead of reading as black holes
        bg.inputs["Color"].default_value = (0.055, 0.058, 0.075, 1)
        bg.inputs["Strength"].default_value = 0.55
        links.new(bg.outputs["Background"], out.inputs["Surface"])
    else:
        sky = nodes.new("ShaderNodeTexSky")
        sky.sky_type = "NISHITA"
        sky.sun_elevation = math.radians(26)
        sky.sun_rotation = math.radians(200)
        sky.sun_intensity = 0.4
        bg.inputs["Strength"].default_value = 0.5
        links.new(sky.outputs["Color"], bg.inputs["Color"])
        links.new(bg.outputs["Background"], out.inputs["Surface"])
        bpy.ops.object.light_add(type="SUN", location=(30, -20, 40))
        sun = bpy.context.active_object
        sun.data.energy = 1.4
        sun.data.angle = math.radians(0.5)
        sun.data.color = (1.0, 0.97, 0.92)
        sun.rotation_euler = (math.radians(38), math.radians(12), math.radians(-35))
        bpy.ops.object.light_add(type="AREA", location=(-8, -28, 16))
        fill = bpy.context.active_object
        fill.data.energy = 90
        fill.data.size = 16
        fill.data.color = (0.65, 0.75, 1.0)


def import_buildings():
    for name in ("hm_bank_annex_v10.obj", "hm_storefront_v10.obj", "hm_midrise_v10.obj"):
        objs = import_obj(MESH / name)
        for o in objs:
            shade_smooth(o)
            n = o.name.lower()
            if any(k in n for k in ("_ash_", "ashrev", "_rev", "_pane", "_room", "banner", "bunting", "flag", "string")):
                o.hide_render = True
                o.hide_viewport = True
                continue
            print("building", name, o.name)


def force_hero_cruiser_mats():
    for mat in bpy.data.materials:
        bsdf = get_bsdf(mat)
        if not bsdf:
            continue
        n = mat.name.lower()
        if any(k in n for k in ("blue_metallic", "bodylivery", "hmpd_paint", "carpaint", "paintchip")) or (
            n.endswith("paint") and "lane" not in n
        ):
            bsdf.inputs["Base Color"].default_value = (0.008, 0.02, 0.08, 1.0)
            bsdf.inputs["Metallic"].default_value = 0.55
            bsdf.inputs["Roughness"].default_value = 0.28
            if "Coat Weight" in bsdf.inputs:
                bsdf.inputs["Coat Weight"].default_value = 1.0
                bsdf.inputs["Coat Roughness"].default_value = 0.05
            for link in list(mat.node_tree.links):
                if link.to_node == bsdf and link.to_socket.name == "Base Color":
                    mat.node_tree.links.remove(link)
        if any(k in n for k in ("tire", "rubber")) and "trim" not in n:
            bsdf.inputs["Base Color"].default_value = (0.02, 0.02, 0.022, 1)
            bsdf.inputs["Roughness"].default_value = 0.78
            bsdf.inputs["Metallic"].default_value = 0.0
        if any(k in n for k in ("alloy", "rim", "c14w_", "c15w_")):
            bsdf.inputs["Base Color"].default_value = (0.40, 0.41, 0.43, 1)
            bsdf.inputs["Metallic"].default_value = 0.85
            bsdf.inputs["Roughness"].default_value = 0.48



def cruiser_construction_pass(cruiser_empty):
    """C16: panel gaps, trim, lightbar polish, glass darken, interior occlusion."""
    if cruiser_empty is None:
        return
    gap_m = make_metal_mat("C16_PanelGap", (0.02, 0.02, 0.022, 1), 0.55)
    trim_m = make_metal_mat("C16_BodyTrim", (0.08, 0.08, 0.09, 1), 0.35)
    chrome_m = make_metal_mat("C16_ChromeTrim", (0.55, 0.55, 0.58, 1), 0.22)
    glass_dark = make_glass_mat("C16_CabinGlass")
    gbsdf = get_bsdf(glass_dark)
    gbsdf.inputs["Base Color"].default_value = (0.02, 0.03, 0.04, 1)
    gbsdf.inputs["Roughness"].default_value = 0.08
    if "Transmission Weight" in gbsdf.inputs:
        gbsdf.inputs["Transmission Weight"].default_value = 0.55
    if "Alpha" in gbsdf.inputs:
        gbsdf.inputs["Alpha"].default_value = 0.85
    interior = bpy.data.materials.new("C16_CabinInterior")
    interior.use_nodes = True
    ib = get_bsdf(interior)
    ib.inputs["Base Color"].default_value = (0.03, 0.03, 0.035, 1)
    ib.inputs["Roughness"].default_value = 0.85

    # Door / hood / trunk shut-line grooves (thin dark cubes)
    gaps = [
        # hood shut
        (0.0, -1.55, 0.78, 1.55, 0.012, 0.012),
        # left door front
        (0.92, -0.35, 0.55, 0.012, 1.15, 0.012),
        # left door rear
        (0.92, 0.55, 0.55, 0.012, 1.05, 0.012),
        # right door front
        (-0.92, -0.35, 0.55, 0.012, 1.15, 0.012),
        # right door rear
        (-0.92, 0.55, 0.55, 0.012, 1.05, 0.012),
        # trunk
        (0.0, 1.55, 0.72, 1.4, 0.012, 0.012),
    ]
    for i, (x, y, z, sx, sy, sz) in enumerate(gaps):
        bpy.ops.mesh.primitive_cube_add(size=1)
        g = bpy.context.active_object
        g.name = f"C16_PanelGap_{i}"
        g.scale = (sx, sy, sz)
        bpy.ops.object.transform_apply(scale=True)
        g.parent = cruiser_empty
        g.location = (x, y, z)
        g.data.materials.append(gap_m)

    # Window surround trim
    for i, (x, y, z, sx, sy, sz) in enumerate([
        (0.88, -0.15, 1.05, 0.03, 1.6, 0.03),
        (-0.88, -0.15, 1.05, 0.03, 1.6, 0.03),
        (0.0, -1.05, 1.12, 1.5, 0.03, 0.03),
        (0.0, 0.85, 1.12, 1.5, 0.03, 0.03),
    ]):
        bpy.ops.mesh.primitive_cube_add(size=1)
        t = bpy.context.active_object
        t.name = f"C16_WinTrim_{i}"
        t.scale = (sx, sy, sz)
        bpy.ops.object.transform_apply(scale=True)
        t.parent = cruiser_empty
        t.location = (x, y, z)
        t.data.materials.append(trim_m)

    # Dark cabin volume + seats (read through glass)
    bpy.ops.mesh.primitive_cube_add(size=1)
    cabin = bpy.context.active_object
    cabin.name = "C16_CabinVolume"
    cabin.scale = (1.35, 1.8, 0.55)
    bpy.ops.object.transform_apply(scale=True)
    cabin.parent = cruiser_empty
    cabin.location = (0.0, 0.05, 0.85)
    cabin.data.materials.append(interior)
    for i, y in enumerate((-0.35, 0.45)):
        bpy.ops.mesh.primitive_cube_add(size=1)
        seat = bpy.context.active_object
        seat.name = f"C16_Seat_{i}"
        seat.scale = (0.55, 0.45, 0.28)
        bpy.ops.object.transform_apply(scale=True)
        seat.parent = cruiser_empty
        seat.location = (0.35 if i == 0 else -0.35, y, 0.55)
        seat.data.materials.append(interior)

    # Steering wheel hint
    bpy.ops.mesh.primitive_torus_add(major_radius=0.16, minor_radius=0.015, major_segments=24, minor_segments=8)
    wheel = bpy.context.active_object
    wheel.name = "C16_Steering"
    wheel.parent = cruiser_empty
    wheel.location = (0.35, -0.55, 0.85)
    wheel.rotation_euler = Euler((math.radians(70), 0, 0), "XYZ")
    wheel.data.materials.append(trim_m)

    # Lightbar base polish
    bpy.ops.mesh.primitive_cube_add(size=1)
    lb = bpy.context.active_object
    lb.name = "C16_LightbarBase"
    lb.scale = (0.95, 0.22, 0.05)
    bpy.ops.object.transform_apply(scale=True)
    lb.parent = cruiser_empty
    lb.location = (0.0, -0.15, 1.55)
    lb.data.materials.append(trim_m)
    for i, (x, col) in enumerate([(-0.35, (0.8, 0.05, 0.05, 1)), (0.35, (0.05, 0.15, 0.85, 1))]):
        bpy.ops.mesh.primitive_cube_add(size=1)
        lens = bpy.context.active_object
        lens.name = f"C16_LightbarLens_{i}"
        lens.scale = (0.28, 0.18, 0.08)
        bpy.ops.object.transform_apply(scale=True)
        lens.parent = cruiser_empty
        lens.location = (x, -0.15, 1.62)
        lm = bpy.data.materials.new(f"C16_LB_{i}")
        lm.use_nodes = True
        lbsdf = get_bsdf(lm)
        lbsdf.inputs["Base Color"].default_value = col
        lbsdf.inputs["Emission Color"].default_value = col
        lbsdf.inputs["Emission Strength"].default_value = 1.5
        lbsdf.inputs["Roughness"].default_value = 0.2
        lens.data.materials.append(lm)

    # Door handles
    for i, (x, y) in enumerate([(0.95, -0.2), (-0.95, -0.2), (0.95, 0.7), (-0.95, 0.7)]):
        bpy.ops.mesh.primitive_cube_add(size=1)
        h = bpy.context.active_object
        h.name = f"C16_Handle_{i}"
        h.scale = (0.03, 0.12, 0.025)
        bpy.ops.object.transform_apply(scale=True)
        h.parent = cruiser_empty
        h.location = (x, y, 0.78)
        h.data.materials.append(chrome_m)

    # Mirror caps
    for i, x in enumerate((0.95, -0.95)):
        bpy.ops.mesh.primitive_cube_add(size=1)
        mir = bpy.context.active_object
        mir.name = f"C16_Mirror_{i}"
        mir.scale = (0.08, 0.14, 0.08)
        bpy.ops.object.transform_apply(scale=True)
        mir.parent = cruiser_empty
        mir.location = (x, -1.15, 1.05)
        mir.data.materials.append(trim_m)

    # Force any existing glass mats darker
    for mat in bpy.data.materials:
        n = mat.name.lower()
        if any(k in n for k in ("glass", "window", "windshield", "glaze")):
            bsdf = get_bsdf(mat)
            if not bsdf:
                continue
            bsdf.inputs["Base Color"].default_value = (0.03, 0.04, 0.05, 1)
            bsdf.inputs["Roughness"].default_value = min(0.12, float(bsdf.inputs["Roughness"].default_value))
            if "Transmission Weight" in bsdf.inputs:
                bsdf.inputs["Transmission Weight"].default_value = 0.6
    print("C16_CRUISER_CONSTRUCTION_PASS")


def place_cruiser():
    path = MESH / "hmpd_cruiser_v7b.obj"
    if not path.exists():
        path = MESH / "hmpd_cruiser_v13b.obj"
    objs = import_obj(path)
    root = fury_to_b(18.0, 0.0, 14.5)
    bpy.ops.object.empty_add(type="PLAIN_AXES", location=root)
    empty = bpy.context.active_object
    empty.name = "HMPD_Cruiser"
    empty.rotation_euler = Euler((0, 0, math.radians(90)))
    for o in objs:
        o.parent = empty
        shade_smooth(o)
        n = o.name.lower()
        if any(k in n for k in ("v11_grime", "v11_cage", "v12_chassis", "v12_ub_",
                                  "v12_well", "cage", "occ", "dirt_brakedust",
                                  "v11_roadgrime", "v12_grimeglass", "guide_",
                                  "push", "rail", "scaffold", "plate", "card",
                                  "panel_side", "sidebox", "equip", "spotlight",
                                  "mirror_block", "antenna", "sensor", "ground", "stripe_")):
            bpy.data.objects.remove(o, do_unlink=True)
            continue
        # Delete thin floating plates on cruiser (dimensions)
        if o.type == "MESH":
            d = sorted(o.dimensions)
            if d[0] < 0.08 and d[1] > 0.3 and d[2] > 0.4 and o.location.z > 0.3:
                print("DEL_CRUISER_CARD", o.name)
                bpy.data.objects.remove(o, do_unlink=True)
                continue
    print("cruiser parts", len([o for o in bpy.data.objects if o.parent == empty]))
    cruiser_construction_pass(empty)
    return empty


def delete_old_wheels(cruiser_empty):
    """Hard-delete prior wheel meshes so smooth discs cannot survive."""
    for o in list(bpy.data.objects):
        n = o.name.lower()
        parented = (o.parent == cruiser_empty) or (o.parent and o.parent.parent == cruiser_empty)
        if any(k in n for k in (
            "wheel", "tire", "rim", "spoke", "rotor", "caliper", "lug",
            "sidewall", "v13_tire", "v13_rim", "v13_spoke", "v13_rotor",
            "c13w_", "well",
        )):
            if parented or n.startswith("wheel_") or "c13w_" in n or "v13_" in n:
                print("DEL_WHEEL", o.name)
                bpy.data.objects.remove(o, do_unlink=True)


def build_spoke_wheel(name, loc, cruiser_empty, flip=False):
    """Manufactured wheel C16: thinner tire/rim + 5 spokes + rotor + caliper + lugs.

    Critical: NO solid face-filling cylinder (that reads as smooth disc).
    """
    tire_m = bpy.data.materials.new(f"C14_Tire_{name}")
    tire_m.use_nodes = True
    t = get_bsdf(tire_m)
    t.inputs["Base Color"].default_value = (0.015, 0.015, 0.016, 1)
    t.inputs["Roughness"].default_value = 0.92
    t.inputs["Metallic"].default_value = 0.0

    alloy_m = bpy.data.materials.new(f"C14_Alloy_{name}")
    alloy_m.use_nodes = True
    a = get_bsdf(alloy_m)
    a.inputs["Base Color"].default_value = (0.42, 0.43, 0.45, 1)
    a.inputs["Metallic"].default_value = 0.85
    a.inputs["Roughness"].default_value = 0.48

    rotor_m = bpy.data.materials.new(f"C14_Rotor_{name}")
    rotor_m.use_nodes = True
    r = get_bsdf(rotor_m)
    r.inputs["Base Color"].default_value = (0.25, 0.25, 0.27, 1)
    r.inputs["Metallic"].default_value = 1.0
    r.inputs["Roughness"].default_value = 0.45

    cal_m = bpy.data.materials.new(f"C14_Caliper_{name}")
    cal_m.use_nodes = True
    c = get_bsdf(cal_m)
    c.inputs["Base Color"].default_value = (0.35, 0.02, 0.02, 1)
    c.inputs["Roughness"].default_value = 0.4
    c.inputs["Metallic"].default_value = 0.3

    side = 1.0 if not flip else -1.0

    def parent_at(obj, z_off=0.0):
        obj.parent = cruiser_empty
        obj.location = (loc[0], loc[1], loc[2] + z_off)
        obj.rotation_euler = Euler((0, math.radians(90), 0), "XYZ")
        shade_smooth(obj)

    # Tire
    bpy.ops.mesh.primitive_torus_add(
        major_radius=0.355, minor_radius=0.085,
        major_segments=56, minor_segments=18, location=(0, 0, 0))
    tire = bpy.context.active_object
    tire.name = f"C14W_tire_{name}"
    tire.data.materials.append(tire_m)
    parent_at(tire)
    # Sidewall tread ribs so the tire does not read as a smooth torus.
    for ri in range(8):
        ang = ri * (2 * math.pi / 8)
        bpy.ops.mesh.primitive_cube_add(size=1)
        rib = bpy.context.active_object
        rib.name = f"C14W_tread_{name}_{ri}"
        rib.scale = (0.04, 0.03, 0.08)
        bpy.ops.object.transform_apply(scale=True)
        rib.parent = cruiser_empty
        rib.location = (
            loc[0] + side * 0.08,
            loc[1] + math.cos(ang) * 0.34,
            loc[2] + math.sin(ang) * 0.34,
        )
        rib.rotation_euler = Euler((ang, math.radians(90), 0), "XYZ")
        rib.data.materials.append(tire_m)

    # Outer rim lip as TORUS ring — never a filled disc
    bpy.ops.mesh.primitive_torus_add(
        major_radius=0.235, minor_radius=0.022,
        major_segments=40, minor_segments=10, location=(0, 0, 0))
    lip = bpy.context.active_object
    lip.name = f"C14W_liplo_{name}"
    lip.data.materials.append(alloy_m)
    parent_at(lip)

    # Inner rim ring
    bpy.ops.mesh.primitive_torus_add(
        major_radius=0.195, minor_radius=0.018,
        major_segments=36, minor_segments=8, location=(0, 0, 0))
    barrel = bpy.context.active_object
    barrel.name = f"C14W_barrel_{name}"
    barrel.data.materials.append(alloy_m)
    parent_at(barrel)

    # Hub
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.07, location=(0, 0, 0), vertices=16)
    hub = bpy.context.active_object
    hub.name = f"C14W_hub_{name}"
    hub.data.materials.append(alloy_m)
    parent_at(hub)

    # 5 thick radial spokes with clear gaps for rotor readout.
    for i in range(5):
        ang = i * (2 * math.pi / 5)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0))
        sp = bpy.context.active_object
        sp.name = f"C14W_spoke_{name}_{i}"
        sp.scale = (0.045, 0.24, 0.038)
        bpy.ops.object.transform_apply(scale=True)
        sp.parent = cruiser_empty
        sp.location = (
            loc[0] + side * 0.05,
            loc[1] + math.cos(ang) * 0.12,
            loc[2] + math.sin(ang) * 0.12,
        )
        sp.rotation_euler = Euler((ang, math.radians(90), 0), "XYZ")
        sp.data.materials.append(alloy_m)
        shade_smooth(sp)

    # Rotor BEHIND spokes (visible through gaps)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.175, depth=0.018, location=(0, 0, 0), vertices=32)
    rot = bpy.context.active_object
    rot.name = f"C14W_rotor_{name}"
    rot.data.materials.append(rotor_m)
    parent_at(rot)
    # nudge inward
    rot.location = (loc[0] - side * 0.04, loc[1], loc[2])

    # Caliper
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0))
    cal = bpy.context.active_object
    cal.name = f"C14W_caliper_{name}"
    cal.scale = (0.06, 0.1, 0.07)
    bpy.ops.object.transform_apply(scale=True)
    cal.parent = cruiser_empty
    cal.location = (loc[0], loc[1] + 0.14, loc[2])
    cal.rotation_euler = Euler((0, math.radians(90), 0), "XYZ")
    cal.data.materials.append(cal_m)

    # Lug nuts
    for i in range(5):
        ang = i * (2 * math.pi / 5) + 0.2
        bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=0.02, location=(0, 0, 0), vertices=8)
        lug = bpy.context.active_object
        lug.name = f"C14W_lug_{name}_{i}"
        lug.parent = cruiser_empty
        lug.location = (
            loc[0] + side * 0.04,
            loc[1] + math.cos(ang) * 0.035,
            loc[2] + math.sin(ang) * 0.035,
        )
        lug.rotation_euler = Euler((0, math.radians(90), 0), "XYZ")
        lug.data.materials.append(alloy_m)


def rebuild_wheels(cruiser_empty):
    """Manufactured rim+tire+rotor assembly. Kenney GLB is single-slot and reads as a
    silver disc once materials are forced; procedural parts keep tire/alloy/rotor
    separation so spokes are readable in the hero crop.
    """
    delete_old_wheels(cruiser_empty)
    specs = [
        ("FL", (0.90, -1.35, 0.34), False),
        ("FR", (-0.90, -1.35, 0.34), True),
        ("RL", (0.90, 1.35, 0.34), False),
        ("RR", (-0.90, 1.35, 0.34), True),
    ]
    for name, loc, flip in specs:
        build_spoke_wheel(name, loc, cruiser_empty, flip=flip)
    print("C16_SPOKE_WHEELS_THINNER", len(specs))



def add_hmpd_markings(cruiser_empty):
    """Dimensional Harbor Metro markings on both doors."""
    mat = bpy.data.materials.new("C14_HMPD_DoorLettering")
    mat.use_nodes = True
    bsdf = get_bsdf(mat)
    bsdf.inputs["Base Color"].default_value = (0.82, 0.88, 0.96, 1)
    bsdf.inputs["Metallic"].default_value = 0.25
    bsdf.inputs["Roughness"].default_value = 0.3
    for side in (-1, 1):
        bpy.ops.object.text_add()
        txt = bpy.context.active_object
        txt.name = f"C14_HMPD_Marking_{side}"
        txt.data.body = "HMPD"
        txt.data.align_x = "CENTER"
        txt.data.align_y = "CENTER"
        txt.data.size = 0.34
        txt.data.extrude = 0.008
        txt.data.bevel_depth = 0.002
        txt.data.materials.append(mat)
        txt.parent = cruiser_empty
        txt.location = (side * 0.915, 0.22, 0.70)
        txt.rotation_euler = Euler((math.radians(90), 0, side * math.radians(90)), "XYZ")


def force_ped_mats():
    """Legacy (C12-C17) ped material fixes. C18 MPFB ped materials are skipped."""
    for mat in bpy.data.materials:
        if mat.name.lower().startswith("hm_ped_"):
            continue
        bsdf = get_bsdf(mat)
        if not bsdf:
            continue
        n = mat.name.lower()
        if "invisible" in n:
            bsdf.inputs["Base Color"].default_value = (0.02, 0.02, 0.02, 1)
            if "Alpha" in bsdf.inputs:
                bsdf.inputs["Alpha"].default_value = 0.0
            if "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = 0.0
            try:
                mat.blend_method = "HASHED"
            except Exception:
                pass
        if "eyewhite" in n or "sclera" in n:
            bsdf.inputs["Base Color"].default_value = (0.82, 0.84, 0.86, 1)
            bsdf.inputs["Roughness"].default_value = 0.4
            if "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = 0.0
            if "Subsurface Weight" in bsdf.inputs:
                bsdf.inputs["Subsurface Weight"].default_value = 0.05
        if "cornea" in n:
            bsdf.inputs["Roughness"].default_value = 0.15
            if "Transmission Weight" in bsdf.inputs:
                bsdf.inputs["Transmission Weight"].default_value = 0.1
            if "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = 0.0
        if n.endswith("_shirt") or "_shirt." in n:
            col = list(bsdf.inputs["Base Color"].default_value)[:3]
            if col[0] > 0.45 and col[1] > 0.25 and col[2] < 0.30:
                bsdf.inputs["Base Color"].default_value = (0.18, 0.28, 0.22, 1)
        if n.endswith("_jacket") or "_jacket." in n:
            col = list(bsdf.inputs["Base Color"].default_value)[:3]
            if col[0] > 0.35 and col[2] < 0.25:
                bsdf.inputs["Base Color"].default_value = (0.12, 0.16, 0.18, 1)
        if "skinwarm" in n:
            bsdf.inputs["Base Color"].default_value = (0.82, 0.65, 0.54, 1)



def import_glb(path: Path):
    if not path.exists():
        print("MISSING", path)
        return []
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    return [o for o in bpy.data.objects if o not in before]


PED_BLENDS = Path("/workspace/vaultline-blender/assets/peds_v18")

# (name, blender-space xy, yaw deg). MPFB humans face -Y (toward the 04 camera) at yaw 0.
# Small conversational group on the south pavement in front of the bank facade (y=-5.5).
PED_LAYOUT = [
    ("rae", (19.80, -0.35), -158.0),   # talking pair, 3/4 toward each other
    ("dane", (20.50, 0.75), -22.0),
    ("noah", (18.35, -0.85), -118.0),  # foreground right, waiting
    ("suki", (21.20, 2.35), -70.0),    # background left, by the pilaster
    ("ivy", (20.80, -5.60), -33.0),    # corner of the block (reads in 02/05 street)
]


def append_ped(name):
    path = PED_BLENDS / f"hm_ped_{name}_v18.blend"
    if not path.exists():
        print("MISSING_PED", path)
        return []
    coll_name = f"hm_ped_{name}"
    with bpy.data.libraries.load(str(path), link=False) as (src, dst):
        dst.collections = [c for c in src.collections if c == coll_name]
    objs = []
    for c in dst.collections:
        bpy.context.scene.collection.children.link(c)
        objs += list(c.objects)
    return objs


def place_peds():
    """Cycle-18: MPFB/MakeHuman CC0 peds (clean silhouettes, SSS skin, textured cloth/hair)."""
    for name, (x, y), yaw in PED_LAYOUT:
        objs = append_ped(name)
        rig = next((o for o in objs if o.type == "ARMATURE"), None)
        if rig is None:
            continue
        rig.location = Vector((x, y, 0.0))
        rig.rotation_euler = Euler((0, 0, math.radians(yaw)))
        for o in objs:
            if o.type == "MESH":
                o.visible_shadow = True
        print("ped v18", name, len(objs))


def is_ped(o):
    return o is not None and o.name.lower().startswith("hm_ped_")


def make_paving_mat(conc):
    """C14 concrete + 0.9 m flag joints (brick texture) + slight per-flag tone variation."""
    m = conc.copy()
    m.name = "C18_PavingFlags"
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    br = nt.nodes.new("ShaderNodeTexBrick")
    br.offset = 0.0
    br.inputs["Scale"].default_value = 1.0
    br.inputs["Mortar Size"].default_value = 0.012
    br.inputs["Brick Width"].default_value = 0.9
    br.inputs["Row Height"].default_value = 0.9
    br.inputs["Color1"].default_value = (1.0, 1.0, 1.0, 1)
    br.inputs["Color2"].default_value = (0.86, 0.85, 0.83, 1)
    br.inputs["Mortar"].default_value = (0.35, 0.34, 0.33, 1)
    nt.links.new(tc.outputs["Object"], br.inputs["Vector"])
    bc = bsdf.inputs["Base Color"]
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs[0].default_value = 1.0
    if bc.is_linked:
        nt.links.new(bc.links[0].from_socket, mul.inputs[6])
    else:
        mul.inputs[6].default_value = bc.default_value
    nt.links.new(br.outputs["Color"], mul.inputs[7])
    nt.links.new(mul.outputs[2], bc)
    # darken slightly: C14 concrete reads too bright/clean at ped hero distance
    dk = nt.nodes.new("ShaderNodeMix")
    dk.data_type = "RGBA"
    dk.blend_type = "MULTIPLY"
    dk.inputs[0].default_value = 1.0
    dk.inputs[7].default_value = (0.78, 0.77, 0.76, 1)
    nt.links.new(mul.outputs[2], dk.inputs[6])
    nt.links.new(dk.outputs[2], bc)
    return m


def add_south_pavement():
    """Concrete pavement + granite kerb along the bank's west pilaster facade (x=22) so
    the 04 group stands on a real sidewalk instead of carriageway asphalt."""
    conc = bpy.data.materials.get("C14_Concrete")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(19.78, 0.0, 0.06))
    pv = bpy.context.active_object
    pv.name = "C18_WestPavement"
    pv.scale = (4.35, 16.0, 0.12)
    bpy.ops.object.transform_apply(scale=True)
    ensure_uv(pv, scale=1.0)
    if conc:
        pv.data.materials.append(make_paving_mat(conc))
    bpy.ops.mesh.primitive_cube_add(size=1, location=(17.62, 0.0, 0.075))
    kb = bpy.context.active_object
    kb.name = "C18_WestKerb"
    kb.scale = (0.18, 16.0, 0.15)
    bpy.ops.object.transform_apply(scale=True)
    bev = kb.modifiers.new("bev", "BEVEL")
    bev.width = 0.02
    bev.segments = 2
    km = bpy.data.materials.new("C18_KerbGranite")
    km.use_nodes = True
    kbs = km.node_tree.nodes.get("Principled BSDF")
    kbs.inputs["Base Color"].default_value = (0.24, 0.24, 0.235, 1)
    kbs.inputs["Roughness"].default_value = 0.7
    kb.data.materials.append(km)
    # peds stand on the pavement top
    for o in bpy.data.objects:
        if o.type == "ARMATURE" and is_ped(o):
            o.location.z = 0.12


def set_camera(name, fury_pos, look_at_fury, lens=35):
    loc = fury_to_b(*fury_pos)
    target = fury_to_b(*look_at_fury)
    old = bpy.data.objects.get(name)
    if old:
        bpy.data.objects.remove(old, do_unlink=True)
    bpy.ops.object.camera_add(location=loc)
    cam = bpy.context.active_object
    cam.name = name
    direction = target - loc
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = lens
    cam.data.clip_start = 0.1
    cam.data.clip_end = 600
    bpy.context.scene.camera = cam
    return cam


def set_camera_blender(name, loc, look_at, lens=35):
    """Camera in Blender world coords (for authored lobby)."""
    old = bpy.data.objects.get(name)
    if old:
        bpy.data.objects.remove(old, do_unlink=True)
    bpy.ops.object.camera_add(location=Vector(loc))
    cam = bpy.context.active_object
    cam.name = name
    direction = Vector(look_at) - Vector(loc)
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = lens
    cam.data.clip_start = 0.05
    cam.data.clip_end = 600
    bpy.context.scene.camera = cam
    return cam


def configure_cycles(samples=72):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    try:
        cprefs = bpy.context.preferences.addons["cycles"].preferences
        for dtype in ("CUDA", "OPTIX", "HIP", "METAL", "ONEAPI"):
            try:
                cprefs.compute_device_type = dtype
                cprefs.get_devices()
                used = False
                for d in cprefs.devices:
                    if d.type != "CPU":
                        d.use = True
                        used = True
                if used:
                    scene.cycles.device = "GPU"
                    break
            except Exception:
                continue
    except Exception:
        pass
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    try:
        scene.cycles.denoiser = "OPENIMAGEDENOISE"
    except Exception:
        pass
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = int(os.environ.get("BEAUTY_PCT", "100"))
    if os.environ.get("BEAUTY_SAMPLES"):
        samples = int(os.environ["BEAUTY_SAMPLES"])
        scene.cycles.samples = samples
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_depth = "8"
    scene.view_settings.view_transform = "Filmic"
    scene.view_settings.look = "Medium High Contrast"
    scene.view_settings.exposure = -1.0
    scene.cycles.max_bounces = 8
    scene.cycles.transparent_max_bounces = 8


def render_to(path: Path):
    scene = bpy.context.scene
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    print("WROTE", path, path.exists(), path.stat().st_size if path.exists() else 0)


def boost_night_windows():
    for mat in bpy.data.materials:
        if "BgWinMat" in mat.name or "LobbySign" in mat.name:
            bsdf = get_bsdf(mat)
            if bsdf and "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = max(
                    8.0, float(bsdf.inputs["Emission Strength"].default_value) * 8)




def add_authored_facade_detail():
    """Window frames, trim, street furniture — less procedural slab."""
    frame_m = make_metal_mat("C16_WinFrame", (0.06, 0.06, 0.07, 1), 0.42)
    sill_m = make_ph_mat("C16_Sill", "concrete_wall_008", scale=8.0, darken=0.1)
    # Authored window frames on bank façade facing street
    for i, (x, z) in enumerate([
        (32.5, 2.2), (34.5, 2.2), (36.5, 2.2), (38.5, 2.2),
        (32.5, 4.4), (34.5, 4.4), (36.5, 4.4), (38.5, 4.4),
        (32.5, 6.6), (34.5, 6.6), (36.5, 6.6), (38.5, 6.6),
    ]):
        # Frame border
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 3.15, z))
        fr = bpy.context.active_object
        fr.scale = (0.95, 0.08, 1.15)
        bpy.ops.object.transform_apply(scale=True)
        fr.name = f"C16_WinFrame_{i}"
        fr.data.materials.append(frame_m)
        # Glass inset
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 3.12, z))
        gl = bpy.context.active_object
        gl.scale = (0.78, 0.04, 0.95)
        bpy.ops.object.transform_apply(scale=True)
        gl.name = f"C16_WinGlass_{i}"
        gl.data.materials.append(make_glass_mat(f"C16_WinGlassMat_{i}"))
        # Sill
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 3.2, z - 0.65))
        si = bpy.context.active_object
        si.scale = (1.05, 0.18, 0.08)
        bpy.ops.object.transform_apply(scale=True)
        si.name = f"C16_Sill_{i}"
        ensure_uv(si)
        si.data.materials.append(sill_m)
    # Street furniture: benches, planters, hydrant, news box
    wood = bpy.data.materials.new("C16_BenchWood")
    wood.use_nodes = True
    get_bsdf(wood).inputs["Base Color"].default_value = (0.18, 0.1, 0.05, 1)
    get_bsdf(wood).inputs["Roughness"].default_value = 0.55
    metal = make_metal_mat("C16_FurnMetal", (0.12, 0.12, 0.13, 1), 0.4)
    for i, x in enumerate((10.0, 14.0, 22.0)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 5.2, 0.28))
        b = bpy.context.active_object
        b.scale = (1.4, 0.45, 0.12)
        bpy.ops.object.transform_apply(scale=True)
        b.name = f"C16_Bench_{i}"
        b.data.materials.append(wood)
        for sx in (-0.55, 0.55):
            bpy.ops.mesh.primitive_cube_add(size=1, location=(x + sx, 5.2, 0.12))
            leg = bpy.context.active_object
            leg.scale = (0.08, 0.35, 0.24)
            bpy.ops.object.transform_apply(scale=True)
            leg.name = f"C16_BenchLeg_{i}_{sx}"
            leg.data.materials.append(metal)
    # Planters
    for i, x in enumerate((12.0, 16.0, 26.0, 30.0)):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.35, depth=0.55, location=(x, 6.0, 0.28))
        pl = bpy.context.active_object
        pl.name = f"C16_Planter_{i}"
        pl.data.materials.append(make_ph_mat(f"C16_PlanterMat_{i}", "concrete_wall_008", scale=3.0))
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.32, location=(x, 6.0, 0.7))
        bush = bpy.context.active_object
        bush.scale = (1.0, 1.0, 0.7)
        bpy.ops.object.transform_apply(scale=True)
        bush.name = f"C16_Bush_{i}"
        bm = bpy.data.materials.new(f"C16_BushMat_{i}")
        bm.use_nodes = True
        get_bsdf(bm).inputs["Base Color"].default_value = (0.08, 0.22, 0.06, 1)
        get_bsdf(bm).inputs["Roughness"].default_value = 0.85
        bush.data.materials.append(bm)
    # Hydrant
    bpy.ops.mesh.primitive_cylinder_add(radius=0.12, depth=0.55, location=(19.5, 5.5, 0.28))
    hy = bpy.context.active_object
    hy.name = "C16_Hydrant"
    hm = bpy.data.materials.new("C16_HydrantMat")
    hm.use_nodes = True
    get_bsdf(hm).inputs["Base Color"].default_value = (0.45, 0.05, 0.05, 1)
    get_bsdf(hm).inputs["Roughness"].default_value = 0.4
    get_bsdf(hm).inputs["Metallic"].default_value = 0.2
    hy.data.materials.append(hm)
    print("C16_FACADE_DETAIL_ADDED")




    # --- C16 arch density: more frames, weathering, street furniture ---
    weather = make_ph_mat("C16_WeatherStain", "concrete_wall_008", scale=10.0, darken=0.35)
    for i, (x, z) in enumerate([(33.0, 0.4), (35.0, 0.4), (37.0, 0.4), (39.0, 0.4)]):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 3.25, z))
        stain = bpy.context.active_object
        stain.scale = (1.6, 0.04, 0.35)
        bpy.ops.object.transform_apply(scale=True)
        stain.name = f"C16_Weather_{i}"
        ensure_uv(stain)
        stain.data.materials.append(weather)
    # Bollards
    for i, x in enumerate((9.0, 12.0, 15.0, 20.0, 25.0)):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.12, depth=0.85, location=(x, 5.6, 0.42))
        b = bpy.context.active_object
        b.name = f"C16_Bollard_{i}"
        b.data.materials.append(make_metal_mat(f"C16_BollardMat_{i}", (0.15, 0.15, 0.16, 1), 0.4))
        bpy.ops.mesh.primitive_cylinder_add(radius=0.13, depth=0.04, location=(x, 5.6, 0.86))
        cap = bpy.context.active_object
        cap.name = f"C16_BollardCap_{i}"
        cap.data.materials.append(make_metal_mat(f"C16_BollardCapMat_{i}", (0.35, 0.3, 0.12, 1), 0.3))
    # Trash bins
    for i, x in enumerate((11.5, 21.5)):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.28, depth=0.85, location=(x, 5.0, 0.42))
        bin_ = bpy.context.active_object
        bin_.name = f"C16_Bin_{i}"
        bin_.data.materials.append(make_metal_mat(f"C16_BinMat_{i}", (0.08, 0.1, 0.08, 1), 0.5))
    # Drain grates near curb
    for i, x in enumerate((10.0, 18.0, 26.0, 34.0)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 1.5, 0.03))
        grate = bpy.context.active_object
        grate.scale = (0.55, 0.35, 0.02)
        bpy.ops.object.transform_apply(scale=True)
        grate.name = f"C16_Drain_{i}"
        grate.data.materials.append(make_metal_mat(f"C16_DrainMat_{i}", (0.05, 0.05, 0.05, 1), 0.6))
    # Window mullion density (extra verticals)
    for i, (x, z) in enumerate([
        (33.5, 2.2), (35.5, 2.2), (37.5, 2.2),
        (33.5, 4.4), (35.5, 4.4), (37.5, 4.4),
        (33.5, 6.6), (35.5, 6.6), (37.5, 6.6),
    ]):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 3.14, z))
        mul = bpy.context.active_object
        mul.scale = (0.04, 0.06, 1.05)
        bpy.ops.object.transform_apply(scale=True)
        mul.name = f"C16_Mullion_{i}"
        mul.data.materials.append(make_metal_mat(f"C16_MulMat_{i}", (0.05, 0.05, 0.055, 1), 0.4))
    print("C16_ARCH_DENSITY")

def purge_debug_geometry():
    """ZERO yellow/orange/magenta/unshaded emitters in every hero.
    Delete proxy/debug meshes and kill hot debug emission materials.
    """
    killed = []
    # Delete objects with debug-ish names
    for o in list(bpy.data.objects):
        n = o.name.lower()
        if n.startswith("hm_ped_"):
            continue  # C18 MPFB peds: authored materials, never debug
        if any(k in n for k in (
            "debug", "proxy", "helper", "calib", "measure", "guide_",
            "empty_axis", "origin_marker", "axis_gizmo",
        )):
            if o.type in ("MESH", "EMPTY", "CURVE"):
                killed.append(o.name)
                bpy.data.objects.remove(o, do_unlink=True)
                continue
        if o.type != "MESH":
            continue
        for slot in o.material_slots:
            mat = slot.material
            if not mat or not mat.use_nodes:
                continue
            bsdf = get_bsdf(mat)
            if not bsdf:
                continue
            mn = mat.name.lower()
            em = 0.0
            if "Emission Strength" in bsdf.inputs:
                em = float(bsdf.inputs["Emission Strength"].default_value)
            col = list(bsdf.inputs["Base Color"].default_value)[:3]
            em_col = list(bsdf.inputs["Emission Color"].default_value)[:3] if "Emission Color" in bsdf.inputs else col
            # Also treat absolute RGB emission (Ke-style) as intensity
            em_peak = max(em_col) if em_col else 0.0

            def is_debug_rgb(c):
                r, g, b = [float(x) for x in c]
                # normalize if Ke dumped into color channels >1
                peak = max(r, g, b, 1e-6)
                if peak > 1.0:
                    r, g, b = r / peak, g / peak, b / peak
                if r > 0.55 and g > 0.25 and b < 0.40 and (r - b) > 0.25:  # yellow/orange/amber
                    return True
                if r > 0.7 and g < 0.25 and b > 0.7:  # magenta
                    return True
                if r < 0.2 and g > 0.7 and b > 0.7:  # cyan
                    return True
                return False

            # Name-based amber / turn-signal / invisible purge
            if any(k in mn for k in ("amber", "orange", "debug", "proxy", "invisible")):
                if "invisible" in mn:
                    bsdf.inputs["Base Color"].default_value = (0.02, 0.02, 0.02, 1)
                    if "Alpha" in bsdf.inputs:
                        bsdf.inputs["Alpha"].default_value = 0.0
                    if "Emission Strength" in bsdf.inputs:
                        bsdf.inputs["Emission Strength"].default_value = 0.0
                    try:
                        mat.blend_method = "HASHED"
                    except Exception:
                        pass
                    killed.append(f"INVIS:{o.name}/{mat.name}")
                else:
                    # Convert amber turn signal to subtle warm lens (not blaze block)
                    bsdf.inputs["Base Color"].default_value = (0.55, 0.42, 0.12, 1)
                    if "Emission Strength" in bsdf.inputs:
                        bsdf.inputs["Emission Strength"].default_value = min(em, 0.35)
                    if "Emission Color" in bsdf.inputs:
                        bsdf.inputs["Emission Color"].default_value = (1.0, 0.7, 0.25, 1)
                    bsdf.inputs["Roughness"].default_value = 0.18
                    if "Transmission Weight" in bsdf.inputs:
                        bsdf.inputs["Transmission Weight"].default_value = 0.35
                    killed.append(f"AMBER:{o.name}/{mat.name}")
                continue

            hot = (em > 1.2 and is_debug_rgb(em_col)) or (em_peak > 1.5 and is_debug_rgb(em_col))
            if hot:
                if any(k in n for k in ("light", "lamp", "head", "tail", "signal", "bar", "amber", "hl", "tl")) or any(
                    k in mn for k in ("amber", "light", "emit", "lens", "signal")
                ):
                    bsdf.inputs["Emission Strength"].default_value = 0.0
                    if "Emission Color" in bsdf.inputs:
                        bsdf.inputs["Emission Color"].default_value = (0.85, 0.9, 1.0, 1)
                    bsdf.inputs["Base Color"].default_value = (0.75, 0.82, 0.9, 1)
                    bsdf.inputs["Roughness"].default_value = 0.08
                    killed.append(f"NEUTRALIZE:{o.name}/{mat.name}")
                else:
                    killed.append(o.name)
                    bpy.data.objects.remove(o, do_unlink=True)
                    break
            elif is_debug_rgb(col) and em < 1.2:
                # Unshaded yellow/orange plastic blocks — recolor always (not name-gated)
                if any(k in mn for k in ("lane", "sign", "mark", "paint", "stripe",
                                          "lip", "skin", "hair", "nail", "brow", "iris",
                                          "cornea", "eye", "pupil", "wood", "brass",
                                          "leather", "warm", "shirt", "jacket", "pants", "shoe")):
                    continue
                bsdf.inputs["Base Color"].default_value = (0.08, 0.09, 0.1, 1)
                bsdf.inputs["Roughness"].default_value = 0.45
                if "Emission Strength" in bsdf.inputs:
                    bsdf.inputs["Emission Strength"].default_value = 0.0
                killed.append(f"RECOLOR:{o.name}/{mat.name}")
    # Sweep ALL materials for leftover debug emission / amber blaze
    for mat in bpy.data.materials:
        if mat.name.lower().startswith("hm_ped_"):
            continue
        bsdf = get_bsdf(mat)
        if not bsdf:
            continue
        mn = mat.name.lower()
        if any(k in mn for k in ("amber", "orange")):
            bsdf.inputs["Base Color"].default_value = (0.55, 0.42, 0.12, 1)
            if "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = min(
                    float(bsdf.inputs["Emission Strength"].default_value), 0.35)
            if "Emission Color" in bsdf.inputs:
                bsdf.inputs["Emission Color"].default_value = (1.0, 0.7, 0.25, 1)
            killed.append(f"MAT_AMBER:{mat.name}")
            continue
        if "Emission Strength" not in bsdf.inputs:
            continue
        em = float(bsdf.inputs["Emission Strength"].default_value)
        em_col = list(bsdf.inputs["Emission Color"].default_value)[:3]
        r, g, b = em_col
        peak = max(r, g, b, 1e-6)
        rn, gn, bn = (r / peak, g / peak, b / peak) if peak > 1 else (r, g, b)
        if (em > 1.5 or peak > 1.5) and rn > 0.55 and gn > 0.25 and bn < 0.40 and (rn - bn) > 0.25:
            bsdf.inputs["Emission Strength"].default_value = 0.0
            bsdf.inputs["Emission Color"].default_value = (0.8, 0.85, 0.95, 1)
            bsdf.inputs["Base Color"].default_value = (0.75, 0.82, 0.9, 1)
            killed.append(f"MAT:{mat.name}")
    print("DEBUG_PURGE", len(killed), killed[:12])


def build_scene(night: bool):
    clear_scene()
    import_buildings()
    hide_imported_grounds()
    place_cruiser()
    empty = bpy.data.objects.get("HMPD_Cruiser")
    if empty:
        rebuild_wheels(empty)
        add_hmpd_markings(empty)
    place_peds()
    make_wet_asphalt_ph()
    build_lobby_interior()
    add_street_dressing()
    add_authored_facade_detail()
    add_street_lamps(night=night)
    add_meridian_sign()
    force_architecture_ph()
    force_hero_cruiser_mats()
    force_ped_mats()
    hide_junk()
    purge_debug_geometry()
    add_south_pavement()
    build_road_c19(night)
    setup_world(night=night)
    configure_cycles(samples=64 if night else 56)
    if night:
        boost_night_windows()
        # Extra practicals + distant glow fill
        for i, x in enumerate([28.0, 31.0, 34.0, 37.0, 40.0, 43.0]):
            for j, z in enumerate([2.5, 5.0, 7.5, 10.0]):
                bpy.ops.object.light_add(type="AREA", location=(x, 2.9, z))
                Lw = bpy.context.active_object
                Lw.data.energy = 280 + (i % 3) * 50
                Lw.data.size = 1.2
                Lw.data.color = (1.0, 0.82, 0.55) if i % 2 == 0 else (0.7, 0.85, 1.0)
                Lw.rotation_euler = (1.1, 0, 0)
        for i, (x, y, z, e, c) in enumerate([
            (16.0, -6.0, 3.5, 700, (1.0, 0.7, 0.4)),
            (22.0, -6.0, 3.2, 550, (1.0, 0.75, 0.5)),
            (18.0, -14.0, 1.5, 400, (0.3, 0.45, 1.0)),
            (10.0, -10.0, 4.0, 450, (1.0, 0.85, 0.6)),
            (40.0, -12.0, 5.0, 380, (0.5, 0.6, 1.0)),
            (50.0, 4.0, 8.0, 600, (1.0, 0.8, 0.5)),
            (-5.0, 6.0, 7.0, 500, (0.6, 0.7, 1.0)),
        ]):
            bpy.ops.object.light_add(type="POINT", location=(x, y, z))
            L = bpy.context.active_object
            L.data.energy = e
            L.data.color = c
            L.data.shadow_soft_size = 0.5
        bpy.ops.object.light_add(type="AREA", location=(20.0, -5.0, 14.0))
        Lf = bpy.context.active_object
        Lf.data.energy = 120
        Lf.data.size = 28
        Lf.data.color = (0.25, 0.35, 0.65)


def face_crop(src: Path, dst: Path, box):
    """Crop with bpy image API (Blender may lack PIL). box=(x0,y0,x1,y1)."""
    try:
        img = bpy.data.images.load(str(src), check_existing=False)
        w, h = img.size
        x0, y0, x1, y1 = [int(v) for v in box]
        x0, y0 = max(0, x0), max(0, y0)
        x1, y1 = min(w, x1), min(h, y1)
        cw, ch = max(1, x1 - x0), max(1, y1 - y0)
        # Blender pixels are bottom-up
        by0 = h - y1
        pixels = list(img.pixels)
        out = [0.0] * (cw * ch * 4)
        for row in range(ch):
            src_row = by0 + row
            for col in range(cw):
                si = ((src_row * w) + (x0 + col)) * 4
                di = ((row * cw) + col) * 4
                out[di:di + 4] = pixels[si:si + 4]
        crop = bpy.data.images.new(dst.name, width=cw, height=ch, alpha=True)
        crop.pixels = out
        crop.filepath_raw = str(dst)
        crop.file_format = "PNG"
        crop.save()
        bpy.data.images.remove(img)
        bpy.data.images.remove(crop)
        print("CROP", dst)
    except Exception as e:
        print("crop fail", e)
        # Fallback: copy full frame so pack never lacks a crop file
        try:
            import shutil
            shutil.copy2(src, dst)
            print("CROP_FALLBACK_COPY", dst)
        except Exception as e2:
            print("crop fallback fail", e2)


PED_HEADS = {}
ASPHALT_CAMS = [
    ("02_asphalt_near.png", (14.2, -20.9, 1.45), (19.2, -18.6, 0.0), 32),
]


def record_ped_heads():
    """Project each ped's head bone into the 04 camera (pixel coords, top-left origin)."""
    from bpy_extras.object_utils import world_to_camera_view
    sc = bpy.context.scene
    bpy.context.view_layer.update()
    W = sc.render.resolution_x * sc.render.resolution_percentage // 100
    Hh = sc.render.resolution_y * sc.render.resolution_percentage // 100
    for o in bpy.data.objects:
        if o.type == "ARMATURE" and is_ped(o) and "head" in o.pose.bones:
            pb = o.pose.bones["head"]
            wp = o.matrix_world @ ((pb.head + pb.tail) * 0.5)
            c = world_to_camera_view(sc, sc.camera, wp)
            if 0 <= c.x <= 1 and 0 <= c.y <= 1 and c.z > 0:
                PED_HEADS[o.name.replace("hm_ped_", "")] = (int(c.x * W), int((1 - c.y) * Hh))
    print("PED_HEADS", PED_HEADS)


PED_CAM = dict(loc=(14.2, 0.30, 1.58), look=(20.0, 0.12, 1.22), lens=45)


def setup_ped_hero():
    """04 hero: eye-level 44mm on the pavement group, bank facade as backdrop.
    Soft key from camera-left/front, cool rim from behind (separates hair/shoulders
    from the stone), weak warm bounce from the pavement."""
    set_camera_blender("04_peds.png", PED_CAM["loc"], PED_CAM["look"], lens=PED_CAM["lens"])
    cam = bpy.context.scene.camera
    cam.data.dof.use_dof = True
    cam.data.dof.focus_distance = (Vector((20.1, 0.2, 1.55)) - Vector(PED_CAM["loc"])).length
    cam.data.dof.aperture_fstop = 5.6

    def area(nm, loc, tgt, energy, size, col):
        bpy.ops.object.light_add(type="AREA", location=loc)
        L = bpy.context.active_object
        L.name = nm
        L.data.energy = energy
        L.data.size = size
        L.data.color = col
        L.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        return L

    # two-key setup for the facing pair: KeyL models Rae (faces +y), KeyR models Dane (faces -y)
    area("C18_PedKey", (15.9, 5.2, 3.8), (19.9, 0.0, 1.35), 1000, 3.5, (1.0, 0.95, 0.88))
    area("C18_PedFill", (16.4, -4.9, 3.2), (20.4, 0.8, 1.45), 650, 3.0, (0.92, 0.94, 1.0))
    area("C18_PedRim", (21.55, -2.2, 3.6), (19.9, 0.2, 1.45), 700, 2.0, (0.92, 0.96, 1.0))
    area("C18_PedRim2", (21.55, 3.6, 3.4), (20.4, 1.0, 1.45), 450, 2.0, (1.0, 0.95, 0.9))
    bpy.context.scene.cycles.samples = int(os.environ.get("PED_SAMPLES", "128"))
    bpy.context.scene.render.resolution_percentage = int(os.environ.get("PED_PCT", "100"))
    record_ped_heads()


def main():
    # Lobby uses Blender-space camera into authored interior
    # Street/cruiser/peds/night use fury_to_b cams
    day_shots = [
        # Street
        ("02_street.png", (10.0, 1.25, 20.5), (24.0, 1.2, 6.0), 24),
        # Cruiser 3/4 front — C19 reframed wider so the wet crosswalk/puddles carry the shot
        ("03_cruiser.png", (24.6, 1.05, 18.9), (18.3, 0.62, 14.4), 35),
        # Peds closer hero — faces readable, not army line
        ("04_peds.png", (5.3, 1.50, 12.1), (8.2, 1.30, 14.2), 54),
    ]
    night_shots = [
        ("05_night_or_alt.png", (14.0, 1.55, 19.0), (26.0, 1.6, 6.0), 28),
    ]

    only = {x.strip() for x in os.environ.get("BEAUTY_ONLY", "").split(",") if x.strip()}

    build_scene(night=False)
    if (not only) or ("01_lobby.png" in only) or ("01" in only):
        # 01 lobby — Blender coords into authored lobby
        # Camera INSIDE lobby (past glass door) — readable desk/depth/materials
        set_camera_blender(
            "01_lobby.png",
            loc=(28.6, -1.35, 2.85),
            look_at=(34.0, 5.0, 1.35),
            lens=20,
        )
        # Hide glass door for this shot so it cannot veil the lobby
        for o in bpy.data.objects:
            n = o.name
            if any(k in n for k in ("LobbyGlassDoor", "LobbyDoorFrame", "hm_bank_annex", "hm_storefront", "hm_midrise")):
                o.hide_render = True
                o.hide_viewport = True
        render_to(OUT / "01_lobby.png")
        # Restore bank kit for subsequent street heroes
        for o in bpy.data.objects:
            n = o.name
            if any(k in n for k in ("hm_bank_annex", "hm_storefront", "hm_midrise")):
                o.hide_render = False
                o.hide_viewport = False

    for fn, cam, look, lens in day_shots:
        if only and fn not in only and fn.split("_")[0] not in only:
            continue
        if fn == "04_peds.png":
            setup_ped_hero()
        else:
            set_camera(fn, cam, look, lens=lens)
        render_to(OUT / fn)
        if fn == "03_cruiser.png":
            # native wheel close-up (Gate B evidence no longer a crop of the wider 03)
            set_camera_blender("03_cruiser_wheel_crop.png", (21.35, -16.95, 0.50), (19.45, -15.35, 0.36), lens=50)
            render_to(OUT / "03_cruiser_wheel_crop.png")
        if fn == "04_peds.png":
            # native (not upscaled) medium two-shot of the conversing pair, same street/lights
            set_camera_blender("04_peds_medium.png", (17.35, -0.25, 1.62), (20.15, 0.22, 1.47), lens=55)
            cam = bpy.context.scene.camera
            cam.data.dof.use_dof = True
            cam.data.dof.focus_distance = 3.0
            cam.data.dof.aperture_fstop = 4.0
            render_to(OUT / "04_peds_medium.png")
            for nm in ("C18_PedKey", "C18_PedFill", "C18_PedRim", "C18_PedRim2"):
                o = bpy.data.objects.get(nm)
                if o:
                    bpy.data.objects.remove(o, do_unlink=True)

    if (not only) or ("02" in only) or ("02_asphalt_near.png" in only):
        # C19 native asphalt close-up: south gutter + drain + pothole patch + crosswalk + puddle
        for fn, loc, tgt, lens in ASPHALT_CAMS:
            set_camera_blender(fn, loc, tgt, lens=lens)
            cam = bpy.context.scene.camera
            cam.data.dof.use_dof = True
            cam.data.dof.focus_distance = (Vector(tgt) - Vector(loc)).length
            cam.data.dof.aperture_fstop = 8.0
            render_to(OUT / fn)

    if (not only) or any(x in only for x in ("05_night_or_alt.png", "05", "05_night")):
        build_scene(night=True)
        for fn, cam, look, lens in night_shots:
            if only and fn not in only and fn.split("_")[0] not in only:
                continue
            set_camera(fn, cam, look, lens=lens)
            render_to(OUT / fn)
        # C19 native night asphalt: low, looking down the wet carriageway into the lamp row
        set_camera_blender("05_night_asphalt_near.png", (7.0, -19.8, 1.25), (30.0, -15.2, 0.2), lens=35)
        render_to(OUT / "05_night_asphalt_near.png")

    ped = OUT / "04_peds.png"
    if ped.exists() and PED_HEADS:
        H = PED_HEADS
        xs = [H[k][0] for k in ("rae", "dane", "noah") if k in H]
        ys = [H[k][1] for k in ("rae", "dane", "noah") if k in H]
        face_crop(ped, OUT / "04_peds_crop_faces.png", (min(xs) - 120, min(ys) - 70, max(xs) + 160, max(ys) + 330))
        for k, fn, r in (("noah", "04_peds_crop_near.png", 150), ("rae", "04_peds_crop_rae_face.png", 90),
                         ("dane", "04_peds_crop_dane_face.png", 90)):
            if k in H:
                x, y = H[k]
                face_crop(ped, OUT / fn, (x - r, y - r, x + r, y + int(r * 1.2)))
    street = OUT / "02_street.png"
    if street.exists():
        face_crop(street, OUT / "02_street_asphalt_crop.png", (150, 380, 950, 700))
        face_crop(street, OUT / "02_street_reflect_crop.png", (400, 350, 1000, 680))
    cruiser = OUT / "03_cruiser.png"
    if cruiser.exists():
        face_crop(cruiser, OUT / "03_cruiser_hood_crop.png", (560, 200, 1000, 440))
        face_crop(cruiser, OUT / "03_cruiser_door_crop.png", (360, 220, 700, 480))
        face_crop(cruiser, OUT / "03_cruiser_glass_crop.png", (400, 150, 800, 330))
    night = OUT / "05_night_or_alt.png"
    if night.exists():
        pass
    nnear = OUT / "05_night_asphalt_near.png"
    if nnear.exists():
        face_crop(nnear, OUT / "05_night_asphalt_crop.png", (240, 380, 1040, 720))
    anear = OUT / "02_asphalt_near.png"
    if anear.exists():
        face_crop(anear, OUT / "02_asphalt_near_crop.png", (240, 330, 1040, 720))
    if night.exists():
        face_crop(night, OUT / "05_night_cruiser_reflect_crop.png", (400, 220, 1000, 560))

    (OUT / "README.md").write_text(
        "# Cycle-19 Blender beauty PRIMARY\n\n"
        "Cycle-19 P0 asphalt: C16 flat plane + disc puddle meshes removed. New crowned carriageway mesh\n"
        "(two kerbs + N-S stub, 4 lanes) shaded with a layered Poly Haven CC0 stack (asphalt_02 x2 anti-tiled,\n"
        "asphalt_04 worn zones, asphalt_track fresh patches, concrete_floor_worn_02 gutter band, metal_grate_rusty\n"
        "drains) driven by authored world-space masks (scripts/gen_road_masks_v19.py): wheel-path ruts + tyre\n"
        "polish, birdbath dips, utility-cut + pothole patches with tar seams, sealed/open cracks + alligator zones,\n"
        "oil drips, kerb grime, worn paint (crosswalks, stop bars, arrows, double yellow), manholes. Water stands\n"
        "only in gutters/ruts/dips (9% of road) as a clear-coat film with ragged edges; rest is damp.\n"
        "Native asphalt close-ups: 02_asphalt_near (+crop), 05_night_asphalt_near (+crop).\n"
        "Also: kerbside parked civ cars, lamp poles moved onto pavements, 03 reframed 3/4 + native wheel close-up.\n"
        "Peds (MPFB2/MakeHuman CC0, C18) unchanged. Harbor Metro / HMPD / Meridian Mutual only. No Rockstar/GTA IP.\n"
    )
    (OUT / "SEE_ALSO_blender_peds.txt").write_text(
        "See ../blender_peds/ for MPFB/MakeHuman CC0 ped face/full/silhouette proofs.\n"
    )
    print("DONE beauty v19")


if __name__ == "__main__":
    main()
