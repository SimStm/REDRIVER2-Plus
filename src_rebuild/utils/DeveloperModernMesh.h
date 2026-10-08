#ifndef DEVELOPER_MODERN_MESH_H
#define DEVELOPER_MODERN_MESH_H

/*
 * Renderer roadmap R2 (renderer modernization): a switchable synthetic unlit
 * mesh drawn through PsyCross's experimental modern mesh path. The mesh shares
 * the legacy camera projection and depth buffer so it and the legacy-rendered
 * floor/obstacles mutually occlude.
 *
 * The path is a developer experiment: it is created only on desktop builds and
 * its experimental state is persisted to developer_modern_mesh.ini so a capture
 * run can enable it without synthetic input.
 */

void DeveloperModernMesh_Initialise(void);
void DeveloperModernMesh_Shutdown(void);

/* Process toggles before legacy/modern ownership decisions for this frame. */
void DeveloperModernMesh_ProcessInput(void);

/* Per-frame: updates the final camera view matrix and pushes the instance. */
void DeveloperModernMesh_Update(void);

/* Runtime toggle (F10) and persistence. */
void DeveloperModernMesh_SetEnabled(int enabled);
int  DeveloperModernMesh_GetEnabled(void);

/* Independent modern-path effects, exposed to the developer Graphics panel. */
void DeveloperModernMesh_SetShadows(int enabled);
int  DeveloperModernMesh_GetShadows(void);
void DeveloperModernMesh_SetStaticUnlit(int enabled);
void DeveloperModernMesh_SetStaticLighting(int enabled);
void DeveloperModernMesh_SetStaticLightingStrength(float strength);
void DeveloperModernMesh_SetStaticSceneLimit(int count);
void DeveloperModernMesh_SetStaticRange(float range);
void DeveloperModernMesh_SetWorldShadowCasters(int enabled);
int  DeveloperModernMesh_GetWorldShadowCasters(void);
void DeveloperModernMesh_SetLegacyShadowCasters(int enabled);
int  DeveloperModernMesh_GetLegacyShadowCasters(void);
void DeveloperModernMesh_SetAmbientOcclusion(int enabled);
int  DeveloperModernMesh_GetAmbientOcclusion(void);

/* Legacy-geometry light receptivity (roadmap legacy-lighting-receptivity): the
   modern sun additionally lights the already-rendered legacy scene. Off by
   default so the shipped legacy shading is unchanged; the scale controls how
   strongly the legacy colour is scaled. F9 toggles it at runtime. */
void  DeveloperModernMesh_SetLegacyLighting(int enabled);
int   DeveloperModernMesh_GetLegacyLighting(void);
void  DeveloperModernMesh_SetLegacyLightReceptivity(float scale);
float DeveloperModernMesh_GetLegacyLightReceptivity(void);

/* Point lights of the legacy receptivity term (roadmap point-light-sources):
   the player's headlights and the light bars of nearby police cars, published
   next to the sun and capped by `pointLightLimit` (1..4, so both backends can
   shade the same lights). Off by default like the receptivity term they feed.
   `pointLightProbe` replaces the game-derived sources with a single known light
   on the player vehicle, for captures that must not depend on traffic. Setters
   persist to developer_modern_mesh.ini and push the light set. */
void  DeveloperModernMesh_SetPointLights(int enabled);
int   DeveloperModernMesh_GetPointLights(void);
void  DeveloperModernMesh_SetPointLightLimit(int limit);
int   DeveloperModernMesh_GetPointLightLimit(void);
void  DeveloperModernMesh_SetPointLightRange(float range);
float DeveloperModernMesh_GetPointLightRange(void);
void  DeveloperModernMesh_SetPointLightIntensity(float intensity);
float DeveloperModernMesh_GetPointLightIntensity(void);
void  DeveloperModernMesh_SetHeadlights(int enabled);
int   DeveloperModernMesh_GetHeadlights(void);
void  DeveloperModernMesh_SetSirenLights(int enabled);
int   DeveloperModernMesh_GetSirenLights(void);
void  DeveloperModernMesh_SetPointLightProbe(int enabled);
int   DeveloperModernMesh_GetPointLightProbe(void);

/* Modern sun and look controls for the developer Graphics panel. The angles
   are derived from the live direction vector, so the F-key controls and the
   panel always agree. Setters persist to developer_modern_mesh.ini. */
void  DeveloperModernMesh_GetSunAngles(float* azimuthDegrees, float* elevationDegrees);
void  DeveloperModernMesh_SetSunAngles(float azimuthDegrees, float elevationDegrees);
float DeveloperModernMesh_GetLightIntensity(void);
void  DeveloperModernMesh_SetLightIntensity(float intensity);
float DeveloperModernMesh_GetAmbient(void);
void  DeveloperModernMesh_SetAmbient(float ambient);
float DeveloperModernMesh_GetExposure(void);
void  DeveloperModernMesh_SetExposure(float exposure);
int DeveloperModernMesh_GetFarShadows(void);
void DeveloperModernMesh_SetFarShadows(int enabled);
float DeveloperModernMesh_GetShadowExtent(void);
void  DeveloperModernMesh_SetShadowExtent(float extent);
int   DeveloperModernMesh_GetShadowDebug(void);
void  DeveloperModernMesh_SetShadowDebugMode(int mode);

/* Non-zero when a mesh exists and is currently visible. */
int  DeveloperModernMesh_IsVisible(void);

#endif // DEVELOPER_MODERN_MESH_H
