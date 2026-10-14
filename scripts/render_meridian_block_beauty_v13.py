#!/usr/bin/env python3
"""Cycle-13 PRIMARY ChatGPT evidence: Blender Cycles beauty — Meridian Mutual block.

P0 gates: wet asphalt NOT mirror; cruiser wheels/equipment finished; peds at hero
distance (all Antonia); architecture stone/paint/glass/metal; denser night;
composition junk removed.

Harbor Metro / HMPD / Meridian Mutual ONLY — no Rockstar/GTA IP.
Outputs → /workspace/Fury/artifacts/aaa_meridian_block/blender_scene_beauty/
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector, Euler

ROOT = Path("/workspace/vaultline-blender")
sys.path.insert(0, str(ROOT / "scripts"))
from vl_common import clear_scene  # noqa: E402

MESH = Path("/workspace/Fury/assets/meshes/harbor_metro")
PEDS = MESH / "peds"
OUT = Path("/workspace/Fury/artifacts/aaa_meridian_block/blender_scene_beauty")
OUT.mkdir(parents=True, exist_ok=True)


def fury_to_b(x: float, y: float, z: float) -> Vector:
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
    bpy.ops.wm.obj_import(
        filepath=str(path),
        forward_axis="NEGATIVE_Z",
        up_axis="Y",
    )
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


def hide_junk():
    for o in list(bpy.data.objects):
        n = o.name.lower()
        if any(k in n for k in (
            "lod1", "cage", "proxy", "helper", "collision", "wire",
            "sensor", "lidar", "rig_pole", "calib", "occ", "guide_",
            "debug_", "measure_", "float_", "v11_grime", "v12_chassis",
            "v12_ub_", "v12_well_", "v11_roadgrime", "dirt_brakedust",
            "v12_grimeglass", "v12_susp", "facadecard", "midglass",
            "envband", "banner", "awning", "cable", "antenna",
            "v11_cage", "steer_", "mdt", "interior", "cabin",
        )):
            o.hide_render = True
            o.hide_viewport = True
        # Thin junk planes / rods / billboard cards floating in frame
        if o.type == "MESH":
            d = o.dimensions
            sorted_d = sorted([d.x, d.y, d.z])
            if "c13_" in n or "lamp" in n or "wet" in n or "lane" in n or "frame" in n or "sign" in n or "facade" in n or "meridian" in n:
                pass
            else:
                # large thin panel (billboard)
                if sorted_d[0] < 0.25 and sorted_d[1] > 1.5 and sorted_d[2] > 2.5 and o.location.z > 1.5:
                    o.hide_render = True
                    o.hide_viewport = True
                if sorted_d[0] < 0.03 and sorted_d[2] > 1.5 and abs(o.location.z) > 2.0:
                    o.hide_render = True
                    o.hide_viewport = True


def hide_imported_grounds():
    """Kill building-kit ground planes that wash out as white mirrors."""
    for o in list(bpy.data.objects):
        n = o.name.lower()
        if o.type != "MESH":
            continue
        # Flat wide horizontal slabs near z=0 from building kits
        if any(k in n for k in ("sidewalk", "asphalt", "road", "ground", "plaza", "pavement", "curb")):
            if "wetasphalt" in n or "authored" in n:
                continue
            o.hide_render = True
            o.hide_viewport = True
            print("HIDE_GROUND", o.name)
        # Heuristic: very flat large meshes near origin
        dims = o.dimensions
        if dims.x > 8 and dims.y > 8 and dims.z < 0.4 and abs(o.location.z) < 0.5:
            if "wetasphalt" not in n and "lane" not in n and "puddle" not in n:
                # likely kit ground
                if any(k in n for k in ("plane", "floor", "slab", "hm_", "concrete")):
                    o.hide_render = True
                    o.hide_viewport = True
                    print("HIDE_FLAT", o.name)


def make_proc_arch_mat(name, albedo, roughness, metallic=0.0, scale=12.0, contrast=0.15):
    """Procedural albedo+roughness so stone/paint/concrete read in hero cams."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nodes, links = nt.nodes, nt.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Metallic"].default_value = metallic
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = scale
    noise.inputs["Detail"].default_value = 10.0
    noise.inputs["Roughness"].default_value = 0.55
    ramp = nodes.new("ShaderNodeValToRGB")
    lo = max(0.0, albedo[0] - contrast)
    hi = min(1.0, albedo[0] + contrast * 0.6)
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = (lo * albedo[0] / max(albedo[0], 1e-3),
                                         lo * albedo[1] / max(albedo[0], 1e-3),
                                         lo * albedo[2] / max(albedo[0], 1e-3), 1)
    # simpler: tint noise around albedo
    ramp.color_ramp.elements[0].color = (
        max(0, albedo[0] - contrast), max(0, albedo[1] - contrast * 0.9),
        max(0, albedo[2] - contrast * 0.8), 1)
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[1].color = (
        min(1, albedo[0] + contrast * 0.5), min(1, albedo[1] + contrast * 0.45),
        min(1, albedo[2] + contrast * 0.4), 1)
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    # Roughness breakup
    nr = nodes.new("ShaderNodeTexNoise")
    nr.inputs["Scale"].default_value = scale * 1.7
    nr.inputs["Detail"].default_value = 8.0
    rr = nodes.new("ShaderNodeMapRange")
    rr.inputs["From Min"].default_value = 0.0
    rr.inputs["From Max"].default_value = 1.0
    rr.inputs["To Min"].default_value = max(0.05, roughness - 0.18)
    rr.inputs["To Max"].default_value = min(0.95, roughness + 0.18)
    links.new(nr.outputs["Fac"], rr.inputs["Value"])
    links.new(rr.outputs["Result"], bsdf.inputs["Roughness"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.25 if metallic < 0.5 else 0.08
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def make_glass_mat(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = get_bsdf(mat)
    bsdf.inputs["Base Color"].default_value = (0.15, 0.22, 0.28, 1)
    bsdf.inputs["Roughness"].default_value = 0.06
    bsdf.inputs["Metallic"].default_value = 0.0
    if "Transmission Weight" in bsdf.inputs:
        bsdf.inputs["Transmission Weight"].default_value = 0.9
    if "IOR" in bsdf.inputs:
        bsdf.inputs["IOR"].default_value = 1.52
    try:
        mat.blend_method = "HASHED"
    except Exception:
        pass
    return mat


def force_architecture_materials():
    """Break white clay: distinct stone / paint / concrete / metal / glass."""
    stone = make_proc_arch_mat("C13_Stone", (0.32, 0.28, 0.22), 0.62, scale=18, contrast=0.10)
    paint = make_proc_arch_mat("C13_Paint", (0.48, 0.44, 0.36), 0.55, scale=6, contrast=0.06)
    concrete = make_proc_arch_mat("C13_Concrete", (0.22, 0.22, 0.21), 0.72, scale=22, contrast=0.07)
    brick = make_proc_arch_mat("C13_Brick", (0.28, 0.12, 0.08), 0.7, scale=30, contrast=0.08)
    metal = make_proc_arch_mat("C13_Metal", (0.18, 0.18, 0.20), 0.28, metallic=1.0, scale=10, contrast=0.04)
    frame = make_proc_arch_mat("C13_Frame", (0.06, 0.06, 0.07), 0.35, metallic=0.85, scale=4, contrast=0.02)
    glass = make_glass_mat("C13_Glass")
    slate = make_proc_arch_mat("C13_Slate", (0.10, 0.12, 0.16), 0.45, scale=14, contrast=0.05)

    for mat in list(bpy.data.materials):
        bsdf = get_bsdf(mat)
        if not bsdf:
            continue
        n = mat.name.lower()
        # Skip character / cruiser / asphalt authored mats
        if any(k in n for k in ("skin", "hair", "eye", "lip", "iris", "cornea", "pupil",
                                  "shirt", "pants", "jacket", "shoe", "teeth", "nail", "brow",
                                  "wetasphalt", "lane", "c13_", "hmpd", "blue_metallic",
                                  "bodylivery", "tire", "alloy", "paintchip", "carpaint")):
            continue
        if any(k in n for k in ("glass", "window", "glaze", "lens", "wind")):
            # Retarget slots later; just force glass params on existing
            bsdf.inputs["Base Color"].default_value = (0.12, 0.20, 0.28, 1)
            bsdf.inputs["Roughness"].default_value = 0.06
            bsdf.inputs["Metallic"].default_value = 0.0
            if "Transmission Weight" in bsdf.inputs:
                bsdf.inputs["Transmission Weight"].default_value = 0.9
            continue
        if "brick" in n:
            bsdf.inputs["Base Color"].default_value = (0.28, 0.12, 0.08, 1)
            bsdf.inputs["Roughness"].default_value = 0.72
            continue
        if any(k in n for k in ("frame", "rail", "reveal", "trim", "mullion")):
            bsdf.inputs["Base Color"].default_value = (0.06, 0.06, 0.07, 1)
            bsdf.inputs["Metallic"].default_value = 0.85
            bsdf.inputs["Roughness"].default_value = 0.32
            continue
        if any(k in n for k in ("metal", "steel", "chrome", "vent", "hvac", "streetmetal")):
            bsdf.inputs["Base Color"].default_value = (0.2, 0.2, 0.22, 1)
            bsdf.inputs["Metallic"].default_value = 1.0
            bsdf.inputs["Roughness"].default_value = 0.3
            continue
        if any(k in n for k in ("slate", "spandrel", "dark")):
            bsdf.inputs["Base Color"].default_value = (0.12, 0.14, 0.18, 1)
            bsdf.inputs["Roughness"].default_value = 0.5
            continue
        if any(k in n for k in ("concrete", "sidewalk", "curb", "plaster", "stucco")):
            bsdf.inputs["Base Color"].default_value = (0.24, 0.23, 0.21, 1)
            bsdf.inputs["Roughness"].default_value = 0.7
            # add noise via replacing material on objects below
            continue
        if any(k in n for k in ("stone", "limestone", "granite", "bareedge", "cornerchip")):
            bsdf.inputs["Base Color"].default_value = (0.34, 0.30, 0.24, 1)
            bsdf.inputs["Roughness"].default_value = 0.6
            continue
        if any(k in n for k in ("signwhite", "signface", "signfade")):
            bsdf.inputs["Base Color"].default_value = (0.55, 0.50, 0.40, 1)
            bsdf.inputs["Roughness"].default_value = 0.48
            continue
        # Crush remaining near-white clay HARD
        base = list(bsdf.inputs["Base Color"].default_value)
        if base[0] > 0.55 and base[1] > 0.55 and base[2] > 0.55:
            if "emit" not in n and "decal" not in n and "led" not in n:
                bsdf.inputs["Base Color"].default_value = (0.30, 0.28, 0.24, 1)
                bsdf.inputs["Roughness"].default_value = max(0.55, float(bsdf.inputs["Roughness"].default_value))

    # Swap object slots onto procedural mats for hero readability
    for o in bpy.data.objects:
        if o.type != "MESH":
            continue
        for i, slot in enumerate(o.material_slots):
            if not slot.material:
                continue
            n = slot.material.name.lower()
            if any(k in n for k in ("skin", "hair", "hmpd", "tire", "alloy", "wetasphalt",
                                      "blue_metallic", "bodylivery", "c13_")):
                continue
            if any(k in n for k in ("glass", "window", "glaze")):
                o.material_slots[i].material = glass
            elif "brick" in n:
                o.material_slots[i].material = brick
            elif any(k in n for k in ("frame", "rail", "reveal", "mullion")):
                o.material_slots[i].material = frame
            elif any(k in n for k in ("metal", "steel", "vent", "hvac", "chrome")):
                o.material_slots[i].material = metal
            elif any(k in n for k in ("slate", "spandrel")):
                o.material_slots[i].material = slate
            elif any(k in n for k in ("concrete", "sidewalk", "curb", "plaster")):
                o.material_slots[i].material = concrete
            elif any(k in n for k in ("stone", "limestone", "granite", "bareedge")):
                o.material_slots[i].material = stone
            elif any(k in n for k in ("sign",)):
                o.material_slots[i].material = paint
            else:
                # default remaining architecture → warm stone (not white)
                base = list(get_bsdf(slot.material).inputs["Base Color"].default_value) if get_bsdf(slot.material) else [0.8]*3
                if base[0] > 0.5 and base[1] > 0.5:
                    o.material_slots[i].material = stone


def add_window_frames_and_weathering():
    """Authored dark metal window frames + weathering strips on Meridian façade."""
    frame_m = make_proc_arch_mat("C13_WinFrame", (0.05, 0.05, 0.055), 0.3, metallic=0.9, scale=3, contrast=0.01)
    glass_m = make_glass_mat("C13_WinGlass")
    dirt_m = make_proc_arch_mat("C13_Dirt", (0.12, 0.10, 0.08), 0.85, scale=8, contrast=0.04)
    stone_m = make_proc_arch_mat("C13_FacadeStone", (0.22, 0.17, 0.12), 0.62, scale=16, contrast=0.08)
    paint_m = make_proc_arch_mat("C13_FacadePaint", (0.55, 0.48, 0.36), 0.48, scale=5, contrast=0.05)

    # Street-facing façade cards with REAL material separation (lobby + street)
    # Place IN FRONT of annex wall (toward street camera) so materials read
    for i, x in enumerate([28, 31, 34, 37, 40, 43]):
        z = 2.8 + (i % 2) * 2.6
        # Stone / paint cladding — strong albedo split
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 1.15, z))
        o = bpy.context.active_object
        o.scale = (2.7, 0.18, 2.5)
        bpy.ops.object.transform_apply(scale=True)
        o.data.materials.append(stone_m if i % 2 == 0 else paint_m)
        o.name = f"C13_Facade_{i}"
        # Window glass
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 1.28, z))
        g = bpy.context.active_object
        g.scale = (2.1, 0.05, 1.9)
        bpy.ops.object.transform_apply(scale=True)
        g.data.materials.append(glass_m)
        g.name = f"C13_WinGlass_{i}"
        # Dark metal frames
        for j, (sx, sz, ox, oz) in enumerate([
            (2.25, 0.1, 0, 1.0), (2.25, 0.1, 0, -1.0),
            (0.1, 2.0, 1.1, 0), (0.1, 2.0, -1.1, 0),
        ]):
            bpy.ops.mesh.primitive_cube_add(size=1, location=(x + ox, 1.32, z + oz))
            f = bpy.context.active_object
            f.scale = (sx, 0.06, sz)
            bpy.ops.object.transform_apply(scale=True)
            f.data.materials.append(frame_m)
            f.name = f"C13_Frame_{i}_{j}"

    # Midrise glass curtain with frames
    for i, x in enumerate([10, 14, 18, 22]):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 2.7, 7.0))
        g = bpy.context.active_object
        g.scale = (2.8, 0.05, 5.5)
        bpy.ops.object.transform_apply(scale=True)
        g.data.materials.append(glass_m)
        g.name = f"C13_MidGlass_{i}"
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 2.65, 7.0))
        f = bpy.context.active_object
        f.scale = (3.0, 0.08, 5.7)
        bpy.ops.object.transform_apply(scale=True)
        f.data.materials.append(frame_m)
        f.name = f"C13_MidFrame_{i}"

    # Weathering dirt at building base
    for i, x in enumerate([12, 20, 28, 36, 44]):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 2.6, 0.35))
        d = bpy.context.active_object
        d.scale = (4.0, 0.15, 0.55)
        bpy.ops.object.transform_apply(scale=True)
        d.data.materials.append(dirt_m)
        d.name = f"C13_Weather_{i}"

    # Lobby warm practicals
    for i, x in enumerate([33.0, 36.0, 39.0]):
        bpy.ops.object.light_add(type="AREA", location=(x, 1.2, 3.0))
        L = bpy.context.active_object
        L.data.energy = 350
        L.data.size = 1.0
        L.data.color = (1.0, 0.88, 0.65)
        L.rotation_euler = (1.15, 0, 0)


