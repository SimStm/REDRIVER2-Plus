---
type: Roadmap
title: Deferred Vulkan to OpenGL compatibility
status: planned
created: 2026-10-06
tags: [roadmap, rendering, vulkan, opengl, compatibility]
---

# Deferred Vulkan to OpenGL compatibility

Scheduling: **no priority, execution order or deadline assigned**. This backlog
does not block Vulkan work. OpenGL implementation starts only when requested by
the user. Recording order is history, not scheduling priority.

2026-10-06 documentation reset: this backlog deliberately stays active at its
mandated path. Future modifications follow the new
[native rendering stages](../../rendering/implementation-plan.md). Historical
evidence links now point to the excluded archive; consult those contents only
on explicit user request. Moving documentation did not implement a Vulkan feature
or an OpenGL port, so no new port task is claimed for the documentation reset.

2026-10-08 planning-only update: native execution now follows R00-R07 -> R10 ->
R11 -> R08 -> R09 -> R12, retaining IDs; Chicago water is not a R03 blocker and
Rio coverage remains pending. Affected records: rendering plan/catalog,
stages03/08/09/10/11/12, decisions/validation, umbrella roadmap, status/handoff
and changelog. No Vulkan code, interfaces, shaders, ownership or tooling changed;
OpenGL port work is not applicable to this scheduling/scope clarification.
Existing port entries remain independent and unscheduled.
Validation: scoped `git diff --check`, order/dependency/local-link/text checks
and both status-file line limits PASS; no backend build or runtime test was run.

## Problem

The user chose Vulkan-only renderer development for now. Changes need a durable
record so later OpenGL compatibility work can recover the intended behaviour,
integration points and evidence without reconstructing earlier sessions.

## Intended behaviour

Record every Vulkan modification from 2026-10-06 onward in this document. Later,
port applicable behaviour to OpenGL with source-backed validation. Explicitly
record changes with no applicable OpenGL equivalent and the reason.

## Scope

- Features, fixes, shaders, resource lifetime, synchronization, shared interfaces,
  build configuration and tooling that affect Vulkan.
- Per-change source references, Vulkan validation, OpenGL impact, concrete port
  tasks, dependencies, risks and completion evidence.
- Maintain this backlog in the same change as each Vulkan modification, following
  [the tracking rule](../../rules/vulkan-opengl-port-tracking.md).

## Non-goals

- Implementing or validating OpenGL during the current Vulkan-only work.
- Assigning a roadmap number, priority, deadline or automatic port schedule.
- Treating proposed Vulkan work as implemented or auditing all historical changes.

## Dependencies and risks

- Port work depends on the actual delivered Vulkan behaviour and shared contracts.
- Vulkan-specific API mechanics may require a different OpenGL implementation
  or have no counterpart; document the behaviour rather than copying API calls.
- Preserve existing OpenGL code and platform guards during shared-source changes.
- Controlled backend comparisons require aligned scene, camera and settings.

## Acceptance criteria

- Every Vulkan modification after adoption has a dated entry with source
  references, actual validation and an explicit OpenGL disposition.
- Pending, ported, already compatible and not applicable entries are distinct.
- A completed port has source evidence and executed validation; completion of a
  Vulkan feature alone never closes its OpenGL task.

## Planned validation

- For each update, review the Vulkan diff against the backlog and run
  `git diff --check` on the changed documentation.
- When OpenGL work is requested, build the affected backend and run relevant
  regression tests and controlled captures. Record only executed results.

## Entry format

Each entry has: identifier and date; Vulkan status and behaviour; affected files
or commit; actual validation; OpenGL impact and concrete port tasks; dependencies
or risks; port status and evidence. Port status is pending, ported, already
compatible, or not applicable (with reason).

## Backlog

### VK-001 - Legacy shadow casters (2026-10-06)

- Vulkan status: implemented and validated experimental caster path. Adds an
  opt-in depth-only pipeline over
  queued opaque PGXP world triangles, conservative volume selection, shared
  texture cutouts, indexed texture batches and caster statistics. CPU selection
  uses cached upload copies and a local UBO copy; mapped Vulkan memory need not
  be CPU-cached. Composite descriptors update before command recording.
- Source: `PsyX_Vk.cpp`, `vk_shaders/psx_shadow.vert`, `vk_shaders/psx.frag`,
  the two new shadow SPIR-V binaries, regenerated
  `PsyX_Vk_Shaders.h`, `PsyX_public.h`, `DeveloperModernMesh.*`,
  `DeveloperGraphicsPanel.cpp`, and `scripts/compile_vk_shaders.ps1`.
  PsyCross fork commit `c564607` is pushed to `origin/master`.
- Shared contract: `PsyXModernLightSet.legacyShadowCastersEnabled` and
  `PsyXModernMeshStats.legacyCasterDrawCalls/legacyCasterTriangles`.
  `legacyshadowcasters=0` is the persisted default; the panel enables its
  control only on Vulkan.
- Actual validation: shader regeneration and Visual Studio MCP
  `Release_dev|x64` builds passed. RenderDoc on/off/sun-rotation captures replayed
  successfully without debug messages; initial batching is 15 draws/690
  triangles. See the [change record](../../outdated-ideas/2026-10-06-rendering-reset/changes/2026-10-06/legacy-shadow-casters/index.md)
  for final results and their limits.
  Final capture output matches the initial cutout prototype byte-for-byte;
  `-vkpsxtest` passed. Native submission is 8.735 ms off versus 8.9165 ms on,
  both approximately 30 FPS in 20 warmed windows. OpenGL remains untested.
- OpenGL impact/port status: **pending**, no priority assigned. Add a matching
  legacy depth program over the live vertex buffer in `g_shadowFbo`, implement
  the same PGXP/offset reconstruction and conservative triangle selection,
  preserve PSX/override cutouts, respect the final-overlay boundary, batch
  compatible opaque geometry, publish the statistics, and enable the panel
  control once the backend supports it. Reuse stable texture state; preserve
  main-pass VRAM ordering. Validate bias, sun rotation, UI exclusion and cost.
- Risks: only camera-submitted geometry is available; queued-VRAM casters are
  skipped and billboards retain camera-facing geometry. CPU/index storage must
  be bounded and complete for the frame; resource reuse follows frame fences.
  No OpenGL code or validation was performed in this delivery.


### VK-002 — Resident static world shadows and coordinate/receiver fixes (2026-10-06)

- Vulkan status: delivered opt-in, single-player opaque source-world shadow
  triangles from resident cells, independent of camera/PVS/backface rejection.
  Copied one-frame API, bounded CPU/GPU storage, fence-ordered upload, separate
  biased packed-xyz pipeline and `worldCasterTriangles` statistics. Game boundary
  converts physics/glTF/UI Y up to source scenery/camera Y down; valid zero-height
  grounding is preserved. Explicit UBO centre/extent fixes sunlight fade; receivers
  reject negative light depth and modern shading gains nine-tap PCF.
- Files: `DeveloperWorldShadow.*`, `DeveloperModernMesh.*`,
  `DeveloperGraphicsPanel.cpp`; PsyCross `PsyX_public.h`, `PsyX_vk.h`,
  `PsyX_ModernMesh.cpp`, `PsyX_Vk.cpp`, `vk_shaders/psx_composite.frag`,
  `psx_modern.vert/.frag`, their SPIR-V and `PsyX_Vk_Shaders.h`.
  PsyCross commit `1f8b5a5` is pushed to `origin/master`.
  Generated VS projects were backed up/regenerated/restored to include the new
  source while preserving GL/playground configurations; no tracked build-script
  change was needed. The existing shader compiler script was executed unchanged.
- Contract/default: `PsyX_ModernShadow_SetWorldTriangles`, 196,608 vertices;
  `worldshadowcasters=0` default, Vulkan panel gating and legacy-path replacement
  while active. Unsupported backends return 0; shared public declarations remain
  C-compatible. Reserved payload bound 6.75 MiB across three buffers.
- Actual validation: shader compilation, Visual Studio MCP Release_dev x64 and
  PSX self-test passed. Four RenderDoc fallback captures replayed without debug
  messages; high/low sun and 90-degree heading have identical 4,266-triangle
  static batches, one draw / 762 objects. Collection/copy median 174.5 us under
  RenderDoc, ten windows. Test inis restored byte-for-byte. GPU/native cost,
  moving/streaming transitions and other cities remain unvalidated. The
  `rdc-tools` MCP doctor/status timed out; GPUInfo tools remain absent. See
  [full evidence](../../outdated-ideas/2026-10-06-rendering-reset/changes/2026-10-06/world-space-shadow-foundation/index.md).
- OpenGL impact/port status: **pending**, unscheduled. Implement the copied
  source-world triangle API in the GL shadow FBO with matching lifetime,
  capacity/overflow policy and statistics. Adopt coherent game-coordinate
  conversion/grounding for colour, shadow, lights and volume; compare orientation,
  sun elevation and zero-height surfaces. Add explicit fade volume inputs,
  negative light-depth rejection and matching PCF. Preserve cutout/dynamic policy
  and final-overlay ordering. Enable the world caster control only after support.
- Vulkan-specific mechanics: Vulkan buffer allocation, frame-fence upload and
  embedded SPIR-V have no direct GL port; reproduce their ownership/visibility
  guarantees with GL's resource model. Generated-project restoration is local
  build-environment preservation, not a renderer behaviour to port.
- Risks: resident-only selection, finite map, approximate legacy receivers and
  excluded dynamic/cutout/event categories. No OpenGL runtime/build validation
  or implementation was performed. This does not complete the broader lighting
  or draw-distance roadmap and does not close VK-001's legacy-cutout port.

### VK-003 - Resident world-shadow budget diagnostics (2026-10-06)

- Vulkan status: delivered CPU diagnostics in `DeveloperWorldShadow.cpp`:
  resident/unavailable search cells, exclusions, payload bytes, region IDs,
  failed-frame windows and mean/peak collection/copy cost. Selection is preserved;
  the guard mirrors the owning cell iterator without requesting loads.
- Actual validation: Visual Studio MCP `Release_dev|x64` passed. Six RenderDoc
  MCP captures/replays; baseline versus diagnostics pixel-identical and matching
  4,266-triangle hash, low-sun/stress/recorded-route inspection. At extent 60,000,
  62,633 triangles approached capacity without overflow. Fresh installed 1.46
  capture/replay was validation-clean; MCP 1.41 captures retain instrumentation
  barrier warnings. Settings restored byte-exactly. See
  [measurement evidence](../../outdated-ideas/2026-10-06-rendering-reset/changes/2026-10-06/world-shadow-budget-diagnostics/index.md).
- OpenGL impact/port status: diagnostics reuse the existing game-owned collector,
  currently gated to Vulkan. No GL backend code is applicable to the logging
  itself. When VK-002 enables GL world casters, retain these CPU diagnostics and
  validate region-slot changes/failure windows there. Deferred, unscheduled;
  no priority, order or deadline. No GL implementation or validation performed.
- Risks: conservative cell counts are not exact missing-caster counts; payload
  bytes exclude GPU/process memory, replay counters are not native frame time,
  and whole-batch overflow remains the existing policy.

