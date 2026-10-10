#include "NativeWorldVisibility.h"
#include <stdio.h>
#include <string.h>

namespace
{
struct CellSelection
{
	int centerX, centerZ, selector;
	int selectedCount;
	bool ready, overflow;
	unsigned char cells[NATIVE_WORLD_CELL_SIDE * NATIVE_WORLD_CELL_SIDE];
};
CellSelection s_selection = {};

int CellIndex(int x, int z)
{
	const long long dx = (long long)x - s_selection.centerX;
	const long long dz = (long long)z - s_selection.centerZ;
	if (dx < -NATIVE_WORLD_CELL_RADIUS || dx > NATIVE_WORLD_CELL_RADIUS ||
		dz < -NATIVE_WORLD_CELL_RADIUS || dz > NATIVE_WORLD_CELL_RADIUS)
		return -1;
	return (int)(dz + NATIVE_WORLD_CELL_RADIUS) * NATIVE_WORLD_CELL_SIDE +
		(int)(dx + NATIVE_WORLD_CELL_RADIUS);
}
}

void NativeWorldVisibility_Begin(int centerX, int centerZ, int selector)
{
	memset(&s_selection, 0, sizeof(s_selection));
	s_selection.centerX = centerX;
	s_selection.centerZ = centerZ;
	s_selection.selector = selector;
	s_selection.ready = true;
}

void NativeWorldVisibility_RecordCell(int x, int z)
{
	if (!s_selection.ready)
		return;
	const int index = CellIndex(x, z);
	if (index < 0)
	{
		s_selection.overflow = true;
		return;
	}
	if (!s_selection.cells[index])
	{
		s_selection.cells[index] = 1;
		++s_selection.selectedCount;
	}
}

int NativeWorldVisibility_BuildObjectMask(const NativeWorldCellReference* references,
	int count, unsigned char* mask, int capacity, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	const char* failure = NULL;
	if (!mask || capacity <= 0 || capacity > 16384 || count < 0 ||
		count > NATIVE_WORLD_MAX_CELL_REFERENCES || (count && !references))
		failure = "invalid native world membership inventory";
	else if (!s_selection.ready || s_selection.overflow)
		failure = "native world cell selection unavailable or exceeds bounds";
	if (failure)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "%s", failure);
		return 0;
	}
	memset(mask, 0, capacity);
	for (int i = 0; i < count; ++i)
	{
		const NativeWorldCellReference& reference = references[i];
		if (reference.sourceObjectIndex < 0 || reference.sourceObjectIndex >= capacity ||
			reference.selector < -1 || reference.selector > 0x3fff)
		{
			memset(mask, 0, capacity);
			if (error && errorCapacity > 0)
				snprintf(error, errorCapacity, "invalid native world object or event selector");
			return 0;
		}
		const int cell = CellIndex(reference.cellX, reference.cellZ);
		if (cell >= 0 && s_selection.cells[cell] && reference.selector == s_selection.selector)
			mask[reference.sourceObjectIndex] = 1;
	}
	return 1;
}

int NativeWorldVisibility_SelectedCellCount(void)
{
	return s_selection.ready && !s_selection.overflow ? s_selection.selectedCount : 0;
}
