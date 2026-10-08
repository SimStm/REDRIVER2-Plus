---
type: Change
title: Captured Vulkan warning provenance
created: 2026-10-08
tags: [okf, rendering, tooling, validation]
---

# Captured Vulkan warning provenance

Two1.41 structured exports prove all four HIGH messages were stored during
capture, with exact description/metadata matching and missing-entry controls.
Each frame's five application barriers contain no buffer barrier. Official
RenderDoc1.41 internal mapped-memory readback uses the reported HOST_READ/
ALL_COMMANDS combination; current native picking uses correct TRANSFER/HOST.

Source-backed instrumentation attribution is recorded separately from clean
validation. The exact injected-binary stack was not collected. All messages and
`validationClean=false` remain intact. No production synchronization change.
Direct native GPU suite exits0/PASS24.931s with validation enabled and no VUID
in fresh stdout/stderr/report; this does not accept full original-city coverage.

[Detailed evidence and limits](../../../rendering/r03-capture-warning-provenance.md).
R03 remains incomplete; OpenGL is preserved. Compatibility record: VK036.
