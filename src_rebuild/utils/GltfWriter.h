#ifndef GLTF_WRITER_H
#define GLTF_WRITER_H

#include "GltfLoader.h"

/*
 * Bounded glTF 2.0 / GLB writer for the E02 conversion path (renderer roadmap
 * R3 / engine migration E02).
 *
 * It writes exactly the subset Gltf_LoadFile / Gltf_LoadFileMulti reads back:
 * one mesh, one node carrying the mesh placement as a column-major matrix, and
 * one or more primitives. Each primitive has POSITION/NORMAL/TEXCOORD_0 float
 * accessors, an optional COLOR_0 attribute (RGBA8 ubyte, normalised), unsigned
 * 32-bit indices and its own pbrMetallicRoughness material
 * with a base-colour factor. A primitive may additionally carry a baked RGBA8
 * base-colour image, which is embedded as one deterministic PNG per material.
 *
 * PNG encoder (no external dependency): the image is written as a valid PNG
 * (8-bit RGBA, no interlace, per-scanline filter type None) whose IDAT zlib
 * stream uses only stored (uncompressed) deflate blocks plus the zlib header and
 * Adler-32 trailer. That subset is deliberately the one the multi loader decodes
 * back, so a written file is always readable by this project's loader; it is
 * also a valid PNG that any conforming decoder accepts.
 *
 * Layout is deterministic and documented: one shared BIN buffer holds, per
 * primitive, POSITION then NORMAL/TEXCOORD_0/COLOR_0 (when present) then the
 * index list, then the PNG image (when present), in that fixed order; accessors
 * are numbered
 * per primitive in the same order; material i belongs to primitive i; the k-th
 * textured primitive owns image/texture/sampler k. Identical input therefore
 * produces byte-identical GLB, and a one-primitive mesh with no image written
 * through `Gltf_WriteFileMulti` is byte-identical to the historical
 * single-primitive `Gltf_WriteFile` output.
 */

/* Writes `mesh` (one primitive) to a .glb at `path`. Returns 0 on success and
   fills nothing else; returns -1 and writes a human-readable reason into
   `error`. Kept for existing callers; it delegates to Gltf_WriteFileMulti with a
   single primitive and preserves the historical diagnostic for an empty mesh. */
int Gltf_WriteFile(const char* path, const GltfMeshData& mesh, char* error, int errorCapacity);

/* Writes a mesh with N primitives, each with its own base-colour material and
   optional embedded image, to a .glb at `path`. Rejects an empty primitive list
   and any primitive without positions, with a triangle-incomplete or out-of-range
   index list, or with image dimensions that disagree with the image data size,
   before anything is written, so a rejected input leaves no file. Returns 0 on
   success; -1 and a named reason in `error` otherwise. */
int Gltf_WriteFileMulti(const char* path, const GltfMultiMeshData& mesh, char* error, int errorCapacity);

#endif // GLTF_WRITER_H
