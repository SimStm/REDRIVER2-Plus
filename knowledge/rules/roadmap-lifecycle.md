---
type: Rule
title: Maintain roadmap records
description: Planned work is recorded separately from implemented product behaviour.
tags: [okf, roadmap, documentation]
---

# Maintain roadmap records

Exploratory options and recommended sequences belong in
[`discussions/`](../discussions/index.md) until a concrete feature scope is
adopted for the roadmap. A request to discuss a possible direction is not
roadmap adoption or implementation authorization.

**When** a feature is planned, **then** create one
`knowledge/roadmap/planned/<slug>.md` record before treating that work as part
of the project roadmap.

**When** the feature is implemented, **then** move the record to
`knowledge/roadmap/done/<slug>.md`, mark it implemented, and add or update its
matching `knowledge/product/<slug>.md` document in the same change.

Roadmap records are planning metadata. A record in `planned/` must never be
reported as implemented during repository analysis. A record in `done/` is a
completion history, but the matching product document and source remain the
authoritative evidence for current behaviour.
