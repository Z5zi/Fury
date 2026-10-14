#!/usr/bin/env python3
"""Cycle-19 P0 asphalt: author world-space road masks for the Meridian Mutual block.

Generates (numpy/PIL, deterministic) maps covering Blender world X[-15,65] x Y[-27,2]
at 1.5 cm/texel. Consumed by render_meridian_block_beauty_v19.py (make_road_c19).

  c19_road_height.png  16-bit  mid-frequency relief (ruts, birdbath dips, patch steps,
                               cracks, sealant over-band, paint film, manhole casting)
  c19_road_m1.png      R wet level (0 damp .. 0.8+ standing water) | G patch id | B tar sealant
  c19_road_m2.png      R white paint | G yellow paint | B open cracks
  c19_road_m3.png      R tyre wear/polish | G oil + drip stains | B gutter/kerb grime
  c19_road_m4.png      R concrete gutter band | G manhole iron | B drain recess

Layout (Blender metres): E-W carriageway between kerb faces y=-22.0 and y=-9.0 (crown
centre y=-15.5, 4 lanes x 3.25 m); N-S stub x=9.5..17.6 north of y=-9.
Crown is modelled in geometry (CROWN metres at centreline) and included here only to decide
where water stands: gutters + ruts + dips (localized puddles, never a uniform mirror).
"""
from __future__ import annotations

import math
import os
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

OUT = Path(os.environ.get("ROAD_MASK_OUT", "/workspace/vaultline-blender/refs/textures/generated/c20_road"))
X0, X1, Y0, Y1 = -15.0, 65.0, -27.0, 2.0
RES = float(os.environ.get("ROAD_RES", "0.015"))
W = int(round((X1 - X0) / RES))
H = int(round((Y1 - Y0) / RES))
YS, YN, YC = -22.0, -9.0, -15.5
XW, XE = 9.5, 17.6
CROWN = 0.05
rng = np.random.default_rng(1919)

xs = X0 + (np.arange(W, dtype=np.float32) + 0.5) * RES
ys = Y1 - (np.arange(H, dtype=np.float32) + 0.5) * RES
XX, YY = np.meshgrid(xs, ys)


def px(x, y):
    return ((x - X0) / RES, (Y1 - y) / RES)


def fbm(scale_m, octaves=4, seed=0, lac=2.0, gain=0.5):
    r = np.random.default_rng(seed)
    acc = np.zeros((H, W), np.float32)
    amp, tot, s = 1.0, 0.0, scale_m
    for _ in range(octaves):
        gw = max(2, int((X1 - X0) / s) + 3)
        gh = max(2, int((Y1 - Y0) / s) + 3)
        g = r.random((gh, gw)).astype(np.float32)
        im = Image.fromarray(g, mode="F").resize((W, H), Image.BICUBIC)
        acc += amp * np.asarray(im, np.float32)
        tot += amp
        amp *= gain
        s /= lac
        if s < RES * 3:
            break
    acc /= tot
    return (acc - acc.mean()) / (acc.std() + 1e-6)


def blur(a, sigma_m):
    s = sigma_m / RES
    F = np.fft.rfft2(a)
    fy = np.fft.fftfreq(a.shape[0])[:, None]
    fx = np.fft.rfftfreq(a.shape[1])[None, :]
    k = np.exp(-2 * (math.pi ** 2) * (s ** 2) * (fx ** 2 + fy ** 2))
    return np.fft.irfft2(F * k, s=a.shape).astype(np.float32)


def sstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def draw_mask(fn):
    im = Image.new("L", (W, H), 0)
    fn(ImageDraw.Draw(im))
    return np.asarray(im, np.float32) / 255.0


def rect(d, x0, y0, x1, y1, v=255):
    a, b = px(x0, y1)
    c, e = px(x1, y0)
    d.rectangle([a, b, c, e], fill=v)


# ---------------- regions ----------------
ew = ((YY > YS) & (YY < YN)).astype(np.float32)
ns = ((XX > XW) & (XX < XE) & (YY >= YN)).astype(np.float32)
road = np.clip(ew + ns, 0, 1)

