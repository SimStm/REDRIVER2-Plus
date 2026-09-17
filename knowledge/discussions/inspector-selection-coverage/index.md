---
type: Discussion
title: Whole-object selection across renderer categories
status: exploring
created: 2026-09-16
updated: 2026-09-16
tags: [discussions, inspector, picking, identity, coverage]
---

# Whole-object selection across renderer categories

## Context and decision status

Roadmap item 06 expands the 3D Debug inspector from a draw-stream approximation
into whole-object selection across renderer categories. This record is the
milestone 1 coverage inventory: every producer path that carries (or loses)
object identity, the in-game reproduction of the reported gaps, and the
root-cause evidence gathered with the Visual Studio debugger.

This is exploration plus inventory evidence, not a shipped feature. Roadmap item
06 remains `planned`; nothing here authorizes treating selection coverage as
complete. Milestones 2–5 (component identity, selection modes and lifetime,
visibility-aware picking, diagnostic fallback and overhead) are still open.

## Picking mechanism (verified source evidence)

- A producer registers a primitive-memory range with
  `PsyX_Inspector_RegisterPrimitiveRange` / `PsyX_Inspector_RegisterObjectRange`
  (`src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp:160` and `:178`). Ranges are
  frame-local and reset at the end of `DrawAllSplits`
  (`PsyX_GPU.cpp:1306`); capacity is `MAX_INSPECTOR_RANGES = 2048`
  (`PsyX_GPU.cpp:87`).
- `FindInspectorRange` (`PsyX_GPU.cpp:190`) maps a packet address to a range in
  reverse registration order, and `ParsePrimitive` stores the result per vertex
  in `g_vertexInspectorRange` (`PsyX_GPU.cpp:2059`, `:2142`).
- `PsyX_Inspector_RequestPick` (`PsyX_GPU.cpp:131`) is resolved by
  `ResolveInspectorPick` (`PsyX_GPU.cpp:1141`) during `DrawAllSplits`
  (`PsyX_GPU.cpp:1304`). The loop projects every submitted triangle
  (`ProjectInspectorTriangle`, PGXP-aware, `PsyX_GPU.cpp:1101`) and tests the
  cursor with `PointInsideTriangle` (`PsyX_GPU.cpp:1132`).
- The pick has **no depth test, no alpha/CLUT cutout test, and no scissor/clip
  test**; it keeps the **last** matching triangle in vertex-buffer order
  (`selectedVertex = vertexIndex`, `PsyX_GPU.cpp:1166`). Whatever primitive is
  parsed last and covers the cursor wins.
- Game-side registration call sites are only three:
  `src_rebuild/Game/C/draw.c:1040` (static buildings),
  `src_rebuild/Game/C/tile.c:337` (city/tile surfaces), and
  `src_rebuild/Game/C/cars.c:1430` (car bodies).

## Producer inventory

State legend: **selectable** = does a click resolve a labelled range today;
**exportable** = any inspector export is enabled for it; **provenance** =
whether a stable object key/model identity is attached.

