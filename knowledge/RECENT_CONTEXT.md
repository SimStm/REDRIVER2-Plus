---
type: Status
title: Recent engineering context
description: FPS/effects publication; replay route investigation deferred.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-09. E00-E03 retain in-progress status.

The user requests final validation, commit and push of FPS/tick-owned effects
only and defers replay trajectory work. They confirm local original files are
NTSC. Do not resume route diagnosis until separately requested.

This scoped publication contains fixed regional clocks, car/camera snapshots,
FPS/region/VSync controls, tick-owned effects and recorded-field diagnostics.
Earlier local input-service, replay-versioning, E02 conversion and E03 Diligent
work remains in the working tree and is not part of this publication. PsyCross
pins reachable fork commit 76d83df (live VSync); its local input edit stays separate.

NTSC/PAL Release_dev x64 builds pass. Final fresh runner 0402adef reports
145354 timing and 21 settings checks per region, plus 2688 NTSC / 2448 PAL
effect checks, zero failures. Eight comparator regressions pass. Full OpenGL
ATTRACT.400 has strict equality across 1378 ticks at NTSC 30/360 and PAL 25/360;
ATTRACT.401 matches 2289 ticks at NTSC 30/360. No recorded draw mutation appears.
The dump/guard covers explicit fields only, not every game field or all RNG state.

Night/rain initially hung at tick 857: catch-up linked a static palette packet
twice into the ordering table. Read-only stack/OT inspection found the cycle.
Colours remain tick-owned; draw copies packets into frame-owned storage. The
regression verifies unique packet links and loading invalidation.

The wall-collision route remains unresolved and deferred. b2d8857 with its
historical project fork produces the same full gameplay state (also Win32/x64);
official upstream 8.0 visually reproduces the collision with these local data.
A speculative FixCarCos bypass worsened the route and was rejected. Full Chase
mission routes, targeted rain/hubcap visuals and other platforms remain unaccepted.

Runtime checks use isolated INIs/data junctions under ignored build/fx-replay-ticks.
Production assets/configs are untouched and all owned game processes stopped.
Valid commands: `-opengl -nointro -nofmv -replay <file> -statedump <file>`.
Tests use Release_dev x64 with explicit -opengl, not Release_dev_gl.

Next: continue only the user's separately requested migration block. Preserve
authoritative simulation, project-fork ownership and the interim OpenGL direction.
No stage acceptance, backend removal, physics retiming or tape resampling follows.
See [full evidence](changes/2026-10-09/effect-timing-replay-regression/index.md).

Publication validation: the staged scope was exported to a fresh ignored tree
with clean fork 76d83df, excluding unrelated local migration code. Both regional
Release_dev x64 rebuilds pass; fresh runner 2745e12d passes all suites and parser
regressions. Its OpenGL 360 FPS night/rain run matches all 2289 reference ticks
strictly, with zero recorded draw mutations. All owned game processes stopped.
Changed-document relative links and cached whitespace checks pass.
