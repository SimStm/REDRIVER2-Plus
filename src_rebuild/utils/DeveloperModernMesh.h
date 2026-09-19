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

/* Non-zero when a mesh exists and is currently visible. */
int  DeveloperModernMesh_IsVisible(void);

#endif // DEVELOPER_MODERN_MESH_H
