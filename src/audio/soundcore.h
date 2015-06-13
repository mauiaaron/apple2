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
 * Apple //e core sound system support. Source inspired/derived from AppleWin.
 *
 */

#ifndef _SOUNDCORE_H_
#define _SOUNDCORE_H_

#define MAX_SAMPLES (8*1024)
#define AUDIO_STATUS_PLAYING    0x00000001
#define AUDIO_STATUS_NOTPLAYING 0x08000000

typedef struct IDirectSoundBuffer {

    void *_this;

    int (*SetVolume)(void* _this, long lVolume);

    int (*GetVolume)(void* _this, long *lplVolume);

    int (*GetCurrentPosition)(void* _this, unsigned long *lpdwCurrentPlayCursor, unsigned long *lpdwCurrentWriteCursor);

    int (*Stop)(void* _this);

    // This method restores the memory allocation for a lost sound buffer for the specified DirectSoundBuffer object.
    int (*Restore)(void *_this);

    int (*Play)(void* _this, unsigned long dwReserved1, unsigned long dwReserved2, unsigned long dwFlags);

    // This method obtains a valid write pointer to the sound buffer's audio data
    int (*Lock)(void* _this, unsigned long dwWriteCursor, unsigned long dwWriteBytes, void **lplpvAudioPtr1, unsigned long *lpdwAudioBytes1, void **lplpvAudioPtr2, unsigned long *lpdwAudioBytes2, unsigned long dwFlags);

    // This method releases a locked sound buffer.
    int (*Unlock)(void* _this, void *lpvAudioPtr1, unsigned long dwAudioBytes1, void *lpvAudioPtr2, unsigned long dwAudioBytes2);

    int (*GetStatus)(void* _this, unsigned long *lpdwStatus);

    // Mockingboard-specific HACKS
    int (*UnlockStaticBuffer)(void* _this, unsigned long dwAudioBytes);
    int (*Replay)(void* _this);

} IDirectSoundBuffer, *LPDIRECTSOUNDBUFFER, **LPLPDIRECTSOUNDBUFFER;

typedef struct AudioParams_s {
    uint16_t nChannels;
    unsigned long nSamplesPerSec;
    unsigned long nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    unsigned long dwBufferBytes;
} AudioParams_s;

typedef struct IDirectSound {
    void *implementation_specific;
    int (*CreateSoundBuffer)(AudioParams_s *pcDSBufferDesc, LPDIRECTSOUNDBUFFER * ppDSBuffer, void *pUnkOuter);
    int (*DestroySoundBuffer)(LPDIRECTSOUNDBUFFER * ppDSBuffer);
} IDirectSound, *LPDIRECTSOUND;

typedef struct
{
    LPDIRECTSOUNDBUFFER lpDSBvoice;
    bool bActive;            // Playback is active
    bool bMute;
    long nVolume;            // Current volume (as used by DirectSound)
    long nFadeVolume;        // Current fade volume (as used by DirectSound)
    unsigned long dwUserVolume;        // Volume from slider on Property Sheet (0=Max)
    bool bIsSpeaker;
    bool bRecentlyActive;    // (Speaker only) false after 0.2s of speaker inactivity
} VOICE, *PVOICE;


bool DSGetLock(LPDIRECTSOUNDBUFFER pVoice, unsigned long dwOffset, unsigned long dwBytes,
                      int16_t** ppDSLockedBuffer0, unsigned long* pdwDSLockedBufferSize0,
                      int16_t** ppDSLockedBuffer1, unsigned long* pdwDSLockedBufferSize1);

int DSGetSoundBuffer(VOICE* pVoice, unsigned long dwFlags, unsigned long dwBufferSize, unsigned long nSampleRate, int nChannels);
void DSReleaseSoundBuffer(VOICE* pVoice);

bool DSZeroVoiceBuffer(PVOICE Voice, char* pszDevName, unsigned long dwBufferSize);
bool DSZeroVoiceWritableBuffer(PVOICE Voice, char* pszDevName, unsigned long dwBufferSize);

typedef enum eFADE {FADE_NONE, FADE_IN, FADE_OUT} eFADE;
void SoundCore_SetFade(eFADE FadeType);

int SoundCore_GetErrorInc();
void SoundCore_SetErrorInc(const int nErrorInc);
int SoundCore_GetErrorMax();
void SoundCore_SetErrorMax(const int nErrorMax);

bool DSInit();
void DSUninit();

extern bool soundcore_isAvailable;

typedef struct IDirectSound SoundSystemStruct;
long SoundSystemCreate(const char *sound_device, SoundSystemStruct **sound_struct);
long SoundSystemDestroy(SoundSystemStruct **sound_struct);
long SoundSystemPause();
long SoundSystemUnpause();
long SoundSystemEnumerate(char ***sound_devices, const int maxcount);

#endif /* whole file */