crown_ew = CROWN * np.clip(1 - ((YY - YC) / ((YN - YS) / 2)) ** 2, 0, 1) * ew
xc_ns = (XW + XE) / 2
crown_ns = CROWN * np.clip(1 - ((XX - xc_ns) / ((XE - XW) / 2)) ** 2, 0, 1) * ns * sstep(YN, YN + 5.0, YY)
# intersection mouth: lift so the stub meets the carriageway without a 9 m valley pool
mouth_lift = 0.016 * sstep(XW, XW + 1.6, XX) * sstep(XE, XE - 1.6, XX) * np.exp(-((YY - YN) / 2.8) ** 2) * road
crown = crown_ew + crown_ns + mouth_lift

# distance to nearest kerb face (for gutter band / grime)
d_kerb = np.full((H, W), 99.0, np.float32)
d_kerb = np.where(ew > 0, np.minimum(YN - YY, YY - YS), d_kerb)
d_kerb = np.where(ns > 0, np.minimum(XX - XW, XE - XX), d_kerb)
# intersection mouth: no kerb across the N-S stub opening
mouth = (XX > XW) & (XX < XE) & (YY > YN - 3)
d_kerb = np.where(mouth & (ew > 0), np.maximum(d_kerb, np.minimum(XX - XW, XE - XX)), d_kerb)
d_kerb = np.where(mouth & (ew > 0) & (YY < YN), YY - YS, d_kerb) if False else d_kerb

gutter = (d_kerb < 0.34).astype(np.float32) * road
gutter = np.where(mouth & (YY > YN - 0.5) & (YY < YN + 0.0), 0, gutter)

print("grid", W, H, flush=True)

# ---------------- height ----------------
h = 0.0035 * fbm(6.0, 3, seed=1) + 0.0015 * fbm(1.2, 3, seed=2)

# wheel paths
lanes_ew = [YN - 1.625 - i * 3.25 for i in range(4)]
wp = np.zeros((H, W), np.float32)
along = 0.75 + 0.35 * fbm(4.0, 2, seed=3)
for yc in lanes_ew:
    for off in (-0.85, 0.85):
        wp = np.maximum(wp, np.exp(-((YY - (yc + off)) ** 2) / (2 * 0.26 ** 2)) * ew)
for xc in (XW + 2.0, XE - 2.0):
    for off in (-0.85, 0.85):
        wp = np.maximum(wp, np.exp(-((XX - (xc + off)) ** 2) / (2 * 0.26 ** 2)) * ns * sstep(YN - 1, YN + 3, YY))
wp *= np.clip(along, 0.2, 1.4)
h -= 0.0055 * wp

# birdbath dips (localized low spots)
dips = [
    (13.2, -19.8, 1.1, 0.7, 0.012), (21.0, -20.6, 1.4, 0.8, 0.010), (27.5, -17.9, 0.9, 0.6, 0.011),
    (15.8, -12.1, 1.2, 0.7, 0.012), (33.0, -13.6, 1.0, 0.55, 0.010), (8.2, -16.6, 1.3, 0.8, 0.012),
    (40.5, -18.4, 1.6, 0.9, 0.012), (24.8, -10.2, 1.8, 0.6, 0.009), (12.6, -4.0, 1.0, 0.8, 0.010),
    (18.9, -14.9, 0.7, 0.45, 0.009), (5.0, -20.9, 1.5, 0.6, 0.010), (46.0, -11.0, 1.4, 0.7, 0.011),
]
# C20: extra shallow birdbaths placed on the mirror points of the street lamps / lit bank windows /
# cruiser lightbar as seen from the 05 night cameras, so night water carries light instead of black sky
dips += [
    (14.9, -16.6, 0.9, 0.45, 0.010), (17.2, -16.9, 0.8, 0.40, 0.009), (19.6, -16.7, 0.9, 0.45, 0.010),
    (16.3, -19.8, 1.0, 0.45, 0.010), (19.1, -19.6, 0.8, 0.40, 0.009), (11.1, -17.6, 0.9, 0.45, 0.010),
    (13.0, -17.7, 0.8, 0.40, 0.009), (12.6, -20.3, 0.9, 0.45, 0.010), (10.3, -20.2, 0.8, 0.40, 0.009),
    (23.0, -11.8, 1.1, 0.5, 0.010), (31.0, -11.6, 1.0, 0.45, 0.010),
]
for (x, y, sx, sy, dpt) in dips:
    dpt *= 1.35
    rr = ((XX - x) / sx) ** 2 + ((YY - y) / sy) ** 2
    h -= dpt * np.exp(-rr * 1.6) * road