| # | Category | Producer (symbol @ file:line) | Registered? | Selectable | Exportable | Provenance |
|---|---|---|---|---|---|---|
| 1 | Static buildings | `DrawAllBuildings` -> `PlotBuildingModel*` @ `draw.c:1012`, registered `draw.c:1040` | Yes (`building:<modelId>:x:y:z:yang`) | intended yes | texture/PNG + source-model batch (catalog) | present (catalog model id + placement) |
| 2 | City tiles / road surface | `DrawTILES` @ `tile.c:219`, `Tile1x1`/`TileNxN`, registered `tile.c:337` | Yes (`tile:<modelId>:x:y:z:yang`) | intended yes | texture/PNG + source-model batch | present |
| 3 | Roads | built by `dr2roads.c`, rendered through the tile path (`roadbits.c` `Tile1x1` family, `DrawTILES`) | via #2 only | yes (as tiles) | as #2 | present (as tile model id) |
| 4 | Car body (player / civilian / police / parked) | `DrawCarObject` @ `cars.c:1375`, registered `cars.c:1430` | Yes (`car:<level>:<variant>:<id>:<model>:<modelNumber>`) | intended yes | OBJ (`Cars_ExportInspectorModel`) | present (slot + source model) |
| 5 | Wheels | `DrawCarWheels` @ `cars.c:650` -> `DrawWheelObject` @ `cars.c:564`, called `cars.c:1750` after the body | **No** | no | no | **lost** |
| 6 | Civilian pedestrians | `DrawAllPedestrians` @ `pedest.c:478` -> `DrawCiv` @ `motion_c.c:1431` -> `DrawBodySprite` @ `motion_c.c:490` | **No** | no | no | **lost** |
| 7 | Tanner / Jericho characters | `DrawTanner` @ `motion_c.c:1623`, `DrawCharacter` @ `motion_c.c:1675` -> `newShowTanner` @ `motion_c.c:1047` -> `RenderModel` @ `motion_c.c:1175` | **No** | no | no | **lost** |
| 8 | Pedestrian head / face | `DrawBodySprite` (HEAD bone) and `DoCivHead` @ `motion_c.c:1967` -> `RenderModel` @ `motion_c.c:2021` | **No** | no | no | **lost** |
| 9 | Animated props | `DrawAllAnimatingObjects` @ `objanim.c:432` -> `animate_object` @ `objanim.c:455` -> `AddTrafficLight`/`AddLightEffect`/`AddSmallStreetLight` (`debris.c`) | **No** | no | no | **lost** |
| 10 | Effects / event geometry | `debris.c` effects, `event.c` `RenderModel` sites, `shadow.c` shadows | **No** | no | no | **lost** |
| 11 | Foliage / sprites | `DrawSprites` @ `draw.c:170`, called `draw.c:1575` | **No** | no | no | **lost** |
| 12 | Sky | `sky.c` (drawn at the OT back) | **No** | no | no | background, no provenance by design |
| 13 | HUD / minimap / overlay | `main.c`, `overlay.c`, `overmap.c` | **No** | no | no | 2D UI, not a world object |
| 14 | Full-screen quad (unidentified) | parsed late, covers the whole viewport (see root cause) | **No** | **no (dominates the pick)** | no | **lost** |

Every category now has an explicit state, satisfying the milestone 1 definition
of done. Categories 5–11 and 14 are the concrete coverage gaps.

## In-game reproduction (evidence)

Run: `Release_dev|x64`, `bin/Release_dev/REDRIVER2_dev.exe`, Chicago (`level 0`,
`mission 50`) from the enabled `developer_debug_start.ini`, 1280x720, F11 ->
**3D Debug** -> **Pick visible primitive** on.

Asset catalog panel at load: `Context: level 0, variant 0, single player`,
`Generation: 1`, `Models 612/1536 | textures 303/2048 | cars 6/32`,
`Material links 223/8192`, `Static footprint: 384256 bytes`.

Every clicked category returned the same unlabelled result. Exact panel strings
(verbatim):

- Player car: `Selected primitive: 2658`, `Texture page: 0 | CLUT: 0`,
  `UV: (0, 0), (0, 0), (0, 0)`,
  `Source region: 128, 128 — 56 x 58 (override source)`,
  `Object: Unlabelled draw source`,
  `OBJ unavailable: only labelled car bodies currently support model export.`
- Civilian car: `Selected primitive: 2842`, `Object: Unlabelled draw source`.
- Pedestrian: `Selected primitive: 2528`, `Object: Unlabelled draw source`.
- Building / road / foliage attempts: primitives 2875, 2735, 2791, 2908/2808,
  all `Object: Unlabelled draw source`.

Screenshots (cropped to the project panel, no game art):
[`evidence/m1-player-car-unlabelled.png`](evidence/m1-player-car-unlabelled.png),
[`evidence/m1-pedestrian-click-unlabelled.png`](evidence/m1-pedestrian-click-unlabelled.png),
[`evidence/m1-building-click-unlabelled.png`](evidence/m1-building-click-unlabelled.png).

