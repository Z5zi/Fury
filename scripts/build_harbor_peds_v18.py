#!/usr/bin/env python3
"""Cycle-18 Harbor Metro peds — MPFB2 / MakeHuman CC0 rebuild (Gate C P0).

Why: C17 (4.4) failed Gate C because hand-built Antonia garment shells and
hair cards produced triangular/needle spikes at beauty distance. C18 throws
that pipeline away and builds each ped from the MakeHuman CC0 system assets
via the MPFB2 Blender extension:
  * fitted, textured garments (diffuse + normal + AO) with proper topology
  * alpha-textured hair meshes (no solidify/displace spikes)
  * ENHANCED_SSS skin (per-slot face/lips/nails, pores, clearcoat)
  * procedural eyes (cornea/iris depth), eyebrows, eyelashes, teeth
  * default rig, natural idle pose authored per ped (arms relaxed, weight shift)

Outputs
  /workspace/vaultline-blender/assets/peds_v18/hm_ped_<name>_v18.blend (packed; beauty source)
  /workspace/Fury/assets/meshes/harbor_metro/peds/hm_ped_<name>_v18.glb (engine asset)
  /workspace/Fury/artifacts/aaa_meridian_block/blender_peds/hm_ped_<name>_{full,face,silhouette}.png

Usage: blender -b --python scripts/build_harbor_peds_v18.py [-- rae,dane] [--no-studio] [--glb-only]
Harbor Metro / HMPD / Meridian Mutual only. No Rockstar/GTA IP.
Licenses: MakeHuman system assets CC0 (makehumancommunity.org); MPFB2 GPL tool only.
"""
from __future__ import annotations

import math
import os
import sys
from pathlib import Path

import bpy
from mathutils import Vector, Matrix

from bl_ext.user_default.mpfb.services.humanservice import HumanService

BLEND_OUT = Path("/workspace/vaultline-blender/assets/peds_v18")
GLB_OUT = Path("/workspace/Fury/assets/meshes/harbor_metro/peds")
PROOF_OUT = Path("/workspace/Fury/artifacts/aaa_meridian_block/blender_peds")
for p in (BLEND_OUT, GLB_OUT, PROOF_OUT):
    p.mkdir(parents=True, exist_ok=True)

