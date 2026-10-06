#!/usr/bin/env python3
"""Cycle-20: repaint the CC-BY 'Police car' (jeandiz) body atlas into HMPD livery inputs.

Outputs (in assets/meshes/harbor_metro/hmpd_cruiser_c20/tex/):
  body_base_hmpd.png   - atlas with original POLICE/705/911 text + grille badge painted out,
                         black paint recoloured to Harbor Metro navy
  body_lights_mask.png - R = headlamp lens islands, G = tail-lamp lens islands (night emission)
  decal_side.png       - RGBA side livery (projected in the body shader along object X)
  decal_roof.png       - RGBA roof unit number (projected along Z)
  decal_hood.png       - RGBA hood crest/word (projected along Z)
Needs numpy + scipy + Pillow (run with /tmp/venv/bin/python).
"""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy import ndimage as ndi

D = Path("/workspace/Fury/assets/meshes/harbor_metro/hmpd_cruiser_c20/tex")
FONT_B = "/usr/share/fonts/truetype/sand-box/google/Barlow Condensed/BarlowCondensed-Bold.ttf"
FONT_SB = "/usr/share/fonts/truetype/sand-box/google/Barlow Condensed/BarlowCondensed-SemiBold.ttf"
NAVY = np.array([9, 19, 50], np.float32)       # sRGB Harbor Metro navy
GOLD = (196, 152, 62, 255)

src = np.array(Image.open(D / "body_Image_0.png").convert("RGB")).astype(np.float32)
H, W, _ = src.shape
out = src.copy()
lum = src.max(axis=2)

# regions holding photo-textured parts (lamps, grille, fog lamps, mirrors) -> never recolour
EXCL = [(0, 0, 345, 95), (0, 90, 185, 220), (0, 440, 195, 505), (695, 395, 1024, 470),
        (750, 250, 930, 312), (640, 340, 700, 365)]
excl = np.zeros((H, W), bool)
_content = src.max(axis=2) > 14
for x0, y0, x1, y1 in EXCL:
    excl[y0:y1, x0:x1] = _content[y0:y1, x0:x1]
excl = ndi.binary_closing(excl, iterations=4)
excl = ndi.binary_fill_holes(excl)
excl = ndi.binary_dilation(excl, iterations=5)

white = lum > 170
# 1) letters = enclosed small dark holes inside white livery blobs -> fill white
dark = ~white
lab, n = ndi.label(dark)
border = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]])))
sizes = ndi.sum(np.ones_like(lab), lab, index=np.arange(n + 1))
fill = np.zeros((H, W), bool)
for i in range(1, n + 1):
    if i in border or sizes[i] > 6000:
        continue
    m = lab == i
    if (m & excl).any():
        continue
    fill |= m
# letters that touch the blob outline: also catch dark pixels in text rows of known word boxes
TEXT_BOXES = [(195, 195, 545, 280), (285, 525, 480, 600), (550, 640, 670, 710), (605, 150, 770, 230)]
for x0, y0, x1, y1 in TEXT_BOXES:
    sub = dark[y0:y1, x0:x1]
    fill[y0:y1, x0:x1] |= sub
fill = ndi.binary_dilation(fill, iterations=2) & ~excl
out[fill] = 255.0
# second pass: any remaining small dark speck fully inside white (letter counters)
for _ in range(2):
    w2 = out.max(axis=2) > 170
    l2, n2 = ndi.label(~w2)
    sz2 = ndi.sum(np.ones_like(l2), l2, index=np.arange(n2 + 1))
    spk = np.isin(l2, [i for i in range(1, n2 + 1) if sz2[i] < 1500]) & ~excl
    spk = ndi.binary_dilation(spk, iterations=2) & ~excl
    out[spk] = 255.0
    fill |= spk

# 2) small white text on black paint (hood POLICE, trunk POLICE, 911s) -> paint
keep = np.zeros((H, W), bool)
keep[690:1024, 0:300] = True
keep[900:935, 670:940] = True
white = out.max(axis=2) > 170
wl, wn = ndi.label(white)
wsz = ndi.sum(np.ones_like(wl), wl, index=np.arange(wn + 1))
small = np.zeros((H, W), bool)
for i in range(1, wn + 1):
    if wsz[i] < 2500:
        m = wl == i
        if not (m & excl).any():
            small |= m
small = ndi.binary_dilation(small, iterations=3) & ~excl & ~fill & ~keep
out[small] = 0.0
w3 = out.max(axis=2) > 170
l3, n3 = ndi.label(w3)
sz3 = ndi.sum(np.ones_like(l3), l3, index=np.arange(n3 + 1))
spk = np.isin(l3, [i for i in range(1, n3 + 1) if sz3[i] < 400]) & ~excl
spk = ndi.binary_dilation(spk, iterations=2) & ~excl & ~keep
out[spk] = 0.0

# 3) grille manufacturer badge -> clone honeycomb from the left of the grille
patch = out[457:481, 44:78].copy()
out[457:481, 78:112] = patch

# 4) recolour black paint -> navy (keep AA ramps to white)
rec = ~excl
o = out[rec]
out[rec] = NAVY + o * (255.0 - NAVY) / 255.0
Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(D / "body_base_hmpd.png")

# lights mask
mask = np.zeros((H, W, 3), np.uint8)
hl = np.zeros((H, W), bool); hl[5:92, 12:340] = True
hl &= src.max(axis=2) > 45
tl = np.zeros((H, W), bool); tl[92:215, 0:180] = True
tl &= (src[..., 0] > 90) & (src[..., 0] > src[..., 2] * 1.3)
mask[..., 0] = hl * 255
mask[..., 1] = tl * 255
Image.fromarray(mask).filter(ImageFilter.GaussianBlur(1.2)).save(D / "body_lights_mask.png")

