// Standalone Windows regression test; no game assets or GL context required.
// Include the implementation to exercise the same private WIC encoder.
#include "../utils/HdTextureOverrides.cpp"

// Renderer stubs: export tests supply synthetic VRAM and never create GL objects.
unsigned int PsyX_CreateRGBATexture(int, int, const unsigned char*) { return 1; }
void PsyX_DestroyRGBATexture(unsigned int) {}
int PsyX_RegisterTextureOverride(const PsyXTextureOverride*) { return 1; }
void PsyX_RemoveTextureOverride(int) {}
void PsyX_Inspector_ClearSelection() {}
void PsyX_SetTextureOverridesEnabled(int) {}
void PsyX_GetTextureOverrideStats(PsyXTextureOverrideStats* stats) { memset(stats, 0, sizeof(*stats)); }
void GR_ReadVRAM(unsigned short* dst, int, int, int width, int height)
{
	for (int i = 0; i < width * height; ++i) dst[i] = 0x001f;

	// Zero texture word at page X base 64 so the sampled colour index is 0,
	// and an STP (semi-transparent) red CLUT entry at CLUT base (16, 0).
	dst[64] = 0x0000;
	dst[16] = 0x801f;
}

static int failures = 0;
static void Check(bool passed, const char* description)
{
	printf("%s: %s\n", passed ? "PASS" : "FAIL", description);
	if (!passed) ++failures;
}

