---
type: Change
title: Native coplanar paint, transform precision and horizontal depth
created: 2026-10-07
tags: [okf, rendering, vulkan, depth]
---

# Native coplanar paint, transform precision and horizontal depth

R03 partial delivery: bounded source-tile paint classification, decoder v2/native
ABI v9 and shared colour/ID constant depth bias fix captured Chicago paint strips.
Subsequent double CPU view/model-first matrix composition corrects false depth
separation at large city coordinates. Fork6ba8068 and09d74de are committed/pushed
to the project origin/master; latest parent gitlink staged.

CPU asset1536, scene22/remaining suites,12 transform coordinates, host exclusions,
VS Release_dev|x64, fresh native/PSX GPU tests and actual RenderDoc1.46 normal/HD
and wide/original native pixels/resource audits PASS. Paint interiors45000/35470
have zero wrong IDs; all bound geometry/artwork bytes match before/after the
precision fix. Backups restore five INIs and debugger settings hash-exactly.

Accurate grass planes revealed coplanar source-overlap z-fighting. Canonical
horizontal fragment depth now gives equal-height world parts identical equations
and explicit native tie ownership, retaining bounded authored paint bias. CPU12
plane checks, GPU11685 pixels per order/alpha/nearer floor/IDs/retirement and real
normal/wide visible-overlap39558/71120 masks PASS: wrong IDs17677/34474 ->0.
Bound geometry/artwork mips unchanged; invalid inverses fail explicitly, ordinary
nonhorizontal raster depth and early/late synchronization remain native. Source
UV/artwork boundaries and full static-region coverage are still pending.
Latest fork2341108 committed/pushed and parent gitlink staged. Final native/PSX
GPU PASS/exit0/no VUID; final city images and bound buffers/all mips match the
tested canonical results. Five INIs/`.user` hash-restored, all10 VS pairs preserved,
debugger stopped/no breakpoints and no task-owned game/helper processes remain.
R03 is not complete; these results do not pass its remaining gates. R04 remains
unstarted and OpenGL preserved. Existing auxiliary consumers remain pending migration; old-world
scene recovery remains disabled. The shape-flag rule now explicitly reflects
that adopted failure policy.

See [product](../../../product/native-static-region.md),
[executed evidence](../../../rendering/r03-terrain-validation.md) and
[durable precision/ownership rule](../../../rules/native-coplanar-depth.md).
