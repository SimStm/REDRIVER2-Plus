---
type: Rule
title: Record every Vulkan change for deferred OpenGL compatibility
description: Develop Vulkan first and keep an unprioritized OpenGL compatibility backlog.
tags: [okf, rules, rendering, vulkan, opengl, roadmap]
---

# Record every Vulkan change for deferred OpenGL compatibility

**When** a change modifies Vulkan behaviour, shaders, resource ownership,
synchronization, shared renderer interfaces, build configuration or tooling,
**then** add or amend an entry in
[`port-vulkan-to-opengl-compatibility.md`](../roadmap/planned/port-vulkan-to-opengl-compatibility.md)
in the same change, before reporting completion.

- Develop and validate Vulkan first. OpenGL implementation is deferred until
  the user requests it; preserve its existing path and shared platform guards.
- Each entry records a stable identifier, date, Vulkan behaviour and affected
  files or commit, actual validation, OpenGL impact and concrete port work,
  dependencies or risks, and port status.
- Record every Vulkan modification, including fixes and maintenance. If it is
  Vulkan-specific with no OpenGL counterpart, record it as not applicable with
  a reason. If parity already exists, identify the source and validation evidence.
- Keep pending port work distinct from Vulkan work that has actually completed.
  Never describe an intended Vulkan change as implemented.
- The backlog has no roadmap priority, execution order or deadline and does not
  block Vulkan delivery. Do not assign any without an explicit user decision.
- Update entries as behaviour evolves instead of duplicating the same port task.
  Do not close a port item without source evidence and actual validation.

Adopted by the user on 2026-10-06. The requested documentation checkpoint was
completed, and the user subsequently authorized Vulkan renderer continuation.
