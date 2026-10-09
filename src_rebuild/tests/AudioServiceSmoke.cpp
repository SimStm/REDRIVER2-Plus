// Controlled service-state smoke, not captured-output audio acceptance.
//
// Two phases share this translation unit:
//  - RED2_DECODE_ONLY: pure PSX SPU ADPCM decode checks, compiled and run
//    without any FMOD header or library.
//  - default (Windows): the decode checks plus the FMOD Core service round-trip.
#include "../Game/C/sounddecode.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int checks, failures;
static void Check(bool ok, const char* text)
{
 ++checks; printf("%s: %s\n", ok ? "PASS" : "FAIL", text); if (!ok) ++failures;
}

// Pure decoder fixtures. No service, no SDK.
static void RunDecodeChecks(void)
{
 char error[SOUND_DECODE_ERROR_CAPACITY] = {};
 unsigned char block[16];
 unsigned char loopVag[48];
 unsigned char loop2[32];
 SoundDecodeSample sample = { 0, 16, 0, 44100 };
 SoundDecodeInfo info = {};
 short out[84];
 int i;

 // 16 bytes -> 28 frames. shift=0/filter=0 makes each nibble a signed 12-bit value.
 memset(block, 0, sizeof(block));
 block[1] = 0x01;			// LoopEnd marker only; the record does not loop
 block[2] = 0xF7;			// frame 0 = 0x7, frame 1 = 0xF
 block[3] = 0x81;			// frame 2 = 0x1, frame 3 = 0x8
 Check(SoundDecode_SpuAdpcm(block, 16, &sample, out, 28, &info, error, sizeof(error)) == SOUND_DECODE_OK,
  "16-byte block decodes to 28 frames");
 Check(info.frameCount == 28 && info.sampleRate == 44100 && !info.loop && info.loopEnd == -1,
  "metadata reports 28 frames, the sample rate and no loop");
 Check(out[0] == 28672 && out[1] == -4096 && out[2] == 4096 && out[3] == -32768,
  "known nibbles decode to sign-extended 12-bit PCM");
 bool silent = true;
 for (i = 4; i < 28; ++i) silent = silent && out[i] == 0;
 Check(silent, "zero nibbles decode to silence");

 // Filters 1..4 with non-zero shifts across four blocks. The predictor carries
 // from block to block, so these values also pin the cross-block state. The
 // expected values were derived from the same coefficient recursion documented
 // in sounddecode.h using an independent double-precision implementation; they
 // match the decoder exactly because every term is an exact binary fraction.
 unsigned char filterVag[64];
 int filterParams[4][2] = { { 1, 11 }, { 2, 10 }, { 3, 9 }, { 4, 8 } };
 for (i = 0; i < 4; ++i)
 {
  unsigned char* filterBlock = filterVag + i * 16;
  filterBlock[0] = (unsigned char)((filterParams[i][0] << 4) | filterParams[i][1]);
  filterBlock[1] = 0x01;
  memset(filterBlock + 2, 0x11, 14);
 }
 short filtered[112];
 SoundDecodeSample filterSample = { 0, 64, 0, 44100 };
 SoundDecodeInfo filterInfo = {};
 Check(SoundDecode_SpuAdpcm(filterVag, 64, &filterSample, filtered, 112, &filterInfo, error, sizeof(error)) == SOUND_DECODE_OK &&
  filterInfo.frameCount == 112,
  "four blocks with filters 1..4 and non-zero shifts decode");
 Check(filtered[0] == 2 && filtered[1] == 4 && filtered[27] == 27 &&
  filtered[28] == 31 && filtered[29] == 37 && filtered[55] == 253 &&
  filtered[56] == 180 && filtered[57] == 66 && filtered[83] == 15 &&
  filtered[84] == 45 && filtered[85] == 87 && filtered[111] == 449,
  "filter/shift/cross-block predictor PCM matches the exact recursion");

 // Loop flags: block 1 marks LoopStart, block 2 marks LoopEnd|Repeat.
 memset(loopVag, 0, sizeof(loopVag));
 loopVag[17] = 0x04;
 loopVag[33] = 0x03;
 SoundDecodeSample loopSample = { 0, 48, 1, 22050 };
 SoundDecodeInfo loopInfo = {};
 Check(SoundDecode_SpuAdpcm(loopVag, 48, &loopSample, out, 84, &loopInfo, error, sizeof(error)) == SOUND_DECODE_OK,
  "looping 3-block sample decodes");
 Check(loopInfo.frameCount == 84 && loopInfo.loop && loopInfo.loopStart == 28 && loopInfo.loopEnd == 83,
  "loop flags map to inclusive, block-aligned frames");
 Check(loopInfo.loopStart != 56,
  "loop start uses the block-start convention, not the reference FIXME (block+1)*28");

 // A whole-sample loop has loopStart == 0.
 memset(loop2, 0, sizeof(loop2));
 loop2[1] = 0x04;
 loop2[17] = 0x03;
 SoundDecodeSample loop2Sample = { 0, 32, 1, 8000 };
 SoundDecodeInfo loop2Info = {};
 Check(SoundDecode_SpuAdpcm(loop2, 32, &loop2Sample, out, 56, &loop2Info, error, sizeof(error)) == SOUND_DECODE_OK &&
  loop2Info.loopStart == 0 && loop2Info.loopEnd == 55,
  "whole-sample loop maps to frames 0..55");

 // Non-looping samples may carry several end markers; the record governs.
 unsigned char multiEnd[32];
 memset(multiEnd, 0, sizeof(multiEnd));
 multiEnd[1] = 0x01; multiEnd[17] = 0x01;
 SoundDecodeSample multiSample = { 0, 32, 0, 44100 };
 SoundDecodeInfo multiInfo = {};
 Check(SoundDecode_SpuAdpcm(multiEnd, 32, &multiSample, out, 56, &multiInfo, error, sizeof(error)) == SOUND_DECODE_OK &&
  !multiInfo.loop && multiInfo.loopEnd == -1,
  "non-looping sample with several end markers decodes without loop metadata");

 // Determinism: same input, identical PCM and metadata.
 short first[28], second[28];
 SoundDecodeInfo firstInfo = {}, secondInfo = {};
 SoundDecodeSample detSample = { 0, 16, 0, 44100 };
 memset(block, 0, sizeof(block));
 block[2] = 0x31; block[5] = 0xC5; block[9] = 0x6A; block[14] = 0x27;
 Check(SoundDecode_SpuAdpcm(block, 16, &detSample, first, 28, &firstInfo, error, sizeof(error)) == SOUND_DECODE_OK &&
  SoundDecode_SpuAdpcm(block, 16, &detSample, second, 28, &secondInfo, error, sizeof(error)) == SOUND_DECODE_OK &&
  memcmp(first, second, sizeof(first)) == 0 && memcmp(&firstInfo, &secondInfo, sizeof(firstInfo)) == 0,
  "decoding is deterministic");

 // Bounds errors must leave the caller's buffer untouched (zero partial output).
 SoundDecodeSample bad = { 0, 8, 0, 44100 };
 memset(out, 0x7F, sizeof(out));
 Check(SoundDecode_SpuAdpcm(block, 8, &bad, out, 28, &info, error, sizeof(error)) == SOUND_DECODE_ERROR_SHORT &&
  out[0] == (short)0x7F7F && error[0] != '\0',
  "short block rejected with zero partial output");
 bad.length = 17;
 Check(SoundDecode_SpuAdpcm(block, 17, &bad, out, 28, &info, error, sizeof(error)) == SOUND_DECODE_ERROR_TRUNCATED,
  "non-multiple-of-16 length rejected as truncated");
 bad.length = 16; bad.address = 8;
 Check(SoundDecode_SpuAdpcm(block, 16, &bad, out, 28, &info, error, sizeof(error)) == SOUND_DECODE_ERROR_RANGE,
  "address/length outside the blob rejected");
 bad.address = 0; bad.length = 32;
 Check(SoundDecode_SpuAdpcm(loop2, 32, &bad, out, 28, &info, error, sizeof(error)) == SOUND_DECODE_ERROR_OVERSIZED,
  "decoded frames beyond the output buffer rejected");
 unsigned char badFilter[16];
 memcpy(badFilter, block, sizeof(badFilter));
 badFilter[0] = 0x50;		// filter nibble 5
 bad.length = 16;
 Check(SoundDecode_SpuAdpcm(badFilter, 16, &bad, out, 28, &info, error, sizeof(error)) == SOUND_DECODE_ERROR_BLOCK_FORMAT,
  "invalid ADPCM filter nibble rejected");

 // Unsupported loop shapes.
 unsigned char noFlags[48];
 memset(noFlags, 0, sizeof(noFlags));
 SoundDecodeSample loopReq = { 0, 48, 1, 44100 };
 Check(SoundDecode_SpuAdpcm(noFlags, 48, &loopReq, out, 84, &info, error, sizeof(error)) == SOUND_DECODE_ERROR_LOOP_SHAPE,
  "looping record without loop flags rejected");
 noFlags[1] = 0x04; noFlags[17] = 0x01;		// LoopEnd without Repeat
 SoundDecodeSample twoBlockLoop = { 0, 48, 1, 44100 };
 Check(SoundDecode_SpuAdpcm(noFlags, 48, &twoBlockLoop, out, 84, &info, error, sizeof(error)) == SOUND_DECODE_ERROR_LOOP_SHAPE,
  "loop end without repeat rejected");
 memset(noFlags, 0, sizeof(noFlags));
 noFlags[1] = 0x03; noFlags[33] = 0x04;		// LoopEnd before LoopStart
 Check(SoundDecode_SpuAdpcm(noFlags, 48, &twoBlockLoop, out, 84, &info, error, sizeof(error)) == SOUND_DECODE_ERROR_LOOP_SHAPE,
  "loop end before loop start rejected");
}