# name, phenotype, race, skin, hair, clothes, eye (major, minor), hair tint, clothes tint, pose
PEDS = {
    "rae": dict(
        pheno=dict(gender=0.0, age=0.45, muscle=0.5, weight=0.45, height=0.55, proportions=0.75, cupsize=0.45, firmness=0.55),
        race=dict(asian=0.05, caucasian=0.9, african=0.05),
        skin="young_caucasian_female.mhmat", hair="ponytail01.mhclo", brows="eyebrow010.mhclo",
        clothes=["female_elegantsuit01.mhclo", "shoes04.mhclo"],
        eyes=((0.10, 0.22, 0.30, 1), (0.05, 0.10, 0.12, 1)),
        hair_tint=(0.36, 0.22, 0.13), cloth_tint={"shoes04": (0.20, 0.19, 0.19)},
        pose=dict(arm=(0.20, 0.10), elbow=14, hip=4, head=(-6, 3), lean=2),
    ),
    "dane": dict(
        pheno=dict(gender=1.0, age=0.62, muscle=0.6, weight=0.55, height=0.45, proportions=0.7),
        race=dict(asian=0.0, caucasian=0.05, african=0.95),
        skin="middleage_african_male.mhmat", hair="short01.mhclo", brows="eyebrow001.mhclo",
        clothes=["male_elegantsuit01.mhclo", "shoes04.mhclo"],
        eyes=((0.18, 0.10, 0.05, 1), (0.08, 0.05, 0.03, 1)),
        hair_tint=(0.16, 0.14, 0.13), cloth_tint={},
        pose=dict(arm=(0.16, 0.06), elbow=10, hip=-3, head=(8, -2), lean=-1),
    ),
    "suki": dict(
        pheno=dict(gender=0.0, age=0.38, muscle=0.45, weight=0.42, height=0.62, proportions=0.7, cupsize=0.4, firmness=0.6),
        race=dict(asian=0.95, caucasian=0.05, african=0.0),
        skin="young_asian_female.mhmat", hair="bob02.mhclo", brows="eyebrow006.mhclo",
        clothes=["male_casualsuit01.mhclo", "shoes05.mhclo"],
        eyes=((0.12, 0.07, 0.04, 1), (0.05, 0.03, 0.02, 1)),
        hair_tint=(0.07, 0.06, 0.06), cloth_tint={},
        pose=dict(arm=(0.22, 0.12), elbow=22, hip=5, head=(10, 4), lean=1),
    ),
    "noah": dict(
        pheno=dict(gender=1.0, age=0.42, muscle=0.55, weight=0.5, height=0.58, proportions=0.65),
        race=dict(asian=0.05, caucasian=0.9, african=0.05),
        skin="young_caucasian_male.mhmat", hair="short04.mhclo", brows="eyebrow002.mhclo",
        clothes=["male_casualsuit05.mhclo", "shoes06.mhclo"],
        eyes=((0.20, 0.30, 0.22, 1), (0.08, 0.12, 0.08, 1)),
        hair_tint=(0.55, 0.42, 0.30), cloth_tint={},
        pose=dict(arm=(0.18, 0.08), elbow=16, hip=-4, head=(-10, -3), lean=0),
    ),
    "ivy": dict(
        pheno=dict(gender=0.0, age=0.5, muscle=0.5, weight=0.5, height=0.5, proportions=0.7, cupsize=0.5, firmness=0.5),
        race=dict(asian=0.0, caucasian=0.1, african=0.9),
        skin="young_african_female.mhmat", hair="long01.mhclo", brows="eyebrow009.mhclo",
        clothes=["male_casualsuit06.mhclo", "shoes01.mhclo"],
        eyes=((0.14, 0.08, 0.04, 1), (0.06, 0.04, 0.02, 1)),
        hair_tint=(0.30, 0.22, 0.18), cloth_tint={},
        pose=dict(arm=(0.20, 0.10), elbow=18, hip=3, head=(4, 5), lean=1),
    ),
}


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def build_human(name, spec):
    hi = HumanService._create_default_human_info_dict()
    hi["phenotype"].update(spec["pheno"])
    hi["phenotype"]["race"] = dict(spec["race"])
    hi["name"] = f"hm_ped_{name}"
    hi["rig"] = "default"
    hi["eyes"] = "high-poly.mhclo"
    hi["eyebrows"] = spec["brows"]
    hi["eyelashes"] = "eyelashes02.mhclo"
    hi["teeth"] = "teeth_base.mhclo"
    hi["hair"] = spec["hair"]
    hi["clothes"] = list(spec["clothes"])
    hi["skin_mhmat"] = spec["skin"]
    hi["skin_material_type"] = "ENHANCED_SSS"
    hi["eyes_material_type"] = "PROCEDURAL_EYES"
    # tone down MPFB default skin gloss: matte, less clearcoat (C16/C17 "plastic" read)
    matte = {"Clearcoat": 0.03, "Clearcoat Roughness": 0.45, "Roughness": 0.52, "Pore strength": 0.28}
    hi["skin_material_settings"] = {k: dict(matte) for k in ("body", "face", "ears")}
    hi["skin_material_settings"]["lips"] = {"Clearcoat": 0.12, "Roughness": 0.38}
    hi["eyes_material_settings"] = {"IrisMajorColor": list(spec["eyes"][0]), "IrisMinorColor": list(spec["eyes"][1])}
    s = HumanService.get_default_deserialization_settings()
    s["subdiv_levels"] = 1
    s["material_instances"] = "ENHANCED"
    body = HumanService.deserialize_from_dict(hi, s)
    rig = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    return body, rig


# ---------------------------------------------------------------- pose
def _bone_vec(rig, bn):
    pb = rig.pose.bones[bn]
    M = rig.matrix_world
    return (M @ pb.tail) - (M @ pb.head), M @ pb.head


