# Character skin material review — 6 October 2026

This is a measured limitation and an unshipped experiment. It is not a claim
that the current characters have production-quality skin or that adding pore
noise solves their visibly smooth appearance.

The actual runtime Face normal tile contains1,048,576 texels but only four RGB
values: red/green127 or128, blue255. All mip levels remain effectively flat.
The existing central-face allocation is approximately251×301 texels. These
observations explain why the existing low-amplitude height field contributes
little visible pore or crease response.

An original-data, normal-only private candidate produced100 RGB normal values,
red121–134,green120–136,blue255 and a maximum3.98-degree tilt. Albedo, ORM and
all normals outside Face stayed byte-identical. Seven matched render pairs
executed on CPU raster, CPU path and actual offscreen OpenGL, including
minified views. In the640px GL front portrait the maximum change was5/255 per
channel and the image-wide mean0.041/255; the visible difference was slight.
The candidate is therefore **not enabled in production**. See the archived
[experiment](../validation/characters/skin-research/PROTOTYPE_REVIEW.md),
[precise private patch](../validation/characters/skin-research/candidate.patch)
and [native face comparison](../validation/characters/skin-research/gl-native-face-crop.png).
The patch is study material against its recorded snapshot, not an instruction
to apply it blindly to later source.

Official MakeHuman graphical assets include explicitly licensedCC0 skin packs:
https://static.makehumancommunity.org/assets/assetpacks/skins01.html . Inspected
candidate maps were unsuitable as direct neutral PBR replacements: core skins
include painted scalp stubble and facial shading, the inspected Aksel set adds
injury/lesion detail, and legacy SPEC maps are specularity, not roughness. A
clean uniform skin had no anatomical detail. None is imported into Fury by this
pass, and no proprietary Rockstar or artist reference image is redistributed.

Useful next work is anatomically differentiated albedo and regional roughness,
adequate face texel density and skin light transport, followed by matched
in-game inspection. Enlarging pores into visible noise, transferring scars or
baked illumination from a reference, or counting changed pixels as realism
would not establish that improvement. Facial deformation and skin shading
remain separate outstanding capabilities.
