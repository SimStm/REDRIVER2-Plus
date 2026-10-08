---
type: Validation
title: R03 west auxiliary car and wheel attribution
created: 2026-10-08
tags: [okf, rendering, validation, ownership]
---

# R03 west auxiliary car and wheel attribution

The west frames contain14 normal/HD auxiliary draws and11 wide/original draws,
against the inspector's existing ten-draw baseline. Earlier source/post-VS
inspection separated51 PGXP vertices from342 screen-space vertices, but could
not establish the game producer. This milestone identifies the PGXP producer
without changing renderer behaviour or accepting the aggregate count gate.

## Actual producer and batching evidence

Fresh configured RenderDoc1.41 frame500 captures use the same west start as the
[source-road evidence](r03-west-road-validation.md). Normal uses restored1280x720,
FOV256/HD on; wide uses1920x1080, FOV128/HD off. A temporary read-only probe at the
end of `PsyX_GPU.cpp::ParsePrimitive` logs existing inspector identity and the
actual expanded `GrVertex` fields for nearby PGXP packets. No packet, transform,
draw policy or resource is modified. Existing identity ranges are registered by
`cars.c::DrawCarObject` and `DrawCarWheels`, not inferred from material names.

The offline join requires the exact VBO vertex index and packed float32 x/y/z/
screen-height/page/clut, UV bytes and RGBA bytes. Every captured vertex has one
unique producer identity; repeated records of the same owner are allowed, while
missing or ambiguous owners fail. Both captures match all51 vertices:

- 12 vertices: **Wheel3 of Car#11**, key
  `car:0:0:11:1:2/component:wheel:3`.
- 39 vertices: **Car#11, model1, high detail, palette3**, parent car identity.
  No captured PGXP prefix vertex is attributed to a car shadow or static tile.

Normal prefix draws contain6 wheel,6 body,6 wheel and33 body vertices. Wide
contains the same12 wheel/39 body vertices in one51-vertex draw. Actual normal
fragment constants alternate `texFormat=3` (RGBA override) and0 (4-bit VRAM):
wheel draws bind a1024x1024 RGBA image, while body draws use the VRAM/LUT route.
Wide uses `texFormat=0` throughout. `AddSplit` explicitly splits on texture
format/texture identity/override state; this accounts for four versus one draw
without assuming a constant auxiliary batch total for unmigrated consumers.

Changing one captured RGBA byte in a separate export makes the exact join fail
at the first vertex with exit1 and no success receipt. Original captures and
exports are unchanged.

## Native pixels, clipping and restoration

Both51-vertex prefixes contain17 triangles wholly outside the left homogeneous
clip plane: maximum x+w is-2.04903245 normal and-0.670045018 wide. Before/after
shared-colour bytes are identical. Remaining ten draws contain342 vertices,
all with screen-height0; this establishes screen-space geometry, not exact HUD/
effect producer attribution. The prior native ID/D32 isolation evidence remains
separate from the shared-colour contract.

Normal native colour/ID/D32 hashes match the uninstrumented1280x720 capture from
the [fixture exit verification](r03-fixture-exit-validation.md) exactly. Wide
hashes match the earlier uninstrumented1920x1080 wide capture exactly. Native
counts remain80 backdrop/1023 physical world/876 logical; four HIGH replay
warnings and `regionCountsMatch=false` remain visible in both reports.

Direct probe builds/captures and local SDK analyses complete successfully;
normal inspector/export14.042s/0.506s, wide15.929s/0.428s. Separate binding exports
pass. No new native/PSX GPU fixture suite is claimed in this investigation.
The probe source is restored byte-exactly and the final direct build recompiles
`PsyX_GPU.cpp`. Copy restoration initially preserved an old timestamp and MSBuild
skipped recompilation; touching the restored source forces the correct build.
All six settings/generated XML hashes match their backups. The fork remains
clean atae83dd2; no probe/runtime contract is shipped and OpenGL is preserved.

Private captures, producer JSONL, expanded vertex exports, exact-join/negative/
clip/equivalence/binding receipts and build logs live in ignored
`src_rebuild/build/native-renderer-r03/aux-producer-probe141/`. They contain game
assets and must not be redistributed.

## Acceptance and pending removal

Producer attribution and batching cause are now verified for these two west
frames. Car body/wheel commands remain explicit **R06 dynamic migration debt**;
do not delete their producers or call their existence automatic scene fallback.
Their complete clipping here does not prove that legacy dynamic commands can
never contribute pixels in other views. Screen-space consumers remain R10 debt.

The [subsequent count contract](r03-count-contract-validation.md) separates static
inventory from the unchanged ten-draw baseline/failed combined field. No silent
change to14/11 or migration acceptance. Next in R03: negative-w static part605,
excluded edges/silhouettes, remaining surface/region coverage and GPU cost.
R03 is incomplete and R04 remains unstarted. Compatibility tracking: VK031.