### VK-004 - Bounded original-texture static colour baseline (2026-10-06)

- Vulkan status: experimental single-player `staticunlit` (default 0), one opaque
  original building with reusable per-material geometry, nearest CLUT textures,
  original modulation/dither and shared colour/shadow buffers. Generic descriptor
  fields select unlit shading; separate visibility expires after recording.
  Catalog/region/VRAM checks restore fallback; input toggles precede selection.
- Files: game `DeveloperStaticScene.*`, `DeveloperModernMesh.*`,
  `DeveloperGraphicsPanel.cpp`, `DeveloperWorldShadow.cpp`, `Game/C/draw.c`,
  `main.c`; PsyCross `PsyX_public.h`, `PsyX_vk.h`, `PsyX_ModernMesh.cpp`,
  `PsyX_Vk.cpp`, `vk_shaders/psx_modern.frag/.spv`, `PsyX_Vk_Shaders.h`.
  Existing Premake glob covers the new file; ignored VS projects were restored
  with GL/playground configurations and new source entries. Shader compiler ran
  unchanged; Vulkan embedded SPIR-V has no direct GL port.
  PsyCross fork commit `5ebd3b2` is pushed to origin/master.
- Actual validation: VS MCP Release_dev x64, shaders and PSX self-test passed.
  MCP shared-buffer/depth-state and route culling inspected; sampled parity differs
  in 202/921,600 pixels. Fresh 1.46 capture/replay has zero messages; MCP1.41
  retains instrumentation warnings. Four INIs restored byte-exactly. See
  [evidence](../../outdated-ideas/2026-10-06-rendering-reset/changes/2026-10-06/static-world-unlit-baseline/index.md).
- OpenGL impact/port status: **pending, unscheduled**. Add equivalent original
  texel/modulation/dither handling with framebuffer colour-space conversion,
  unlit descriptor support, shared depth and consumed frame visibility. Extend
  game gating after backend validation. Current GL returns failure for these
  new capabilities; existing lit defaults remain unchanged. No GL validation.
- Limits: opaque nearest-only, no enabled overrides, one instance/four materials/
  256 polygons. Controlled receivers, cutouts, picking, broad depth acceptance,
  invalidation transitions and native profiling remain pending. Resource creation
  can stall; memory caps are payload bounds. No priority, execution order,
  deadline or Vulkan-blocking dependency.

### VK-005 - Original lighting, pooled geometry and resident reach (2026-10-06)

- Vulkan status: delivered source-world normals and original diffuse receiving,
  controlled prelit-art blend, shared source/LOD texture resources and copied
  frame instances. Resident visual selection extends eligible coverage without
  loading/gameplay changes. Two nested 2048-square volumes share a 4096x2048 D32
  atlas, with tile-clamped PCF/bias scaling/transition blend. Overflow retries near;
  UI reports effective coverage. This is not camera-frustum-fitted CSM.
- Files: `DeveloperStaticScene.*`, `DeveloperModernMesh.*`,
  `DeveloperGraphicsPanel.cpp`, `DeveloperWorldShadow.*`, `Game/C/draw.c`;
  PsyCross `PsyX_public.h`, `PsyX_vk.h`, `PsyX_ModernMesh.cpp`, `PsyX_Vk.cpp`,
  `vk_shaders/psx_modern.vert/.frag`, `psx_composite.frag`, new `game_shadow.glsl`,
  matching `.spv` and `PsyX_Vk_Shaders.h`. Existing source/build guards preserved;
  shader compiler ran unchanged, including resolving the shared GLSL include.
  PsyCross fork commit `c2ab107` is pushed to origin/master.
  Generated SPIR-V/header and Vulkan atlas device-limit checks have no direct
  OpenGL implementation to port; GL needs its own shaders/resource-limit checks.
- Actual validation: VS MCP Release_dev x64, shader regeneration and fresh PSX
  self-test passed. Per-instance colour/shadow matrices matched; same-frame shader
  replacements confirmed original receiving and far contribution. Low-sun and
  region-route captures at 18k fit; 40k exercised near fallback. VS CPU scopes and
  allocation bounds recorded. Fresh 1.46 first replay clean; MCP1.41 retains
  instrumentation warnings. Four INIs restored byte-exactly. See
  [evidence](../../outdated-ideas/2026-10-06-rendering-reset/changes/2026-10-06/static-world-lighting-and-range/index.md).
- OpenGL port status: **pending, unscheduled**. Implement generic frame-instance
  copying/expiry and original diffuse mode/strength, with world normals independent
  of camera projection scaling. Match original texel/dither/colour-space handling
  from VK-004. Extend static/LOD game gating after GL validation. Port pooled
  resources, dual shadow maps/atlas, tile-safe PCF, stable volumes, blending and
  world-unit bias to both modern and legacy receivers. Match far-extent/stat APIs
  and whole-collection near retry; validate actual coverage/resource budgets.
  New Vulkan-only mesh capabilities currently return failure on GL; its existing
  rendering path is preserved. No GL compilation or rendering validation performed.
- Bounds/risks: eight source keys, sixteen textures, 128 placements (default 64),
  finite resident range, nearest opaque art only. Game atlas32 MiB even far off;
  two-volume raster work and creation stalls need hardware profiling. Dynamic/
  cutout/streaming acceptance remains pending. There is no priority, execution
  order, deadline or dependency that blocks Vulkan work.

### VK-006 - Light-demand selection and complete range budgets (2026-10-06)

- Vulkan status: exact snapped CPU matrix/world-bounds query supports conservative
  resident cell/source-bound/triangle selection. Complete far tiers requested,
  75%, 50%, near-only reduce capacity pressure; recovery waits 120 successful
  frames before one higher probe. F11 reports effective range/culls/retries.
- Files: game DeveloperWorldShadow.cpp/.h, DeveloperModernMesh.cpp,
  DeveloperGraphicsPanel.cpp; PsyCross include/PsyX/PsyX_public.h, PsyX_vk.h,
  src/render/PsyX_ModernMesh.cpp, PsyX_Vk.cpp. Product, roadmaps, discussion,
  rule, status/handoff, changelog and validation records document this surface.
  Fork446b96d pushed origin/master; parent gitlink staged. No new build source,
  shader, UBO, atlas, pass or allocation change, so those need no new port entry.
- Actual validation: VS MCP Release_dev x64 and fresh PSX self-test passed;
  fresh installed1.46 capture/first replay zero messages. Spawn/route at18k have
  identical raw maps/colour with 8,751/8,535 triangles instead of 21,766/28,632.
  Low-sun40k route fits48,477 triangles/both volumes, preserving near depth.
  Isolated actual-function tests with mocked capacity passed clipping/bounds and
  complete tier reduction/recovery; real optimized route did not overflow.
  CPU scopes recorded without GPU/FPS claims; no shader regeneration needed/run.
  MCP1.41 instrumentation warnings remain. Four INIs restored byte-exactly. See
  [evidence](../../outdated-ideas/2026-10-06-rendering-reset/changes/2026-10-06/light-demand-shadow-budgets/index.md).
- OpenGL port status: **pending, unscheduled**. Provide equivalent snapped CPU
  query and failure/output contract. Use GL clip-depth convention (negative W
  near), or explicitly supply depth semantics, rather than copying Vulkan planes.
  Enable game selection/tier/UI diagnostics after matching matrices, conservative
  source bounds and complete-collection parity tests. Current GL returns failure
  for the new query; no GL build/render validation performed. Existing GL path
  and shared platform guards are preserved. No priority, execution order, deadline
  or Vulkan-blocking dependency; no streaming/dynamic acceptance claim.

### VK-007 - R00 native-replacement baseline and diagnostics (2026-10-07)

- Vulkan status: implemented baseline only. C-compatible producer counts for 17
  categories and F11/log diagnostics separate legacy, earlier experiments and
  native (zero). Read-only physical feature/format/sample/memory-budget query;
  adopted native ownership/handle/snapshot/camera/pass contracts and test budgets.
  No Vulkan shader, device, synchronization, allocation or draw-order change.
- Files: `utils/DeveloperRenderCoverage.*`, `DeveloperGraphicsPanel.cpp`,
  `Game/C/{main,draw,tile,cars,sky,event,loadview,shadow,debris,motion_c,overmap}.c`,
  `Game/Frontend/FEmain.c`, `tests/RenderCoverageTests.cpp`,
  `tests/RenderCoverageCProbe.c`, `scripts/run_render_coverage_tests.ps1`,
  `scripts/query_vulkan_baseline.cpp/.ps1`; active rendering notes, product,
  status/handoff, changelog and debug-start/project-generation rules. Premake
  utils glob already covers the new implementation; generated membership refreshed.
- Actual validation: Windows Release_dev x64 VS build, 12 invariant/C ABI tests
  and fresh PSX readback PASS; installed RenderDoc 1.46 city/wide/replay/playground
  first replays zero messages. Matched original-city RGB/pass/index counts
  unchanged (773 draws). F11 17 rows inspected. Probe succeeded; physical 1/2/4/8
  support differs from effective single-sample game targets. Five INIs and original
  debugger settings restored byte-exactly. GPU time/renderer allocation peaks
  unknown; no Linux/OpenGL run. [Evidence](../../rendering/r00-measured-baseline.md).
- OpenGL disposition: producer counters/table/log are **shared source, runtime
  unvalidated on GL**; no backend port code is needed, but run matching GL mode
  samples before claiming parity. Vulkan capability query is **not applicable**
  as an OpenGL API implementation; future GL quality support needs its own actual
  context/format/sample queries. Native contracts are future design: GL port must
  preserve one owner/lifetimes and explicitly choose its clip-depth convention.
  No native renderer port delivered or scheduled. This backlog does not block R01.

### VK-008 - R01 native world, copied scene and camera/depth (2026-10-07)

- Vulkan status: implemented opt-in analytic indexed world inside the existing
  sole device/swapchain/command/fence owner; copied C POD snapshots, bounded
  generational meshes, fence upload and deferred destruction. RH final-game
  view/projection, independent D32, real source position/normal/UV/linear colour,
  unlit shaders and separate original HUD/minimap/ImGui composition. Legacy
  world/composites are suppressed only for an effective complete native frame.
  R01 is accepted for the declared Windows analytic slice at fork `99fa641`;
  original-world migration remains pending. [Evidence](../../rendering/r01-validation.md).
- Files: PsyCross `include/PsyX/PsyX_native.h`, `PsyX_vk.h`,
  `src/render/PsyX_NativeScene.*`, `PsyX_VkNative.inl`,
  `PsyX_VkNativeTests.inl`, `PsyX_Vk.cpp`, native vertex/fragment GLSL/SPIR-V and
  `PsyX_Vk_Shaders.h`; game `DeveloperNativeScene.*`, `DeveloperNativeCamera.*`,
  `DeveloperGraphicsPanel.cpp`, `DeveloperVkFixture.cpp`, `Game/C/main.c`,
  `redriver2_psxpc.cpp`, `premake5_psycross.lua`, native CPU/C tests and runner,
  `scripts/compile_vk_shaders.ps1`, active rendering/product/status documentation.