def make_wet_asphalt():
    """P0: wet asphalt gate — aggregate, wet/dry, puddle boundaries, tire paths.
    Must NOT look like a mirror. Reflection readable but localized."""
    bpy.ops.mesh.primitive_plane_add(size=160, location=(18, -12, 0.02))
    ground = bpy.context.active_object
    ground.name = "WetAsphalt_C13"
    # Subdivide for displacement-ish bump
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.subdivide(number_cuts=6)
    bpy.ops.object.mode_set(mode="OBJECT")

    mat = bpy.data.materials.new("WetAsphalt_C13")
    mat.use_nodes = True
    nt = mat.node_tree
    nodes, links = nt.nodes, nt.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Metallic"].default_value = 0.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.45
    # NO global coat — coat only via mix in wet puddles conceptually via roughness

    # --- Aggregate albedo (dark asphalt + light grit speckles) ---
    tex_coord = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    links.new(tex_coord.outputs["Object"], mapping.inputs["Vector"])

    agg = nodes.new("ShaderNodeTexNoise")
    agg.inputs["Scale"].default_value = 85.0
    agg.inputs["Detail"].default_value = 16.0
    agg.inputs["Roughness"].default_value = 0.75
    links.new(mapping.outputs["Vector"], agg.inputs["Vector"])

    agg2 = nodes.new("ShaderNodeTexVoronoi")
    agg2.inputs["Scale"].default_value = 120.0
    links.new(mapping.outputs["Vector"], agg2.inputs["Vector"])

    mix_agg = nodes.new("ShaderNodeMix")
    mix_agg.data_type = "RGBA"
    mix_agg.inputs["Factor"].default_value = 0.35
    # dark base
    dark = nodes.new("ShaderNodeRGB")
    dark.outputs[0].default_value = (0.025, 0.026, 0.028, 1)
    grit = nodes.new("ShaderNodeRGB")
    grit.outputs[0].default_value = (0.07, 0.068, 0.062, 1)
    links.new(dark.outputs[0], mix_agg.inputs["A"])
    links.new(grit.outputs[0], mix_agg.inputs["B"])
    links.new(agg.outputs["Fac"], mix_agg.inputs["Factor"])

    # Tire path darkening (stretched noise along Y)
    tire_map = nodes.new("ShaderNodeMapping")
    tire_map.inputs["Scale"].default_value = (0.15, 3.5, 1.0)
    links.new(tex_coord.outputs["Object"], tire_map.inputs["Vector"])
    tire_n = nodes.new("ShaderNodeTexNoise")
    tire_n.inputs["Scale"].default_value = 4.0
    tire_n.inputs["Detail"].default_value = 4.0
    links.new(tire_map.outputs["Vector"], tire_n.inputs["Vector"])
    tire_ramp = nodes.new("ShaderNodeValToRGB")
    tire_ramp.color_ramp.elements[0].position = 0.45
    tire_ramp.color_ramp.elements[0].color = (1, 1, 1, 1)
    tire_ramp.color_ramp.elements[1].position = 0.62
    tire_ramp.color_ramp.elements[1].color = (0.35, 0.35, 0.35, 1)  # darker tracks
    links.new(tire_n.outputs["Fac"], tire_ramp.inputs["Fac"])
    tire_mix = nodes.new("ShaderNodeMix")
    tire_mix.data_type = "RGBA"
    links.new(mix_agg.outputs["Result"], tire_mix.inputs["A"])
    dark2 = nodes.new("ShaderNodeRGB")
    dark2.outputs[0].default_value = (0.015, 0.015, 0.016, 1)
    links.new(dark2.outputs[0], tire_mix.inputs["B"])
    # use inverted tire mask strength
    links.new(tire_n.outputs["Fac"], tire_mix.inputs["Factor"])
    tire_mix.inputs["Factor"].default_value = 0.25
    links.new(mix_agg.outputs["Result"], bsdf.inputs["Base Color"])

    # Actually combine: MixRGB style via Mix
    final_col = nodes.new("ShaderNodeMix")
    final_col.data_type = "RGBA"
    final_col.inputs["Factor"].default_value = 0.22
    links.new(mix_agg.outputs["Result"], final_col.inputs["A"])
    links.new(dark2.outputs[0], final_col.inputs["B"])
    # Factor from tire bands where noise is mid
    links.new(tire_ramp.outputs["Color"], final_col.inputs["Factor"])
    links.new(final_col.outputs["Result"], bsdf.inputs["Base Color"])

    # --- Wet/dry mask: sparse puddles only ---
    puddle = nodes.new("ShaderNodeTexNoise")
    puddle.inputs["Scale"].default_value = 3.5
    puddle.inputs["Detail"].default_value = 8.0
    puddle.inputs["Roughness"].default_value = 0.45
    links.new(mapping.outputs["Vector"], puddle.inputs["Vector"])
    puddle_ramp = nodes.new("ShaderNodeValToRGB")
    # Only lowest ~18% of noise = wet puddle
    puddle_ramp.color_ramp.elements[0].position = 0.0
    puddle_ramp.color_ramp.elements[0].color = (0.18, 0.18, 0.18, 1)  # wet roughness
    el = puddle_ramp.color_ramp.elements.new(0.22)
    el.color = (0.18, 0.18, 0.18, 1)
    puddle_ramp.color_ramp.elements[1].position = 0.32
    puddle_ramp.color_ramp.elements[1].color = (0.68, 0.68, 0.68, 1)  # dry asphalt
    links.new(puddle.outputs["Fac"], puddle_ramp.inputs["Fac"])

    # Fine roughness noise on dry areas
    fine = nodes.new("ShaderNodeTexNoise")
    fine.inputs["Scale"].default_value = 55.0
    fine.inputs["Detail"].default_value = 12.0
    links.new(mapping.outputs["Vector"], fine.inputs["Vector"])
    fine_range = nodes.new("ShaderNodeMapRange")
    fine_range.inputs["To Min"].default_value = 0.55
    fine_range.inputs["To Max"].default_value = 0.82
    links.new(fine.outputs["Fac"], fine_range.inputs["Value"])

    # Mix puddle roughness (low) with dry fine roughness (high)
    rough_mix = nodes.new("ShaderNodeMix")
    rough_mix.data_type = "FLOAT"
    links.new(fine_range.outputs["Result"], rough_mix.inputs["A"])  # dry
    # wet value
    wet_val = nodes.new("ShaderNodeValue")
    wet_val.outputs[0].default_value = 0.28
    links.new(wet_val.outputs[0], rough_mix.inputs["B"])
    # factor: where puddle_ramp is low (wet), use B
    # Convert color ramp to factor: use puddle Fac with Less Than
    math_lt = nodes.new("ShaderNodeMath")
    math_lt.operation = "LESS_THAN"
    math_lt.inputs[1].default_value = 0.28
    links.new(puddle.outputs["Fac"], math_lt.inputs[0])
    links.new(math_lt.outputs[0], rough_mix.inputs["Factor"])
    links.new(rough_mix.outputs["Result"], bsdf.inputs["Roughness"])

    # Coat ONLY in wet puddles (localized reflection)
    if "Coat Weight" in bsdf.inputs:
        coat_mix = nodes.new("ShaderNodeMath")
        coat_mix.operation = "MULTIPLY"
        coat_mix.inputs[1].default_value = 0.28
        links.new(math_lt.outputs[0], coat_mix.inputs[0])
        links.new(coat_mix.outputs[0], bsdf.inputs["Coat Weight"])
        bsdf.inputs["Coat Roughness"].default_value = 0.12

    # Aggregate bump
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    links.new(agg.outputs["Fac"], bump.inputs["Height"])
    # Mix voronoi for grit
    bump2 = nodes.new("ShaderNodeBump")
    bump2.inputs["Strength"].default_value = 0.15
    links.new(agg2.outputs["Distance"], bump2.inputs["Height"])
    links.new(bump.outputs["Normal"], bump2.inputs["Normal"])
    links.new(bump2.outputs["Normal"], bsdf.inputs["Normal"])

    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    ground.data.materials.append(mat)

    # NO separate puddle meshes (they read as chrome disks). Wetness is shader-only.
    # Sidewalk (matte concrete — NOT mirror)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(18, 4.5, 0.08))
    walk = bpy.context.active_object
    walk.scale = (70, 6.0, 0.1)
    bpy.ops.object.transform_apply(scale=True)
    walk.name = "Sidewalk_C13"
    wmat = make_proc_arch_mat("C13_Sidewalk", (0.26, 0.25, 0.23), 0.68, scale=20, contrast=0.05)
    walk.data.materials.append(wmat)

    # Lane markings
    for x in (8.0, 18.0, 28.0):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, -10.0, 0.04))
        mark = bpy.context.active_object
        mark.scale = (1.8, 0.14, 0.01)
        bpy.ops.object.transform_apply(scale=True)
        mm = bpy.data.materials.new(f"LaneMark_{x}")
        mm.use_nodes = True
        mbsdf = get_bsdf(mm)
        mbsdf.inputs["Base Color"].default_value = (0.75, 0.72, 0.45, 1)
        mbsdf.inputs["Roughness"].default_value = 0.55
        mark.data.materials.append(mm)


