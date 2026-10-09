---
type: Catalog
title: Planned features
description: Active engine migration and retained superseded references.
tags: [okf, roadmap, planned]
---

# Planned features

Plans describe intent, not available behavior. Follow the [lifecycle](../index.md)
and confirm implementation against source/product evidence.

## Active adopted umbrella

[Engine migration](engine-migration.md) is the single execution owner for
`feat/new-engine`: Diligent/DiligentFX, FMOD Core, converted original
content, project-owned fixed ticks/services and final PsyCross removal.
All required E00-E07 stages are planned; begin with E00/E01 when requested.
PopcornFX is optional final E08, only explicitly requested after E07; it is not
a prerequisite of full-game acceptance, PsyCross removal or migration completion.
Read the [stage catalog](../../engine-migration/index.md),
[issue briefs](../../engine-migration/issue-briefs.md) and
[validation policy](../../engine-migration/validation.md).

The user's five implementation issues map to services/ticks, Diligent/assets,
FMOD, full mechanics/presentation and final removal. FMOD Studio, RmlUi,
authoring editors and supplementary physics are captured as later extensions.

[Deferred OpenGL compatibility](port-vulkan-to-opengl-compatibility.md) remains
independent, unscheduled and does not block Vulkan development.

## Superseded execution references

These records are retained for source context, not a competing execution order
or instructions to extend the old rendering path:

- [Hybrid renderer modernization](renderer-modernization.md).
- [Old high-FPS timestep proposal](high-fps-timestep.md); E01 now owns ticks/replays.
- [Old tooling interoperability](modding-toolchain-integration.md) and
  [model export/import](model-export-import-roundtrip.md); E02 owns native conversion.
- [Old draw-distance proposal](draw-distance-streaming.md).
- [Legacy shadow casters](legacy-shadow-casters.md) and
  [legacy point-light receptivity](point-light-sources.md).
- [Old graphics-quality profiles](graphics-quality-profiles.md).

The independent original-native rework lives on
[`feat/vulkan-native-rework`](https://github.com/SimStm/REDRIVER2-Plus/tree/feat/vulkan-native-rework).
Its R03 acceptance is not a prerequisite here.

## Independent tooling and implemented surfaces

[Inspector navigation/previews](inspector-camera-preview.md) remains a future
tooling proposal; integrate it with the new resource boundary when commissioned.
Other implemented surfaces retain their product records in [done/](../done/index.md).
No plan becomes a completed feature until it has executed acceptance and a product
document. Update current status/handoff and changelog after each delivered block.