#ifndef RED2_DECODE_ONLY
#include "../utils/AudioService.h"
#ifdef _WIN32
#include <windows.h>

static AudioPlayRequest Request(unsigned int id)
{
 AudioPlayRequest r; AudioContract_InitPlayRequest(&r); r.eventId = id; r.kind = AUDIO_KIND_SFX; return r;
}
#endif
#endif

int main(int argc, char** argv)
{
 RunDecodeChecks();

#ifdef RED2_DECODE_ONLY
 printf("%d checks, %d failures\n", checks, failures); return failures ? 1 : 0;
#else
#ifndef _WIN32
 puts("Windows x64 audio smoke required"); return 2;
#else
 char error[512] = {}, version[32] = {};
 Check(AudioService_FormatVersion(0x00020315, version, sizeof(version)) && strcmp(version, "2.03.15") == 0,
  "BCD fixture 0x00020315 renders 2.03.15, not 2.03.21");
 Check(AudioService_FormatVersion(0x00020321, version, sizeof(version)) && strcmp(version, "2.03.21") == 0,
  "BCD fixture 0x00020321 renders 2.03.21");
 Check(!AudioService_FormatVersion(0x0002031A, version, sizeof(version)), "invalid BCD digit rejected");
 if (!AudioService_Init(error, sizeof(error)))
 {
  printf("FAIL: audio initialization: %s\nReal output device required; initialization failed (distinct exit 2).\n", error); return 2;
 }
 Check(AudioService_IsActive(), "real device active");
 Check(argc == 2 && strcmp(AudioService_Version(), argv[1]) == 0, "service version agrees with independently checked header/DLL metadata");
 const int frames = 11025;
 short pcm[frames], stereo[frames * 2];
 for (int i = 0; i < frames; ++i)
 {
  pcm[i] = (short)(sin(i * 6.283185307179586 * 440 / 22050) * 8000);
  stereo[i * 2] = pcm[i]; stereo[i * 2 + 1] = (short)(pcm[i] / 3);
 }
 const unsigned int id = AudioContract_MakeEventId(AUDIO_KIND_SFX, 1, 1);
 const unsigned int loopId = id + 1, stereoId = id + 2, streamId = id + 3;
 Check(AudioService_RegisterSoundMemory(id, pcm, sizeof(pcm), 1, 22050, frames, -1, -1, 0, error, sizeof(error)), "mono PCM registered");
 AudioPlayRequest r = Request(id); r.flags = AUDIO_PLAY_PAUSED;
 AudioVoiceHandle a = {}, b = {};
  Check(AudioService_Play(&r, &a, error, sizeof(error)), "2D voice created paused after configuration");
 // The first allocation must be retained across restart: a reset-to-zero bug
 // resurrects generation 1, whereas a handle captured after many cycles can
 // accidentally remain stale even in that broken implementation.
 Check(a.generation == 1, "restart regression starts with first allocation generation 1");
 const AudioVoiceHandle old = a;
 AudioService_Shutdown();
 Check(AudioService_Init(error, sizeof(error)), "first-voice service restart initialized");
 Check(AudioService_RegisterSoundMemory(id, pcm, sizeof(pcm), 1, 22050, frames, -1, -1, 0, error, sizeof(error)) &&
  AudioService_Play(&r, &a, error, sizeof(error)), "first post-restart voice acquired");
 Check(a.index == old.index && a.generation != old.generation, "restart reuses same slot without resurrecting generation 1");
 float restartPosition[3] = { 20, 0, 0 }, restartVelocity[3] = {};
 Check(!AudioService_Pause(old, 0), "pre-restart handle cannot pause/resume new voice");
 Check(!AudioService_SetVoicePitch(old, 2.0f), "pre-restart handle cannot change pitch");
 Check(!AudioService_SetVoiceVolume(old, 0.1f), "pre-restart handle cannot change volume");
 Check(!AudioService_SetVoicePosition(old, restartPosition, restartVelocity), "pre-restart handle cannot change position/mode");
 Check(!AudioService_IsVoicePlaying(old), "pre-restart handle cannot observe new voice as its own");
 Check(!AudioService_Stop(old), "pre-restart handle cannot stop new voice");
 Check(AudioService_IsVoicePlaying(a) && AudioService_VoiceCount() == 1, "new voice survives every stale control operation");
 int spatial = -1, paused = -1; float pos[3];
 Check(AudioService_QueryVoice(a, &spatial, &paused, pos) && !spatial && paused, "actual FMOD mode is 2D and final state paused");
 Check(AudioService_IsVoicePlaying(a), "paused voice remains live");
 Check(AudioService_SetVoiceVolume(a, 0.2f) && AudioService_SetVoicePitch(a, 1.25f), "volume and pitch controls");
 Check(AudioService_Pause(a, 0) && AudioService_QueryVoice(a, &spatial, &paused, pos) && !paused, "actual pause state resumes");
 Check(AudioService_Stop(a) && AudioService_VoiceCount() == 0, "stop releases slot");
 Check(!AudioService_Stop(a), "stale stop rejected");

 Check(AudioService_RegisterSoundMemory(loopId, pcm, sizeof(pcm), 1, 22050, frames, 100, frames - 1, 0, error, sizeof(error)), "inclusive end-of-sample loop accepted");
 unsigned int length = 0, start = 0, end = 0;
 Check(AudioService_QuerySound(loopId, &length, &start, &end) && length == frames && start == 100 && end == frames - 1,
  "actual Sound loop points exactly 100..frames-1, no increment");
 Check(!AudioService_RegisterSoundMemory(loopId, pcm, sizeof(pcm), 1, 22050, frames, 100, 100, 0, error, sizeof(error)) && strstr(error, "equal loop endpoints"), "equal loop rejected without lengthening");
 short one = 0;
 Check(!AudioService_RegisterSoundMemory(id + 10, &one, 2, 1, 22050, 1, 0, 0, 0, error, sizeof(error)) && strstr(error, "equal loop endpoints"), "one-frame sample degenerate loop rejected");
 Check(AudioService_QuerySound(loopId, &length, &start, &end) && end == frames - 1, "failed replacement preserves original SDK loop bounds");
 r = Request(loopId); r.flags = AUDIO_PLAY_LOOP | AUDIO_PLAY_PAUSED | AUDIO_PLAY_3D;
 r.minDistance = 10; r.maxDistance = 1000; r.position[0] = 100;
 Check(AudioService_Play(&r, &a, error, sizeof(error)), "first emitter created");
 r.position[0] = -200;
 Check(AudioService_Play(&r, &b, error, sizeof(error)) && (a.index != b.index || a.generation != b.generation), "same sample event creates distinct emitter handles");
 Check(AudioService_QueryVoice(a, &spatial, &paused, pos) && spatial && pos[0] == 100, "first emitter actual 3D position retained");
 Check(AudioService_QueryVoice(b, &spatial, &paused, pos) && spatial && pos[0] == -200, "second emitter actual independent 3D position");
 float moved[3] = { -300, 0, 0 }, velocity[3] = {};
 Check(AudioService_SetVoicePosition(b, moved, velocity) && AudioService_QueryVoice(b, &spatial, &paused, pos) && pos[0] == -300, "second emitter independently updated");
 Check(AudioService_Stop(a) && AudioService_IsVoicePlaying(b), "stopping first emitter leaves second live");
 r.flags |= AUDIO_PLAY_RESTART;
 Check(!AudioService_Play(&r, nullptr, error, sizeof(error)) && strstr(error, "explicit voice identity") && AudioService_IsVoicePlaying(b), "unscoped RESTART rejected without stopping another emitter");
 AudioService_Stop(b);
 Check(AudioService_RegisterSoundMemory(stereoId, stereo, sizeof(stereo), 2, 22050, frames, -1, -1, 0, error, sizeof(error)) &&
  AudioService_QuerySound(stereoId, &length, &start, &end) && length == frames, "actual stereo Sound PCM length equals frames, not interleaved sample count");
 r = Request(stereoId); r.flags = AUDIO_PLAY_PAUSED;
 Check(AudioService_Play(&r, &a, error, sizeof(error)) && AudioService_QueryVoice(a, &spatial, &paused, pos) && !spatial, "stereo voice uses actual 2D mode");
 Check(AudioService_SetVoicePosition(a, moved, velocity) && AudioService_QueryVoice(a, &spatial, &paused, pos) && spatial && pos[0] == -300, "position setter explicitly transitions actual mode to 3D");
 AudioService_Stop(a);

 AudioListener listener = {}; listener.forward[2] = 1; listener.up[1] = 1; listener.up[2] = 0.5f;
 Check(AudioService_SetListener(&listener, error, sizeof(error)), "skewed independent listener accepted and orthogonalized");
 AudioListener actual = {};
 Check(AudioService_QueryListener(&actual) && fabs(actual.up[2]) < 1e-6 && fabs(actual.up[1] - 1) < 1e-6 && actual.forward[2] == 1,
  "actual SDK listener basis is unit perpendicular");
 listener.up[1] = 0; listener.up[2] = 2;
 Check(!AudioService_SetListener(&listener, error, sizeof(error)) && strstr(error, "parallel"), "parallel basis rejected with diagnostic");
 Check(AudioService_QueryListener(&actual) && actual.up[1] == 1, "rejected pose leaves actual SDK listener unchanged");
 listener.forward[2] = 1e-30f; listener.up[2] = 0; listener.up[1] = 1e-30f;
 Check(AudioService_SetListener(&listener, error, sizeof(error)) && AudioService_QueryListener(&actual) && actual.forward[2] == 1 && actual.up[1] == 1,
  "tiny basis normalized without float underflow");

 Check(AudioService_RegisterSoundMemory(streamId, pcm, sizeof(pcm), 1, 22050, frames, -1, -1, 1, error, sizeof(error)), "memory stream registered with owned PCM storage");
 r = Request(streamId); r.flags = AUDIO_PLAY_PAUSED;
 Check(AudioService_Play(&r, &a, error, sizeof(error)), "stream cursor created");
 Check(!AudioService_Play(&r, &b, error, sizeof(error)) && strstr(error, "live cursor") && AudioService_IsVoicePlaying(a), "concurrent stream cursor rejected without affecting first");
 AudioService_Stop(a);
 Check(!AudioService_RegisterSoundMemory(streamId, pcm, sizeof(pcm), 1, 22050, frames, 0, frames - 1, 1, error, sizeof(error)) && strstr(error, "prebuffer"), "stream loops explicitly deferred");

 r = Request(id); r.volume = (float)NAN;
 Check(!AudioService_Play(&r, &a, error, sizeof(error)) && strstr(error, "volume"), "NaN volume contract diagnostic");
 r = Request(0);
 Check(!AudioService_Play(&r, &a, error, sizeof(error)) && strstr(error, "event id"), "zero event contract diagnostic");
 r = Request(id);
 Check(AudioService_Play(&r, &a, error, sizeof(error)), "one-shot starts for natural completion");
 for (int i = 0; i < 100 && AudioService_VoiceCount(); ++i) { Sleep(20); AudioService_Update(); }
 Check(!AudioService_IsVoicePlaying(a) && !AudioService_VoiceCount(), "natural end reclaims slot");
 r.flags = AUDIO_PLAY_PAUSED;
 Check(AudioService_Play(&r, &b, error, sizeof(error)) && !AudioService_Stop(a) && AudioService_IsVoicePlaying(b), "natural-end stale handle cannot stop reused slot");
 Check(AudioService_UnregisterSound(id) && !AudioService_IsVoicePlaying(b) && !AudioService_VoiceCount(), "unregister stops owned voices");
 Check(!AudioService_UnregisterSound(id), "unknown unregister fails");

 // E04 menu-bank seam: decode a synthetic looping VAG, register its PCM with
 // the real service, and verify metadata/playback. The decoded buffer is freed
 // immediately after registration (non-streaming sample memory is copied).
 {
  unsigned char menuVag[32];
  memset(menuVag, 0, sizeof(menuVag));
  menuVag[1] = 0x04;		// block 0: LoopStart
  menuVag[17] = 0x03;		// block 1: LoopEnd | Repeat
  SoundDecodeSample menuSample = { 0, 32, 1, 22050 };
  SoundDecodeInfo menuInfo = {};
  short* decoded = (short*)malloc(56 * sizeof(short));
  const unsigned int menuId = AudioContract_MakeEventId(AUDIO_KIND_SFX, 41, 0);
  const unsigned int menuId2 = AudioContract_MakeEventId(AUDIO_KIND_SFX, 41, 1);
  Check(decoded && SoundDecode_SpuAdpcm(menuVag, 32, &menuSample, decoded, 56, &menuInfo, error, sizeof(error)) == SOUND_DECODE_OK,
   "menu-style VAG decodes before service registration");
  Check(AudioService_RegisterSoundMemory(menuId, decoded, menuInfo.frameCount * 2, 1, menuInfo.sampleRate,
   menuInfo.frameCount, menuInfo.loopStart, menuInfo.loopEnd, 0, error, sizeof(error)),
   "decoded PCM registers as a non-streaming sound");
  free(decoded);
  Check(AudioService_QuerySound(menuId, &length, &start, &end) && length == 56 && start == 0 && end == 55,
   "decoded sound metadata query succeeds after the source buffer is freed");
  r = Request(menuId); r.flags = AUDIO_PLAY_LOOP | AUDIO_PLAY_PAUSED;
  Check(AudioService_Play(&r, &a, error, sizeof(error)) && AudioService_IsVoicePlaying(a),
   "decoded sound plays after the source buffer is freed");
  Check(AudioService_Stop(a) && !AudioService_VoiceCount(), "decoded voice stops cleanly");
  Check(AudioService_RegisterSoundMemory(menuId, pcm, sizeof(pcm), 1, 22050, frames, -1, -1, 0, error, sizeof(error)) &&
   AudioService_QuerySound(menuId, &length, &start, &end) && length == frames,
   "re-registration replaces the decoded sound metadata");
  Check(AudioService_RegisterSoundMemory(menuId2, pcm, sizeof(pcm), 1, 22050, frames, -1, -1, 0, error, sizeof(error)),
   "sibling decoded-id sound registers");
  Check(AudioService_UnregisterSound(menuId) && AudioService_QuerySound(menuId2, &length, &start, &end),
   "unregister removes only the targeted decoded sound");
  Check(!AudioService_QuerySound(menuId, &length, &start, &end) && !AudioService_UnregisterSound(menuId),
   "unregistered decoded sound is no longer queryable or unregisterable");
  Check(AudioService_UnregisterSound(menuId2), "sibling decoded-id sound unregisters");
 }
 AudioService_Shutdown(); AudioService_Shutdown();
 Check(!AudioService_IsActive() && !AudioService_VoiceCount(), "shutdown and double shutdown release ownership");
 Check(!AudioService_Play(&r, &a, error, sizeof(error)) && strstr(error, "not active"), "inactive play diagnosed");
 AudioService_Shutdown();
 printf("%d checks, %d failures\n", checks, failures); return failures ? 1 : 0;
#endif
#endif
}
