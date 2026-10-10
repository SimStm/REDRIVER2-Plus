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
 * The scene draw is camera-driven and depth-tested. It draws submitted
 * instances when any are set, otherwise converted E02 content loaded through
 * NativeRenderer_LoadContent, otherwise a built-in placeholder.
 * Converted content may carry several primitives (the E03 multi-submesh GLB):
 * each primitive owns its immutable vertex/index buffers, its own base-colour
 * factor and its own shader bindings, and an instance draws all of them. A
 * primitive whose record carries an embedded base-colour image gets its own
 * RGBA8_UNORM texture sampled with a nearest/clamp PSX-look sampler; primitives
 * without an image sample a shared 1x1 white texel. Converted UVs are raw texel
 * coordinates and are scaled by 1/256 at upload time, so a full 256-texel page
 * samples correctly. RenderSceneToRGBA runs the same draw into an offscreen
 * readback target for tests and evidence capture.
 *
 * Alpha/cutout: the baked texture carries the PSX alpha convention from
 * knowledge/rules/texture-alpha-semantics.md (0x0000 -> 0, STP=1 with unknown
 * blend context -> 128, otherwise 255). The unlit slice samples that alpha and
 * blends with one straight-alpha state (SRC_ALPHA / INV_SRC_ALPHA for RGB,
 * ONE / INV_SRC_ALPHA for alpha): alpha 255 is opaque, alpha 128 is a 50%
 * blend into the background, and alpha 0 leaves the destination untouched. A
 * single pipeline covers textured and untextured primitives because the shared
 * white texel (alpha 255) makes the blend a no-op, so the placeholder and
 * untextured paths are unchanged. Depth writes stay enabled for this bounded
 * slice, so a semi-transparent primitive also writes depth; ordered
 * transparency is a later block.
 *
 * Coordinate/unit policy (pinned by tests/SourceModelDecoderTests.cpp): the
 * camera `view` matrix, decoded vertices and the projection near/far must share
 * one unit. The game and the decoder use raw game units (4096 per metre), and
 * the bridge keeps the camera translation raw while scaling only the rotation by
 * 1/4096, so a raw-unit scene needs raw-unit near/far. NativeRenderer_SetClipPlanes
 * supplies that pair (the bridge passes ~100 / ~125000); the default 0.02 / 300
 * is only valid for metre-scale synthetic fixtures.
 *
 * Vertical convention: the game camera space is +Y-down. GTE screen space is
 * +Y-down (PsyX_GTE.cpp:395-396, SY = OFY + IR2*h/sz, so a camera-space +y
 * point lands below the screen centre), and the game's inv_camera_matrix row 1
 * is the camera Y axis (rndrasm.c), while Diligent's Vulkan backend flips the
 * viewport to Direct3D's +Y-up convention (DeviceContextVkImpl.cpp "Turn the
 * viewport upside down"). The scene projection therefore negates camera-space y
 * (BuildPerspective out[5] = -f) so a camera-space +y point maps to the LOWER
 * half of the target, exactly as the game and the project's modern-mesh path
 * (PsyX_ModernMesh.cpp:105, `-view.y`) do. The camera `view` matrix is fed
 * unchanged: the game's inv_camera_matrix already carries the game axes and the
 * negation lives only in the projection (NativeCameraView_Compose stays as-is).
 *
 * Winding policy: the y negation above is a reflection, so it flips the signed
 * area sign of every triangle in the NDC. The decoder emits quads
 * (v0,v1,v3,v2) whose first triangle (v0,v1,v3) is the game's gte_nclip
 * triangle (draw.c:780-796); under the reflected projection that game-front
 * triangle has a NEGATIVE signed area, so the scene pipelines cull back faces
 * with CULL_MODE_BACK and FrontCounterClockwise False (Diligent's Vulkan D3D
 * emulation). The reflection and the flipped pin cancel: exactly the same world
 * faces remain front as before the vertical fix. The projection is +Z-forward
 * (the game/GTE convention, GR_Perspective3D clip.w = +z), so geometry ahead of
 * the camera has positive camera-space z and a positive clip w; the game's
 * inv_camera_matrix is fed directly with no reflection.
 *
 * Residency and submission are separate: resources are registered by key
 * (NativeRenderer_RegisterResource), and a frame submits a bounded list of
 * instances (NativeRenderer_SetInstances) that reference those keys. A missing
 * key is skipped with a counter and a first-key diagnostic; it is never fatal
 * and never falls back to the placeholder. The draw mode is explicit:
 * instances when the list is non-empty, otherwise loaded content, otherwise the
 * built-in placeholder.
 */

#ifdef _WIN32

/* Bounded residency and per-frame submission limits. Reaching a limit is a
   named error; the renderer never evicts a resource or an instance silently. */
#define NATIVE_MAX_RESOURCES 64
#define NATIVE_MAX_INSTANCES 4096
#define NATIVE_RESOURCE_KEY_CAPACITY 160
#define NATIVE_INSTANCE_DIAGNOSTIC_CAPACITY 256