# drains: recessed grates at the gutter, surrounding dish
drains = [(26.0, YN - 0.2, 0), (40.0, YN - 0.2, 0), (16.0, YS + 0.2, 0), (30.0, YS + 0.2, 0),
          (44.0, YS + 0.2, 0), (XE - 0.2, -3.0, 1)]
drain_m = np.zeros((H, W), np.float32)
for (x, y, rot) in drains:
    sx, sy = (0.45, 0.2) if rot == 0 else (0.2, 0.45)
    drain_m = np.maximum(drain_m, ((np.abs(XX - x) < sx) & (np.abs(YY - y) < sy)).astype(np.float32))
    rr = ((XX - x) / 1.6) ** 2 + ((YY - y) / 1.0) ** 2
    h -= 0.009 * np.exp(-rr * 2.0) * road

# ---------------- patches ----------------
patch_id = np.zeros((H, W), np.float32)
seal = np.zeros((H, W), np.float32)
patch_h = np.zeros((H, W), np.float32)
patches = [  # x0,y0,x1,y1,id(0.5 old/1 new), dh
    (23.5, -21.2, 31.0, -20.1, 1.0, -0.002),   # utility trench along south lane
    (11.2, -14.6, 13.6, -12.4, 0.5, 0.002),    # square cut in the intersection
    (29.0, -12.9, 30.6, -11.2, 1.0, -0.001),
    (34.5, -16.8, 38.8, -15.9, 0.5, 0.0015),   # long patch near centreline
    (2.0, -13.4, 4.2, -11.0, 1.0, 0.002),
    (15.0, -7.5, 16.9, -1.5, 0.5, -0.0015),    # N-S stub trench (04 foreground)
    (19.0, -18.9, 20.2, -17.6, 1.0, 0.001),
]
sw = 0.045  # tar seam width
im_p = Image.new("F", (W, H), 0.0)
im_s = Image.new("L", (W, H), 0)
im_ph = Image.new("F", (W, H), 0.0)
dp, ds, dph = ImageDraw.Draw(im_p), ImageDraw.Draw(im_s), ImageDraw.Draw(im_ph)
for (x0, y0, x1, y1, pid, dh) in patches:
    a, b = px(x0, y1)
    c, e = px(x1, y0)
    dp.rectangle([a, b, c, e], fill=pid)
    dph.rectangle([a, b, c, e], fill=dh)
    wpx = max(2, int(sw / RES))
    ds.rectangle([a, b, c, e], outline=255, width=wpx)
# irregular pothole fills
blob_n = fbm(0.35, 3, seed=11)
for i, (x, y, r) in enumerate([(15.6, -20.35, 0.42), (9.4, -19.2, 0.55), (36.4, -20.3, 0.45), (22.6, -11.4, 0.6), (44.0, -14.2, 0.5),
                               (6.2, -10.4, 0.4), (16.3, -16.9, 0.35)]):
    rr = np.sqrt((XX - x) ** 2 + (YY - y) ** 2) / r
    m = (rr + 0.28 * blob_n) < 1.0
    ring = ((rr + 0.28 * blob_n) < 1.0 + sw * 1.4 / r) & ~m
    patch_id = np.where(m, 1.0, patch_id)
    patch_h = np.where(m, 0.0015, patch_h)
    seal = np.maximum(seal, ring.astype(np.float32))
patch_id = np.maximum(patch_id, np.asarray(im_p, np.float32))
patch_h = patch_h + np.asarray(im_ph, np.float32)
seal = np.maximum(seal, np.asarray(im_s, np.float32) / 255.0)

