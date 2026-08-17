#!/usr/bin/env python3
"""
Compare android_fidelity_reference's real M3 Expressive screenshots
(android_fidelity_reference/output) against campello_widgets C++ rendered
screenshots (tests/visual_fidelity/cpp_output), for the expressive_light/
expressive_dark themes.

Produces:
- tests/visual_fidelity/diffs/<theme>/<name>_diff.png
- tests/visual_fidelity/reports/<theme>.json
- tests/visual_fidelity/reports/<theme>.md

Mirrors compare_ios_cpp.py's structure/metrics; unlike the iOS side, both
captures are already the same size (both harnesses render at the exact same
Pixel 7 canvas — 1080x2400 physical, real-device crop already applied at
export time in android_fidelity_reference/export_references.sh), so no
size-reconciliation step is needed here.
"""

import json
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent.parent
REF_DIR = ROOT / "android_fidelity_reference" / "output"
CPP_DIR = ROOT / "tests" / "visual_fidelity" / "cpp_output"
DIFF_DIR = ROOT / "tests" / "visual_fidelity" / "diffs"
REPORT_DIR = ROOT / "tests" / "visual_fidelity" / "reports"

TOLERANCE = 8  # per-channel difference (0-255)

# android_fidelity_reference/export_references.sh crops the real device's
# status bar (136px) and navigation bar (63px) off its raw screencap before
# saving — real chrome, not app content (see that script's comment; exact
# values from `adb shell dumpsys window displays`' statusBars/
# navigationBars InsetsSource frames on the campello_m3_expressive_test
# emulator). The C++ side renders the full 1080x2400 canvas since it has no
# OS chrome to begin with, but still *centers* its content within that full
# height — matching how Compose's fillMaxSize()+Alignment.Center centers
# relative to the whole screen regardless of system bars drawn on top of it
# — so cropping the same margins here keeps both sides' centering reference
# frame identical, the same reconciliation approach used for the iOS side.
STATUS_BAR_PX = 136
NAV_BAR_PX = 63


def crop_cpp_to_match(cpp_img: Image.Image, ref_size: tuple[int, int]) -> Image.Image:
    if cpp_img.size == ref_size:
        return cpp_img
    w, h = cpp_img.size
    cropped = cpp_img.crop((0, STATUS_BAR_PX, w, h - NAV_BAR_PX))
    return cropped


def ensure_dirs():
    for d in (DIFF_DIR, REPORT_DIR):
        d.mkdir(parents=True, exist_ok=True)


def load_image(path: Path) -> Image.Image:
    return Image.open(path).convert("RGBA")


def _per_pixel_max_diff(ref_img: Image.Image, cpp_img: Image.Image) -> np.ndarray:
    ref_arr = np.asarray(ref_img, dtype=np.int16)
    cpp_arr = np.asarray(cpp_img, dtype=np.int16)
    return np.abs(ref_arr - cpp_arr).max(axis=2)


def compare_images(ref_img: Image.Image, cpp_img: Image.Image):
    pd = _per_pixel_max_diff(ref_img, cpp_img)
    total = pd.size
    mask = pd > TOLERANCE
    diff_pixels = int(mask.sum())
    max_diff = int(pd.max()) if total else 0
    total_diff = int(pd[mask].sum()) if diff_pixels else 0

    diff_percent = (diff_pixels / total) * 100.0 if total else 0.0
    avg_diff = total_diff / diff_pixels if diff_pixels else 0.0
    return {
        "total_pixels": total,
        "diff_pixels": diff_pixels,
        "diff_percent": round(diff_percent, 2),
        "max_diff": max_diff,
        "avg_diff": round(avg_diff, 2),
    }


def generate_diff_image(ref_img: Image.Image, cpp_img: Image.Image) -> Image.Image:
    width, height = ref_img.size
    pd = _per_pixel_max_diff(ref_img, cpp_img)
    mask = pd > TOLERANCE

    out = np.zeros((height, width, 4), dtype=np.uint8)
    intensity = np.clip(pd, 0, 255).astype(np.uint8)
    out[..., 0] = np.where(mask, intensity, 0)   # R
    out[..., 3] = np.where(mask, 128, 0)          # A

    return Image.fromarray(out, mode="RGBA")


def process_theme(theme: str):
    ref_theme_dir = REF_DIR / theme
    cpp_theme_dir = CPP_DIR / theme
    diff_theme_dir = DIFF_DIR / theme
    diff_theme_dir.mkdir(parents=True, exist_ok=True)

    if not ref_theme_dir.exists():
        print(f"Reference theme directory missing: {ref_theme_dir}")
        return None
    if not cpp_theme_dir.exists():
        print(f"C++ theme directory missing: {cpp_theme_dir}")
        return None

    results = []

    for ref_path in sorted(ref_theme_dir.glob("*.png")):
        name = ref_path.stem
        cpp_path = cpp_theme_dir / ref_path.name
        if not cpp_path.exists():
            results.append({
                "name": name,
                "status": "missing_cpp",
                "diff_percent": 100.0,
            })
            continue

        ref_img = load_image(ref_path)
        cpp_img = crop_cpp_to_match(load_image(cpp_path), ref_img.size)
        if ref_img.size != cpp_img.size:
            print(f"WARNING: size mismatch for {name}: ref={ref_img.size} cpp={cpp_img.size}")
            results.append({
                "name": name,
                "status": "size_mismatch",
                "diff_percent": 100.0,
            })
            continue

        stats = compare_images(ref_img, cpp_img)
        stats["name"] = name
        stats["status"] = "ok" if stats["diff_percent"] == 0 else "diff"

        diff_img = generate_diff_image(ref_img, cpp_img)
        diff_path = diff_theme_dir / f"{name}_diff.png"
        diff_img.save(diff_path)
        stats["diff_image"] = str(diff_path.relative_to(ROOT))
        results.append(stats)

    results.sort(key=lambda r: (-r.get("diff_percent", 0), r["name"]))

    json_path = REPORT_DIR / f"{theme}.json"
    with open(json_path, "w") as f:
        json.dump(results, f, indent=2)

    md_path = REPORT_DIR / f"{theme}.md"
    with open(md_path, "w") as f:
        f.write(f"# Fidelity Report: {theme}\n\n")
        f.write(f"Tolerance: {TOLERANCE}/255 per channel\n\n")
        f.write("| Rank | Component | Diff % | Pixels | Max Δ | Avg Δ | Status |\n")
        f.write("|------|-----------|--------|--------|-------|-------|--------|\n")
        for i, r in enumerate(results, 1):
            if r["status"] in ("missing_cpp", "size_mismatch"):
                f.write(f"| {i} | {r['name']} | — | — | — | — | {r['status']} |\n")
            else:
                f.write(
                    f"| {i} | {r['name']} | {r['diff_percent']}% | "
                    f"{r['diff_pixels']}/{r['total_pixels']} | {r['max_diff']} | "
                    f"{r['avg_diff']} | {r['status']} |\n"
                )

    print(f"{theme}: {len(results)} cases, report: {md_path}")
    return results


def main():
    ensure_dirs()
    themes = ["expressive_light", "expressive_dark"]
    for theme in themes:
        process_theme(theme)


if __name__ == "__main__":
    main()
