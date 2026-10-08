---
type: Catalog
title: Native rendering replacement
description: Single active plan for replacing the PSX graphics pipeline while preserving Driver 2.
created: 2026-10-06
tags: [okf, rendering, vulkan, architecture]
---

# Native rendering replacement

The adopted goal is to replace the game's entire graphics path with a native
Vulkan renderer: original cities and dynamic objects become persistent 3D
resources, with modern materials, lighting, shadows, distance and anti-aliasing.
Original content, file compatibility, mechanics, collision, physics, AI, missions
and replay behaviour remain Driver 2. Useful PsyCross platform services can stay.

**This is a plan, not an implemented renderer.** The 2026-10-06 task reorganized
documentation only. R00 baseline/diagnostics, R01 analytic bootstrap and R02
original-import slice are implemented and accepted on their tested Windows scope;
R03 is in progress and R04-R12 remain pending. Existing
experiments are baseline evidence, not completed stages of this new plan.

## Reading order

1. [Current state](current-state.md): what the current source actually does.
2. [Requirements and document review](requirements-and-document-review.md):
   consolidated user goals, archived scope and retained compatibility records.
3. [Architecture](architecture.md): responsibilities, data flow and key choices.
4. [Asset and file compatibility](asset-and-file-compatibility.md): original
   formats, mods and glTF/GLB share one native scene contract.
5. [Implementation plan](implementation-plan.md): R00-R07, then R10, R11, R08, R09 and R12; stage IDs remain stable.
6. [Validation and completion](validation-and-completion.md): evidence needed
   to claim complete native rendering.
7. [Decisions and glossary](decisions-and-glossary.md): plain explanations,
   rationale and unresolved choices.

The lifecycle owner is the
[native Vulkan renderer roadmap](../roadmap/planned/native-vulkan-renderer.md).
Finish each stage's acceptance checks before advancing.
The next stage is **R03: complete native static region**.
New capture/replay work uses [RenderDoc1.41 and the configured rdc tools](renderdoc-141-validation.md).
Latest [original tree pixel evidence](r03-cutout-validation.md) remains partial:
reproduced filtering errors pass the strict gate; full silhouettes stay pending.
[Mixed-height terrain evidence](r03-mixed-height-validation.md) accepts the scoped
grass/sidewalk top conflict. [West road evidence](r03-west-road-validation.md)
passes scoped road/backdrop pixels; blue cap is original single-texel artwork.
[Auxiliary producer join](r03-auxiliary-producer-validation.md) identifies Car#11
body/wheel commands and HD/VRAM batching; [count scopes](r03-count-contract-validation.md)
separate static inventory/auxiliary baseline. [Part605 clipped interiors](r03-clipped-static-validation.md)
pass source colour/ID/depth; vertical ranges, excluded pixels and full R03 remain open.
[Visible vertical part762](r03-vertical-static-validation.md) passes its declared
interior scope; part605 side culling is source-confirmed. Other surfaces/edges stay pending.
[Source-bounded fragment traces](r03-raster-depth-trace-validation.md) close34
remaining raster-depth decisions in the tested22-view scope, including exact ties;
other surfaces/region/cost remain pending.
[Complete605 footprint](r03-clipped-static-validation.md#complete605-footprint-and-canonical-ties-2026-10-08)
resolves125 prior exclusions, including74 exact canonical ties; device-scoped
clip/viewport evidence does not establish a universal raster model.
[Complete tested backdrop pixels](r03-full-backdrop-validation.md) resolve206
sky edge exclusions with full source/post coverage and mutation/replay evidence;
other surfaces/full region/silhouette/cost remain pending.
[Complete-view geometry/observed sky audit](r03-complete-view-coverage-validation.md)
covers every range and3351166 observed world pixels;8919 observed sky absences
are source-alpha explained. Complete source-selected visibility remains pending.
[Source-generated scene composition](r03-complete-composition-validation.md)
now decides4145830 pixels with0 errors and40 rejection controls;1370 decisions
remain unaccepted, including277 provisional source/observed-ID differences.
[All-candidate references](r03-composition-trace-validation.md) subsequently
resolve every exclusion:4147200 complete-view pixels PASS,0 errors/exclusions;
2873 traces/764548 steps and42 new rejection controls explain candidate winners.
Other-view/original-source/region/silhouette/resource/cost acceptance remains pending.
Routine tests use the [verified direct workflow](renderdoc-direct-workflow.md);
Visual Studio MCP remains useful for debugger-specific investigations.
[Warning provenance](r03-capture-warning-provenance.md) separates stored capture
instrumentation diagnostics from direct validation-enabled native GPU results;
all four HIGH messages remain retained, with no clean-replay exemption.
The [adopted execution policy](implementation-plan.md#adopted-execution-policy-2026-10-07)
prohibits automatic old-scene recovery, separates loading from errors, preserves
OpenGL independently and removes old dependencies only after consumer migration.

R00 evidence: [implemented diagnostics](../product/native-renderer-baseline.md),
[source inventory](r00-source-inventory.md), [native contracts](r00-native-contracts.md)
and [hardware/budgets/test states](r00-measured-baseline.md). The 2026-10-07 user
goal authorizes sequential implementation, with acceptance before each advance.
R01:[implemented bootstrap](../product/native-renderer-bootstrap.md),
[executed validation](r01-validation.md). Original categories are not migrated
by its analytic scene; the whole renderer remains planned.
R02:[implemented original imports](../product/native-original-assets.md),
[executed validation](r02-validation.md). Complete region/world coverage remains pending.

Old graphics documents were moved to `knowledge/outdated-ideas/` and are
excluded from normal analysis unless explicitly requested. Shared asset/mod/tool
contracts remain active. The
[OpenGL compatibility backlog](../roadmap/planned/port-vulkan-to-opengl-compatibility.md)
also remains active, with no priority, deadline or dependency on this sequence.
