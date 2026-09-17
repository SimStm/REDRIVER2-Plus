---
type: Product
title: Whole-object selection across renderer categories
description: How the 3D Debug inspector identifies, scopes, retains and picks drawn sources across renderer categories.
tags: [product, inspector, picking, identity, mods]
---

# Whole-object selection across renderer categories

The 3D Debug tab can resolve a clicked primitive to the source that produced it,
present that source at four scopes, retain it across LOD changes and report when
it becomes stale. Selection and picking UI live in the **3D Debug** tab; mod
information stays in **Mods**.

Implementation: game-owned identity in
`src_rebuild/Game/C/assetcatalog.{h,c}` and the producers in `cars.c`,
`motion_c.c`, `pedest.c`, `objanim.c`, `draw.c` and `tile.c`; the panel in
`src_rebuild/utils/DeveloperGraphicsPanel.cpp`; picking, cutout coverage and the
diagnostics in `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp` (carried by
`patches/psycross/developer-overlay.patch`).

## Coverage

A producer registers a copy of the object key plus a human label over the
primitive-memory range it just submitted. Identity is carried by:

- **Buildings and tiles** - `building:<model id>:<x>:<y>:<z>:<yang>` and
  `tile:...`, built from `cop->type`, so the key is identical for the
  high-detail and low-detail plot branches.
- **Animated props** - `anim:<model id>:<x>:<y>:<z>:<yang>` over the placement.
- **Cars** - `car:<level>:<variant>:<id>:<model>:<modelNumber>` for the body of
  every drawn car.
- **Wheels** - `<car key>/component:wheel:<n>`, one range per wheel drawn by
  `DrawCarWheels`. Only cars in the high-detail branch register wheels.
- **Pedestrian parts** - `ped:<level>:<variant>:<slot>:<pedType>` for the
  instance and `<parent>/component:<bone|head>:<n>` for the parts registered by
  `DrawBodySprite`, `newShowTanner` and `DoCivHead`.
- **Sprites, including trees** - `sprite:<model id>:<x>:<y>:<z>` over the
  billboard `DrawSprites` plots, which is a subdivided mesh for nearby sprites
  and a single tile beyond its subdivision distance. The ground shadow of a
  sprite is intentionally left outside the range.
- **Unlabelled** - screen-space effects such as the sun haze, car FX and any
  geometry no producer claimed. These are reported by their render label only.

Model names come from the level name table and are only reported when the entry
is printable ASCII; some sprite-only entries are not text and resolve as an
unnamed model instead.

## Scopes

One pick can be read at four scopes, chosen with the `Selection scope` control:

- **Face** - the clicked primitive.
- **Material** - the resolved texture (name, page, index) and its source region.
- **Component** - the parsed `component:<kind>:<index>` and its parent, when the
  primitive carries one.
- **Logical object** - the parent key of a component, otherwise the object key.

## Lifetime

A pick captures an `AssetCatalogAnchor`: the catalog generation and, for keys
that embed `model:<level>:<variant>:<index>`, a model handle. The panel reports
`Lifetime: valid | stale | not model-backed`. A level change, a freed slot or a
reused slot reports `stale` instead of silently retargeting. Car keys are also
anchored by slot and model, so a reused car slot is reported as
`Car slot N now holds model M (anchor X): stale`.

Because the key uses the source model id rather than the drawn LOD, a retained
selection survives a detail change; only the instance going away invalidates it.

## Picking policy

The pick walks the completed draw stream, which is ordered by the ordering table
from far to near:

- the **last** matching triangle at the cursor is the nearest one, so arrival
  order is the depth test;
- a candidate whose vertex maps to a **registered source** is preferred over
  unidentified geometry, so screen-space overlays and effects cannot shadow the
  scene they are composited over;
- the cursor is projected back into the split's **emulated display area**, so
  the viewport and display bounds are respected;
- the existing **PGXP** projection (perspective correction, widescreen offsets)
  is used unchanged, and the path works with PGXP enabled or disabled;
- **override cutout** is mirrored on the CPU: textures created with transparency
  keep a one-bit-per-texel coverage mask, and a pick that lands on an override
  samples that mask at the cursor with the shader's own mapping, so texels the
  renderer discards are skipped and the geometry behind resolves instead.

The PSX clip rectangle is not modelled at this layer, and there is no CPU alpha
for textures that were created fully opaque.

## Diagnostics

- **Pick index** lists every frame-local registered source with the number of
  parsed vertices it owns, and counts those that own **zero** vertices as
  *registered but unreachable*, so an unsupported producer is visible instead of
  failing silently.