The report that "player car, civilian car, police car and pedestrian body/face
fail to select while other objects export correctly" is reproduced for cars and
pedestrians. The control assumption that "other objects select fine" did **not**
hold in this scene: buildings, roads and foliage also returned unlabelled.

## Root cause (debugger evidence)

Breakpoint at `PsyX_GPU.cpp:1211` inside `ResolveInspectorPick`, click at
window (450, 300):

- `g_inspectorRangeCount = 349` — the registration mechanism is active and well
  under the 2048 cap, so this is **not** range-array saturation.
- `g_vertexIndex = 8814`, `windowWidth = 1280`, `windowHeight = 720`,
  `g_inspectorPickX = 450`, `g_inspectorPickY = 300` — the click maps correctly.
- Picked split: `startVertex = 8466`, `numVerts = 6`;
  `g_vertexInspectorRange[8466] = -1` and `g_vertexInspectorRange[8469] = -1`.
- The split is `textureOverridden = 1`, `inspectorSourceU = 128`,
  `dispenv.disp.w = 320`, `dispenv.disp.h = 240`, `texFormat = TF_4_BIT`,
  `textureId = 1`.
- Its six vertices project to normalized `x ∈ [-1.05, 1.77]`, `y ∈ [0, 1.07]`
  (a full-viewport quad), with vertex colour `r = g = b = 0x11` and `z = 0`.
- By contrast, sampled labelled vertices exist across the buffer:
  `g_vertexInspectorRange[2000] = 330`, `[4000] = 346`, `[6000] = 42`,
  `[8000] = 345`, `[8400] = 294`.

Interpretation: registration works (349 ranges, labelled vertices up to index
~8400), but a late, unlabelled, full-viewport flat quad is parsed after the
labelled scene geometry. Because the pick keeps the last match with no depth,
alpha or clip test, that quad wins for essentially every pixel and reports
`Unlabelled draw source`. The identical stale `Source region: 128, 128 — 56 x 58
(override source)` on every click is consistent with the override source globals
(`g_automaticOverrideSource*`, `PsyX_GPU.cpp:47-49`) being inherited by splits
that are not themselves an overridden material; that is a separate observation
to confirm in milestone 2.

**Producer identified:** `add_haze` in `src_rebuild/Game/C/debris.c:3529` draws a
full-screen untextured semitransparent `TILE` (`setTile` -> page 0 / CLUT 0,
`u = v = 0`, `x0 = -500`, `w = 1200`, `y0 = 0`, `h = 256`, colour `top_col`,
`addPrim(current->ot + ot_pos, ...)`) — the exact signature of the picked
primitive. In this scene it is called from the sun-flare haze path
(`sky.c:595`, `ot_pos = 7`, near the front), which is active whenever the sun is
visible. The death fade (`main.c:2400`, `gDieWithFade = 0`), weather
(`gWeather = 0`), the distant `debris.c:3909` haze (`ot_pos = 4222`, far) and the
loading/letterbox fades (`loadview.c`) were ruled out. `add_haze` is a screen
effect, not a world object; it is the first thing milestone 2 classifies and
excludes from world-object picking so that labelled world geometry becomes
reachable behind it.

## Recommendations