- Actual validation: Windows11/RTX3060Ti/610.88, VS Release_dev x64 PASS;
  26 ownership/C ABI/camera tests and host excluded-platform compilations PASS.
  Native GPU readback, resize800x600/restore1280x720, minimize skipping submission,
  clipping/winding/depth, camera/FOV, UI and eight reloads PASS; fresh PSX
  self-test PASS. Tests found/fixed old-size native depth allocation and a stale
  resize-scissor extent in the test producer. Plain VS validation-enabled run
  has no VUID messages after the scissor correction. RenderDoc1.46 city/pass/
  post-VS inspection PASS, PGXP on/off PNGs SHA256-identical; wide FOV clean.
  Capture2/6 clean; resize capture5 alone records legacy PRESENT/GENERAL warning
  under instrumentation. Separate plain validation run is clean; no GENERAL
  layout exists in application source. See R01 evidence for its disposition.
  Existing OpenGL3.3 city/HUD/ImGui launches with `-opengl -nativeworld` and logs
  unsupported backend, preserving the original world. This is fallback validation,
  not native GL parity; Linux/macOS/complete other-target builds unexecuted.
- OpenGL port status: **pending, unscheduled**. Share copied descriptors/CPU
  validation, identity/reset contracts and diagnostics, implement GL-owned indexed
  buffers/pass/upload-retirement with one context/presenter, native shaders and
  an independent depth attachment. Choose GL clip depth/Y conventions explicitly;
  Vulkan matrices and positive viewport cannot be copied without adaptation.
  Validate original UI composition, resource lifetime, PGXP/mod independence and
  resize/rollback before enabling the native adapter on GL. SPIR-V/header
  regeneration is **not applicable** to GL; compile corresponding GLSL instead.
  Native analytic/source categories remain distinct. Payload caps are32 meshes,
  64 instances,8 MiB CPU data, not measured GPU allocation peaks. No priority,
  execution order, deadline or dependency blocks Vulkan work.

### VK-009 - R02 original decoders and native materials (2026-10-07)

- Vulkan status: R02 original-import slice implemented and accepted on Windows. Bounded original
  MODEL/placement/VRAM decoders and separate source-length metadata, native PNG/
  CLUT copies with existing precedence/reload identity; C ABI v2 bounded material
  handles, sRGB owned images/clamp samplers/descriptors, checked same-queue upload
  and last-submission retirement. Initial upload waits on its own fence in the existing queue.
  Model/parent catalog geometry cache, pre-GTE read-only city probe and owned GLB
  primitive bridge are implemented. Original GPU/material cache now keys actual
  content, live tpage/CLUT, region, mod revision and filtering, with atomic rollback
  and both catalog/scene generations. An incomplete Chicago import fixture follows
  actual selected source/LOD resources. Capture byte hashes and live catalog slot/
  context reuse pass; complete static region and long streaming routes remain R03/R04.
- Files: game `NativeAssetDecode.*`, `NativeTextureDecode.*`, `HdTextureOverrides.*`,
  `Game/C/models.*`, `spool.c`, `playground.c`, `DeveloperVkFixture.cpp`,
  `NativeAssetCache.*`, `NativeModernDecode.*`, `DeveloperNativeAssets.*`,
  `NativeGpuAssetCache.*`, `NativeOriginalMaterials.*`, `DeveloperNativeAssetFixture.*`,
  `Game/C/draw.c`, `tile.c`, `main.c`, `redriver2_psxpc.cpp`,
  `tests/NativeAssetTests.cpp`, `InspectorExportTests.cpp`, native ownership tests
  and asset runner; PsyCross `PsyX_native.h`, `PsyX_NativeScene.*`,
  `PsyX_VkNativeMaterials.inl`, `PsyX_VkNative.inl`, `PsyX_VkNativeTests.inl`,
  `PsyX_Vk.cpp`, native GLSL/SPIR-V/embedded header. Product/status/stage/changelog
  updated. R02 fork `9ff48afee0384f92157568f6a2a3400b06b55fbd` is committed/pushed
  to origin/master and the parent gitlink staged.
- Executed validation: decoder/cache/owned-GLB1,202 checks; fresh catalog145/export120 checks;
  scene18/camera8/material11 cases and host platform guards; shader generation,
  VS Release_dev x64 build PASS. Validation-enabled native GPU readback PASS:
  analytic regression, exact188 sRGB linear midpoint, cutout/background, byte-
  identical native output across legacy filter0/1, eight material lifetime cycles.
  Validation loaded/no VUID output. Report enlarged to retain complete late-cycle
  evidence; complete rerun and fresh legacy PSX readback PASS. Read-only Chicago
  probe69 CPU entries/68 complete/one unsupported and stable257972 accounted
  bytes; cached fallback stats corrected and rerun confirms one rejection.
  Owned GLB2220 vertices/1479 triangles/bounds/UV/PNG and unchanged file hash
  checked. New asset observer platform stubs compiled on host.
  GPU timing/peaks and other-target runtime acceptance unexecuted.
  [Product](../../product/native-original-assets.md), [evidence](../../rendering/r02-validation.md).
  Submitted-upload fault injection now PASS: real submission, conservative failed
  wait, original-world draw/pixel fallback, retained command/staging/image/fence,
  actual fence recovery and zero ownership. Added vkGetFenceStatus loading;
  shutdown preserves all resources if device-idle completion is unconfirmed.
  Actual road/traffic-light/street-lamp import samples drew with enabled PNGs and
  bilinear1/no VUID. Final source37 road/facade273 capture:14 D32 native draws/
  534 indices,10 D24S8 UI draws/318 indices, no messages. Four complete vertex/
  index/artwork hashes and14 GPU/source transforms match. Live filter/PGXP/mod/
  UI reload and actual level reload with slot revisions/context generation pass.
  Forced reset during creation exposed orphan resources; interrupted-transaction
  rollback now passes regression and repeated GPU fault, restoring four resources
  and7176/4247296 bytes. Upload evidence metadata/fixture logs are game-owned;
  their GL reuse needs matching GL capture validation, not another Vulkan port.
  Original
  texture crop now includes source UVs outside named rectangles; active PNGs keep
  explicit clamp semantics. Bounded tests cover atlas expansion and overscan.
- OpenGL disposition: original decoders/loader metadata/mod copy semantics are
  shared source; their GL runtime use remains unvalidated. Native material API
  currently returns unsupported on GL. Port its copied generational contracts,
  owned sRGB textures, isolated clamp filtering, alpha cutoff and deferred release
  into the same GL context, then test linear-light interpolation, content/reload
  invalidation, stale handles, UI and resize. Vulkan descriptor/barrier code and
  SPIR-V are not directly applicable; corresponding GL objects/GLSL are required.
  No native GL implementation or parity delivered; pending and unscheduled.

### VK010 - R03 region inventory and shared native materials (in progress)

- Behaviour: game-owned fixed Chicago cell collection precedes legacy GTE work,
  with owned placement keys/identities and an incomplete opt-in indexed draw
  path. Exact-content shared materials retain reference ownership across failure,
  replacement and scene reset. Native C ABI v3 adds back/two-sided culling;
  Vulkan selects the corresponding depth-tested pipeline. Payload limits grow
  to512 meshes/4096 instances/32MiB geometry and256 materials/64MiB RGBA.
- Files: `DeveloperNativeRegion.*`, `DeveloperNativeAssets.cpp`,
  `NativeGpuAssetCache.*`, `NativeOriginalMaterials.cpp`,
  `DeveloperNativeAssetFixture.cpp`, `redriver2_psxpc.cpp`, `Game/C/main.c`,
  native asset/scene tests; fork `PsyX_native.h`, `PsyX_NativeScene.*`,
  `PsyX_VkNative.inl`, `PsyX_VkNativeTests.inl`, native test runner.
  R03 fork304c14a committed/pushed to origin/master; parent gitlink staged.
- Actual validation: initial ordinary-cell probe144 resident cells/530 distinct
  copied objects/22 cutouts/zero source rejection,3642 triangles/672 parts.
  Actual high-detail region4814 triangles/876 parts, usage bitset preserved,
  legacyWorld0. RenderDoc1.46 frame610 confirms876 D32 draws/14442 indices plus
  10 D24S8 UI draws/318 indices, no messages. Immutable PNG owners avoid large
  repeated copies; exact-byte reuse skips repeated whole-artwork hashing while
  preserving live-original VRAM content comparison. Actual mean assembly
  45.910 ms/121 samples before,3.710 ms/481 after; includes initial uploads,
  different cumulative windows, no GPU time/p95 claim. Asset/cache1219,
  scene18/camera8/material13 and VS Release_dev x64 PASS.
  Recent120 CPU assembly mean2.983/p953.540 ms at tick600; collection mean0.308/
  p950.396 ms. Validation-enabled GPU reverse-facing two-sided cutout/back-cull
  tests and fresh PSX readback PASS, Khronos loaded/no VUID output. Unknown source
  blend-policy/water flags explicitly reject the incomplete opt-in view; current
  region contains neither. Shape names/alpha alone do not establish blending;
  the source-backed rule records this constraint. No foreign-target runtime claim.
  Region coverage/mips/picking/turns/FOV/mod captures remain pending.
- OpenGL work: collector/identity/material sharing are backend-independent game
  ownership code; no separate algorithm port is needed, but GL use is untested.
  Implement v3 cull policy and matching bounded generational native ownership
  in GL, then prove shared-content rollback/reset and two-sided cutout drawing.
  Vulkan pipeline objects have no direct GL equivalent. No GL parity delivered;
  pending and unscheduled; this backlog does not block Vulkan work.

### VK011 - Original native sky/painted horizon (R03 in progress)

- Behaviour: C ABI v4 world/backdrop layers; Vulkan draws the camera-relative
  original backdrop before world geometry using static depth test/write off.
  Bounded original MODEL/art bindings are copied before GTE sky projection;
  original vertical offset and normal/reflection brightness are retained.
  Current VRAM originals resolve page/CLUT/UV artwork with exact-content GPU
  reuse; immutable frame-local artwork shares duplicate decoded bytes.
- Files: `Game/C/sky.c`, `DeveloperNativeBackdrop.*`, `NativeBackdropDecode.*`,
  `DeveloperNativeCamera.*`, `DeveloperNativeRegion.cpp`, native test runners/
  tests; fork `PsyX_native.h`, `PsyX_NativeScene.cpp`, `PsyX_VkNative.inl`,
  `PsyX_VkNativeTests.inl`.
  Fork83ccdbc committed/pushed to origin/master; parent gitlink staged.