- Clicking geometry no producer claimed states
  `Unsupported source: no producer registered a draw-source range for this primitive.`
- `Locate pickable pixel` / `Pick first reachable component` /
  `Pick first reachable wheel` run the same resolution the cursor uses over a
  grid inside a source's projected bounds and report a pixel that actually
  resolves to it, which separates "occluded" from "hard to aim at".
- `Locate a cut-out texel` reports masked override primitives and cut-out texels
  on screen; `Skip cut-out texels when picking` toggles the cutout rule so its
  effect can be compared directly; `Locate a cut-out fall-through` looks for a
  cut-out texel with nothing behind it.
- **Last pick cost** reports the CPU time of the last resolved pick.

## Measured behaviour

Gameplay, 1280x720, Chicago day drive (13 civilian cars):

- frame time with the developer panel closed: 16.67 ms (60 FPS); with the panel
  open: 34.22 ms (29.2 FPS) - the difference is the pre-existing Dear ImGui
  developer panel;
- pick cost: 1199 us at 1280x720, 970 us at 1084x611 and 951 us with PGXP
  disabled - a one-off cost per click, with no measurable idle per-frame cost;
- resizing to a 1084x611 client and disabling PGXP both keep the same selection;
- clicking the player car with the atmospheric haze on screen resolves
  `car:0:0:0:2:3`; before the identified-source preference the same click
  returned `Unlabelled draw source`;
- a pedestrian part resolves `ped:.../component:bone:6` (`modelName "Bone 6"`);
- a cut-out foliage texel, with the cutout rule off, selects the override
  primitive itself and, with it on, selects the labelled building behind it.

## Whole-object selection

A single click in pick mode selects the part under the cursor. A double-click
resolves the same pick but keeps the **parent instance** of the clicked
component: the component suffix (`/component:<kind>:<index>`) is stripped from
the key, `wholeObject` is set on the selection, and highlighting matches every
range whose key has that prefix. A double-click on a car wheel therefore
highlights and exports the whole car (measured: 136 triangles across body and
wheels, against the single wheel range of 30 vertices), and a double-click on a
pedestrian part highlights every part of that pedestrian. The flag applies to
the pick it was issued with and is consumed by the first resolution.

Keys stay producer-owned: PsyCross cuts the key textually at the component
marker and keeps no knowledge of the catalog's key format.

## Export metadata and layout

Every manifest entry written by the inspector carries the texture's identity
triple plus descriptive metadata that never participates in override matching:

- `type` - derived from the object key prefix (`buildings`, `sprites`, `cars`,
  `pedestrians`, `tiles`, `props`, otherwise `other`);
- `level` - the current level name (`chicago`, `havana`, `vegas`, `rio`),
  lowercased with the same sanitizer as the type;
- `modelReferences` - unchanged.

Type and level are stored as lowercase, path-safe tokens. With the developer
panel's **Organize exports by type and level** setting enabled (default off),
files are written to `assets/inspector/<type>/<level>/` and the manifest's
`file` value records that relative path; with it disabled, files stay in
`assets/inspector/`.

## Export palette (base colours)

Cars and pedestrians are drawn through runtime palettes (`civ_clut`), so which
colours a PNG carries is an explicit choice, controlled by the **Export textures
in base colours** developer setting (default **enabled**, and independent of
where the file is written):

- **enabled** - the export substitutes the palette the level registered for the
  exported region, so the PNG carries the original artwork colours. This is the
  correct input for a recolouring mod, because the game still selects a runtime
  palette and re-maps the modded image through it;
- **disabled** - the export uses the CLUT of the clicked primitive, so a car or
  pedestrian is exported with the colours that instance currently shows. The
  same texture can then produce a different PNG per instance.

The substitution is applied inside the export core for every path (single
export and both batches), by matching the exported region against the registered
known textures on the same page and taking the smallest containing entry. When
no registered entry contains the region, the primitive's palette is used
unchanged.

## Known limitations

- **Wheels cannot be clicked in the chase-camera fixture.** Identity, the
  30-vertex range per wheel and the pick preference are verified, and the
  locator confirms no wheel pixel is reachable because only the player car is
  drawn in the high-detail branch, where the tyres sit behind the body.
- Pedestrian parts are only clickable when the part is large enough on screen;
  distant pedestrians project to sub-pixel slivers.
- The PSX clip rectangle is not intersected when picking; only the display area
  is checked.
- Override cutout coverage is retained only for textures up to 256x256 that were
  created with transparency.