1. Milestone 2 first identifies the full-screen/overlay producer and either
   attaches an explicit non-object identity (so the panel can say "screen
   overlay" instead of "unlabelled") or excludes it from object picking. Then
   add component/parent identity for wheels (`cars.c`) and pedestrian parts
   (`motion_c.c`), reusing the catalog for resource identity and keeping the mod
   override triple untouched.
2. Milestone 4 must replace last-match picking with a method that respects
   depth (and cutout alpha) so a background quad can no longer shadow a nearer
   object; this is the direct fix for the reproduced failure.
3. Milestone 5 must keep a labelled fallback that names the unsupported path
   instead of silently returning "Unlabelled draw source".

## Alternatives considered

- **Bounding-volume/CPU ray cast against source models.** Source-aware and cheap
  to attribute, but needs per-category world transforms the inspector does not
  currently keep. Keep as the comparison arm for milestone 4.
- **ID/colour readback pass in PsyCross.** Gives true depth/alpha visibility, but
  touches the submodule and must be carried as a patch under `patches/psycross/`.
- **Attach identity to the full-screen quad as a world object.** Rejected: it is
  not a world object, and labelling it would make the panel lie.

## Open questions

- Should `add_haze` and other full-screen effects be excluded from picking, be
  reported as an explicit "screen effect" selection, or be selectable only when
  nothing world-space is under the cursor?
- Is the inherited `inspectorSource` region on non-overridden splits a rendering
  correctness bug or only a diagnostics artifact?
- For wheel and pedestrian-component identity, should the parent be the live
  slot instance (`car:<...>:<id>`) or the source resource (`model:<...>`)?
- Do HUD/2D and sky paths get an explicit "not a world object" classification in
  the panel, or stay out of the inspector entirely?

## Discussion history

### 2026-09-16 - Milestone 1 coverage inventory

Inventoried all producer paths (buildings, tiles/roads, car bodies, wheels,
pedestrians and articulated characters, heads, animated props, effects, foliage,
sky, HUD, and an unidentified full-screen quad) with an explicit selectable/
exportable/provenance state each. Reproduced the reported gaps in a Chicago
debug-start scene: player car, civilian car and pedestrian all return
`Object: Unlabelled draw source`, and building/road/foliage clicks do too. Using
the Visual Studio debugger at `PsyX_GPU.cpp:1211`, established that 349 ranges
are registered (no saturation) and that the picked primitive is a late
full-viewport flat quad with `g_vertexInspectorRange = -1`, which wins the
depthless last-match pick. Roadmap item 06 stays `planned`; milestone 2 starts
with identifying that quad and attaching component identities for wheels and
pedestrian parts.

### 2026-09-16 - Full-screen quad producer identified

Traced the milestone-1 root cause to `add_haze` (`debris.c:3529`): a
full-screen untextured semitransparent `TILE` (page 0 / CLUT 0, `x0 = -500`,
`w = 1200`, `y0 = 0`, `h = 256`, near `ot_pos`) added by the sun-flare haze
path (`sky.c:595`, `ot_pos = 7`) and by event/bomberman haze. This matches the
picked primitive's signature (page/clut 0, `u = v = 0`, full viewport, dark
colour). Milestone 2 therefore starts by classifying/excluding screen effects
from world-object picking, then adds wheel and pedestrian-part component
identity.

### 2026-09-16 - Milestone 2 component and parent identity

Added game-owned component identity to `assetcatalog.{h,c}`:
`AssetCatalog_MakeComponentKey` / `AssetCatalog_IsComponentKey` /
`AssetCatalog_ParseComponentKey` encode
`"<parentKey>/component:<kind>:<index>"`. A parent may not itself be a
component, the kind may not contain `/` or `:`, and parsing rejects malformed
keys, so a key is unambiguous and round-trips. Unit tests grew from 87 to
**121 checks, 0 failures**. The mod override key is untouched; components are
inspector identity only.

Producers now attach identity:

- **Wheels** (`cars.c`): `Cars_BuildInspectorKey` is shared by `DrawCarObject`
  and `DrawCarWheels`; every `DrawWheelObject` submission is registered as a
  `wheel` component of the car instance. Debugger (Chicago, `Release_dev`):
  parent `car:0:0:0:2:3`, kind `wheel`, index `0`.
- **Pedestrian parts** (`motion_c.c`): `Ped_BuildInspectorKey` uses the pool
  slot as the instance identity (`ped:<level>:<variant>:<slot>:<pedType>`);
  `DrawBodySprite` registers a `bone` component, `newShowTanner` registers each
  skinned bone, and `DoCivHead` registers `head`. Debugger:
  `ped:0:0:3:3/component:bone:2`, `modelName "Bone 2"` (civilian slot 3).
- **Animated props** (`objanim.c`): `DrawAllAnimatingObjects` registers the
  whole world placement (`anim:<modelId>:x:y:z:yang`) over the primitives it
  submits, reusing the building/tile model id.

