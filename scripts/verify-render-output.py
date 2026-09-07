"""Verify renderer-owned captures and counters without third-party Python packages."""
import json
import pathlib
import sys


def pixels(root, name):
    data = (root / (name + ".ppm")).read_bytes()
    magic, dimensions, maximum, rgb = data.split(b"\n", 3)
    width, height = map(int, dimensions.split())
    assert magic == b"P6" and maximum == b"255", f"{name}: invalid PPM header"
    assert len(rgb) == width * height * 3, f"{name}: incomplete capture"
    return rgb


def main(root):
    summary = json.loads((root / "summary.json").read_text(encoding="utf-8-sig"))
    cases = {case["case"]: case["result"] for case in summary["cases"]}
    for mode in ("native", "fsr", "xess"):
        still = cases[mode + "-still"]
        assert still["tlas_builds"] == 1, f"{mode}: static TLAS was unnecessarily rebuilt"
        assert 0 < still["blas_builds"] < still["instances"], f"{mode}: meshes are not shared"
        assert still["unique_triangles"] < still["triangles"], f"{mode}: instancing not demonstrated"
        moving = cases[mode + "-motion-resize"]
        assert moving["blas_builds"] >= moving["frames"], f"{mode}: deformation did not rebuild geometry"
        assert moving["tlas_builds"] >= moving["frames"] - 1, f"{mode}: moving instances were not updated"
        assert (moving["output_width"], moving["output_height"]) == (960, 540), f"{mode}: resize failed"
    assert "XeSS" in cases["runtime-upscaler-switch"]["upscaler"], "Runtime SDK switching did not reach XeSS"
    neutral = pixels(root, "motion-buffer-static")
    assert min(neutral) >= 127 and max(neutral) <= 128, "Static motion vectors are not zero"
    moving = pixels(root, "motion-buffer-moving")
    assert sum(value not in (127, 128) for value in moving) > len(moving) // 100, "Camera motion vectors are missing"
    depth = pixels(root, "depth-buffer")
    assert max(depth) - min(depth) > 30, "Depth buffer has no useful geometry range"
    normals = pixels(root, "normal-buffer")
    assert max(normals) - min(normals) > 128, "Normal buffer has no surface variation"
    alpha = pixels(root, "alpha-visibility-change")
    assert cases["alpha-visibility-change"]["tlas_builds"] == 2, "Material visibility flags did not update the TLAS"
    for x, y, channel in ((256, 144, 2), (384, 144, 1), (256, 216, 1), (384, 216, 2)):
        sample = alpha[(y * 640 + x) * 3 : (y * 640 + x) * 3 + 3]
        other = 1 if channel == 2 else 2
        assert sample[channel] > sample[other] + 50, f"Alpha cutout visibility is incorrect at {(x, y)}: {sample}"
    print("Capture sizes, zero/static and moving vectors, depth, normals, instancing, deformation and SDK switching verified.")


if __name__ == "__main__":
    main(pathlib.Path(sys.argv[1]))
