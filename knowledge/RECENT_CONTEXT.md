---
type: Status
title: Recent engineering context
description: R00-R02 Windows slices accepted; R03 inventory/material/backdrop implemented, region acceptance pending.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-08. Preserve the paused renderer on `feat/vulkan-native-rework`; start engine migration independently from unchanged `master` through local `main`. [Checkpoint](rendering/r03-pause-checkpoint.md), [snapshot](changes/2026-10-08/vulkan-rework-branch-snapshot/index.md).
> Confirm this handoff against source, Git state and actual validation output.

## Latest user scope and delivery

The full native plan follows the 2026-10-08 adopted order R00-R07 -> R10 -> R11 -> R08 -> R09 -> R12, retaining IDs.
Acceptance still gates every advance; Chicago water is not a R03 blocker, applicable transparency stays required and Rio water remains pending. Documentation order/dependency/link/hygiene/line-cap checks PASS; no new runtime validation or stage acceptance.
The goal is cancelled; renderer execution is paused until the user resumes it.
Earlier documentation reset archived graphics history without deleting runtime code.

Created [the active rendering catalog](rendering/index.md), source-backed baseline,
requirements/disposition review, architecture, original-file/mod/GLB compatibility,
glossary/decisions, validation gates and thirteen stages R00-R12.
The [umbrella roadmap](roadmap/planned/native-vulkan-renderer.md) owns lifecycle.
**R00-R02 accepted on their declared Windows slices; R03 in progress, R04-R12 pending.**

### R03 current milestone

Fixed Chicago source cells contain144 resident cells/628 references/98 duplicates,
530 distinct copied objects,508 opaque/22 cutout/16 animated base objects;
no missing/invalid/rejected source, water or unknown blend policy. High-detail
world has4814 triangles/876 part instances; gameplay usage bitset stays intact.
Shared exact-content materials and immutable PNG owners preserve rollback/reset
and eliminate large PNG copies/hashes. Original models0..3 each30 vertices/
20 family21 quads now supply80 native unlit sky/painted-horizon draws. Copy raw
geometry/art bindings before GTE; camera rotation/offset/brightness are retained.
C ABI v4 layers draw backdrop first with depth test/write off; world retains D32.
Actual VS tick740: effective1/reasonOK/worldLegacy0/native956/backdrop80,
148320 owned geometry bytes/37446416 material bytes (not peak GPU allocation).
RenderDoc1.46 frame500 confirms80 depth-off/876 depth-on/10 UI draws; no messages.
Backdrop D32 probes stay1.0; sky stays1.0 after world, world probes become<1.0.
Asset1503/scene20/camera12/material13 and VS Release_dev x64 PASS. Native GPU
readbacks prove order/no depth write/translation/rotation/UI/retirement;
fresh PSX regression PASS, Khronos loaded/no VUID output. First city test found
missing sky material revision and safely fell back; corrected before capture.
Separate backdrop CPU timing found recent120 mean8.676/p959.135 ms; duplicate
frame artwork now shares immutable decoded bytes, mean4.000/p954.185 ms at
subsequent tick480. These are CPU windows, not GPU/frame times.
Before/after1920x1080 sky-only replay pixels are exactly equal; depth probes,
draw counts and zero messages match after immutable art sharing. Picking,
outside accounting and controlled turns/FOV/mod/drive acceptance remain pending.
Backdrop fork83ccdbc is committed/pushed to origin/master; parent gitlink staged.
Five INIs and `.user` restored byte-exactly; debugger/owned game processes stopped.
Private logs/captures/helpers/backups live at ignored `build/native-renderer-r03/`.
Generated membership includes new backdrop files, all10 configuration pairs
preserved. Wait for IDE build Done before closing: an early close produced a
modal warning; dismissed/recovered after the successful build.

ABI v5 mip and v6 retained per-instance sampling milestones are detailed in
[R03 evidence](rendering/r03-validation.md). CPU asset1505/scene20/camera12/
material13/mip13/sampling8, host guards, VS and fresh Khronos native/PSX GPU
suites PASS/no VUID. Actual minification188, cutout holes and real-fence retention
pass. RenderDoc1.46 nearest/trilinear frame500 verifies61 full chains/views and
80 backdrop/876 world/10 UI draws/no messages; actual eight toggles keep all960
intervening frames native with no extra uploads/budget growth. Initial live filter
replacement failure was fixed by retained descriptors; no current old recovery.
Forkd3e5a409 and subsequentc051051 committed/pushed, gitlink staged at milestones;
five INIs/.user restored, all10 configurations preserved. No timing/parity claim.

