// Standalone regression tests for the E03 block 2a polygon-type probe
// (Game/C/modelptypeprobe.*). The probe is pure and asset-free, so its walk,
// bounded validation, zero-stride finding and per-type/class-input counts can be
// pinned with synthetic spans. Build and run instructions are in tests/README.md.

#include "../Game/C/modelptypeprobe.h"

#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(condition) \
	do { \
		g_checks++; \
		if (!(condition)) { \
			g_failures++; \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		} \
	} while (0)

// Copy of the real record stride table (draw.c PolySizes) for the first 32
// declared types. Indexed by ptype & 0x1f exactly as the game indexes it.
static const int kPolySizes[32] =
{
	8,  12, 16, 24,
	20, 20, 28, 32,
	8,  12, 16, 16,
	0,  0,  0,  0,
	12, 12, 12, 16,
	20, 20, 24, 24,
	0,  0,  0,  0,
	0,  0,  0,  0,
};

static ModelPtypeCounts NewCounts()
{
	ModelPtypeCounts counts;
	memset(&counts, 0, sizeof(counts));
	counts.firstStrideZeroPtype = -1;
	counts.firstStrideZeroOffset = -1;
	return counts;
}

// Quad (11) + flat triangle (20): both project into the runtime drawn set
// {11,21,23} (20 -> 21 via ConvertPolygonTypes' |1), so both count as
// building-drawn; only 20 is in the catalog material set.
static void TestQuadAndTriangle()
{
	unsigned char bytes[64] = {};
	bytes[8] = 11;   // stride 16
	bytes[24] = 20;  // stride 20

	ModelPtypeView view = { 8, 2 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	const ModelPtypeProbeIssueKind kind =
		ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue);

	CHECK(kind == MODEL_PTYPE_PROBE_ISSUE_NONE);
	CHECK(issue.kind == MODEL_PTYPE_PROBE_ISSUE_NONE);
	CHECK(counts.models == 1 && counts.records == 2);
	CHECK(counts.declaredPtypeCounts[11] == 1 && counts.declaredPtypeCounts[20] == 1);
	CHECK(counts.buildingPathRecords == 2);
	CHECK(counts.catalogMaterialRecords == 1);
	CHECK(counts.outsideAllowlistRecords == 0);
	CHECK(counts.strideZeroRecords == 0 && counts.firstStrideZeroPtype == -1);
	CHECK(counts.invalidRecords == 0);
}

// The building loop draws only {11,21,23}, but ConvertPolygonTypes converts
// every even declared ptype except 8 by |1 first. So the declared types that
// reach the building loop are {10,11,20,21,22,23}; even types that project
// elsewhere (4 -> 5, 8 stays 8) are not building-drawn.
static void TestBuildingDrawnProjection()
{
	unsigned char bytes[96] = {};
	bytes[0] = 10;   // stride 16, projects to 11
	bytes[16] = 20;  // stride 20, projects to 21
	bytes[36] = 22;  // stride 24, projects to 23
	bytes[60] = 8;   // stride 8, NOT converted (excluded)
	bytes[68] = 4;   // stride 20, projects to 5 (not drawn)

	ModelPtypeView view = { 0, 5 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	const ModelPtypeProbeIssueKind kind =
		ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue);

	CHECK(kind == MODEL_PTYPE_PROBE_ISSUE_NONE);
	CHECK(counts.records == 5);
	CHECK(counts.declaredPtypeCounts[10] == 1 && counts.declaredPtypeCounts[20] == 1 &&
		counts.declaredPtypeCounts[22] == 1 && counts.declaredPtypeCounts[8] == 1 &&
		counts.declaredPtypeCounts[4] == 1);
	CHECK(counts.buildingPathRecords == 3); // 10, 20, 22
	CHECK(counts.catalogMaterialRecords == 3); // 4, 20, 22
	CHECK(counts.outsideAllowlistRecords == 2); // 8, 4
	CHECK(counts.outsideAllowlistPtypeCounts[8] == 1 && counts.outsideAllowlistPtypeCounts[4] == 1);
}

