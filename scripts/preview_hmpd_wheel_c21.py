#!/usr/bin/env python3
"""Cycle-21 quick look-dev for the HMPD wheel assembly (isolated: cruiser + wet ground + sky).
blender -b --factory-startup -P scripts/preview_hmpd_wheel_c21.py -- OUTDIR [views]"""
import bpy, sys, math
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUTD = argv[0] if argv else "/tmp/c21"
VIEWS = argv[1].split(",") if len(argv) > 1 else ["wheel", "arch"]
bpy.ops.wm.open_mainfile(filepath="/workspace/Fury/assets/meshes/harbor_metro/hmpd_cruiser_c21/hmpd_cruiser_c21.blend")
sc = bpy.context.scene
bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, 0))
gp = bpy.context.active_object
m = bpy.data.materials.new("g"); m.use_nodes = True
b = m.node_tree.nodes["Principled BSDF"]
b.inputs["Base Color"].default_value = (0.05, 0.05, 0.05, 1); b.inputs["Roughness"].default_value = 0.45
gp.data.materials.append(m)
w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
nt = w.node_tree; sky = nt.nodes.new("ShaderNodeTexSky"); sky.sky_type = "NISHITA"
sky.sun_elevation = math.radians(26); sky.sun_rotation = math.radians(200); sky.sun_intensity = 0.4
nt.links.new(sky.outputs[0], nt.nodes["Background"].inputs[0]); nt.nodes["Background"].inputs[1].default_value = 0.5
bpy.ops.object.light_add(type="SUN"); sun = bpy.context.active_object
sun.data.energy = 1.4; sun.rotation_euler = (math.radians(38), math.radians(12), math.radians(-35 - 90))
cams = {"wheel": ((-2.55, -1.95, 0.46), (-0.80, -1.45, 0.33), 55),
        "arch": ((-3.6, -3.4, 0.9), (-0.7, -1.2, 0.4), 40),
        "c20pose": ((-2.45, -3.35, 0.50), (-0.85, -1.45, 0.36), 50)}
sc.render.engine = "CYCLES"; sc.cycles.samples = int(__import__("os").environ.get("S", "48"))
sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
sc.render.resolution_percentage = int(__import__("os").environ.get("P", "60"))
sc.view_settings.view_transform = "Filmic"; sc.view_settings.look = "Medium High Contrast"; sc.view_settings.exposure = -1.0
for v in VIEWS:
    loc, tgt, lens = cams[v]
    cd = bpy.data.cameras.new(v); cd.lens = lens
    c = bpy.data.objects.new(v, cd); sc.collection.objects.link(c)
    c.location = loc; c.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    sc.camera = c
    sc.render.filepath = f"{OUTD}/prev_{v}.png"
    bpy.ops.render.render(write_still=True)