### R02 delivered original-import slice

R02's complete declared Windows slice is accepted; details and failed-attempt
context remain in [R02 evidence](rendering/r02-validation.md). Owned bounded
MODEL/placement/palette/PNG decoders preserve source spans, original UV overscan,
LOD/catalog identity, existing mod precedence and rollback/reset ownership.
Native indexed geometry and sRGB materials remain separate from legacy batches.
GPU cache shares exact-content materials and verifies scene/catalog generations;
submitted-upload failures retain GPU objects until the actual fence completes.

CPU baseline1,202 checks passed; later shared-art changes expanded those suites.
Actual Chicago fixture ultimately used logical road36/source37 and facade273,
not the earlier incorrect traffic-light/lamp candidates. Seven real placements
submit14 native draws, four mesh/material parts, geometry7176/artwork4247296
bytes. Read-only source probing rejects unsupported model1176 explicitly;
`-nativeassets` observes sources without drawing them. R02's small fixture is
not R03 region coverage. Original GltfLoader still supports only the first
primitive; whole-scene/production import is deferred to R09.

Direct native GPU readbacks passed linear-light188, black/white, cutout, legacy
filter independence, real-fence failure/recovery and zero retired ownership.
Fresh PSX GPU regression passed; Khronos loaded/no VUID output. RenderDoc1.46
matched all four vertex/index/artwork byte hashes,14 post-VS transforms at1e-5
and source-placement/final-camera MVPs at predeclared1e-4 text precision.
Actual mod on/off, reload, catalog generation/reset and interrupted-create
recovery returned to the four-part budgets without orphan handles.
Fork9ff48af was pushed; current fork/gitlink advances through later R03 milestones.
Generated project membership preserves all10 configurations. Five runtime INIs
and `.user` were restored byte-exactly after each accepted test run.

Fifty former dedicated renderer discussions/plans/product/change records,
hybrid bridge rules, diagrams and validation files were moved into
`knowledge/outdated-ideas/2026-10-06-rendering-reset/`, preserving original hashes.
Manifest records old/new paths, byte counts and final hashes after link repair.
Archive content is excluded from routine analysis unless the user explicitly asks;
metadata integrity checks are permitted. Shared current asset/mod/tooling contracts,
playground and the mandatory unscheduled OpenGL backlog remain active.

Updated AGENTS, archive policy/Cursor wrapper, catalogs, current status and changelog.
Only documentation changed in that earlier reset; R00 changes are below.
Executed documentation validation and its results are recorded in
[the change record](changes/2026-10-06/rendering-documentation-reset/index.md).

## R00 delivered surface and actual evidence

- Product: [native-renderer-baseline](product/native-renderer-baseline.md).
  Inventory: [r00-source-inventory](rendering/r00-source-inventory.md).
  Adopted ownership/POD handles/snapshot/camera/depth/material decisions:
  [r00-native-contracts](rendering/r00-native-contracts.md).
  [Measured baseline](rendering/r00-measured-baseline.md) contains hardware,
  numerical future budgets, effective settings, mode/mutation audit and recipes.
- New `DeveloperRenderCoverage.*` and source producer hooks cover 17 categories;
  F11 table and optional `PSYX_COVERAGE_LOG=1` distinguish requested/nonempty
  legacy/earlier experimental/native batches. Counts overlap and are not GPU
  totals. Native stays zero. No new persisted setting or game mutation.
- `query_vulkan_baseline.cpp/.ps1`: Windows read-only instance query, physical
  feature/format-usage/sample intersections and memory budgets. RTX3060Ti,
  driver 610.88; physical API 1.4.0, loader1.4.357; queried colour/depth samples
  1/2/4/8. Current game enables neither optional sample shading nor anisotropy.
- VS MCP Release_dev x64 build passed (failed projects 0), fresh `-vkpsxtest`
  passed. `run_render_coverage_tests.ps1` passed 12 cases, including `/TC` C
  consumer linkage/struct return. Unit tool now matches project CRT policy at /W4.
