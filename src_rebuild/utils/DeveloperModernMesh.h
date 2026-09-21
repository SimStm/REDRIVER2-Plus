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

/* Per-frame: updates the camera view matrix and pushes the instance. */
void DeveloperModernMesh_Update(void);

/* Runtime toggle (F10) and persistence. */
void DeveloperModernMesh_SetEnabled(int enabled);
int  DeveloperModernMesh_GetEnabled(void);

/* Independent modern-path effects, exposed to the developer Graphics panel. */
void DeveloperModernMesh_SetShadows(int enabled);
int  DeveloperModernMesh_GetShadows(void);
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
float DeveloperModernMesh_GetShadowExtent(void);
void  DeveloperModernMesh_SetShadowExtent(float extent);
int   DeveloperModernMesh_GetShadowDebug(void);
void  DeveloperModernMesh_SetShadowDebugMode(int mode);

/* Non-zero when a mesh exists and is currently visible. */
int  DeveloperModernMesh_IsVisible(void);

#endif // DEVELOPER_MODERN_MESH_H
