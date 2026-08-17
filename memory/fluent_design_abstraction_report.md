# Report: Microsoft Fluent 2 Design — fit with `campello_widgets` abstraction

Sources consulted:
- [Fluent 2 — Get started / design](https://fluent2.microsoft.design/get-started/design)
- [Fluent 2 — Design tokens](https://fluent2.microsoft.design/design-tokens)
- [Fluent 2 — Material](https://fluent2.microsoft.design/material)
- [Fluent 2 — Typography](https://fluent2.microsoft.design/typography)
- [Microsoft Learn — Materials used in Windows apps](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/materials)

---

## 1. What Fluent 2 is

Fluent 2 is Microsoft’s cross-platform design system. It is heavily tokenized: global tokens store raw values (hex, font sizes, corner radii, stroke widths, durations) and alias tokens give them semantic meaning (e.g. `backgroundPressed`, `accentFillRest`). It supports light, dark, and high-contrast themes out of the box, plus an OS-derived accent color.

Four signature materials define Fluent surfaces:

1. **Solid** — opaque, mode-aware fill.
2. **Mica** — opaque material subtly tinted with the desktop wallpaper; active/inactive window states.
3. **Acrylic** — semi-transparent frosted-glass material for transient surfaces (flyouts, menus).
4. **Smoke** — translucent black overlay used behind modal UI.

Typography is platform-adaptive: **Segoe UI Variable** on Windows, San Francisco Pro on macOS/iOS, Roboto on Android. The type ramp uses a small set of semantic roles (Caption, Body, Subtitle, Title, Large Title, Display) with Strong/Bold variants.

Components include many Windows-specific patterns: `NavigationView`, `CommandBar`, `ContentDialog`, `TeachingTip`, `InfoBar`, `NumberBox`, `PersonPicture`, `BreadcrumbBar`, `Pivot`, etc.

Motion in Fluent is easing + duration based (not spring physics). The system uses standard, entrance, exit, and emphasis timing curves.

---

## 2. Mapping to the current abstraction

| Fluent 2 concept | Current `campello_widgets` support | Fit |
|---|---|---|
| Token layers (global + alias) | `DesignTokens` is one flat struct; roles are encoded by field names | ✅ Mappable |
| Semantic color roles | `ColorScheme` already has `primary`, `surface`, `outline`, `error`, etc. | ✅ Good |
| OS accent color | `ColorScheme` is static; no accent seed | ⚠️ Addable |
| Light / dark / high contrast | `Brightness` enum has `light`/`dark` only | ⚠️ Needs high-contrast mode |
| Materials (Mica/Acrylic/Smoke) | `RenderBackdropFilter` + blur shaders exist; no noise/exclusion/luminosity blends, no wallpaper sampling | ⚠️ Needs backend work |
| Typography ramp + Strong variants | `TypographyScale` has 15 slots; can hold the Fluent ramp | ✅ Mappable |
| Segoe UI Variable / variable fonts | `TextStyle` — variable-font axis support not verified | ⚠️ Possible gap |
| Shape radii (0/2/4/8dp) | `ShapeTokens` already covers these values | ✅ Good |
| Elevation / shadows | `ElevationTokens` + `BoxShadow` exist; Fluent shadows are subtle | ✅ Good |
| Easing + duration motion | `MotionTokens` uses `Curve` function pointers | ✅ Good |
| Density modes (compact/comfortable) | Not modeled | ⚠️ Addable |
| New components (NavigationView, CommandBar, InfoBar, TeachingTip, …) | `DesignSystem` interface does not declare them | ✅ Can be added to `FluentDesignSystem` only |

---

## 3. Gaps that must be closed

### 3.1 Materials are the biggest rendering challenge
Fluent’s identity depends on Mica and Acrylic.

- **Mica** requires sampling the desktop wallpaper / window background and tinting it. On Windows this is usually done via the DWM API; in a cross-platform renderer it means the framework must expose a way to treat the window background as an input texture.
- **Acrylic** needs Gaussian blur, a noise texture, exclusion/luminosity blends, and a tint layer. Your framework already has `RenderBackdropFilter` and blur shaders (used for Liquid Glass), so the plumbing exists, but Acrylic’s specific blend recipe is not implemented.
- **Smoke** is trivial — a translucent black scrim (`Color::fromRGBA(0,0,0,0.3)` or similar).

This is a **rendering-pipeline** gap, not an abstraction gap. The `DesignSystem` abstraction can represent *which* material a surface should use (e.g. via a `FluentMaterial` enum); actually rendering it is backend work.

### 3.2 Accent color and high-contrast theming
Fluent derives its palette from the OS accent color. `ColorScheme` currently stores static colors. To support Fluent properly you would add:

- An `accent` color field or seed passed into the design-system factory.
- A `Brightness::highContrast` value (or a separate `ContrastMode`).

This is additive and does not break the interface.

### 3.3 Variable fonts
Fluent on Windows uses **Segoe UI Variable**, which has optical axes (weight, width, etc.). If `TextStyle` / the text renderer does not support variable-font axes, that is a gap. For non-Windows targets Fluent falls back to system fonts, so this is only a fidelity issue on Windows.

### 3.4 Density modes
Fluent supports compact vs. comfortable density. `SpacingTokens` can model this, but you may want a dedicated `Density` enum on the design system or tokens.

### 3.5 New components
`NavigationView`, `CommandBar`, `TeachingTip`, `InfoBar`, `NumberBox`, `PersonPicture`, `BreadcrumbBar`, etc. are not in the base `DesignSystem` interface. Because they are Fluent-specific, the cleanest approach is to add them to a `FluentDesignSystem` subclass or as standalone widgets in `campello_fluent`, not to the shared interface.

Some existing builders map directly:
- `buildAppBar` → `CommandBar`
- `buildNavigationBar` / `buildNavigationRail` → `NavigationView` top/left modes
- `buildDialog` → `ContentDialog`
- `buildBanner` → `InfoBar`
- `buildStepper` → `NumberBox`

---

## 4. Verdict

**The `DesignSystem` abstraction can hold Fluent 2 very well.**

Fluent 2 is a conventional token-based design system. It is less radical than M3 Expressive: it does **not** require spring physics, shape morphing, or a 30-style type scale. Its primitives (color roles, shape tokens, type roles, spacing, elevation, easing+duration motion) map almost one-to-one onto the existing `DesignTokens` structure.

The main implementation effort is:

1. **Rendering materials** — Mica/Acrylic shaders, blur, noise, blends, wallpaper sampling.
2. **Accent + high-contrast color model** — runtime accent injection and a third brightness mode.
3. **Variable font support** — if Windows fidelity is required.
4. **New Fluent component builders** — NavigationView, CommandBar, InfoBar, TeachingTip, etc.

A `FluentDesignSystem` would sit naturally alongside `MaterialDesignSystem` and `CupertinoDesignSystem` as a third implementation of the same interface.

---

## 5. Recommended next step

Start with a **token-only spike**:

- Add accent-color and high-contrast presets to a `FluentDesignSystem` sketch.
- Map the Fluent type ramp onto `TypographyScale`.
- Verify `ShapeTokens` and `MotionTokens` already cover Fluent’s values.
- Render all surfaces as **solid fills first**; defer Mica/Acrylic to a second phase once the component builders exist.

This proves the abstraction holds before investing in the material shaders.