// The textured 4..7 bank: every record is outside the decoder allowlist but is
// still counted (never failed), and all are in the catalog material set.
static void TestTextureBankOutsideAllowlist()
{
	unsigned char bytes[100] = {};
	bytes[0] = 4;    // stride 20
	bytes[20] = 5;   // stride 20
	bytes[40] = 6;   // stride 28
	bytes[68] = 7;   // stride 32 (ends exactly at the span)

	ModelPtypeView view = { 0, 4 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	const ModelPtypeProbeIssueKind kind =
		ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue);

	CHECK(kind == MODEL_PTYPE_PROBE_ISSUE_NONE);
	CHECK(counts.records == 4);
	CHECK(counts.outsideAllowlistRecords == 4);
	CHECK(counts.outsideAllowlistPtypeCounts[4] == 1 && counts.outsideAllowlistPtypeCounts[7] == 1);
	CHECK(counts.catalogMaterialRecords == 4);
	CHECK(counts.buildingPathRecords == 0);
}

// A zero stride is its own finding: the declared type is recorded, the walk
// stops and never advances past it, and later records are not counted.
static void TestStrideZeroStopsTheWalk()
{
	unsigned char bytes[64] = {};
	bytes[0] = 11;   // stride 16, counted
	bytes[16] = 12;  // stride 0, stops the walk
	bytes[17] = 11;  // must never be reached

	ModelPtypeView view = { 0, 3 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	const ModelPtypeProbeIssueKind kind =
		ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue);

	CHECK(kind == MODEL_PTYPE_PROBE_ISSUE_STRIDE_ZERO);
	CHECK(issue.kind == MODEL_PTYPE_PROBE_ISSUE_STRIDE_ZERO);
	CHECK(issue.ptype == 12 && issue.offset == 16 && issue.recordIndex == 1);
	CHECK(counts.records == 1);
	CHECK(counts.declaredPtypeCounts[11] == 1 && counts.declaredPtypeCounts[12] == 1);
	CHECK(counts.strideZeroRecords == 1);
	CHECK(counts.firstStrideZeroPtype == 12 && counts.firstStrideZeroOffset == 16);
	CHECK(counts.outsideAllowlistRecords == 1 && counts.outsideAllowlistPtypeCounts[12] == 1);
	CHECK(counts.declaredPtypeCounts[11] == 1); // no third record
}

// A record whose extent leaves the span is a named range issue.
static void TestRecordOutsideSpan()
{
	unsigned char bytes[24] = {};
	bytes[0] = 11;   // stride 16, fits
	bytes[16] = 11;  // 16 + 16 = 32 > 24

	ModelPtypeView view = { 0, 2 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	const ModelPtypeProbeIssueKind kind =
		ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue);

	CHECK(kind == MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE);
	CHECK(issue.kind == MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE);
	CHECK(issue.recordIndex == 1 && issue.ptype == 11 && issue.offset == 16);
	CHECK(counts.records == 1 && counts.invalidRecords == 1);
}

// A polygon block start outside the span is a named range issue.
static void TestPolyBlockOutsideSpan()
{
	unsigned char bytes[64] = {};
	ModelPtypeView view = { 100, 1 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	const ModelPtypeProbeIssueKind kind =
		ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue);

	CHECK(kind == MODEL_PTYPE_PROBE_ISSUE_POLY_BLOCK_RANGE);
	CHECK(issue.kind == MODEL_PTYPE_PROBE_ISSUE_POLY_BLOCK_RANGE);
	CHECK(counts.models == 0 && counts.records == 0);
}

