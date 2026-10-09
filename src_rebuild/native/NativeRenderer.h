#ifndef NATIVE_RENDERER_H
#define NATIVE_RENDERER_H

/*
 * E03 native renderer service (Diligent Engine, Vulkan backend).
 *
 * The service owns exactly one device, one immediate context and one swap chain
 * for a single native window handle, so a window never has two independent
 * presenters. It is deliberately Diligent-free at this boundary: the game and
 * other C/C++ callers include only this header, and the Diligent types stay in
 * the implementation. The header is Windows-only because the selected E03
 * desktop scope is Win32 + Vulkan; other platforms keep their existing path.
 *
 * The scene draw is camera-driven and depth-tested. It draws converted E02
 * content when one has been loaded through NativeRenderer_LoadContent and falls
 * back to a built-in placeholder otherwise. RenderSceneToRGBA runs the same
 * draw into an offscreen readback target for tests and evidence capture.
 */

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

/* Creates the device/context and a swap chain on `nativeWindowHandle` (an HWND).
   Returns 1 on success, or 0 with a diagnostic in `error`. Fails when the
   renderer is already active. */
int NativeRenderer_Init(void* nativeWindowHandle, int width, int height,
	char* error, int errorCapacity);

/* Non-zero when the device and swap chain are live. */
int NativeRenderer_IsActive(void);

/* Recreates the swap chain buffers for a new client size. No-op when inactive. */
void NativeRenderer_Resize(int width, int height);

/* Clears the current back buffer and presents it. Returns 1 on success, or 0
   with a diagnostic in `error`. */
int NativeRenderer_RenderClearAndPresent(float red, float green, float blue, float alpha,
	char* error, int errorCapacity);

/* Feeds the game camera: `view` is a column-major world->camera matrix and
   `fovYRadians` the vertical field of view. The projection is built from the
   current draw target's aspect on the next scene draw. */
void NativeRenderer_SetCamera(const float view[16], float fovYRadians);

/* Draws the native scene (loaded converted content when present, otherwise a
   textured ground and pillars, depth-tested) with the last camera and presents
   it. Returns 1 on success, or 0 with a diagnostic in `error`. Creates its
   pipeline and depth buffer lazily on first use and after a resize. */
int NativeRenderer_RenderScene(char* error, int errorCapacity);

/* Loads converted content from a .glb (the E02 record: one mesh/primitive with
   positions, optional UVs, 16/32-bit indices, an optional embedded base-colour
   image and a base-colour factor) and uploads immutable GPU buffers, texture
   and sampler. The node transform places the mesh in the world; world units
   stay raw game units (4096 per metre), no conversion is applied. Returns 1 on
   success; returns 0 with a diagnostic in `error` and leaves previously loaded
   content untouched. Re-loading replaces and releases the old content
   resources. Requires an active renderer. */
int NativeRenderer_LoadContent(const char* glbPath, char* error, int errorCapacity);

/* Releases the loaded content and returns the scene to its placeholder. Safe to
   call when nothing is loaded. */
void NativeRenderer_ClearContent(void);

/* Non-zero while converted content is loaded. */
int NativeRenderer_HasContent(void);

/* Vertex and triangle counts of the loaded content; both 0 when none. Either
   pointer may be NULL. */
void NativeRenderer_GetContentStats(int* vertexCount, int* triangleCount);

/* Renders the same scene draw as NativeRenderer_RenderScene (content when
   loaded, else placeholder) with the last camera into an offscreen RGBA8 target
   with a D32 depth buffer of `width` x `height`, then reads back tightly packed
   `width * height * 4` bytes into `rgba` (no row padding). Does not touch or
   present the swap chain. Returns 1 on success, or 0 with a diagnostic in
   `error`. Intended for tests and evidence capture. */
int NativeRenderer_RenderSceneToRGBA(unsigned char* rgba, int width, int height,
	char* error, int errorCapacity);

/* Releases the loaded content, the scene resources, the swap chain, context and
   device. Safe to call when inactive. */
void NativeRenderer_Shutdown(void);

/* Opaque access to the live Diligent device and immediate context for a caller
   that owns the Diligent headers (the native path and its tests). Both return
   NULL when the renderer is inactive. */
void* NativeRenderer_GetDevice(void);
void* NativeRenderer_GetContext(void);

#ifdef __cplusplus
}
#endif

#endif // _WIN32
#endif // NATIVE_RENDERER_H
