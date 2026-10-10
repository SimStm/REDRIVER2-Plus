#ifndef NATIVE_DEVELOPER_OVERLAY_H
#define NATIVE_DEVELOPER_OVERLAY_H

struct ImDrawData;
// The panel owns the ImGui/SDL context. This adapter owns only Diligent GPU
// resources and draws into the renderer's existing target, without presenting.
bool NativeDeveloperOverlay_Init(void* device);
bool NativeDeveloperOverlay_Draw(void* context, void* target, ImDrawData* data);
void NativeDeveloperOverlay_Shutdown();

#endif