The 3D Debug panel parses a component key and shows
`Component: <kind> #<index> of <parent>`, and skips the catalog-model message
for component keys.

The component identity is also present in the live pick index, verified with the
`Release_dev` debugger inside `ResolveInspectorPick`: for the player car's
`DrawCarObject` range (`modelIndex` 2) the next four registered ranges are the
wheels, 240 bytes each (six quads), and

- `strcmp((char*)g_inspectorRanges[361].object.key, "car:0:0:0:2:3/component:wheel:0") == 0`
- `g_inspectorRanges[361].label[0] == 'W'` ("Wheel 0 of Car #0"),
  `g_inspectorRanges[361].object.modelIndex == -1`
- ranges are contiguous: `g_inspectorRanges[360].end == g_inspectorRanges[361].begin`.

Remaining limitation, now precisely scoped: those wheel ranges exist with the
correct keys, but no vertices in `g_vertexInspectorRange` map to them (the
vertices after the body range resolve to `-1`), so a click on a wheel still
returns the car body. Fixing the packet-to-vertex mapping and the depthless
last-match behaviour is milestone 4 (visibility-aware picking). No gameplay
geometry, packet layout or animation was changed.

### 2026-09-16 - Milestone 3 selection scopes and lifetime

The 3D Debug tab now exposes a `Selection scope` selector (Face / Material /
Component / Logical object) built entirely from one pick - no geometry, packet
or exporter change:

- Face = `primitiveIndex`; Material = the resolved `(tname, page, index)`;
  Component = the parsed component key (`kind #index of parent`);
  Logical object = the parent key for a component, otherwise the object key.
- A pick captures an `AssetCatalogAnchor` (`AssetCatalog_CaptureAnchor` /
  `AssetCatalog_CheckAnchor`) holding the catalog generation and, for keys that
  embed `model:<level>:<variant>:<index>` (building/tile/anim and model-resource
  keys), a model handle. The panel reports `Lifetime: valid | stale | not
  model-backed (anchor generation N)`; a generation change, a freed slot or a
  reused slot reports `stale` instead of retargeting silently.
- Car keys are additionally anchored by slot and model
  (`car:<level>:<variant>:<id>:<model>:<number>`); a reused car slot is reported
  as `Car slot N now holds model M (anchor X): stale`.
- LOD survival is by construction: the building/tile key carries the source
  model id and the car key carries the slot id, both independent of the
  high/low detail branch.

In-game (Chicago, heading with the atmospheric quad off-screen so the picker is
not shadowed): clicking the player car reports
`Object: Car #0 | model 2 | high detail | palette 5`,
`Object key: car:0:0:0:2:3`, `Selected scope -> Component: this primitive has
no component identity.`, `Lifetime: not model-backed (anchor generation 1)` and
`Car instance: slot 0 | model 2 (live)` - the body is correctly not a component,
and the car anchor tracks the live slot.

### 2026-09-16 - Milestone 4 visibility-aware picking

First corrected a false lead from milestone 2. The wheel ranges were never
unreachable: adding a per-range vertex counter (`g_inspectorRangeVertexCount`,
exposed through `PsyX_Inspector_GetRangeInfo`) showed the four wheel ranges own
**30 vertices each**. The earlier `-1` samples looked at the wrong window,
because the vertex buffer is filled in ordering-table order, not in primitive
table order.

The real defect is that the geometry walk kept the *last* matching triangle with
no identity test, so a near-ordered screen-space overlay (the atmospheric haze
quad) shadowed the scene it is composited over. The pick now:

