#ifndef DEVELOPER_RENDER_COVERAGE_H
#define DEVELOPER_RENDER_COVERAGE_H

/* Producer diagnostics, not GPU draw or visible-object counts. Native means
 * the new R01+ scene; the earlier modern adapter is counted separately. */
typedef enum DeveloperRenderCategory
{
	RENDER_CATEGORY_ROAD,
	RENDER_CATEGORY_SCENERY,
	RENDER_CATEGORY_FOLIAGE,
	RENDER_CATEGORY_WATER,
	RENDER_CATEGORY_CAR_BODY_DAMAGE,
	RENDER_CATEGORY_WHEELS,
	RENDER_CATEGORY_PEDESTRIANS,
	RENDER_CATEGORY_ANIMATED_PROPS,
	RENDER_CATEGORY_EVENTS,
	RENDER_CATEGORY_PARTICLES,
	RENDER_CATEGORY_DECALS,
	RENDER_CATEGORY_SKY,
	RENDER_CATEGORY_SECONDARY,
	RENDER_CATEGORY_HUD,
	RENDER_CATEGORY_FRONTEND,
	RENDER_CATEGORY_LOADING,
	RENDER_CATEGORY_CUTSCENE,
	RENDER_CATEGORY_COUNT
} DeveloperRenderCategory;

typedef struct DeveloperRenderCoverageCount
{
	unsigned int requested;
	unsigned int legacy;
	unsigned int experimental;
	unsigned int native;
} DeveloperRenderCoverageCount;

#ifdef __cplusplus
extern "C" {
#endif
void DeveloperRenderCoverage_BeginFrame(const char* mode, unsigned int frame);
void DeveloperRenderCoverage_RecordLegacy(DeveloperRenderCategory category, const void* begin, const void* end);
void DeveloperRenderCoverage_RecordDirect(DeveloperRenderCategory category, unsigned int count);
void DeveloperRenderCoverage_RecordExperimental(DeveloperRenderCategory category);
void DeveloperRenderCoverage_EndFrame(void);
const char* DeveloperRenderCoverage_GetName(DeveloperRenderCategory category);
DeveloperRenderCoverageCount DeveloperRenderCoverage_GetCount(DeveloperRenderCategory category);
const char* DeveloperRenderCoverage_GetMode(void);
#ifdef __cplusplus
}
#endif

/* Keep the draw expression at its original execution point. Aggregate and
 * child categories can overlap; never sum them into GPU draw totals. */
#ifndef PSX
#define DEVELOPER_COVERAGE_DRAW(category, expression) do { \
	const void* coverageBegin = current->primptr; \
	expression; \
	DeveloperRenderCoverage_RecordLegacy(category, coverageBegin, current->primptr); \
} while (0)
#else
#define DEVELOPER_COVERAGE_DRAW(category, expression) do { expression; } while (0)
#endif
#endif
