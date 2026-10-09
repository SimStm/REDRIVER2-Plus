#include "soundasset.h"

#include <stdio.h>
#include <string.h>

// E02 sound asset record. Pure data: no PsyCross, GPU or game state.

static void CopyField(char* destination, int capacity, const char* source)
{
	int i = 0;
	if (capacity <= 0)
		return;
	if (!source)
		source = "";
	for (; source[i] && i < capacity - 1; ++i)
		destination[i] = source[i];
	destination[i] = '\0';
}

void SoundAsset_Init(SoundAsset* asset)
{
	if (!asset)
		return;
	memset(asset, 0, sizeof(*asset));
	asset->bankId = -1;
	asset->voiceId = -1;
	asset->sampleRate = 44100;
	asset->channels = 1;
	asset->bitDepth = 16;
	asset->loopEnd = -1;
	asset->pitchReferenceHz = 44100;
	asset->musicBank = -1;
	asset->musicProgram = -1;
	asset->musicSequence = -1;
}

int SoundAsset_SetId(SoundAsset* asset, const char* id)
{
	if (!asset || !id || id[0] == '\0' || strlen(id) >= sizeof(asset->id))
		return 0;
	CopyField(asset->id, sizeof(asset->id), id);
	return 1;
}

void SoundAsset_SetLoop(SoundAsset* asset, int startFrame, int endFrame)
{
	if (!asset)
		return;
	asset->loopStart = startFrame;
	asset->loopEnd = endFrame;
	asset->loop = endFrame >= startFrame && endFrame >= 0 ? 1 : 0;
}

int SoundAsset_Validate(const SoundAsset* asset, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!asset)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "no sound record");
		return 0;
	}
	if (asset->id[0] == '\0')
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the sound has no id");
		return 0;
	}
	if (asset->sampleRate <= 0)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the sample rate must be positive");
		return 0;
	}
	if (asset->channels != 1 && asset->channels != 2)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the channel count must be 1 or 2");
		return 0;
	}
	if (asset->bitDepth != 8 && asset->bitDepth != 16)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the bit depth must be 8 or 16");
		return 0;
	}
	if (asset->frameCount <= 0)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the frame count must be positive");
		return 0;
	}
	if (asset->pitchReferenceHz <= 0)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the pitch reference must be positive");
		return 0;
	}
	if (asset->loop &&
		(asset->loopStart < 0 || asset->loopEnd < asset->loopStart || asset->loopEnd >= asset->frameCount))
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the loop range is outside the sound");
		return 0;
	}
	if (asset->musicBank >= 0 && (asset->musicProgram < 0 || asset->musicSequence < 0))
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the music metadata is incomplete");
		return 0;
	}
	return 1;
}

int SoundAsset_Serialize(const SoundAsset* asset, char* out, int capacity)
{
	if (!asset || !out || capacity <= 0)
		return -1;

	const int written = snprintf(out, capacity,
		"{\"id\":\"%s\",\"bank\":%d,\"voice\":%d,\"sampleRate\":%d,\"channels\":%d,"
		"\"bitDepth\":%d,\"frames\":%d,\"loop\":%d,\"loopStart\":%d,\"loopEnd\":%d,"
		"\"pitchReferenceHz\":%d,\"music\":{\"bank\":%d,\"program\":%d,\"sequence\":%d}}",
		asset->id, asset->bankId, asset->voiceId, asset->sampleRate, asset->channels,
		asset->bitDepth, asset->frameCount, asset->loop, asset->loopStart, asset->loopEnd,
		asset->pitchReferenceHz, asset->musicBank, asset->musicProgram, asset->musicSequence);
	if (written <= 0 || written >= capacity)
		return -1;
	return written;
}
