#ifndef DEVELOPER_NATIVE_SCENE_H
#define DEVELOPER_NATIVE_SCENE_H
#ifdef __cplusplus
extern "C" {
#endif
void DeveloperNativeScene_Initialise(int requested);
void DeveloperNativeScene_Shutdown(void);
void DeveloperNativeScene_Update(void);
void DeveloperNativeScene_SetRequested(int requested);
int DeveloperNativeScene_GetRequested(void);
void DeveloperNativeScene_Rebuild(void);
#ifdef __cplusplus
}
#endif
#endif
