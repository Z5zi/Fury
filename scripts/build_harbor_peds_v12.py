#!/usr/bin/env python3
"""Harbor Metro Cycle-12 AAA pedestrians (Antonia.Polygon CC0 + Quaternius CC0).

rae/suki/ivy  = Antonia.Polygon continuous mesh + clothing + SSS + hair
dane/noah     = Quaternius Matt/Sam body + fabric overlays

No Rockstar/GTA IP.
"""
from __future__ import annotations

import math
import shutil
import sys
import traceback
from pathlib import Path

import bpy
from mathutils import Euler, Vector

ROOT = Path("/workspace/vaultline-blender")
sys.path.insert(0, str(ROOT / "scripts"))
from vl_common import OUT, RENDERS, apply_mat, clear_scene, shade_smooth  # noqa: E402

ANTONIA = ROOT / "refs/ped_bases/antonia/AntoniaA-1.2.obj"
MATT = ROOT / "refs/ped_bases/matt.glb"
SAM = ROOT / "refs/ped_bases/sam.glb"
FACES = ROOT / "refs/ped_faces"
FURY_PEDS = Path("/workspace/Fury/assets/meshes/harbor_metro/peds")
ART_PEDS = Path("/workspace/Fury/artifacts/aaa_meridian_block/blender_peds")
REND_PEDS = RENDERS / "peds_v12"
for _p in (OUT, REND_PEDS, FURY_PEDS, ART_PEDS, FACES):
    _p.mkdir(parents=True, exist_ok=True)