def aim_bone(rig, bn, target_dir):
    """Rotate pose bone (world space, about its head) so it points along target_dir."""
    bpy.context.view_layer.update()
    cur, head = _bone_vec(rig, bn)
    q = cur.normalized().rotation_difference(Vector(target_dir).normalized())
    pb = rig.pose.bones[bn]
    Mw = rig.matrix_world @ pb.matrix
    R = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head)
    pb.matrix = rig.matrix_world.inverted() @ R @ Mw
    bpy.context.view_layer.update()


def rot_world(rig, bn, axis, deg):
    bpy.context.view_layer.update()
    pb = rig.pose.bones[bn]
    Mw = rig.matrix_world @ pb.matrix
    head = Mw.to_translation()
    R = Matrix.Translation(head) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-head)
    pb.matrix = rig.matrix_world.inverted() @ R @ Mw
    bpy.context.view_layer.update()


def idle_pose(rig, p):
    """Relaxed standing idle. MPFB human faces -Y; character left = +X."""
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="POSE")
    ax, ay = p["arm"]
    for side, sx in (("L", 1.0), ("R", -1.0)):
        # upper arm hangs down, slightly out and slightly back
        aim_bone(rig, f"upperarm01.{side}", (sx * ax, ay * 0.6, -1.0))
        # forearm: gentle elbow bend forward
        e = math.radians(p["elbow"] + (4 if side == "R" else 0))
        aim_bone(rig, f"lowerarm01.{side}", (sx * ax * 0.5, -math.sin(e), -math.cos(e)))
        # hand continues forearm, palm toward thigh
        aim_bone(rig, f"wrist.{side}", (sx * 0.02, -math.sin(e) * 0.8, -1.0))
        # relaxed finger curl
        for f in range(2, 6):
            for j, deg in ((1, 12), (2, 18), (3, 12)):
                bn = f"finger{f}-{j}.{side}"
                if bn in rig.pose.bones:
                    pb = rig.pose.bones[bn]
                    pb.rotation_mode = "XYZ"
                    pb.rotation_euler.x += math.radians(deg)
        for j, deg in ((2, 8), (3, 8)):
            bn = f"finger1-{j}.{side}"
            if bn in rig.pose.bones:
                pb = rig.pose.bones[bn]
                pb.rotation_mode = "XYZ"
                pb.rotation_euler.x += math.radians(deg)
    # weight shift: pelvis tilt + counter in spine; head turn/tilt
    hip = p["hip"]
    rot_world(rig, "spine05", "Y", hip * 0.6)
    rot_world(rig, "spine03", "Y", -hip * 0.9)
    rot_world(rig, "spine01", "X", p.get("lean", 0))
    ht, hz = p["head"]
    rot_world(rig, "neck02", "Z", ht * 0.5)
    rot_world(rig, "head", "Z", ht * 0.5)
    rot_world(rig, "head", "Y", hz)
    # legs: narrow the MakeHuman default stance (feet ~hip width, not shoulder width).
    # Aim the whole chain: rotate upperleg01 so hip->ankle has reduced lateral spread.
    for side in ("L", "R"):
        bn, fb = f"upperleg01.{side}", f"foot.{side}"
        if bn in rig.pose.bones and fb in rig.pose.bones:
            bpy.context.view_layer.update()
            M = rig.matrix_world
            hip_w = M @ rig.pose.bones[bn].head
            ank_w = M @ rig.pose.bones[fb].head
            v = ank_w - hip_w
            tgt = Vector((v.x * p.get("stance", 0.35), v.y, v.z))
            q = v.normalized().rotation_difference(tgt.normalized())
            pb = rig.pose.bones[bn]
            Mw = M @ pb.matrix
            R = Matrix.Translation(hip_w) @ q.to_matrix().to_4x4() @ Matrix.Translation(-hip_w)
            pb.matrix = M.inverted() @ R @ Mw
            bpy.context.view_layer.update()
            # keep the sole flat after the adduction
            rot_world(rig, fb, "Y", -math.degrees(q.to_euler().y))
    # legs: slight knee flex on the relaxed leg
    relaxed = "R" if hip > 0 else "L"
    rot_world(rig, f"lowerleg01.{relaxed}", "X", 6)
    bpy.ops.object.mode_set(mode="OBJECT")


