---
type: Catalog
title: Project roadmap
description: Planning records for upcoming work and completion records for implemented features.
tags: [okf, roadmap]
---

# Roadmap

This directory records feature intent and completion history. It is not a
source of truth for code currently present in the repository.

- [`planned/`](planned/index.md) contains proposals that have not been
  implemented. Ignore these records when determining available behaviour,
  compatibility, or shipped functionality.
- [`done/`](done/index.md) contains records moved after implementation. They
  record the delivery decision, but are not sufficient evidence of current
  behaviour; use the linked product document and source code instead.

## Lifecycle

1. Create `planned/<slug>.md` for every newly planned feature. Use a lowercase
   kebab-case slug that can also name its product document.
2. Include YAML frontmatter with `type: Roadmap`, `title`, `status: planned`,
   and useful tags. The document must describe the problem, intended behaviour,
   scope, non-goals, dependencies or risks, acceptance criteria, and planned
   validation.
3. Do not claim the feature exists because a planned record exists. Update or
   remove the record if the plan changes or is abandoned.
4. Once implemented, first add or update
   `knowledge/product/<slug>.md` with the operational product behaviour. Then
   move the same roadmap record to `done/<slug>.md`, change its status to
   `implemented`, add the completion date, and link to that product document.
5. Never move a record to `done/` without its corresponding product document.

## Planned record outline

```markdown
---
type: Roadmap
title: Feature name
status: planned
tags: [roadmap]
---

# Feature name

## Problem
## Intended behaviour
## Scope
## Non-goals
## Dependencies and risks
## Acceptance criteria
## Validation plan
```
