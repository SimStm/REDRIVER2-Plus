---
type: Playbook
title: Native texture mip ownership and cutouts
created: 2026-10-07
tags: [okf, rendering, ownership, textures]
---

# Native texture mip ownership and cutouts

**When** adding or changing native mip sampling, **then** preserve the ownership,
filtering and transition checks below and corroborate them with actual GPU pixels.

Native trilinear materials own every RGBA mip level. Validate dimensions and
charge the complete chain against the material cap before traversing caller
pixels; retain those bytes while submitted upload work is pending retirement.
Level zero stays byte-exact. Lower levels filter RGB in linear light weighted
by alpha, then unpremultiply. Keep zero-alpha holes and choose the closest
attainable alpha-test coverage using the shader's normalized float cutoff.

NPOT area footprints include every edge texel; each dimension halves with a
minimum of one. Upload, barriers, views and sampler LOD bounds cover the same
chain. A tiny mip or equal-alpha bucket cannot represent every source coverage
fraction; test actual silhouettes instead of promising exact coverage at1x1.

Changing a runtime filter must not require overlapping complete old/new texture
sets beyond the declared cap. Keep original full mip chains and copy sampling
selection into each instance; bind prewritten retained descriptor sets without
mutating resources in flight. A recovered steady-state sample does not prove
the absence of temporary fallback: compare cumulative effective/fallback and
upload counters across every intervening submission, plus actual sampled pixels.

Native filtered artwork now fetches decoded sRGB texels and mixes RGB/alpha in
FP32. Query selected implicit LOD before per-fragment discard in draw-uniform
control flow; clamp UVs before integer conversion and mip indices to owned levels.
Do not read the next mip at an integer LOD. Preserve base-only bilinear and
nearest semantics, and verify fractional mip blends and extreme UV/LOD bounds
with GPU pixels. Sampling uses the private push padding word; public ABI and
frame-owned descriptors/retirement remain unchanged. Track four/eight fetch
cost separately from colour correctness; a passing pixel does not prove budget.