# ---------------------------------------------------------------- materials
def _base_tex_link(mat):
    nt = mat.node_tree
    for node in nt.nodes:
        if node.type == "BSDF_PRINCIPLED":
            inp = node.inputs["Base Color"]
            if inp.is_linked:
                return nt, node, inp.links[0]
    return nt, None, None


def tint_material(mat, rgb, sat=1.0, rough=None, sheen=None):
    """Multiply the texture-driven base colour by rgb (for hair colour / wardrobe variation)."""
    nt, bsdf, link = _base_tex_link(mat)
    if bsdf is None:
        return False
    src = link.from_socket
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs[0].default_value = 1.0
    mix.inputs[7].default_value = (*rgb, 1.0)
    nt.links.new(src, mix.inputs[6])
    hsv = nt.nodes.new("ShaderNodeHueSaturation")
    hsv.inputs["Saturation"].default_value = sat
    nt.links.new(mix.outputs[2], hsv.inputs["Color"])
    nt.links.new(hsv.outputs["Color"], bsdf.inputs["Base Color"])
    if rough is not None and not bsdf.inputs["Roughness"].is_linked:
        bsdf.inputs["Roughness"].default_value = rough
    if sheen is not None and "Sheen Weight" in bsdf.inputs:
        bsdf.inputs["Sheen Weight"].default_value = sheen
        bsdf.inputs["Sheen Roughness"].default_value = 0.5
    return True


# Third-party logos (MakeHuman) painted out of CC0 garment textures by
# scripts/clean_ped_textures_v18.py — Harbor Metro branding only.
TEXTURE_OVERRIDES = {
    "male_casualsuit06_diffuse.png": GLB_OUT / "textures" / "male_casualsuit06_diffuse_nologo.jpg",
}


def finish_materials(name, spec):
    for img in bpy.data.images:
        base = os.path.basename(bpy.path.abspath(img.filepath))
        if base in TEXTURE_OVERRIDES and TEXTURE_OVERRIDES[base].exists():
            img.filepath = str(TEXTURE_OVERRIDES[base])
            img.reload()
            print("TEX_OVERRIDE", name, base)
    for o in bpy.data.objects:
        if o.type != "MESH":
            continue
        for slot in o.material_slots:
            m = slot.material
            if not m or not m.use_nodes:
                continue
            n = o.name.lower()
            if any(h in n for h in ("ponytail", "bob0", "short0", "long0", "afro", "braid")):
                tint_material(m, spec["hair_tint"], sat=0.9, rough=0.58)
                # soft anisotropic-ish sheen
                for nd in m.node_tree.nodes:
                    if nd.type == "BSDF_PRINCIPLED":
                        nd.inputs["Specular IOR Level"].default_value = 0.22
                        if "Coat Weight" in nd.inputs:
                            nd.inputs["Coat Weight"].default_value = 0.0
                m.blend_method = "HASHED"
            elif "suit" in n or "shoes" in n:
                rough = 0.35 if "shoes" in n else 0.82
                sheen = 0.0 if "shoes" in n else 0.35
                tint_material(m, spec["cloth_tint"].get(n.split(".")[-1], (1, 1, 1)), rough=rough, sheen=sheen)
            elif "eyebrow" in n or "eyelash" in n:
                m.blend_method = "HASHED"


def bounds_z(objs):
    zs = []
    for o in objs:
        if o.type != "MESH":
            continue
        dg = bpy.context.evaluated_depsgraph_get()
        oe = o.evaluated_get(dg)
        me = oe.to_mesh()
        zs += [(oe.matrix_world @ v.co).z for v in me.vertices]
        oe.to_mesh_clear()
    return min(zs), max(zs)