static void TestArgumentsAndEmpty()
{
	unsigned char bytes[16] = {};
	ModelPtypeView view = { 0, 1 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	CHECK(ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, NULL, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_ARGUMENT);
	CHECK(ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), NULL, kPolySizes, 32, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_ARGUMENT);
	CHECK(ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, NULL, 0, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_ARGUMENT);

	// Empty or reversed span.
	CHECK(ModelPtypeProbe_AddModel(bytes, bytes, &view, kPolySizes, 32, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_RANGE);
	CHECK(ModelPtypeProbe_AddModel(bytes + 4, bytes, &view, kPolySizes, 32, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_RANGE);

	// A null issue output still reports through the return value.
	CHECK(ModelPtypeProbe_AddModel(bytes, bytes, &view, kPolySizes, 32, &counts, NULL) ==
		MODEL_PTYPE_PROBE_ISSUE_RANGE);

	// Zero declared polygons is valid and counts one model.
	counts = NewCounts();
	view.numPolys = 0;
	CHECK(ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_NONE);
	CHECK(counts.models == 1 && counts.records == 0);
}

// The same accumulator aggregates several models.
static void TestAccumulation()
{
	unsigned char bytes[64] = {};
	bytes[0] = 11;
	bytes[16] = 21;
	bytes[36] = 23;

	ModelPtypeView view = { 0, 3 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	CHECK(ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_NONE);
	CHECK(ModelPtypeProbe_AddModel(bytes, bytes + sizeof(bytes), &view, kPolySizes, 32, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_NONE);

	CHECK(counts.models == 2 && counts.records == 6);
	CHECK(counts.buildingPathRecords == 6);
	CHECK(counts.catalogMaterialRecords == 4); // 21/23 only
}

static void TestAllowlistHelper()
{
	CHECK(ModelPtypeProbe_IsDecoderAllowlisted(10));
	CHECK(ModelPtypeProbe_IsDecoderAllowlisted(11));
	CHECK(ModelPtypeProbe_IsDecoderAllowlisted(20));
	CHECK(ModelPtypeProbe_IsDecoderAllowlisted(21));
	CHECK(ModelPtypeProbe_IsDecoderAllowlisted(22));
	CHECK(ModelPtypeProbe_IsDecoderAllowlisted(23));
	CHECK(!ModelPtypeProbe_IsDecoderAllowlisted(4));
	CHECK(!ModelPtypeProbe_IsDecoderAllowlisted(8));
	CHECK(!ModelPtypeProbe_IsDecoderAllowlisted(12));
	CHECK(!ModelPtypeProbe_IsDecoderAllowlisted(31));
}

// The game resets its per-level accumulator at level start
// (AssetCatalogGame_BeginLevel; the game bridge is not linked into this
// standalone suite). This pins the contract a fresh accumulator must satisfy:
// it reports only the current batch and never carries previous records.
static void TestPerLevelReset()
{
	unsigned char levelA[48] = {};
	levelA[0] = 11;   // stride 16
	levelA[16] = 21;  // stride 20

	ModelPtypeView viewA = { 0, 2 };
	ModelPtypeCounts counts = NewCounts();
	ModelPtypeProbeIssue issue;

	CHECK(ModelPtypeProbe_AddModel(levelA, levelA + sizeof(levelA), &viewA, kPolySizes, 32, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_NONE);
	CHECK(counts.models == 1 && counts.records == 2 && counts.buildingPathRecords == 2);
	CHECK(counts.declaredPtypeCounts[11] == 1 && counts.declaredPtypeCounts[21] == 1);

	// Level boundary: the accumulator is discarded and starts clean.
	counts = NewCounts();

	unsigned char levelB[48] = {};
	levelB[0] = 23;  // stride 24

	ModelPtypeView viewB = { 0, 1 };
	CHECK(ModelPtypeProbe_AddModel(levelB, levelB + sizeof(levelB), &viewB, kPolySizes, 32, &counts, &issue) ==
		MODEL_PTYPE_PROBE_ISSUE_NONE);
	CHECK(counts.models == 1 && counts.records == 1 && counts.buildingPathRecords == 1);
	CHECK(counts.declaredPtypeCounts[23] == 1);
	CHECK(counts.declaredPtypeCounts[11] == 0 && counts.declaredPtypeCounts[21] == 0);
}

int main()
{
	TestQuadAndTriangle();
	TestBuildingDrawnProjection();
	TestTextureBankOutsideAllowlist();
	TestStrideZeroStopsTheWalk();
	TestRecordOutsideSpan();
	TestPolyBlockOutsideSpan();
	TestArgumentsAndEmpty();
	TestAccumulation();
	TestAllowlistHelper();
	TestPerLevelReset();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
