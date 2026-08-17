# Proposal: redefining the theme abstraction for Fluent 2 and M3 Expressive

Companion to `fluent_design_abstraction_report.md` and
`m3_expressive_abstraction_report.md`. Those reports each conclude
independently that `DesignSystem`/`DesignTokens` *already* holds the new
theme — this proposal is about what to actually add, in what order, and
what to deliberately defer.

## 0. The abstraction does not need to be redefined

Both reports' verdicts hold up against the code, not just in theory:
`campello_fluent/` already exists — `FluentDesignSystem` is a full,
706-line implementation of every `DesignSystem` builder, with
`light()`/`dark()`/`highContrast()` presets and a runtime accent color,
built entirely on the **current, unmodified** `DesignTokens`. It renders
Fluent surfaces as solid fills (Mica/Acrylic deferred, exactly as the
report recommended) and needed zero changes to the shared interface to do
it. That's the proof this doesn't need a redefinition — it needs
**targeted, additive extensions**, most of which only one theme cares
about.

The rule this proposal follows throughout: **extend `DesignTokens`/
`DesignSystem` only for concepts that are genuinely cross-platform (every
design system could plausibly have one); keep everything else local to the
concrete class**, the same way `CupertinoDesignSystem::buildConfirmationDialog()`
and `FluentMaterial`/`accent_` already live outside the shared interface.

## 1. Six extensions, none of them breaking

| # | Extension | Who needs it | Where it lives | Breaking? |
|---|---|---|---|---|
| 1 | `ContrastLevel` (standard/high) | Fluent high-contrast; also a real iOS/Android accessibility axis | New field on `DesignTokens`, default `standard` | No — new field, default-neutral |
| 2 | Runtime accent/seed color | Fluent (OS accent); M3 (dynamic color / wallpaper seed) | **Not shared.** Stays a constructor param + accessor on the concrete class, exactly as `FluentDesignSystem::accent_` already does | No — no shared change at all |
| 3 | Emphasized typography (15 → 30 styles) | M3 Expressive | 15 new fields on `TypographyScale` (`*_emphasized`); `textStyleForRole()` gains a defaulted `bool emphasized = false` param | No — additive fields, defaulted param |
| 4 | Expanded shape scale (7 → 10 radii) | M3 Expressive | New fields on `ShapeTokens` (`radius_xl_inc`, `radius_xxl`, etc., alongside the existing 7) | No — additive fields |
| 5 | `SpringTokens` (stiffness/damping, spatial + effects) | M3 Expressive | New optional struct nested in `MotionTokens` | No — additive field |
| 6 | Component-size scale (XS–XL) | M3 Expressive | New optional field on `ButtonConfig`/`ChipConfig`/icon-button config, default = current fixed size | No — default-neutral field on shared config, Cupertino/Fluent builders ignore it |

Everything else — new components (`NavigationView`, `CommandBar`,
`InfoBar`, `ButtonGroup`, `SplitButton`, `FABMenu`, `LoadingIndicator`,
docked/floating `Toolbar`) — follows the pattern already established this
session: extra public methods on the concrete class only, never on
`DesignSystem`.

## 2. The two genuinely hard gaps, and how to not get blocked on them

Two items in both reports are real rendering-pipeline projects, not token
work, and trying to do them properly up front would stall everything else:

- **Squircles/scallops/shape morphing** (M3) and **Mica/Acrylic** (Fluent)
  need new shape primitives / blur+noise+blend shaders. Ship V1 with
  ordinary `RRect` at the new radius scale (M3) and solid fills (Fluent,
  already done) — visually close, unblocks fidelity testing today. Do the
  real primitive as its own follow-up project.
- **Spring physics motion** (M3) can't be represented by the existing
  `Curve = double(*)(double)` (a spring's settle time isn't a fixed
  duration — it's an ODE, not a fixed-duration function of normalized
  time). Ship V1 by having the Expressive preset pick the closest
  duration+easing approximation (`easeOutCubic` or a custom bezier tuned
  to typical spring response) for `MotionTokens::curve_emphasized`. Add a
  real `SpringAnimationController` alongside (not replacing)
  `AnimationController` as a separate, later animation-subsystem project;
  swap the token consumption over once it exists.

This mirrors the same "ship an approximation now, invest in the real
primitive later" call in both places — a deliberate, consistent policy,
not two ad-hoc shortcuts.

## 3. Sequencing

**Phase A — unlocks Android fidelity testing now:**
`MaterialDesignSystem::expressiveLight()`/`expressiveDark()` presets:
bolder/higher-contrast `ColorScheme`, baseline (non-emphasized)
`TypographyScale` tuned to the Expressive ramp, existing `ShapeTokens`
rounded rects at up-sized radii, existing easing curves. Skip items 1, 3
(emphasized half), 5, 6 initially — none of them block getting real M3
Expressive components on screen and diffed against Android 16.

**Phase B:** items 1, 3–6 from the table above, plus the expanded shape
scale, as their own follow-ups once Phase A's presets are validated
against real device captures (same incremental-per-item discipline that
worked for the iOS dialog fidelity work this session).

**Phase C — real rendering-pipeline investment:** spring controller,
squircle/scallop primitive + morphing, Mica/Acrylic shaders, variable-font
axes (`TextStyle` currently has no axis support at all — a real gap,
independent of the theme work, needed for Segoe UI Variable and Roboto
Flex fidelity on both platforms eventually).

Fluent itself needs nothing further from this proposal right now —
`campello_fluent` already implements the interface; it's blocked only on
a Windows machine for real fidelity testing (already deferred) and Phase C
materials.

---

Proceeding now: Phase A preset + Android M3 Expressive fidelity harness,
mirroring the `ios_fidelity_reference/` pattern (real-device/emulator
capture, `compare_*.py` pixel diff, incremental per-builder rollout).
