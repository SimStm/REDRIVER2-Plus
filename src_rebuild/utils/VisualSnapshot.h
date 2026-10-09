#ifndef VISUAL_SNAPSHOT_H
#define VISUAL_SNAPSHOT_H

/*
 * Project-owned previous/current visual snapshot seam (E01).
 *
 * The fixed simulation tick updates authoritative state at tick boundaries;
 * presentation runs at the display rate. This module publishes, once per tick,
 * car translations/rotations and camera poses for one shared time interval.
 * Original camera controllers advance at the regional tick rate. Presentation
 * changes only render globals/local matrices, never car physics or player state;
 * restore the current tick camera before simulation consumes those globals.
 *
 * The interpolation factor comes from the fixed clock (the fractional part of
 * the tick accumulator) as a Q8 value (0..256). At 256 the current tick pose is
 * shown; at 0 the previous one is. A tick reset or a large per-tick jump
 * (teleport, level load) drops the previous pose so no streak is drawn.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Records the current tick pose as `cur`, shifting the old `cur` into `prev`.
   Call once per simulation tick, after StepGame has updated the cars. */
void VisualSnapshot_Capture(void);

/* Invalidates all snapshots (level load, replay restart, state change). */
void VisualSnapshot_Reset(void);

/* Writes the interpolated world position for a car slot and returns 1, or
   returns 0 when no valid snapshot exists (caller keeps the tick pose).
   `alphaQ8` is clamped to 0..256. */
int VisualSnapshot_GetPosition(int slot, int alphaQ8, int outXYZ[3]);

/* Rotation uses the same snapshot interval as translation; endpoints are exact. */
int VisualSnapshot_GetRotation(int slot, int alphaQ8, short outRotation[3][3]);

/* Advance original camera logic once per scheduled tick and publish its pose. */
void VisualSnapshot_UpdateCameras(void);

/* Render globals only. Restore the tick camera before returning to simulation. */
void VisualSnapshot_ApplyCamera(int view);
void VisualSnapshot_RestoreCamera(void);

/* Current fixed-clock interpolation factor in Q8 (0..256). */
int VisualSnapshot_GetAlpha(void);

#ifdef __cplusplus
}
#endif

#endif
