# PsyCross patches

These patches carry REDRIVER2-Plus-specific PsyCross changes without requiring
a fork of the entire upstream submodule.

After cloning the repository, initialise the submodule and apply the patches:

```powershell
git submodule update --init --recursive
powershell -ExecutionPolicy Bypass -File scripts/apply_psycross_patches.ps1
```

`developer-overlay.patch` is based on PsyCross commit
`e56e4cde1c2b8a15e0d4e38b26cdd9202e0d17e6` and is applied automatically by
`scripts/apply_psycross_patches.ps1`. Besides the developer overlay,
it provides the reusable public RGBA texture-region override API and the
frame-local primitive inspector API used by the mod and 3D-inspector tools.
Inspector producers can register a copied object key and model metadata,
independent of LOD/display labels. Captured triangles expose source texture
regions for the selected-object texture list. The renderer remains independent
of game model types and mod-directory policy.

Picking resolves candidates from the completed draw stream. Because the stream
is walked in ordering-table order, arrival order is used as the depth test, but
an identified source (a producer that registered a primitive range) is preferred
over unidentified geometry, so screen-space overlays and effects can no longer
shadow the scene they are composited over. The cursor is also projected back
into the split's emulated display area so the viewport and display bounds are
respected. `PsyX_Inspector_GetRangeInfo` exposes each frame-local range and the
number of parsed vertices it owns; a range with zero vertices is registered but
unreachable, which callers report instead of failing silently.
`PsyX_Inspector_GetPickCostMicros` reports the CPU cost of the last resolved
pick so the one-off cost can be measured next to the frame statistics.
`PsyX_Inspector_GetSelectionRangeIndex` and `PsyX_Inspector_GetRangeBounds`
expose the projected screen bounds of the selected source, so a source that is
registered but not reachable (or that is occluded) can be located and reported.

Picking mirrors the override cutout on the CPU: textures with transparency keep
a one-bit-per-texel coverage mask (bounded, retained only for non-opaque images),
and a pick that lands on an override samples that mask at the cursor using the
shader's override UV mapping, so texels the renderer discards are skipped
instead of selected. `PsyXInspectorSelection.cutoutSampled` reports whether the
coverage was consulted.

Override draws enable an explicit cutout: the 32-bit RGBA shader discards
fragments whose alpha is below 0.5 only while a region override is active.
Original PSX sampling and the high-resolution font/texture path keep their
previous alpha behaviour, so disabling overrides restores the original
renderer state without leaking the cutout uniform.

Fully opaque overrides are created with mipmaps (`GR_CreateRGBATextureMipmapped`)
to reduce minification shimmer; overrides with transparency use plain
filtering to avoid mip-averaging alpha bleed into cutout edges.
The script refuses to apply it to another revision and succeeds harmlessly
when the patch is already present.
