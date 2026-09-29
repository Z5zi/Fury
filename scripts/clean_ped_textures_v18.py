#!/usr/bin/env python3
"""Remove third-party (MakeHuman) logos from CC0 garment textures used by Harbor Metro peds.

Harbor Metro / HMPD / Meridian Mutual branding only. Logos are detected inside the
t-shirt UV region as pixels that are saturated (orange) or darker than the fabric, then
in-painted with a masked (normalized-convolution) blur of the surrounding cloth.
"""
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter

SRC = Path("/home/box/.config/blender/4.2/extensions/.user/user_default/mpfb/data/clothes")
DST = Path("/workspace/Fury/assets/meshes/harbor_metro/peds/textures")
DST.mkdir(parents=True, exist_ok=True)


# logo boxes in normalized UV-image coords (x0, y0, x1, y1), top-left origin
LOGO_BOXES = {
    "male_casualsuit06": [(0.19, 0.11, 0.30, 0.17), (0.44, 0.11, 0.63, 0.205), (0.72, 0.14, 0.77, 0.18)],
}


def clean(name, tee_rows=0.42):
    im = np.asarray(Image.open(SRC / name / f"{name}_diffuse.png").convert("RGB")).astype(np.float32) / 255.0
    h, w, _ = im.shape
    mx, mn = im.max(2), im.min(2)
    sat = (mx - mn) / np.maximum(mx, 1e-4)
    lum = im.mean(2)
    region = np.zeros((h, w), bool)
    for x0, y0, x1, y1 in LOGO_BOXES[name]:
        region[int(y0 * h):int(y1 * h), int(x0 * w):int(x1 * w)] = True
    # fabric reference = local median-ish brightness of the white tee
    # whole logo box is replaced (the fabric there is plain white jersey)
    mask = region
    # ignore the flat background grey outside UV islands (lum ~0.82, sat~0)
    m = Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(9))
    mask = np.asarray(m) > 0
    keep = (~mask).astype(np.float32)
    out = im.copy()
    for rad in (6, 14, 30, 60):
        num = np.stack([np.asarray(Image.fromarray((im[..., c] * keep * 255).astype(np.uint8)).filter(
            ImageFilter.GaussianBlur(rad))).astype(np.float32) for c in range(3)], 2)
        den = np.asarray(Image.fromarray((keep * 255).astype(np.uint8)).filter(
            ImageFilter.GaussianBlur(rad))).astype(np.float32)[..., None]
        fill = num / np.maximum(den, 1e-3)  # both 0..255 -> ratio is 0..1 colour
        sel = mask & (den[..., 0] > 20)
        out[sel] = fill[sel]
        keep = np.maximum(keep, sel.astype(np.float32))
        mask = mask & ~sel
    dst = DST / f"{name}_diffuse_nologo.jpg"
    Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8)).save(dst, quality=92)
    print("CLEANED", dst, int(mask.sum()), "px left unfilled")
    return dst


if __name__ == "__main__":
    clean("male_casualsuit06")
