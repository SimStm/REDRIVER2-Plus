---
type: Plan
title: Engine migration future extensions
status: planned
created: 2026-10-08
tags: [engine, studio, rmlui, editors, physics]
---

# Engine migration future extensions

These user-adopted directions are captured under the engine-migration umbrella;
they are not implemented, scheduled blockers or permission to change gameplay.
Create a separate lifecycle record when a concrete extension is commissioned.

- **FMOD Studio:** add authored banks/events/parameters to E04's stable game
  audio contract. Validate bank versions/reload, missing events, RPM transitions,
  mission/cutscene synchronization and backward-compatible original sound IDs.
- **RmlUi:** use a native Diligent rendering/input adapter for authored menus/HUD.
  Preserve controller navigation, UI input capture, localization, resolution and
  readability. Existing basic native UI may satisfy E05 without this extension.
- **In-game authoring:** map/model/material/sound/mission/cutscene tools consume
  readable package metadata and source identities. Viewing/exporting is not
  editing or safe re-import. Define transactional save/undo, schema validation,
  gameplay/collision/streaming consequences and original-format round trips.
  Reuse OpenDriver2Tools format knowledge; never overwrite the user's original
  installation automatically. RmlUi is not a requirement for developer editors.
- **Supplementary physics:** start with cosmetic rigid bodies/particle queries
  isolated from authoritative car/collision logic and gameplay RNG. Define
  contact/query ownership and one solver per body. Changes to car physics or tick
  frequency require separate baselines, input/replay versioning and explicitly
  approved gameplay differences; fixed ticks alone do not make them compatible.
- **Modern assets/materials:** authored high-detail/PBR replacements use E02 IDs
  and mod precedence. Conversion itself does not create new detail, correct
  roughness/metalness maps or collision suitable for new geometry.

Each extension needs a bounded objective, concrete dependencies, supported-target
contract and reproducible functional/performance tests before it is called ready.
