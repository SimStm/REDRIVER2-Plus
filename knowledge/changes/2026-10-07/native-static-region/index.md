---
type: Change
title: R03 source inventory, shared materials and partial region drawing
status: partial
created: 2026-10-07
tags: [okf, rendering, vulkan, native]
---

# R03 source inventory, shared materials and partial region drawing

## Latest logical picking and explicit-failure milestone

Current native C ABI v8 includes GPU logical picking and explicit scene availability.
An R32_UINT MRT shares native alpha/depth/culling; one-pixel readback resolves copied
64-bit identities and mesh/material handles after the presenter fence. Game-owned
original model/placement/material-part/texture metadata is bounded and generation
checked. CPU picking19, actual GPU allocation rollback/retry, order/depth/alpha/
shared identity/lifetime checks, real Chicago building/tree/miss and RenderDoc1.46
ID/UI exclusion evidence PASS. No native face highlight/export or dynamic coverage.

User steering prohibits automatic native-to-old-world scene fallback and adopts
100% graphics replacement in this separate version. Expected loading and actual
errors are distinct, visible states. Native unavailable frames clear colour and
suppress old-world/composites; UI/frontend/effect/VRAM consumers remain recorded
pending dependencies, to remove only after migration. OpenGL is preserved as a
separate backend choice. No new legacy features/recovery mechanisms were added.

CPU frame-state13 and actual GPU error/loading/new-native-resource recovery pixels
PASS. Submitted-upload resources survive until their actual fence, then retire to
zero owned bytes. Chicago loading/error world0/UI10, recovery native956/world0,
uploads61 and unchanged geometry148320/RGBA49926792 verified; diagnostic visible
with F11 closed, including the first failed frame after fixing automatic size
measurement. Details, hardware/settings and private proof paths are recorded in
[R03 validation](../../../rendering/r03-validation.md). Picking and explicit-failure
gates pass; region/visual/camera/mod/drive gates remain pending. R04 is unstarted.

Final CPU/asset1505, VS and native/PSX GPU reruns PASS; direct outputs load
Khronos validation with no VUID. Actual missing publication and valid snapshot
without boundary fail explicitly; native boundary restoration recovers pixels.
Unsupported `-nativeworld -opengl` shows a useful startup message and exits1
after acknowledgement. Final report10966 characters is untruncated.

Implement a camera-independent fixed Chicago source region before attempting
city-wide native residency. The region owns all selected placements/resources,
preserves gameplay's usage bitset and high-detail links, and draws876 parts for
530 distinct objects/4814 triangles. Capture confirms native D32 geometry and
separate original UI, with zero duplicated legacy-world draws in that frame.

Repeated per-frame copies/hashes of high-resolution PNGs caused expensive CPU
preparation. Immutable owned PNG vectors now survive by shared ownership across
adapter/cache invalidation; live original textures still compare freshly decoded
VRAM bytes. Shared native materials keep exact content/context and last-reference
retirement, including partial creation/reset rollback. C ABI v3 adds explicit
back/two-sided culling with larger bounded payloads.

Asset/cache1219, scene18/camera8/material13 and host guard checks PASS; VS build
PASS; actual RenderDoc1.46 region replay has zero messages. CPU cumulative
assembly mean changed45.910 ms/121 samples to3.710 ms/481 samples, including
initial uploads and using different windows. No isolated p95/GPU time claim.

Original sky/painted horizon now uses bounded owned source geometry and copied
art tables before GTE projection. C ABI v4 backdrop instances rotate with the
camera, preserve original vertical offset/brightness and draw first with depth
test/write disabled. Actual source models0..3 each have30 vertices/20 family21
quads. Frame500 capture independently confirms80 depth-off draws/480 indices,
876 depth-on world draws/14442 indices and10 UI draws. D32 backdrop remains1.0;
world writes<1.0, zero messages. GPU readback proves order, translation/rotation,
UI preservation and retirement; fresh PSX regression PASS. Latest asset1503/
scene20/camera12/material13 tests and VS build PASS.

This is a partial milestone. Native picking/outside accounting and
controlled turns/FOV/filter/mod/drive acceptance remain pending. GPU culling and
fresh PSX readbacks PASS, Khronos loaded/no VUID output. At tick600 recent120
assembly mean2.983/p953.540 ms (CPU only). Fork304c14a committed/pushed and parent
gitlink staged. R03 has not been accepted, R04 has not started.
See [product](../../../product/native-static-region.md),
[evidence](../../../rendering/r03-validation.md) and the unscheduled
[OpenGL backlog](../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md).

Subsequent ABI v5 mip milestone owns full trilinear chains, generated in linear
light with alpha-weighted NPOT area filtering and closest attainable cutout
coverage. All levels count toward64MiB RGBA before source traversal; Vulkan
upload/barriers/views/sampler bounds cover the full chain. Nearest/linear generic
callers retain their level-zero semantics. Original smooth filtering selects
trilinear for world/backdrop. Scene20/camera12/material13/mip13 and asset1505
PASS; actual GPU minification/cutout/upload-retention and fresh PSX regressions
PASS with Khronos loaded/no VUID. RenderDoc1.46 frame500 validates61 complete
material chains,80 backdrop/876 world/10 UI draws and no messages.

City steady samples use49926792 RGBA bytes/956 native draws. Initial live filter
replacement exceeded the cap; ABI v6 fixes it with copied per-instance sampling
and prewritten retained descriptor sets on one full chain. CPU sampling8/GPU
eight choices and independent instances PASS. Actual city eight toggles across
ticks120-1080 keep all960 intervening submissions native, uploads61/no extra,
fallback0, same observed image/RGBA bytes. No replacement work remains for this
filter setting. Picking/outside/visual/mod/drive gates still block R03 acceptance.

Subsequent ABI v8 milestone accepts logical instance/material-part picking and
explicit native failure only. R32_UINT depth/cutout IDs and copied source metadata
resolve building/tree/miss picks without old scene packets. Unavailable world
frames suppress old world/composites, distinguish loading/errors and show useful
diagnostics with F11 closed. GPU pixels/resources prove native recovery, not old
scene recovery. CPU picking19/frame-state13, asset1505, native/PSX GPU and VS
checks PASS; Khronos loaded/no VUID. RenderDoc ID/scene/UI replay PASS. Independent
OpenGL city still renders; native-on-OpenGL fails startup explicitly with exit1.
Fork bd5b67540fcfb3028256ec2c1907212596d55185 pushed/gitlink staged; five INIs and
`.user` restored byte-exactly. Outside/visual/camera/mod/drive gates still block
R03 acceptance; R04 remains unstarted. See the current evidence linked above.

Subsequent source/publication accounting reconciles530 copied/selected/published
objects/876 parts and records228 original view-inside/78 outside candidates.
Duplicate/missing/wrong publication rejects the native scene; outside sources are
declared omissions. Asset1523 (18 accounting cases), scene/host suites and VS build
PASS. New RenderDoc1.46 frame500:80 backdrop/876 world/10 UI/no messages; full
colour and ID images equal the previous capture. Ownership/upload counts unchanged.
Five INIs/`.user` restored byte-exactly. Accounting item accepted; controlled
turns/FOV/mod/driving and remaining R03 acceptance still block R04.
