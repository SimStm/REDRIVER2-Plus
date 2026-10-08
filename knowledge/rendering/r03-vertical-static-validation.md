---
type: Validation
title: R03 static vertical face and culling evidence
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# R03 static vertical face and culling evidence

Following [part605 horizontal clipping](r03-clipped-static-validation.md), inspect
its remaining range and an actually visible vertical face. Use the same existing
normal/HD and wide/original1.41 captures; no new game launch/build/capture or GPU
fixture is claimed. Private exports and independent source oracles supply all
results below. Production source, synchronization, settings and forkae83dd2 stay
unchanged. This accepts declared scopes only, not the whole west/region gate.

## Part605 side range

Event4195 selects the final12 indices/four triangles from the same24-vertex
buffer as event4193. Its captured depth plane is `[0,0,0,-1]`, selecting native
raster depth rather than the horizontal canonical plane. Concatenating both
ranges reproduces the entire index-buffer hash; vertices/MVP agree exactly.

Independent source clipping yields normal polygon corners4/3/0/0 and wide
3/3/3/4. The resulting three normal/five wide triangles all have back-facing
winding under captured `frontCCW=true`/back culling. No face survives. Therefore
zero compared opaque side pixels is expected for these two views, and does not
prove visible side shading elsewhere. Before/after colour/ID/D32 are exactly
unchanged throughout35,948 normal and115,125 wide crop pixels. A copied colour
mutation is rejected as exactly one outside-footprint error.

## Visible vertical part762

The existing native range inventory contains251 opaque raster-depth ranges.
Select part762/event5305 because its final native ID footprint is visible; this
selection locates a test subject only. Expected pixels come independently from
indexed source positions, UVs, tint and mip artwork, not from that footprint.

The range selects24 indices/eight source triangles. Positions form a vertical
local plane at `z=-640`, spanning `x=-1023..1023`, `y=512..2560`. Captured
depthPlane `[0,0,0,-1]` confirms raster-depth use. All eight triangles survive
six-plane clipping and front-face testing in both views. Source vertex/index
hashes and concatenated full seven-mip artwork hash exactly match each prior
capture inventory binding. All individual mip hashes also match their metadata.

The first oracle attempt stops at the copied part605-specific0.0001 absolute
source/post-VS threshold: actual maximum difference is0.000249863. Source
coordinates are larger, so that constant is not a general FP32 dot-product
bound. This was a rejected check, not an accepted renderer failure or a pixel
tolerance change. Verify each output component against its own arithmetic
rounding bound instead: `gamma7 * sum(abs(matrixTerm * positionTerm))`, where
`u=2^-24`, `gamma7=7*u/(1-7*u)`, covering four FP32 multiplies/three additions.
Maximum component budget is0.003281374; maximum error/budget ratio is0.127881.
A copied MVP translation perturbation of0.1 fails this source gate. The actual
colour, LOD, raster and D32 acceptance limits stay unchanged.

- Normal:140,667 source-colour/ID/D32 pixels and1,414 eligible neighbour-band
  pixels PASS. Maximum compared depth error1.185e-7.
- Wide:35,165 source-colour/ID/D32 pixels and740 eligible neighbour-band pixels
  PASS. Maximum compared depth error1.177e-7.
- Outside the projected geometry,23,080 normal and6,078 wide crop pixels preserve
  all three attachments exactly. No wrong colour/ID/depth/preservation samples.
- Normal53/wide25 raster-band pixels remain excluded. No depth-ambiguous pixels
  are present in the declared interior scope. Exclusions are not accepted.

The same strict limits are retained: RGB2 bytes, LOD +/-0.35, alpha0.005,
triangle distance>1/128 pixel, visibility1e-6 and D32 error5e-7. At642,256 in
separate copied exports, independent mutations of colour, ID and depth each
produce exactly one corresponding failure and oracle exit1. Matrix and culled-
side mutations also exit1. All original normal export file hashes remain intact.

## Executed workflow and pending work

