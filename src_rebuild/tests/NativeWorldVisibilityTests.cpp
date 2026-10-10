#include "../utils/NativeWorldVisibility.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int checks = 0, failures = 0;
static void Check(bool passed, const char* name)
{
	++checks;
	if (!passed) { ++failures; printf("FAIL: %s\n", name); }
}

int main()
{
	unsigned char mask[8] = {};
	char error[128];
	NativeWorldCellReference refs[] = {
		{ 0, 10, 20, -1 }, { 0, 11, 20, -1 }, // straddler through either cell
		{ 1, 10, 20, 14 }, { 2, 11, 20, -1 },
		{ 3, 100, 100, -1 }, { 4, 10, 20, -1 }
	};
	Check(!NativeWorldVisibility_BuildObjectMask(refs, 6, mask, 8, error, sizeof(error)) && error[0],
		"missing frame selection is explicit");
	NativeWorldVisibility_Begin(10, 20, -1);
	NativeWorldVisibility_RecordCell(10, 20);
	NativeWorldVisibility_RecordCell(10, 20);
	Check(NativeWorldVisibility_SelectedCellCount() == 1, "duplicate approved cells counted once");
	Check(NativeWorldVisibility_BuildObjectMask(refs, 6, mask, 8, error, sizeof(error)), "default selection built");
	Check(mask[0] && mask[4] && !mask[1] && !mask[2] && !mask[3],
		"only selected default-list memberships are visible");
	NativeWorldVisibility_Begin(10, 20, -1);
	NativeWorldVisibility_RecordCell(11, 20);
	Check(NativeWorldVisibility_BuildObjectMask(refs, 6, mask, 8, error, sizeof(error)) && mask[0] && mask[2] && !mask[4],
		"straddler survives through a different approved cell");
	NativeWorldVisibility_Begin(10, 20, 14);
	NativeWorldVisibility_RecordCell(10, 20);
	Check(NativeWorldVisibility_BuildObjectMask(refs, 6, mask, 8, error, sizeof(error)) && mask[1] && !mask[0] && !mask[4],
		"event list replaces default list rather than forming a union");
	NativeWorldVisibility_Begin(10, 20, -1);
	memset(mask, 1, sizeof(mask));
	Check(NativeWorldVisibility_BuildObjectMask(refs, 6, mask, 8, error, sizeof(error)) && !mask[0] && !mask[1],
		"an empty approved-cell set clears previous visibility");
	NativeWorldVisibility_RecordCell(32, 20);
	Check(!NativeWorldVisibility_BuildObjectMask(refs, 6, mask, 8, error, sizeof(error)) && strstr(error, "bounds"),
		"selection overflow is diagnosed instead of truncated");
	NativeWorldVisibility_Begin(INT_MAX, INT_MIN, -1);
	NativeWorldVisibility_RecordCell(INT_MAX, INT_MIN);
	NativeWorldCellReference extremes[] = { { 0, INT_MAX, INT_MIN, -1 }, { 1, INT_MIN, INT_MAX, -1 } };
	Check(NativeWorldVisibility_BuildObjectMask(extremes, 2, mask, 8, error, sizeof(error)) && mask[0] && !mask[1],
		"coordinate subtraction handles integer extremes");
	Check(!NativeWorldVisibility_BuildObjectMask(refs, NATIVE_WORLD_MAX_CELL_REFERENCES + 1, mask, 8, error, sizeof(error)),
		"oversized reference inventory rejected before reading it");
	Check(!NativeWorldVisibility_BuildObjectMask(NULL, 1, mask, 8, error, sizeof(error)), "missing references rejected");
	Check(!NativeWorldVisibility_BuildObjectMask(refs, 6, NULL, 8, error, sizeof(error)), "missing output rejected");
	refs[0].sourceObjectIndex = 8;
	Check(!NativeWorldVisibility_BuildObjectMask(refs, 6, mask, 8, error, sizeof(error)) && !mask[0],
		"bad object index rejects and clears the whole result");
	refs[0].sourceObjectIndex = 0; refs[0].selector = -2;
	Check(!NativeWorldVisibility_BuildObjectMask(refs, 6, mask, 8, error, sizeof(error)), "bad event selector rejected");
	Check(NativeWorldVisibility_BuildObjectMask(NULL, 0, mask, 8, error, sizeof(error)), "empty reference inventory supported");
	printf("NativeWorldVisibilityTests: %d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
