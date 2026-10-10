#ifndef NATIVE_CAMERA_VIEW_H
#define NATIVE_CAMERA_VIEW_H

/*
 * Pure game-camera view helper (E03 block 2 item 4).
 *
 * The game's GTE path transforms a world point to camera space as
 * camera = inv_camera_matrix * (world - camera_position) in raw game units
 * (4096 per metre), and its camera space is +Z-forward
 * (Game/ASM/rndrasm.c Apply_InvCameraMatrixSetTrans; the native projection is
 * +Z-forward to match, see native/NativeRenderer.h). The native renderer
 * consumes a column-major world->camera matrix:
 *
 *   view = [ R | -R*cam ]   (R = inv_camera_matrix rotation, 4096 = 1.0)
 *
 * so `mul(view, float4(world,1))` yields the raw camera-space point with a
 * positive z ahead of the camera. This is the bridge's camera construction made
 * explicit and testable; it has no Diligent, GPU or game headers and is C and
 * C++ compatible.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Composes the raw-unit world->camera view matrix from the game's inverse
   camera rotation (row-major 3x3, fixed point with 4096 = 1.0, flattened as
   rotation[0..8]) and the camera world position in raw units. `out` is the
   column-major 16-float view matrix. Either input pointer must be non-NULL. */
void NativeCameraView_Compose(const int rotation[9], const int cameraPosition[3], float out[16]);

/* Conservative homogeneous AABB test for x/y in [-w,w], z in [0,w].
   Boundary-touching and near-plane-crossing boxes remain visible. The MVP
   includes the exact draw transform, projection and screen offset. */
int NativeCameraView_BoundsVisible(const float mvp[16], const float minimum[3],
	const float maximum[3]);

#ifdef __cplusplus
}
#endif

/* The PSX display the legacy GTE screen mapping is expressed against:
   Game/C/system.c SetGeomOffset(320/2, height/2) with height = SCREEN_H
   (240 on NTSC, 256 on PAL). The native projection's vertical focal length is
   2h/NATIVE_PSX_DISPLAY_HEIGHT, so the native field of view follows the live
   GTE screen distance h instead of a fixed FOV. */
#define NATIVE_PSX_DISPLAY_WIDTH  320.0f
#define NATIVE_PSX_DISPLAY_HEIGHT 240.0f

/* Builds the raw-unit, +Z-forward native projection from the live GTE screen
   distance `screenDistanceH` (the game's scr_z) and screen offset
   (`offsetX`/`offsetY`, the live SetGeomOffset OFX/OFY in PSX display pixels),
   replicating the legacy GTE projection:

     x_screen = ofx + x_cam * h / z_cam
     y_screen = ofy + y_cam * h / z_cam

   The desktop window maps the 320x240 PSX display to the whole render target
   with square pixels: the default PGXP path folds the target aspect into the
   horizontal scale (Projection3D width = 1, height = 1/(aspect*240/320)), so
   the legacy image is uniform, not stretched. The native NDC is therefore

     ndc.x =  (2h / displayHeight) * (x_cam / z_cam) / aspect
     ndc.y = -(2h / displayHeight) * (y_cam / z_cam)

   with `aspect` = targetWidth / targetHeight.

   The GTE screen offset is a constant NDC translation. The legacy vertex
   shader adds the projection offset in clip space, proportional to clip.w
   (psx.vert:43-47, ofsMat translation (ofx', -ofy') with ofx' = (ofx - dispW/2)
   * 2 / dispW and ofy' = (ofy - dispH/2) * 2 / dispH), so after the perspective
   divide it is exactly

     ndc.x += 2 * offsetX / displayWidth  - 1
     ndc.y += 1 - 2 * offsetY / displayHeight

   i.e. the display centre (displayWidth/2, displayHeight/2) is neutral and the
   game's live (160,128) pushes the image 8 PSX lines (24px at 720p) DOWN. Folding
   it into the matrix uses the z-column of x/y (out[8]/out[9]) so it is scaled by
   clip.w = +z and survives the perspective divide; the horizontal translation is
   aspect-independent, exactly as the legacy ofsMat is.

   The projection is +Z-forward (clip.w = +z), depth is [0,1], and out[5]
   negates camera-space y so the game's +Y-down maps to the lower half after
   Diligent's viewport flip. The header-inline form lets the pure test and the
   Diligent renderer share one derivation without an extra link dependency.
   `out` is column-major. */
static inline void NativeCameraView_BuildProjection(float screenDistanceH,
	float displayWidth, float displayHeight, float offsetX, float offsetY,
	float aspect, float nearPlane, float farPlane, float out[16])
{
	for (int i = 0; i < 16; ++i)
		out[i] = 0.0f;
	const float f = 2.0f * screenDistanceH / displayHeight;
	out[0] = f / aspect;
	out[5] = -f;
	out[8] = 2.0f * offsetX / displayWidth - 1.0f;
	out[9] = 1.0f - 2.0f * offsetY / displayHeight;
	out[10] = farPlane / (farPlane - nearPlane);
	out[11] = 1.0f;
	out[14] = -nearPlane * farPlane / (farPlane - nearPlane);
}

#endif // NATIVE_CAMERA_VIEW_H