- Actual validation: asset1503/scene20/camera12/material13 and VS Release_dev
  x64 PASS, excluded-platform host guards PASS. Actual city956 native draws:
  876 world/80 backdrop; models0..3 each30 vertices/20 family21 quads.
  RenderDoc1.46 frame500 verifies80 D32 depth-off draws/480 indices and876
  depth-on draws/14442 indices,10 original UI draws and zero messages. Backdrop
  depth probes stay1.0; after world, sky stays1.0 and world becomes<1.0.
  Native GPU readback proves order/no depth write/translation/rotation/UI/
  retirement; fresh PSX regression PASS, Khronos loaded/no VUID output.
  Shared frame artwork cuts recent backdrop CPU mean8.676 to4.000 ms;
  before/after1920x1080 sky-only replay pixels match exactly. CPU windows only,
  not total frame/GPU timing; subsequent depth state/probes remain correct.
  The first game run correctly fell back for missing material revision; fixed
  before successful capture. R03 acceptance/foreign-target runtime remain pending.
- OpenGL work: implement v4 copied layer contract, explicit backdrop-before-world
  ordering, camera translation removal, original view-space offset and scoped
  depth test/write off with restoration. Game-owned original source/VRAM adapters
  need no separate decoder algorithm port, but GL integration is untested.
  Prove source/art parity, rotation/translation, UI and retirement with GL pixels.
  Pending and unscheduled; no OpenGL implementation/parity is delivered.

### VK012 - Owned native trilinear mip chains (R03 in progress)

- Behaviour: ABI v5 trilinear mode owns a complete chain; nearest/linear retain
  level-zero sampling. Generic CPU generation uses linear-light alpha-weighted
  NPOT area filtering and closest attainable cutout coverage. All derived RGBA
  counts toward64MiB before source traversal. Native Vulkan uploads every level,
  transitions/views the full range and uses linear min+mip filtering/maxLOD last.
  Original world/backdrop smooth filtering selects trilinear. No legacy mip
  implementation edit. Live filter replacement still overlaps texture sets,
  temporarily exceeding the cap and falling back; R03 filtering remains pending.
- Files: fork `PsyX_native.h`, `PsyX_NativeMip.*`, `PsyX_NativeScene.*`,
  `PsyX_VkNativeMaterials.inl`, `PsyX_VkNativeTests.inl`; game
  `NativeGpuAssetCache.cpp`, `NativeOriginalMaterials.cpp`,
  `DeveloperNativeBackdrop.cpp`, native test suites/runners and knowledge.
  Forkd3e5a40 committed/pushed to origin/master; parent gitlink staged.
- Actual validation: asset1505/scene20/camera12/material13/mip13 PASS; excluded
  host guards PASS; VS Release_dev x64 build/all10 configs preserved. Direct
  Khronos native/PSX GPU suites PASS/no VUID: minified checker188 vs base-only
  black, transparent background/opaque white without red bleed, full-chain
  budget and real submitted-upload fault retention/fence recovery to zero.
  RenderDoc1.46 city frame500 proves61 complete native chains/views/linear mip
  samplers/maxLOD last,80 backdrop/876 world/10 UI draws/no messages. Steady
  city native956/RGBA49926792 bytes. Both filter directions log transient cap
  failure before recovery; no filtering acceptance/performance/parity claim.
- OpenGL work: adopt ABI v5 filter semantics and shared owned mip layout; upload
  all generated SRGB levels with matched views/base/max level limits and
  trilinear minification. Preserve alpha-test holes, cutoff semantics and GPU
  retirement guarantees. Test linear-light188, NPOT/thin textures, cutouts,
  full-chain ownership and filter-transition stability with actual GL pixels.
  CPU algorithm is shared and needs no duplicate decoder port; GL sampling/
  integration is unimplemented. Pending, unscheduled and non-blocking.

### VK013 - Native runtime sampling without texture replacement (R03)

- Behaviour: ABI v6 copies optional per-instance nearest/linear/trilinear
  selection. Original native world/backdrop/import fixture retain one full chain;
  Vulkan prepares up to three descriptor sets/samplers once over that image and
  chooses a set per draw. No in-flight descriptor mutation. Base-only samplers
  clamp maxLOD0; trilinear uses the final level. Incomplete-chain overrides reject
  atomically; hardware linear support is checked and missing sets are unsupported.
  Bounded pool3*(256+1); cumulative effective/fallback/ready-upload counters expose
  temporary fallback that steady-state snapshots miss. This fixes VK012's
  transient filter replacement cap failure without raising64MiB.
- Files: fork `PsyX_native.h`, `PsyX_NativeScene.cpp`, `PsyX_VkNative.inl`,
  `PsyX_VkNativeMaterials.inl`, `PsyX_VkNativeTests.inl`; game
  `NativeOriginalMaterials.cpp`, `DeveloperNativeRegion.cpp`,
  `DeveloperNativeBackdrop.cpp`, `DeveloperNativeAssetFixture.cpp`, scene/C tests,
  status/product/validation/plan/change records and mip ownership rule.
  Forkc051051 committed/pushed to origin/master; parent gitlink staged.
- Actual validation: scene20/camera12/material13/mip13/sampling8, asset1505,
  excluded host guards and VS Release_dev x64 PASS. Direct Khronos native GPU
  eight sampling choices produce black/188, with shared nearest/trilinear
  instances in one frame, zero new uploads/fallback and stable image/bytes;
  existing culling/backdrop/cutout/fault recovery PASS. Fresh PSX GPU PASS;
  both outputs load Khronos/no VUID. Actual city eight toggles/ticks120-1080:
  all960 intervening submissions native, uploads61/no extra, fallback0,
  native956/backdrop80/world0, stable image and49926792 RGBA bytes.
  RenderDoc1.46 nearest/trilinear city frame500 replays both61 full chains/views,
  proper min/mip samplers/maxLOD0 versus last,80 backdrop/876 world/10 UI/no
  messages; dimension/level/view/last1x1 RGBA profiles agree. No timing claim.
- OpenGL work: consume ABI v6 instance sampling independently of shared image
  ownership, retain sampler choices/whole chains and avoid texture replacement.
  Restore surrounding GL state. Port cumulative counters and reproduce actual
  minification/independent-instance/continuous-transition/retirement tests.
  Shared CPU snapshot validation needs no separate algorithm port; GL sampler/
  descriptor equivalent and runtime integration remain unimplemented, unscheduled
  and non-blocking. R03 picking/visual/coverage acceptance remains pending.

## VK014 - Frame-owned native logical picking (2026-10-07)

- Vulkan behaviour: ABI v7 picking (included in current v8) uses R32_UINT MRT
  IDs with native depth/culling/cutout, backdrop0 and UI exclusion. Bounded copied
  identities/generational handles resolve after one-pixel readback at the owner
  fence. Window scaling, resize, supersession/cancel and stale handles are explicit.
  Readback allocation publishes only complete local handles and cleans partial
  failures. Game-owned bounded model/placement/material-part metadata uses catalog
  generations; native face highlighting/export remain unimplemented.
- Affected files: fork `PsyX_native.h`, `PsyX_NativePick.h`, `PsyX_VkNative.inl`,
  `PsyX_Vk.cpp`, native shaders/SPIR-V/header and `PsyX_VkNativeTests.inl`; game
  `DeveloperNativeRegion.*`, `DeveloperGraphicsPanel.cpp`, scene/C tests and
  product/validation/status/change/plan records. Final fork revision recorded below.
- Actual validation: CPU picking19, scene/material/camera/mip/sampling regressions
  and host exclusion compiles PASS; asset1505 and VS Release_dev x64 PASS. Native
  GPU depth/cutout/order/shared-instance/miss/UI/supersession/cancel/resize/retirement
  and actual mapped-buffer rollback/retry PASS. Real Chicago building285 two parts/
  same logical instance, tree162/TREE01 and sky miss PASS. RenderDoc1.46 frame500
  R32_UINT ID image,80 backdrop/876 world/10 UI/no messages; UI leaves IDs unchanged.
  Direct native/PSX tests load Khronos/no VUID. No timing or allocation-charge claim.
- OpenGL work: integer attachment/output with matching alpha/depth/culling, bounded
  native ID table and asynchronous readback/owner-fence equivalent; logical SDL/
  drawable scaling and complete transactional allocation/retirement. Restore GL
  state around remaining UI consumers. Reproduce native pixel/order/shared identity/
  resize/cancel/resource-reuse tests. Shared copied source/CPU validation needs no
  distinct algorithm. Port status: pending, unscheduled, non-blocking.

## VK015 - Explicit native scene failure, no old-world recovery (2026-10-07)

- Vulkan behaviour: ABI v8 frame states distinguish expected source/upload loading,
  ready, real failure and remaining auxiliary consumers. Unavailable world frames
  clear their image and suppress old-world/shadow/composite/experimental draws;
  ordered VRAM and required overlays remain tracked dependencies. Status diagnostics
  are visible with F11 closed, use logical UI coordinates and known dimensions to
  avoid hiding the first failed frame. Separate ready/loading/failed/auxiliary
  counters replace the automatic scene-fallback counter. Loading is consumed per
  submission; failed producer status discards old publication. Actual submitted
  upload resources remain retained until real fence completion, then native recovery
  renders newly created resources. No new legacy recovery or OpenGL implementation.
- Affected files: fork `PsyX_native.h`, `PsyX_NativeScene.*`, `PsyX_VkNative.inl`,
  `PsyX_Vk.cpp`, `PsyX_VkNativeTests.inl`; game `main.c`, `redriver2_psxpc.cpp`,
  `DeveloperNativeScene.cpp`, `DeveloperNativeRegion.cpp`,
  `DeveloperNativeAssetFixture.cpp`, panel, CPU tests and active plan/gates/docs.
- Actual validation: frame-state13 and GPU error(32,0,32)/loading(0,0,32) versus old
  world(248,0,0) pixels PASS, no legacy world draws; remaining UI and rejected picks
  verified. Real submitted-upload fault retains handles, native recovery white255
  and retirement to zero owned bytes PASS. Real Chicago loading/error world0/UI10,
  recovery native956/world0/uploads61 with geometry148320/RGBA49926792 unchanged.
  F11-closed loading/error/first-failed-frame diagnostics visually verified.
- OpenGL impact/work: existing backend preserved. Native scene selection is
  separate from backend selection; unsupported native startup fails explicitly and
  analytic selector is disabled on non-Vulkan. If a native GL port is requested,
  it must implement the same states, no old-scene recovery, ownership-safe native
  retry and pixel/resource tests. Do not add legacy recovery as port work.
  Port status: pending for a future native GL renderer, unscheduled, non-blocking.

VK014/VK015 final fork: bd5b67540fcfb3028256ec2c1907212596d55185, committed/pushed
to origin/master and staged parent gitlink. Final native GPU report10966 characters
and fresh PSX GPU regression PASS, Khronos loaded/no VUID; host guards and VS build
PASS. Actual native-on-OpenGL startup exits1; separate OpenGL Chicago city renders.
Test INIs/`.user` restored byte-exactly, all10 solution pairs preserved.

## VK016 - Native region source/publication accounting (2026-10-07)

- Behaviour/files: game-owned `NativeRegionCoverage.h`, `DeveloperNativeRegion.*`,
  `DeveloperNativeAssets.*`, read-only `draw.c` candidate hook, F11 panel, native
  asset tests and product/stage/evidence/status/changelog records. Bounded frame
  slots reconcile selected/copied/published objects and exact part counts;
  duplicate/missing/inconsistent publication fails explicitly. Original view
  candidates outside the fixed region remain declared omissions; no old recovery.