- prefers a candidate whose vertex maps to a registered range ("identified
  source") over unidentified geometry, while ordering-table arrival order
  remains the depth test between identified sources;
- projects the cursor back into the split's emulated display area so the
  viewport and display bounds are respected before the triangle test;
- keeps the existing PGXP-aware projection, so perspective correction and
  widescreen offsets are unchanged.

Chosen over an ID pass: a colour/ID render target would add a second framebuffer,
a per-primitive ID attribute and a full re-render per pick, while the
source-aware CPU method reuses the splits and vertices already produced for the
frame and can be checked against the same projection the renderer uses. The ID
pass remains the documented alternative if per-texel accuracy is ever required.

Debugger evidence (`Release_dev`, gameplay frame: 9036 vertices, 13 civilian
cars):
`PsyX_Inspector_RequestPick(640, 520)` then `ResolveInspectorPick` yields
`valid = 1`, `object.modelIndex = 2`,
`strcmp(object.key, "car:0:0:0:2:3") == 0`, `provenance[0] == 'C'` - with the
sun and its haze visible on screen. Before this change the same clicks returned
`Unlabelled draw source`.

Not covered yet, and reported rather than hidden: cutout alpha (PsyCross keeps
only the GL texture for an override, so there is no CPU alpha to sample) and
exact PSX clip-rect intersection (`drawenv.clip` is not a framebuffer clip rect
at this layer). The panel's new "Pick index" list marks every registered range
that owns zero vertices as *unreachable*, so an unsupported producer is visible
instead of failing silently.

### 2026-09-16 - Milestone 5 diagnostics and measured overhead

The 3D Debug tab gains a `Pick index (frame-local registered sources)` list
built on the new public `PsyX_Inspector_GetRangeInfo`: it reports the range
count, how many registered ranges own zero vertices (registered but
*unreachable*) and every range with its vertex count, so a producer that cannot
be picked is visible instead of silent. Clicking geometry no producer claimed
now states `Unsupported source: no producer registered a draw-source range for
this primitive.`, and a texture-overridden click states that cutout alpha was
not sampled.

Overhead is measured rather than assumed. `PsyX_Inspector_GetPickCostMicros`
times the pick with `SDL_GetPerformanceCounter` and the panel shows
`Last pick cost`. In gameplay (Chicago, 13 civilian cars, ~8.5-9k PSX
vertices/frame):

- panel closed: 16.67 ms (60 FPS);
- panel open: 34.22 ms (29.2 FPS) - the ~17.6 ms delta is the pre-existing
  Dear ImGui developer panel;
- pick: 1199 us at 1280x720, 970 us at 1084x611, 951 us with PGXP off - one-off
  per click, no measurable idle per-frame cost.

The window was resized to a 1084x611 client area and the pick still resolved
`car:0:0:0:2:3`, and the same key resolved with `pgxpTextureMapping=0` /
`pgxpZBuffer=0`, so the cursor-to-display mapping and the identified-source
preference hold across resize and both PGXP modes.

Still open for item 06 (so the record stays `planned`): an in-game click that
lands on a wheel or pedestrian part and returns the `wheel`/`bone` component key
(the component keys and their ranges are proven, the click itself is not), and
the picking policy for transparent holes, which needs CPU alpha for overrides
that PsyCross does not currently keep.

### 2026-09-16 - Component capture attempt and the case for a second scene

Rather than guessing pixels, the wheel ranges were located through the debugger
(the four `modelIndex = -1` ranges after the player car body, 30 vertices each)
and `PsyX_Inspector_GetRangeBounds` - a new diagnostic that reports the projected
screen bounds of a registered range - gave wheel 0 as normalised
`0.542-0.563 x 0.585-0.700`, i.e. window `~(694-720, 421-504)` at 1280x720.
Picking the bounds centre and the lowest visible rows all resolved to the **car
body**, not the wheel: from the chase camera the wheels sit behind the body
silhouette, so the body is genuinely nearer in ordering-table order. That is the
picking policy working correctly (nearest identified source wins); it means a
wheel click cannot be captured in this fixture and needs a scene where a car is
seen side-on.

LOD survival is now observed at runtime as well. Two different instances of the
same source model in one gameplay frame:

- range 324: resource id `model:0:0:1180`, world `(1472, -227136)`, **48** drawn
  vertices;
- range 326: resource id `model:0:0:1180`, world `(4256, -230016)`, **54** drawn
  vertices.

`strcmp` of the full keys returns non-zero (different instances) while
`strncmp(key324, key326, 23)` returns 0, so both selections carry the identical
`building:model:0:0:1180` resource id. The instances are 2880 world units apart,
so they straddle `DRAW_LOD_DIST_HIGH` and are drawn with different geometry, yet
the identity keeps the same source model - the LOD-independent part of the key,
seen at runtime.

LOD survival is also backed by code rather than assumption: `draw.c` picks the
high or low detail plot function but constructs the inspector key once from
`cop->type` through `AssetCatalog_MakeModelId` (`model:<level>:<variant>:<type>`),
and that id depends only on the source model index, so the same building or tile
keeps its key across a detail change. Cars key off the slot and model
independently of the `detail` argument.

Remaining before item 06 can be called done: a captured wheel or character
component click in a scene where the part is visible, and cutout-alpha sampling
so transparent holes fall through to the geometry behind.

### 2026-09-16 - Cutout coverage on the CPU

Item 01 discards override fragments whose alpha is below 0.5, but picking had no
GPU feedback, so a click on a transparent part of a foliage quad still selected
the quad. The picker now mirrors the renderer's coverage: textures created with
transparency keep a one-bit-per-texel mask (bounded to 32 masks of at most
256x256, freed with the texture), and a pick landing on an override interpolates
the vertex UV at the cursor and samples the mask with the shader's own mapping
(`tc = v_texcoord.xy / 255` over the source region), skipping any texel the
renderer would discard. `PsyXInspectorSelection.cutoutSampled` records whether
the coverage was consulted and the panel says so.

Runtime evidence: with the live override set, 42 overrides are registered and 8
cutout masks are retained; masks 1-3 contain genuinely transparent texels
(`bits[0] == 0x00`) while masks 0 and 4-7 are opaque at the 0.5 threshold, so the
CPU coverage matches real override images rather than being trivially all-set.
Picking the player car still returns `modelIndex 2` with `cutoutSampled = 0`,
confirming ordinary selection is unchanged.

The masks were also tied back to named overrides. The retained mask texture ids
are 8, 14, 15, 17, 19, 21, 22 and 23, and the override table shows
`g_textureOverrides[14]` (TREE01, `tpage 11`, `clut 20798`) with `textureId 21` -
which is exactly `g_cutoutMasks[5]`, so the CPU coverage belongs to real foliage
images and not to opaque level textures. Probing canopy and hillside pixels found
several overridden surfaces, but the ones reached used other page-11 CLUTs
(`clut 20606`) whose images are opaque, so `cutoutSampled` correctly stayed 0.

To settle whether the fall-through can be exercised at all, a diagnostic
(`PsyX_Inspector_FindCutoutSample`, surfaced as `Locate a cut-out texel`) now
scans the frame for override primitives whose sampled texel - at each vertex and
at the centroid - is cut out, and reports the point, its bounds, the owning range
and a count. In the Chicago fixture it reports **zero** cut-out points: no
visible surface there draws an override with transparency, so there is no pixel
to fall through. Mapping the retained mask texture ids to manifest entries by
creation order (override `n` uses textureId `n + 7`) shows why: the masks belong
to the car trim/window textures (FRNTLC1, SWINDW, SFRWIN, SDRTOP, SFRONT1),
TREE01, TREE02 and LIGHT, while the player car and the visible trees draw with
textures outside the override set (`tpage 14`; page-11 CLUTs whose images are
opaque).

The descriptor table then explained the miss precisely: the masked overrides
target specific palettes (`[8] SWINDW` = `tpage 11 / clut 20604`,
`[12] SFRONT1` = `11 / 20735`, `[14] TREE01` = `11 / 20798`,
`[15] TREE02` = `12 / 21052`, `[3] SRBOT1` = `10 / 20413`,
`[13] LINES` = `11 / 20797`), while the surfaces actually visible draw with
neighbouring CLUTs - the page-11 foliage at `clut 20606` sits 32 texels from the
`20604` palette, and the player car is `tpage 14 / clut 19070`. The overridden
textures belong to other geometry that this snapshot does not place in view.

That is the diagnostic fallback doing its job - reporting an unsupported case
instead of failing silently - but it also means the fall-through needs a snapshot
that actually draws one of those palettes. Until then the policy is verified at
the mechanism level only.

### 2026-09-16 - Component reachability measured, not guessed

Rather than guess pixels, a second diagnostic - `PsyX_Inspector_FindRangePixel`,
surfaced in the panel as `Locate pickable pixel` - runs the cursor's own
resolution over a 7x7 grid inside a range's projected bounds and reports a pixel
that actually resolves to that range. It shares `ResolvePickVertex` with the real
pick, so a hit cannot come from a different rule set.

This settled the vehicle case: for the player car's wheel range (30 vertices,
`modelIndex -1`) the locator finds **no** pickable pixel, i.e. the tyre is fully
covered by the car body from the chase camera. That is the defined occlusion
policy behaving correctly rather than a coverage gap.