# sealed longitudinal paving joints (tar over-band), partial & wobbly
im_j = Image.new("L", (W, H), 0)
dj = ImageDraw.Draw(im_j)
for (yj, xa, xb) in [(YC + 0.26, -15, 9.0), (YC + 0.26, 22.8, 65), (YN - 3.25, 24.0, 52.0), (YS + 3.25, -10.0, 7.5),
                     (YS + 3.25, 26.0, 47.0)]:
    pts = []
    x = xa
    while x <= xb:
        pts.append(px(x, yj + 0.025 * math.sin(x * 1.7) + 0.02 * rng.standard_normal()))
        x += 0.5
    dj.line(pts, fill=255, width=max(2, int(rng.uniform(0.05, 0.075) / RES)))
seal = np.maximum(seal, np.asarray(im_j, np.float32) / 255.0)

# ---------------- cracks ----------------
im_c = Image.new("L", (W, H), 0)   # open cracks
im_cs = Image.new("L", (W, H), 0)  # sealed cracks (over-band)
dc, dcs = ImageDraw.Draw(im_c), ImageDraw.Draw(im_cs)


def walk(x, y, ang, length, step=0.06, jitter=0.35):
    pts = [(x, y)]
    for _ in range(int(length / step)):
        ang += rng.normal(0, jitter)
        x += step * math.cos(ang)
        y += step * math.sin(ang)
        pts.append((x, y))
    return pts


def draw_crack(pts, sealed, branch=True):
    P = [px(*p) for p in pts]
    if sealed:
        # over-band with varying width (hand-applied tar)
        for k in range(0, len(P) - 1, 3):
            seg = P[k:k + 4]
            if len(seg) > 1:
                dcs.line(seg, fill=255, width=max(2, int(rng.uniform(0.03, 0.085) / RES)))
        dc.line(P, fill=110, width=1)
    else:
        dc.line(P, fill=255, width=max(1, int(0.012 / RES)))
    if branch and len(pts) > 20:
        for _ in range(rng.integers(1, 4)):
            k = int(rng.integers(5, len(pts) - 5))
            bx, by = pts[k]
            sub = walk(bx, by, rng.uniform(0, 2 * math.pi), rng.uniform(0.3, 1.4), jitter=0.5)
            draw_crack(sub, False, branch=False)


# transverse cracks across the carriageway (thermal) — some sealed
for i in range(34):
    x = rng.uniform(-14, 64)
    sealed = rng.random() < 0.45
    pts = walk(x, YS + rng.uniform(0.1, 1.0), math.pi / 2 + rng.normal(0, 0.12), rng.uniform(3.0, 12.5), jitter=0.16)
    draw_crack(pts, sealed)
# longitudinal wheel-path cracks
for i in range(22):
    yc = rng.choice(lanes_ew) + rng.choice([-0.85, 0.85]) + rng.normal(0, 0.15)
    pts = walk(rng.uniform(-14, 60), yc, rng.choice([0, math.pi]) + rng.normal(0, 0.05), rng.uniform(1.5, 7.0), jitter=0.12)
    draw_crack(pts, rng.random() < 0.3)
# N-S stub cracks
for i in range(8):
    pts = walk(rng.uniform(XW + 0.5, XE - 0.5), rng.uniform(-8.5, 1.0), rng.normal(0, 0.3) + rng.choice([0, math.pi / 2]),
               rng.uniform(1.0, 4.0), jitter=0.25)
    draw_crack(pts, rng.random() < 0.4)
# kerb-line crack along gutter joint (asphalt/concrete seam)
for (y, xa, xb) in [(YN - 0.34, -15, 65), (YS + 0.34, -15, 65)]:
    pts = [(x, y + 0.01 * math.sin(x * 3.1)) for x in np.arange(xa, xb, 0.1)]
    dc.line([px(*p) for p in pts], fill=200, width=max(1, int(0.012 / RES)))

crack = np.asarray(im_c, np.float32) / 255.0
crack_s = np.asarray(im_cs, np.float32) / 255.0