PEDS = [
    dict(name="rae", kind="antonia", height=1.72, pose="walk", jacket=True,
         skin=(0.86, 0.70, 0.58, 1.0), shirt=(0.14, 0.26, 0.48, 1.0),
         pants=(0.10, 0.12, 0.16, 1.0), hair=(0.08, 0.05, 0.04, 1.0),
         shoes=(0.05, 0.05, 0.06, 1.0), iris=(0.22, 0.32, 0.42, 1.0),
         face=FACES / "rae_face.png"),
    dict(name="dane", kind="matt", height=1.82, pose="idle", jacket=False,
         skin=(0.42, 0.28, 0.20, 1.0), shirt=(0.62, 0.14, 0.12, 1.0),
         pants=(0.08, 0.09, 0.11, 1.0), hair=(0.04, 0.03, 0.02, 1.0),
         shoes=(0.08, 0.06, 0.05, 1.0), iris=(0.28, 0.18, 0.10, 1.0),
         face=FACES / "dane_face.png"),
    dict(name="suki", kind="antonia", height=1.62, pose="converse_a", jacket=False,
         skin=(0.90, 0.78, 0.70, 1.0), shirt=(0.12, 0.46, 0.44, 1.0),
         pants=(0.18, 0.14, 0.28, 1.0), hair=(0.05, 0.03, 0.03, 1.0),
         shoes=(0.75, 0.75, 0.78, 1.0), iris=(0.28, 0.36, 0.26, 1.0),
         face=FACES / "suki_face.png"),
    dict(name="noah", kind="sam", height=1.74, pose="converse_b", jacket=True,
         skin=(0.72, 0.54, 0.42, 1.0), shirt=(0.22, 0.24, 0.30, 1.0),
         pants=(0.14, 0.18, 0.34, 1.0), hair=(0.10, 0.08, 0.06, 1.0),
         shoes=(0.06, 0.06, 0.07, 1.0), iris=(0.20, 0.28, 0.34, 1.0),
         face=FACES / "noah_face.png"),
    dict(name="ivy", kind="antonia", height=1.66, pose="lean", jacket=True,
         skin=(0.88, 0.72, 0.62, 1.0), shirt=(0.62, 0.36, 0.16, 1.0),
         pants=(0.32, 0.20, 0.14, 1.0), hair=(0.48, 0.22, 0.10, 1.0),
         shoes=(0.10, 0.05, 0.04, 1.0), iris=(0.32, 0.38, 0.22, 1.0),
         face=FACES / "ivy_face.png"),
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
    if transmission > 0.01:
        if "Transmission Weight" in bsdf.inputs:
            bsdf.inputs["Transmission Weight"].default_value = transmission
        try:
            m.blend_method = "HASHED"
        except Exception:
            pass
    if alpha < 0.999:
        bsdf.inputs["Alpha"].default_value = alpha
        try:
            m.blend_method = "HASHED"
        except Exception:
            pass
    return m


def make_mats(spec):
    skin = spec["skin"]
    warm = (min(1.0, skin[0] * 1.08), skin[1] * 0.85, skin[2] * 0.75, 1.0)
    mats = {
        "Skin": principled(f"{spec['name']}_Skin", skin, roughness=0.42, sss=0.35,
                           sss_radius=(1.0, 0.35, 0.2), specular=0.45),
        "SkinFace": principled(f"{spec['name']}_SkinFace", skin, roughness=0.38, sss=0.48,
                               sss_radius=(1.0, 0.28, 0.12), specular=0.5),
        "SkinWarm": principled(f"{spec['name']}_SkinWarm", warm, roughness=0.4, sss=0.4,
                               sss_radius=(1.0, 0.25, 0.1)),
        "Lip": principled(f"{spec['name']}_Lip",
                          (skin[0] * 0.9, skin[1] * 0.45, skin[2] * 0.48, 1),
                          roughness=0.35, sss=0.5, sss_radius=(1.0, 0.15, 0.08), coat=0.15),
        "Nail": principled(f"{spec['name']}_Nail", (0.85, 0.75, 0.72, 1), roughness=0.25, coat=0.3),
        "EyeWhite": principled(f"{spec['name']}_EyeWhite", (0.92, 0.93, 0.95, 1), roughness=0.25, sss=0.15),
        "Iris": principled(f"{spec['name']}_Iris", spec["iris"], roughness=0.18, specular=0.6),
        "Pupil": principled(f"{spec['name']}_Pupil", (0.01, 0.01, 0.015, 1), roughness=0.3),
        "Cornea": principled(f"{spec['name']}_Cornea", (1, 1, 1, 1), roughness=0.02,
                             transmission=0.95, specular=1.0),
        "Hair": principled(f"{spec['name']}_Hair", spec["hair"], roughness=0.35, specular=0.55, coat=0.1),
        "HairCard": principled(f"{spec['name']}_HairCard", spec["hair"], roughness=0.4, alpha=0.92),
        "Brow": principled(f"{spec['name']}_Brow",
                           (spec["hair"][0] * 0.8, spec["hair"][1] * 0.8, spec["hair"][2] * 0.8, 1),
                           roughness=0.55),
        "Shirt": principled(f"{spec['name']}_Shirt", spec["shirt"], roughness=0.72),
        "Pants": principled(f"{spec['name']}_Pants", spec["pants"], roughness=0.78),
        "Jacket": principled(f"{spec['name']}_Jacket",
                             (spec["shirt"][0] * 0.7, spec["shirt"][1] * 0.75, spec["shirt"][2] * 0.9, 1),
                             roughness=0.55, specular=0.25),
        "Shoes": principled(f"{spec['name']}_Shoes", spec["shoes"], roughness=0.55, coat=0.05),
        "Teeth": principled(f"{spec['name']}_Teeth", (0.95, 0.93, 0.88, 1), roughness=0.25, sss=0.1),
        "Invisible": principled(f"{spec['name']}_Invisible", (1, 1, 1, 1), alpha=0.0),
    }
    # Face portrait projection disabled in v12 (Antonia UVs are not portrait-aligned).
    return mats


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


def scale_to_height(obj, height):
    h = max(obj.dimensions.z, 1e-4)
    s = height / h
    obj.scale = (s, s, s)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)


def ensure_z_up(obj):
    """Antonia is Y-up (height on +Y). +90° X maps Y→Z so she stands feet-down."""
    if obj.dimensions.y > obj.dimensions.z * 1.25:
        obj.rotation_euler = Euler((math.radians(90.0), 0.0, 0.0), "XYZ")
        bpy.ops.object.select_all(action="DESELECT")
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)


def pose_offset(pose):
    table = {
        "walk": (math.radians(12), math.radians(4)),
        "idle": (math.radians(4), math.radians(2)),
        "converse_a": (math.radians(-12), math.radians(8)),
        "converse_b": (math.radians(10), math.radians(-6)),
        "lean": (math.radians(18), math.radians(10)),
    }
    return table.get(pose, (0.0, 0.0))


