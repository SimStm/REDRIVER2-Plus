#include "modelptypeprobe.h"

/*
 * Pure implementation: no game, PsyCross or GPU includes. See the header for
 * the contract and the non-claims.
 */

static int PtypeIsAllowlisted(int ptype)
{
	switch (ptype)
	{
	case 10:
	case 11:
	case 20:
	case 21:
	case 22:
	case 23:
		return 1;
	default:
		return 0;
	}
}

int ModelPtypeProbe_IsDecoderAllowlisted(int ptype)
{
	return PtypeIsAllowlisted(ptype);
}

static void SetIssue(ModelPtypeProbeIssue* issue, ModelPtypeProbeIssueKind kind,
	int ptype, int recordIndex, long long offset)
{
	if (issue == 0)
		return;

	issue->kind = kind;
	issue->ptype = ptype;
	issue->recordIndex = recordIndex;
	issue->offset = offset;
}

ModelPtypeProbeIssueKind ModelPtypeProbe_AddModel(const unsigned char* begin, const unsigned char* end,
	const ModelPtypeView* view, const int* polySizes, int polySizeCount,
	ModelPtypeCounts* io, ModelPtypeProbeIssue* issue)
{
	SetIssue(issue, MODEL_PTYPE_PROBE_ISSUE_NONE, -1, -1, -1);

	if (io == 0 || view == 0 || polySizes == 0 || polySizeCount <= 0)
	{
		SetIssue(issue, MODEL_PTYPE_PROBE_ISSUE_ARGUMENT, -1, -1, -1);
		return MODEL_PTYPE_PROBE_ISSUE_ARGUMENT;
	}

	if (begin == 0 || end == 0 || begin >= end)
	{
		SetIssue(issue, MODEL_PTYPE_PROBE_ISSUE_RANGE, -1, -1, -1);
		return MODEL_PTYPE_PROBE_ISSUE_RANGE;
	}

	const long long span = (long long)(end - begin);

	if (view->polyBlock < 0 || (long long)view->polyBlock > span)
	{
		SetIssue(issue, MODEL_PTYPE_PROBE_ISSUE_POLY_BLOCK_RANGE, -1, -1, (long long)view->polyBlock);
		return MODEL_PTYPE_PROBE_ISSUE_POLY_BLOCK_RANGE;
	}

	io->models++;

	const unsigned char* cursor = begin + view->polyBlock;

	for (unsigned int recordIndex = 0; recordIndex < view->numPolys; ++recordIndex)
	{
		if (cursor >= end)
		{
			io->invalidRecords++;
			SetIssue(issue, MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE, -1, (int)recordIndex, (long long)(cursor - begin));
			return MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE;
		}

		const int ptype = (int)(*cursor & 0x1f);
		const long long offset = (long long)(cursor - begin);

		if (ptype >= polySizeCount)
		{
			io->invalidRecords++;
			SetIssue(issue, MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE, ptype, (int)recordIndex, offset);
			return MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE;
		}

		const int stride = polySizes[ptype];

		if (stride == 0)
		{
			// A zero stride is a finding of its own: record the declared type,
			// then stop without advancing (a stride-0 walk has no next record).
			// Its declared type still counts toward the allowlist histogram.
			io->declaredPtypeCounts[ptype]++;
			io->strideZeroRecords++;
			if (io->firstStrideZeroPtype < 0)
			{
				io->firstStrideZeroPtype = ptype;
				io->firstStrideZeroOffset = offset;
			}
			if (!PtypeIsAllowlisted(ptype))
			{
				io->outsideAllowlistRecords++;
				io->outsideAllowlistPtypeCounts[ptype]++;
			}
			SetIssue(issue, MODEL_PTYPE_PROBE_ISSUE_STRIDE_ZERO, ptype, (int)recordIndex, offset);
			return MODEL_PTYPE_PROBE_ISSUE_STRIDE_ZERO;
		}

		if (offset + stride > span)
		{
			io->invalidRecords++;
			SetIssue(issue, MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE, ptype, (int)recordIndex, offset);
			return MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE;
		}

		io->records++;
		io->declaredPtypeCounts[ptype]++;

		// ConvertPolygonTypes converts every even ptype except 8 by |1, so the
		// declared types that project into the runtime drawn set {11,21,23} are
		// {10,11,20,21,22,23}.
		switch (ptype)
		{
		case 10:
		case 11:
		case 20:
		case 21:
		case 22:
		case 23:
			io->buildingPathRecords++;
			break;
		default:
			break;
		}

		switch (ptype)
		{
		case 4:
		case 5:
		case 6:
		case 7:
		case 20:
		case 21:
		case 22:
		case 23:
			io->catalogMaterialRecords++;
			break;
		default:
			break;
		}

		if (!PtypeIsAllowlisted(ptype))
		{
			io->outsideAllowlistRecords++;
			io->outsideAllowlistPtypeCounts[ptype]++;
		}

		cursor += stride;
	}

	return MODEL_PTYPE_PROBE_ISSUE_NONE;
}
