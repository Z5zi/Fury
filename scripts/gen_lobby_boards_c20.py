"""Cycle 20: Meridian Mutual lobby signage textures (rates board + brand poster). Original artwork,
Harbor Metro / Meridian Mutual fictional branding only."""
import random
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = Path("/workspace/vaultline-blender/refs/textures/generated/c20_lobby")
OUT.mkdir(parents=True, exist_ok=True)
FD = "/usr/share/fonts/truetype/sand-box/google/Barlow Condensed/"
FB, FSB = FD + "BarlowCondensed-Bold.ttf", FD + "BarlowCondensed-SemiBold.ttf"
NAVY, GOLD, WHITE = (10, 22, 56), (201, 162, 74), (238, 240, 244)

# --- digital rates board (16:9) ---
W, H = 1600, 900
im = Image.new("RGB", (W, H), NAVY)
d = ImageDraw.Draw(im)
for y in range(H):                                   # subtle vertical gradient
    t = y / H
    d.line([(0, y), (W, y)], fill=(int(10 + 8 * t), int(22 + 10 * t), int(56 + 22 * t)))
d.rectangle([0, 0, W, 150], fill=(6, 14, 38))
d.text((60, 28), "MERIDIAN MUTUAL", font=ImageFont.truetype(FB, 92), fill=WHITE)
d.text((W - 60, 46), "TODAY'S RATES", font=ImageFont.truetype(FSB, 64), fill=GOLD, anchor="ra")
d.rectangle([0, 150, W, 158], fill=GOLD)
rows = [("HIGH-YIELD SAVINGS", "3.85%", "APY"), ("12-MONTH CERTIFICATE", "4.10%", "APY"),
        ("HARBOR CHECKING PLUS", "0.45%", "APY"), ("NEW AUTO LOAN", "6.49%", "APR"),
        ("30-YR FIXED MORTGAGE", "6.875%", "APR")]
fr, fv, fu = ImageFont.truetype(FSB, 70), ImageFont.truetype(FB, 96), ImageFont.truetype(FSB, 44)
for i, (lab, val, unit) in enumerate(rows):
    y = 200 + i * 128
    if i % 2 == 0:
        d.rectangle([40, y - 14, W - 40, y + 104], fill=(16, 32, 74))
    d.text((80, y + 6), lab, font=fr, fill=WHITE)
    d.text((W - 190, y - 6), val, font=fv, fill=GOLD, anchor="ra")
    d.text((W - 80, y + 40), unit, font=fu, fill=(180, 190, 210), anchor="ra")
d.text((60, H - 52), "Rates effective 10/01. Deposits protected by the Harbor Metro Deposit Board. See a teller for details.",
       font=ImageFont.truetype(FSB, 32), fill=(150, 160, 185))
im.save(OUT / "rates_board.png")

# --- brand poster (portrait) : stylised harbour skyline at dusk ---
W, H = 900, 1350
im = Image.new("RGB", (W, H))
d = ImageDraw.Draw(im)
for y in range(H):
    t = y / H
    c = (int(18 + 200 * t ** 1.6), int(30 + 110 * t ** 1.8), int(70 + 40 * t))
    d.line([(0, y), (W, y)], fill=c)
rnd = random.Random(4)
x = 0
base = 1000
while x < W:                                          # skyline silhouette with lit windows
    bw, bh = rnd.randint(50, 120), rnd.randint(160, 560)
    d.rectangle([x, base - bh, x + bw, base], fill=(14, 20, 40))
    for wy in range(base - bh + 14, base - 10, 22):
        for wx in range(x + 8, x + bw - 8, 16):
            if rnd.random() < 0.35:
                d.rectangle([wx, wy, wx + 7, wy + 10], fill=(255, 205, 120))
    x += bw + rnd.randint(2, 12)
d.rectangle([0, base, W, H], fill=(10, 18, 42))
for y in range(base + 6, H - 260, 9):                 # water reflections
    for _ in range(14):
        rx = rnd.randint(0, W)
        d.line([(rx, y), (rx + rnd.randint(8, 40), y)], fill=(200, 150, 90))
im = im.filter(ImageFilter.GaussianBlur(0.6))
d = ImageDraw.Draw(im)
d.rectangle([0, H - 270, W, H], fill=NAVY)
d.rectangle([0, H - 270, W, H - 262], fill=GOLD)
d.text((W // 2, 90), "HOME STARTS", font=ImageFont.truetype(FB, 120), fill=WHITE, anchor="ma")
d.text((W // 2, 210), "IN HARBOR METRO", font=ImageFont.truetype(FB, 96), fill=GOLD, anchor="ma")
d.text((W // 2, H - 230), "MERIDIAN MUTUAL", font=ImageFont.truetype(FB, 104), fill=WHITE, anchor="ma")
d.text((W // 2, H - 110), "First-home mortgages  |  Ask a teller today", font=ImageFont.truetype(FSB, 46),
       fill=(190, 198, 215), anchor="ma")
im.save(OUT / "poster_home.png")
print("OK", sorted(p.name for p in OUT.iterdir()))