def add_fabric_clothes(body, mats, spec):
    """Fitted clothing shells — keep Antonia face/hands fully visible."""
    dim = body.dimensions
    cx, cy = body.location.x, body.location.y
    objs = []

    # Pants: cylinder from ankles to waist
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=32, radius=0.12, depth=dim.z * 0.38,
        location=(cx, cy + 0.01, dim.z * 0.26))
    pants = bpy.context.active_object
    pants.name = "Pants"
    pants.scale = (1.05, 0.78, 1.0)
    bpy.ops.object.transform_apply(scale=True)
    apply_mat(pants, mats["Pants"])
    shade_smooth(pants)
    objs.append(pants)

    # Shirt: short torso cylinder (not a sphere that swallows the head)
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=32, radius=0.15, depth=dim.z * 0.28,
        location=(cx, cy + 0.01, dim.z * 0.58))
    shirt = bpy.context.active_object
    shirt.name = "Shirt"
    shirt.scale = (1.0, 0.72, 1.0)
    bpy.ops.object.transform_apply(scale=True)
    apply_mat(shirt, mats["Shirt"])
    shade_smooth(shirt)
    solid = shirt.modifiers.new("Solidify", "SOLIDIFY")
    solid.thickness = 0.006
    objs.append(shirt)

    if spec.get("jacket"):
        bpy.ops.mesh.primitive_cylinder_add(
            vertices=28, radius=0.17, depth=dim.z * 0.32,
            location=(cx, cy + 0.015, dim.z * 0.57))
        jacket = bpy.context.active_object
        jacket.name = "Jacket"
        jacket.scale = (1.08, 0.78, 1.0)
        bpy.ops.object.transform_apply(scale=True)
        apply_mat(jacket, mats["Jacket"])
        shade_smooth(jacket)
        jsol = jacket.modifiers.new("Solidify", "SOLIDIFY")
        jsol.thickness = 0.01
        objs.append(jacket)

    for sx in (-0.08, 0.08):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(cx + sx, cy + 0.06, 0.035))
        sh = bpy.context.active_object
        sh.scale = (0.06, 0.13, 0.045)
        bpy.ops.object.transform_apply(scale=True)
        apply_mat(sh, mats["Shoes"])
        shade_smooth(sh)
        objs.append(sh)

    # Compact hair cap behind forehead
    bpy.ops.mesh.primitive_uv_sphere_add(
        segments=20, ring_count=12, radius=0.085,
        location=(cx, cy + 0.05, dim.z * 0.94))
    hair = bpy.context.active_object
    hair.name = "HairCap"
    hair.scale = (1.05, 0.9, 0.7)
    bpy.ops.object.transform_apply(scale=True)
    apply_mat(hair, mats["Hair"])
    shade_smooth(hair)
    objs.append(hair)

    # Few hair cards around back of skull only
    for i in range(10):
        ang = -0.6 + i / 9 * 1.2  # back hemisphere
        r = 0.08
        bpy.ops.mesh.primitive_cube_add(
            size=1,
            location=(cx + math.sin(ang) * r,
                      cy + 0.06 + math.cos(ang) * 0.02,
                      dim.z * 0.92 - 0.02 * (i % 3)))
        c = bpy.context.active_object
        c.scale = (0.012, 0.003, 0.06 + 0.015 * (i % 3))
        c.rotation_euler = (0.3, 0.0, ang)
        bpy.ops.object.transform_apply(scale=True, rotation=True)
        apply_mat(c, mats["HairCard"])
        objs.append(c)
    return objs



def import_antonia(spec, mats):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=str(ANTONIA))
    new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    body = new[0]
    body.name = f"Body_{spec['name']}"
    assign_antonia_mats(body, mats)
    shade_smooth(body)
    ensure_z_up(body)
    scale_to_height(body, spec["height"])
    ground_object(body)
    clothes = add_fabric_clothes(body, mats, spec)
    yaw, lean = pose_offset(spec["pose"])
    body.rotation_euler.z = yaw
    body.rotation_euler.x = lean * 0.12
    return [body] + clothes


def import_quaternius(spec, mats, path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before]
    keep = {"matt", "sam", "lis", "shaun", "shoulder"}
    body = None
    for o in list(new):
        n = o.name.lower().split(".")[0]
        if o.type == "MESH" and n in keep and n != "shoulder":
            body = o
        elif o.type == "MESH" and n not in keep:
            bpy.data.objects.remove(o, do_unlink=True)
    if body is None:
        meshes = [o for o in bpy.data.objects if o.type == "MESH"]
        body = max(meshes, key=lambda o: len(o.data.vertices))
    body.name = f"Body_{spec['name']}"
    for slot in body.material_slots:
        if slot.material and slot.material.use_nodes:
            for n in slot.material.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED":
                    n.inputs["Roughness"].default_value = 0.55
                    if "Subsurface Weight" in n.inputs:
                        n.inputs["Subsurface Weight"].default_value = 0.12
    ensure_z_up(body)
    scale_to_height(body, spec["height"])
    ground_object(body)
    shade_smooth(body)
    clothes = add_fabric_clothes(body, mats, spec)
    yaw, lean = pose_offset(spec["pose"])
    body.rotation_euler.z = yaw
    body.rotation_euler.x = lean * 0.12
    return [body] + clothes


