---
type: Validation
title: R03 west road pixels and direct RenderDoc workflow
status: partial
created: 2026-10-08
tags: [okf, rendering, vulkan, validation, renderdoc]
---

# R03 west road pixels and direct RenderDoc workflow

The source-confirmed replacement west views pass the scoped road/sidewalk pixel
oracle. Subsequent source-clipped backdrop colour/visible-sky coverage also passes;
the angular blue cap is original artwork. **The west gate and R03 remain incomplete.**
Exact auxiliary producer attribution, negative-w geometry and other
region/silhouette/cost checks remain pending. No native production fix is made
by this milestone; PsyCross remains at pushed fork `ae83dd2`.

## Source-confirmed captures

The [earlier source investigation](r03-mixed-height-validation.md#direct-west-source-investigation-2026-10-08)
established model54/part309 Y0 road at start[-7000,-229300]. Both new configured
RenderDoc1.41 Vulkan captures use Chicago mission50, heading512, frame500,
1920x1080, native region enabled and the separate modern-mesh experiment disabled:
normal FOV256/HD1 and wide FOV128/HD0. Original game physics/follow camera run;
the starting position is not the captured camera position.

Using actual captured part309 MVP and its source-confirmed exact quarter-turn
basis, reconstruct the view under the production integer camera and fixed/4096
rotation constraints. Both views yield native camera[-7726,303,-230042]. Unsnapped
position[-7725.998843,302.999624,-230041.996022] is within0.01 of the integer result.
Cross-check ten source placements and their indexed post-VS outputs: maximum
matrix error0.003855 and reconstructed clip error0.004112, below0.005.
This is captured GPU reconstruction with known source constraints, **not a
same-frame CPU debugger observation** or the earlier ill-conditioned tree fit.

Both frames retain80 native backdrop/1023 physical world draws/876 logical parts,
44 cutout parts and148320 geometry bytes. Normal61 images/49926792 RGBA bytes;
wide62/2174132. Exact geometry/all-mip artwork record multisets match prior
mixed-height captures after excluding only local ResourceId fields. All876
logical parts retain identical source bindings and ordered indexed partitions.

## Independent road pixel oracle

Export ten first-index road/sidewalk ranges, indexed source positions/UV/colour,
all bound artwork mips, actual MVP/post-VS outputs and before/after native
colour/R32_UINT/D32 attachments. Nine positive-w draws are compared; part605
crosses the camera plane (minimum w=-38.84375) and is explicitly excluded.
This does not accept that part, every range of each mesh, or all vertical sides.

Keep the existing strict two-byte original-art RGB gate, alpha margin0.005,
triangle distance>1/128 pixel, LOD uncertainty+/-0.35 and depth1e-6/5e-7.
No tolerance is widened to obtain a pass. Actual results, both oracle exit0:

- Normal:810425 source-colour/ID/depth samples and1602 opaque occlusion samples.
- Wide:406960 source-colour/ID/depth samples and3014 opaque occlusion samples.
- Combined:1217385 source-colour/ID/depth,4616 occlusion and12764 boundary
  comparisons; zero scoped colour/ownership/depth/occlusion/hole errors.

All729600 lower pixels (y>=700) in each native world export have ID>0 and D32<1,
with zero ID0/D32=1 pixels. This is native coverage before auxiliary presentation,
not a legacy colour screenshot. It replaces the unsupported old spawn's empty
ground diagnosis with actual source-backed road evidence; it does not validate
every lower pixel against its source colour or accept backdrop coverage.

Both native PNGs were visually inspected. The wide view has a flat blue/angular
gap at the top sky. The subsequent independent source backdrop check below
identifies original single-texel cap geometry; it is not missing native sky.
Full building seams/silhouettes remain open.

## Auxiliary and diagnostic limits

Normal has14 auxiliary draws and wide11 against the existing inspector
expectation10. Preserve `regionCountsMatch=false`; do not silently change that
expectation. Pipeline/push/target exports show all auxiliary draws occur after
native world and use regular PSX overlay fragment constants. They write shared
colour with separate D24S8; none writes native IDs or native D32. End-capture
native IDs/depth are byte-identical to the last native world attachments.
Exact HUD/effect source consumer attribution and count reconciliation remain
pending at this initial milestone. Subsequent prefix evidence below explains
the count partition without accepting unknown game producers. Existing required
auxiliary consumers remain retained.

Each replay retains four initial HIGH barrier warnings and
`validationClean=false`; later event changes cannot erase them. Scoped pixel
success does not establish clean replay. The stale `render_coverage.log` ends at
a different run/frame and is excluded from this capture's category evidence.

## Direct build/capture/analysis validation

The user authorized direct tooling on2026-10-08. Installed rdc-cli0.6.3 has a
30s daemon socket limit for `rdc script`, independently of the MCP wrapper.
`scripts/run_renderdoc_analysis.py` loads its configured RenderDoc1.41 module
directly, requires a unique explicit GPU, checks replay support/status, runs
trusted local analysis scripts with fresh argument dictionaries and releases
controller/capture/replay in ownership order, including on exceptions. It
prints phase progress and writes results/timings; these are analysis receipts,
not automatic stage acceptance. See [repeatable workflow](renderdoc-direct-workflow.md).

Executed direct MSBuild Release_dev/x64 rebuild:48.48s,128 warnings,0 errors,
exit0. Direct hidden `-vknativetest` launch writes fresh native self-test PASS
including native loading/error/recovery, mixed-height pixels and retirement.
At that run the handler discarded the fixture return code; log evidence was
required. The [subsequent exit fix](r03-fixture-exit-validation.md) propagates it.

Direct replay of the first normal capture runs inspector15.933s plus road
export14.968s, total32.022s, exit0; public report exactly matches the MCP report.
A fresh direct `rdc capture` of the rebuilt executable succeeds with frame500.
Relative `-o` unexpectedly resolves below the target working directory; the
private generated capture is moved to the intended ignored build directory.
Future captures use absolute output paths. No duplicate capture is launched.

Direct replay of that fresh capture: inspector16.238s plus export15.692s,
total33.060s, exit0. Its entire public report, native attachment hashes, ranges,
geometry and materials exactly equal the MCP normal capture. Fresh independent
road oracle repeats810425 colour/ID/depth,1602 occlusion and7514 boundary
comparisons with zero scoped errors, exit0. A nonexistent GPU is rejected
explicitly (exit1, available GPU names reported, no success summary). A deliberate
analysis exception also exits1 without a success summary; subsequent replay
opens successfully, reads real textures and proves per-script argument isolation.
Python syntax checks pass. No daemon/RPC response limit is involved in this route;
monitor the same process across command observation yields instead of restarting.
This verifies reliability of the measured route, not a universal timing guarantee.

All six temporarily changed settings/generated XML files are restored and
SHA256-verified from `west-road-backup/manifest.json`. No owned game process
remains. The previous all10 VS configuration pairs/Design/game startup/zero
breakpoints state is preserved; the direct route does not edit IDE state.

Root/fork `git diff --check` and Python syntax checks pass. Thirteen affected
documents pass whitespace/182 local-link checks; current status/recent context
remain349 lines each, below the350-line cap. Fork working tree is clean.

## Subsequent backdrop and auxiliary-prefix validation (2026-10-08)

Replay the same source-road normal/wide captures directly through configured1.41;
export all80 native backdrop draws,160 indexed source triangles, positions/UV/
colour, actual MVP/post-VS, all material mips and pre-world backdrop attachments.
Source `sky.c:LoadSky` initializes single UVs for the first sky row;
`HorizonTextures` selects them and `PlotSkyPoly` applies the original corner map.
`NativeBackdropDecode` preserves that map and source Y-up geometry. Captured
single-UV materials are1x1 RGBA[140,165,206,255]. This is the blue cap's source,
not the dark native clear colour or an auxiliary image.

The independent oracle multiplies bound source positions by the captured MVP,
clips against all six Vulkan homogeneous planes and evaluates indexed geometry,
perspective UV/colour and decoded sRGB trilinear artwork in native order. Maximum
source-matrix/post-VS error is0.00003993 normal/0.00002448 wide. Keep strict RGB2
bytes, LOD+/-0.35 and the1/128-pixel triangle edge exclusion; no tolerance change.
Both oracle runs exit0:

- Normal:2073246 backdrop colour samples,344961 visible-sky samples, zero errors.
- Wide:1787785 backdrop colour samples,450867 visible-sky samples, zero errors.
- Total3861031 colour/795828 visible-sky samples. All345060 normal/450974 wide
  visible sky pixels have projected source coverage; none lacks source geometry.
- Wide123992 uniform-cap samples,106253 visible after world occlusion, all pass.
  Normal cap is outside the compared view. Both backdrop ID arrays remain0 and
  D32 arrays remain1 across the whole frame; the world subsequently owns depth.

The source-mask PNG was inspected against the native wide PNG: the cap boundary
matches the source dome. Excluded raster borders remain unaccepted, including
206 visible-sky pixels excluded across both views. This accepts the scoped cap/
interior/visible source coverage, not every silhouette/region boundary.
An initial helper omitted `__file__`; its first actual raster run wrongly
transposed RenderDoc's row-exposed MVP and failed the matrix/colour gates.
Correct row interpretation matches captured post-VS without a production change;
`failed-oracle-matrix-layout.json` retains the rejected result. Negative control
copies exports into a separate ignored directory, increments one visible cap
pixel's blue byte by5 at[1100,30], and is rejected with exactly one colour/sky/cap
failure. Original capture/exports are untouched.

Auxiliary source-VB/post-VS exports partition normal14 draws into4 PGXP-prefix
plus10 screen-space draws; wide11 into1 plus10. Both prefixes contain51 vertices/
17 triangles; every triangle clips away, with maximum left-plane distance
-2.049032 normal/-0.670045 wide. Actual before/after prefix colour is byte-identical.
Both remaining ten draws have342 vertices and `scr_h=0`; all prefix vertices use
the PGXP branch. Exact game producer and batching cause still need attribution.
Do not label all14/11 as HUD or silently accept the existing aggregate count gate.
Earlier whole-auxiliary native ID/D32 preservation remains valid.

Read-only inverse PGXP inference uses the source GTE128 scale,320x240 source
normalization and captured native view. Both views reconstruct identical51
points, bounds x[-8302.704,-7878.456], native y[-2.092,189.673],
z[-229781.534,-229502.144]. Their nearest copied static vertices differ by
137.578..406.601 units, so no static-source match is established. The method
assumes default PGXP Z scale/offset and original billboard placement transforms;
it is not exact producer attribution. Probe the dynamic car/shadow/wheel producer
against these points next rather than guessing from auxiliary draw counts.

This milestone changes no production source, shader or resource contract. No
new build, direct fixture or game launch is claimed; prior direct results above
remain separate. SDK replay closes in ownership order and no settings are changed.
Root/fork diff hygiene and private helper syntax pass. Twelve affected documents
pass181 local-link/whitespace checks; both status files remain349 lines. The
owned game target is absent and the fork working tree remains clean.
Private added helpers/receipts: `export-west-backdrop141.py`,
`check-west-backdrop141.py`, `test-west-backdrop-oracle141.py`,
`backdrop-westroad-{normal,wide}141/{draws,source-pixel-check}.json`, source-mask PNGs,
`backdrop-negative-control141.json`, `export-west-aux-vertices141.py`,
`check-west-aux-prefix141.py`, `west-road-{normal,wide}-aux-vertices141.json` and
`west-road-aux-prefix-check141.json`; read-only `infer-west-aux-source141.py` and
`west-road-aux-source-inference141.json`. All stay in the ignored private build directory.

## Private receipts and next check

Ignored `src_rebuild/build/native-renderer-r03/` contains captures
`native-west-road-{normal,wide}141_frame500.rdc` and
`native-west-road-direct141_frame500.rdc`; reports `tree-westroad-{normal,wide}141/`,
`direct-west-road-fresh141/`, `road-{normal,wide,directnormal}141/`;
`west-road-camera141.json`, `west-road-resources141.json`,
`west-road-{normal,wide}-aux141.json`, direct replay/pixel/equivalence summaries,
`direct-msbuild.log`, `direct-native-gpu.log` and preserved application logs.
Private helpers include `export-west-road141.py`, `export-cutout-pixels141.py`,
`check-west-road141.py`, `check-west-road-camera141.py`,
`check-west-road-resources141.py` and `export-west-aux141.py`.
Captures/exports contain local game assets and must not be redistributed.

The [subsequent producer join](r03-auxiliary-producer-validation.md) identifies
12 wheel3/39 body vertices of Car#11 in both views and proves HD/VRAM batching.
The [count contract](r03-count-contract-validation.md) now separates matching static
inventory from the unchanged auxiliary baseline/failed combined field.
[Part605 clipped horizontal interiors](r03-clipped-static-validation.md) now pass
212270 source-colour/ID/depth and117029 outside-footprint crop checks; vertical
ranges and125 excluded raster/depth pixels remain pending. Next: remaining geometry,
remaining HD trees/silhouettes/seams, full region coverage and GPU cost. Chicago
water is not a R03 blocker under adopted scope; actual Rio water stays pending.
No whole-region gate or later stage is accepted. OpenGL backlog entry VK028.

## Complete tested backdrop follow-up (2026-10-08)

[Full source/post-VS backdrop validation](r03-full-backdrop-validation.md) now
resolves all206 visible sky exclusions:3861830 colour/796034 visible sky samples,
285370 outside-source preservation checks,0 errors/edge exclusions. Seven copied
corruptions reject,404 original raw hashes stay intact and direct1.41 replays
match all attachments/artwork/cull-depth states. Other west static surfaces,
complete region/silhouette/cost and retained R06/R10 consumers remain pending.