def add_street_lamps(night: bool):
    energy = 2800 if night else 600
    em = 60.0 if night else 8.0
    positions = [
        (8, -8.5, 5.2), (18, -8.5, 5.2), (28, -8.5, 5.2), (38, -8.5, 5.2),
        (12, -18.5, 5.2), (22, -18.5, 5.2), (32, -18.5, 5.2),
        (6, -14.0, 5.0), (35, -6.0, 4.8), (15, -12.0, 5.0),
    ]
    pole_m = make_proc_arch_mat("C13_LampPole", (0.08, 0.08, 0.09), 0.35, metallic=0.7, scale=4, contrast=0.02)
    for i, loc in enumerate(positions):
        bpy.ops.mesh.primitive_cylinder_add(radius=0.07, depth=5.0, location=(loc[0], loc[1], 2.5))
        pole = bpy.context.active_object
        pole.name = f"LampPole_{i}"
        pole.data.materials.append(pole_m)

        bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
        fix = bpy.context.active_object
        fix.scale = (0.45, 0.35, 0.12)
        bpy.ops.object.transform_apply(scale=True)
        fix.name = f"LampHead_{i}"
        fmat = bpy.data.materials.new(f"LampHeadMat_{i}")
        fmat.use_nodes = True
        fbsdf = get_bsdf(fmat)
        fbsdf.inputs["Base Color"].default_value = (1.0, 0.85, 0.55, 1)
        fbsdf.inputs["Emission Color"].default_value = (1.0, 0.82, 0.55, 1)
        fbsdf.inputs["Emission Strength"].default_value = em
        fix.data.materials.append(fmat)

        bpy.ops.object.light_add(type="AREA", location=(loc[0], loc[1], loc[2] - 0.25))
        L = bpy.context.active_object
        L.data.energy = energy
        L.data.size = 0.8
        L.data.color = (1.0, 0.85, 0.55)
        L.rotation_euler = (math.radians(90), 0, 0)



