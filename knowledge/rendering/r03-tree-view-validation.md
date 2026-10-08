---
type: Evidence
title: R03 tree view and boundary validation
status: partial
created: 2026-10-07
tags: [okf, rendering, vulkan, validation]
---

# R03 tree view and boundary validation

This extends the [sampling correction evidence](r03-cutout-validation.md) on
unchanged fork426c9a3. Parent HEAD2e93f215 remains dirty with the active goal and
unrelated edits. No production source, shader, gameplay or asset changed here;
no build or native/PSX self-test rerun is claimed. R03 remains incomplete.

## Executed captures and comparisons

Configured RenderDoc1.41 MCP captures three new actual frame500 Vulkan runs on
Windows/RTX3060Ti (preceding driverVersionRaw2559967232),1920x1080, trilinear,
focal128, HD overrides off. Fixed mission50/gametype1/level0/car0/single player,
startpos6917,-218984, native region. Starting headings3822/750/1774 are quarter-
turn increments from2798; these are actual game follow-camera runs, not a claim
of mathematically exact frozen camera rotations. Captures use API validation;
no replay shader replacement or old-world recovery is used.

The unchanged original oracle passes all three. A separate tightened-scope
oracle also runs on those captures and the prior normal/HD and wide/original
captures. It compares every eligible opaque pixel's RGB, retaining the two-byte
channel gate and LOD envelope +/-0.35, depth margins1e-6/5e-7. It replaces the
old barycentric>0.01 exclusion with distance>1/128 pixel from triangle edges,
and reduces the alpha threshold exclusion from0.1 to0.005. The declared edge
band is two observed8-bit subpixel units, not a proven universal precision bound.
[Vulkan polygon rules](https://docs.vulkan.org/spec/latest/chapters/primsrast.html#primsrast-polygons)
support point-sample coverage and perspective interpolation; exact-edge ties,
uncertain LOD/threshold and depth ties are still excluded and unaccepted.

All five tightened comparisons exit0: normal503966, wide168333, heading3822
209655, heading750143417, heading1774124676 opaque colour pixels, zero errors.
Combined1150047 comparisons also pass expected IDs/depth. Hole/occlusion masks
preserve attachments with zero errors. The eligible one-pixel actual-ID neighbour
bands contain5535/4484/4516/12054/9340 comparisons (35929 total). Actual IDs locate
this reporting band only; expected sampling/depth and full-scope preservation
remain independent. Excluded pixels are never converted into passing evidence.

The five-view union has positive geometric footprint coverage for all44 logical
parts;39 have actual opaque colour evidence. Parts731,733,734,735,751 have none.
Occluded-only checks are useful preservation evidence, not visible silhouette
acceptance. The new heading views query6/26/31 parts; querying a bounding box or
submitting a draw is not sufficient to count visible native coverage. The audit
corrects the private original verifier's fixed three-tree metadata to actual
queried counts; numerical comparisons are unchanged.

Each new frame retains80 backdrop/876 world/10 auxiliary UI draws,44 two-sided
cutout policies, geometry148320 bytes and62 images/2174132 material bytes. All
source-buffer/artwork-mip hashes, bindings, formats and logical cutout policy
match the preceding wide frame. Event IDs differ by earlier command count; the
first audit incorrectly compared those local IDs and failed, then correctly
compared policy excluding only `eid`. Raw reports remain. Four initial HIGH
HOST_READ/ALL_COMMANDS barrier messages remain per new replay (`validationClean=false`);
prior captures retain five. Three new world PNGs are visually inspected; their
terrain/sidewalk seams are not accepted by the tree masks.

## Reproduction and remaining work

Private local artifacts below ignored `src_rebuild/build/native-renderer-r03/`:
`native-tree-turn90-wide141_frame500.rdc` and180/270 equivalents, inspector
`tree-turn*-wide141/`, exported `cutout-turn*wide141/`, original/boundary logs,
`make-cutout-boundary-oracle.py`, `check-cutout-boundary141.py`,
`audit-tree-view-coverage141.py` and `tree-view-coverage141.json`.
Run inspector/export scripts through configured `rdc script`, offline oracles
through bundled NumPy Python, then the audit for resource and per-part coverage.
Original local artwork/captures remain private. Five runtime INIs hash-restored
from `tree-views-backup`; owned targets terminated, replay daemon closed, IDE
remained Design and was not reconfigured.

This five-view milestone is superseded in coverage, but retained as evidence,
by the following exposure checks. Compatibility record for this milestone: VK025.

## Additional exposure checks (2026-10-08)

Read-only IDE inspection after region assembly identifies the previously missing
parts:731/733 are model163 part1 at12510,0,-217738 /15560,-17,-218753;
734/735 are model164 parts0/1 at-5165,0,-230249;751 is model164 part1 at
14817,0,-222677. World pick IDs map to copied source indices plus one here;
backdrop parts append afterwards. No debugger function evaluation or state edit
is used. Source receipts remain private with the captures.

Configured RenderDoc1.41 captures six more actual Vulkan frame500 runs with the
same mission/category/extent/filter and native region settings. Eastnorth and
eastsouth use startpos14000,-220800 with heading0/2048 respectively; west uses
-6500,-231500/heading512. Each runs focal128/original and focal256/HD. Both
unchanged original and tightened oracles exit0 for every capture. The additional
tightened colour/band counts are eastnorth-wide112093/7661, eastsouth-wide
427688/9160, west-wide27545/3876, eastnorth-normal430758/14107,
eastsouth-normal841547/10912, west-normal90023/5507. All hole/solid-ID/depth/
occlusion/colour error counters stay zero within the stated scope.

Across eleven views this is3079701 eligible colours and87152 neighbour-band
comparisons. All44 parts now have positive opaque colour evidence in wide/original
views;36 have it in normal/HD. HD parts712/713/716/717/742/743/750/751 remain
without colour evidence. Part751 has only35 eligible original-art pixels, from
west-wide; the closer east/south views are occluded, not accepted silhouettes.
These counts do not accept full silhouettes, excluded edges/thresholds or all
minification conditions. The west camera/terrain issue below also limits that
view's use to the sampled tree contract, not valid region/drive coverage.

Each new run retains80 backdrop/876 world/10 auxiliary draws,44 cutout policies
and geometry148320 bytes. Original uses62 images/2174132 material bytes; HD uses
61/49926792. The first raw resource audit fails because capture-local ResourceId
keys differ. Corrected comparison removes only those keys and compares multisets
of complete records, preserving duplicate multiplicity, all hashes/bytes/extents/
mip/view/LOD fields. Ordered draw content/bindings, formats, counts and logical
policy also match their respective preceding normal/wide references. The audit
passes all nine additional views; raw reports and the failed diagnosis remain.
Four HIGH initial replay barrier warnings remain on every new capture;
`validationClean=false`, with no warning suppression or clean-replay claim.

All six new world PNGs are visually inspected. Eastnorth views show grass/sidewalk
stippled overlap at another location, which the tree oracle does not test. West
views show a thin ground edge with backdrop below it: establish camera height,
source geometry and bounded-region coverage before deciding whether the spawn
is invalid or native terrain is incomplete. Do not label it harmless or accept
the frame as whole-region evidence. Eastsouth also retains terrain boundaries.

Subsequent RenderDoc1.41 pixel history narrows the eastnorth defect to native
grass569/EID3927 and sidewalk681/EID4599. Four sampled pixels retain their raw
histories/source/clip vertices/push constants. At1400,795 the sidewalk fails depth
with0.9910836219787598 against grass0.991083562374115 (one D32 unit); the adjacent
1401,795 ties and passes. Grass uses canonical horizontal depth; sidewalk has
`depthPlane.w=-1` and local vertex heights0/17 because its horizontal top and
vertical side share one material part. Current `PsyX_NativeScene.cpp` classifies
only meshes whose every vertex has the same height; `NativeAssetDecode.cpp`
groups by material and authored layer. This identifies a concrete mixed-surface
case for investigation, not yet proof of identical transformed source planes
or an accepted fix. Confirm copied source transforms before choosing a split
or extending per-face depth metadata. Receipt: `exposure-terrain-history141.json`;
private script `inspect-exposure-terrain141.py`, four histories,720ms, no replacement.

Private artifacts: `native-tree-{eastnorth,eastsouth,west}-{wide,normal}141_frame500.rdc`,
`tree-{eastnorth,eastsouth,west}-{wide,normal}141/` inspector directories,
`cutout-{eastnorth,eastsouth,west}{wide,normal}141/`, `tree-exposure-sources.json`,
`audit-tree-exposure141.py`, `tree-exposure-coverage141.json`,
`tree-exposure-resource-audit141.json` and per-view logs.
The combined first export exceeds the configured30s CLI response timeout; session
status confirms the live replay and completed files, and offline oracles pass.
It is not restarted on timeout. Subsequent helper calls separate inspection from
exports, each below30s. All owned targets/replays close. Five INIs and generated
`.user` are byte/hash-restored from `tree-exposure-backup`, reverified after IDE
reopen; all10 VS pairs, Release_dev/x64, Design and no breakpoints are confirmed.
No production source/build/self-test rerun is claimed. Compatibility: VK026.

Subsequent source confirmation and native fix for569/681 pass scoped normal/wide
top pixels; see [mixed-height evidence](r03-mixed-height-validation.md). The prior
whole-mesh classification described above is historical, replaced by bounded
consecutive indexed ranges. Tree regression passes in both new captures.

Next: diagnose west camera/ground coverage and vertical-side pixels,
then remaining HD parts, excluded threshold/geometry boundaries and minification,
remaining region surfaces, replay warnings and GPU cost. R03 stays incomplete;
R04 remains unstarted.
