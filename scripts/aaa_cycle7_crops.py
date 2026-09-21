#!/usr/bin/env python3
"""Generate soft-path P0 crops + convert PPM→PNG for Cycle-7 judgment."""
from pathlib import Path
from PIL import Image

ROOT = Path("/workspace/Fury/artifacts/aaa_meridian_block")

def load_ppm(path):
    data = path.read_bytes()
    # skip header
    i = 0
    if data[:2] != b"P6":
        raise SystemExit(f"not P6: {path}")
    # parse header
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

    # P0 crops (1280x720 framing from Cycle-6/7 cameras)
    street = load_ppm(ROOT / "02_street.ppm")
    # asphalt center-lower third
    crop_box(street, 640, 520, 280, 140).save(ROOT / "02_street_asphalt_crop.png")
    # reflection hero — mid road elongated lamps
    crop_box(street, 700, 480, 320, 160).save(ROOT / "02_street_reflect_crop.png")

    cruiser = load_ppm(ROOT / "03_cruiser.ppm")
    crop_box(cruiser, 700, 360, 260, 160).save(ROOT / "03_cruiser_hood_crop.png")
    crop_box(cruiser, 520, 380, 200, 180).save(ROOT / "03_cruiser_door_crop.png")
    crop_box(cruiser, 780, 300, 220, 140).save(ROOT / "03_cruiser_glass_crop.png")

    peds = load_ppm(ROOT / "04_peds.ppm")
    crop_box(peds, 640, 280, 300, 200).save(ROOT / "04_peds_crop_faces.png")
    crop_box(peds, 560, 400, 280, 220).save(ROOT / "04_peds_crop_near.png")
    crop_box(peds, 480, 260, 160, 160).save(ROOT / "04_peds_crop_rae_face.png")

    night = load_ppm(ROOT / "05_night_or_alt.ppm")
    crop_box(night, 700, 500, 300, 160).save(ROOT / "05_night_asphalt_crop.png")
    crop_box(night, 720, 360, 260, 180).save(ROOT / "05_night_cruiser_reflect_crop.png")
    print("crops done")

if __name__ == "__main__":
    main()