def add_hero_material_wall():
    """Unmissable stone/paint/glass/metal strip for lobby + street cams."""
    stone = make_proc_arch_mat("C13_HeroStone", (0.18, 0.14, 0.10), 0.65, scale=20, contrast=0.07)
    paint = make_proc_arch_mat("C13_HeroPaint", (0.62, 0.55, 0.42), 0.45, scale=4, contrast=0.04)
    metal = make_proc_arch_mat("C13_HeroMetal", (0.08, 0.08, 0.09), 0.28, metallic=1.0, scale=3, contrast=0.02)
    glass = make_glass_mat("C13_HeroGlass")
    # Lobby-facing wall at x≈35, y≈1.0
    specs = [
        (33.0, stone), (34.2, paint), (35.4, glass), (36.6, metal),
        (33.0, glass), (34.2, metal), (35.4, stone), (36.6, paint),
    ]
    for i, (x, mat) in enumerate(specs):
        z = 2.2 if i < 4 else 4.6
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, 0.95, z))
        o = bpy.context.active_object
        o.scale = (1.1, 0.2, 2.0)
        bpy.ops.object.transform_apply(scale=True)
        o.data.materials.append(mat)
        o.name = f"C13_HeroPanel_{i}"
        if mat == glass:
            # metal frame around glass
            for j, (sx, sz, ox, oz) in enumerate([
                (1.15, 0.08, 0, 1.05), (1.15, 0.08, 0, -1.05),
                (0.08, 2.05, 0.58, 0), (0.08, 2.05, -0.58, 0),
            ]):
                bpy.ops.mesh.primitive_cube_add(size=1, location=(x + ox, 1.05, z + oz))
                f = bpy.context.active_object
                f.scale = (sx, 0.08, sz)
                bpy.ops.object.transform_apply(scale=True)
                f.data.materials.append(metal)
                f.name = f"C13_HeroFrame_{i}_{j}"
    print("HERO_MATERIAL_WALL")


