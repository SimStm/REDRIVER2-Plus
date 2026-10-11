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

## Follow-up: dynamic wheels and diagnostic HUD

The text-only `RED2_NATIVE_VISUAL_DEBUG=1` HUD runs on the existing overlay independently of F9
and does not capture input. Wheel positions update from current actor transforms; source snapshots
are owned CPU data, workers do not dereference live models, and generation/revision changes reject
stale work. Dynamic uploads validate finite vertices/counts before bounds are committed. Legacy
constant-black wheel quads are preserved and participate in source cache identity.

Focused `NativeCarMeshTests` passed with zero failures; `NativeRendererSmoke` passed 1264/0. Both
`Release_dev_native|x64` and `Release_dev|x64` builds passed. The corrected native Chicago capture
replayed 350 draws with zero RenderDoc validation messages. Its HUD reported 160 world resources,
6740 placements, 13 active/ready/submitted cars, and 52 ready/submitted wheels; the inspected PNG
shows traffic with two visible tires. Steady samples recorded zero dropped ticks. The run reported
`playerCar=-1`, so player identity remains unproven. A separate reference OpenGL startup succeeded
and logged OpenGL 3.3. Pedestrians, damage and broader city coverage remain outstanding.
