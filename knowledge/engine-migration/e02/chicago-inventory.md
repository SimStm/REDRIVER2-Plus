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
