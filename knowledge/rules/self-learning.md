---
type: Playbook
title: Self-learning rules
description: Promote a repeating decision into knowledge/rules the same time you apply it.
tags: [okf, agents]
---

# Self-learning

This repo is meant to get stricter over time without waiting for a process ticket.

**When** you hit a constraint that is not written down and you can see it applying again (a naming ban, a token rule, a CI gotcha, a “never do X in Y”) **then**:

1. Encode it in tooling if that is cheap (Biome rule, convention script, test).
2. Add `knowledge/rules/<slug>.md` with OKF frontmatter and a **When / then** pair.
3. Link it from `knowledge/rules/index.md`.
4. Record the change under `knowledge/changes/<yyyy-MM-dd>/<slug>/` if it is part of a shipped feature.

Do not leave the only copy of the rule in a Linear comment or a PR description. Do not invent product behavior — only capture decisions that already have evidence in this repo or the issue that just settled them.
