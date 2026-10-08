---
type: Change
title: Preserve native GPU fixture failure in process status
created: 2026-10-08
tags: [okf, rendering, tooling, validation]
---

# Preserve native GPU fixture failure in process status

The fixture handler discarded `RunVulkanSelfTest`/`RunFixture` results and the
game exited0 even for initialization failure. An explicit result enum now
distinguishes unrelated command lines, successful fixtures and failed fixtures;
the game propagates failure as exit1 before ordinary startup.

Direct MSBuild and native/PSX GPU suites pass. A real missing-driver negative
control exits1 with a fresh explicit diagnostic; after restoring process-local
loader variables, the new native process exits0 with the full GPU PASS report.
Excluded PSX/Android/Emscripten host branches compile. See the
[executed evidence and limits](../../../rendering/r03-fixture-exit-validation.md).
R03 remains incomplete; no scene fallback or OpenGL removal is introduced.