`C:/Python314/python.exe scripts/run_renderdoc_analysis.py` uses each existing
`native-west-road-{normal,wide}141_frame500.rdc`, explicit `--gpu "RTX 3060 Ti"`,
`--arg label=normal`/`label=wide` and distinct summaries. Scripts
`export-side-605141.py` complete0.331s/0.353s; `export-vertical-762141.py`
complete0.202s/0.158s. These are analysis-phase timings, not total replay or
GPU frame timings. SDK version remains1.41.

Offline NumPy runtime
`C:/Users/simst/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`
runs `check-side-605141.py normal wide`, `check-vertical-762141.py normal wide`
and `test-vertical-pixels141.py` in the private build directory. Real oracles
exit0; the harness exits0 only after proving all five nested failures and
vertex/index/full-mip integrity. SDK exports refuse to overwrite existing roots.

Five helper syntax checks, twelve affected documents/185 local links, root/fork
diff hygiene and six settings hashes PASS. Status/handoff stay349 lines each;
the fork is clean and no game process remains. GitHub API confirms upstream8.0
baseline `b2d8857`/2026-07-02 before the changelog amendment.

Private artifacts stay under ignored `src_rebuild/build/native-renderer-r03/`:
the five helpers above, `side-source-605-{normal,wide}141/`,
`side-605-{normal,wide}141/`, `vertical-source-762-{normal,wide}141/`,
`vertical-762-{normal,wide}141/`, copied negative-control roots,
four SDK summaries and `vertical-source-controls141.json`.

Continue R03 with remaining visible vertical surfaces, the78 excluded facade
raster pixels and125 previous part605 raster/depth exclusions, remaining HD
tree silhouettes, region coverage and GPU cost. Four HIGH replay warnings and
failed auxiliary-baseline/combined fields are retained. R06 dynamic and R10
screen-space debt persists. R03 is incomplete; R04 unstarted. Compatibility: VK034.

## Additional ten-part coverage (2026-10-08)

The same existing normal/wide captures now test parts759/760/761/763/767/779/
781/793/797/815, selected from the visible raster-depth inventory. This adds
709948 source-colour/ID/D32 samples,12136 opaque occlusion checks,17035 eligible
neighbour-band samples and155912 outside-footprint crop preservation checks.
All declared comparisons PASS. Maximum eligible D32 error1.931e-7 and source/
post-VS error/budget ratio0.244921 remain within unchanged acceptance bounds.
504 raster-band pixels and65 depth/alpha-ambiguous pixels remain excluded.

The initial oracle fails52 ID/depth and49 colour comparisons on815, whose
source range mixes facade/roof faces. It incorrectly overwrites expected pixels
by triangle order without selecting the nearest surviving face inside the draw.
Independent minimum-depth selection reduces the result to one normal failure
and zero wide failures. At1594,356, the closer source triangle has edge distance
0.003060 pixel, inside the existing1/128-pixel uncertainty band, while a farther
face has distance0.287455. A foreground edge with uncertain raster coverage must
not silently accept the background face's shading. Track minimum uncertain-edge
depth and exclude overlapping foreground uncertainty after all triangles are
processed. This adds two normal815 exclusions, counted in the504 raster pixels;
it does not widen the band or the colour/depth limits. Those pixels remain open.
Initial and intermediate failed receipts are retained.

All tested mip alpha bytes are255, so the new within-draw nearest-depth oracle
is explicitly scoped to these opaque parts. A copied815 triangle-order reversal
produces exactly identical pixel result rows. A copied MVP perturbation fails
the FP32 source gate, and one copied D32 corruption produces exactly one wrong
solid pixel. Original export hashes remain unchanged. All20 normal/wide vertex,
full-index, selected-index-slice and concatenated full-mip bindings match prior
inventories.815's selected range has168 indices while its full buffer has174;
the remaining horizontal range is not accepted by this vertical check.

