---
type: Roadmap
title: Deferred Vulkan to OpenGL compatibility tracking
status: planned
created: 2026-10-08
tags: [roadmap, vulkan, opengl, compatibility]
---

# Deferred Vulkan to OpenGL compatibility tracking

## Problem and intended behavior

Preserve the independent backend-support contract while Vulkan-first engine work
proceeds. This backlog has no priority, deadline or execution order that blocks
Vulkan work. [Engine migration](engine-migration.md) owns new runtime decisions;
old scene fallback is not backend selection.

## Scope, dependencies and acceptance

For each future Vulkan-affecting change, record files/interfaces/shaders/build
or tooling behavior, actual validation and required OpenGL adaptation, or explain
why no port applies. Diligent backend availability alone does not accept an
OpenGL build, material behavior or supported platform. Do not delete an existing
supported path before replacement or an explicit user scope decision.

## Validation plan and current entry

2026-10-08 planning change: engine migration adopts DiligentCore/DiligentFX with
Vulkan as the first rendering scope. Files: `AGENTS.md`, `knowledge/engine-migration/`
and `engine-migration.md`. No Vulkan source/interface/shader/build behavior changed;
therefore no code port applies. Existing separately selected OpenGL remains.
Future native OpenGL integration requires recorded target/material/resource tests.
No build, renderer or GPU validation was run for this documentation-only change.

The previous rework's detailed Vulkan/OpenGL entries remain preserved at this
same path on `feat/vulkan-native-rework`; do not copy their implementation claims
into the different baseline source on `feat/new-engine`.