- Actual validation:18 accounting cases and asset1523 PASS; scene/camera/material/
  mip/sampling/picking/frame-state and host guards PASS; VS Release_dev x64 PASS.
  Real Chicago530 selected/copied/published,876 parts,228 inside/78 outside/zero
  uncopied/duplicate/invalid; separate backend native956/world0/UI10,148320 geometry/
  49926792 RGBA bytes,61 uploads. No complete R03 or timing claim.
- OpenGL work: no graphics API or backend algorithm changed, so no API-specific
  port applies to this ledger/hook. A future native GL consumer must use the same
  explicit source/publication accounting and independently prove its GPU draws/
  pixels; shared counts alone cannot establish native GL coverage. Existing GL
  remains intact; this follow-up is unscheduled and non-blocking.
  New RenderDoc1.46 city frame500 replay PASS:80 backdrop/876 native world/10 UI,
  no messages; full colour and ID images equal the previous capture. Five INIs/
  `.user` restored byte-exactly, all10 VS configuration pairs preserved.

VK016 subsequent actual QA: four-quarter normal/wide FOV, translation, nearest/
trilinear and mod-off/on native pixel/resource comparisons PASS; restored world
pixels exact,740 frames native956/world0/no loading/errors. A133-step mapped-pad
physics fixture moves through turns/braking with native camera/street readbacks.
Restored GPU table61 images/allocations/no pending uploads/retiring materials;
five INIs/.user restored. Camera/settings gate accepted, road-marking strips and
terrain seam review still block R03/R04. No renderer code/API changed in this QA;
no new GL port applies. Existing OpenGL remains preserved and unmodified; a
future native GL renderer needs its own equivalent pixels/resources validation.

## VK017 - Authored native coplanar terrain layers (2026-10-07)

- Behaviour: bounded game-owned tile classification assigns smaller contained
  horizontal faces a depth layer without moving geometry. Native ABI v9 copies/
  validates layer0..8; decoder version2 separates old cached layouts. Vulkan
  dynamic constant bias -4 per layer makes paint survive observed D32 coplanar
  rounding; ordinary world/backdrop reset0. Colour/picking share alpha/depth;
  no old-scene recovery, slope/clamp or optional device feature is introduced.
- Files: game `NativeTerrainLayers.h`, `NativeAssetDecode.*`,
  `NativeGpuAssetCache.cpp`, asset/scene tests; fork `PsyX_native.h`,
  `PsyX_NativeScene.*`, `PsyX_Vk.cpp` loader, `PsyX_VkNative.inl` pipeline/draw,
  `PsyX_VkNativeTests.inl`; current product/plan/stage/terrain evidence/status/
  handoff/changelog. Scene reclamation and cache rejection preserve ownership.
- Actual validation: asset1536/scene22 and existing CPU/host suites, VS build,
  fresh native/PSX GPU regressions PASS; Khronos loaded/no VUID. GPU holes/nearer
  occlusion/both orders/IDs/retirement PASS. RenderDoc1.46 normal paint interior
  wrong IDs17050 ->0/45000, widened original0/35470; baseline/fixed bound
  buffers/all-mip image hashes match.80 backdrop/876 world/10 UI/no messages,
  148320 geometry bytes unchanged. See [evidence](../../rendering/r03-terrain-validation.md).
  Ground seams/full R03 acceptance remain pending; no GL runtime claim here.
- OpenGL work: if a native GL port is requested, implement the same bounded
  authored layer contract and equivalent depth policy (for example controlled
  polygon offset), including reset around ordinary/backdrop/auxiliary draws,
  colour/ID parity and transactional resources. Run actual pixel/occlusion/order/
  retirement and source-city cases; Vulkan bias constants do not prove GL parity.
  Shared CPU classification/validation requires no separate algorithm. Existing
  GL backend is preserved; port status pending, unscheduled and non-blocking.

## VK018 - Native large-city matrix precision (2026-10-07)

- Behaviour/files: private fork `PsyX_NativeTransform.h` composes projection with
  view/model using double CPU intermediates, narrowing only the shader matrix;
  `PsyX_VkNative.inl` uses it for world and translation-free backdrop. Public ABI,
  float vertex/shader data, resource ownership and synchronization do not change.
  `PsyX_VkNativeTests.inl` adds large-origin near-face/order/pick/retirement tests;
  game `NativeSceneTests.cpp` adds actual Chicago shared-world coordinate checks.
- Actual validation:12 coordinates fail old composition then pass corrected;
  scene22/remaining CPU/host suites, asset1536 and VS build PASS. Native GPU
  local/large origins, nearer face, both draw orders, ID602 and retirement PASS;
  full native and fresh PSX GPU reports PASS/exit0, Khronos loaded/no VUID.
  RenderDoc1.46 normal/HD and wide/original maintain80 backdrop/876 world/10 UI,
  no messages and zero wrong paint IDs/45000 and35470 interiors. Captured grass
  plane-slope spread8.01e-8 after versus1.05e-5 before. Source-overlap z-fighting
  still blocks R03; this change does not establish visual seam acceptance.
  Bound buffer/all-mip image hashes match before/after at both actual city settings.
- OpenGL work: no existing GL path changes. A future native GL renderer should
  reuse the CPU composition and independently validate large-origin pixels,
  depth, IDs, backdrop and ownership; GL projection/depth conventions still need
  their own tests. No separate classification algorithm or GPU float64 capability
  is needed. Port status pending, unscheduled and non-blocking; see
  [native evidence](../../rendering/r03-terrain-validation.md).

## VK019 - Canonical native horizontal depth (2026-10-07)

- Behaviour: exact copied horizontal source planes derive one world-height depth
  equation from a double frame inverse; matching planes use identical fragment
  depth and explicit last-submitted native tie ownership. Authored paint retains
  bounded D32 layer bias, with a zero-depth guard. Other/pitched/edge-on geometry
  uses ordinary native raster depth. Invalid inverses/coefficients fail explicitly;
  alpha, physical nearer occlusion, IDs and fence retirement remain in the native
  path. No source vertex/artwork rewrite, optional feature or new GPU payload.
- Files: fork `PsyX_NativePlane.h`, `PsyX_NativeScene.*`, `PsyX_VkNative.inl`,
  `PsyX_VkNativeTests.inl`, native vertex/fragment GLSL/SPIR-V and generated shader
  header; game `NativeSceneTests.cpp`, plan/stage/product/evidence/rule/status/
  handoff/change/changelog. Private 112-byte push record/offset96 checked; native
  depth-attachment dependencies now cover early and late fragment tests.
  `PsyX_native.h` clarifies the finite/invertible view contract without an ABI
  layout change. Shader compilation and final native/PSX GPU PASS/exit0, Khronos
  loaded/no VUID; final city colour/ID/depth exactly match tested canonical images.
  Fork2341108 pushed/gitlink staged; five INIs/`.user` hash-restored, all10 VS pairs
  preserved/debugger stopped. Existing GL code and auxiliary consumers unchanged.
- Actual validation: CPU15 plane checks plus existing scene/host suites,
  asset1536 and VS Release_dev|x64 PASS. GPU11685 interior pixels per order,
  cutout paint/hole, nearer floor, IDs and zero-owned retirement PASS; native
  loading/failure/fence-recovery regressions remain tested. RenderDoc1.46 normal/
  wide masks compare39558/71120 visible overlaps: wrong IDs17677/34474 ->0;
  paint45191/35601 also0.80 backdrop/876 world/10 UI/no messages. Bound buffers/
  every artwork mip exact before/after; geometry148320 bytes, HD61/RGBA49926792
  versus original62/2174132. Full R03/source UV-boundary/region coverage remains
  pending. See [terrain evidence](../../rendering/r03-terrain-validation.md).
- OpenGL work: existing GL is preserved. If a native GL port is requested, reuse
  copied plane classification/inversion with GL projection/window-depth conventions,
  implement corresponding fragment depth/layer/tie ownership and synchronization,
  then independently prove alpha/occlusion/IDs/resources and original-city pixels.
  Vulkan equations/bias do not prove GL parity. No separate CPU classification
  algorithm is needed. Port pending, unscheduled and non-blocking.

## VK020 - RenderDoc1.41 native capture tooling (2026-10-07)

- Files/behaviour: `scripts/inspect_native_capture.py`, active plan/validation/
  tool evidence/index, tooling rule, status/handoff/changelog. Configured rdc MCP
  captures/replays1.41; inspector checks version, native passes/resources/pixels
  and preserves initial/per-event diagnostics rather than silently clearing them.
  Runtime shaders/backend/synchronization and game assets remain unchanged.
- Actual validation: fresh normal/HD and wide/original1.41 captures/replays,
  80 backdrop/876 world/10 UI; geometry148320 bytes, RGBA49926792/2174132.
  Colour/ID/depth byte equality and ordered bound buffer/all-mip hashes match
  historical final1.46 evidence. Five HIGH initial barrier warnings retained,
  `validationClean=false`; origin not fully isolated, full R03 still incomplete.
  Fresh five INIs hash-restored; see [evidence](../../rendering/renderdoc-141-validation.md).
- OpenGL port: not applicable. This is Vulkan capture/replay inspection tooling;
  it adds no rendering contract to port and makes no OpenGL runtime claim.
  Existing OpenGL backend and deferred native port items stay unchanged.

## VK021 - Original tree capture policy and pixel diagnosis (2026-10-07)

- Files/behaviour: `scripts/inspect_native_capture.py` adds two-sided world alpha/
  depth/pick inventory; rendering tree evidence/stage/plan/product/index and
  status/handoff/changelog updated. No runtime C++/shader/backend/asset changes.
- Actual validation: fresh RenderDoc1.41 normal/wide inspector runs find44 tree
  parts with cutoff0.5/depth enabled, unchanged resources/pixels and five retained
  HIGH warnings. Independent three-tree footprint hole/ID/depth/occlusion masks
  pass; colour oracle exits1 (normal5/30458 failures, wide0/10140). LOD0 matches
  original sample; FP32-fetch probe approaches ideal. Replacements restored/freed.
  See [evidence](../../rendering/r03-cutout-validation.md); R03 remains incomplete.
- OpenGL port: not applicable to capture inspection/diagnostic probes; no runtime
  rendering contract was added. Existing OpenGL backend remains preserved;
  no GL pixel acceptance or native port implementation is claimed.

## VK022 - Sampling precision capability diagnostics (2026-10-07)

- Files/behaviour: `scripts/query_vulkan_baseline.cpp/.ps1` add actual texel/mip/
  raster fractional precision fields and CLI output, preserving existing schema1
  fields; tree evidence/product/plan/status/handoff/changelog updated. Runtime
  shaders, resource formats, backend selection and assets unchanged.
- Actual validation: two compiled capability-probe runs exit0; RTX3060Ti reports
  eight bits for all three limits, loader log empty. RenderDoc1.41 traces match
  all five original shader outputs to captured pixels. Replay texel-centre
  sampling matches four explicit fetches/FP32 mix at276,472; shader restored/freed.
  Strict colour oracle still exits1; CTS diagnostic allowance is not acceptance.
  See [evidence](../../rendering/r03-cutout-validation.md); R03 remains incomplete.
