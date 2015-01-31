/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

/*
 * Sources derived from AppleWin emulator
 * Ported by Aaron Culliney
 */

#ifndef _SOUNDCORE_H_
#define _SOUNDCORE_H_

#include "audio/ds-shim.h"

#define MAX_SAMPLES (8*1024)

extern bool g_bDisableDirectSound;

typedef struct
{
    LPDIRECTSOUNDBUFFER lpDSBvoice;
#ifdef APPLE2IX
        // apparently lpDSNotify isn't used...
#define LPDIRECTSOUNDNOTIFY void*
#endif
    LPDIRECTSOUNDNOTIFY lpDSNotify;
    bool bActive;            // Playback is active
    bool bMute;
    LONG nVolume;            // Current volume (as used by DirectSound)
    LONG nFadeVolume;        // Current fade volume (as used by DirectSound)
    DWORD dwUserVolume;        // Volume from slider on Property Sheet (0=Max)
    bool bIsSpeaker;
    bool bRecentlyActive;    // (Speaker only) false after 0.2s of speaker inactivity
} VOICE, *PVOICE;


bool DSGetLock(LPDIRECTSOUNDBUFFER pVoice, DWORD dwOffset, DWORD dwBytes,
                      SHORT** ppDSLockedBuffer0, DWORD* pdwDSLockedBufferSize0,
                      SHORT** ppDSLockedBuffer1, DWORD* pdwDSLockedBufferSize1);

HRESULT DSGetSoundBuffer(VOICE* pVoice, DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels);
void DSReleaseSoundBuffer(VOICE* pVoice);

bool DSZeroVoiceBuffer(PVOICE Voice, char* pszDevName, DWORD dwBufferSize);
bool DSZeroVoiceWritableBuffer(PVOICE Voice, char* pszDevName, DWORD dwBufferSize);

typedef enum eFADE {FADE_NONE, FADE_IN, FADE_OUT} eFADE;
void SoundCore_SetFade(eFADE FadeType);

int SoundCore_GetErrorInc();
void SoundCore_SetErrorInc(const int nErrorInc);
int SoundCore_GetErrorMax();
void SoundCore_SetErrorMax(const int nErrorMax);

bool DSInit();
void DSUninit();

LONG NewVolume(DWORD dwVolume, DWORD dwVolumeMax);

void SysClk_WaitTimer();
bool SysClk_InitTimer();
void SysClk_UninitTimer();
void SysClk_StartTimerUsec(DWORD dwUsecPeriod);
void SysClk_StopTimer();

//

extern bool g_bDSAvailable;

#ifdef APPLE2IX
typedef struct IDirectSound SoundSystemStruct;
long SoundSystemCreate(const char *sound_device, SoundSystemStruct **sound_struct);
long SoundSystemDestroy(SoundSystemStruct **sound_struct);
long SoundSystemPause();
long SoundSystemUnpause();
long SoundSystemEnumerate(char ***sound_devices, const int maxcount);
#endif

#endif /* whole file */
