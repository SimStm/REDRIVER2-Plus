---
type: Evidence
title: E02 Chicago slice inventory reconciliation
description: Scoped final diagnostic inventory for the Chicago day slice.
tags: [okf, engine, assets, inventory, e02]
---

# E02 Chicago slice inventory reconciliation

Final scoped run (2026-10-09, `feat/new-engine`, fresh SDK-present x64
`Release_dev`, NTSC data). Executable was the isolated
`src_rebuild/bin/e04_gate4/REDRIVER2_dev.exe`; working directory was
`src_rebuild/bin/Release_dev`:

```
REDRIVER2_dev.exe -opengl -nointro -nofmv -mission 1 -assetinventory
```

The report is published atomically to
`<working dir>/assetcatalog_level_0_inventory.txt` after `SpoolSYNC()` in
`State_GameInit`; the flag is a developer tool and a failed report only warns.

The process ran for 25 seconds; the fresh report was inspected/copied, and
`CloseMainWindow` returned a normal process exit code 0. The report copy is
`src_rebuild/build_e04_gate4/logs/chicago_inventory_final.txt`.

## Final scoped diagnostic result (2026-10-09): `reconciliation: OK`

Gate 4A attempt 2 was **approved as a scoped diagnostic only** after fix-4.
The fresh parent integration report below is recorded evidence; `reconciliation:
OK` does not approve full source coverage, conversion or native-world behavior.

| Metric | Value |
|---|---|
| eligible / scanned / skipped region barrels | 4 / 4 / 0 |
| placements | 6740 (all unique) |
| overlap warnings | 0 |
| shared (region-straddling) placements | 227 |
| registered / referenced models | 516 / 150 |
| missing model refs | 0 |
| unreferenced registered models | 366 |
| registered / referenced textures | 366 / 13 |
| unknown material classifications | 114 |
| barrel placements / capacity | 989/2816, 1612/3072, 1913/2816, 1999/2816 |
| findings | 0 errors, 480 warnings (64 displayed; truncated) |

## Interpretation and limits

- `cell_objects_add` deltas are per-barrel capacities, not expected counts.
  The loader walks cell lists; source straddlers are decoded with the referencing
  cell's `nearCell`. The pure walker checks barrel spans, terminators and
  ownership, error totals/retention, and slot-material lifecycle.
- The corrected walker visits the terminal object marked `0x8000`, using its
  lower 14-bit model reference. This explains why the initial 6078-placement
  report omitted terminal objects; those counts are superseded, not an unchanged
  coverage result.
- Missing catalog material links are UNKNOWN, not proof of untextured geometry.
  Union default/event geometry overlaps are warnings, not proven duplicate
  source corruption.
- This checks the source-union diagnostic for the resident Chicago day slice,
  not conversion completeness or playable/native-world acceptance. Other
  cities/variants and dynamic/streamed categories remain E06 coverage work.
- The earlier startup hang was diagnosed: unknown `-assetinventory` opened the
  modal command-line helper in `main.c`; it was not environmental launch
  flakiness. The explicit argument and help-text fix was verified with build
  exit 0 and a first-run report; the final report above came from the isolated
  x64 integration executable.

## Follow-up source and resident-world audit (2026-10-11)

The four-barrel inventory above is the Chicago day resident union, not the entire city. It reports
6,740 unique placements, 516 registered model slots, 150 referenced models, no missing model refs,
and 114 referenced models with no catalog material link. Those 114 classifications are unknown;
they do not prove absent texture assets. The PL10/PL11 texture-key path now uses
`ModelTextureLink_GetTextureKey` in `texture.c`, shared with production catalog classification.

The current `model_ptypes_level_0.txt` lists 5,538 records over 516 loaded model slots. Its only
out-of-decoder-allowlist records are ptype 9 at model slots 1030 and 1176 (one record each). The
report classifies both as buildings (`shape_flags` 0x0001 and 0x8001), with nonzero 12-byte stride,
no invalid spans and no zero-stride records. Legacy `PlotBuildingModel` draws only {11,21,23}
after `ConvertPolygonTypes`; these ptype 9 faces are skipped there too. The building decoder's
`SkipUnsupported` behavior matches that legacy skip, so these two records are not evidence of lost
legacy-visible geometry. Tile/sprite callers remain strict and are not authorized to skip them.

The probe's `catalogMaterialRecords` counter now calls the same
`ModelTextureLink_GetTextureKey` helper used by catalog linking, so its classification includes
ptype 10/11 as well as 4-7 and 20-23. This counts polygon records carrying a key, not successful
texture resolution. The checked-in `model_ptypes_level_0.txt` predates this correction; its old
812 counter is stale and no updated aggregate has been generated yet.

Runtime counts refer to different stages and snapshots. In `draw-distance-1800-tick30-run.log`,
the four-barrel source yielded 6,740 placements, 160 converted resources, then 233 camera-selected
instance records; 186 were drawn in the logged frame. The wheel/HUD session later showed 6,740
placements and 156 CPU-selected instances. `async-world-corrected-run.log` records later live
region sets of 8,594 placements / 206 resources and 10,167 / 185 resources, each with zero
conversion failures, unresolved submissions or truncation. Those dynamic sets include selected
detail/LOD variants and a different resident region union; they are not additive to the 6,740
day-inventory total or proof that all Chicago sectors/cities are covered. `NATIVE_MAX_INSTANCES`
is 16,384. These logs show no concrete missing model reference or conversion failure in the sampled
sets; there is no per-city or alternate-variant source inventory yet. Next useful bounded coverage
work is to run and reconcile the same source inventory for each city/variant before attributing a
visual gap to decoder or material policy. This audit corrected the probe classification and made no runtime changes.