- OpenGL port: not applicable to Vulkan device capability queries and private
  replay diagnosis. A requested native GL port needs its own actual precision
  diagnostics/accuracy evidence; existing OpenGL backend remains preserved.

## VK023 - Direct native fractional-filter characterization (2026-10-07)

- Files/behaviour: fork `PsyX_VkNativeTests.inl` adds constant-UV 2x2 patterns,
  nearest/linear/trilinear readbacks, ideal-colour diagnostics and uniformity/
  resource/retirement gates; parent `DeveloperVkFixture.cpp` expands the report
  buffer. Rendering plan/gates/product/evidence/status/handoff/changelog and
  `knowledge/rules/native-sampling-validation.md`/rule index updated.
  Production shaders, formats, backend selection and synchronization unchanged.
- Actual validation: VS Release_dev x64 PASS; native/PSX GPU PASS/exit0/Khronos
  enabled/no VUID. Fifteen uniform masks, nearest/filter consistency and216
  geometry/16 or20 image bytes plus final zero ownership PASS. RenderDoc1.41 MCP
  confirms ten native/capture RGB comparisons exact; strict ideal deviations
  remain unaccepted. First fixture assertion corrected to base-only ownership.
  Fork7616e0d pushed/gitlink staged; six test files restored with exact hashes.
  See [evidence](../../rendering/r03-cutout-validation.md); R03 incomplete.
- OpenGL port: not applicable to this Vulkan-only native diagnostic fixture.
  A requested native GL implementation must validate its own actual sampler
  precision and lifetime; no shared API change or existing GL behaviour changed.
  Deferred native compatibility work remains pending, without blocking Vulkan.

## VK024 - Native FP32 filtered artwork precision (2026-10-07)

- Files/behaviour: fork `PsyX_VkNative.inl`, native vertex/fragment GLSL, generated
  SPIR-V/`PsyX_Vk_Shaders.h` and `PsyX_VkNativeTests.inl`. Private push padding92
  carries sampling mode; size112/depth offset96/public ABI9 retained. Bilinear
  mixes four decoded sRGB texels and linear alpha in FP32; trilinear mixes two
  bounded levels using selected implicit LOD. Nearest/base-only semantics,
  formats, ownership/fences/retirement/backend selection are preserved. Extreme
  UVs clamp before integer conversion; invalid UVs discard. Filter cost is up to
  eight texel fetches; performance acceptance remains pending. Plan/stage/product,
  evidence/status/handoff/changelog, sampling/mip rules and change record updated.
- Actual validation: shader compilation/VS Release_dev x64 and CPU scene/asset1536
  PASS. Native/PSX GPU PASS/exit0/Khronos/no VUID;15 fractional cases pass strict
  two-byte accuracy, six mip/clamp checks exact, final ownership zero. Fresh
  RenderDoc1.41 normal/wide40598 colour samples have zero errors; source-alpha/
  ID/depth/occlusion and existing terrain interiors PASS. Bound buffers/all mips
  exact. Five HIGH initial replay warnings remain; timing experiment inconclusive.
  Fork426c9a3 pushed/gitlink staged; six files hash-restored. R03 incomplete;
  see [evidence](../../rendering/r03-cutout-validation.md).
- OpenGL port: deferred native implementation must reproduce decoded-texel FP32
  interpolation, draw sampling choice, base-only modes and bounded implicit LOD/
  fetches, with its own GPU accuracy/alpha/retirement and cost evidence. Vulkan
  private push layout is backend-specific. Existing OpenGL remains preserved;
  this work neither removes it nor makes it native-scene recovery.

## VK025 - Native tree view and boundary evidence (2026-10-07)

- Files/behaviour: rendering plan/stage, tree evidence/new view evidence,
  native-static-region product, current status/recent context, sampling rule and
  changelog. Production source/shaders/interfaces/resources unchanged at426c9a3.
- Actual validation: three actual1.41 heading captures plus tightened oracle on
  five views PASS:1150047 eligible colours,35929 neighbour-band comparisons,
  no hole/ID/depth/occlusion errors. All44 parts have footprint evidence,39 opaque
  colour evidence. Buffer/all-mip hashes/bindings/formats/counts/policy match;
  replay-local EIDs excluded only from policy comparison after first audit failed.
  Four/five initial HIGH replay barriers retained, GPU cost unaccepted. Five
  runtime INIs hash-restored, targets stopped/session closed. No build or self-test
  rerun claimed. See [evidence](../../rendering/r03-tree-view-validation.md).
- OpenGL port: not applicable to this Vulkan capture/evidence milestone; no new
  runtime contract. A future native GL port needs independent view/silhouette/
  resource evidence. Existing OpenGL remains preserved; R03 incomplete.

## VK026 - Native tree exposure and terrain findings (2026-10-08)

- Files/behaviour: rendering plan/stage, tree evidence, native-static-region
  product, current status/recent context, sampling rule, change record/changelog.
  Production source/shaders/interfaces/resources unchanged at426c9a3.
- Actual validation: six new configured RenderDoc1.41 frame500 captures; both
  independent oracles exit0 per view. Eleven-view tightened total3079701 colours/
  87152 neighbour-band comparisons, no scoped hole/ID/depth/occlusion errors.
  All44 original-art parts have opaque colour,36 HD; part751 only35 pixels.
  Resource record multisets and ordered bindings match after excluding local
  ResourceId keys only; initial raw audit failure retained. Four HIGH initial
  warnings per new replay remain. All six PNGs inspected; eastnorth terrain
  stippling and west camera/ground coverage are unaccepted findings. Six settings
  files restored byte/hash-exactly, all10 VS pairs/Design; captures/replays closed.
  No build/self-test rerun claimed. [Evidence](../../rendering/r03-tree-view-validation.md).
- Additional diagnosis: four1.41 histories show grass569/sidewalk681, including
  a one-D32-unit failed sidewalk pixel beside a passing tie. Mixed-height681
  disables canonical depth. Source-transform confirmation/fix remain pending.
- OpenGL port: not applicable to this capture/evidence milestone; no new runtime
  contract. Future native GL validation needs independent silhouette/terrain/
  resource oracles. Existing OpenGL is preserved; R03 incomplete, R04 unstarted.

## VK027 - Mixed-height native indexed depth ranges (2026-10-08)

- Files/behaviour: PsyCross `PsyX_NativeScene.{h,cpp}`, `PsyX_VkNative.inl`,
  `PsyX_VkNativeTests.inl`; parent `NativeSceneTests.cpp`, native capture inspector,
  rendering plan/stage/evidence/index, native-static-region product, depth/tool
  rules, status/handoff, change record/changelog. Exact horizontal indexed runs
  within mixed top/side parts receive canonical world-height depth; nonhorizontal
  runs keep raster depth. Preserve consecutive triangle order/source buffers/
  materials/logical ID; backdrop keeps one draw. Physical ranges cap4096 per mesh/
  frame with separate32MiB CPU metadata ownership; atomic rejection and existing
  mesh fence retirement remain explicit. Public ABI9/push/shaders unchanged.
- Actual validation: CPU17 new range/source/budget/publication/fence checks and
  remaining native host suites PASS, asset1536/VS Release_dev x64/native/PSX GPU
  PASS. Mixed fixture independently requires three draws for two logical meshes;
  11685 top pixels/order plus holes/nearer occlusion/IDs/retirement pass. Initial
  old draw-count test rejection retained, corrected before passing rerun.
  Two actual configured1.41 frame500 normal/HD and wide/original captures pass
  source-world height17, overlap11456/2639 with wrong4462/2111 ->0, unchanged D32
  bits and56030 source-colour/542851 tree regression comparisons. Every logical
  part's source bindings/content and exact index partition match;876 parts now
  1023 physical world draws, unchanged geometry148320/artwork ownership.
  Four HIGH replay warnings and a Pixel History/raw-attachment discrepancy are
  retained; no clean replay/cost/full-region claim. Six settings files restored,
  all10 VS pairs/Design/startup/zero breakpoints verified. Forkae83dd2 pushed;
  parent gitlink staged. [Evidence](../../rendering/r03-mixed-height-validation.md).
  Subsequent read-only west matrix/ray inference estimates an above-ground camera
  and729600 lower pixels ID0/D32=1, almost all hypothetical ground rays inside the
  slice. Direct frame1/262 cameras are above ground but differ from frame500;
  144 source cells/628 ordinary references/530 slots match530 copies. Old west
  sampled cell is empty/resident; proposed[-7000,-229300] lies inside model54/
  part309 road. Read-only1.41 export/footprint checks retain billboard/ray limits.
  Replacement view pixels/resources and captured-camera parity remain pending.
  No production modification or OpenGL port applies to this source diagnosis.
- OpenGL port: a future native backend must preserve ordered indexed ranges,
  equal-height depth equations, logical picking and identical bounds/lifetimes;
  use GL indexed offsets and per-range state with its own pixel/fence evidence.
  Current GL backend selection is preserved; native GL implementation remains
  deferred. No legacy scene recovery is introduced. R03 remains incomplete.

## VK028 - Direct RenderDoc1.41 validation and west road evidence (2026-10-08)

- Files/behaviour: `scripts/run_renderdoc_analysis.py`, rendering direct workflow/
  west evidence/index/plan/stage, native-static-region product, tooling rule,
  status/handoff and changelog. Trusted local scripts run configured1.41 SDK
  directly with explicit GPU/status checks and ordered exception-safe shutdown;
  no daemon30s response limit. Direct MSBuild/capture are user-authorized. No
  native production code, shader, shared interface or resource contract changes.
- Actual validation: direct Release_dev/x64 rebuild48.48s/128 warnings/0 errors;
  fresh native GPU log PASS. Two west1.41 source-road captures pass1217385 scoped
  colour/ID/depth and4616 occlusion samples, ten source/MVP camera checks,
  exact buffers/mips/ranges and729600 lower native pixels/view. Direct analyses
  32/33s exit0; fresh rebuilt direct capture/report exactly equals MCP, normal
  810425 pixel rerun PASS. Invalid GPU and deliberate script failure both exit1
  without success receipts; subsequent replay/argument isolation PASS. Python
  syntax PASS. All six settings/XML files hash-restored, owned targets closed.
  Auxiliary14/11 versus expected10 and wide sky/negative-w geometry remain open;
  four HIGH replay warnings retained. [Evidence](../../rendering/r03-west-road-validation.md).
- OpenGL port/status: not applicable to this Vulkan capture/evidence/tooling
  milestone; no runtime contract to port. Future native GL needs independent
  source/pixel/resource evidence and its own capture/replay setup. Existing GL
  backend and deferred backlog are preserved. No legacy recovery is added;
  whole west/R03 acceptance remains pending, R04 unstarted.

## VK029 - Source-clipped west native backdrop validation (2026-10-08)