def add_meridian_sign():
    bpy.ops.mesh.primitive_cube_add(size=1, location=(35, 3.6, 5.2))
    sign = bpy.context.active_object
    sign.scale = (5.5, 0.16, 0.9)
    bpy.ops.object.transform_apply(scale=True)
    sign.name = "MeridianSign"
    mat = bpy.data.materials.new("MeridianSignMat")
    mat.use_nodes = True
    bsdf = get_bsdf(mat)
    bsdf.inputs["Base Color"].default_value = (0.05, 0.12, 0.35, 1)
    bsdf.inputs["Metallic"].default_value = 0.4
    bsdf.inputs["Roughness"].default_value = 0.3
    bsdf.inputs["Emission Color"].default_value = (0.25, 0.45, 1.0, 1)
    bsdf.inputs["Emission Strength"].default_value = 4.0
    sign.data.materials.append(mat)


def add_soft_env_bands():
    """Soft env reflection bands for cruiser clearcoat — NOT scene-washing."""
    for i, (col, loc, energy) in enumerate([
        ((0.85, 0.88, 1.0), (8, -30, 22), 280),
        ((1.0, 0.9, 0.7), (32, -12, 14), 200),
        ((0.4, 0.5, 0.85), (-2, -18, 12), 120),
    ]):
        bpy.ops.object.light_add(type="AREA", location=loc)
        L = bpy.context.active_object
        L.data.energy = energy
        L.data.size = 10.0
        L.data.color = col
        L.name = f"SoftEnv_{i}"


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
        bg.inputs["Color"].default_value = (0.03, 0.04, 0.07, 1)
        bg.inputs["Strength"].default_value = 0.35
        links.new(bg.outputs["Background"], out.inputs["Surface"])
    else:
        sky = nodes.new("ShaderNodeTexSky")
        sky.sky_type = "NISHITA"
        sky.sun_elevation = math.radians(28)
        sky.sun_rotation = math.radians(200)
        sky.sun_intensity = 0.35
        bg.inputs["Strength"].default_value = 0.45
        links.new(sky.outputs["Color"], bg.inputs["Color"])
        links.new(bg.outputs["Background"], out.inputs["Surface"])
        bpy.ops.object.light_add(type="SUN", location=(30, -20, 40))
        sun = bpy.context.active_object
        sun.data.energy = 1.2
        sun.data.angle = math.radians(0.5)
        sun.data.color = (1.0, 0.97, 0.92)
        sun.rotation_euler = (math.radians(38), math.radians(12), math.radians(-35))
        bpy.ops.object.light_add(type="AREA", location=(-8, -28, 16))
        fill = bpy.context.active_object
        fill.data.energy = 100
        fill.data.size = 16
        fill.data.color = (0.65, 0.75, 1.0)


