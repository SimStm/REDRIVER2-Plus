#ifndef DEVELOPER_WORLD_SHADOW_H
#define DEVELOPER_WORLD_SHADOW_H

/* Resident-only, opaque static scenery submitted before camera visibility.
   Game formats/region ownership stay here; PsyCross receives only world xyz. */
/* Returns the complete distant half-size actually submitted, or 0 for near
   only/failure. Budget reductions retain whole volumes; no partial batch is published. */
float DeveloperWorldShadow_Submit(float nearExtent, float requestedFarExtent);
void DeveloperWorldShadow_Clear(void);

typedef struct
{
	int residentCells, unavailableCells;
	int triangles, outsideObjects, outsideTriangles;
	int collectionAttempts, capacityFailures;
	float requestedFarExtent, effectiveFarExtent;
} DeveloperWorldShadowStats;
void DeveloperWorldShadow_GetStats(DeveloperWorldShadowStats* stats);

#endif
