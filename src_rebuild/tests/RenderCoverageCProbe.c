#include "../utils/DeveloperRenderCoverage.h"

/* Compile as C and link to the C++ implementation: public linkage and the
 * struct-return ABI must remain usable by reconstructed C producers. */
int RenderCoverageCProbe(void)
{
	DeveloperRenderCoverageCount count;
	DeveloperRenderCoverage_BeginFrame("c-consumer", 7);
	DeveloperRenderCoverage_RecordDirect(RENDER_CATEGORY_LOADING, 2);
	DeveloperRenderCoverage_EndFrame();
	count = DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_LOADING);
	return count.requested == 2 && count.legacy == 2 && count.native == 0;
}