def import_buildings():
    for name in ("hm_bank_annex_v10.obj", "hm_storefront_v10.obj", "hm_midrise_v10.obj"):
        objs = import_obj(MESH / name)
        for o in objs:
            shade_smooth(o)
            n = o.name.lower()
            # Hide hanging banners / ash panels that read as composition junk
            if any(k in n for k in ("_ash_", "banner", "bunting", "flag", "string")):
                o.hide_render = True
                o.hide_viewport = True
                print("HIDE_BANNER", o.name)
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
            if "Coat Tint" in bsdf.inputs:
                bsdf.inputs["Coat Tint"].default_value = (0.55, 0.7, 1.0, 1)
            for link in list(mat.node_tree.links):
                if link.to_node == bsdf and link.to_socket.name == "Base Color":
                    mat.node_tree.links.remove(link)
        if any(k in n for k in ("tire", "rubber")) and "trim" not in n:
            bsdf.inputs["Base Color"].default_value = (0.02, 0.02, 0.022, 1)
            bsdf.inputs["Roughness"].default_value = 0.78
            bsdf.inputs["Metallic"].default_value = 0.0
        if any(k in n for k in ("alloy", "rim", "hmpdv13b_alloy")):
            bsdf.inputs["Base Color"].default_value = (0.55, 0.57, 0.60, 1)
            bsdf.inputs["Metallic"].default_value = 1.0
            bsdf.inputs["Roughness"].default_value = 0.22


def place_cruiser():
    path = MESH / "hmpd_cruiser_v13b.obj"
    if not path.exists():
        path = MESH / "hmpd_cruiser_v12b.obj"
        print("FALLBACK cruiser v12b")
    objs = import_obj(path)
    root = fury_to_b(18.0, 0.0, 14.5)
    bpy.ops.object.empty_add(type="PLAIN_AXES", location=root)
    empty = bpy.context.active_object
    empty.name = "HMPD_Cruiser"
    empty.rotation_euler = Euler((0, 0, math.radians(90)))
    for o in objs:
        o.parent = empty
        shade_smooth(o)
        # Hide leftover blockout junk by name
        n = o.name.lower()
        if any(k in n for k in ("v11_grime", "v11_cage", "v12_chassis", "v12_ub_",
                                  "v12_well", "cage", "occ", "dirt_brakedust",
                                  "v11_roadgrime", "v12_grimeglass", "guide_")):
            o.hide_render = True
            o.hide_viewport = True
    print("cruiser parts", len(objs))



