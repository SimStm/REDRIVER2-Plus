---
type: Catalog
title: Planned features
description: Active plans; implementation evidence is separate.
tags: [okf, roadmap, planned]
---

# Planned features

Plans describe intent, not available behaviour. Follow the
[roadmap lifecycle](../index.md) and verify source/product evidence.

## Rendering replacement

- [Complete native Vulkan graphics renderer](native-vulkan-renderer.md):
  the single adopted graphics replacement plan, detailed in
  [knowledge/rendering/](../../rendering/index.md). Stages R00-R12 are pending;
  next is R00. It incorporates original meshes, shadows, draw distance,
  scalable lights, GLB/glTF/PBR, AA, complete modes/UI and preserved simulation.
- [Deferred Vulkan to OpenGL compatibility](port-vulkan-to-opengl-compatibility.md):
  mandatory tracking for every Vulkan change, with no priority, execution order
  or deadline. It does not block Vulkan work.

Dedicated older graphics plans are archived and excluded from routine analysis.
Do not resume their historical priorities or infer new-stage completion from them.

## Independent tooling plans

- [Inspector navigation and independent previews](inspector-camera-preview.md):
  stable input modes and source/override previews. Integrate with native resources
  when needed; this is not a prerequisite for the graphics rewrite.
- [OpenDriver2Tools interoperability and legacy overrides](modding-toolchain-integration.md):
  source-format/tooling investigation and conversion compatibility. Original
  installation and existing PNG mods remain independent of external tools.

Implemented unrelated surfaces are listed in [done/](../done/index.md).
Request a bounded milestone, inspect current source/diffs, record actual checks
and leave incomplete features planned. Every finished implementation also updates
its product surface, CURRENT_STATUS, RECENT_CONTEXT and Unreleased changelog.
