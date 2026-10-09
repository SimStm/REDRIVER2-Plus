// Standalone regression tests for the E02 sound asset record.
//
// The record is pure data (no PsyCross, GPU or game state), so its defaults,
// validation, loop handling, music metadata and canonical serialization can be
// checked in isolation. Build and run instructions are in tests/README.md.

#include "../Game/C/soundasset.h"

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

static SoundAsset MakeValidSound()
{
	SoundAsset asset;
	SoundAsset_Init(&asset);
	SoundAsset_SetId(&asset, "sfx:bank3:voice7");
	asset.bankId = 3;
	asset.voiceId = 7;
	asset.frameCount = 1000;
	return asset;
}

static void TestDefaults()
{
	SoundAsset asset;
	SoundAsset_Init(&asset);
	CHECK(asset.sampleRate == 44100);
	CHECK(asset.channels == 1);
	CHECK(asset.bitDepth == 16);
	CHECK(asset.loop == 0 && asset.loopEnd == -1);
	CHECK(asset.bankId == -1 && asset.voiceId == -1);
	CHECK(asset.musicBank == -1 && asset.musicProgram == -1 && asset.musicSequence == -1);
}

static void TestIdAndValidation()
{
	char error[SOUND_ASSET_ERROR_CAPACITY];

	SoundAsset asset = MakeValidSound();
	CHECK(SoundAsset_Validate(&asset, error, sizeof(error)) == 1);

	// An empty or too-long id is rejected by the setter.
	CHECK(SoundAsset_SetId(&asset, "") == 0);
	char longId[SOUND_ASSET_ID_CAPACITY + 4];
	memset(longId, 'a', sizeof(longId) - 1);
	longId[sizeof(longId) - 1] = '\0';
	CHECK(SoundAsset_SetId(&asset, longId) == 0);

	// Field bounds.
	SoundAsset bad = MakeValidSound();
	bad.sampleRate = 0;
	CHECK(SoundAsset_Validate(&bad, error, sizeof(error)) == 0);
	bad = MakeValidSound();
	bad.channels = 3;
	CHECK(SoundAsset_Validate(&bad, error, sizeof(error)) == 0);
	bad = MakeValidSound();
	bad.bitDepth = 12;
	CHECK(SoundAsset_Validate(&bad, error, sizeof(error)) == 0);
	bad = MakeValidSound();
	bad.frameCount = 0;
	CHECK(SoundAsset_Validate(&bad, error, sizeof(error)) == 0);
	bad = MakeValidSound();
	bad.pitchReferenceHz = 0;
	CHECK(SoundAsset_Validate(&bad, error, sizeof(error)) == 0);
	CHECK(error[0] != '\0');

	bad = MakeValidSound();
	bad.id[0] = '\0';
	CHECK(SoundAsset_Validate(&bad, error, sizeof(error)) == 0);
}

static void TestLoop()
{
	char error[SOUND_ASSET_ERROR_CAPACITY];
	SoundAsset asset = MakeValidSound();

	SoundAsset_SetLoop(&asset, 100, 500);
	CHECK(asset.loop == 1 && asset.loopStart == 100 && asset.loopEnd == 500);
	CHECK(SoundAsset_Validate(&asset, error, sizeof(error)) == 1);

	// A reversed range clears the loop flag.
	SoundAsset_SetLoop(&asset, 500, 100);
	CHECK(asset.loop == 0);

	// A loop that runs past the sound is invalid.
	SoundAsset_SetLoop(&asset, 100, 500);
	asset.loopEnd = 2000;
	CHECK(SoundAsset_Validate(&asset, error, sizeof(error)) == 0);
}

static void TestMusicMetadata()
{
	char error[SOUND_ASSET_ERROR_CAPACITY];
	SoundAsset asset = MakeValidSound();

	// A bank without a program/sequence is incomplete.
	asset.musicBank = 1;
	CHECK(SoundAsset_Validate(&asset, error, sizeof(error)) == 0);

	asset.musicProgram = 2;
	asset.musicSequence = 3;
	CHECK(SoundAsset_Validate(&asset, error, sizeof(error)) == 1);
}

static void TestSerialization()
{
	SoundAsset asset = MakeValidSound();
	SoundAsset_SetLoop(&asset, 100, 500);
	asset.musicBank = 1;
	asset.musicProgram = 2;
	asset.musicSequence = 3;

	char text[SOUND_ASSET_SERIALIZED_CAPACITY];
	const int length = SoundAsset_Serialize(&asset, text, sizeof(text));
	CHECK(length > 0);
	CHECK(strstr(text, "\"id\":\"sfx:bank3:voice7\"") != NULL);
	CHECK(strstr(text, "\"sampleRate\":44100") != NULL);
	CHECK(strstr(text, "\"frames\":1000") != NULL);
	CHECK(strstr(text, "\"loop\":1") != NULL);
	CHECK(strstr(text, "\"loopStart\":100") != NULL);
	CHECK(strstr(text, "\"music\":{\"bank\":1,\"program\":2,\"sequence\":3}") != NULL);

	// Deterministic.
	char again[SOUND_ASSET_SERIALIZED_CAPACITY];
	CHECK(SoundAsset_Serialize(&asset, again, sizeof(again)) == length);
	CHECK(strcmp(text, again) == 0);

	// A too-small buffer fails rather than truncating.
	CHECK(SoundAsset_Serialize(&asset, text, 8) < 0);
}

int main()
{
	TestDefaults();
	TestIdAndValidation();
	TestLoop();
	TestMusicMetadata();
	TestSerialization();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
