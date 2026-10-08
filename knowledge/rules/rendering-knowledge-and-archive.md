---
type: Rule
title: Use the active rendering plan and exclude archived ideas
description: Keep one current renderer direction and consult historical graphics records only on explicit request.
tags: [okf, rendering, documentation, agents]
---

# Use the active rendering plan and exclude archived ideas

**When** analyzing or planning rendering work, **then** start at
[the rendering catalog](../rendering/index.md), its current-state note and
the adopted roadmap. Confirm implementation claims from the current source.

- Exclude `knowledge/outdated-ideas/**` from routine discovery, recursive
  searches, analysis, retrieval, and instructions. Use, for example,
  `rg --glob '!**/outdated-ideas/**' <pattern> knowledge`. `knowledge/.ignore`
  also excludes the archive from ordinary ripgrep searches without affecting Git.
- Read archived contents only when the user explicitly requests historical
  review. A link from an active document does not authorize following it.
  Archive manifest/path/hash/link checks are allowed when maintaining this archive.
- Archived rules, priorities, prerequisites and completion claims are historical;
  they do not govern the native renderer or establish current behaviour.
- The full native Vulkan renderer is the adopted destination. The old hybrid
  projection/depth rules apply only when repairing that existing legacy bridge.
- Preserve original files, mods, game simulation and useful non-graphics PsyCross
  services. Planning documents do not authorize executing every stage.
- Keep the mandatory Vulkan-to-OpenGL compatibility backlog active at its
  existing path, unscheduled and independent of Vulkan progress.
- Update the active rendering documents rather than creating competing plans.
  Material design updates belong there with evidence and a dated decision entry;
  the rendering directory is the user-requested exception to the usual
  `discussions/<subject>/` location. Follow the roadmap/product lifecycle.
