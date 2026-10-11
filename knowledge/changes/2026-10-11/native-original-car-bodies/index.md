---
type: Change
title: Resident original car body submission and source texture links
date: 2026-10-11
tags: [okf, engine, diligent, rendering, assets]
---

# Resident original car body submission and source texture links

The native resident-world renderer now submits clean original car body meshes with live per-car
scene transforms. The source asset catalog also extracts texture keys for declared PL10/PL11
polygon links through the same helper used by production classification. The texture fix improves
source identity coverage; it does not resolve every unknown material classification.

## Validation

- `NativeCarMeshTests` focused executable passed. `scripts/run_inspector_tests.ps1` passed all 19
  suites, including the production-linked PL10/PL11 texture-key regression.
- Native renderer smoke had passed 1249 checks with zero failures during the orientation-shading
  milestone. After the final diagnostic-latch change, isolated x64 `Release_dev_native` and
  reference `Release_dev` solution builds both passed.
- Chicago Take A Drive launched with `-nointro -nofmv -mission 1 -drawregion`. Logs showed
  14/14 active car bodies ready and submitted, no hidden/waiting/failed/missing bodies, and a
  published world with 200/200 resources, 8,876 placements and zero failed/unresolved/truncated.
- RenderDoc 1.41 capture `native-car-pose-20261011.rdc` replayed 1,036 draws and 6,706 triangles
  with zero debug messages. The inspected PNG shows the player body, traffic, road and buildings.
  Steady samples between ticks 2,481 and 2,903 reported 30/31 ticks and zero drops; capture/debug
  pauses are excluded.
- The final native/reference builds passed; the reference OpenGL startup was not run because the
  Visual Studio configuration MCP call hung. Four probe profile/INI files were restored and their
  hashes matched the backups.

This is bounded Chicago TAD evidence, not full city or engine-migration acceptance. Wheels, damage,
pedestrians, story missions, on-foot views, additional cities and complete scene parity remain open.
