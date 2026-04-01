---
name: Retina/DPR coordinate fix
description: How the macOS DPR pixel math works after the Retina quarter-size fix
type: feedback
---

All Metal shaders compute NDC as `(pixel_coord / viewport) * 2 - 1`. Draw commands carry logical pixel coordinates. `vp_w_/vp_h_` in the backend stores physical drawable size. The fix seeds `current_transform` in `flushDrawList` with the DPR scale matrix so all draw commands are converted to physical pixels.

**Why:** On 2x Retina, logical coords / physical viewport = 1/4 the screen area (1/2 each dimension). The user reported "quarter of ideal size."

**How to apply:**
- `renderer.cpp::flushDrawList` initializes `current_transform.data[0] = dpr, .data[5] = dpr`
- Transform-aware commands (`drawRect`, `drawCircle`, etc.) automatically get physical coords via the accumulated transform
- Transform-ignorant commands (`drawText`, `drawImage`, `drawBackdropFilter`, `drawShaderMaskComposite`) must apply the passed `transform` to their coordinates manually
- `RenderText` and `RenderParagraph` pre-scale `font_size` by DPR in `performPaint()` before emitting `DrawTextCmd` — do NOT scale font size again in `drawText`. The backend receives a physically-sized font and uses it directly.
- `view_insets_` (from `safeAreaInsets`) are already in logical points — do NOT divide by DPR in `layoutPass`
