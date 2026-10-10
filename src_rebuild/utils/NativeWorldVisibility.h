#ifndef NATIVE_WORLD_VISIBILITY_H
#define NATIVE_WORLD_VISIBILITY_H

#define NATIVE_WORLD_MAX_CELL_REFERENCES 262144
#define NATIVE_WORLD_CELL_RADIUS 21
#define NATIVE_WORLD_CELL_SIDE (NATIVE_WORLD_CELL_RADIUS * 2 + 1)

typedef struct NativeWorldCellReference
{
	int sourceObjectIndex;
	int cellX;
	int cellZ;
	int selector; /* -1 default; otherwise the original event-list selector */
} NativeWorldCellReference;

#ifdef __cplusplus
extern "C" {
#endif

/* Main-thread presentation bridge. Records approved cells from the original
   square spiral/PVS/event selection without reading or mutating game state.
   The native renderer consumes this frame after DrawMap completes. */
void NativeWorldVisibility_Begin(int centerX, int centerZ, int selector);
void NativeWorldVisibility_RecordCell(int cellX, int cellZ);
int NativeWorldVisibility_BuildObjectMask(const NativeWorldCellReference* references,
	int referenceCount, unsigned char* objectMask, int objectCapacity,
	char* error, int errorCapacity);
int NativeWorldVisibility_SelectedCellCount(void);

#ifdef __cplusplus
}
#endif
#endif
