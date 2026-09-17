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
| [`rules/`](rules/index.md) | Repeatable constraints (“when X, do Y”). If you discover a pattern that will come up again, add a rule here instead of leaving it in a PR comment. |
| [`product/`](product/index.md) | How a product surface works and what must be configured. |
| [`discussions/`](discussions/index.md) | Evolving technical discussions, evidence, alternatives, diagrams, and unresolved decisions before roadmap adoption. |
| [`roadmap/`](roadmap/index.md) | Planned-feature records and completed-feature history. These documents are planning metadata, not proof of the current implementation. |
| [`changes/YYYY-MM-DD/<name>/`](changes/2026-09-15/asset-catalog-identity/index.md) | Why a specific change landed, on the date it shipped. |

Cross-links between documents are encouraged. Do not put secrets here.
