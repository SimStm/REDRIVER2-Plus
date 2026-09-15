# Legacy HD texture overrides

This folder is the original single-manifest HD texture location. It is kept as
a compatibility fallback and is only read when no `mods/<id>/manifest.json`
files are found. New mods should follow the JSON layout documented in
[`../README.md`](../README.md).

It is intentionally outside `data/`: do not copy, modify, or redistribute
original game files here.

The original vertical slice replaces named level texture regions through
`mods/hd_textures/manifest.ini`. The manifest is read once while a level's
texture pages are loaded. The initial target is `SEA`, a stable `TEXINF` name
used by the level texture pipeline.

## Manifest format

Copy `manifest.example.ini` to `manifest.ini`, then put your PNG beside it:

```ini
version=1

[texture_overrides]
SEA=sea.png
```

Keys are exact, case-sensitive `TEXINF` names. Values are relative PNG paths
inside this folder. Absolute paths, drive-qualified paths, and paths containing
`..` are rejected. A manifest can list more names later with one `NAME=image.png`
entry per line; duplicate names are resolved in manifest order.

The renderer only applies an entry when the current primitive matches the
texture's resolved PlayStation texture page, CLUT, and complete `TEXINF` UV
rectangle. This prevents a same-name or overlapping page from being replaced
accidentally. The original compressed page, TIM override (when present), and
CLUT upload still run first. If the manifest, entry, PNG, or renderer mapping is
unavailable, the original VRAM texture is drawn.

PNG decoding in this vertical slice is implemented with Windows Imaging
Component, so it is available in the Windows desktop build. Other platforms
keep the fallback and report the limitation in the Developer Graphics Panel.

## Testing

1. Create `mods/hd_textures/manifest.ini` from the example.
2. Create your own `mods/hd_textures/sea.png` as an RGBA PNG. Any resolution is
   accepted up to 16384 by 16384 pixels; a power-of-two image is a good first
   choice.
3. Start a level that renders water and open the Developer Graphics Panel with
   F11. It reports whether the manifest was found, how many PNGs were loaded,
   and how many renderer mappings are active.
4. Toggle **Enable HD texture overrides** to compare the image with the
   original texture immediately. Editing `manifest.ini` or the PNG requires a
   level reload or application restart because assets are registered while
   texture pages load.

The runtime switch is saved in `developer_graphics.ini`; the manifest is not
rewritten by the game.
