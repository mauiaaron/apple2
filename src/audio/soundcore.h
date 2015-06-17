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

typedef struct AudioBuffer_s {
    bool bActive;            // Playback is active
    bool bMute;
    long nVolume;            // Current volume (as used by DirectSound)
    void *_this;
#warning TODO rename _this variable to indicate that it is implementation_specific

    long (*SetVolume)(void* _this, long lVolume);

    long (*GetVolume)(void* _this, long *lplVolume);

    long (*GetCurrentPosition)(void* _this, unsigned long *lpdwCurrentPlayCursor, unsigned long *lpdwCurrentWriteCursor);

    long (*Stop)(void* _this);

    // This method restores the memory allocation for a lost sound buffer for the specified DirectSoundBuffer object.
    long (*Restore)(void *_this);

    long (*Play)(void* _this, unsigned long dwReserved1, unsigned long dwReserved2, unsigned long dwFlags);

    // This method obtains a valid write pointer to the sound buffer's audio data
    long (*Lock)(void* _this, unsigned long dwWriteCursor, unsigned long dwWriteBytes, void **lplpvAudioPtr1, unsigned long *lpdwAudioBytes1, void **lplpvAudioPtr2, unsigned long *lpdwAudioBytes2, unsigned long dwFlags);

    // This method releases a locked sound buffer.
    long (*Unlock)(void* _this, void *lpvAudioPtr1, unsigned long dwAudioBytes1, void *lpvAudioPtr2, unsigned long dwAudioBytes2);

    long (*GetStatus)(void* _this, unsigned long *lpdwStatus);

    // Mockingboard-specific HACKS
    long (*UnlockStaticBuffer)(void* _this, unsigned long dwAudioBytes);
    long (*Replay)(void* _this);

} AudioBuffer_s;

typedef struct AudioParams_s {
    uint16_t nChannels;
    unsigned long nSamplesPerSec;
    unsigned long nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    unsigned long dwBufferBytes;
} AudioParams_s;

typedef struct AudioContext_s {
    void *implementation_specific;
    long (*CreateSoundBuffer)(const AudioParams_s *params, INOUT AudioBuffer_s **buffer, const struct AudioContext_s *sound_system);
    long (*DestroySoundBuffer)(INOUT AudioBuffer_s **buffer);
} AudioContext_s;


bool DSGetLock(AudioBuffer_s *pVoice, unsigned long dwOffset, unsigned long dwBytes,
                      int16_t** ppDSLockedBuffer0, unsigned long* pdwDSLockedBufferSize0,
                      int16_t** ppDSLockedBuffer1, unsigned long* pdwDSLockedBufferSize1);

long DSGetSoundBuffer(INOUT AudioBuffer_s **pVoice, unsigned long dwFlags, unsigned long dwBufferSize, unsigned long nSampleRate, int nChannels);
void DSReleaseSoundBuffer(INOUT AudioBuffer_s **pVoice);

bool DSZeroVoiceBuffer(AudioBuffer_s *pVoice, char *pszDevName, unsigned long dwBufferSize);
bool DSZeroVoiceWritableBuffer(AudioBuffer_s *pVoice, char *pszDevName, unsigned long dwBufferSize);

typedef enum eFADE {FADE_NONE, FADE_IN, FADE_OUT} eFADE;
void SoundCore_SetFade(eFADE FadeType);

int SoundCore_GetErrorInc(void);
void SoundCore_SetErrorInc(const int nErrorInc);
int SoundCore_GetErrorMax(void);
void SoundCore_SetErrorMax(const int nErrorMax);

/*
 * Prepare the audio subsystem, including the backend renderer.
 */
bool audio_init(void);

/*
 * Shutdown the audio subsystem and backend renderer.
 */
void audio_shutdown(void);

extern bool audio_isAvailable;

typedef struct AudioBackend_s {

    // basic backend functionality controlled by soundcore
    PRIVATE long (*init)(const char *sound_device, INOUT AudioContext_s **audio_context);
    PRIVATE long (*shutdown)(INOUT AudioContext_s **audio_context);
    PRIVATE long (*enumerateDevices)(INOUT char ***sound_devices, const int maxcount);

    PUBLIC long (*pause)(void);
    PUBLIC long (*resume)(void);

} AudioBackend_s;

/*
 * The registered audio backend (renderer).
 */
extern AudioBackend_s *audio_backend;

#endif /* whole file */
