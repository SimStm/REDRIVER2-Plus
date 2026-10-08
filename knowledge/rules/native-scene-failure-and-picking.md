---
type: Rule
title: Native scene failure and frame-owned picking
description: Keep native availability explicit and logical picking tied to the GPU-submitted snapshot.
tags: [okf, rules, rendering, vulkan, ownership, picking]
---

# Native scene failure and frame-owned picking

**When** native rendering is selected, **then** unavailable native world frames
must never draw the old world or its composites. Explicit source/upload loading
is expected; absent publication, unsupported content/modes/capabilities, stale
handles, budget failures and GPU failures are errors with useful diagnostics.
Only declared unmigrated auxiliary consumers may remain; count and migrate them.
OpenGL backend choice is separate. This policy was adopted on 2026-10-07.

**When** picking native geometry, **then** use its actual depth/culling/cutout
coverage and copy logical identity/resource handles from the submitted snapshot.
Never infer a hit from old PSX packets, current draw order or reused slots.
Read the bounded pixel buffer only after the ordinary owner fence. Validate
window/drawable scaling, resize, scene/resource generations and request serials;
new requests/cancellation supersede in-flight results. Original source metadata
must be owned, bounded and generation-checked, never borrowed spool pointers.

**When** creating mapped readback resources, **then** allocate into local handles
and publish only after all steps succeed. On partial failure unmap, destroy and
free in dependency order; a subsequent retry must not reuse an unbound buffer.
Actual post-allocation failure/retry and GPU pixels are the regression evidence,
not just a successful compile. Preserve submitted upload resources until actual
fence/device-idle proof; test recovery with newly rendered native pixels.

Evidence: [R03 validation](../rendering/r03-validation.md). Native logical instance/
material-part picking does not imply face highlighting/export or complete dynamic
coverage. Remove old dependencies only after their consumers pass migration.

**When** reporting a bounded native region, **then** reconcile selected/copied/
published sources and their exact material-part counts. Shared cell references
select once; source slots are frame-local accounting keys, never logical identity.
Distinguish original view candidates outside the slice from missing in-slice
sources, producer publication and actual GPU visibility. Declare omissions and
verify native pixels/resources separately; counts cannot prove complete coverage.
