---
type: Catalog
title: Project knowledge
description: Entry point for the OKF bundle — rules, product notes, and change records.
tags: [okf, catalog]
---

# Knowledge

This directory follows the [Open Knowledge Format](https://github.com/GoogleCloudPlatform/knowledge-catalog/blob/main/okf/SPEC.md) (markdown + YAML frontmatter). Agents and humans read it before changing conventions.

| Path | What belongs here |
| --- | --- |
| [`CURRENT_STATUS.md`](CURRENT_STATUS.md) | The cumulative source of truth for what has been implemented in this fork. Add a short factual entry when a change lands. |
| [`RECENT_CONTEXT.md`](RECENT_CONTEXT.md) | Session-to-session handoff: what the last interactions changed, the techniques and environment notes that mattered, and the next recommended action. |
| [`rules/`](rules/index.md) | Repeatable constraints (“when X, do Y”). If you discover a pattern that will come up again, add a rule here instead of leaving it in a PR comment. |
| [`product/`](product/index.md) | How a product surface works and what must be configured. |
| [`discussions/`](discussions/index.md) | Evolving technical discussions, evidence, alternatives, diagrams, and unresolved decisions before roadmap adoption. |
| [`roadmap/`](roadmap/index.md) | Planned-feature records and completed-feature history. These documents are planning metadata, not proof of the current implementation. |
| [`changes/YYYY-MM-DD/<name>/`](changes/2026-09-15/asset-catalog-identity/index.md) | Why a specific change landed, on the date it shipped. |

Cross-links between documents are encouraged. Do not put secrets here.

## Active direction on feat/new-engine

Start at [engine migration](roadmap/planned/engine-migration.md) and
[its stage/validation catalog](engine-migration/index.md). Diligent, FMOD Core,
converted original content and independent simulation ticks are planned;
PopcornFX is optional final E08, only explicitly requested after E07. Current
source still depends on PsyCross. Earlier hybrid graphics and
timing plans are retained as superseded references, not the execution sequence.
The separate native Vulkan rework is preserved on `feat/vulkan-native-rework`.
See [current branch status](CURRENT_STATUS.md) before making implementation claims.
