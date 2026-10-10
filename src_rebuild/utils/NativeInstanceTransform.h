#ifndef NATIVE_INSTANCE_TRANSFORM_H
#define NATIVE_INSTANCE_TRANSFORM_H

/*
 * Pure instance placement helper (E03 block 2 item 4).
 *
 * The native renderer consumes a world matrix per instance; the game stores a
 * packed cell object as a raw position (SVECTOR, 4096 units per metre) and a
 * yaw index `yang` (6 bits). The game's object rotation table is
 * matrixtable[i] = RotMatrixY(i * 64) over a 4096-unit turn
 * (Game/C/draw.c:507-525, 1508-1517; cell.h:19), so yang selects the same
 * matrix as yang + 64. The native placement is T = Translate(pos) * RotY(yang *
 * 64) in raw units, matching utils/DeveloperModernMesh.cpp BuildInstanceWorld
 * (484-492) and the game's own matrixtable convention.
 *
 * The matrix is column-major, the same layout the native shader consumes with
 * mul(MVP, float4(pos, 1)) and the same the E02 GLB node transform uses:
 *
 *   out[0]=c  out[4]=0  out[8]=s   out[12]=pos.x
 *   out[1]=0  out[5]=1  out[9]=0   out[13]=pos.y
 *   out[2]=-s out[6]=0  out[10]=c  out[14]=pos.z
 *   out[3]=0  out[7]=0  out[11]=0  out[15]=1
 *
 * where c=cos(yang*64*2pi/4096), s=sin(...). This module has no Diligent, GPU
 * or game headers; it is C++11 and C-compatible.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Composes T = Translate(pos) * RotY(yang * 64) into `out` (column-major, 16
   floats). `yang` is masked to 6 bits, so yang and yang + 64 give the same
   matrix. `pos` is raw game units; either pointer must be non-NULL. */
void NativeInstance_ComposeTransform(const int pos[3], unsigned int yang, float out[16]);

#ifdef __cplusplus
}
#endif

#endif // NATIVE_INSTANCE_TRANSFORM_H
