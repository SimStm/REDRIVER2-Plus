---
type: Design
title: Engine migration validation policy
status: planned
created: 2026-10-08
tags: [okf, engine, validation, deterministic, rendering]
---

# Engine migration validation policy

## What must remain compatible

Game mechanics are validated by simulation state, not by rendered-image equality.
Define field-wise canonical checksums with stable IDs and explicit serialization;
never hash struct padding, addresses or wall-clock diagnostics. Compare positions,
velocities, collision/damage, AI/mission state, timers, RNG and replay cursors at
the same tick. Preserve the original arithmetic and update ordering first.

Use baseline NTSC and PAL builds and recorded inputs. Establish actual regional
semantics: 30/25Hz pacing does not imply every mission counter uses that divisor.
Existing tapes must be mapped to tick repeats without silently resampling them.
New replay metadata records schema, region/rate, content identity and tick inputs;
old files remain readable or produce an explicit version/region diagnostic.

Test presentation at 25/30/60/120/144 FPS plus an uncapped schedule, and injected
uneven/slow frames. Equal tick inputs must yield equal approved simulation states.
Separately measure wall-clock tick pacing. For deterministically injected time,
expected completed-tick counts must be exact. Live pacing tolerances are defined
before the run. No debugger pause, minimize, stall or fast CPU may cause an
unbounded catch-up loop, silent tick loss, gameplay acceleration or tape drift.
State an explicit pause/backlog policy and test its resume behavior.

Rendering must not mutate authoritative simulation, consume its RNG or advance
mission/replay/audio events. Presentation interpolation reads previous/current
snapshots; camera cuts, teleports, reloads and slot-generation changes reset it.

## What may look different

Native perspective, filtering, lighting, shadows, AA and new effects may differ
intentionally. Original image equality is not a universal acceptance gate.
Document material/color-space policy, camera intent, transparency/decal semantics
and intended improvements before approving visual results.

Use selected reproducible scenes and small analytic fixtures to test missing
geometry, winding, UVs, cutout holes, occlusion, overlaps, animation, depth,
shadow receivers and UI composition. Check actual native resources and draw paths.
Pixel comparisons are appropriate for regression fixtures and suspected defects;
do not recreate an exact offline model of every GPU interpolation operation as
a prerequisite of a playable block. Known unresolved defects stay recorded.

## Conversion and integration evidence

- Record source/converter hashes, schema and options, supported variants, IDs,
  mesh/instance/material counts, bounds and rejected/unsupported categories.
- Test malformed/truncated data, missing references, failed conversions, stale
  caches and cancellation. Publish only complete packages; keep the last valid
  cache when a replacement fails, without treating it as current content.
- Verify streamed sectors, moving/destroyed instances and mod precedence. A
  screenshot from a city viewer does not prove in-game source coverage.
- Preserve raw sound sample rates, loop metadata, stable sound/event IDs and
  timing. Measure rendered/mixed audio and voice lifecycle, not just API success.
- Verify resource create/reload/retire cycles, memory caps, synchronization and
  failure diagnostics. Distinguish loading, unsupported features and real errors.

## Execution and reporting

Run the smallest meaningful automated checks after each change, then the block's
integration scenarios. Reuse suites; expand them for new behavior or an observed
failure. Development blocks may ship partial capabilities under explicit gates;
the dependent acceptance claim waits for essential contracts, not optional polish.

Direct MSBuild/project CLI and local RenderDoc1.41 are suitable routine paths.
Record SDK/tool versions and phase progress. A client/tool response timeout is
not evidence that the process failed; inspect the same owned process and its logs.
Use bounded real test deadlines, explicit exit codes and cleanup; do not promise
that an interactive application or third-party tool can never stall.
Generated build files from the other branch must be regenerated in an isolated
directory before builds; a stale executable cannot validate the current branch.

For budgets collect CPU/GPU p50/p95/p99, loading/upload time, peak/resident memory,
draw/material/voice/particle counts and hardware/settings. A frame cap or capture
replay duration is not GPU rendering cost. Windows evidence does not accept Linux,
web or Android. Existing baseline evidence remains dated, not rerun implicitly.

Every acceptance entry states revision, scenario, command, result, artifact
location and remaining scope. No code build/game/audio/GPU validation was run
for the documentation that establishes this policy.
