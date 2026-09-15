---
type: Change
title: Adopt playground-first renderer planning and Meshy test assets
description: Link the two roadmaps and discussions with an explicit minimum playground handoff.
tags: [okf, roadmap, discussions, rendering, playground, meshy]
---

# Adopt playground-first renderer planning and Meshy test assets

## Context

The user requested a playable playground roadmap linked to its discussion and
the renderer roadmap, selecting the playground as the first implementation stage.
The renderer direction previously existed only as a discussion; the existing
graphics-quality plan explicitly excluded this broader modernization.

## Decision

Create planned records 13 (playground) and 14 (renderer modernization) in a
dedicated track. Playground P1-P4 provide the validated scene consumed by R2;
P5 adds Take a Ride. Initial playground rendering uses the existing backend,
avoiding a circular dependency. JSON/editor/cross-city authoring remain deferred.
Original-city regressions complement the controlled test scene.

The user additionally selected their implementation agent's configured Meshy
MCP for prompt-generated PBR assets. R3/R4 now require a retained, validated
generated fixture with provenance. The MCP configuration has not been verified
here and no generation job has been submitted.

## Impact and evidence

Updated both discussions, the technical sequence, catalog, diagram and changelog.
Earlier discussion history is retained with the superseding user decision.
No game capability is implemented or product document created by this change.

Before updating Unreleased, checked the
[upstream releases page](https://github.com/OpenDriver2/REDRIVER2/releases)
on 2026-09-15: the recorded `8.0` baseline remains `b2d8857`, listed as a
pre-release, with `7.4-rc2` retaining the Latest badge.

## Validation

- Checked nine documents, 88 local links and required sections of both new
  roadmaps; all passed.
- `git diff --check` passed for the documentation changes.
- Regenerated and visually inspected the updated sequence diagram.
- No game build, runtime experiment or Meshy generation was performed.
