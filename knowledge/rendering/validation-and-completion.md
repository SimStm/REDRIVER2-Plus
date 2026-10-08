---
type: Design
title: Rendering validation and completion gates
status: planned
created: 2026-10-06
tags: [okf, rendering, validation]
---

# Rendering validation and completion gates

## What complete native rendering means

On the declared supported Vulkan desktop targets, every currently functional
original graphical category and mode uses native Vulkan resources/passes: world,
dynamic objects, effects, UI/frontend and auxiliary targets. Native-mode PSX GPU
draw counters are zero. Original content formats remain readable and gameplay
behaviour is preserved. PsyCross input/audio/CD services may remain, and the
existing OpenGL/non-native platform selection is a separate compatibility decision.
Automatic old-scene recovery inside native mode is prohibited. This version aims
at 100% replacement; remove old dependencies only after their consumers migrate.

For the fixed R03 slice, report static logical inventory/index partitions and
historical auxiliary batching separately. Variable car/wheel/HD batches cannot
establish missing static geometry or completed migration. Retain failed baseline
checks and source/pixel/resource diagnostics; eventual zero PSX draws remain the
full native acceptance requirement. See [count evidence](r03-count-contract-validation.md).

Every active stage must prove explicit failure and expected loading separately:
missing/unsupported/stale/budget/upload errors cannot draw old world pixels or
composites. A valid native publication after loading/recovery must produce native
pixels and counters, with bounded resource ownership and real fence completion.
Preserved unmigrated frontend/HUD/effect/VRAM consumers are pending coverage,
never evidence of a successful native world. Earlier fallback checks are historical
and superseded by the execution policy in the active implementation plan.

This is not unlimited draw distance, every painted building becoming 3D,
thousands of simultaneous shadowed lights or support for every glTF extension.
Existing unsupported desktop FMV or platforms are recorded separately. No scope
is hidden behind a checkbox whose effective mode is disabled by filtering/mods.

## Coverage matrix to establish in R00

Execution follows the user-adopted 2026-10-08 order R00-R07 -> R10 -> R11 ->
R08 -> R09 -> R12. Later AA and modern-asset acceptance must regression-test
the effects/UI/multiple views and fixed-tick presentation accepted earlier.

Track source category, resource owner, colour draw path, caster/receiver policy,
native picking/export support, failure/loading/auxiliary counts, tested modes and evidence.
Cover roads/terrain/buildings/props; foliage/billboards/cutouts; transparent
surfaces/water; sky/horizon; cars/wheels/damage; pedestrians/animation;
smashables/bridges/event/mission objects; decals/particles/flares/debris;
mirrors/other secondary views; HUD/minimap/overhead map; frontend/loading/pause;
split-screen; replay/director and cutscenes. A translucent effect may deliberately
not cast mapped shadows; document that physical/material policy rather than
label it an unimplemented adapter.

Exercise Chicago, Havana, Las Vegas and Rio, representative available
day/night/weather/variant content, long fast routes, dense traffic/police,
on-foot play, crashes/damage and mission transitions. Include the user's wide-FOV
sky case, facade shadow cases, near/far boundary and off-camera sun blockers.

Absent categories need a source-backed not-applicable entry for the tested
slice, not invented geometry. R03 Chicago has no water objects in its copied
inventory; the user identifies apparent water there as sky/background over empty
space. Validate that backdrop and applicable transparency; water does not block
Chicago acceptance. Actual Rio water remains future city-specific coverage before
R12; absence in Chicago never proves Rio water correctness.

## Evidence for every rendering change

- Record parent/fork revisions and dirty-tree caveats, build configuration,
  target OS/GPU/driver, window resolution, camera/spawn/replay state, effective
  renderer/material/range/light/AA settings and mod order.
- Use Visual Studio MCP `Release_dev|x64` builds/debugging, the current configured
  Vulkan documentation tools, and RenderDoc capture/replay/pixel/mesh/pass inspection.
  GPUInfo returns lookup links, not verified hardware values. Check actual device
  features/formats and limits before exposing settings.
- Use RenderDoc **1.41** through the configured `rdc-cli`/`rdc-tools` for new
  captures and replay (user instruction, 2026-10-07). Verify the loaded module
  and capture tool separately; `Program Files` still contains1.46. Historical
  1.46 evidence retains its version. See [the executed transition](renderdoc-141-validation.md).
- Preserve initial and per-event diagnostics: `SetFrameEvent` can reset the
  message list. Explain capture/replay warnings; an empty final log alone does
  not prove validation cleanliness or visual correctness.
- Compare stable frames with matched state; moving traffic/animated textures need
  same-backend controls or deterministic fixtures. Record tolerances before using
  image metrics. Old parity results do not prove new lighting correctness.
- Separate sampling characterization from accuracy acceptance. Constant-UV native
  fixtures can isolate texture hardware and replay effects, but matching a capture
  does not justify relaxing an independent ideal-colour tolerance. Record failed
  comparisons and establish the applicable accuracy oracle/bounds before accepting
  that gate; a fixture's ownership/uniformity PASS covers only those invariants.
  Follow [the sampling validation rule](../rules/native-sampling-validation.md).
- Inspect source/native counts, colour/depth/normal/material diagnostics, shadow
  maps and receiver results. Use pixel history for facade/sky causes, not guesses.
- Test reload, unload, teleport, lost focus, resize, minimize/restore, malformed
  assets, capability/quality fallback, low memory and async cancellation as relevant.

## Performance and simulation gates

R00 sets numerical budgets on named hardware. Record native CPU/GPU frame-time
distributions (p50/p95/p99), I/O/decode/upload stalls, resident CPU/GPU and transient
memory, draw/material/instance counts, cluster/light overlap, cascade/shadow costs
and requested/effective visibility. Use Vulkan timestamps to isolate GPU work;
instrumented replay cost or a 30 Hz capped frame is not GPU throughput.

Compare game state at simulation ticks, not render frames. Preserve native NTSC
and PAL timing, replay format, mission timers, random/update order, traffic/AI,
collision and damage. Any render-side interpolation must leave authoritative
simulation unchanged. Record which deterministic comparisons actually exist;
do not imply a complete state-hash harness was run when it is only planned.

## Build/test discipline and final gate

Run `git diff --check`, appropriate generated builds and meaningful affected
tests. For catalog/inspector export changes run
`pwsh -NoProfile -File scripts/run_inspector_tests.ps1` in fresh test directories.
The current `-vkpsxtest` protects preserved PSX consumers; create native tests when
there is a concrete new invariant to verify. Preserve target guards and do not
claim Linux/macOS/web/Android native support from a Windows build.

Before R12 completion, every coverage/mode gate must have executed evidence,
budget compliance and zero old-scene recovery or unexplained coverage. Publish
`knowledge/product/native-vulkan-renderer.md`, then move the umbrella roadmap
to `done/` and update catalogs/status/handoff/changelog. Until then the roadmap
stays planned even when individual stages are implemented.

## Stage evidence template

Record: stage/slice; objective; source revisions/diff; effective settings;
hardware/tool versions; changed files; checks actually executed and real results;
visual coverage/limits; simulation checks; CPU/GPU/memory observations;
explicit failure/loading/native recovery/rollback; product document;
OpenGL backlog entry; next concrete action.
Use concise evidence summaries and durable fixture recipes, not long logs,
secrets, temp URLs or original copyrighted assets.