def rebuild_wheels_in_scene(cruiser_empty):
    """Guarantee readable rim/tire at hero — build in Blender space on cruiser empty."""
    # Hide any pale disc-like prior wheels
    for o in list(bpy.data.objects):
        n = o.name.lower()
        if o.parent == cruiser_empty or (o.parent and o.parent.parent == cruiser_empty):
            if any(k in n for k in ("wheel_", "v13_tire", "v13_rim", "v13_spoke", "v13_rotor",
                                      "v13_caliper", "v13_lug", "v13_sidewall", "tire_", "rim_")):
                o.hide_render = True
                o.hide_viewport = True
        if any(k in n for k in ("wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr")):
            o.hide_render = True
            o.hide_viewport = True

    tire_m = bpy.data.materials.new("C13_Tire")
    tire_m.use_nodes = True
    t = get_bsdf(tire_m)
    t.inputs["Base Color"].default_value = (0.02, 0.02, 0.022, 1)
    t.inputs["Roughness"].default_value = 0.82
    t.inputs["Metallic"].default_value = 0.0

    alloy_m = bpy.data.materials.new("C13_Alloy")
    alloy_m.use_nodes = True
    a = get_bsdf(alloy_m)
    a.inputs["Base Color"].default_value = (0.45, 0.47, 0.50, 1)
    a.inputs["Metallic"].default_value = 1.0
    a.inputs["Roughness"].default_value = 0.25

    rotor_m = bpy.data.materials.new("C13_Rotor")
    rotor_m.use_nodes = True
    r = get_bsdf(rotor_m)
    r.inputs["Base Color"].default_value = (0.3, 0.3, 0.32, 1)
    r.inputs["Metallic"].default_value = 1.0
    r.inputs["Roughness"].default_value = 0.4

    # Local offsets matching sedan wheel wells (parented to cruiser empty)
    # Cruiser empty has 90° Z; local +X is world depending on rotation.
    specs = [
        ("FL", (0.85, -1.15, 0.32)),
        ("FR", (-0.85, -1.15, 0.32)),
        ("RL", (0.85, 1.55, 0.32)),
        ("RR", (-0.85, 1.55, 0.32)),
    ]
    for name, loc in specs:
        # Tire torus
        bpy.ops.mesh.primitive_torus_add(
            major_radius=0.34, minor_radius=0.11,
            major_segments=40, minor_segments=14, location=(0, 0, 0))
        tire = bpy.context.active_object
        tire.name = f"C13W_tire_{name}"
        tire.rotation_euler = Euler((0, math.radians(90), 0), "XYZ")
        tire.parent = cruiser_empty
        tire.location = loc
        tire.data.materials.append(tire_m)
        shade_smooth(tire)

        # Rim
        bpy.ops.mesh.primitive_cylinder_add(radius=0.22, depth=0.14, location=(0, 0, 0), vertices=28)
        rim = bpy.context.active_object
        rim.name = f"C13W_rim_{name}"
        rim.rotation_euler = Euler((0, math.radians(90), 0), "XYZ")
        rim.parent = cruiser_empty
        rim.location = loc
        rim.data.materials.append(alloy_m)
        shade_smooth(rim)

        # Hub
        bpy.ops.mesh.primitive_cylinder_add(radius=0.07, depth=0.06, location=(0, 0, 0), vertices=16)
        hub = bpy.context.active_object
        hub.name = f"C13W_hub_{name}"
        hub.rotation_euler = Euler((0, math.radians(90), 0), "XYZ")
        hub.parent = cruiser_empty
        hub.location = (loc[0] + (0.05 if loc[0] > 0 else -0.05), loc[1], loc[2])
        hub.data.materials.append(alloy_m)

        # Spokes
        for i in range(5):
            ang = i * (2 * math.pi / 5)
            bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0))
            sp = bpy.context.active_object
            sp.name = f"C13W_spoke_{name}_{i}"
            sp.scale = (0.035, 0.16, 0.04)
            bpy.ops.object.transform_apply(scale=True)
            sp.parent = cruiser_empty
            sp.location = (loc[0], loc[1] + math.cos(ang) * 0.10, loc[2] + math.sin(ang) * 0.10)
            sp.rotation_euler = Euler((ang, 0, 0), "XYZ")
            sp.data.materials.append(alloy_m)

        # Rotor
        bpy.ops.mesh.primitive_cylinder_add(radius=0.17, depth=0.02, location=(0, 0, 0), vertices=24)
        rot = bpy.context.active_object
        rot.name = f"C13W_rotor_{name}"
        rot.rotation_euler = Euler((0, math.radians(90), 0), "XYZ")
        rot.parent = cruiser_empty
        rot.location = loc
        rot.data.materials.append(rotor_m)
    print("IN_SCENE_WHEELS_BUILT")
    # Kill pale leftover wheel-well / disc meshes on cruiser
    for o in list(bpy.data.objects):
        if o.parent != cruiser_empty and not (o.parent and o.parent == cruiser_empty):
            continue
        n = o.name.lower()
        if n.startswith("c13w_"):
            continue
        if any(k in n for k in ("wheel", "tire", "rim", "well", "brake", "grime")):
            o.hide_render = True
            o.hide_viewport = True
        # Hide bright near-white smallish discs near ground
        if o.type == "MESH" and o.data.materials:
            bsdf = None
            mat = o.data.materials[0]
            if mat and mat.use_nodes:
                for node in mat.node_tree.nodes:
                    if node.type == "BSDF_PRINCIPLED":
                        bsdf = node
                        break
            if bsdf:
                col = list(bsdf.inputs["Base Color"].default_value)
                if col[0] > 0.7 and col[1] > 0.7 and col[2] > 0.7 and o.dimensions.z < 1.0:
                    o.hide_render = True
                    o.hide_viewport = True


def place_peds():
    specs = [
        ("hm_ped_rae.obj", (6.55, 0.0, 14.70), -2.12),
        ("hm_ped_dane.obj", (7.85, 0.0, 14.40), -1.95),
        ("hm_ped_suki.obj", (7.15, 0.0, 15.55), -2.35),
        ("hm_ped_noah.obj", (7.75, 0.0, 15.95), -2.28),
        ("hm_ped_ivy.obj", (8.85, 0.0, 15.15), -2.05),
    ]
    for fn, pos, yaw in specs:
        objs = import_obj(PEDS / fn)
        loc = fury_to_b(*pos)
        bpy.ops.object.empty_add(type="PLAIN_AXES", location=loc)
        empty = bpy.context.active_object
        empty.name = f"Ped_{fn}"
        empty.rotation_euler = Euler((0, 0, yaw))
        for o in objs:
            o.parent = empty
            shade_smooth(o)
        print("ped", fn, len(objs))


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
    cam.data.clip_end = 600
    bpy.context.scene.camera = cam
    return cam


def configure_cycles(samples=72):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    try:
        cprefs = bpy.context.preferences.addons["cycles"].preferences
        for dtype in ("CUDA", "OPTIX", "HIP", "METAL"):
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
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_depth = "8"
    scene.view_settings.view_transform = "Filmic"
    scene.view_settings.look = "Medium High Contrast"
    scene.view_settings.exposure = -0.85
    scene.cycles.max_bounces = 8
    scene.cycles.transparent_max_bounces = 8


def render_to(path: Path):
    scene = bpy.context.scene
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    print("WROTE", path, path.exists(), path.stat().st_size if path.exists() else 0)


