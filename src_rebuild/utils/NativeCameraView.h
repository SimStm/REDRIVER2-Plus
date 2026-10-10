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

#ifdef __cplusplus
}
#endif

#endif // NATIVE_CAMERA_VIEW_H
