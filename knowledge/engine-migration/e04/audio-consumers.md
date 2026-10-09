---
type: Evidence
title: E04 audio consumer inventory
description: Every existing audio output path and its migration target for FMOD Core.
tags: [okf, engine, audio, fmod, migration, e04]
---

# E04 audio consumer inventory

Reconnaissance for the E04 FMOD Core migration (2026-10-09, `feat/new-engine`).
Everything currently funnels through `sound.c` (SPU voice emulation) plus four
independent OpenAL consumers. This map drives the replacement order; exact line
numbers refer to the tree at that date.

## Update cadence

- **Per rendered frame** (vblank callback, `sound.c:102/149`): `XM_Update()`
  (sequenced music) and `SoundHandler()` (SFX note-off timeouts). Music/SFX
  decay advance per presentation frame, not per simulation tick.
- **Per simulation tick** (30/25 Hz, `gamesnd.c:1614` `SoundTasks`, called from
  `StepSim` at `main.c:1230`): environment sounds, Doppler, engine/siren channel
  updates, `ControlSpeech`, `DoCutsceneSound`/`DoMissionSound`, `LeadHorn`.

The FMOD service must preserve this split deliberately (the stage requires
fixed-tick independence from presentation FPS; the XM cadence is a known
presentation-FPS dependency to remove).

## Consumers by category

| Category | Entry points | Notes |
|---|---|---|
| Engine RPM / idle | `gamesnd.c:550-588` start, `ControlCarRevs` `gamesnd.c:708-802`, applied in `SoundTasks` `gamesnd.c:1663-1694` via `SetChannelPosition3` (`sound.c:697`) | Pitch from `cp->hd.revs`; per-tick gain |
| AI traffic Doppler | `DoDopplerSFX` `gamesnd.c:985-1349` (4 `car_noise` slots) | Start/update per tick |
| Sirens / horns | Player `gamesnd.c:571/1686-1694`; AI `gamesnd.c:1051-1182`; horn `main.c:1190-1228`, `LeadHorn` `gamesnd.c:2566` | Uses `SpuSetVoicePitch(chan,0)` as a silence hack |
| Collisions / impacts | `CollisionSound` `gamesnd.c:1398-1513` -> `Start3DSoundVolPitch` | Callers: `bcollide.c:873`, `wheelforces.c:331`, `drivinggames.c:181` |
| Explosions / thunder | `gamesnd.c:1516-1563`, `debris.c:4299` | One-shot |
| Skids / wheel | `handling.c:1560/1620` | `StartSound` + `SetChannelPosition3` |
| Frontend | `FESound` `sound.c:1160` | |
| Speech queue | `gSpeechQueue` `gamesnd.c:125`, `PlaySpeech` `gamesnd.c:815`, `ControlSpeech` `gamesnd.c:846`, `DoSpeech` `gamesnd.c:806` | Polls `SpuGetKeyStatus` for end-of-line; needs a completion signal |
| Music (XM) | `StartXM/StopXM/FreeXM` `sound.c:1056-1090`, sequencer `xmplay.c`, `FunkUpDaBGMTunez` `gamesnd.c:1593` | In-house module player drives SPU voices |
| CD-XA streams | `xaplay.c` (`PrepareXA/PlayXA/StopXA/PauseXA/ResumeXA/SetXAVolume`; OpenAL path `xaplay.c:230-487`), selection `mc_snd.c:173-211`, polled `gamesnd.c:1726` | Music/dialogue; subtitles `xaplay.c:126` |
| FMV | `fmvplay.c:39-46`, `utils/video_source/VideoPlayer.cpp:525-662` | Own OpenAL stream; gain from `master_volume` |
| Pause/resume | `PauseSound`/`UnPauseSound` `sound.c:799/871` (fade + zero pitch); `PauseSFX`/`UnPauseSFX` `spool.c:303/312` | Callers in `main.c`, `director.c`, `glauncher.c` |
| Replay/fast-forward | `director.c:92-101/1022/1539-1587`; tick override `main.c:1648/1695` | |

## Hidden second paths (must be covered, not only `sound.c`)

1. `xaplay.c` calls OpenAL directly on its own source (`xaplay.c:236-457`); only
   the buffer uses the WAV wrapper.
2. `VideoPlayer.cpp:525-662` (FMV) runs its own OpenAL streaming loop.
3. PsyCross owns the OpenAL device/context (`PsyX_SPUAL.cpp:158-289`, 24
   voices) shared by XA and FMV; FMOD opening its own device means two audio
   backends coexist during the migration.
4. `utils/audio_source/*` (`snd_al_source.cpp:12-47`) is buffer/loop-only: no
   voice management, pause, 3D or gain.
5. `SpuSetVoicePitch(chan,0)` is used as silence (`gamesnd.c:1691`,
   `sound.c:792`) and `SpuGetKeyStatus` as line-finished polling
   (`gamesnd.c:866/890`, `main.c:1215`): the contract/service needs explicit
   stop and completion signals.
6. Volume globals are split (`master_volume` `sound.c:52` vs
   `gMasterVolume`/`gMusicVolume` `sound.c:54-55`); XA reads the latter, FMV the
   former. Unify under the contract/service.

## Replacement order (by risk/centrality)

1. SPU/voice SFX core (`sound.c` channel engine: `SoundHandler`,
   `CompleteSoundSetup`, `StartSound`, `Start3DTrackingSound`,
   `Start3DSoundVolPitch`, `SetChannel*`, `GetFreeChannel`) - unlocks engine,
   sirens, collisions, skids.
2. Speech queue (`gamesnd.c:806-952`, `ControlSpeech`) with explicit completion.
3. Pause/resume (`sound.c:731-876` and its callers).
4. XM sequenced music (`sound.c:1056-1090`) while removing the presentation-FPS
   cadence.
5. XA streaming (`xaplay.c`, `mc_snd.c:173-211`).
6. FMV audio (`VideoPlayer.cpp:525-662`).
7. Mission/cutscene sequencers (`mc_snd.c:214-749`) on top of the service.

## Service constraints captured elsewhere

- Contract: `Game/C/audiocontract.*` (stable event ids, voice generations,
  listener) - see the E04 section of `knowledge/CURRENT_STATUS.md`.
- Acquisition/version/distribution/threading: `knowledge/rules/fmod-integration.md`
  (SDK 2.03.15 verified at `src_rebuild/dependencies/fmod/core`).
