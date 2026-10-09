---
type: Rule
title: Version the replay format in reserved header fields and keep V1 readable
description: How the project replay (.D2RP) format carries version/region/rate/content metadata without breaking legacy tapes.
tags: [okf, engine, replay, format, determinism]
---

# Version the replay format in reserved header fields and keep V1 readable

**When** changing the replay (`.D2RP`) container, **then** add a new format
version instead of reinterpreting the existing bytes, and keep every earlier
version loadable.

- The legacy format is `REPLAY_SAVE_HEADER` with `DRIVER2_REPLAY_MAGIC`
  (`0x14793209`). New tapes use `DRIVER2_REPLAY_MAGIC_V2` (`0x1479320A`) and
  store metadata in the previously unused `reserved1`/`reserved2` fields, so the
  on-disk header size and layout stay identical:
  - `reserved1` = `REPLAY_FORMAT_VERSION`
  - `reserved2[0]` = region (`REPLAY_REGION_NTSC`/`REPLAY_REGION_PAL`)
  - `reserved2[1]` = simulation tick rate in Hz (30 NTSC / 25 PAL)
  - `reserved2[2]` = tick period in microseconds (33333 / 40000)
  - `reserved2[3]` = 32-bit FNV-1a checksum over everything after the header
    (stream data, parameter block, waypoints, cameras, pings)
- Read the format from the magic, never from the reserved bytes: legacy tapes
  leave them undefined. `LoadReplayFromBuffer` accepts both magics; the V1 path
  is unchanged.
- Diagnose mismatches explicitly; never silently accept or silently resample.
  Unknown magic, unsupported `reserved1`, region/rate differing from the active startup mode,
  and a checksum mismatch each print a distinct message. A region/rate mismatch
  warns and continues at the active cadence. Select the matching regional mode
  and restart before loading for original timing; changing a saved preference
  must never retime an already running tape.
- The recorded simulation cadence is part of replay compatibility. Do not change
  the NTSC/PAL tick period without a new format version and a migration note.

Test the contract with the `-savereplay <file> -saveat <tick>` developer fixture
(`DEBUG_OPTIONS`): round-trip a tape, corrupt the version byte, flip the region,
corrupt a content byte, and confirm the legacy `ATTRACT.*` tapes still load and
match the determinism baseline.

See [engine-migration-boundaries.md](engine-migration-boundaries.md) for the
rule that regional simulation semantics are preserved during migration.
