---
type: Change
title: Reset rendering documentation around complete native Vulkan replacement
date: 2026-10-06
tags: [okf, rendering, documentation, archive]
---

# Reset rendering documentation around complete native Vulkan replacement

## User scope and decision

The user requested review of existing graphics goals, physical archiving of older
documents excluded from normal analysis, a dedicated rendering directory and a
stage-by-stage plan through complete native graphics coverage. The resume message
continued this documentation task, not engine implementation.

## Delivered documentation

- `knowledge/rendering/`: baseline, requirements/disposition review, architecture,
  original-file/mod/glTF compatibility, decisions/glossary, validation gates,
  sequence and thirteen detailed stages R00-R12. Every stage is pending.
- One planned lifecycle record: `knowledge/roadmap/planned/native-vulkan-renderer.md`.
- Fifty dedicated former graphics records/supporting files moved under
  `knowledge/outdated-ideas/2026-10-06-rendering-reset/` with path/hash manifest.
  Original SHA-256 values verified immediately after moves; final hashes recorded
  after relative-link repair. No existing documentation content was discarded.
- AGENTS and the active archive rule/Cursor wrapper enforce explicit-request-only
  archive retrieval. Shared current asset/mod/tool contracts, playground and the
  mandatory unscheduled Vulkan/OpenGL backlog remain active.
- Updated catalogs, cross-links, cumulative status, handoff and Unreleased changelog.
  Official upstream releases still identify 8.0/b2d8857 as the baseline.

## Runtime boundary

No source, shaders, assets, settings, build definitions or PsyCross changes in
this task. Existing dirty implementation work remains untouched. Archiving product
history does not remove implemented code; its verified summary stays in the active
current-state note. No build/game/capture result is newly claimed here.

## Validation

- Original SHA-256 comparison immediately after every move: all 50 files matched.
- Final archive manifest hashes/path checks after link maintenance: 50/50 passed;
  former file paths are absent and destinations exist. Historical broken relative
  links encountered by the check were repaired, including missing parent levels.
- Local Markdown link scan over active/archived knowledge and root documents:
  zero missing destinations after repairs. Archive reads here were link/hash
  maintenance, not reintroduction of historical instructions into the plan.
- Thirteen stage files, unique R00-R12 IDs, planned status, frontmatter and required
  objective/steps/deliverables/acceptance/validation/fallback/handoff sections: passed.
- New-file UTF-8/frontmatter/final-newline/trailing-whitespace check: 27 files passed.
- Ordinary `rg --files knowledge`: archive excluded by `knowledge/.ignore`;
  archive remains visible to Git and explicitly authorized file access.
- CURRENT_STATUS 200 lines; RECENT_CONTEXT 103 lines, both below 350.
- `git -c core.safecrlf=false diff --check`: passed. PsyCross `diff --check` passed;
  submodule remains clean at 446b96d. Existing dirty source work remains present.
- No build, game launch, RenderDoc capture or GPU performance test executed in this
  documentation-only task. Earlier runtime evidence is labelled as previous work.

## Next action

R00: category inventory, native contracts/ownership, numerical hardware budgets
and repeatable source-backed baseline. Do not proceed to graphics implementation
solely because the planning documents were created.
