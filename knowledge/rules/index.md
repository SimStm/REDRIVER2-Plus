---
type: Catalog
title: Project rules
description: Repeatable, evidence-based constraints for working in this repository.
tags: [okf, rules]
---

# Rules

- [Use the active rendering plan and exclude archived ideas](rendering-knowledge-and-archive.md):
  start at `knowledge/rendering/`; archived graphics records require explicit
  user request and do not govern current implementation.

- [Record every Vulkan change for deferred OpenGL compatibility](vulkan-opengl-port-tracking.md):
  develop Vulkan first and maintain a separate compatibility backlog without
  assigning roadmap priority or blocking Vulkan delivery.
- [Use the new drawable extent across Vulkan resize](vulkan-resize-extents.md):
  allocate targets at the resolved new size and validate world/UI scissors with
  direct Vulkan validation as well as GPU pixels.

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
- [Keep asset identity and instance identity separate](asset-identity-lifetime.md):
  resource records are shared and stable while instances and runtime slots are
  not, and retained references must detect reuse.
- [Self-learning rules](self-learning.md): turn repeated, evidenced constraints
  into durable project knowledge.
- [Use configured MCP servers for docs, debugging, and game runs](mcp-agent-tooling.md):
  check available servers; use verified direct build/capture/local replay for
  routine native checks and VS MCP for debugging; preserve settings and evidence.

- [Verify renderer parity with state and pixel evidence](renderer-parity-evidence.md):
  check API semantics, actual bridge paths and controlled captures.
- [Validate native backdrop against clipped source geometry and artwork](native-clip-and-backdrop-validation.md):
  confirm source UVs, matrix layout, six-plane clipping and independent pixels;
  keep excluded edges and unknown auxiliary producers pending.
- [Back up the generated Visual Studio projects before regenerating](visual-studio-project-regeneration.md):
  `premake5 vs2022` deletes the hand-added `Release_dev_gl` and
  `Release_dev_playground` configurations, and new `utils/` sources are only
  compiled after a regeneration or a manual project edit.
- [Write developer settings files through DeveloperSettingsFile_WriteKeys](developer-settings-files.md):
  preserve unknown keys and comments, drop what the owner no longer wants, and
  keep the previous contents as `.bak` so a failed save cannot lose the file.
- [Keep Vulkan CPU selection reads in cached memory](vulkan-cpu-selection-memory.md):
  host-coherent mappings can be expensive to read; maintain bounded current-frame
  CPU copies and measure native submission cost.
- [Preserve upload lifetimes and original atlas UV coverage](native-upload-and-original-uv-lifetimes.md):
  retain submitted resources after failed waits; named texture rectangles do not
  bound every original MODEL UV. Active PNGs keep explicit clamp behaviour.
- [Derive native blending from the original draw policy](native-source-shape-flags.md):
  collision/shape flags and texture alpha alone do not establish raster blending.
- [Own and validate complete native mip chains](native-texture-mips.md):
  linear-light alpha-weighted filtering, quantized cutout coverage and complete
  ownership/LOD ranges; runtime replacements must fit the declared cap.
- [Keep native failures explicit and picking tied to the submitted frame](native-scene-failure-and-picking.md):
  no automatic old-world recovery, distinct loading/error states, owned logical
  GPU picks and transactional mapped-buffer allocation with real retry evidence.
- [Preserve native coordinate precision and prove coplanar ownership](native-coplanar-depth.md):
  cancel large translations before projection; authored paint and source overlaps
  need distinct evidence, with native pixels/IDs and visible seams still gating acceptance.
- [Separate native sampling diagnosis from accuracy acceptance](native-sampling-validation.md):
  isolate texels/UVs/filtering/encoding, preserve failed tolerances and keep
  hardware characterization distinct from accepted colour accuracy.