- Installed RenderDoc 1.46 capture/first replay: original Chicago773 draws,
  widened focal 128 case 1150, original ATTRACT.400 route560, playground51;
  zero messages each. City before/after R00 has identical RGB pixels and pass/
  index counts. This is sampled diagnostic parity, not universal image correctness.
- Standalone `-vkfixture -vkcapture 120 -vknogui`:12 draws, saved 1280x720 BMP.
  Actual F11 table inspected with 17 rows; input capture flags 3 while visible.
  Existing saved panel position 1535,35 was outside a 1280-wide window. Temporary
  visible position allowed testing, then original bytes were restored. Keyboard
  close attempts were inconclusive; stop owned debugging instead of claiming success.
- Uninjected city:53 one-second windows after 4 loading windows, mean frame
  33.4319 ms, submit 6.9562 ms (window means6.24..8.07). These are window averages,
  not per-frame percentiles/GPU times. Process private810,610,688 bytes and
  peak working set378,814,464; renderer GPU/decoded peaks still unknown.
- Runtime mod order is one enabled valid `remaster-textures`,35 entries,
  33 registered overrides. Bilinear1 plus active overrides disables the earlier
  source-static checkbox; its submissions are not new native rendering.
- Five INIs and original `.vcxproj.user` restored byte-exactly after stopping
  owned sessions; hashes in ignored `build/native-renderer-r00/ini-restored.json`.
  Source/owned fixtures/game assets unchanged except diagnostics. Fork clean.
  `git diff --check` passed with existing LF/CRLF normalization warnings.

## R01 delivered surface and actual evidence

- [Product](product/native-renderer-bootstrap.md) and
  [evidence](rendering/r01-validation.md) record the accepted Windows analytic
  slice. It is not original-world coverage; source category native counts stay0.
- PsyCross C POD API copies bounded mesh/snapshot data, rejects stale handles,
  uploads after its existing fence and defers GPU retirement. Native indexed
  world shares g_vk's sole presenter and uses independent D32 conventional depth;
  original VRAM work and HUD/minimap/ImGui compose independently. No PGXP gating.
- Game adapter uses final original camera with explicit Y/basis/winding conversion,
  fixed near10/far125000 and asymmetric analytic geometry; session-only checkbox,
  rebuild button and `-nativeworld`. Unsupported/missing snapshots restore the
  complete legacy world. Split view is outside this stage's analytic slice.
- VS Release_dev x64 build PASS;26 owned CPU/C ABI/camera cases PASS. Native GPU
  readback checks depth/winding/clipping/camera/FOV/UI, resize/minimize/restore,
  submit/destroy retirement and8 resets PASS; fresh legacy PSX self-test PASS.
  Tests exposed/fixed previous-extent depth allocation and test-scissor sizing.
  Direct VS Khronos validation after fixes has no VUID/Validation Error messages.
- RenderDoc1.46 city:1 native18-index D32 draw,10 original UI draws/318 indices,
  zero messages. Matched PGXP texture/depth on/off PNG hashes are identical;
  widened focal128 remains clean. Actual ImGui mouse toggle restores the whole
  city; rebuild leaves1 ready/0 pending/0 retiring/0 rejected. Automated F11
  delivery was unreliable; widget inspection used debugger visibility instead.
- Instrumented resize capture5 records one legacy PRESENT/GENERAL layout VUID;
  other fixture/city frames and direct validation are clean. RenderDoc source
  uses GENERAL for fake presentation images; precise layer interaction remains
  unidentified. Document this limitation; do not claim every capture is clean.
- OpenGL3.3 city/HUD/ImGui runs with explicit native-unsupported fallback;
  no native GL parity or Linux/macOS runtime validation. Host excluded-platform
  branches compile, which is not a full cross-platform build. GPU timing/peaks open.
- Five INIs and original `.vcxproj.user` restored byte-exactly; owned game sessions
  stopped, VS solution reopened without added breakpoints. Preserve those bytes.
  Fork `99fa6418e03c95f4db71e07c2f09adf554669f51` committed/pushed to origin/master,
  clean; parent gitlink staged. Parent R00/R01 code/docs remain uncommitted beside
  substantial pre-existing changes. No original game data changed.
- New extent rule: [vulkan-resize-extents](rules/vulkan-resize-extents.md).
  All R01 Vulkan changes tracked in unscheduled VK-008; R02 acceptance follows above.

