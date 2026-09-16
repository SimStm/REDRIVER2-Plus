#ifndef PLAYGROUND_H
#define PLAYGROUND_H

/*
 * Playable testing playground (renderer/roadmap item 13).
 *
 * A finite, resident procedural scene that reuses the existing vehicle
 * simulation and renderer. The world (surface queries, cell objects and the
 * visible floor) is generated at runtime; original city geometry, roads and
 * collisions are replaced rather than hidden. A loaded original level is used
 * only as a resource donor (car models, textures, sky, sounds) and its world
 * content is discarded while the playground is active.
 */

#define PLAYGROUND_SCENE_ID "playground.flatpad.v1"

extern int gPlaygroundRequested;	// launch has been requested
extern int gPlaygroundActive;		// generated scene is built and running

/* Shared launch operation. Any entry point (debug CLI, developer panel,
   Take a Ride) must call this instead of mutating scene globals directly. */
void Playground_RequestLaunch(void);

int Playground_IsRequested(void);
int Playground_IsActive(void);

/* Replaces the loaded world with the generated playground. Must be called
   from State_GameInit after the base level has been initialised. */
void Playground_BuildScene(void);

/* Returns the player car to the fixture spawn without reloading the level. */
void Playground_ResetCar(void);

/* Cheap per-frame developer input handling (reset). Desktop only. */
void Playground_Tick(void);

/* Fills the visibility table so the legacy cell walker sees the fixture. */
void Playground_FillVisibility(void);

/* Clears the active/requested state when returning to the frontend. */
void Playground_Shutdown(void);

const char* Playground_GetSceneId(void);

#endif // PLAYGROUND_H
