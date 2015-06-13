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

#ifndef _DS_SHIM_H_
#define _DS_SHIM_H_

// 2013/09/19 - http://msdn.microsoft.com/en-us/library/ms897820.aspx

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

    int (*UnlockStaticBuffer)(void* _this, unsigned long dwAudioBytes);

    int (*Replay)(void* _this);

} IDirectSoundBuffer, *LPDIRECTSOUNDBUFFER, **LPLPDIRECTSOUNDBUFFER;

#define DS_OK                           0

#define     DSBSTATUS_LOOPING   0x00000004
#define     DSBSTATUS_PLAYING   0x00000001
#define     _DSBSTATUS_NOTPLAYING 0x08000000
#define DSBPLAY_LOOPING             0x00000001
#define DSBVOLUME_MIN               -10000
#define DSBVOLUME_MAX               0

#if defined(FAILED)
#undef FAILED
#endif
#if defined(SUCCEEDED)
#undef SUCCEEDED
#endif
static inline bool FAILED(int x) { return x != DS_OK; }
static inline bool SUCCEEDED(int x) { return x == DS_OK; }

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

#endif /* whole file */