## Build and tool recovery notes

Premake regenerated new utils membership. The first backup attempt used paths
relative to the wrong working directory; generated hand-added configurations
needed recovery from existing `.bak-preinput` originals. Recovered both extra
variants for Win32/x64 while retaining new source membership. XML cloned-node
namespace declarations initially caused2 MSB4066 failures; corrected and rebuilt
successfully. Generated files remain ignored; do not rerun the ignored recovery
helper unchanged. Preservation rule now records path/namespace checks.

VS caches `.vcxproj.user` arguments/environment. Close/reopen after disk edits,
verify launched command line and fresh logs. An initial self-test reading was
stale; it was discarded, fresh requested runs passed after reopening. `vk_fixture.log`
is the self-test log, not REDRIVER2.log. RenderDoc MCP1.41 injection/replay failed;
installed1.46 Python API captured/replayed successfully. Do not compare injected
timing with uninjected performance. A late replay/UI run briefly overlapped two
owned processes; those shared logs were not used as performance evidence.

## Working-tree baseline and implementation limits

Parent HEAD `2e93f215c54e61dd1bbb5d26e439c25303466709` has substantial existing
dirty renderer/documentation work. PsyCross is clean at
`9ff48afee0384f92157568f6a2a3400b06b55fbd`, pushed to fork origin/master; parent
gitlink is staged. Do not reset, overwrite assets or stage unrelated work.

The [active baseline](rendering/current-state.md) preserves implemented facts:
Vulkan still consumes PSX primitives; modern/static adapters and resident near/far
shadows are partial. Native static migration requires nearest/no registered overrides
and PGXP, so saved bilinear=1 can leave the checkbox ineffective. Composite shader
classification may shadow reconstructed sky; exact facade/FOV causes remain open.
Screenshot 2 has one shadow volume/far off/zero capacity failures. Painted skyline
is not necessarily world geometry. These issues motivate the rewrite, not a claim
that native geometry/lighting stages are delivered. R00 diagnostics establish a
baseline, not replacement geometry.

Previous milestone: VS MCP Release_dev x64 and fresh `-vkpsxtest` passed. Fresh
installed RenderDoc 1.46 capture/first replay had zero messages; MCP 1.41 retained
ten instrumentation warnings. At 18k two captured cases reduced collection by
about 60-70% with identical colour/raw depth maps; the 40k low-sun route fit both
volumes at 48,477 triangles. Isolated actual-function MSVC tests used mocked capacity.
Real tier recovery/dynamic/city/streaming/global-shadow acceptance remains open.
GPU frame latency and peak memory are not established by those captures.
Four test INIs were restored byte-exactly; owned sessions closed. Ignored evidence
lives under `src_rebuild/build/shadow-demand-validation/`.

## MCP environment facts

GPUInfo is a tutorial hardware-report link provider, not a Vulkan spec registry or
a fetched hardware-limit source. R00 uses executed local Vulkan queries instead.
Context7 queried the official Vulkan Guide; official
Khronos glTF/depth docs and upstream releases were also checked. Upstream baseline
remains 8.0 at b2d8857 (July 2, 2026).

Earlier external MCP fixes remain: GPUInfo uses MCP v1 FastMCP, encoded query URLs,
explicit stdio and clear link-only output; rdc-tools child stdin is DEVNULL to keep
JSON-RPC input separate, with preserved stderr hints. Backups exist beside both
external scripts; Python AST/local client tests passed then. No external MCP code
or configuration changed in this documentation reset.

Duplicate RenderDoc 1.41/1.46 capture layers remain registered. Record capture/replay
versions and inspect warnings; do not silently equate instrumented timing with native
GPU cost. Check configured tools at the next implementation start.

## Useful reproduction and preservation notes

- Use Visual Studio MCP for the already generated solution, Release_dev x64.
  Never block a terminal on the interactive game process.
- F11 via desktop MCP can open the panel; scripted gameplay keys were unreliable.
  Debugger snapshot/breakpoint-driven captures are the reliable existing approach.
- `-vkpsxtest` reports PASS/FAIL in `vk_fixture.log`, not REDRIVER2.log.
- Inspector export tests require fresh directories and are not idempotent.
- INI settings, camera, sun and mod order must be aligned for image comparisons.
- Existing shared asset catalog/mod/palette/export/runtime settings/playground
  product documents remain active and their contracts must survive native rendering.
