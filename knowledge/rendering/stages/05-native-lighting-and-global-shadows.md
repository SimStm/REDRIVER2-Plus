---
type: ImplementationStage
title: R05 - Native sun, materials and global shadow coverage
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R05 - Native sun, materials and global shadow coverage

**Status:** pending. **Dependency:** R04 accepted for off-camera residency; reference lighting can be developed earlier on the R03 slice.

## Objective

Replace depth-derived legacy lighting/shadows with consistent native surface lighting and cascaded sun shadows.

## Steps

1. Implement linear-light material evaluation using source normals and explicit metallic/roughness defaults, sun and ambient/environment response. Define original prelit texture modulation to avoid double darkening.
2. Fit cascades to native camera receiver ranges; extend caster queries toward the sun and request off-camera blockers. Use the same meshes/transforms/deformation contract in colour and shadow passes.
3. Implement stable texel alignment, cascade transitions, filtering and measured slope/receiver bias. Cutout casters match their visible material silhouette.
4. Exclude sky/backdrop/UI explicitly. Expose cascade indices, shadow factor, normals, missing caster resources and actual shadow reach.
5. Exercise the reported facade/FOV/sun-angle cases; native shadows must not inherit legacy reconstructed-depth exclusions or near-only caster lists.

## Deliverables

Native static lighting and cascaded sun shadows over the validated visual range, with explicit material/coverage controls.

## Acceptance gate

- [ ] Static opaque and cutout geometry casts and receives across the accepted range; upstream off-camera buildings can shade visible roads.
- [ ] Sky/HUD remain unaffected; no false facade shadowing, unstable cascade seams or abrupt unexplained cutoff in acceptance scenes.
- [ ] Original texture mods and prelit artwork follow the declared appearance policy.
- [ ] Shadow range, map memory and cost are measured; dynamic coverage remains pending R06 rather than falsely claimed complete.

## Planned validation

VS build; RenderDoc light-view depth/colour/pixel history; high/low/reversed sun angles, widened FOV, near/far cascade boundaries, off-camera blockers, cutout trees and static routes; timestamp/resource measurements.

## Risks and fallback

Bias and cascade resolution trade acne against detached shadows. Document tolerances; a single large shadow map is not the solution.

Independent native lighting/shadow toggles; conservative validated cascade profile.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