Executed local SDK scripts `export-vertical-set141.py` take1.545s/1.438s and
`export-vertical-set-indices141.py` take0.180s/0.174s, using the same runner,
explicit1.41/GPU and normal/wide arguments above. Offline
`check-vertical-set141.py normal wide` initially exits1, then passes after the
independent oracle corrections; `test-vertical-set141.py` exits0 after proving
the order/mutation/resource controls. These scripts/exports, index sidecars,
SDK summaries, `vertical-set-{initial-failure,nearest-failure,pixel-check,controls}141.json`
and copied controls stay in the ignored private build directory.

This extends visible surface evidence, with no production/build/game/settings
change or new GPU fixture. Other surfaces/region, every excluded pixel, replay
warnings and GPU cost remain pending. R03 incomplete/R04 unstarted. VK035.
Four helper syntax checks, ten affected documents/158 local links, root/fork diff
hygiene, status caps349/349 and six settings hashes PASS. Fork remains clean;
no game process remains. Upstream8.0/`b2d8857` baseline was reverified through
GitHub API before the changelog update.

## Complete part762 raster footprint and remaining edge failure (2026-10-08)

An independent fixed-grid coverage model rounds projected source vertices to
1/256 pixel and applies the negative-area framebuffer top-left edge rule.
The original source/MVP and verified post-VS positions independently predict
exactly the same footprint as actual native IDs:140707 normal/35185 wide pixels,
zero coverage differences. All eight triangles remain front-facing and unclipped.
This validates this model for these two facade views, not every GPU/geometry.

The full source-artwork oracle now compares all175892 facade colour/ID/D32
pixels and29176 outside-footprint crop pixels, with no raster or depth/alpha
exclusions and zero wrong samples.2172 eligible neighbour-band samples pass;
depth maxima remain1.185e-7/1.177e-7. RGB2-byte, LOD +/-0.35 and depth1e-6/5e-7
limits are unchanged. The former53 normal/25 wide raster exclusions are now
resolved:60 covered pixels receive source colour/ID/depth checks and18 uncovered
pixels receive exact preservation checks. This closes these78 pixels only.

A synthetic shared diagonal assigns each pixel to exactly one triangle under
the declared top-left rule. Separate copied normal exports corrupt a formerly
excluded covered pixel459,10 in colour/ID/depth and an uncovered pixel504,23 in
colour. Each is rejected with exactly one corresponding error. Reversing source
triangle/post-VS order yields identical pixel rows; original export hashes stay
unchanged. Host `check-facade-raster141.py`, `check-facade-pixels141.py normal wide`
and `test-facade-edges141.py` all complete successfully in the existing NumPy
runtime. No new game/build/capture/replay/GPU fixture was run this turn.

Applying the same exact-grid/within-draw nearest-depth model to the ten additional
parts gives710318 eligible colour/ID/depth comparisons and12152 occlusion checks,
but `check-vertical-full-set141.py normal wide` **exits1**: normal779 has one wrong
colour/ID/depth pixel at1125,397. Before/after native ID0, D32=1 and colour
`[252,248,244,255]` are unchanged, while the model predicts coverage. Source and
post-VS snapped vertex coordinates agree, so source-versus-post matrix rounding
alone does not explain this discrepancy. Other nineteen part/view combinations
have zero compared errors, but this does not accept the failing twenty-view set.
156029 outside-footprint crop pixels preserve attachments;66 depth-ambiguous
pixels remain excluded. The failing receipt and all per-part reports are retained.

Private helpers/receipts stay in ignored `src_rebuild/build/native-renderer-r03/`:
the four scripts above, `facade-raster-prediction141.json`,
`facade-edge-controls141.json`, `vertical-762-{normal,wide}141/full-raster-pixel-check.json`,
copied edge controls and `vertical-full-set-pixel-check141.json`. No production
source, resource, shader, synchronization, settings or OpenGL change. VK037.