# ---------------------------------------------------------------- studio proofs
def studio(name, height):
    sc = bpy.context.scene
    w = bpy.data.worlds.new("Studio")
    sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.20, 0.205, 0.21, 1)
    bg.inputs[1].default_value = 0.55

    def area(nm, loc, en, size, col, tgt):
        bpy.ops.object.light_add(type="AREA", location=loc)
        L = bpy.context.active_object
        L.name = nm
        L.data.energy = en
        L.data.size = size
        L.data.color = col
        L.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()

    h = height
    area("Key", (-1.8, -2.6, h * 1.25), 380, 1.6, (1.0, 0.95, 0.88), (0, 0, h * 0.75))
    area("Fill", (2.4, -2.2, h * 0.8), 90, 2.8, (0.82, 0.88, 1.0), (0, 0, h * 0.6))
    area("Rim", (0.9, 2.4, h * 1.3), 320, 1.2, (0.92, 0.96, 1.0), (0, 0, h * 0.8))
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 96
    sc.cycles.use_denoising = True
    sc.view_settings.view_transform = "Filmic"
    sc.view_settings.look = "Medium High Contrast"
    sc.view_settings.exposure = -0.35
    bpy.ops.object.camera_add()
    cam = bpy.context.active_object
    sc.camera = cam

    def shoot(fn, loc, tgt, lens, rx, ry):
        cam.location = loc
        cam.data.lens = lens
        cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        sc.render.resolution_x, sc.render.resolution_y = rx, ry
        sc.render.filepath = str(PROOF_OUT / fn)
        bpy.ops.render.render(write_still=True)

    shoot(f"hm_ped_{name}_full.png", (0.30, -3.25, h * 0.58), (0, 0, h * 0.5), 50, 720, 1024)
    shoot(f"hm_ped_{name}_face.png", (0.18, -1.05, h * 0.94), (0, 0, h * 0.925), 85, 720, 720)
    # silhouette check: flat light background, 3/4 view at hero-shot distance
    bg.inputs[0].default_value = (0.85, 0.86, 0.87, 1)
    bg.inputs[1].default_value = 1.0
    shoot(f"hm_ped_{name}_silhouette.png", (-1.9, -3.1, 1.55), (0, 0, h * 0.52), 50, 720, 1024)


def export_glb(name):
    """Engine GLB: reopen the packed .blend, downscale textures to 1k, apply modifiers
    (drops MakeHuman helper geometry), keep the skinned rig. Beauty uses the .blend."""
    bpy.ops.wm.open_mainfile(filepath=str(BLEND_OUT / f"hm_ped_{name}_v18.blend"))
    for im in bpy.data.images:
        if im.size[0] > 1024:
            im.scale(1024, max(1, int(im.size[1] * 1024 / im.size[0])))
    try:
        bpy.ops.export_scene.gltf(filepath=str(GLB_OUT / f"hm_ped_{name}_v18.glb"), export_format="GLB",
                                  export_apply=True, export_animations=False, export_morph=False,
                                  export_image_format="JPEG", export_jpeg_quality=75)
    except Exception as e:  # engine export is secondary
        print("GLB_FAIL", name, e)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    names = argv[0].split(",") if argv and not argv[0].startswith("--") else list(PEDS.keys())
    no_studio = "--no-studio" in argv
    if "--glb-only" in argv:
        for name in names:
            export_glb(name)
        print("DONE glb v18", names)
        return
    for name in names:
        spec = PEDS[name]
        clear()
        body, rig = build_human(name, spec)
        idle_pose(rig, spec["pose"])
        finish_materials(name, spec)
        objs = [o for o in bpy.data.objects if o.parent == rig or o == rig]
        # all ped parts into one collection for appending into beauty
        col = bpy.data.collections.new(f"hm_ped_{name}")
        bpy.context.scene.collection.children.link(col)
        for o in list(bpy.data.objects):
            for c in list(o.users_collection):
                c.objects.unlink(o)
            col.objects.link(o)
        zmin, zmax = bounds_z(objs)
        print(f"PED {name} height={zmax - zmin:.3f} zmin={zmin:.3f}")
        bpy.ops.file.pack_all()
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_OUT / f"hm_ped_{name}_v18.blend"), compress=True)
        if not no_studio:
            studio(name, zmax - zmin)
        export_glb(name)
    print("DONE peds v18", names)


if __name__ == "__main__":
    main()
