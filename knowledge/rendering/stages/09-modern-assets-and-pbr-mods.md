---
type: ImplementationStage
title: R09 - Production GLB/glTF and modern PBR overrides
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R09 - Production GLB/glTF and modern PBR overrides

**Status:** pending. **Dependency:** R02 import contract, R06 dynamic mapping and R08 material/colour policy accepted.

Execution follows accepted R10/R11/R08 under the 2026-10-08 adopted order and
precedes R12. Modern replacements reuse the completed original-content native
scene; they must preserve effects/UI integration and interpolated pose/reset
contracts without changing authoritative simulation.

## Objective

Accept modern 3D content through the same native scene while keeping original installations and texture mods compatible.

## Steps

1. Deliver the declared glTF 2.0/GLB subset: multi-primitive meshes, node transforms, accessors/indices, required PBR textures, alpha modes and explicit required-extension rejection.
2. Implement stable original-ID visual replacement mapping with a versioned optional sidecar only if needed. Preserve existing PNG mod manifest identities/precedence and inspect original versus active resources.
3. Support or explicitly reject skin/animation/damage mappings according to the R06 contract. Imported mesh topology never replaces gameplay collision buffers.
4. Provide no-edit export/import comparison for supported source categories; retain useful OBJ/MTL interoperability and label unsupported data.
5. Reuse owned modern fixtures; use Meshy MCP only when additional test geometry/material cases are needed, recording provenance/hashes.

## Deliverables

Validated production modern asset import/replacement, PBR material bindings and safe source export/round-trip support for declared categories.

## Acceptance gate

- [ ] Original-only installations, existing texture mods and optional model replacements coexist without rewriting data files.
- [ ] Multiple materials/nodes and metal/rough/normal/emissive/alpha semantics match the supported contract.
- [ ] Malformed resources, unsupported required extensions and incompatible dynamic mappings fail clearly; any recovery uses validated native resources, never the old scene renderer.
- [ ] Round-trip counts, bounds, UV/material identity and component transforms meet declared tolerance; asset terms/provenance are retained.
- [ ] Original/replacement resources preserve accepted effects/UI/multiple-view behaviour and colour/shadow/picking pose agreement at R11 presentation rates.

## Planned validation

Importer/round-trip tests on owned/synthetic files, Khronos glTF validation when available, inspector/catalog tests for affected contracts, VS build and RenderDoc material/pose inspection under known lighting.

## Risks and fallback

glTF is broad and generated assets are not automatically runtime-ready. Never claim universal extension, rig or damage support.

Reject invalid overrides and keep the original native resource; original files stay read-only.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