Next: diagnose779's edge coverage using the actual viewport/raster arithmetic,
without silently dropping the pixel or widening a tolerance. Continue all other
R03 region/silhouette/depth exclusions and GPU cost; four stored tooling warnings
and R06/R10 debt remain retained. R03 incomplete/R04 unstarted.

## FP32 viewport model resolves779 coverage (2026-10-08)

The retained failing pixel1125,397 is triangle6's left edge. Projecting verified
post-VS coordinates in FP64 yields first vertex x=1125.8339639094906; FP32
perspective division/addition/viewport scaling yields1125.833984375. Their nearest
1/256-grid x coordinates are288213 and288214. The signed first-edge function at
the pixel centre changes from-7035 (covered) to11214 (outside). Other edges remain
negative. The native attachments stay sky, matching the FP32 viewport prediction.
This demonstrates an oracle arithmetic discrepancy in the tested frames, not a
production renderer fix or a universal hardware arithmetic prescription.

[Khronos viewport rules](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/vertexpostproc.adoc)
define the transform and limited framebuffer precision through subPixelPrecisionBits.
The specification alone does not prove this exact sequence of FP32 intermediate
operations. Context7 returned those rules; actual source/post-VS and attachment
comparisons establish the scoped device model. Source MVP interpolation, colour,
LOD/alpha and depth tolerances remain unchanged. Only raster-coordinate prediction
now uses FP32 viewport arithmetic before the existing fixed-grid/top-left test.

All22 part/view combinations (the previous ten parts plus762) pass886209 source-
colour/ID/D32 samples,12152 opaque occlusion checks,19317 neighbour-band checks and
185206 outside-footprint crop preservation checks. There are zero wrong samples
and zero raster-band exclusions.66 depth-ambiguous samples remain unaccepted.
Independent source and actual post-VS positions predict identical footprints:
898427 total covered pixels, zero differences across22 draws. The former failing
779 pixel is explicitly checked as outside-footprint preservation, not discarded.
The prior failed FP64 receipts are preserved separately.

Three copied779 exports corrupt this pixel's colour, ID or D32 independently;
each produces exactly one preservation error. Triangle-order reversal gives
identical result rows and original export hashes remain unchanged. These controls
verify the corrected expectation can still fail. No production source, shader,
resource, synchronization, settings, build, game launch, new capture/replay or GPU
fixture changed or ran for this offline milestone. The existing1.41 exports are
used; their prior resource-binding evidence is unchanged.

Executed NumPy-runtime commands under ignored `src_rebuild/build/native-renderer-r03/`:
`check-vertical-fp32-viewport141.py` (exit0), `check-viewport-source-post141.py`
(exit0), `test-viewport-edges141.py` (exit0). Receipts are
`vertical-fp32-viewport-pixel-check141.json`, `viewport-source-post-coverage141.json`,
`viewport-edge-controls141.json` and per-part `fp32-viewport-pixel-check.json`.
Old full-raster failure receipts remain intact. No RPC or execution timeout occurred.

Next: resolve remaining depth ambiguity and other clipped/source surfaces, then
full region/silhouette and measured resource/GPU cost. Four stored tool warnings,
R06/R10 consumers and full R03 gates remain open; R04 unstarted. VK038.

Hygiene: ten affected documents/170 local links, three helper syntax checks,
root/fork diff checks,349/349 status caps and six settings hashes PASS. Fork clean;
no game process remains. Upstream8.0 tag `b2d8857`/2026-07-02 release reverified
through tag/release API (8.0 is marked prerelease; `/latest` returns older7.4-rc2).

## Bounded depth decision and retained ownership ambiguity (2026-10-08)

Inventory the66 previously excluded pixels rather than accepting an aggregate
count. Seven part/view cases have conflicts:760 versus597;763 versus610/612;
815 versus575/577; wide793 versus761. The first three are raster facade versus
captured canonical horizontal ground;793/761 are two raster surfaces. Native
owners change37 times and preserve the prior owner29 times. This inventory does
not establish which surface should win. Forkae83dd2 remains unchanged: native
pipeline uses LESS_OR_EQUAL and raster-depth shader writes gl_FragCoord.z.

