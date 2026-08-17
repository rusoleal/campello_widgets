#!/usr/bin/env python3
"""
Compare iOS UIKit reference screenshots (ios_fidelity_reference/output)
against campello_widgets C++ rendered screenshots (tests/visual_fidelity/cpp_output).

Produces:
- tests/visual_fidelity/diffs/<theme>/<name>_diff.png
- tests/visual_fidelity/reports/<theme>.json
- tests/visual_fidelity/reports/<theme>.md
"""

import json
import os
import sys
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent.parent
REF_DIR = ROOT / "ios_fidelity_reference" / "output"
CPP_DIR = ROOT / "tests" / "visual_fidelity" / "cpp_output"
DIFF_DIR = ROOT / "tests" / "visual_fidelity" / "diffs"
REPORT_DIR = ROOT / "tests" / "visual_fidelity" / "reports"

TOLERANCE = 8  # per-channel difference (0-255)


def ensure_dirs():
    for d in (DIFF_DIR, REPORT_DIR):
        d.mkdir(parents=True, exist_ok=True)


def load_image(path: Path) -> Image.Image:
    return Image.open(path).convert("RGBA")


def downscale_to_match(ref_img: Image.Image, cpp_img: Image.Image) -> tuple[Image.Image, Image.Image]:
    """Reconciles ref_img/cpp_img to the same size, returning (ref, cpp).

    Same width, different height (the real-capture dialog cases: the iOS
    side is cropped to its safe area, status bar/home indicator removed;
    the C++ side renders the full screen) — center-crop the taller one
    instead of stretching, since a non-uniform resize shifts content by
    dozens of pixels and produces a meaningless diff. Any other mismatch
    falls back to a uniform resize of ref to cpp's size.
    """
    if ref_img.size == cpp_img.size:
        return ref_img, cpp_img
    if ref_img.width == cpp_img.width and ref_img.height != cpp_img.height:
        # Not a symmetric center-crop: the real-capture cases (currently
        # just dialog_*) crop away a real device's status bar (186px
        # physical, iPhone 16/17 Pro) and home indicator asymmetrically —
        # themed_component_harness.cpp's renderDialogCase() centers its
        # dialog within that same excluded top margin, so the crop here
        # must use the identical offset for the two sides to align.
        top_margin = 186
        taller, shorter = (cpp_img, ref_img) if cpp_img.height > ref_img.height else (ref_img, cpp_img)
        cropped = taller.crop((0, top_margin, taller.width, top_margin + shorter.height))
        return (cropped, shorter) if taller is ref_img else (shorter, cropped)
    if ref_img.width == cpp_img.width * 3 and ref_img.height == cpp_img.height * 3:
        return ref_img.resize(cpp_img.size, Image.Resampling.LANCZOS), cpp_img
    # Fallback: resize reference to C++ size.
    return ref_img.resize(cpp_img.size, Image.Resampling.LANCZOS), cpp_img


def _per_pixel_max_diff(ref_img: Image.Image, cpp_img: Image.Image) -> np.ndarray:
    """Per-pixel max-abs-diff across RGBA channels, vectorized with numpy.
    Pure-Python per-pixel loops over full-resolution (1179x2556, ~3M px)
    images were the original implementation — measured at 10+ minutes
    without finishing even the first theme's ~58 images. This is the same
    comparison, just computed as array operations instead of a Python
    for-loop, which is several orders of magnitude faster for this size.
    """
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
        cpp_img = load_image(cpp_path)
        ref_scaled, cpp_scaled = downscale_to_match(ref_img, cpp_img)

        stats = compare_images(ref_scaled, cpp_scaled)
        stats["name"] = name
        stats["status"] = "ok" if stats["diff_percent"] == 0 else "diff"

        diff_img = generate_diff_image(ref_scaled, cpp_scaled)
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
            if r["status"] == "missing_cpp":
                f.write(f"| {i} | {r['name']} | — | — | — | — | missing C++ |\n")
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
    themes = ["cupertino_light", "cupertino_dark", "liquid_glass_light", "liquid_glass_dark"]
    if len(sys.argv) > 1:
        themes = [t for t in sys.argv[1:] if (REF_DIR / t).exists()]

    for theme in themes:
        process_theme(theme)


if __name__ == "__main__":
    main()
