# Report: M3 Expressive — fit with `campello_widgets` design-system abstraction

Sources consulted:
- [Material 3 Expressive Design Guidelines (canine-labs mirror)](https://github.com/canine-labs/m3-expressive-design-md/blob/main/DESIGN.md)
- [Supercharge design overview](https://supercharge.design/blog/material-3-expressive)
- [UX Encyclopedia summary](https://ux.detroit3d.com/design-systems/material-design-summary.html)

---

## 1. What M3 Expressive is

M3 Expressive is **not a new version** of Material Design; it is an expansion of Material 3 / Material You announced at Google I/O 2025 and rolling out with Android 16. Its purpose is to move M3 away from ultra-flat sameness toward more emotionally engaging, research-backed interfaces.

Four axes drive it:

1. **Vibrant, higher-contrast color** — bolder use of dynamic/tonal palettes.
2. **Expressive typography** — variable fonts (e.g. Roboto Flex) and an **“Emphasized”** variant for every baseline type role.
3. **Intentional, physics-based motion** — **spring animations** replace the old duration+easing token model. Two schemes: *spatial springs* (movement/scale) and *effects springs* (color/opacity/shape).
4. **Contrasting shapes + shape morphing** — a library of ~35 shapes (squircles, scallops, bursts, etc.) and animated morphing between them.

Concrete spec changes:

- **Typography**: 15 baseline styles → **30 styles** (baseline + emphasized for each role).
- **Shape tokens**: expanded radius scale (None, XS 4dp, S 8dp, M 12dp, L 16dp, L-Inc 20dp, XL 28dp, XL-Inc 32dp, XXL 48dp, Full).
- **Component sizes**: XS/S/M/L/XL height/padding scale for buttons, icon buttons, chips.
- **New components**: `ButtonGroup`, `SplitButton`, `FABMenu`, `LoadingIndicator`, docked/floating `Toolbar`, vertical carousel, etc.
- **Updated components**: buttons, FABs, cards, navigation bar/rail, progress indicators.

---

## 2. Mapping to the current abstraction

| M3 Expressive concept | Current `campello_widgets` support | Fit |
|---|---|---|
| Semantic color roles (`primary`, `surface`, `outline`, …) | `ColorScheme` already has these roles | ✅ Good |
| Dynamic color / HCT tonal palettes | Static `ColorScheme` only; no HCT generator | ⚠️ Addable |
| 30-style type scale (baseline + emphasized) | `TypographyScale` has 15 slots | ⚠️ Extendable |
| Variable font axes | `TextStyle` — needs verification of variable-font support | ⚠️ Possible gap |
| 10-level shape radii | `ShapeTokens` has 6 radii | ⚠️ Extendable |
| Squircles, scallops, ~35 shapes + morphing | `RRect`/`RRectComplex` only support circular corners; `Path` exists | ⚠️ Needs backend work |
| Spring physics motion | `AnimationController` + `CurvedAnimation` + `Tween`; only easing curves, no springs | ⚠️ Needs new animation layer |
| XS/S/M/L/XL component sizing | Not modeled; builders pick hard-coded sizes | ⚠️ Add token or config |
| New components (ButtonGroup, FABMenu, Toolbar, …) | `DesignSystem` interface does not declare them | ✅ Can be added to `MaterialDesignSystem` only |

---

## 3. Gaps that must be closed

### 3.1 Motion: no spring physics today
`MotionTokens` stores durations and `Curve` function pointers (`Curves::easeInOut`, etc.). M3 Expressive replaces this with spring physics (stiffness/damping). To support it faithfully:

- **Minimum**: add a new spring curve type or a `SpringTokens` struct inside `MotionTokens` so M3 Expressive can specify stiffness/damping per scheme.
- **Better**: implement a real spring simulation in the animation system, because springs do **not** map cleanly to a fixed duration. `AnimationController` currently drives a value from `lower` to `upper` over `duration_ms`. A spring controller would need target-based convergence instead.

This is the **deepest** change, but it is local to the animation subsystem; it does not break `DesignSystem`.

### 3.2 Shapes: only circular rounded rectangles
`RRectComplex` supports per-corner circular radii, but M3 Expressive uses **squircles, scallops, bursts, and morphing** between them. Faithful support needs:

- A richer shape primitive (beyond `RRect`) or shader-based shape rendering.
- Shape-morphing animation between arbitrary shapes.
- Expansion of `ShapeTokens` from 6 to ~10 radius levels.

This is a **rendering-pipeline** gap, not an abstraction gap.

### 3.3 Typography: 15 styles vs. 30 styles
M3 Expressive needs an “Emphasized” variant for every baseline role. Options:

- Add 15 new fields to `TypographyScale` (e.g. `display_large_emphasized`, …).
- Or make `TypographyScale` hold an array / map keyed by `(role, emphasized)`.

Either way, `TextRole` and `textStyleForRole()` need extension. This is additive.

### 3.4 Variable fonts
M3 Expressive expects variable fonts like Roboto Flex. If `TextStyle` / the text renderer does not yet expose font-variation axes (weight, width, slant, grade), that is a real gap independent of the design-system abstraction.

### 3.5 Component sizing scale
The XS/S/M/L/XL container scale is not modeled anywhere. This can be added as a new `ComponentSize` enum + token set, or as extra fields in `SpacingTokens`/`ShapeTokens`. It does not require breaking `DesignSystem`.

### 3.6 New components
`ButtonGroup`, `SplitButton`, `FABMenu`, `LoadingIndicator`, and floating/docked `Toolbar` are new. Because they are Material-specific, the cleanest approach is **not** to add them to the base `DesignSystem` interface, but to implement them as new methods on `MaterialDesignSystem` (or as standalone widgets in `campello_material`). This keeps the abstraction clean.

---

## 4. Verdict

**The `DesignSystem` abstraction can hold M3 Expressive.**

M3 Expressive is an **evolution, not a revolution**. It still thinks in the same primitives the abstraction already has: color roles, shape tokens, type roles, spacing, motion, and component builders. None of its additions require removing or redefining the existing `DesignSystem` interface.

What is required:

1. **Extend `DesignTokens`** — more shape radii, emphasized type styles, spring-motion tokens, and possibly a component-size scale.
2. **Extend the animation system** — add spring-based controllers alongside the existing duration-based `AnimationController`.
3. **Extend the rendering backend** — support non-circular shapes and shape morphing.
4. **Extend `MaterialDesignSystem`** — new M3 Expressive components and the updated visual treatment of existing ones.

`MaterialDesignSystem` can absorb M3 Expressive as a **new preset** (e.g. `MaterialDesignSystem::expressiveLight()` / `expressiveDark()`) plus the new component builders, without forcing Cupertino or Fluent to care about it.

---

## 5. Recommended next step

If this is pursued later, start with a **token-only spike**:

- Add `ShapeTokens` levels for the M3 Expressive radii.
- Add emphasized variants to `TypographyScale`.
- Add a `SpringTokens` or spring-curve field to `MotionTokens`.
- Create `MaterialDesignSystem::expressiveLight()` / `expressiveDark()` presets that return these extended tokens.

That proves the abstraction holds before investing in spring animation or squircle shaders.