/* One instance submission. `resourceKey` is the canonical text key of a
   registered resource (Game/C/conversioncache.h FormatKey). `transform` is the
   column-major world matrix (utils/NativeInstanceTransform.h composes the game
   Translate * RotY placement). The integer fields are diagnostics only: the
   renderer is game-agnostic and never interprets them. */
typedef struct NativeInstance
{
	char resourceKey[NATIVE_RESOURCE_KEY_CAPACITY];
	float transform[16];
	int level;
	int variant;
	int sourceObjectIndex;
	int modelSlot;
	int modelRevision;
} NativeInstance;

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

/* Draws the native scene (submitted instances when any are set, otherwise the
   loaded converted content, otherwise a textured ground and pillars) with the
   last camera and presents it. Returns 1 on success, or 0 with a diagnostic in
   `error`. Creates its pipeline and depth buffer lazily on first use and after
   a resize. With instances set, the draw is ONLY those instances: an
   all-unresolved submission clears and reports a diagnostic, and never falls
   back to the placeholder. */
int NativeRenderer_RenderScene(char* error, int errorCapacity);

/* Loads converted content from a .glb (the E03 record: one mesh with one or
   more primitives, each with positions, optional UVs, 16/32-bit indices and its
   own base-colour factor plus an optional embedded base-colour image) and
   uploads immutable GPU buffers plus the shader bindings for both scene
   pipelines. A textured primitive gets its own RGBA8_UNORM texture sampled with
   a nearest/clamp PSX-look sampler; a primitive without an image samples a
   shared 1x1 white texel, so its base-colour factor is what is visible. UVs are
   raw texel coordinates and are scaled by 1/256 at upload time. The node
   transform places the mesh in the world; world units stay raw game units (4096
   per metre), no conversion is applied. Returns 1 on success; returns 0 with a
   diagnostic in `error` and leaves previously loaded content untouched.
   Re-loading replaces and releases the old content resources. Requires an
   active renderer. */
int NativeRenderer_LoadContent(const char* glbPath, char* error, int errorCapacity);

/* Releases the loaded content and returns the scene to its placeholder. Safe to
   call when nothing is loaded. */
void NativeRenderer_ClearContent(void);

/* Non-zero while converted content is loaded. */
int NativeRenderer_HasContent(void);

/* Vertex and triangle counts of the loaded content; both 0 when none. Either
   pointer may be NULL. */
void NativeRenderer_GetContentStats(int* vertexCount, int* triangleCount);

/* Registers a converted .glb (one or more primitives) under a canonical `key`
   and uploads its immutable GPU resources. Idempotent: re-registering a
   resident key is a no-op that returns 1. Returns 0 with a diagnostic in `error`
   when the key or path is empty, the renderer is inactive, the file cannot be
   loaded, or the resource capacity (NATIVE_MAX_RESOURCES) is already full; a
   full table never evicts a resident resource. Requires an active renderer. */
int NativeRenderer_RegisterResource(const char* key, const char* glbPath,
	char* error, int errorCapacity);

/* Releases one registered resource and its GPU buffers. Safe when the key is
   unknown or nothing is registered. */
void NativeRenderer_UnregisterResource(const char* key);

/* Releases every registered resource. Safe to call when none are registered. */
void NativeRenderer_ClearResources(void);

/* Non-zero while `key` has a resident resource. */
int NativeRenderer_ResourceResident(const char* key);

/* Copies a bounded instance list for the next draws. Everything is copied; the
   caller may release its array afterwards. A key with no resident resource is
   skipped with a counter and a first-key diagnostic (see
   NativeRenderer_GetFirstInstanceDiagnostic); it is not an error. Returns 0
   with a diagnostic in `error` only when the renderer is inactive, the list is
   NULL with a positive count, or the count exceeds NATIVE_MAX_INSTANCES. */
int NativeRenderer_SetInstances(const NativeInstance* instances, int count,
	char* error, int errorCapacity);

/* Clears the instance list, returning the renderer to the content/placeholder
   modes. */
void NativeRenderer_ClearInstances(void);

/* Sets the projection near/far clip planes, in the same unit as the camera and
   vertices (raw game units for the game scene). A non-positive near or a far
   not greater than near is ignored. Defaults to 0.02 / 300. */
void NativeRenderer_SetClipPlanes(float nearPlane, float farPlane);

/* Counts from the last NativeRenderer_SetInstances / scene draw. `submitted` is
   the instance count, `drawn` how many instances were actually drawn (an
   instance counts once even when it draws several primitives), `skippedUnresolved`
   how many had no resident resource, and `residentResources` the current
   resource count. Any pointer may be NULL. */
void NativeRenderer_GetInstanceStats(int* submitted, int* drawn,
	int* skippedUnresolved, int* residentResources);

/* Copies the first instance diagnostic (an unresolved key or an all-missing
   submission) into `out`. Returns 1 when one is present, else 0. */
int NativeRenderer_GetFirstInstanceDiagnostic(char* out, int capacity);

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
