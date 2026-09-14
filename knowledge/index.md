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
| [`changes/YYYY-MM-DD/<name>/`](changes/2026-08-24/lint-format-typecheck-ci/index.md) | Why a specific change landed, on the date it shipped. |
| [`../DESIGN.md`](../DESIGN.md) | Visual system — tokens, typography, layout, component anatomy, and current drift. Read before adding or restyling UI. Prototypes live in [`references/prototypes/`](references/prototypes/). |

Cross-links between documents are encouraged. Do not put secrets here.
