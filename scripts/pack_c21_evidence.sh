#!/usr/bin/env bash
# Cycle-21: JPG-ify beauty/peds evidence and pack ONE lean archive for the ChatGPT judge.
set -euo pipefail
FURY=/workspace/Fury
BEAUTY=$FURY/artifacts/aaa_meridian_block/blender_scene_beauty
PEDS=$FURY/artifacts/aaa_meridian_block/blender_peds
DST=/home/box/Downloads/aaa-meridian-c21
PACK=$DST/pack
mkdir -p "$PACK/blender_scene_beauty" "$PACK/blender_peds"
# drop stale C19 pixel crops (retired in C20)
rm -f "$BEAUTY"/03_cruiser_hood_crop.* "$BEAUTY"/03_cruiser_door_crop.* "$BEAUTY"/03_cruiser_glass_crop.*
python3 - "$BEAUTY" <<'PY'
import sys, pathlib
from PIL import Image
d = pathlib.Path(sys.argv[1])
for p in sorted(d.glob("*.png")):
    Image.open(p).convert("RGB").save(p.with_suffix(".jpg"), quality=int(__import__("os").environ.get("JPGQ", "84")), optimize=True)
PY
rm -f "$PACK"/blender_scene_beauty/* "$PACK"/blender_peds/*
cp "$BEAUTY"/*.jpg "$BEAUTY"/README.md "$PACK/blender_scene_beauty/"
cp "$PEDS"/*.jpg "$PEDS"/README.md "$PACK/blender_peds/"
cp "$FURY"/artifacts/aaa_meridian_block/JUDGE_PROMPT.txt "$FURY"/artifacts/aaa_meridian_block/CAPTURE_LOG.md "$PACK/"
cd "$PACK"
if command -v xz >/dev/null 2>&1; then
  tar -cJf "$DST/cycle21_evidence_chatgpt.tar.xz" JUDGE_PROMPT.txt CAPTURE_LOG.md blender_scene_beauty blender_peds
else  # box without xz-utils: Python's lzma writes the same .tar.xz
  python3 -c "import tarfile,sys; t=tarfile.open(sys.argv[1],'w:xz'); [t.add(n) for n in sys.argv[2:]]; t.close()" \
    "$DST/cycle21_evidence_chatgpt.tar.xz" JUDGE_PROMPT.txt CAPTURE_LOG.md blender_scene_beauty blender_peds
fi
ls -la "$DST/cycle21_evidence_chatgpt.tar.xz"
SZ=$(stat -c %s "$DST/cycle21_evidence_chatgpt.tar.xz")
if [ "$SZ" -gt 5000000 ]; then echo "WARN archive > 5MB; rerun with JPGQ=78"; fi
