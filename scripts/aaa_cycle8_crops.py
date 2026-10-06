#!/usr/bin/env python3
"""Generate soft-path P0 crops + Cycle-8 contact sheet for low-upload ChatGPT rejudge."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

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
    # Cycle-8: reflection-hero crops (elongated lamp pools)
    crop_box(street, 640, 540, 300, 150).save(ROOT / "02_street_asphalt_crop.png")
    crop_box(street, 680, 500, 340, 170).save(ROOT / "02_street_reflect_crop.png")

    cruiser = load_ppm(ROOT / "03_cruiser.ppm")
    crop_box(cruiser, 700, 380, 280, 170).save(ROOT / "03_cruiser_hood_crop.png")
    crop_box(cruiser, 520, 390, 200, 180).save(ROOT / "03_cruiser_door_crop.png")
    crop_box(cruiser, 780, 300, 220, 140).save(ROOT / "03_cruiser_glass_crop.png")

    peds = load_ppm(ROOT / "04_peds.ppm")
    crop_box(peds, 640, 260, 320, 210).save(ROOT / "04_peds_crop_faces.png")
    crop_box(peds, 560, 400, 280, 220).save(ROOT / "04_peds_crop_near.png")
    crop_box(peds, 420, 240, 170, 170).save(ROOT / "04_peds_crop_rae_face.png")

    night = load_ppm(ROOT / "05_night_or_alt.ppm")
    crop_box(night, 700, 520, 320, 160).save(ROOT / "05_night_asphalt_crop.png")
    crop_box(night, 720, 360, 280, 180).save(ROOT / "05_night_cruiser_reflect_crop.png")
    print("crops done")

    # Contact sheet: heroes + key P0 crops
    cells = [
        ("01 lobby", ROOT / "01_lobby.png"),
        ("02 street REFLECT", ROOT / "02_street.png"),
        ("03 cruiser HOOD", ROOT / "03_cruiser.png"),
        ("04 peds FACES", ROOT / "04_peds.png"),
        ("05 night DEFINE", ROOT / "05_night_or_alt.png"),
        ("street reflect crop", ROOT / "02_street_reflect_crop.png"),
        ("cruiser hood crop", ROOT / "03_cruiser_hood_crop.png"),
        ("peds faces crop", ROOT / "04_peds_crop_faces.png"),
        ("night asphalt crop", ROOT / "05_night_asphalt_crop.png"),
        ("rae face crop", ROOT / "04_peds_crop_rae_face.png"),
        ("blender rae face", ROOT / "blender_peds" / "hm_ped_rae_face.png"),
        ("blender dane face", ROOT / "blender_peds" / "hm_ped_dane_face.png"),
    ]
    tw, th = 420, 260
    cols, rows = 4, 3
    sheet = Image.new("RGB", (cols * tw + 20, rows * th + 60), (18, 18, 22))
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 12), "Vaultline AAA Meridian Mutual — Cycle-8 contact sheet (Harbor Metro / HMPD)", fill=(230, 230, 235))
    for i, (label, path) in enumerate(cells):
        if not path.exists():
            print("skip missing", path)
            continue
        im = Image.open(path).convert("RGB")
        im.thumbnail((tw - 10, th - 28))
        x = (i % cols) * tw + 10
        y = (i // cols) * th + 40
        sheet.paste(im, (x + (tw - 10 - im.size[0]) // 2, y + 18))
        draw.text((x + 4, y + 2), label, fill=(200, 210, 220))
    out = ROOT / "sheet_cycle8.jpg"
    sheet.save(out, quality=88, optimize=True)
    print("sheet", out, out.stat().st_size)

if __name__ == "__main__":
    main()
