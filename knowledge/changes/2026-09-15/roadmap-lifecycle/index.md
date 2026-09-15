---
type: Change
title: Add the roadmap knowledge lifecycle
description: Establish planned and completed feature records without confusing plans with implementation evidence.
tags: [okf, roadmap, documentation]
---

# Add the roadmap knowledge lifecycle

## Context

The project needed a durable place to describe future work without causing
repository readers to report planned features as implemented behaviour.

## Decision

Add `knowledge/roadmap/planned/` for feature proposals and
`knowledge/roadmap/done/` for records moved after implementation. Completed
records require a corresponding `knowledge/product/<slug>.md` document.

## Impact

Planning, implementation evidence, and operational product documentation now
have distinct roles. Agents must ignore planned records when evaluating the
current feature set.
