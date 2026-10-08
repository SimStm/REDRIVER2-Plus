#include "../utils/DeveloperRenderCoverage.h"
#include <cstdio>
#include <cstring>

static int failures = 0;
extern "C" int RenderCoverageCProbe(void);
static void Check(bool value, const char* name)
{
	std::printf("%s: %s\n", value ? "PASS" : "FAIL", name);
	if (!value) ++failures;
}

int main()
{
	char buffer[2] = {};
	DeveloperRenderCoverage_RecordDirect(RENDER_CATEGORY_ROAD, 8);
	Check(DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_ROAD).legacy == 0, "inactive producers are ignored");
	DeveloperRenderCoverage_BeginFrame("split-screen", 42);
	DeveloperRenderCoverage_RecordLegacy(RENDER_CATEGORY_SCENERY, buffer, buffer);
	DeveloperRenderCoverageCount count = DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_SCENERY);
	Check(count.requested == 1 && count.legacy == 0, "culled request is not emitted coverage");
	DeveloperRenderCoverage_RecordLegacy(RENDER_CATEGORY_SCENERY, buffer, buffer + 1);
	DeveloperRenderCoverage_RecordExperimental(RENDER_CATEGORY_SCENERY);
	count = DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_SCENERY);
	Check(count.requested == 3 && count.legacy == 1 && count.experimental == 1 && count.native == 0,
		"legacy and experimental emissions never imply native coverage");
	DeveloperRenderCoverage_RecordLegacy(RENDER_CATEGORY_WATER, NULL, buffer);
	Check(DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_WATER).legacy == 0, "invalid range cannot claim emission");
	DeveloperRenderCoverage_RecordDirect(RENDER_CATEGORY_FRONTEND, 7);
	Check(DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_FRONTEND).legacy == 7, "persistent packets are counted explicitly");
	DeveloperRenderCoverage_RecordLegacy(RENDER_CATEGORY_WHEELS, buffer, buffer + 1);
	DeveloperRenderCoverage_RecordLegacy(RENDER_CATEGORY_WHEELS, buffer, buffer + 1);
	Check(DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_WHEELS).legacy == 2, "multiple views aggregate before frame completion");
	DeveloperRenderCoverage_EndFrame();
	DeveloperRenderCoverage_RecordDirect(RENDER_CATEGORY_WHEELS, 3);
	Check(DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_WHEELS).legacy == 2, "completed frame remains inspectable and immutable");
	Check(std::strcmp(DeveloperRenderCoverage_GetMode(), "split-screen") == 0, "mode survives completion");
	DeveloperRenderCoverage_BeginFrame("loading", 0);
	Check(DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_WHEELS).legacy == 0, "mode change resets every category");
	DeveloperRenderCoverage_RecordDirect(static_cast<DeveloperRenderCategory>(-1), 99);
	DeveloperRenderCoverage_RecordDirect(RENDER_CATEGORY_COUNT, 99);
	Check(DeveloperRenderCoverage_GetCount(RENDER_CATEGORY_COUNT).requested == 0, "invalid categories cannot corrupt counts");
	bool named = true;
	for (int i = 0; i < RENDER_CATEGORY_COUNT; ++i)
		named = named && std::strcmp(DeveloperRenderCoverage_GetName(static_cast<DeveloperRenderCategory>(i)), "unknown") != 0;
	Check(named, "all migration categories are observable");
	DeveloperRenderCoverage_EndFrame();
	Check(RenderCoverageCProbe() != 0, "C consumer links and returns count records correctly");
	return failures ? 1 : 0;
}