- Files/behaviour: west validation/plan/stage/index, native-static-region product,
  source-clip/backdrop rule and rule index, status/handoff, change record/changelog.
  Private ignored1.41 export/clip/artwork/auxiliary-prefix oracles add evidence;
  production source, shaders, interfaces/resources and OpenGL are unchanged.
- Actual validation: same normal/wide captures replay via local configured1.41
  SDK,80 backdrop draws/160 source triangles. Independent six-plane source/MVP
  clipping and strict two-byte colour oracle pass3861031 colours/795828 visible
  sky samples, no visible sky without source coverage; D32=1/IDs0 throughout.
  Wide cap1x1 RGB[140,165,206],123992 samples/106253 visible, matches original sky
  bindings. One pixel blue+5 in a separate copied export is rejected. Initial
  matrix-layout oracle failure is retained; correct rows match post-VS within
  0.00003993. Auxiliary14/11 =4/1 fully clipped PGXP draws/51 vertices/17 triangles
  plus ten screen-space draws/342 vertices; prefix colour unchanged. Exact game
  producer/batching cause and206 excluded visible-sky edge pixels remain pending.
  No new game run/build/direct GPU fixture or clean replay claim. Four prior HIGH
  warnings remain retained. [Evidence](../../rendering/r03-west-road-validation.md).
- OpenGL port/status: not applicable to this source-backed Vulkan evidence
  milestone; no runtime contract change. Future native GL must use its own clip
  conventions and independent source/pixel/resource oracles. Existing backend
  selection and deferred port tasks remain preserved. Full west/R03 is incomplete.

## VK030 - Vulkan fixture process exit propagation (2026-10-08)

- Files/behaviour: `DeveloperVkFixture.h/.cpp`, `redriver2_psxpc.cpp`, direct
  workflow/tooling rule, R03 exit validation/stage/plan/product, status/handoff,
  change record and changelog. Explicit result enum propagates existing fixture
  failure as process exit1; unrelated arguments continue ordinary startup.
  GPU rendering, ownership, synchronization and backend selection are unchanged.
- Actual validation: direct Release_dev/x64 build exit0; real missing driver
  native exit1 with fresh diagnostic; restored driver native24.83s/PSX5.30s
  exit0/PASS. PSX/Android/Emscripten excluded host branches compile. Fresh1.41
  Vulkan frame500/local SDK inspection14.88s and offline check prove768299 native
  pixels at1280x720 and unchanged geometry/all-mip payloads/876 ordered bindings.
  Four HIGH warnings and auxiliary count gate remain failed. Six settings hashes
  match; owned target closed. [Evidence](../../rendering/r03-fixture-exit-validation.md).
- OpenGL port/status: no native GL implementation or backend removal. The shared
  desktop entry point preserves OpenGL selection; no renderer contract to port.
  An OpenGL runtime/build was not executed. Full R03 acceptance remains pending.

## VK031 - West auxiliary producer attribution (2026-10-08)

- Files/behaviour: R03 auxiliary evidence/west/stage/plan/index/product,
  clip/backdrop rule, status/handoff, change record/changelog. A temporary removed
  `PsyX_GPU.cpp` inspector-identity probe produces private exact VBO/source joins;
  no shader, scene, resource, synchronization or backend contract is shipped.
- Actual validation: two new configured1.41 direct frame500 captures and SDK
  inspection/export. All51 prefix vertices match unique producer identities in
  both:12 wheel3/39 body of Car#11/model1/high/palette3. Actual HD wheel RGBA versus
  VRAM body/whole wide prefix explains4/1 batches. All17 triangles outside left
  clip plane; shared colour unchanged. Native colour/ID/D32 hashes match their
  uninstrumented captures exactly. One changed RGBA byte is rejected. Probe
  source byte-restored and final build recompiles it; six settings hashes match,
  fork clean. Four HIGH warnings/count gate remain failed; no new GPU fixture
  suite or full R03 acceptance. [Evidence](../../rendering/r03-auxiliary-producer-validation.md).
- OpenGL port/status: no runtime change to port. Existing OpenGL and producer
  code stay intact until R06 dynamic/R10 screen-space migration. Future native GL
  requires independent clipping/source/pixel/resource evidence; this Vulkan
  attribution does not validate GL behaviour or eliminate migration debt.

## VK032 - Independent capture count scopes (2026-10-08)

- Files/behaviour: `scripts/inspect_native_capture.py`, its new host contract
  tests, count evidence/change record, active plan/stage/index/shared gates/clip rule,
  west/auxiliary/product records, status/handoff and changelog. Static inventory/
  complete range partitions now have a separate result from the unchanged ten-
  draw auxiliary baseline; the existing combined field remains unchanged.
- Actual validation: three host tests/eight scenarios and Python syntax PASS.
  Two real configured1.41 SDK replays14.136s/16.091s preserve every prior report
  field, native attachment/resource hashes, bindings and four HIGH messages.
  Static matches; auxiliary baseline/combined/clean-validation remain false.
  No runtime/build/game/settings change. [Evidence](../../rendering/r03-count-contract-validation.md).
- OpenGL port/status: no runtime API, shader, resource or backend change to port.
  This capture inspector is Vulkan-specific; future native GL requires its own
  inventory, migration and pixel/resource diagnostics. Existing GL is preserved,
  R06/R10 debt stays pending and no full R03 acceptance is claimed.

## VK033 - Clipped static surface source oracle (2026-10-08)

- Files/behaviour: private ignored source clipping/export/pixel/negative-control
  helpers; R03 clipped evidence/change record, west/stage/plan/index/product,
  status/handoff and changelog. Negative-w part605 is clipped before perspective
  sampling, with captured CCW/back culling and independent source depth.
- Actual validation: existing configured1.41 normal/wide SDK exports0.153s/0.110s;
  212270 source-colour/ID/D32 and117029 outside-footprint crop checks PASS.
  One copied colour byte is rejected; original export hashes stay unchanged.
  125 raster/depth-uncertain pixels and vertical ranges remain unaccepted.
  Four HIGH warnings/failed auxiliary baseline persist. No new game/build/GPU
  fixture or production source/settings change. [Evidence](../../rendering/r03-clipped-static-validation.md).
- OpenGL port/status: no runtime API/shader/resource/backend change to port.
  Future native GL needs its own clipping, winding, depth and source-pixel proof;
  Vulkan capture evidence does not validate GL. Existing OpenGL is preserved.

## VK034 - Visible static vertical face and culling checks (2026-10-08)

- Files/behaviour: ignored side/facade source/pixel/control helpers; new vertical
  evidence/change record, clip rule, stage/plan/index/product/clipped evidence,
  status/handoff and changelog. Prove expected605 back culling and independent
  visible762 raster-depth source shading; per-component FP32 arithmetic bound
  preserves strict pixel gates. No production resource/synchronization change.
- Actual validation: four1.41 SDK phases0.331/0.353/0.202/0.158s;175832 visible
  colour/ID/D32 pixels,29158 facade outside-footprint and151073 culled-side crop
  pixels PASS. Five copied mutations rejected, originals/source buffer/full-mip
  hashes intact.78 facade raster pixels/previous exclusions/other faces/cost stay
  pending; warnings retained. No build/game/GPU fixture. [Evidence](../../rendering/r03-vertical-static-validation.md).
- OpenGL port/status: validation tooling/docs have no runtime change to port.
  Preserve GL; a future native port requires independent winding, clipping,
  raster-depth/FP32 and source-pixel/resource proof. No GL validation claimed.

## VK035 - Multi-face source occlusion and surface coverage (2026-10-08)

- Files/behaviour: private ignored batch export/index/independent pixel/control
  helpers; vertical evidence/change record, clip rule, stage/plan/product,
  status/handoff and changelog. Source oracle now chooses nearest opaque face
  inside each draw and propagates the unchanged uncertain foreground-edge band.
  No production shader/resource/synchronization/backend change.
