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

Keep patch files at LF (`.gitattributes` sets `patches/**/*.patch text eol=lf`).
A `git apply` patch contains a base64 binary payload; a Windows checkout with
`core.autocrlf=true` rewrites its line endings and `git apply` then fails with
`git diff header lacks filename information`. Always validate the patch in a
fresh clone, not only in the already-modified working tree.
