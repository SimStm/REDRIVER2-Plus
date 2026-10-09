#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include "../Game/C/audiocontract.h"

/* Windows x64 + FMOD SDK only. All calls are confined to the game thread;
   Update is caller-driven once per rendered frame. No FMOD callbacks.
   FMOD and the still-active PsyCross OpenAL device coexist during migration;
   this does not authorize consumer migration or a fallback on initialization
   failure. Handles survive only until release; generations persist across
   shutdown/init (subject to the contract's finite-generation wrap limit).
   Coordinates and attenuation remain raw game units. Doppler is disabled
   until an authoritative raw-unit/metre mapping is adopted, not guessed.
   Samples allow independent concurrent voices, including the same event id.
   Streams have one cursor: concurrent playback and explicit stream loops are
   rejected in this block (prebuffer/loop timing remains unverified).
   RESTART is rejected: restart an explicitly owned handle with Stop + Play.
   Loop endpoints are INCLUSIVE in the contract and FMOD. Equal endpoints
   are metadata-valid but rejected by this service, never silently lengthened.
   PCM memory is copied into service-owned storage for streams; callers may
   release their input after registration. Unregister stops dependent voices.
   Return values are 1 success / 0 failure unless specified otherwise. */
#define AUDIO_SERVICE_SOUND_MAX 256

#ifdef _WIN32
#ifdef __cplusplus
extern "C" {
#endif
int AudioService_Init(char* error, int errorCapacity);
int AudioService_IsActive(void);
void AudioService_Update(void);
void AudioService_Shutdown(void);
/* Rejects invalid/parallel bases; normalizes forward and orthogonalizes up.
   Cached state is committed only after FMOD accepts the pose. */
int AudioService_SetListener(const AudioListener* listener, char* error, int cap);
int AudioService_RegisterSound(unsigned int eventId, const char* wavPath, int streaming, char* error, int cap);
/* PCM16 interleaved, mono/stereo; bytes == frames * channels * 2.
   No loop: loopStart == loopEnd == -1. Otherwise inclusive distinct bounds. */
int AudioService_RegisterSoundMemory(unsigned int eventId, const void* pcm16, int bytes, int channels,
 int sampleRate, int frameCount, int loopStart, int loopEnd, int streaming, char* error, int cap);
int AudioService_UnregisterSound(unsigned int eventId);
int AudioService_Play(const AudioPlayRequest* request, AudioVoiceHandle* out, char* error, int cap);
int AudioService_Stop(AudioVoiceHandle handle);
int AudioService_Pause(AudioVoiceHandle handle, int paused);
int AudioService_SetVoicePitch(AudioVoiceHandle handle, float pitch);
int AudioService_SetVoiceVolume(AudioVoiceHandle handle, float volume);
/* Explicitly transitions a 2D voice to 3D linear rolloff. */
int AudioService_SetVoicePosition(AudioVoiceHandle handle, const float position[3], const float velocity[3]);
int AudioService_IsVoicePlaying(AudioVoiceHandle handle);
int AudioService_VoiceCount(void);
const char* AudioService_Version(void);
/* SDK-free observation seam: queries actual SDK state, not cached requests. */
int AudioService_QuerySound(unsigned int eventId, unsigned int* frames, unsigned int* loopStart, unsigned int* loopEnd);
int AudioService_QueryVoice(AudioVoiceHandle handle, int* spatialized, int* paused, float position[3]);
int AudioService_QueryListener(AudioListener* listener);
/* Packed-BCD formatter, also used by Version; returns 0 for invalid digits. */
int AudioService_FormatVersion(unsigned int packed, char* out, int cap);
#ifdef __cplusplus
}
#endif
#endif
#endif