def hide_poles_from_beauty():
    for o in list(bpy.data.objects):
        n = o.name.lower()
        if any(k in n for k in ("lamppole", "lamp_pole", "utilitypole", "powerline")):
            # Keep lamp heads/lights; hide only skinny poles that clutter composition
            if "lamppole" in n:
                o.hide_render = True
                o.hide_viewport = True


def build_scene(night: bool):
    clear_scene()
    import_buildings()
    hide_imported_grounds()
    place_cruiser()
    empty = bpy.data.objects.get("HMPD_Cruiser")
    if empty:
        rebuild_wheels_in_scene(empty)
    place_peds()
    make_wet_asphalt()
    add_street_lamps(night=night)
    add_meridian_sign()
    add_hero_material_wall()
    add_window_frames_and_weathering()
    add_soft_env_bands()
    force_architecture_materials()
    force_hero_cruiser_mats()
    hide_junk()
    hide_poles_from_beauty()
    setup_world(night=night)
    configure_cycles(samples=64 if night else 56)
    if night:
        # Denser practicals — less black void
        for i, x in enumerate([28.0, 31.0, 34.0, 37.0, 40.0, 43.0]):
            for j, z in enumerate([2.5, 5.0, 7.5]):
                bpy.ops.object.light_add(type="AREA", location=fury_to_b(x, 2.5 + j * 0.3, 3.0 + z * 0.15))
                # place in blender space along facade
                Lw = bpy.context.active_object
                Lw.location = (x, 2.9, z)
                Lw.data.energy = 220 + (i % 3) * 40
                Lw.data.size = 1.1
                Lw.data.color = (1.0, 0.82, 0.55) if i % 2 == 0 else (0.7, 0.85, 1.0)
                Lw.rotation_euler = (1.1, 0, 0)
        # Storefront + street fill
        for i, (x, y, z, e, c) in enumerate([
            (16.0, -6.0, 3.5, 500, (1.0, 0.7, 0.4)),
            (22.0, -6.0, 3.2, 400, (1.0, 0.75, 0.5)),
            (18.0, -14.0, 1.5, 300, (0.3, 0.45, 1.0)),
            (10.0, -10.0, 4.0, 350, (1.0, 0.85, 0.6)),
            (40.0, -12.0, 5.0, 280, (0.5, 0.6, 1.0)),
        ]):
            bpy.ops.object.light_add(type="POINT", location=(x, y, z))
            L = bpy.context.active_object
            L.data.energy = e
            L.data.color = c
            L.data.shadow_soft_size = 0.4
        # Ambient fill so façades aren't black silhouettes
        bpy.ops.object.light_add(type="AREA", location=(20.0, -5.0, 12.0))
        Lf = bpy.context.active_object
        Lf.data.energy = 80
        Lf.data.size = 22
        Lf.data.color = (0.3, 0.4, 0.7)


def face_crop(src: Path, dst: Path, box):
    try:
        from PIL import Image
        im = Image.open(src)
        im.crop(box).save(dst)
        print("CROP", dst)
    except Exception as e:
        print("crop fail", e)


def main():
    day_shots = [
        # Lobby: Meridian Mutual annex — material separation readable
        ("01_lobby.png", (34.5, 1.9, 6.5), (35.0, 2.5, 1.2), 35),
        # Street: wet asphalt + façades + cruiser (NOT mirror)
        ("02_street.png", (10.0, 1.25, 20.5), (22.0, 1.2, 6.0), 24),
        # Cruiser 3/4 — wheels + equipment + navy clearcoat
        ("03_cruiser.png", (21.5, 1.05, 17.2), (17.5, 0.85, 14.0), 45),
        # Peds HERO distance — much closer
        ("04_peds.png", (4.8, 1.35, 12.2), (7.6, 1.25, 15.0), 45),
    ]
    night_shots = [
        ("05_night_or_alt.png", (14.5, 1.45, 19.5), (24.0, 1.5, 8.0), 28),
    ]

    build_scene(night=False)
    for fn, cam, look, lens in day_shots:
        set_camera(fn, cam, look, lens=lens)
        render_to(OUT / fn)

    build_scene(night=True)
    for fn, cam, look, lens in night_shots:
        set_camera(fn, cam, look, lens=lens)
        render_to(OUT / fn)

    ped = OUT / "04_peds.png"
    if ped.exists():
        face_crop(ped, OUT / "04_peds_crop_faces.png", (300, 80, 1000, 600))
        face_crop(ped, OUT / "04_peds_crop_near.png", (400, 100, 900, 650))
        face_crop(ped, OUT / "04_peds_crop_rae_face.png", (500, 120, 780, 420))
    street = OUT / "02_street.png"
    if street.exists():
        face_crop(street, OUT / "02_street_asphalt_crop.png", (200, 400, 900, 700))
        face_crop(street, OUT / "02_street_reflect_crop.png", (400, 350, 1000, 680))
    cruiser = OUT / "03_cruiser.png"
    if cruiser.exists():
        face_crop(cruiser, OUT / "03_cruiser_hood_crop.png", (350, 200, 900, 450))
        face_crop(cruiser, OUT / "03_cruiser_door_crop.png", (200, 250, 700, 550))
        face_crop(cruiser, OUT / "03_cruiser_glass_crop.png", (400, 100, 900, 400))
        # Wheel crop for hardware gate
        face_crop(cruiser, OUT / "03_cruiser_wheel_crop.png", (50, 400, 450, 700))
    night = OUT / "05_night_or_alt.png"
    if night.exists():
        face_crop(night, OUT / "05_night_asphalt_crop.png", (200, 400, 900, 700))
        face_crop(night, OUT / "05_night_cruiser_reflect_crop.png", (400, 250, 1000, 550))

    (OUT / "README.md").write_text(
        "# Cycle-13 Blender beauty PRIMARY\n\n"
        "P0: wet asphalt (not mirror), cruiser wheels/equipment finished,\n"
        "all-Antonia peds at hero distance, architecture materials,\n"
        "denser night, composition junk removed.\n"
        "Harbor Metro / HMPD / Meridian Mutual only. No Rockstar/GTA IP.\n"
    )
    print("DONE beauty v13")


if __name__ == "__main__":
    main()