- Actual validation: four configured1.41 SDK phases1.545/1.438/0.180/0.174s;
  ten additional parts/two views pass709948 colour/ID/D32,12136 occlusion and
  155912 outside-footprint crop samples. Twenty source buffer/mip/slice bindings
  match. Triangle-order reversal identical; matrix/depth mutations rejected.
  Initial/intermediate failed815 receipts retained;504 raster/65 ambiguous pixels
  stay pending. No build/game/GPU fixture/settings change; warnings retained.
  [Evidence](../../rendering/r03-vertical-static-validation.md#additional-ten-part-coverage-2026-10-08).
- OpenGL port/status: private validation/docs require no runtime port. A future
  native GL oracle must model its own clipping/winding/depth/within-draw occlusion;
  Vulkan proof does not validate GL. Existing OpenGL remains intact.

## VK036 - Captured warning provenance and direct native validation (2026-10-08)

- Files/behaviour: ignored structured/message/control helpers and fixture
  receipts; warning evidence/change record, plan/stage/index/product, status/
  handoff and changelog. Separate persisted capture diagnostics from subsequent
  replay operations; no message filtering or production synchronization change.
- Actual validation: two final1.41 SDK phases0.015s/0.010s; four exact stored
  warnings per capture, five image/no buffer barriers, two missing-warning
  controls rejected. Official1.41 source matches internal HOST_READ/ALL_COMMANDS;
  exact injected stack uncollected. Direct native GPU exit0/PASS24.931s, fresh
  16922-byte report, validation enabled/no recorded VUID. No build/new capture/
  pixel oracle or PSX rerun. [Evidence](../../rendering/r03-capture-warning-provenance.md).
- OpenGL port/status: no API/shader/resource/backend change to port. GL remains
  intact; Vulkan tooling attribution does not establish GL validation or remove
  R06/R10 consumer debt. Full R03/region/edge/cost gates remain pending.

## VK037 - Complete facade edge oracle and preserved broader failure (2026-10-08)

- Files/behaviour: private raster/source-pixel/shared-edge/mutation/order helpers;
  vertical evidence/change record, plan/stage/product, status/handoff/changelog.
  Test actual source/MVP/post-VS footprint using a device-scoped1/256 grid and
  top-left rule; no production shader/resource/synchronization change.
- Actual validation:762 complete175892 colour/ID/D32 and29176 outside-footprint
  crop pixels PASS with no exclusions; all78 prior band pixels resolved. Four
  copied corruptions detected/shared-edge and order controls PASS; originals
  unchanged. Broader ten-part/two-view oracle exits1 at normal779 pixel1125,397;
  source/post snapped coordinates agree but native coverage is absent. Failure
  retained;66 broad depth exclusions remain. No build/game/capture/replay/GPU
  fixture this turn. [Evidence](../../rendering/r03-vertical-static-validation.md#complete-part762-raster-footprint-and-remaining-edge-failure-2026-10-08).
- OpenGL port/status: no runtime change to port; GL preserved. A future native
  GL raster oracle needs independent device precision/fill/viewport evidence;
  this two-frame Vulkan model does not establish universal raster behavior.

## VK038 - Verified FP32 viewport coverage model (2026-10-08)

- Files/behaviour: private viewport pixel/source-post/mutation/order helpers;
  vertical evidence/change record, source-validation rule, plan/stage/product,
  status/handoff/changelog. FP32 viewport arithmetic before fixed-grid coverage
  explains779's former failure. No production shader/resource/synchronization change.
- Actual validation:22 part/views pass886209 colour/ID/D32,12152 occlusion and
  185206 outside-footprint samples, zero raster exclusions;66 depth ambiguities
  retained. Source/post coverage898427 pixels identical. Three copied sky pixel
  corruptions rejected/order reversal identical; originals unchanged. No build/
  game/new capture/replay/GPU fixture. Failed FP64 receipts retained.
  [Evidence](../../rendering/r03-vertical-static-validation.md#fp32-viewport-model-resolves779-coverage-2026-10-08).
- OpenGL port/status: private validation/docs have no runtime port. Preserve GL;
  its future native coverage proof needs its own device/viewport/fill evidence.
  This scoped Vulkan result does not establish universal arithmetic or full R03.

## VK039 - Bounded source-depth decisions and ambiguous attachment integrity (2026-10-08)

- Files/behaviour: ignored depth inventory/bounded-pixel/control helpers;
  vertical evidence/change record, source-validation rule, plan/stage/product,
  status/handoff/changelog. Use existing5e-7 error gate as a source-depth interval;
  promote stored D32 to FP64 before adding bounds. Preserve unresolved ownership.
  No production shader/resource/synchronization/backend change.
- Actual validation:22 part/views pass886226 source-colour/ID/D32,12167 occlusion,
  185206 outside-footprint samples,0 raster exclusions.32 of66 depth exclusions
  resolved;34 source decisions remain ambiguous,20 native/14 preserved attachment
  integrity checks PASS. Eight copied corruptions fail/order reversal identical;
  original hashes unchanged. Initial FP32-threshold receipt retained. No build/
  game/new capture/replay/GPU fixture. [Evidence](../../rendering/r03-vertical-static-validation.md#bounded-depth-decision-and-retained-ownership-ambiguity-2026-10-08).
- OpenGL port/status: private validation/docs require no runtime port. Preserve GL;
  any future native GL decision oracle needs its own D32/raster arithmetic and
  comparison evidence. This does not validate GL or full R03/source ownership.

## VK040 - Source-bounded GPU raster-depth ownership reference (2026-10-08)

- Files/behaviour: private source-primitive/shader-trace/attachment/pixel/control
  helpers; raster-depth evidence/change record, source-validation rule, plan/
  stage/index/product/status/handoff/changelog. Supplement34 uncertain decisions
  with explicit primitive/reflection-mapped raster inputs and completed shader
  outputs; no production renderer/source/resource/synchronization change.
- Actual validation: configured1.41 SDK final phases0.858s/1.706s;7648 shader steps,
  matched buffers/state,20 native/14 preserved pixels and three exact D32 ties.
  22 part/views pass886246 colour/ID/D32/12181 occlusion/185206 outside-footprint
  samples,0 exclusions. Seven mutations/comparison controls reject; originals
  unchanged. Initial incorrect bias-disabled assertion fails; effective bias
  values are zero despite enable=true. Four warnings retained. No build/game/
  new capture/GPU fixture. [Evidence](../../rendering/r03-raster-depth-trace-validation.md).
- OpenGL port/status: private validation/docs need no runtime port. Preserve GL;
  future native proof requires its own fragment-depth/comparison/device reference.
  This does not validate GL, universal raster arithmetic or full R03 acceptance.

## VK041 - Complete clipped605 footprint and canonical-depth ties (2026-10-08)

- Files/behaviour: private expanded/variant/guard-band-style/viewport/trace/
  equation/pixel/control helpers; clipping evidence/change record, source rule,
  plan/stage/index/product/status/handoff/changelog. Model device-scoped depth
  clipping/viewport-domain coverage with separate FP32 multiply/add; no runtime
  shader/resource/synchronization/backend change.
- Actual validation: initial expansion exits1 at three edge pixels;24 source/
  post/clip/arithmetic variants identify exact footprints.22 vertical cases pass
  unchanged older-scope counts.74 coplanar ties/17168 shader steps, matched501/605
  equation/shader hashes, SDK10.177s/0.041s. Complete605212377 colour/ID/D32 and
  117047 outside-footprint samples PASS with0 exclusions; all125 prior exclusions
  resolved. Seven pixel corruptions/order reversal and four trace controls PASS;
  originals/failed receipts retained. Four warnings retained; no build/game/new
  capture/GPU fixture. [Evidence](../../rendering/r03-clipped-static-validation.md#complete605-footprint-and-canonical-ties-2026-10-08).
- OpenGL port/status: private validation/docs need no runtime port. Preserve GL;
  future native GL proof needs its own clip/viewport/raster precision and depth
  reference. Observed guard-band-style model does not prove internal driver bounds,
  universal arithmetic or full-region acceptance. Full R03 remains incomplete.

## VK042 - Complete tested backdrop pixels and direct replay checks (2026-10-08)

- Files/behaviour: private full-backdrop source/post/attachment/control/replay
  helpers; backdrop evidence/change record, source rule, direct-workflow/plan/
  stage/index/product/status/handoff/changelog. No runtime shader/backend/
  resource/synchronization change; preserve near/far clipping and viewport domain.
- Actual validation:3861830 colour/796034 visible sky/285370 outside-source
  preservation samples PASS,0 edge exclusions;206 previous visible sky edges
  resolved. Source/post masks agree; seven copied corruptions reject;404 raw
  hashes unchanged. Configured1.41 final SDK phases1.425s/1.357s match six
  attachments/28 materials/196 mips/80 states per view. Four HIGH messages retained;
  no clean flag waiver. No build/game/new capture/GPU fixture. Six settings/fork
  preserved. [Evidence](../../rendering/r03-full-backdrop-validation.md).
- OpenGL port/status: private validation/docs require no runtime port. Preserve GL;
  a future native GL oracle needs its own clipping/raster/filter/attachment proof.
  This does not establish universal primitive ownership, full region or GPU cost.
  R03 remains incomplete/R04 unstarted.

## VK043 - Complete-view geometry attribution and observed sky absences (2026-10-08)

- Files/behaviour: private complete-world export/geometry/source-alpha/discard-
  trace/attachment/control helpers; complete-view evidence/change record, source
  rule, direct-workflow/plan/stage/index/product/status/handoff/changelog. No
  production shader/backend/resource/synchronization change. Preserve source
  clipping/culling and use instruction mappings/effective LOD correctly.
- Actual validation:1023 ranges/876 parts/33 world materials per view, hashes/
  ordered partitions match.3351166 observed world pixels have projected source/
  post attribution,0 footprint differences.8919 observed sky absences explained;
  58 thresholds/72 traces/16048 steps, source sampled alpha/exact backdrop/ID/D32
  preservation PASS. Six resource/geometry and20 sky controls reject;522/528
  original hashes intact. Configured1.41 exports41.262s/32.849s, final traces
  2.609s/2.627s exit0 without RPC timeout. Four HIGH messages retained; initial
  UV/raw-LOD/helper failures recorded. No build/game/new capture/GPU fixture.
  [Evidence](../../rendering/r03-complete-view-coverage-validation.md).
- OpenGL port/status: private validation/docs need no runtime port. Preserve GL;
  a future native GL proof needs its own clip/raster/interpolation/alpha/attachment
  reference. This does not accept GL, independent expected nearest world owner,
  filled-hole detection, complete region or GPU cost. R03 remains incomplete.

## VK044 - Source-generated complete-scene composition with pending pixels (2026-10-08)

- Files/behaviour: private composition-state export, full-scene oracle and copied
  state/pixel controls; composition evidence/change record, source rule, direct
  workflow/plan/stage/index/product/status/handoff/changelog. No production
  shader/backend/resource/synchronization change. Expected owners are generated
  before reading final native IDs; source alpha/depth competition remains explicit.
- Actual validation: all1023 ranges/876 parts/33 materials per view; actual
  shader/constants/depth/blend/sample state match.4145830 decided pixels pass
  colour/ID/depth/sky;1370 decisions remain unaccepted, including277 provisional
  source/observed-ID differences.40 controls reject/14 original file hashes intact.
  Direct1.41 final SDK14.406s/13.768s and host42.656s/31.531s exit0 without RPC
  timeout; four HIGH diagnostics retained. Initial bookkeeping/schema mistakes
  corrected before final reruns. No build/game/new capture/native GPU fixture.
  [Evidence](../../rendering/r03-complete-composition-validation.md).
- OpenGL port/status: private validation/docs require no runtime port. Preserve GL;
  a future native GL implementation needs its own source-generated competition,
  interpolation/filter/depth/state references and corruption proof. This accepts
  neither GL nor full-frame/region/cost. Resolve pending Vulkan pixels first;
  R03 remains incomplete/R04 unstarted.

## VK045 - Complete two-view composition using all-candidate references (2026-10-08)

- Files/behaviour: private candidate enumeration/trace/raw-artwork/full-composition
  and reference/pixel-control helpers; complete composition evidence/change,
  prior composition follow-up/rule/direct workflow/plan/stage/index/product/
  status/handoff/changelog. No production shader/backend/resource/synchronization
  change. Every source-covered candidate competes before captured IDs are compared.
- Actual validation:1370 prior exclusions/277 provisional ID differences resolved
  by2873 traces/764548 steps.4147200 complete-view colour/ID/depth/sky pixels PASS,
  0 errors/exclusions;1365 traced solid winner D32 values bit-exact/five exact sky
  preserves.42 new controls reject/20 original hashes intact; prior40 controls
  retain their14 hashes. Actual1.41 SDK163.973s/84.143s exit0 without RPC timeout,
  four HIGH diagnostics retained. Initial bounds/schema/UV failures recorded;
  derived conservative FP32 compatibility intervals supplement strict artwork/
  pixel/depth gates, without an exact/universal interpolation claim. No build/
  game/new capture/native GPU fixture. [Evidence](../../rendering/r03-composition-trace-validation.md).
- OpenGL port/status: private validation/docs need no runtime port. Preserve GL;
  any future native GL proof needs its own source-selected coverage/competition,
  shader-input/output reference, raw sampling and pixel/control evidence. This
  does not accept GL, all original assets/region/other views/resources/GPU cost;
  R03 incomplete/R04 unstarted.

## Renderer execution pause checkpoint (2026-10-08)

- The user cancelled the goal and paused at incomplete R03; R04 unstarted.
  [Checkpoint](../../rendering/r03-pause-checkpoint.md), plan/stage/umbrella and
  CURRENT_STATUS/RECENT_CONTEXT record the stopping point and resumption steps.
- The last private original-LEV topology host audit exited0/0.879s and matched
  530 instances/876 parts/4814 triangles per existing normal/wide view. Its
  negative controls remain unrun; this is preliminary evidence, not acceptance.
  Raw captures/scripts/receipts remain ignored local artifacts; hashes recorded.
- No runtime/backend/shader/resource/synchronization change, build, game run,
  capture or GPU fixture at this pause. Documentation needs no OpenGL runtime
  port. Preserve OpenGL and all prior compatibility debt; native GL acceptance
  still requires its own behavior/pixel/resource evidence when requested.