# side decal: 2048 x 512, covers the door band (u = car length fraction inside the decal box)
def text_w(draw, s, f):
    b = draw.textbbox((0, 0), s, font=f)
    return b[2] - b[0], b[3] - b[1], b[1]

side = Image.new("RGBA", (2048, 512), (0, 0, 0, 0))
d = ImageDraw.Draw(side)
fb = ImageFont.truetype(FONT_B, 330)
s = "HMPD"
tw, th, toff = text_w(d, s, fb)
x = (2048 - tw) // 2 - 40
d.text((x, 40 - toff), s, font=fb, fill=(14, 26, 58, 255))
fs = ImageFont.truetype(FONT_SB, 74)
s2 = "HARBOR  METRO  POLICE"
tw2, th2, toff2 = text_w(d, s2, fs)
d.text(((2048 - tw2) // 2 - 40, 395 - toff2), s2, font=fs, fill=(14, 26, 58, 255))
# gold pinstripe either side of the subtitle
d.rectangle((60, 372, 2000, 382), fill=GOLD)
side.save(D / "decal_side.png")

# rear-quarter unit number + DIAL 911 (separate small decal)
rq = Image.new("RGBA", (512, 256), (0, 0, 0, 0))
d = ImageDraw.Draw(rq)
f = ImageFont.truetype(FONT_B, 150)
tw, th, toff = text_w(d, "27", f)
d.text(((512 - tw) // 2, 10 - toff), "27", font=f, fill=(235, 238, 242, 255))
f2 = ImageFont.truetype(FONT_SB, 52)
tw, th, toff = text_w(d, "DIAL 911", f2)
d.text(((512 - tw) // 2, 180 - toff), "DIAL 911", font=f2, fill=(235, 238, 242, 255))
rq.save(D / "decal_quarter.png")

# roof number (read from above, front of car = top of image)
roof = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
d = ImageDraw.Draw(roof)
f = ImageFont.truetype(FONT_B, 380)
tw, th, toff = text_w(d, "27", f)
d.text(((512 - tw) // 2, (512 - th) // 2 - toff), "27", font=f, fill=(14, 26, 58, 255))
roof.save(D / "decal_roof.png")

# hood: small crest + HMPD
hood = Image.new("RGBA", (1024, 512), (0, 0, 0, 0))
d = ImageDraw.Draw(hood)
cx, cy = 512, 200
shield = [(cx - 95, cy - 120), (cx + 95, cy - 120), (cx + 95, cy + 10), (cx, cy + 125), (cx - 95, cy + 10)]
d.polygon(shield, fill=GOLD)
inner = [(cx - 78, cy - 104), (cx + 78, cy - 104), (cx + 78, cy + 4), (cx, cy + 104), (cx - 78, cy + 4)]
d.polygon(inner, fill=(232, 234, 238, 255))
fh = ImageFont.truetype(FONT_B, 92)
tw, th, toff = text_w(d, "HM", fh)
d.text((cx - tw // 2, cy - 62 - toff), "HM", font=fh, fill=(14, 26, 58, 255))
fh2 = ImageFont.truetype(FONT_B, 120)
tw, th, toff = text_w(d, "HMPD", fh2)
d.text((cx - tw // 2, 360 - toff), "HMPD", font=fh2, fill=(235, 238, 242, 255))
hood.save(D / "decal_hood.png")
print("livery ok", int(fill.sum()), int(small.sum()))

# tyre sidewall lettering strip (wrapped around the sidewall in polar coords; RGB = height)
sw = Image.new("L", (4096, 160), 0)
d = ImageDraw.Draw(sw)
f = ImageFont.truetype(FONT_B, 92)
f2 = ImageFont.truetype(FONT_SB, 54)
segs = [("HARBOR PURSUIT", f, 30), ("P235/55R17  98V", f2, 52), ("ALL-SEASON  M+S", f2, 52),
        ("HARBOR PURSUIT", f, 30), ("P235/55R17  98V", f2, 52), ("DOT HM7R 2725", f2, 52)]
x = 40
for s, ff, yy in segs:
    tw, th, toff = text_w(d, s, ff)
    d.text((x, yy - toff), s, font=ff, fill=255)
    x += tw + 120
d.line((0, 8, 4096, 8), fill=150, width=4)
d.line((0, 150, 4096, 150), fill=150, width=4)
sw = sw.filter(ImageFilter.GaussianBlur(1.6))
sw.save(D / "tyre_sidewall.png")
plate = Image.new("RGB", (1040, 520), (236, 238, 240))
d = ImageDraw.Draw(plate)
d.rectangle((0, 0, 1039, 519), outline=(20, 30, 60), width=14)
d.rectangle((0, 0, 1039, 92), fill=(18, 30, 62))
ft = ImageFont.truetype(FONT_SB, 64)
tw, th, toff = text_w(d, "HARBOR METRO", ft)
d.text(((1040 - tw) // 2, 14 - toff), "HARBOR METRO", font=ft, fill=(236, 238, 240))
fp = ImageFont.truetype(FONT_B, 300)
tw, th, toff = text_w(d, "HM 2727", fp)
d.text(((1040 - tw) // 2, 150 - toff), "HM 2727", font=fp, fill=(18, 30, 62))
fe = ImageFont.truetype(FONT_SB, 50)
tw, th, toff = text_w(d, "EXEMPT  -  POLICE", fe)
d.text(((1040 - tw) // 2, 452 - toff), "EXEMPT  -  POLICE", font=fe, fill=(160, 30, 30))
plate.save(D / "plate_rear.png")
print("tyre + plate ok")