The vehicle case is now measured rather than assumed. Scanning the frame's range
list finds exactly one wheel run (ranges 365-368, straight after the car body at
range 364 with `modelIndex 2`, the player car); every other sampled range
(330-358) is a level or scenery model. `FindComponentPixelMatching
("/component:wheel:")` scans a 13x13 grid inside each wheel range's projected
bounds and returns -1, so no wheel pixel is reachable: only the player car is
drawn in the high-detail branch, which is the only branch that calls
`DrawCarWheels`, and its tyres sit behind the body from the chase camera. A wheel
click needs a fixture or camera with a car side-on; identity, range and pick
preference for wheels are all verified.

It also settled the character case: for a pedestrian part the locator did find a
pickable pixel, `(360, 350)`, so character components are resolvable. That
pedestrian then walked out from under the cursor before the pick resolved, and
the pedestrians in this snapshot are otherwise sub-pixel slivers - one part's
projected bounds measured 0.4 px by 8.6 px - so no click was captured.

The cutout policy was then demonstrated rather than asserted. A gameplay frame
reports 174 masked override primitives and 30 cut-out texels on screen, so the
case is present in the fixture after all (earlier frames drew no masked surface).
`PsyX_Inspector_FindCutoutFallThrough` looks for a cut-out pixel where the pick
finds nothing behind; it found none, because in this scene an identified source
is always behind, so the identified-source preference decides those pixels either
way.