- Preserve NTSC/PAL native cadence and draw-time update semantics. Higher-rate
  interpolation belongs to R11; do not simply remove the frame gate.

## Next concrete action

Capture R03 west road[-7000,-229300]: normal/wide camera/ground/backdrop pixels/resources,
then side pixels/HD/silhouettes/region/drive/replay-warning/GPU-cost gates. The scoped
mixed-height569/681 top fix passes actual source/pixel/resource checks. Use1.41
rdc tools; no old-world recovery. R04 remains unstarted; retain fence/picking/
backdrop contracts and the OpenGL backlog. [Evidence](rendering/r03-mixed-height-validation.md).

## Latest R03 milestone and user steering (2026-10-07)

The user explicitly adopted 100% native Vulkan graphics replacement in this
separate version: no automatic old-scene recovery, no new legacy features/recovery,
progressive dependency removal after consumer migration. OpenGL stays separate.
The plan/gates reflect this decision. R03 is first incomplete; R04-R12 unstarted.

ABI8 picking/frame states remain in ABI9: native unavailable clears/suppresses
old world/composites, explicit loading versus failure. HUD/frontend/VRAM remain
unmigrated. CPU picking19/frame-state13 and GPU rollback/native recovery PASS;
city error world0/UI10, recovery native956/uploads61, geometry148320/RGBA49926792.
Prior city picking resolves building/tree/miss; actual loading/error world0/UI10
and native recovery/fences pass. Detailed [evidence](rendering/r03-validation.md)
retains the initial ABI8/RenderDoc1.46 tests and original preservation receipts.
Independent OpenGL city works; native-on-OpenGL fails explicitly at startup.
Source accounting reconciles530 objects/876 parts, original view228 inside/78
outside/zero uncopied/duplicate/invalid;18 checks/asset1523/scene/VS PASS. Initial
`native-coverage146_frame500.rdc`:80 backdrop/876 world/10 UI/no messages, colour/
IDs identical to prior pick frame; geometry148320/RGBA49926792/uploads61 unchanged.
`region-coverage-backup` restored INIs/`.user` exactly/all10 VS pairs; owned targets
stopped. Accounting accepted; remaining R03 gates pending, R04 unstarted.

Controlled normal/wide camera/filter/mod GPU BMP checks PASS:14 comparisons,
exact restored world pixels, native956/world0/no failures/loading over740 frames,
fixed geometry148320 and restored61 images/no pending uploads. A133-step mapped-
pad normal-physics fixture moves car/follow camera without assigning transforms;
desktop keyboard driving is not claimed. Camera/settings gate accepted. Private
camera/drive BMP/JSON/debug logs retain evidence; camera-mod-backup restored INIs/
.user exactly, all10 VS pairs preserved, debugger stopped/test breakpoints removed.

Terrain milestones: fork6ba8068 adds bounded tile paint layers/ABI v9;09d74de
adds double CPU view/model-first composition. Both pushed/gitlinks staged; their
source/GPU/retirement/road paint audits PASS. Precise grass planes then exposed
equal-height overlap z-fighting. Latest canonical horizontal fragment depth
uses one double frame inverse/world height, with identical equations for matching
planes and explicit last-submitted native tie ownership. Authored paint retains
four D32 units/layer; other/pitched/edge-on geometry keeps native raster depth.
Singular/nonfinite transforms fail explicitly. Alpha/physical nearer occlusion/
IDs/retirement stay native; early/late depth dependencies and112-byte aligned
push record preserve synchronization/layout. Public ABI, geometry/art and GPU
payloads unchanged. CPU15 plane checks,12 coordinates/scene22/other host suites,
asset1536/VS build PASS. GPU11685 pixels/order, paint/hole/nearer floor/IDs/zero-
owned retirement PASS; initial downward-facing fixture failed then was corrected.
Actual RenderDoc1.46 normal/wide clipped/culling-aware visible overlaps39558/71120:
wrong IDs17677/34474 ->0; excluded occluded/boundary23926/5357 are not accepted.
Paint45191/35601 also0 (new clipping masks differ from earlier45000/35470).
All bound buffers/mips exact;80 backdrop/876 world/10 UI/no messages, geometry
148320, HD61 images/RGBA49926792 versus original62/2174132. Private canonical-*
reports/captures under build/native-renderer-r03; [terrain evidence](rendering/r03-terrain-validation.md).
Source artwork/UV boundaries and full region/silhouette/water/drive gates remain pending.
Canonical-plane native/PSX GPU PASS/exit0/no VUID; fork2341108 pushed/gitlink staged;
canonical-plane-backup hash-restored five INIs/`.user`, all10 VS pairs/Design.

