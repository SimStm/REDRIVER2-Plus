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
- [Keep model references as metadata outside the texture override key](model-reference-metadata.md):
  batch-export by identity, list shared models without duplicating the image,
  and preserve a mapped `file` on re-export.
- [Build and run the Linux target (including via WSL)](linux-wsl-build.md):
  install the system SDL2/OpenAL/GL/libjpeg packages, check out the PsyCross
  fork submodule, generate with Premake, and build in `src_rebuild/build`.
- [Regenerate build files after source additions](generated-build-files.md):
  generated IDE projects do not discover newly added source files on their own.
- [Track project PsyCross changes in the fork](psycross-fork.md): commit
  submodule changes to the project fork and bump the parent gitlink.
- [Maintain roadmap records](roadmap-lifecycle.md): planned features remain
  separate from implementation evidence and require product documentation when
  completed.
- [Reproduce gameplay with debug start snapshots](developer-debug-start.md):
  start a debug build directly at a known mission, vehicle, and position, and
  capture frames without input injection.
- [Generate a coherent world for a playground or new map](world-scene-generation.md):
  replace surface, cell, collision, visibility and streaming data together when
  adding a generated scene, and avoid donor-world leakage.
- [Share the legacy projection and depth when adding a modern mesh path](psyx-modern-mesh-bridge.md):
  reuse `Projection3D` and the GTE vertex encoding, draw in `GR_EndScene`, and
  restore the caller's VAO instead of binding 0.
- [Keep asset identity and instance identity separate](asset-identity-lifetime.md):
  resource records are shared and stable while instances and runtime slots are
  not, and retained references must detect reuse.
- [Self-learning rules](self-learning.md): turn repeated, evidenced constraints
  into durable project knowledge.
- [Use configured MCP servers for docs, debugging, and game runs](mcp-agent-tooling.md):
  check for context7, visual-studio-ide-mcp, and desktop-control MCPs first, and
  never block on a terminal launch of the interactive game process.

- [Verify renderer parity with state and pixel evidence](renderer-parity-evidence.md):
  check API semantics, actual bridge paths and controlled captures.
