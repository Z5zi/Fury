#!/usr/bin/env python3
"""Generate soft-path P0 crops + Cycle-10 mini contact sheet (≤4 tiles) for low-upload ChatGPT."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path("/workspace/Fury/artifacts/aaa_meridian_block")

def load_ppm(path):
    data = path.read_bytes()
    if data[:2] != b"P6":
        raise SystemExit(f"not P6: {path}")
    parts = []
    i = 2
    while len(parts) < 3:
        while i < len(data) and data[i] in b" \t\n\r":
            i += 1
        if data[i:i+1] == b"#":
            while i < len(data) and data[i] not in b"\n":
                i += 1
            continue
        j = i
        while j < len(data) and data[j] not in b" \t\n\r":
            j += 1
        parts.append(data[i:j].decode())
        i = j
    while i < len(data) and data[i] in b" \t\r":
        i += 1
    if data[i:i+1] == b"\n":
        i += 1
    w, h, maxv = int(parts[0]), int(parts[1]), int(parts[2])
    raw = data[i:i + w * h * 3]
    return Image.frombytes("RGB", (w, h), raw)

def crop_box(im, cx, cy, half_w, half_h):
    w, h = im.size
    l = max(0, int(cx - half_w)); r = min(w, int(cx + half_w))
    t = max(0, int(cy - half_h)); b = min(h, int(cy + half_h))
    return im.crop((l, t, r, b))

def main():
    for stem in ["01_lobby", "02_street", "03_cruiser", "04_peds", "05_night_or_alt"]:
        ppm = ROOT / f"{stem}.ppm"
        if not ppm.exists():
            print("missing", ppm); continue
        im = load_ppm(ppm)
        im.save(ROOT / f"{stem}.png")
        print("png", stem, im.size)

    street = load_ppm(ROOT / "02_street.ppm")
    crop_box(street, 640, 540, 300, 150).save(ROOT / "02_street_asphalt_crop.png")
    crop_box(street, 680, 500, 340, 170).save(ROOT / "02_street_reflect_crop.png")

    cruiser = load_ppm(ROOT / "03_cruiser.ppm")
    crop_box(cruiser, 700, 380, 280, 170).save(ROOT / "03_cruiser_hood_crop.png")
    crop_box(cruiser, 520, 390, 200, 180).save(ROOT / "03_cruiser_door_crop.png")
    crop_box(cruiser, 780, 300, 220, 140).save(ROOT / "03_cruiser_glass_crop.png")
    # Side-by-side reflection-as-subject crop (hood + asphalt)
    hood = crop_box(cruiser, 700, 360, 260, 150)
    asphalt = crop_box(street, 680, 520, 260, 150)
    sb = Image.new("RGB", (hood.size[0] + asphalt.size[0] + 8, max(hood.size[1], asphalt.size[1]) + 28), (14, 14, 18))
    d = ImageDraw.Draw(sb)
    d.text((4, 4), "REFLECT DEFINE: hood bands | wet asphalt pools", fill=(230, 230, 240))
    sb.paste(hood, (0, 24))
    sb.paste(asphalt, (hood.size[0] + 8, 24))
    sb.save(ROOT / "03_reflect_sidebyside_crop.png")

    peds = load_ppm(ROOT / "04_peds.ppm")
    crop_box(peds, 640, 260, 320, 210).save(ROOT / "04_peds_crop_faces.png")
    crop_box(peds, 560, 400, 280, 220).save(ROOT / "04_peds_crop_near.png")
    crop_box(peds, 420, 240, 170, 170).save(ROOT / "04_peds_crop_rae_face.png")

    night = load_ppm(ROOT / "05_night_or_alt.ppm")
    crop_box(night, 700, 520, 320, 160).save(ROOT / "05_night_asphalt_crop.png")
    crop_box(night, 720, 360, 280, 180).save(ROOT / "05_night_cruiser_reflect_crop.png")
    print("crops done")

    # Mini sheet: MAX 4 tiles for low-upload ChatGPT
    cells = [
        ("04 faces ATLAS", ROOT / "04_peds_crop_faces.png"),
        ("03 hood BANDS", ROOT / "03_cruiser_hood_crop.png"),
        ("02 wet REFLECT", ROOT / "02_street_reflect_crop.png"),
        ("05 night DEFINE", ROOT / "05_night_asphalt_crop.png"),
    ]
    tw, th = 480, 300
    cols, rows = 2, 2
    sheet = Image.new("RGB", (cols * tw + 16, rows * th + 48), (16, 16, 20))
    draw = ImageDraw.Draw(sheet)
    draw.text((10, 10), "Vaultline AAA Meridian Mutual — Cycle-10 MINI (Harbor Metro / HMPD)", fill=(230, 230, 235))
    draw.text((10, 28), "P0: face atlases + planar hood/wet reflections DEFINING", fill=(180, 190, 200))
    for i, (label, path) in enumerate(cells):
        if not path.exists():
            print("skip missing", path)
            continue
        im = Image.open(path).convert("RGB")
        im.thumbnail((tw - 12, th - 32))
        x = (i % cols) * tw + 8
        y = (i // cols) * th + 46
        sheet.paste(im, (x + (tw - 12 - im.size[0]) // 2, y + 18))
        draw.text((x + 4, y + 2), label, fill=(210, 220, 230))
    out = ROOT / "sheet_cycle10_mini.jpg"
    sheet.save(out, quality=86, optimize=True)
    print("mini sheet", out, out.stat().st_size)

def build_all_in_one():
    """Quota-safe single collage: heroes + reflect side-by-side + faces."""
    cells = [
        ("01 lobby", ROOT / "01_lobby.png"),
        ("02 street REFLECT", ROOT / "02_street.png"),
        ("03 cruiser HOOD", ROOT / "03_cruiser.png"),
        ("04 peds FACES", ROOT / "04_peds.png"),
        ("05 night DEFINE", ROOT / "05_night_or_alt.png"),
        ("REFLECT crop", ROOT / "03_reflect_sidebyside_crop.png"),
        ("faces crop", ROOT / "04_peds_crop_faces.png"),
        ("hood bands", ROOT / "03_cruiser_hood_crop.png"),
    ]
    tw, th = 420, 250
    cols, rows = 4, 2
    sheet = Image.new("RGB", (cols * tw + 20, rows * th + 56), (12, 12, 16))
    draw = ImageDraw.Draw(sheet)
    draw.text((10, 8), "Vaultline AAA Meridian Mutual — Cycle-10 ALL-IN-ONE (Harbor Metro / HMPD)", fill=(230, 230, 235))
    draw.text((10, 26), "P0: midtone contrast + planar DEFINE reflections + sharper faces + contact ground | 0% clip target", fill=(170, 185, 200))
    for i, (label, path) in enumerate(cells):
        if not path.exists():
            print("skip", path); continue
        im = Image.open(path).convert("RGB")
        im.thumbnail((tw - 12, th - 28))
        x = (i % cols) * tw + 10
        y = (i // cols) * th + 50
        sheet.paste(im, (x + (tw - 12 - im.size[0]) // 2, y + 16))
        draw.text((x + 4, y + 2), label, fill=(210, 220, 230))
    out = ROOT / "cycle10_ALL_IN_ONE.jpg"
    sheet.save(out, quality=88, optimize=True)
    print("ALL_IN_ONE", out, out.stat().st_size)

if __name__ == "__main__":
    main()
    build_all_in_one()
