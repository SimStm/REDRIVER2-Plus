---
type: Design
title: Rendering decisions and plain-language glossary
status: adopted-plan
created: 2026-10-06
tags: [okf, rendering, decisions, glossary]
---

# Rendering decisions and plain-language glossary

## Decisions and rationale

- **2026-10-06 — adopted goal:** replace all graphics while preserving original
  files, mods and Driver 2 gameplay. The user requested this dedicated directory,
  an excluded history archive and stage-by-stage implementation planning.
- **Vulkan first:** retain the existing OpenGL fallback; future compatibility
  work stays in its mandated unscheduled backlog. No new OpenGL implementation.
- **Native original-file adapters:** converting to GLB is optional for content
  authoring, not required to load the existing installation. The API can consume
  native buffers decoded from original files without PSX GPU packet conversion.
- **Useful PsyCross services stay:** its current graphics path is the part being
  replaced; removing audio/input/CD compatibility would not fix scene geometry.
- **Clustered Forward+ is the recommended implementation direction:** many local
  lights and transparent/cutout content can share one material path. R07 validates
  the choice and its budgets. Deferred remains a measured alternative, not an
  adopted second renderer or a solution for missing source objects.
- **Bounded whole-city visual resources:** measure full CPU preload first, then
  select residency from evidence. Camera/light demand stays independent of gameplay.
- **Accurate geometry before effects:** correct normals, depth, transforms and
  silhouettes are prerequisites for convincing shadows and image quality.

## Open decisions assigned to stages

R00: first target hardware, numerical budgets, unit scale, camera/depth format,
feature minimum and supported target set. R02: decoder/cache schema and exact
glTF import subset. R04: preload versus sector residency and LOD transition policy.
R05: cascade count/resolution/range and original prelit-material tuning. R07:
cluster dimensions, overflow and shadowed-local-light budgets. R08: measured AA
default and fallback. R09: optional model sidecar and dynamic mapping rules.
These are implementation decisions to resolve with evidence; no invented dates,
performance promises or hardware limit values are attached to this plan.

## Terms with examples

- **Mesh:** vertices and triangles describing an object. One building mesh can
  be reused at many placements instead of decoding its faces every frame.
- **Instance:** one placement of a mesh, with position/rotation and identity.
  Five identical lamp posts share a mesh but have five instances.
- **Material:** how a surface responds to light: textures, roughness, metalness,
  opacity and other properties. A texture image alone is not a complete material.
- **PBR:** a consistent approximation of physical light/material behaviour.
  Painted metal and rough concrete respond differently; it does not automatically
  remove baked shadows from original textures or make low-detail models realistic.
- **GLB/glTF:** modern asset containers describing meshes/materials/transforms,
  sometimes animation. They are optional inputs, not a replacement for game logic.
- **Depth buffer:** per-pixel distance used to decide which surface is in front.
  A fake/quantized legacy depth can make lighting use an incorrect position.
- **Caster/receiver:** a building blocks sunlight (casts); the road displays its
  shadow (receives). The building may be behind the camera and still matter.
- **Cascade:** one distance band of sun shadow detail. Several bands preserve
  sharp nearby shadows while covering farther buildings at lower resolution.
- **Bias:** a small offset that prevents a surface from mistakenly shadowing
  itself. Too much makes shadows detach; too little creates stripes/speckles.
- **Residency/streaming:** which assets are available in CPU/GPU memory and when
  they load. A longer camera range is useless if the distant mesh has not loaded.
- **Culling/LOD:** skip objects irrelevant to a view; use a simpler model farther
  away. Light visibility must not reuse camera-only culling blindly.
- **Forward+/clustered:** shade a surface using a prepared list of nearby lights.
  Divide space into cells so a lamp on another street does not cost every pixel.
- **Deferred:** store surface information first, then light it in later passes.
  It has memory/transparency tradeoffs and still needs complete geometry/resources.