Direct comparison settles it instead. Adding a gate
(`g_inspectorCutoutPickingEnabled`, panel `Skip cut-out texels when picking`) and
picking the same pixel `(1130, 288)` twice gives:

- cutout **off**: primitive **2387**, `textureOverridden = 1`, key empty - the
  cut-out override primitive is selected;
- cutout **on**: primitive **2324**, a labelled building behind it.

The selection changes and the only difference is the cutout check, so a
transparent override texel is skipped and the geometry behind resolves. That is
the policy captured end-to-end.

A third diagnostic closed the loop. `PsyX_Inspector_FindComponentPixel`
(`Pick first reachable component`) scans component ranges and returns the first
one that owns a pickable pixel. Called at the pick call site, then requesting
that pixel and resolving in the **same frame** (request while paused at the call,
remove that breakpoint, continue into the resolver), produced a real character
component selection:

- `object.key` starts `ped:` and contains `/component:bone:`
- `object.modelName = "Bone 6"`, `object.modelIndex = -1`
- provenance `"Bone 6 …"`, `PsyX_Inspector_GetSelectionRangeIndex() = 3`

That is the character half of "characters and vehicles can select the logical
whole or a component" captured end-to-end: vehicles give the logical whole
(`car:0:0:0:2:3`) and characters give a bone component.

Pedestrian registration is confirmed live at the same time: a breakpoint on
`motion_c.c:647` (`DrawBodySprite` -> `Ped_RegisterComponent`) shows
`g_inspectorRangeCount` going 0 -> 1 for ped slot 6, and the frame's leading
ranges all carry `'p'` keys. So identity, registration and pickability are all
demonstrated; only a captured click on a suitably placed part is missing.

Inspected revision: `master` worktree with the project-PsyCross patch applied
(the `src_rebuild/PsyCross` submodule is locally modified: 6 tracked files plus
untracked `third_party/`), 2026-09-16. Unrelated local `data/` changes were
preserved and not touched.
