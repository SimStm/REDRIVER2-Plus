#ifndef DETERMINISTIC_BASELINE_H
#define DETERMINISTIC_BASELINE_H

/*
 * E00/E01 deterministic baseline checkpoint dump.
 *
 * The engine migration must prove that equal tick inputs produce equal
 * authoritative simulation state regardless of presentation FPS or CPU speed.
 * This developer-only module writes a canonical, field-wise state record once
 * per simulation tick so two runs can be compared byte for byte.
 *
 * It never hashes struct padding, pointers, addresses or wall-clock values:
 * only explicit gameplay/effect fields are serialized, with a 64-bit FNV-1a hash over
 * the same values appended to each tick line.
 *
 * Recorded entities: player world transform, every active car's transform/
 * velocities/damage (the presentation-only camera collider is excluded),
 * mission/AI timers and message timers, and the run-length replay cursor per
 * replay stream. Tick-owned effect pools and light phases are included; the
 * camera and temporary draw poses are excluded. Drawing can be checked against
 * the same record to detect presentation feedback into these states.
 *
 * Enabled only through the command line, e.g.:
 *     REDRIVER2_dev.exe -replay REPLAYS/ATTRACT.400 -statedump baseline.txt
 *     REDRIVER2_dev.exe -mission 1 -statedump baseline.txt -statedumpevery 5
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Parses -statedump <file> and -statedumpevery <n> from argv. No-op when the
   arguments are absent. Must be called after PsyX logging is available. */
void DeterministicBaseline_ConfigureFromCommandLine(int argc, char** argv);

/* Flushes and closes the dump, writing the final record count and hash. */
void DeterministicBaseline_Shutdown(void);

/* Records the current authoritative state if the dump is enabled and this
   simulation tick is due. Cheap when disabled. Call once per StepGame tick. */
void DeterministicBaseline_AfterTick(void);

// Optional read-only-frame invariant, enabled only when a dump is open.
void DeterministicBaseline_BeginPresentation(void);
void DeterministicBaseline_EndPresentation(void);

#ifdef __cplusplus
}
#endif

#endif