- **MSAA/spatial/temporal AA:** sample edges multiple times; filter the current
  image; or combine frames. They reduce different jagged/flickering patterns and
  have different costs. Temporal AA needs motion/history and can leave trails.
- **HDR/tone mapping:** retain lighting values above display brightness and map
  them to the screen. It is a colour pipeline, not proof of realistic lighting.
- **Fixed simulation/interpolation:** update physics at the original game rate,
  while displaying intermediate poses smoothly. Drawing twice as often must not
  run physics, missions or siren timers twice as fast.

## Technical references

- [Khronos glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html):
  asset and material semantics.
- [Khronos Vulkan Guide: depth](https://docs.vulkan.org/guide/latest/depth.html):
  depth formats/usages; query the actual device before selecting them.
- [Khronos Vulkan Guide: versions](https://docs.vulkan.org/guide/latest/versions.html)
  and [formats](https://docs.vulkan.org/guide/latest/formats.html): API version,
  enabled features and supported format usage are distinct checks.
  Context7 queried the official Vulkan Guide in this documentation task.

## Decision history maintenance

### 2026-10-08: execution order and Chicago water scope

User adopted R00-R07 -> R10 -> R11 -> R08 -> R09 -> R12, retaining stage IDs,
filenames and acceptance-before-advance. R03 remains first incomplete. Migrate
original effects/UI and separate presentation from fixed simulation before
image-quality refinement and production modern imports. The previous numeric
order made R10 depend on R09; the reviewed R10 consumers need native material,
composition and target contracts, not production GLB/glTF. R10 now follows R07;
R11 still requires R10 and the draw-time mutation audit. R08/R09 must preserve
and regression-test those accepted consumers, poses and multiple-view contracts.

User clarification: Chicago's apparent water is empty space over sky/background;
actual water is associated with Rio, which is not the current priority. Existing
[R03 source inventory](stages/03-complete-static-region.md) records no water or
transparency-flagged objects in the copied slice. Water is not applicable to that
slice and must not block R03 or prompt invented geometry. Keep applicable
transparency and tree silhouettes as independent acceptance work; retain actual
Rio water in future city-specific coverage before R12 full-game acceptance.

This is a documentation/scheduling change only. No runtime behaviour, source
assets, resources or synchronization changed; no stage or visual gate became
accepted. Existing GPU/region evidence retains its original scope. Remaining
terrain, silhouette, transparency and performance checks require actual results.
Executed documentation checks: scoped `git diff --check`, exact thirteen-stage
order, dependency assertions, local links/text hygiene and status-file line caps
PASS. No runtime tests or builds were run for this decision update.

### 2026-10-07: native failure and logical picking

User-adopted decision: this separate version replaces 100% of old graphics;
automatic old-scene recovery is prohibited. Preserve existing OpenGL separately
and remove old dependencies only after their consumers pass native migration.
Explicit source/upload loading is expected; missing publication, unsupported,
stale, budget and GPU errors require diagnostics and no old-world pixels.
Actual GPU failure/loading and subsequent new-native-image recovery tests pass;
remaining UI/frontend/effect/VRAM coverage is recorded as pending consumers.

Implemented picking uses a native R32_UINT MRT output sharing depth, culling and
fragment alpha discard, with one-pixel readback after the ordinary owner fence.
Copied bounded snapshot identities and game-owned source metadata resolve logical
instances/material parts independently of array order. CPU projected picking
would approximate texture/depth coverage; geometry shaders/gl_PrimitiveID are not
required. GPU and original-city/RenderDoc evidence support this declared slice.
Face highlights/export remain pending. Future MSAA/transparent picking policies
must be defined in R08/R10; do not infer them from opaque/cutout coverage.

Amend this file and the affected stage after a material renderer discussion.
Record recommendation versus user adoption, evidence, alternatives and remaining
questions. Do not resurrect archived prerequisites or turn a design choice into
an implementation claim. The active rendering directory is the user-requested
home for this subject; follow the general discussion rule's evidence discipline.
