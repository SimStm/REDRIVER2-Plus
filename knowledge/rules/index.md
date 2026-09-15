---
type: Catalog
title: Project rules
description: Repeatable, evidence-based constraints for working in this repository.
tags: [okf, rules]
---

# Rules

- [Maintain evolving discussion records](discussion-records.md): update the
  same topic as decisions evolve, separating exploration from roadmap adoption
  and implemented behaviour.
- [Reconstruct texture manifests from inspector filenames](texture-manifest-from-filenames.md):
  recover texture names and page/index selectors from original or upscaled exports.
- [Validate inspector exports before publication](inspector-export-validation.md):
  repeated exports replace completed files, while failed writes preserve them.
- [Design override PNG alpha for the 0.5 cutout and the effect blend mode](texture-alpha-semantics.md):
  fragments below 0.5 alpha are cut out on every primitive, `BM_AVERAGE`
  honours texel alpha proportionally, and the additive/subtractive modes
  ignore it.
- [Append texture registrations without rewriting manifest data](manifest-append-merge.md):
  add missing `(texture, texturePage, textureIndex)` entries atomically while
  preserving unknown fields and existing mappings.
- [Build and run the Linux target (including via WSL)](linux-wsl-build.md):
  install the system SDL2/OpenAL/GL/libjpeg packages, apply the PsyCross patch,
  generate with Premake, and build in `src_rebuild/build`.
- [Regenerate build files after source additions](generated-build-files.md):
  generated IDE projects do not discover newly added source files on their own.
- [Keep PsyCross changes as patches](psycross-patches.md): project-specific
  submodule changes must be reproducible without a private fork.
- [Maintain roadmap records](roadmap-lifecycle.md): planned features remain
  separate from implementation evidence and require product documentation when
  completed.
- [Reproduce gameplay with debug start snapshots](developer-debug-start.md):
  start a debug build directly at a known mission, vehicle, and position, and
  capture frames without input injection.
- [Self-learning rules](self-learning.md): turn repeated, evidenced constraints
  into durable project knowledge.