# alligator (fatigue) cracking in two wheel-path zones: Voronoi edges
def alligator(cx, cy, sx, sy, cell=0.16, seed=5):
    r = np.random.default_rng(seed)
    x0p, y0p = px(cx - sx, cy + sy)
    x1p, y1p = px(cx + sx, cy - sy)
    x0p, y0p, x1p, y1p = int(x0p), int(y0p), int(x1p), int(y1p)
    sub_x = XX[y0p:y1p, x0p:x1p]
    sub_y = YY[y0p:y1p, x0p:x1p]
    n = int((2 * sx) * (2 * sy) / (cell ** 2))
    ptsx = r.uniform(cx - sx, cx + sx, n).astype(np.float32)
    ptsy = r.uniform(cy - sy, cy + sy, n).astype(np.float32)
    d1 = np.full(sub_x.shape, 9.0, np.float32)
    d2 = np.full(sub_x.shape, 9.0, np.float32)
    for px_, py_ in zip(ptsx, ptsy):
        d = np.sqrt((sub_x - px_) ** 2 + (sub_y - py_) ** 2)
        m = d < d1
        d2 = np.where(m, d1, np.minimum(d2, d))
        d1 = np.where(m, d, d1)
    edge = np.clip(1 - (d2 - d1) / 0.018, 0, 1)
    fall = np.exp(-(((sub_x - cx) / sx) ** 2 + ((sub_y - cy) / sy) ** 2) * 1.5)
    crack[y0p:y1p, x0p:x1p] = np.maximum(crack[y0p:y1p, x0p:x1p], edge * np.clip(fall * 1.4, 0, 1))


alligator(14.8, lanes_ew[2] - 0.85, 2.4, 0.45, seed=5)
alligator(17.2, lanes_ew[3] + 0.85, 1.3, 0.42, seed=8)       # 02_asphalt_near foreground wheel path
alligator(31.5, lanes_ew[0] + 0.85, 1.8, 0.4, seed=6)
alligator(12.4, -2.0, 1.2, 0.9, seed=7)
crack *= road
crack_s *= road
seal = np.maximum(seal, crack_s) * road

h -= 0.004 * crack * (1 - crack_s)
h += 0.0015 * seal
h += patch_h * road

# ---------------- paint ----------------
im_w = Image.new("L", (W, H), 0)
im_y = Image.new("L", (W, H), 0)
dw, dy = ImageDraw.Draw(im_w), ImageDraw.Draw(im_y)
# double yellow centreline (stops at the crosswalks)
for (xa, xb) in [(-15, 4.6), (22.6, 65)]:
    for off in (-0.12, 0.12):
        rect(dy, xa, YC + off - 0.05, xb, YC + off + 0.05)
# dashed white lane lines: 3 m dash / 9 m gap
for yl in (YN - 3.25, YS + 3.25):
    for xa in np.arange(-14.0, 65, 12.0):
        if 4.0 < xa + 3 and xa < 22.8:
            continue
        rect(dw, xa, yl - 0.06, xa + 3.0, yl + 0.06)
# crosswalks (continental bars) east + west legs, and across the N-S stub
for (xa, xb) in [(18.4, 21.6), (5.6, 8.8)]:
    y = YS + 0.55
    while y + 0.5 < YN - 0.4:
        rect(dw, xa, y, xb, y + 0.5)
        y += 1.1
x = XW + 0.5
while x + 0.5 < XE - 0.4:
    rect(dw, x, -6.9, x + 0.5, -4.0)
    x += 1.1
# stop bars
rect(dw, 22.6, YC + 0.3, 23.05, YN - 0.35)
rect(dw, 4.35, YS + 0.35, 4.8, YC - 0.3)
rect(dw, XW + 0.35, -3.6, xc_ns - 0.2, -3.15)


# straight + left-turn arrows (westbound, travelling -x) before stop bar
def arrow(d, x, y, flip=1, left=False):
    L, sw_, hw, hl = 3.6, 0.15, 0.45, 1.1
    body = [(x + hl, y - sw_), (x + L, y - sw_), (x + L, y + sw_), (x + hl, y + sw_)]
    head = [(x, y), (x + hl, y - hw), (x + hl, y + hw)]
    d.polygon([px(*p) for p in body], fill=255)
    d.polygon([px(*p) for p in head], fill=255)
    if left:
        d.polygon([px(*p) for p in [(x + 1.9, y + sw_), (x + 2.2, y + sw_), (x + 1.4, y + 0.95), (x + 1.1, y + 0.95)]], fill=255)


