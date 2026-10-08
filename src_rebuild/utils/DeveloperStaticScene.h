#ifndef DEVELOPER_STATIC_SCENE_H
#define DEVELOPER_STATIC_SCENE_H

struct CELL_OBJECT;
struct MODEL;
struct _pct;

// Bounded Vulkan migration: shared opaque original scenery, default off.
void DeveloperStaticScene_SetEnabled(int enabled);
int DeveloperStaticScene_GetEnabled(void);
void DeveloperStaticScene_SetLighting(int enabled);
int DeveloperStaticScene_GetLighting(void);
void DeveloperStaticScene_SetLightingStrength(float strength);
float DeveloperStaticScene_GetLightingStrength(void);
void DeveloperStaticScene_SetObjectLimit(int count);
int DeveloperStaticScene_GetObjectLimit(void);
void DeveloperStaticScene_SetRange(float range);
float DeveloperStaticScene_GetRange(void);
void DeveloperStaticScene_PrepareFrame(void);
void DeveloperStaticScene_SubmitExtended(void);
int DeveloperStaticScene_TryDraw(const CELL_OBJECT* object, MODEL* model, const _pct* context);
int DeveloperStaticScene_Owns(const CELL_OBJECT* object);
void DeveloperStaticScene_Shutdown(void);

#endif
