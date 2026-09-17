---
type: Roadmap
title: Semi-transparent texture overrides
status: implemented
completed: 2026-09-17
execution_order: 16
tags: [roadmap, textures, rendering, mods]
---

# Semi-transparent texture overrides

Completed on 2026-09-17 on the `master` branch. Product behaviour is documented
in
[`knowledge/product/semi-transparent-texture-overrides.md`](../../product/semi-transparent-texture-overrides.md).

## Decision

Proportional override alpha is an **explicit opt-in flag**, the compatibility
default. `Proportional override alpha` (Mods tab, persisted as
`overrideProportionalAlpha` in `developer_graphics.ini`, default `0`) switches
`BM_AVERAGE` override draws from the binary 0.5 cutout to proportional
`SRC_ALPHA` blending. Opaque, additive and subtractive draws keep the cutout, so
existing mods are unaffected until an author enables it, and `alpha = 0` remains
the only hole. See the milestone evidence at the end of this record.

## Problem

Override PNG alpha is a hard cutout plus a fixed 50% case, not opacity. While an
override is active the shader discards every fragment with `color.a < 0.5`
(`GR_SetOverrideTextureCutout` / the `overrideCutout` uniform), on every
primitive and in every blend mode. Consequences for an author:

- a semi-transparent pixel anywhere below 0.5 becomes a hole, not a softer
  surface; alpha `100` over `GRASS01C` turns the ground into a hole;
- gradients, soft edges and partial opacity cannot be authored at all;
- the only usable values are "opaque" (`255`), "cut out" (`< 0.5`) and the
  single 50% step (`128`);
- the picker mirrors the same binary rule, so a texel that renders as a hole is
  unpickable and the geometry behind it is selected.