arrow(dw, 26.0, YN - 1.625)
arrow(dw, 26.0, YN - 1.625 - 3.25, left=True)
white = np.asarray(im_w, np.float32) / 255.0
yellow = np.asarray(im_y, np.float32) / 255.0
# paint wear: eroded in wheel paths, speckled loss, softened edge
erode = fbm(0.25, 4, seed=21)
fine = fbm(0.05, 3, seed=22)
keep = np.clip(1.25 - 0.95 * wp - 0.35 * np.clip(erode, 0, None) - 0.25 * np.clip(fine, 0, None), 0, 1)
keep = sstep(0.25, 0.75, keep)
white = blur(white, 0.004) * keep * road
yellow = blur(yellow, 0.004) * np.clip(keep + 0.15, 0, 1) * road
h += 0.0009 * np.clip(white + yellow, 0, 1)

# ---------------- manholes ----------------
man = np.zeros((H, W), np.float32)
for (mx, my, mr) in [(13.4, -17.4, 0.32), (32.2, -14.1, 0.32), (13.6, -1.2, 0.32), (44.5, -19.9, 0.32)]:
    rr = np.sqrt((XX - mx) ** 2 + (YY - my) ** 2)
    disc = (rr < mr).astype(np.float32)
    ring_gap = ((rr > mr) & (rr < mr + 0.012)).astype(np.float32)
    frame = ((rr >= mr + 0.012) & (rr < mr + 0.07)).astype(np.float32)
    # cast pattern: concentric + radial ribs
    ang = np.arctan2(YY - my, XX - mx)
    rib = (np.abs(np.sin(rr * 2 * math.pi / 0.045)) > 0.55) & (rr < mr - 0.03)
    rad = np.abs(np.sin(ang * 12)) < 0.12
    pat = np.clip(rib.astype(np.float32) + rad.astype(np.float32) * (rr < mr - 0.03), 0, 1)
    man = np.maximum(man, np.clip(disc + frame, 0, 1))
    h += (-0.004 * disc + 0.0012 * pat * disc - 0.01 * ring_gap + 0.001 * frame)
    # clear paint/cracks on iron
    white *= (1 - disc - frame).clip(0, 1)
    crack *= (1 - disc - frame).clip(0, 1)

h -= 0.012 * drain_m

# ---------------- water ----------------
total = crown + h
# gutter pools: surface below a level set near the kerb; plus local fill of dips/ruts
level_g = 0.0008 + 0.0042 * fbm(5.0, 3, seed=31)
dep_g = level_g - total
local = blur(h, 1.2) - h - 0.0042
depth = np.maximum(dep_g, local) * road
depth = np.where(man > 0.5, np.minimum(depth, 0.0), depth)
water = sstep(0.0, 0.0012, depth)
water = blur(water, 0.01)
halo = np.clip(blur(water, 0.22) * 1.6, 0, 1)
wetvar = 0.5 + 0.5 * np.tanh(0.8 * fbm(2.5, 3, seed=33))
wet_level = np.clip(0.22 * wetvar + 0.30 * halo + 0.12 * gutter - 0.12 * wp * (1 - halo), 0, 0.75)
wet_level = np.maximum(wet_level, 0.80 + 0.2 * np.clip(depth / 0.006, 0, 1)) * (water > 0.5) + wet_level * (water <= 0.5)
wet_level = np.clip(wet_level, 0, 1) * road + 0.2 * (1 - road)
print("water coverage (road) %.1f%%" % (100 * (water * road).sum() / road.sum()), flush=True)

# ---------------- wear / oil / grime ----------------
wear = np.clip(wp * (0.6 + 0.4 * fbm(1.5, 3, seed=41)), 0, 1)
ix = ((XX > XW - 2) & (XX < XE + 2) & (ew > 0)).astype(np.float32)
wear = np.clip(wear + 0.35 * ix * (0.5 + 0.5 * np.tanh(fbm(2.0, 2, seed=42))), 0, 1) * road

