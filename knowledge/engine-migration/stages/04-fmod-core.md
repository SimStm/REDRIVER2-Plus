---
type: Plan
title: E04 - FMOD Core audio
status: planned
created: 2026-10-08
tags: [engine, audio, fmod]
---

# E04 - FMOD Core audio

## Objective and dependencies

Replace inventoried SPU/OpenAL/CD audio consumers using E01 tick events and E02
converted audio metadata. The user has selected FMOD Core and confirmed permitted
use. Pin the SDK and document acquisition/runtime attribution; do not commit SDK
contents or game sound banks. FMOD Studio is a future extension, not this gate.

## Implementation blocks

- Introduce a project audio contract: stable sound/event IDs, voice handles with
  generations, sample/loop metadata, pitch/volume, position/listener and lifecycle.
  Gameplay does not call FMOD throughout unrelated systems or retain SDK pointers.
- Map original bank/voice allocation, key on/off/status, envelope/reverb and
  music/CD/XA behavior required by supported modes. Define approved audible
  differences rather than silently discarding original semantics.
- Trigger once per simulation event; update spatial presentation from snapshots.
  Preserve engine RPM modulation, sirens, collisions, dialogue/music, fades,
  pause/resume, replay/fast-forward and cutscene synchronization.
- Leave an event layer suitable for later Studio banks/parameters; Core resources
  and Studio authoring are distinct. Provide device loss/error/voice-stealing
  diagnostics and bounded sample/channel budgets.

## Acceptance criteria

- [ ] Original required sound categories play through FMOD, with inventory
  reconciliation and no hidden SPU/OpenAL audio path for accepted categories.
- [ ] Loops, pitch, envelopes, listener direction and voice start/stop/reuse work
  against controlled fixtures and recorded mixed output/timelines.
- [ ] Events are neither doubled nor lost at high/low display FPS; mission,
  replay and cutscene timing matches the approved simulation timeline.
- [ ] Pause/reload/level changes/device errors/shutdown release voices/resources
  safely; missing banks and unsupported codecs produce useful diagnoses.
- [ ] Actual memory/voice/update costs and supported-target results are recorded.

## Validation and limits

Use synthetic ramps/loops/impulses plus local original sounds, captured audio and
event logs. Test driving RPM, police, collision, mission dialogue and a chase
cutscene at several presentation schedules. API initialization or a single heard
sound is insufficient. Remove the old audio consumer only after all its required
semantics pass; final submodule/build removal remains E07.
