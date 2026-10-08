---
type: Evidence
title: R03 complete tested backdrop pixels
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# R03 complete tested backdrop pixels

The existing west-road normal/wide RenderDoc1.41 captures now pass the complete
tested backdrop colour footprint, including all206 previously excluded visible
sky pixels. This extends [the source/artwork evidence](r03-west-road-validation.md#subsequent-backdrop-and-auxiliary-prefix-validation-2026-10-08);
it does not accept the complete region, other views or GPU cost. R03 remains
incomplete and R04 unstarted. Production renderer/settings/source are unchanged.

## Model and actual results

The private source oracle retains original indexed positions, MVP, perspective
UV/tint and decoded sRGB trilinear artwork. It applies near/far homogeneous
clipping before division, separate FP32 viewport multiply/add, nearest1/256-pixel
quantization and top-left coverage for both windings. Actual80 draws per view
are unculled, with depth test/write off. The viewport pixel domain enforces
XY visibility without generating XY boundary vertices. This numerical model
was previously investigated for [clipped605](r03-clipped-static-validation.md#complete605-footprint-and-canonical-ties-2026-10-08);
driver guard-band internals/bounds remain unmeasured. No runtime clipping change
or universal device-arithmetic claim follows from this result.

- Normal:2073600 source-colour samples/345060 visible sky samples,0 errors.
- Wide:1788230 source-colour samples/450974 visible sky samples,0 errors;
  285370 pixels outside source coverage preserve pre-backdrop RGBA exactly.
- Total3861830 colour samples/796034 visible sky samples,0 edge exclusions.
  This adds799 colour checks and resolves99 normal/107 wide visible sky edges.
- Wide original single-texel cap:124027 colour checks/106281 visible after world
  occlusion,0 errors. All visible sky has source coverage in both views.
- Whole-frame backdrop IDs remain0/D32 remains1; compared RGBA alpha is255.
  RGB2-byte and LOD+/-0.35 gates are unchanged. Source/post-VS maximum position
  errors remain0.00003993/0.00002448. Separately recomputed post-VS coverage masks
  have identical SHA256 to source masks in each view; colour checks also pass.

Uniform artwork cannot establish an exact GPU primitive owner from colour alone.
These checks accept source-compatible backdrop pixels, preserved clear pixels
and depth/ID policy, not a universal exact primitive-ownership model.

## Executed validation and controls

`test-full-backdrop141.py` exits0 using the bundled NumPy/Pillow Python. It
recomputes full source and post-VS oracles plus the previous interior oracle.
Seven fresh copied attachment controls reject exactly one error each: colour,
ID and D32 at formerly excluded normal[1153,7]/wide[10,0], plus outside-source
wide[963,818] RGBA corruption. Controls reuse the immutable source-derived
colour/coverage arrays with the same attachment validator; no corrupted image
determines expected coverage. SHA256 of all404 original raw/mip files is unchanged.
The old interior receipts/masks are regenerated and retained alongside full ones.

`verify-full-backdrop-replay141.py` runs through the public direct analysis runner
on the RTX3060Ti/configured1.41 for both existing captures. Each replay verifies
six raw before/after attachments,28 materials/196 mip levels and all80 actual
cull/depth states against stored exports. Final SDK script phases1.425s/1.357s
exit0 without RPC timeouts; earlier runs1.797s/1.736s also pass. These are analysis
timings, not native GPU cost. Four initial HIGH messages are explicitly retained
per final receipt with `validationClean=false`; no clean-validation claim.

All private helpers/receipts stay under ignored `src_rebuild/build/native-renderer-r03/`:
`check-full-backdrop141.py`, `test-full-backdrop141.py`,
`verify-full-backdrop-replay141.py`, `full-backdrop-controls141.json`,
`full-backdrop-native-replay-{normal,wide}141.json`,
`full-backdrop-{normal,wide}-final-sdk141.json` and
`backdrop-westroad-{normal,wide}141/{full,post-full}-pixel-check.json`.
No build/game/new capture/GPU fixture was run in this milestone. Six preserved
settings hashes and root/fork diff checks pass; fork remains clean at ae83dd2.
Next: remaining static surfaces, complete region/silhouette/resource/cost gates.
Retained R06/R10 consumers remain pending migration; OpenGL tracking VK042.
