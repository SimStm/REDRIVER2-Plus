---
type: Change
title: Identify retained west native auxiliary car and wheel producers
created: 2026-10-08
tags: [okf, rendering, validation, diagnostics]
---

# Identify retained west native auxiliary car and wheel producers

Two direct RenderDoc1.41 captures and a removed read-only inspector-identity
probe match every expanded PGXP prefix vertex to its actual game producer:
12 wheel3 vertices and39 body vertices of Car#11, model1/high detail/palette3.
HD wheel RGBA versus original VRAM state explains four versus one draw. Exact
native attachment hashes match the corresponding uninstrumented captures;
all17 prefix triangles are clipped and shared-colour bytes are unchanged.

An independently changed colour byte is rejected by the attribution join.
Probe source/binary and six settings are restored; no renderer or legacy
feature is added. Car/wheel migration remains R06 debt and screen-space consumers
R10 debt. The existing auxiliary count gate stays failed; full R03 is incomplete.
See [evidence and limitations](../../../rendering/r03-auxiliary-producer-validation.md).
