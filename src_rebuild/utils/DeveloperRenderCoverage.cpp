#include "DeveloperRenderCoverage.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
const char* const kNames[RENDER_CATEGORY_COUNT] = {
	"road", "scenery", "foliage", "water", "car-body-damage", "wheels",
	"pedestrians", "animated-props", "events", "particles", "decals", "sky",
	"secondary", "hud", "frontend", "loading", "cutscene"
};
DeveloperRenderCoverageCount s_counts[RENDER_CATEGORY_COUNT] = {};
char s_mode[24] = "none";
unsigned int s_frame = 0;
unsigned int s_completed = 0;
bool s_active = false;

bool Valid(DeveloperRenderCategory category)
{
	return category >= 0 && category < RENDER_CATEGORY_COUNT;
}
}

void DeveloperRenderCoverage_BeginFrame(const char* mode, unsigned int frame)
{
	std::memset(s_counts, 0, sizeof(s_counts));
	std::snprintf(s_mode, sizeof(s_mode), "%s", mode ? mode : "unknown");
	s_frame = frame;
	s_active = true;
}

void DeveloperRenderCoverage_RecordLegacy(DeveloperRenderCategory category, const void* begin, const void* end)
{
	if (!s_active || !Valid(category)) return;
	++s_counts[category].requested;
	if (begin && end && begin != end) ++s_counts[category].legacy;
}

void DeveloperRenderCoverage_RecordDirect(DeveloperRenderCategory category, unsigned int count)
{
	if (!s_active || !Valid(category)) return;
	s_counts[category].requested += count;
	s_counts[category].legacy += count;
}

void DeveloperRenderCoverage_RecordExperimental(DeveloperRenderCategory category)
{
	if (!s_active || !Valid(category)) return;
	++s_counts[category].requested;
	++s_counts[category].experimental;
}

void DeveloperRenderCoverage_EndFrame(void)
{
	if (!s_active) return;
	s_active = false;
	const char* enabled = std::getenv("PSYX_COVERAGE_LOG");
	if (!enabled || !enabled[0] || enabled[0] == '0') return;
	// First sample and every 120 completed producer frames, not every draw.
	if (s_completed++ % 120 != 0) return;
	FILE* file = std::fopen("render_coverage.log", "a");
	if (!file) { std::fprintf(stderr, "Cannot append render_coverage.log\n"); return; }
	for (int i = 0; i < RENDER_CATEGORY_COUNT; ++i)
	{
		const DeveloperRenderCoverageCount& count = s_counts[i];
		std::fprintf(file, "coverage: mode=%s frame=%u category=%s requested=%u legacy_batches=%u experimental=%u native=%u\n",
			s_mode, s_frame, kNames[i], count.requested, count.legacy, count.experimental, count.native);
	}
	if (std::fclose(file) != 0)
		std::fprintf(stderr, "Cannot finish render_coverage.log write\n");
}

const char* DeveloperRenderCoverage_GetName(DeveloperRenderCategory category)
{
	return Valid(category) ? kNames[category] : "unknown";
}

DeveloperRenderCoverageCount DeveloperRenderCoverage_GetCount(DeveloperRenderCategory category)
{
	const DeveloperRenderCoverageCount empty = {};
	return Valid(category) ? s_counts[category] : empty;
}

const char* DeveloperRenderCoverage_GetMode(void) { return s_mode; }
