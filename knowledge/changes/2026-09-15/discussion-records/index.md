---
type: Change
title: Preserve evolving technical discussions
description: Add discussion records and maintenance guidance before roadmap adoption.
tags: [okf, documentation, discussions]
---

# Preserve evolving technical discussions

## Context

The user requested durable notes, image diagrams and continued updates while
exploring renderer modernization, explicitly before adding that work to the roadmap.

## Decision

Add `knowledge/discussions/<subject>/` with an indexed synthesis, supporting
notes, diagrams and dated history. Add a maintenance rule and an AGENTS.md
entry so future material discussions update the same topic automatically.
Clarify the boundary between exploration and adopted roadmap scope.

## Impact

The renderer-modernization record captures current source evidence, architectural
options, pending decisions and a recommended experiment sequence. The only
delivered changes here are documentation and its diagram generator/images;
no new rendering capability or roadmap feature is claimed.

Before the Unreleased documentation entry was added, the
[upstream releases page](https://github.com/OpenDriver2/REDRIVER2/releases)
was checked on 2026-09-15: it lists `8.0` at `b2d8857` as a pre-release;
`7.4-rc2` retains the Latest badge. The documented fork baseline remains `8.0`.

## Validation

- `git diff --check` passed for the documentation changes.
- Checked five new Markdown files and all 31 local links; no broken targets
  or trailing whitespace were found.
- Regenerated both PNGs with Python/Pillow and visually inspected their layout.
- No game build or runtime validation was performed for this documentation-only
  change. The renderer experiments remain unexecuted recommendations.
