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
 * without an image sample a shared 1x1 white texel. A primitive may also carry a
 * per-vertex COLOR_0 attribute (the E03 visual-parity P2 legacy shading bake):
 * the pixel shader multiplies the sampled texel by it, matching the legacy
 * `texColor * v_color` (psx.frag:147). Geometry with no COLOR_0 defaults to
 * opaque white, so its base-colour factor still applies unchanged. Converted UVs
 * are raw texel
 * coordinates and are scaled by 1/256 at upload time, so a full 256-texel page
 * samples correctly.
 *
 * Vertex-colour scale (E03 visual-parity P6): the legacy GPU packs a per-
 * primitive `bright` multiplier into the vertex data; every textured primitive
 * uses bright = 2 (PsyX_GPU.cpp:790,844,893 MakeTexcoordQuad/Triangle/Rect) and
 * the legacy vertex shader scales the normalised colour by it before
 * interpolation (psx.vert:33-34 `v_color.xyz *= float(a_texcoord.z)`). The
 * effective textured modulation is therefore (colourByte/255) * 2, i.e.
 * colourByte/128; a day ambient of 128 becomes ~1.0, not 0.502. Converted
 * content primitives are all textured legacy world primitives, so they carry
 * colorScale = 2 and the scene vertex shader applies it in float to the vertex
 * colour (never baked into the attribute). The built-in placeholder is a
 * native-only fixture with white vertex colours and keeps colorScale = 1.
 * RenderSceneToRGBA runs the same draw into an offscreen
 * readback target for tests and evidence capture.
 *
 * Alpha/cutout: the baked texture carries the PSX alpha convention from
 * knowledge/rules/texture-alpha-semantics.md (0x0000 -> 0, STP=1 with unknown
 * blend context -> 128, otherwise 255). World models are always opaque:
 * PlotBuildingModel, PlotModelSubdivNxN and drawMesh issue setPolyFT4 with the
 * semi-transparency bit clear, and the legacy fragment shader ignores STP for
 * opaque draws. This slice therefore renders world geometry without blending:
 * the pixel shader discards a texel whose sampled alpha is 0 (the 0000h cutout,
 * psx.frag:111), renders every other texel fully opaque (an STP=128 texel shows
 * its full RGB), and writes an opaque framebuffer alpha. The shared white texel
 * (alpha 255) keeps the placeholder and untextured paths opaque with no discard.
 * Depth writes stay enabled, so a discarded texel writes no depth and a cutout
 * hole shows whatever is behind it. True semi-transparent content (STP with a
 * real blend context) needs a separate blended pass with the depth test disabled
 * and ordered transparency; that is a later block, not this opaque world slice.
 *
 * Dev-only P7 diagnosis toggles (both default off; unset environment = the
 * production pipeline byte-for-byte). They are read exactly once, on the first
 * scene-pipeline build, and are process-wide:
 *   * RED2_NATIVE_NO_CUTOUT=1 omits the alpha==0 discard, so a cutout texel
 *     renders with its own colour (alpha still ignored/opaque). It tells a
 *     wrong-cutout/CLUT/page problem apart from missing geometry.
 *   * RED2_NATIVE_NO_DEPTH=1 disables the scene depth test AND write, so every
 *     primitive draws regardless of order. It tells a depth-occlusion problem
 *     apart from missing geometry.
 * Each active toggle is named once in native_renderer.log
 * ("native renderer: cutout disabled by RED2_NATIVE_NO_CUTOUT"), warn-only.
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
 * Winding/culling policy: the scene pipelines cull nothing (CULL_MODE_NONE),
 * matching the legacy GPU (PsyX_Vk.cpp sets VK_CULL_MODE_NONE). The game
 * pre-culls every primitive in software with gte_nclip / opz > 0 and the
 * PLOT_NO_CULL / PLOT_INV_CULL overrides (draw.c:784/796/1156-1165), so the
 * renderer must not cull again or it removes the faces the game deliberately
 * kept. The vertical convention above still reflects the NDC (a y negation), but
 * with culling disabled the winding sign no longer selects a face; the decoder's
 * stored order only needs to produce the correct raster coverage. The projection
 * is +Z-forward (the game/GTE convention, GR_Perspective3D clip.w = +z), so
 * geometry ahead of the camera has positive camera-space z and a positive clip
 * w; the game's inv_camera_matrix is fed directly with no reflection.
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
   named error; the renderer never evicts a resource or an instance silently.
   Sized for the whole resident world (E03 visual-parity P4): the level ptypes
   probe records up to 516 model slots, so the resource table covers them with
   headroom, and the E02 inventory counts 6740 unique placements, so the
   per-frame instance bound covers them too. */
#define NATIVE_MAX_RESOURCES 1024
#define NATIVE_MAX_INSTANCES 8192
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
   `screenDistanceH` the live GTE screen distance h (the game's scr_z). The
   projection's vertical focal length is 2h/NATIVE_PSX_DISPLAY_HEIGHT and its
   horizontal scale follows the current draw target's aspect on the next scene
   draw (see NativeCameraView_BuildProjection). */
void NativeRenderer_SetCamera(const float view[16], float screenDistanceH);

/* Feeds the live GTE screen offset (SetGeomOffset OFX/OFY), in PSX display
   pixels (the 320x240 space NATIVE_PSX_DISPLAY_*). It is a constant NDC
   translation added by the projection: the PSX display centre (160,120) is
   neutral, and the game's live (160,128) pushes the image 8 PSX lines down.
   The renderer is game-agnostic and defaults to the centre, so a caller that
   never sets it keeps the centred projection. */
void NativeRenderer_SetScreenOffset(float offsetX, float offsetY);

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