Native FP32 decoded-texel bilinear/trilinear corrects reproduced strict errors;
nearest/base semantics, implicit LOD, private push112/offsets/public ABI9 preserved.
15 fractional +6 mip/large-UV/last-level GPU cases PASS; native/PSX/CPU PASS/no VUID.
Six additional1.41 eastnorth/eastsouth/west captures: eleven-view3079701 colours/
87152 bands PASS;44 original/36 HD colour parts, content hashes exact after local-ID exclusion.
Mixed-height fix: copied569/681 source tops both worldY17. Consecutive indexed
ranges preserve source bytes/order/logical IDs; horizontal tops canonical, sides
raster. Caps4096 mesh/frame draws, separate32MiB metadata, atomic rejection/fence
retirement. Public ABI/shaders unchanged. CPU17 new checks/remaining host suites,
asset1536/VS/native/PSX GPU PASS. First GPU attempt fails old logical draw count;
corrected test passes11685 top pixels/order, holes/nearer depth/IDs/retirement.
Actual1.41 normal/wide:11456/2639 overlaps, wrong4462/2111 ->0; source-colour56030/
tree542851 PASS, exact buffers/mips/bindings;876 parts/1023 world draws/148320 bytes.
Four HIGH warnings and Pixel History/raw-ID discrepancy retained; cost unaccepted.
Forkae83dd2 pushed/gitlink staged; six files byte/hash-restored, all10 VS pairs/
Design/game startup/zero breakpoints; owned targets/replays closed. MCP C++ startup
setter needs REDRIVER2.vcxproj; reopens can select FontTool/rewrite cached .user
history. West144 lists/628 references/530 slots match530 copies; old spawn empty/resident.
New road normal/wide GPU camera[-7726,303,-230042] passes ten source/MVP/post-VS checks;
Road1217385 colour/ID/depth/4616 occlusions pass; backdrop3861031 colours/795828 sky pass, blue cap is original1x1 art; negative control rejects blue+5.
Direct workflow48.48s/native GPU/fresh capture/33s replay/810425 oracle PASS; equal MCP. Subsequent fixture result enum fixes discarded failure: real no-driver exit1, restored native exit0/PASS24.83s, PSX5.30s; guards compile. Null environment restoration initially failed; remove absent variables. [Exit evidence](rendering/r03-fixture-exit-validation.md).
Pause checkpoint2026-10-08: user cancelled the goal; no further renderer implementation. R00-R02 Windows scopes accepted/R03 incomplete/R04 unstarted. [Complete two-view composition](rendering/r03-composition-trace-validation.md) passes4147200 pixels,0 errors/exclusions;2873 candidate traces/764548 steps,42 new controls and20 original hashes preserved, four HIGH diagnostics retained. Last investigation independently parsed original CHICAGO.LEV topology:530 instances/876 parts/4814 triangles per view match both retained native exports;43 placement/selected models, seven streamed-only models. Host run exit0/0.879s, but negative controls NOT RUN; placements still use retained live-source ledger, no new acceptance. [Exact stopping point, hashes, limits and next actions](rendering/r03-pause-checkpoint.md). First resume step: verify inputs, add topology corruption controls and unchanged-input hashes, then source placement/pose/region/other-view/silhouette/resource/GPU-cost gates. Private captures/scripts/receipts live in ignored src_rebuild/build/native-renderer-r03 and do not travel with a Git branch; preserve locally/back up before cleaning. PsyCross clean ae83dd229f2c4a287575fb4cb853006996d9eef8; parent has extensive existing dirty/untracked work and staged gitlink. No new production/build/game/capture/GPU fixture or branch/commit/push at pause. Use direct MSBuild/CLI/local RenderDoc1.41, observing the same process across yields; VS MCP for debugger state. Native failures remain explicit, OpenGL and unmigrated R06/R10 consumers preserved. Goal inspection returns null.
