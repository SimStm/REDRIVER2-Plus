# Mods

This directory is the user-owned home for REDRIVER2-Plus mods. Mods must never
modify files under `data/`; when a mod is disabled or removed, the original
game assets remain the fallback. The runtime first looks for `mods/` in its
current working directory, then for `../mods/`; this supports both launches
from the install root and the usual development launch from `data/`.

## Layout

Each mod has its own directory and a `manifest.json`:

```text
mods/
  enabled.json
  water-hd/
    manifest.json
    assets/LEVELS/CHICAGO/PAGE_12/SEA.png
  car-x/
    manifest.json
    assets/CARS/CCARS/car-x-body.png
```

Folder names are for humans. The manifest `id` is the stable identifier used
by the load-order file and must contain only letters, numbers, `-`, and `_`.

## Enabling and ordering mods

Copy `enabled.example.json` to `enabled.json`. Entries are evaluated from top
to bottom. If two enabled mods match the same texture, the later mod wins.
When `enabled.json` is absent, every valid JSON mod is enabled in alphabetical
order by id. A malformed `enabled.json` leaves JSON mods disabled and reports
the error in the Developer Graphics Panel.

## Texture manifests

Copy `template/manifest.example.json` to your mod directory as `manifest.json`.
The current texture pipeline recognizes:

- `id`, `name`, and `description` for mod metadata;
- `textures`, a list of texture overrides;
- `texture` (or `name`) for the exact, case-sensitive `TEXINF` name;
- `file` for a PNG relative to the mod directory;
- optional `texturePage` and `textureIndex` to narrow an otherwise ambiguous
  texture name.

The physical path can mirror a useful game-oriented layout, but it is not used
as an implicit override key. Packed and streamed assets, especially car data,
cannot safely be identified from a filename alone. Declare the resource in the
manifest instead. The renderer additionally verifies page, CLUT, and the full
UV rectangle before applying an image.

Override draws use an alpha cutout: fragments below 0.5 alpha are discarded, so
transparent PNG regions no longer render black or occlude the geometry behind
them. This is a cutout, not conventional smooth alpha blending; the primitive's
original PSX blend mode is unchanged, and exported originals still encode only
fully transparent versus opaque coverage.

This first resolver is connected to the level `TEXINF` texture-page loader.
It does not yet replace packed `CCARS.RAW` vehicle models or their spooled
texture data; the `car-x` layout is reserved for that next asset-resolver step.

All asset paths must be relative to their mod directory. Absolute paths,
drive-qualified paths, and paths containing `..` are rejected.

PNG decoding is currently available in the Windows desktop build. Other
platforms discover manifests for diagnostics but retain the original texture.

## Legacy compatibility

`mods/hd_textures/manifest.ini` remains supported only when no JSON mod
manifests are found. Migrate it to a dedicated JSON mod when practical.

The `3D Debug` tab in the Developer Graphics Panel lists the discovered mods,
their effective load order, and declared texture resources. Enable **Pick
visible primitive** and left-click outside the ImGui windows to inspect the
last matching PSX triangle in the processed draw batch. Picking is a
draw-stream approximation: it does not reproduce depth, alpha, or scissor
tests. The inspector reports texture
page, CLUT, UV coordinates, resolved `TEXINF` name where available, active
override, and provenance. Car bodies and city tiles currently provide model
provenance; buildings now also associate their submitted geometry with their
source model. Other renderer paths still provide their GPU resource details.

Selected level textures can be exported as PNG to
`mods/<id>/assets/inspector/`. Exporting into a new id creates a minimal
manifest. Exporting into an existing mod appends the new `(texture,
texturePage, textureIndex)` registration when it is missing, preserving all
existing entries, unknown fields, and user edits; re-exporting an already
registered texture replaces only its PNG. A manifest whose `textures` array
cannot be read is reported and left untouched, and the created PNG is still
announced. Use **Reload mod manifests and images** to apply PNG or manifest
edits to texture pages already loaded by the level.

Selected car bodies can also be exported as OBJ under `assets/models/`. OBJ
import is intentionally not enabled yet: vehicle data is packed and needs a
validated conversion step before it can safely replace a runtime car model.

### Inspector selection and export controls

- The panel displays the absolute mods root. The loader uses `mods/` in the
  process working directory, falling back to `../mods/` when that directory
  exists. If neither exists, exports create `mods/`.
- Export buttons remain visible. Disabled buttons explain missing texture
  registration, unsupported object types, or platform restrictions.
- PNG exports contain the entire registered original VRAM texture, not the
  clicked triangle's UV crop or the loaded HD replacement. Clicking export
  again replaces the previous PNG/OBJ/TXT at that path. Back up edited assets
  before dumping original assets over them. Failed writes preserve the old
  destination; successful PNG writes are decoded and checked before replacing it.
- **Export selection details (TXT)** writes
  `mods/<id>/assets/inspector/selection.txt`, replacing an existing report.
  **Copy selection details** copies the same runtime identifiers,
  UVs, texture region and override paths to the clipboard. Reports do not
  create mod manifests.
- Coloured fill, wire edges and a label mark the selected labelled draw
  source, refreshed from submitted triangles each frame (up to 4096).
  This is a non-depth-tested overlay, so hidden portions can show through.
  Unlabelled selections can only highlight their clicked triangle for the
  capture frame. No geometry is shown if the labelled source is not submitted.
- Buildings and tiles are keyed by level, source model, position and rotation,
  rather than primitive index or current LOD. Identical coincident instances
  share a key. The source pack model name is shown when present; it is not an
  archive filename. Model-name offsets are indexed once with lump-size bounds.
- Car positions track a live car slot, independent of LOD. Reselect after
  car-slot reuse. Parsing a new texture set clears the old selection. These
  keys are not globally persistent asset IDs.
- **Textures on the selected object** lists up to 128 registered texture/palette
  bindings found in captured triangles, refreshing at most four times per
  second. Each item can export its full original texture. This is not an
  exhaustive material list from the archive: culled, unregistered and
  uninstrumented geometry is not included.
- The image preview shows an already loaded override, not original VRAM.
  An orbitable model preview, general model export/import, original texture
  preview and source-archive filename tracking are not implemented.
- Text wraps within the developer window. For long controls, widen the panel;
  ImGui button and checkbox labels themselves are not paragraph widgets.
