---
type: Rule
title: Track project PsyCross changes in the fork
description: Project-specific PsyCross changes are committed to the project fork, not carried as patches.
tags: [okf, psycross, submodule, fork]
---

# Track project PsyCross changes in the fork

**When** REDRIVER2-Plus needs a PsyCross change that upstream does not provide,
**then** commit it directly in the project fork and record the resulting commit
in the parent gitlink.

Use the fork, not upstream, as the working source. The fork carries this
project's Vulkan backend (`PsyX_Vk.*`), the MoltenVK/portability work and other
renderer changes that `OpenDriver2/PsyCross` does not have, so the build must
always come from the fork. Never check out upstream's `master` in
`src_rebuild/PsyCross`, never point `.gitmodules` or the gitlink at
`OpenDriver2/PsyCross`, and never build against a copy of it.

The submodule `src_rebuild/PsyCross` is wired to the project fork:

- `origin` = `git@github.com:SimStm/PsyCross.git` (push target; the checkout's
  working branch tracks `origin/master`)
- `upstream` = `https://github.com/OpenDriver2/PsyCross.git` (read-only source
  of upstream commits for rebasing the fork)

Workflow:

1. Edit the code in `src_rebuild/PsyCross` (the working tree the build uses).
2. Commit and push: `git -C src_rebuild/PsyCross push origin HEAD:master`.
3. Record the commit in the parent: `git add .gitmodules src_rebuild/PsyCross`.

Do not reintroduce `patches/psycross/` or `scripts/apply_psycross_patches.ps1`;
they were removed when the submodule moved to the fork. A patch and a live
gitlink would both try to own the same files.

To integrate upstream PsyCross work, fetch `upstream` and rebase the fork branch
on top of it, then bump the parent gitlink. Resolve conflicts in the fork, not
by re-adding patches.

Keep vendored dependencies that the game build needs (for example
`third_party/imgui`) committed inside the fork, so a parent clone builds without
extra setup.

Keep public PsyCross declarations C-compatible where the mixed C/C++ game code
consumes them, and keep the fork reachable from any machine that clones the
parent.
