// Controlled service-state smoke, not captured-output audio acceptance.
#include "../utils/AudioService.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>

static int checks, failures;
static void Check(bool ok, const char* text)
{
 ++checks; printf("%s: %s\n", ok ? "PASS" : "FAIL", text); if (!ok) ++failures;
}
static AudioPlayRequest Request(unsigned int id)
{
 AudioPlayRequest r; AudioContract_InitPlayRequest(&r); r.eventId = id; r.kind = AUDIO_KIND_SFX; return r;
}
int main(int argc, char** argv)
{
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
 AudioService_Shutdown(); AudioService_Shutdown();
 Check(!AudioService_IsActive() && !AudioService_VoiceCount(), "shutdown and double shutdown release ownership");
 Check(!AudioService_Play(&r, &a, error, sizeof(error)) && strstr(error, "not active"), "inactive play diagnosed");
 AudioService_Shutdown();
 printf("%d checks, %d failures\n", checks, failures); return failures ? 1 : 0;
}
#else
int main() { puts("Windows x64 audio smoke required"); return 2; }
#endif
