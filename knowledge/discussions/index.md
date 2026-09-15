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
  assets, and a proposed sequence of experiments. **Exploring.**

## Structure and lifecycle

Use one `subject/index.md` per durable topic, with supporting notes and
`diagrams/` alongside it. Include context, current evidence, recommendations,
alternatives, decision status, open questions, and dated discussion history.
Record material updates in place; retain the rationale for superseded choices.

The lifecycle is discussion, adopted roadmap scope, implementation, then product
documentation and a completed roadmap record. A discussion can stay unresolved
or be closed without entering the roadmap. Retain it when a feature is adopted
and link both ways. Record only actual user decisions as adopted.
