---
type: Rule
title: Keep PsyCross changes as patches
description: Project-specific PsyCross changes are carried as reproducible patches, not unpublished gitlinks.
tags: [okf, psycross, submodule]
---

# Keep PsyCross changes as patches

**When** REDRIVER2-Plus needs a PsyCross change that is not available from its
upstream repository, **then** store a focused binary-safe patch under
`patches/psycross/` and apply it with `scripts/apply_psycross_patches.ps1`
after submodule initialisation.

Keep the parent gitlink at the documented upstream base commit. An unpublished
submodule commit makes a parent pull request impossible to clone or build.
Update the patch, script documentation, and the expected base revision together
when rebasing the integration onto a different PsyCross revision.
