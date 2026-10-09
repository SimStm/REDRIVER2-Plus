#ifndef GLTF_WRITER_H
#define GLTF_WRITER_H

#include "GltfLoader.h"

/*
 * Bounded glTF 2.0 / GLB writer for the E02 conversion path (renderer roadmap
 * R3 / engine migration E02).
 *
 * It writes exactly the subset Gltf_LoadFile reads back: one mesh, one
 * primitive, POSITION/NORMAL/TEXCOORD_0 float accessors, unsigned 32-bit
 * indices, one pbrMetallicRoughness material with a base-colour factor, and one
 * node carrying the mesh placement as a column-major matrix. No image is
 * embedded. The output is deterministic: identical input produces byte-identical
 * GLB, so two identical conversions produce equal content hashes.
 */

/* Writes `mesh` to a .glb at `path`. Returns 0 on success and fills nothing
   else; returns -1 and writes a human-readable reason into `error`. */
int Gltf_WriteFile(const char* path, const GltfMeshData& mesh, char* error, int errorCapacity);

#endif // GLTF_WRITER_H