oil = np.zeros((H, W), np.float32)
spots = []
for yc in lanes_ew:
    for _ in range(70):
        spots.append((rng.uniform(-14, 64), yc + rng.normal(0, 0.18), rng.uniform(0.05, 0.28)))
# idling queues behind stop bars and parking positions along the curb lanes
for _ in range(40):
    spots.append((rng.uniform(23.2, 33), YN - 1.625 - rng.choice([0, 3.25]) + rng.normal(0, 0.12), rng.uniform(0.12, 0.4)))
    spots.append((rng.uniform(-6, 4.2), YS + 1.625 + rng.choice([0, 3.25]) + rng.normal(0, 0.12), rng.uniform(0.12, 0.4)))
for _ in range(9):  # hand-placed drip cluster in the 02_asphalt_near foreground
    spots.append((16.9 + rng.normal(0, 0.35), -20.55 + rng.normal(0, 0.12), rng.uniform(0.06, 0.2)))
for x in np.arange(-12, 64, 5.6):
    for yy in (YN - 1.3, YS + 1.3):
        spots.append((x + rng.normal(0, 0.4), yy + rng.normal(0, 0.1), rng.uniform(0.25, 0.55)))
on = fbm(0.12, 3, seed=43)
im_o = Image.new("F", (W, H), 0.0)
do = ImageDraw.Draw(im_o)
for (x, y, r) in spots:
    a, b = px(x - r, y + r * 0.7)
    c, e = px(x + r, y - r * 0.7)
    do.ellipse([a, b, c, e], fill=float(rng.uniform(0.35, 1.0)))
oil = blur(np.asarray(im_o, np.float32), 0.05)
oil = np.clip(oil * (0.7 + 0.5 * on), 0, 1) * road
# centre-of-lane oil stripe (long-term drips)
stripe = np.zeros((H, W), np.float32)
for yc in lanes_ew:
    stripe = np.maximum(stripe, np.exp(-((YY - yc) ** 2) / (2 * 0.35 ** 2)))
oil = np.clip(oil + 0.28 * stripe * ew * (0.5 + 0.5 * np.tanh(fbm(1.0, 3, seed=44))), 0, 1)

grime = np.clip(np.exp(-np.clip(d_kerb, 0, None) / 0.45) * (0.7 + 0.5 * fbm(0.6, 3, seed=51)), 0, 1) * road
grime = np.clip(grime + 0.25 * np.clip(fbm(1.8, 3, seed=52), 0, None) * road + 0.5 * blur(drain_m, 0.25), 0, 1)

# ---------------- write ----------------
OUT.mkdir(parents=True, exist_ok=True)


def u8(a):
    return (np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8)


hn = np.clip((h + 0.03) / 0.06, 0, 1)
Image.fromarray((hn * 65535).astype(np.uint16)).save(OUT / "c19_road_height.png")
Image.fromarray(np.dstack([u8(wet_level), u8(patch_id), u8(seal)])).save(OUT / "c19_road_m1.png")
Image.fromarray(np.dstack([u8(white), u8(yellow), u8(crack * (1 - 0.6 * crack_s))])).save(OUT / "c19_road_m2.png")
Image.fromarray(np.dstack([u8(wear), u8(oil), u8(grime)])).save(OUT / "c19_road_m3.png")
Image.fromarray(np.dstack([u8(gutter), u8(man), u8(drain_m)])).save(OUT / "c19_road_m4.png")
# coarse crown grid (0.2 m) for the Blender road mesh
step = int(round(0.2 / RES))
np.save(OUT / "c19_crown_0p2m.npy", crown[::step, ::step].astype(np.float32))
(OUT / "layout.txt").write_text(
    f"X0={X0} X1={X1} Y0={Y0} Y1={Y1} RES={RES} W={W} H={H}\nYS={YS} YN={YN} YC={YC} XW={XW} XE={XE} CROWN={CROWN}\n"
    f"drains={drains}\n")
# preview
prev = np.dstack([u8(wet_level), u8(np.clip(white + yellow, 0, 1)), u8(np.clip(crack + seal, 0, 1))])
Image.fromarray(prev).resize((W // 4, H // 4)).save(OUT / "preview_wet_paint_crack.jpg")
print("WROTE", OUT, flush=True)
