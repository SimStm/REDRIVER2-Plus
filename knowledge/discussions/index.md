---
type: Catalog
title: Technical discussions
description: Evolving evidence, options, diagrams, and recommendations before roadmap adoption.
tags: [okf, discussions, architecture]
---

# Technical discussions

Discussions preserve reasoning while the direction is still being explored.
They are neither adopted roadmap commitments nor proof of implemented features.
Follow the [discussion maintenance rule](../rules/discussion-records.md).

## Subjects

- [Renderer modernization](renderer-modernization/index.md): PsyCross's role,
  modern scene submission, PBR materials, lighting, graphics backends, custom
  assets, and a staged sequence of experiments. **Roadmap adopted; not implemented.**
- [Playable testing playground](playable-testing-playground/index.md): a
  procedural or declarative driving test environment, world-data requirements,
  reusable city assets, and Take a Ride/debug/command-line entry points.
  **Implemented; roadmap item 13 complete.**
- [Source-aware asset catalog and material identity](asset-catalog-identity/index.md):
  the resource-versus-instance identity and lifetime specification behind
  roadmap item 04, including stable ids, manifest-compatible texture identity,
  explicit provenance, and many-to-many model/texture relationships.
  **Implemented; roadmap item 04 complete.**
- [Whole-object selection across renderer categories](inspector-selection-coverage/index.md):
  the coverage inventory behind roadmap item 06, the in-game reproduction of
  unlabelled car/pedestrian/building selections, and the debugger root cause
  (a late full-viewport unlabelled quad winning the depthless last-match pick).
  **Exploring; roadmap item 06 planned.**
- [SDL2 to SDL3 migration](sdl-version-migration/index.md): the size of the
  current SDL2 surface, what SDL3 changes, and a recommendation to defer the
  migration until after Vulkan parity. **Exploring; no decision adopted.**

## Structure and lifecycle

Use one `subject/index.md` per durable topic, with supporting notes and
`diagrams/` alongside it. Include context, current evidence, recommendations,
alternatives, decision status, open questions, and dated discussion history.
Record material updates in place; retain the rationale for superseded choices.

The lifecycle is discussion, adopted roadmap scope, implementation, then product
documentation and a completed roadmap record. A discussion can stay unresolved
or be closed without entering the roadmap. Retain it when a feature is adopted
and link both ways. Record only actual user decisions as adopted.