int main()
{
	unsigned char pixels[4 * 4 * 4];
	for (int i = 0; i < 16; ++i)
	{
		pixels[i * 4] = 10; pixels[i * 4 + 1] = 40;
		pixels[i * 4 + 2] = 200; pixels[i * 4 + 3] = (unsigned char)(i * 16);
	}
	const bool saved = SavePngRgba("test.png", pixels, 4, 4);
	WIN32_FILE_ATTRIBUTE_DATA file = {};
	GetFileAttributesExA("test.png", GetFileExInfoStandard, &file);
	Check(saved && file.nFileSizeLow > 0, "WIC PNG encoder creates a nonempty file");
	unsigned char* decoded = NULL;
	int width = 0, height = 0;
	Check(LoadPngRgba("test.png", &decoded, &width, &height) && width == 4 && height == 4 &&
		!memcmp(pixels, decoded, sizeof(pixels)), "RGBA -> BGRA -> PNG -> RGBA preserves all channels including alpha");
	free(decoded);
	char status[512];
	Check(HdTextureOverrides_ExportInspectorReport("regression", "first report", status, sizeof(status)), "report first export");
	Check(HdTextureOverrides_ExportInspectorReport("regression", "replacement report", status, sizeof(status)), "report repeated export");
	int bytes = 0;
	char* report = ReadTextFile("mods/regression/assets/inspector/selection.txt", &bytes);
	Check(report && !strcmp(report, "replacement report"), "report replacement contains the new content");
	free(report);
	const char* destination = "mods/regression/assets/inspector/RED_p1_i1.png";
	Check(HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "RED", 1, 1, "regression", status, sizeof(status)), "VRAM texture first export");
	Check(HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "RED", 1, 1, "regression", status, sizeof(status)), "VRAM texture repeated export");
	decoded = NULL;
	Check(LoadPngRgba(destination, &decoded, &width, &height) && width == 4 && height == 4 &&
		decoded[0] == 255 && decoded[1] == 0 && decoded[2] == 0 && decoded[3] == 255, "exported original texture decodes to synthetic red VRAM pixels");
	free(decoded);
	char temporary[MAX_PATH];
	Check(InspectorExport_Temporary(destination, temporary, status, sizeof(status)) &&
		!InspectorExport_Commit(temporary, destination, status, sizeof(status)), "empty temporary export cannot replace a completed PNG");
	decoded = NULL;
	Check(LoadPngRgba(destination, &decoded, &width, &height), "previous PNG remains readable after failed replacement");
	free(decoded);
	Check(HdTextureOverrides_ExportTexture(1, 1, 0, 0, 4, 4, "SEMI", 1, 5, "regression", status, sizeof(status)), "STP texture export");
	decoded = NULL;
	Check(LoadPngRgba("mods/regression/assets/inspector/SEMI_p1_i5.png", &decoded, &width, &height) &&
		decoded[3] == 128, "PSX STP colours export as half alpha");
	free(decoded);
	Check(!HdTextureOverrides_ExportTexture(256, 0, 255, 0, 4, 4, "RED", 1, 1, "regression", status, sizeof(status)), "out-of-range source rejected");
	Check(!HdTextureOverrides_ExportInspectorReport("../outside", "invalid", status, sizeof(status)), "unsafe mod id rejected");

	// Append-only manifest merging.
	Check(HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "BLUE", 1, 2, "regression", status, sizeof(status)), "second distinct texture export");
	int mergedBytes = 0;
	char* mergedManifest = ReadTextFile("mods/regression/manifest.json", &mergedBytes);
	int redCount = 0;
	if (mergedManifest)
		for (char* scan = mergedManifest; (scan = strstr(scan, "\"RED\"")) != NULL; ++scan) ++redCount;
	Check(mergedManifest && strstr(mergedManifest, "\"RED\"") && strstr(mergedManifest, "\"BLUE\""), "manifest appends a second texture instead of replacing the first");
	Check(redCount == 1, "repeated export leaves exactly one registration per texture");
	free(mergedManifest);

	const char* customManifest = "{ \"schemaVersion\": 1, \"id\": \"regression\", \"custom\": { \"keep\": true }, \"textures\": [ { \"texture\": \"RED\", \"texturePage\": 1, \"textureIndex\": 1, \"file\": \"assets/inspector/RED_p1_i1.png\" } ] }";
	Check(InspectorExport_WriteText("mods/regression/manifest.json", customManifest, status, sizeof(status)), "rewrite manifest containing an unknown field");
	Check(HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "GREEN", 1, 3, "regression", status, sizeof(status)), "export into a manifest with unknown fields");
	mergedManifest = ReadTextFile("mods/regression/manifest.json", &mergedBytes);
	Check(mergedManifest && strstr(mergedManifest, "\"keep\"") && strstr(mergedManifest, "\"GREEN\"") && strstr(mergedManifest, "\"RED\""), "merge preserves unknown fields and existing registrations");
	free(mergedManifest);

	const char* wildcardManifest = "{ \"schemaVersion\": 1, \"id\": \"regression\", \"textures\": [ { \"texture\": \"RED\", \"file\": \"assets/inspector/RED_wild.png\" } ] }";
	Check(InspectorExport_WriteText("mods/regression/manifest.json", wildcardManifest, status, sizeof(status)), "write a wildcard manifest");
	Check(HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "RED", 1, 9, "regression", status, sizeof(status)), "export alongside a wildcard entry");
	mergedManifest = ReadTextFile("mods/regression/manifest.json", &mergedBytes);
	Check(mergedManifest && strstr(mergedManifest, "RED_wild.png") && strstr(mergedManifest, "RED_p1_i9.png"), "wildcard entry is preserved and a specific entry is appended");
	free(mergedManifest);

	const char* duplicateManifest = "{ \"schemaVersion\": 1, \"id\": \"regression\", \"textures\": [ { \"texture\": \"RED\", \"texturePage\": 1, \"textureIndex\": 9, \"file\": \"a.png\" }, { \"texture\": \"RED\", \"texturePage\": 1, \"textureIndex\": 9, \"file\": \"b.png\" } ] }";
	Check(InspectorExport_WriteText("mods/regression/manifest.json", duplicateManifest, status, sizeof(status)), "write a manifest with a duplicate legacy pair");
	Check(HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "RED", 1, 9, "regression", status, sizeof(status)), "export with duplicate legacy entries");
	mergedManifest = ReadTextFile("mods/regression/manifest.json", &mergedBytes);
	int duplicateCount = 0;
	if (mergedManifest)
		for (char* scan = mergedManifest; (scan = strstr(scan, "\"textureIndex\": 9")) != NULL; ++scan) ++duplicateCount;
	Check(duplicateCount == 2, "duplicate legacy entries are left untouched and no third entry is added");
	free(mergedManifest);

	Check(InspectorExport_WriteText("mods/regression/manifest.json", "{ this is not json", status, sizeof(status)), "write a malformed manifest");
	Check(!HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "YELLOW", 1, 4, "regression", status, sizeof(status)), "malformed manifest blocks registration");
	int malformedBytes = 0;
	char* malformed = ReadTextFile("mods/regression/manifest.json", &malformedBytes);
	Check(malformed && strstr(malformed, "this is not json"), "malformed manifest is left untouched");
	free(malformed);
	decoded = NULL;
	Check(LoadPngRgba("mods/regression/assets/inspector/YELLOW_p1_i4.png", &decoded, &width, &height) && width == 4, "PNG is published even when registration fails");
	free(decoded);
	Check(InspectorExport_WriteText("car.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", status, sizeof(status)) &&
		InspectorExport_WriteText("car.obj", "v 0 0 1\nv 1 0 1\nv 0 1 1\nf 1 2 3\n", status, sizeof(status)), "OBJ text writer supports repeat export");

	// Batch export: one file per texture identity, model references are
	// metadata, and a failed resource is reported without aborting the batch.
	HdTextureOverrideBatchItem batchItems[2] = {};
	for (int i = 0; i < 2; ++i)
	{
		batchItems[i].tpage = 256; batchItems[i].clut = 0;
		batchItems[i].u = 0; batchItems[i].v = 0; batchItems[i].width = 4; batchItems[i].height = 4;
		snprintf(batchItems[i].textureName, sizeof(batchItems[i].textureName), "SHARED");
		batchItems[i].texturePage = 2; batchItems[i].textureIndex = 2;
		snprintf(batchItems[i].filenameSuffix, sizeof(batchItems[i].filenameSuffix), "POLICE CAR/01");
		snprintf(batchItems[i].modelReferences[0], HD_TEXTURE_MODEL_REFERENCE_CAPACITY, "model:0:0:7");
		snprintf(batchItems[i].modelReferences[1], HD_TEXTURE_MODEL_REFERENCE_CAPACITY, "model:0:0:9");
		batchItems[i].modelReferenceCount = 2;
	}
	HdTextureOverrideBatchResult batch = {};
	Check(HdTextureOverrides_ExportTextureBatch("batchtest", batchItems, 2, &batch), "batch export request succeeds");
	Check(batch.requested == 2 && batch.exported == 1 && batch.duplicates == 1 && batch.failed == 0,
		"batch deduplicates an identical identity and reports the omitted duplicate");
	decoded = NULL;
	Check(LoadPngRgba("mods/batchtest/assets/inspector/SHARED_p2_i2_POLICE_CAR_01.png", &decoded, &width, &height) && width == 4,
		"batch writes one sanitized, model-suffixed PNG per identity");
	free(decoded);
	int batchBytes = 0;
	char* batchManifest = ReadTextFile("mods/batchtest/manifest.json", &batchBytes);
	Check(batchManifest && strstr(batchManifest, "\"modelReferences\"") &&
		strstr(batchManifest, "\"model:0:0:7\"") && strstr(batchManifest, "\"model:0:0:9\""),
		"batch manifest records the shared model references as metadata");
	int sharedCount = 0;
	if (batchManifest)
		for (char* scan = batchManifest; (scan = strstr(scan, "\"SHARED\"")) != NULL; ++scan) ++sharedCount;
	Check(sharedCount == 1, "a shared texture is registered exactly once");
	free(batchManifest);

	// References are not the override key: a second export with different
	// references must not add a duplicate registration.
	batchItems[0].modelReferenceCount = 1;
	snprintf(batchItems[0].modelReferences[0], HD_TEXTURE_MODEL_REFERENCE_CAPACITY, "model:0:0:11");
	Check(HdTextureOverrides_ExportTextureBatch("batchtest", batchItems, 1, &batch), "re-export of a registered texture succeeds");
	batchManifest = ReadTextFile("mods/batchtest/manifest.json", &batchBytes);
	sharedCount = 0;
	if (batchManifest)
		for (char* scan = batchManifest; (scan = strstr(scan, "\"SHARED\"")) != NULL; ++scan) ++sharedCount;
	Check(sharedCount == 1, "changing model references does not change the override identity");
	free(batchManifest);

	HdTextureOverrideBatchItem failing = {};
	failing.tpage = 256; failing.clut = 0; failing.u = 0; failing.v = 255; failing.width = 4; failing.height = 4;
	snprintf(failing.textureName, sizeof(failing.textureName), "BROKEN");
	failing.texturePage = 3; failing.textureIndex = 3;
	HdTextureOverrideBatchResult failedBatch = {};
	Check(HdTextureOverrides_ExportTextureBatch("batchtest", &failing, 1, &failedBatch) &&
		failedBatch.failed == 1 && failedBatch.exported == 0 && strstr(failedBatch.status, "Last failure"),
		"batch names a failed resource instead of aborting");
	HdTextureOverrideBatchResult emptyBatch = {};
	Check(!HdTextureOverrides_ExportTextureBatch("batchtest", NULL, 0, &emptyBatch), "batch rejects an empty request");

	// Milestone 3: a re-export preserves the mapped file and merges references
	// into the existing entry instead of appending a duplicate registration.
	const char* mappedManifest = "{ \"schemaVersion\": 1, \"id\": \"mapped\", \"textures\": [ { \"texture\": \"MAP\", \"texturePage\": 4, \"textureIndex\": 4, \"file\": \"assets/custom/MAP_remaster.png\", \"modelReferences\": [\"model:0:0:1\"] } ] }";
	CreateDirectoryA("mods", NULL);
	CreateDirectoryA("mods/mapped", NULL);
	Check(InspectorExport_WriteText("mods/mapped/manifest.json", mappedManifest, status, sizeof(status)), "write a manifest with a mapped file and reference");
	HdTextureOverrideBatchItem mapped = {};
	mapped.tpage = 256; mapped.clut = 0; mapped.u = 0; mapped.v = 0; mapped.width = 4; mapped.height = 4;
	snprintf(mapped.textureName, sizeof(mapped.textureName), "MAP");
	mapped.texturePage = 4; mapped.textureIndex = 4;
	snprintf(mapped.filenameSuffix, sizeof(mapped.filenameSuffix), "NEWSUFFIX");
	snprintf(mapped.modelReferences[0], HD_TEXTURE_MODEL_REFERENCE_CAPACITY, "model:0:0:1");
	snprintf(mapped.modelReferences[1], HD_TEXTURE_MODEL_REFERENCE_CAPACITY, "model:0:0:2");
	mapped.modelReferenceCount = 2;
	HdTextureOverrideBatchResult mappedResult = {};
	Check(HdTextureOverrides_ExportTextureBatch("mapped", &mapped, 1, &mappedResult) && mappedResult.exported == 1,
		"re-export of a mapped texture succeeds");
	decoded = NULL;
	Check(LoadPngRgba("mods/mapped/assets/custom/MAP_remaster.png", &decoded, &width, &height) && width == 4,
		"re-export replaces the mapped file instead of a new name");
	free(decoded);
	int mappedBytes = 0;
	char* mappedText = ReadTextFile("mods/mapped/manifest.json", &mappedBytes);
	Check(mappedText && strstr(mappedText, "MAP_remaster.png") && !strstr(mappedText, "NEWSUFFIX") && !strstr(mappedText, "MAP_p4_i4"),
		"existing file mapping is preserved and no suffixed duplicate is registered");
	int mapEntries = 0;
	if (mappedText)
		for (char* scan = mappedText; (scan = strstr(scan, "\"MAP\"")) != NULL; ++scan) ++mapEntries;
	Check(mapEntries == 1, "re-export does not duplicate the texture entry");
	int refOne = 0, refTwo = 0;
	if (mappedText)
	{
		for (char* scan = mappedText; (scan = strstr(scan, "\"model:0:0:1\"")) != NULL; ++scan) ++refOne;
		for (char* scan = mappedText; (scan = strstr(scan, "\"model:0:0:2\"")) != NULL; ++scan) ++refTwo;
	}
	Check(refOne == 1 && refTwo == 1, "re-export merges new references and keeps existing ones once");
	free(mappedText);

	// An entry created before model references existed gains the field on
	// re-export while its mapped file is preserved.
	const char* legacyManifest = "{ \"schemaVersion\": 1, \"id\": \"mapped2\", \"textures\": [ { \"texture\": \"OLD\", \"texturePage\": 1, \"textureIndex\": 1, \"file\": \"assets/inspector/OLD_custom.png\" } ] }";
	CreateDirectoryA("mods", NULL);
	CreateDirectoryA("mods/mapped2", NULL);
	Check(InspectorExport_WriteText("mods/mapped2/manifest.json", legacyManifest, status, sizeof(status)), "write a manifest entry without model references");
	HdTextureOverrideBatchItem legacy = {};
	legacy.tpage = 256; legacy.clut = 0; legacy.u = 0; legacy.v = 0; legacy.width = 4; legacy.height = 4;
	snprintf(legacy.textureName, sizeof(legacy.textureName), "OLD");
	legacy.texturePage = 1; legacy.textureIndex = 1;
	snprintf(legacy.modelReferences[0], HD_TEXTURE_MODEL_REFERENCE_CAPACITY, "model:0:0:5");
	legacy.modelReferenceCount = 1;
	HdTextureOverrideBatchResult legacyResult = {};
	Check(HdTextureOverrides_ExportTextureBatch("mapped2", &legacy, 1, &legacyResult) && legacyResult.exported == 1,
		"re-export of a legacy entry succeeds");
	decoded = NULL;
	Check(LoadPngRgba("mods/mapped2/assets/inspector/OLD_custom.png", &decoded, &width, &height) && width == 4,
		"legacy entry keeps its mapped file on re-export");
	free(decoded);
	int legacyBytes = 0;
	char* legacyText = ReadTextFile("mods/mapped2/manifest.json", &legacyBytes);
	Check(legacyText && strstr(legacyText, "OLD_custom.png") && strstr(legacyText, "\"modelReferences\"") &&
		strstr(legacyText, "\"model:0:0:5\""), "legacy entry gains model references without losing its file");
	int legacyEntries = 0;
	if (legacyText)
		for (char* scan = legacyText; (scan = strstr(scan, "\"OLD\"")) != NULL; ++scan) ++legacyEntries;
	Check(legacyEntries == 1, "legacy entry is not duplicated");
	free(legacyText);

	// Milestone 4: the catalog material adapter resolves a hidden material's
	// VRAM region from the registered level texture set.
	HdTextureOverrides_BeginPage(7);
	HdTextureOverrides_RegisterTexture(7, 3, "HIDDEN", 256, 0, 1, 2, 4, 4);
	unsigned short knownTpage = 0, knownClut = 0, knownU = 0, knownV = 0, knownW = 0, knownH = 0;
	char knownName[48] = {};
	Check(HdTextureOverrides_GetKnownTextureRegion(7, 3, &knownTpage, &knownClut, &knownU, &knownV, &knownW, &knownH, knownName, sizeof(knownName)) &&
		knownTpage == 256 && knownClut == 0 && knownU == 1 && knownV == 2 && knownW == 4 && knownH == 4 && !strcmp(knownName, "HIDDEN"),
		"known-texture adapter resolves a hidden material region");
	Check(!HdTextureOverrides_GetKnownTextureRegion(7, 99, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0),
		"known-texture adapter reports a missing material");

	// Milestone 5: cooperative stepping, cancellation, per-resource outcomes and
	// retry. Each resource is published on its own; there is no all-or-nothing
	// transaction, so a cancel keeps everything already written.
	HdTextureOverrideBatchItem jobItems[2] = {};
	for (int i = 0; i < 2; ++i)
	{
		jobItems[i].tpage = 256; jobItems[i].clut = 0;
		jobItems[i].u = 0; jobItems[i].v = 0; jobItems[i].width = 4; jobItems[i].height = 4;
		snprintf(jobItems[i].textureName, sizeof(jobItems[i].textureName), "JOB%d", i + 1);
		jobItems[i].texturePage = 5; jobItems[i].textureIndex = i + 1;
	}
	HdTextureOverrideBatchJob job;
	Check(HdTextureOverrides_BeginBatchJob(&job, "jobtest", jobItems, 2), "batch job begins with a valid plan");
	Check(HdTextureOverrides_BatchJobActive(&job), "batch job is active before stepping");
	Check(HdTextureOverrides_StepBatchJob(&job, 1) == 1 && job.outcomes[0] == HD_TEXTURE_BATCH_OUTCOME_EXPORTED &&
		job.outcomes[1] == HD_TEXTURE_BATCH_OUTCOME_PENDING, "stepping exports one resource and leaves the next pending");
	Check(HdTextureOverrides_StepBatchJob(&job, 1) == 0 && !HdTextureOverrides_BatchJobActive(&job) && job.exported == 2,
		"stepping to the end completes the batch");
	Check(job.outcomes[1] == HD_TEXTURE_BATCH_OUTCOME_EXPORTED, "per-resource outcome records the second export");

	HdTextureOverrideBatchItem cancelItems[2] = {};
	for (int i = 0; i < 2; ++i)
	{
		cancelItems[i].tpage = 256; cancelItems[i].clut = 0;
		cancelItems[i].u = 0; cancelItems[i].v = 0; cancelItems[i].width = 4; cancelItems[i].height = 4;
		snprintf(cancelItems[i].textureName, sizeof(cancelItems[i].textureName), "CANCEL%d", i + 1);
		cancelItems[i].texturePage = 6; cancelItems[i].textureIndex = i + 1;
	}
	HdTextureOverrideBatchJob cancelJob;
	Check(HdTextureOverrides_BeginBatchJob(&cancelJob, "jobtest", cancelItems, 2), "cancel batch job begins");
	HdTextureOverrides_CancelBatchJob(&cancelJob);
	char jobStatus[320];
	HdTextureOverrides_GetBatchJobStatus(&cancelJob, jobStatus, sizeof(jobStatus));
	Check(!HdTextureOverrides_BatchJobActive(&cancelJob) && strstr(jobStatus, "Cancelled"), "cancelled batch stops and reports the partial state");
	Check(HdTextureOverrides_StepBatchJob(&cancelJob, 2) == 0 && cancelJob.exported == 0, "cancelled batch exports nothing further");

	HdTextureOverrideBatchItem duplicateJobItems[2] = {};
	for (int i = 0; i < 2; ++i)
	{
		duplicateJobItems[i].tpage = 256; duplicateJobItems[i].clut = 0;
		duplicateJobItems[i].u = 0; duplicateJobItems[i].v = 0; duplicateJobItems[i].width = 4; duplicateJobItems[i].height = 4;
		snprintf(duplicateJobItems[i].textureName, sizeof(duplicateJobItems[i].textureName), "DUP");
		duplicateJobItems[i].texturePage = 8; duplicateJobItems[i].textureIndex = 1;
	}
	HdTextureOverrideBatchJob duplicateJob;
	Check(HdTextureOverrides_BeginBatchJob(&duplicateJob, "jobtest", duplicateJobItems, 2), "duplicate batch job begins");
	Check(duplicateJob.outcomes[0] == HD_TEXTURE_BATCH_OUTCOME_PENDING &&
		duplicateJob.outcomes[1] == HD_TEXTURE_BATCH_OUTCOME_DUPLICATE &&
		duplicateJob.uniqueCount == 1 && duplicateJob.duplicates == 1, "duplicate identity is planned out, not queued");

	HdTextureOverrideBatchItem retryItems[1] = {};
	retryItems[0].tpage = 256; retryItems[0].clut = 0;
	retryItems[0].u = 0; retryItems[0].v = 255; retryItems[0].width = 4; retryItems[0].height = 4;
	snprintf(retryItems[0].textureName, sizeof(retryItems[0].textureName), "RETRY");
	retryItems[0].texturePage = 9; retryItems[0].textureIndex = 1;
	HdTextureOverrideBatchJob retryJob;
	Check(HdTextureOverrides_BeginBatchJob(&retryJob, "jobtest", retryItems, 1), "retry batch job begins");
	HdTextureOverrides_StepBatchJob(&retryJob, 1);
	Check(retryJob.outcomes[0] == HD_TEXTURE_BATCH_OUTCOME_FAILED && retryJob.failed == 1, "invalid region is recorded as a failed resource");
	HdTextureOverrides_RetryFailedBatchJob(&retryJob);
	Check(HdTextureOverrides_BatchJobActive(&retryJob) && retryJob.outcomes[0] == HD_TEXTURE_BATCH_OUTCOME_PENDING && retryJob.failed == 0,
		"retry re-queues only the failed resource");

	char token[48];
	Check(HdTextureOverrides_MakeSafeNameToken("POLICE CAR/01", token, sizeof(token)) && !strcmp(token, "POLICE_CAR_01"),
		"safe name token sanitizes a model label");
	Check(!HdTextureOverrides_MakeSafeNameToken("  //  ", token, sizeof(token)) && token[0] == '\0',
		"safe name token rejects a label with no alphanumeric characters");

	printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