The previous1e-6 visibility band was larger than the already declared5e-7 D32
error gate. Use the existing error bound as the conservative source-depth
interval: sourceDepth+5e-7 < beforeDepth guarantees a nearer source candidate;
sourceDepth-5e-7 > beforeDepth guarantees occlusion. Values between remain
ownership-ambiguous. This narrows the uncertainty interval; RGB2 bytes, LOD+/-0.35,
alpha and5e-7 D32 error gates are unchanged.17 additional source-colour/ID/D32 and
15 exact occlusion comparisons PASS.22 part/views now pass886226 colour/ID/D32,
12167 occlusion and185206 outside-footprint samples, with0 raster exclusions.
Maximum compared D32 error1.931e-7.34 source ownership decisions remain unaccepted.

Check those34 separately for native attachment integrity:20 new-owner samples
must have source-artwork colour, source-compatible D32 within5e-7, and actual
postDepth <= beforeDepth.14 prior-owner samples must preserve colour/ID/D32
exactly. All PASS. These checks cannot establish the unique source winner inside
the declared arithmetic uncertainty; do not count them as34 more accepted source
samples or silently remove the ownership gate. A possible prior/new candidate
is not a deterministic reference answer.

An initial interval implementation applies the scalar tolerance directly to an
FP32 depth array. At this depth, NumPy rounds the nominal5e-7 to4.76837158203125e-7,
incorrectly classifying one additional prior-owner sample (33 ambiguities).
The final helper promotes the stored D32 array to FP64 before interval arithmetic:
34 ambiguities,17 new source and15 new occlusion checks. Preserve the initial
`bounded-depth-initial-fp32-threshold141.json`; avoid claiming its narrowed
33-pixel result as the exact declared bound.

Eight copied815 controls each fail exactly once: native/prior colour, unknown
ID and D32 corruption (depth changes only one ULP), one missed guaranteed-visible
sample and one changed guaranteed-occluded sample. Triangle reversal produces
identical rows; original export hashes remain unchanged. This proves attachment
checks and the bounded decision can fail, but does not prove unique ownership
for the34 uncertain samples. No production/build/game/settings/new capture/replay
or GPU fixture change/run; existing1.41 exports and prior bindings are retained.

Executed NumPy-runtime helpers under ignored `src_rebuild/build/native-renderer-r03/`:
`inspect-depth-ambiguity141.py`, `check-bounded-depth141.py`,
`test-bounded-depth141.py` exit0. Main receipts are
`depth-ambiguity-inventory141.json`, `bounded-depth-pixel-check141.json`,
`bounded-depth-controls141.json` and per-part `bounded-depth-pixel-check.json`.
No RPC/execution timeout occurred. Next: obtain a tighter independently verified
raster-depth reference for the34 ownership decisions; continue other source
surfaces/region/silhouette and GPU cost. Warnings and R06/R10 debt retained.
R03 incomplete/R04 unstarted. Mandatory OpenGL tracking VK039.

Hygiene: ten affected documents/173 local links, three helper syntax checks,
root/fork diff checks,349/349 status caps and six settings hashes PASS. Fork clean;
no game process remains. Upstream8.0/`b2d8857`/2026-07-02 release reverified
through GitHub tag/release API before the changelog update.

## Subsequent source-bounded GPU raster reference (2026-10-08)

[Explicit fragment trace evidence](r03-raster-depth-trace-validation.md) now
verifies all34 remaining decisions:20 native/14 preserved, three exact D32 ties.
Source-covered primitive/reflection mappings, effective zero bias and exact
shader/native D32 bits are checked; seven trace/comparison mutations reject.
The22-view oracle now passes886246 colour/ID/D32/12181 occlusion samples, with
no exclusions. This supplements source geometry/artwork with GPU raster-input
ownership proof; it does not claim an exact independent FP64-only winner model.
Other surfaces/region/cost remain pending; original receipts retained. VK040.