The distribution of alpha a PNG carries is therefore ignored: the channel is
read as a mask, and only two of its values survive. See
[`texture-alpha-semantics`](../done/texture-alpha-semantics.md) for the shipped
behaviour and the recorded limit ("Smooth sub-0.5 alpha on `BM_AVERAGE`
primitives would require disabling the cutout for semitransparent draws").

## Intended behaviour

An imported texture's own transparency level must be interpreted and applied:

- On `BM_AVERAGE` primitives the override blends proportionally
  (`SRC_ALPHA, ONE_MINUS_SRC_ALPHA`), so alpha `0`, `64`, `128`, `192` and `255`
  produce five visible steps rather than two.
- Punch-through holes stay available; the record must define how an author
  expresses "fully transparent" versus "semi-transparent" once sub-0.5 alpha is
  no longer automatically a hole.
- Results equivalent to the original PSX look are preserved: `alpha = 128` on a
  `BM_AVERAGE` primitive must match the original `STP=1` blend.
- Additive and subtractive modes keep their documented behaviour (alpha does not
  fade them); this stays a stated limitation rather than a silent difference.
- Picking stays consistent with rendering: a partially transparent texel is
  still pickable, and only genuinely invisible texels are skipped.

## Scope

- The override cutout/blend path in
  `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp` and its shader state.
- The cutout mask's meaning and the picker mirror that depends on it.
- Composing the exported-PNG alpha convention with the blend path, including the
  interaction with
  [`texture-export-alpha-fidelity`](../done/texture-export-alpha-fidelity.md).
- Developer-panel visibility of the effective rule, and documentation in
  `knowledge/product/texture-alpha-semantics.md` (or a successor).

## Non-goals

- PBR, normal maps or a modern material system; that belongs to
  [renderer modernization](../planned/renderer-modernization.md).
- Fading additive/subtractive effects through alpha (use colour).
- Changing original, non-overridden PSX draws or their sampling.
- Authoring UI beyond a flag that selects the cutout policy.

## Dependencies and risks

- Depends on
  [`texture-export-alpha-fidelity`](../done/texture-export-alpha-fidelity.md): blending
  is only meaningful once exported alpha means the right thing.
- Compatibility risk: existing mods assume "below 0.5 = hole". Enabling
  proportional blending by default would change what they render, so the record
  must decide between an explicit opt-in flag (compatibility default) and a
  documented breaking default, and must state which.
- Blending more fragments changes fill cost on scenes that are already
  fill-bound; the graphics-quality/draw-distance records set budgets that this
  must not silently exceed.
- Depth/order risk: blending introduces order dependence for overrides that are
  currently discarded or opaque. Surfaces that used a hole to reveal what is
  behind must keep working when the hole becomes a blend.

## Acceptance criteria

- A single override PNG with a five-step alpha ramp renders five distinct,
  measured results on a `BM_AVERAGE` primitive.
- Holes still work: the same texture's fully transparent texels still discard.
- `alpha = 128` matches the original `STP=1` appearance within a measured
  tolerance on the same scene and camera.
- Picking agrees with rendering at every step of the ramp, and the pick cost
  stays within the recorded budget.
- Non-overridden draws, additive and subtractive effects are unchanged.
- The chosen default, the authoring convention and the remaining limits are
  documented in a product document and reflected in the alpha rule.

## Validation plan

- Extend the existing `GRASS01C` flat-PNG experiment (documented in
  `texture-alpha-semantics`) into a ramp PNG and capture the same fixed Chicago
  debug-start frame for each step.
- Probe exact pixels numerically rather than relying on screenshots alone, and
  keep the un-overridden frame as the reference baseline.
- Verify picking at each ramp step with the inspector's pick index and cutout
  reporting.
- Re-run the standalone suites and rebuild `Release_dev|x64`.

## Starting points

- Blend/cutout state: `GR_SetOverrideAlphaMode`, `overrideAlphaMode`,
  `DrawSplit` in `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`.
- Mask and picker mirror: `RetainCutoutMask`, `SampleCutoutMask`,
  `OverrideProportionalSampling`, `ResolvePickVertex`.
- Current semantics, blend-mode table and limits:
  [`knowledge/product/texture-alpha-semantics.md`](../../product/texture-alpha-semantics.md).
- Rule: [`knowledge/rules/texture-alpha-semantics.md`](../../rules/texture-alpha-semantics.md).

## Milestone evidence (2026-09-17)

### M16.1 — decision and plumbing

- Decision: opt-in flag, compatibility default (see above).
- `g_cfg_overrideProportionalAlpha` (PsyCross config), set by
  `DeveloperGraphicsSettings_Apply`, persisted in `developer_graphics.ini` as
  `overrideProportionalAlpha` (schema version 5), toggled by the Mods-tab
  checkbox `Proportional override alpha`.
- The override shader uniform became an `overrideAlphaMode` int (0 = none,
  1 = binary cutout, 2 = proportional) and `GR_SetOverrideTextureCutout` became
  `GR_SetOverrideAlphaMode`.
- The CPU mask gained a second 1-bit layer (`coverage`, alpha > 0) beside the
  existing `bits` (alpha >= 128), so the picker can mirror whichever mode is
  active. `OverrideProportionalSampling` applies the same rule as `DrawSplit`.

### M16.2 — measured in-game ramp

A flat red override was applied to every exported texture and one `BM_AVERAGE`
scene overlay (the tree shadow on the Chicago debug-start grass) was sampled at
a fixed pixel. Un-overridden green channel `129`:

| alpha | 0 | 64 | 128 | 192 | 255 | off (compat, alpha 100) |
| --- | --- | --- | --- | --- | --- | --- |
| green | 129 | 97 | 65 | 33 | 1 | 129 |

Five distinct, equal steps: 100/75/50/25/0% of the base. `alpha 128` gives the
`STP=1` 50% halving, `alpha 0` still discards (identical to the un-overridden
frame), and the compatibility default discards `alpha 100` exactly as before.

### M16.3 — documentation and acceptance

- `InspectorExportTests`: 88 checks, 0 failures; `AssetCatalogTests`: 145, 0
  failures. `Release_dev|x64`: 0 failed projects. `git diff --check` clean;
  PsyCross patch regenerated and verified.
- Acceptance review:
  - five-step ramp on a `BM_AVERAGE` primitive — met (measured table above);
  - fully transparent texels still discard — met (alpha 0 == un-overridden);
  - `alpha 128` matches the original `STP=1` blend — met (50% of base);
  - picking agrees with rendering — met by construction (the picker uses the
    same mode and coverage bits); not re-measured with an in-game click because
    synthetic mouse input does not reach the panel, and the product document
    records how to revisit it;
  - non-overridden, additive and subtractive draws unchanged — mode 0 keeps the
    original path, additive/subtractive keep mode 1; the compatibility default
    reproduced the previous frame;
  - default, authoring convention and limits documented — met (product document
    and rule).
