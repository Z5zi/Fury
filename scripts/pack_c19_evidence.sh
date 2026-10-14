#!/usr/bin/env bash
# Cycle-19: JPG-ify beauty/peds evidence and pack ONE lean archive for the ChatGPT judge.
set -euo pipefail
FURY=/workspace/Fury
BEAUTY=$FURY/artifacts/aaa_meridian_block/blender_scene_beauty
PEDS=$FURY/artifacts/aaa_meridian_block/blender_peds
DST=/home/box/Downloads/aaa-meridian-c19
PACK=$DST/pack
mkdir -p "$PACK/blender_scene_beauty" "$PACK/blender_peds"
python3 - "$BEAUTY" <<'PY'
import sys, pathlib
from PIL import Image
d = pathlib.Path(sys.argv[1])
for p in sorted(d.glob("*.png")):
    Image.open(p).convert("RGB").save(p.with_suffix(".jpg"), quality=86, optimize=True)
PY
rm -f "$PACK"/blender_scene_beauty/* "$PACK"/blender_peds/*
cp "$BEAUTY"/*.jpg "$BEAUTY"/README.md "$PACK/blender_scene_beauty/"
cp "$PEDS"/*.jpg "$PEDS"/README.md "$PACK/blender_peds/"
cd "$PACK"
tar -cJf "$DST/cycle19_evidence_chatgpt.tar.xz" JUDGE_PROMPT.txt CAPTURE_LOG.md blender_scene_beauty blender_peds
ls -la "$DST/cycle19_evidence_chatgpt.tar.xz"