def join_export(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    live = []
    for o in objs:
        if o and o.name in bpy.data.objects:
            o.select_set(True)
            live.append(o)
    bpy.context.view_layer.objects.active = live[0]
    if len(live) > 1:
        bpy.ops.object.join()
    root = live[0]
    root.name = f"hm_ped_{name}"
    return root


def render_hero(obj, name):
    for o in list(bpy.data.objects):
        if o.type == "LIGHT":
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.light_add(type="AREA", location=(1.5, -1.8, 1.6))
    L = bpy.context.active_object
    L.data.energy = 280
    L.data.size = 1.5
    bpy.ops.object.light_add(type="AREA", location=(-1.2, 1.0, 1.4))
    L2 = bpy.context.active_object
    L2.data.energy = 90
    L2.data.size = 1.0
    L2.data.color = (0.7, 0.8, 1.0)

    world = bpy.data.worlds.new(f"W_{name}")
    bpy.context.scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.16, 0.16, 0.18, 1)
    bg.inputs[1].default_value = 0.35

    bpy.ops.object.camera_add(location=(0.2, -1.85, 1.35))
    cam = bpy.context.active_object
    cam.rotation_euler = (Vector((0, 0, 1.15)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 55
    bpy.context.scene.camera = cam
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 40
    sc.cycles.use_denoising = True
    sc.render.resolution_x = 640
    sc.render.resolution_y = 896
    sc.view_settings.view_transform = "Filmic"
    sc.view_settings.look = "Medium High Contrast"
    sc.render.filepath = str(REND_PEDS / f"hm_ped_{name}_full.png")
    bpy.ops.render.render(write_still=True)

    bpy.ops.object.camera_add(location=(0.05, -0.62, 1.55))
    cam2 = bpy.context.active_object
    cam2.rotation_euler = (Vector((0, 0.02, 1.52)) - cam2.location).to_track_quat("-Z", "Y").to_euler()
    cam2.data.lens = 85
    bpy.context.scene.camera = cam2
    sc.render.resolution_x = 640
    sc.render.resolution_y = 640
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
    blend = OUT / f"hm_ped_{name}_v12.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    obj_path = OUT / f"hm_ped_{name}_v12.obj"
    bpy.ops.wm.obj_export(
        filepath=str(obj_path), export_selected_objects=True,
        forward_axis="NEGATIVE_Z", up_axis="Y")
    glb = OUT / f"hm_ped_{name}_v12.glb"
    bpy.ops.export_scene.gltf(filepath=str(glb), use_selection=True, export_format="GLB")
    for ext in (".obj", ".mtl"):
        src = OUT / f"hm_ped_{name}_v12{ext}"
        if src.exists():
            shutil.copy(src, FURY_PEDS / f"hm_ped_{name}{ext}")
    if glb.exists():
        shutil.copy(glb, FURY_PEDS / f"hm_ped_{name}_v12.glb")
    print("EXPORTED", name, "verts", len(obj.data.vertices))


def build_one(spec):
    clear_scene()
    mats = make_mats(spec)
    if spec["kind"] == "antonia":
        objs = import_antonia(spec, mats)
    elif spec["kind"] == "matt":
        objs = import_quaternius(spec, mats, MATT)
    else:
        objs = import_quaternius(spec, mats, SAM)
    root = join_export(objs, spec["name"])
    export_ped(root, spec["name"])
    render_hero(root, spec["name"])


def main():
    import os
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
        "# Harbor Metro peds v12 — Antonia.Polygon (CC0) + Quaternius\n\n"
        "rae/suki/ivy: Antonia.Polygon 1.2 continuous mesh + clothing + SSS.\n"
        "dane/noah: Quaternius Matt/Sam body + fabric overlays.\n"
        "Face albedo refs on SkinFace. No Rockstar/GTA IP.\n"
    )
    print("DONE peds v12")


if __name__ == "__main__":
    main()
