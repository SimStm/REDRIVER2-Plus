---
type: Rule
title: Maintain evolving discussion records
description: Preserve substantive technical discussions without turning proposals into implementation claims.
tags: [okf, discussions, agents, documentation]
---

# Maintain evolving discussion records

**When** a project conversation materially develops an architectural direction,
tradeoff, or multi-stage technical investigation, **then** read the discussion
catalog and create or update `knowledge/discussions/<subject>/index.md` in the
same task. This standing instruction authorizes routine documentation updates;
do not request confirmation again. Skip trivial Q&A and unchanged repetition.

**When** the same subject returns, **then** update its existing record rather
than creating a duplicate. Keep the current synthesis readable and append a
dated history entry recording what changed and why. Preserve superseded
decisions and their rationale; distinguish user decisions from recommendations.

- Use English, OKF frontmatter, a stable kebab-case subject, `status: exploring`
  initially, and created/updated dates. Link new subjects from the catalog.
- Include context, verified source evidence, recommendations, alternatives,
  open questions, decision status, and dated history. Label unknowns explicitly.
- Cite source paths and symbols; record the inspected revision and dirty-tree
  caveats when relevant. Existing plans are not implementation evidence.
- Keep substantial supporting notes and diagrams within the subject folder.
  Save diagrams as an image plus editable source or a reproducible generator;
  label proposed architecture and re-render when its meaning changes.
- Summarize technical substance rather than copying raw chat transcripts.
  Do not copy secrets, personal data, or proprietary game assets.
- Promote only an adopted, bounded scope into the roadmap using its existing
  lifecycle. Keep the discussion and link the resulting roadmap records.
- Discussion updates alone do not authorize prototypes or implementation.
  Product documents describe implemented behaviour, not discussion outcomes.
