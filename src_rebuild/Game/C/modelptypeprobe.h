#ifndef MODEL_PTYPE_PROBE_H
#define MODEL_PTYPE_PROBE_H

/*
 * E03 block 2a polygon-type probe (pure, asset-free, no GPU).
 *
 * Counts the DECLARED polygon types of reconstructed MODEL polygon blocks
 * without reading anything but the record bytes and without deciding a
 * draw-path policy. The real record stride table (draw.c PolySizes) is passed
 * in by the caller, so this module has no game, PsyCross or GPU dependency.
 *
 * Scope and non-claims:
 *  - The caller supplies the MODEL polygon-block byte offset and the declared
 *    polygon count explicitly (a byte span alone cannot delimit the records,
 *    because the record stride depends on each record's type). `begin` is the
 *    MODEL base; every offset is relative to it.
 *  - The probe walks exactly the declared records, advancing by
 *    `PolySizes[ptype & 0x1f]` (the table the game actually uses).
 *  - Bounded validation: every record start and every `offset + stride` must
 *    stay inside [begin,end). A violation is a named issue and stops the walk.
 *  - A zero stride is its own finding: it is reported (first type and offset)
 *    and the walk never advances past it.
 *  - Per declared ptype counts and the raw draw-path classification inputs are
 *    recorded. No draw-path class is guessed here: the caller combines these
 *    counts with the live shape_flags/flags2.
 *  - The building path's runtime drawn set is {11,21,23}, but the declared
 *    types that reach it are the ones that project there through
 *    ConvertPolygonTypes' even->odd |1 conversion (every even type except 8):
 *    {10,11,20,21,22,23}. buildingPathRecords counts that declared set.
 *  - The decoder allowlist {10,11,20,21,22,23} is mirrored only to count
 *    occurrences outside it. It is not a policy and this module never fails a
 *    model for an unsupported type.
 */

#define MODEL_PTYPE_PROBE_PTYPE_COUNT 32

typedef enum ModelPtypeProbeIssueKind
{
	MODEL_PTYPE_PROBE_ISSUE_NONE = 0,
	MODEL_PTYPE_PROBE_ISSUE_ARGUMENT,          /* null/negative input */
	MODEL_PTYPE_PROBE_ISSUE_RANGE,             /* empty or reversed [begin,end) */
	MODEL_PTYPE_PROBE_ISSUE_POLY_BLOCK_RANGE,  /* polygon block start outside the span */
	MODEL_PTYPE_PROBE_ISSUE_STRIDE_ZERO,       /* PolySizes[ptype] == 0; walk stopped */
	MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE       /* a record start or extent leaves the span */
} ModelPtypeProbeIssueKind;

/* Description of the first stopping issue. `offset` is relative to `begin`;
   `ptype`/`recordIndex`/`offset` are -1 when not applicable. */
typedef struct ModelPtypeProbeIssue
{
	ModelPtypeProbeIssueKind kind;
	int ptype;
	int recordIndex;
	long long offset;
} ModelPtypeProbeIssue;

/* Accumulated counts. `models`/`records` and the arrays grow with every
   successful AddModel call, so the same struct can aggregate a whole pack. */
typedef struct ModelPtypeCounts
{
	int models;                                          /* models walked */
	int records;                                         /* fully walked records */
	int declaredPtypeCounts[MODEL_PTYPE_PROBE_PTYPE_COUNT];
	int outsideAllowlistRecords;                         /* records outside {10,11,20,21,22,23} */
	int outsideAllowlistPtypeCounts[MODEL_PTYPE_PROBE_PTYPE_COUNT];
	int strideZeroRecords;                               /* records that hit a zero stride */
	int firstStrideZeroPtype;                            /* first zero-stride type, or -1 */
	long long firstStrideZeroOffset;                     /* offset of that record, or -1 */
	int invalidRecords;                                  /* records that left the span */
	/* Draw-path classification inputs, not a decided class. ConvertPolygonTypes
	   converts every even declared ptype except 8 by |1, so the declared types
	   the building loop actually draws are {10,11,20,21,22,23} (projecting to
	   {11,21,23}). The tile and sprite loops iterate every record as FT4, so
	   their input count is `records`. */
	int buildingPathRecords;                             /* declared ptype in {10,11,20,21,22,23} */
	int catalogMaterialRecords;                          /* records accepted by ModelTextureLink_GetTextureKey */
} ModelPtypeCounts;

/* Caller-supplied polygon-block description (plain types, MODEL echo). */
typedef struct ModelPtypeView
{
	int polyBlock;          /* byte offset of the polygon block from begin */
	unsigned int numPolys;  /* MODEL::num_polys */
} ModelPtypeView;

/* Walks the declared polygon records of one model into `io`. Returns the
   stopping issue kind (NONE when every record was walked); `issue` receives the
   same description when it is non-NULL. `io` and `view` and `polySizes` must be
   non-NULL; `polySizeCount` must cover every declared ptype. */
ModelPtypeProbeIssueKind ModelPtypeProbe_AddModel(const unsigned char* begin, const unsigned char* end,
	const ModelPtypeView* view, const int* polySizes, int polySizeCount,
	ModelPtypeCounts* io, ModelPtypeProbeIssue* issue);

/* True when `ptype` (already masked with 0x1f) is inside the decoder allowlist
   {10,11,20,21,22,23}. Mirrors SourceModelDecoder_IsSupportedType; report-only. */
int ModelPtypeProbe_IsDecoderAllowlisted(int ptype);

#endif /* MODEL_PTYPE_PROBE_H */
