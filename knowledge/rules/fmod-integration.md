---
type: Rule
title: Integrate FMOD Core behind the project audio contract
description: Version, acquisition, distribution and threading rules for the FMOD Engine used by the E04 audio migration.
tags: [okf, engine, audio, fmod, migration]
---

# Integrate FMOD Core behind the project audio contract

**When** adding or changing FMOD usage, **then** keep gameplay on the
project-owned audio contract (`Game/C/audiocontract.*`), pin the 2.03 production
line, and never commit or ship SDK contents beyond the runtime library.

- **Version**: target the 2.03 production line. Current local ignored SDK
  metadata identifies 2.03.15 (`FMOD_VERSION` 0x00020315 and DLL resource
  version 2.3.15). Do not interpret hexadecimal `0x15` as decimal 21.
  Pin source/local metadata; verify again
  through the SDK vendor before changing the pin.
- **Acquisition**: the SDK is login-gated at `fmod.com/download`; the script
  never downloads it. Extract/copy the installer's `api\core` folder to the
  gitignored `src_rebuild/dependencies/fmod\core` (`inc\fmod.h`,
  `lib\x64\fmod_vc.lib`, `lib\x64\fmod.dll`). `FMOD_DIR` overrides the root.
- **Distribution/release review**: keep SDK contents out of Git and distribute
  only runtime files approved by the applicable FMOD terms; builds currently use
  `fmod.dll` beside the executable. Before release, consult the current official
  [FMOD licensing and attribution guidance](https://www.fmod.com/licensing) and
  SDK license/readme, confirm applicable attribution and logo requirements, and
  retain the source/version reviewed. Do not treat this rule as a legal opinion
  or assert specific obligations without checking that official source. No FMOD
  logo or attribution UI is implemented by this documentation change.
- **Contract boundary**: game systems call the project audio contract and never
  retain FMOD pointers or call FMOD directly; the service layer maps contract
  requests (stable event IDs, voice handles with generations, pitch/volume,
  position/listener) to Core calls.
- **Threading**: the Core API is thread-safe by default, but callbacks run on
  FMOD threads and must be guarded. `System::update()` runs once per rendered
  frame (gameplay state changes come from simulation ticks); `close()` and
  `release()` are main-thread teardown.
- **Formats**: PSX VAG/SPU ADPCM and CD-XA are not native FMOD formats. Their
  conversion to PCM/FSB remains migration work; apply loop points
  explicitly with `Sound::setLoopPoints` from converted metadata
  (`Game/C/soundasset.*`), not inferred from containers.

Validate integration with controlled audio fixtures (loops/pitch/envelopes/
listener and channel/polyphony lifetime); API initialization or a single heard
sound is not audible-correctness acceptance. Preserve inclusive loop endpoints;
equal endpoints are unsupported and must be diagnosed rather than lengthened.

See [engine-migration-boundaries.md](engine-migration-boundaries.md) for the
stage rules and `knowledge/engine-migration/stages/04-fmod-core.md` for the E04
acceptance criteria.
