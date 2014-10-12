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

#ifndef _SOUNDCORE_OPENAL_H_
#define _SOUNDCORE_OPENAL_H_

#include "common.h"

#ifdef __APPLE__
#import <OpenAL/al.h>
#import <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

#undef DSBVOLUME_MIN
#define DSBVOLUME_MIN 0

#undef DSBVOLUME_MAX
#define DSBVOLUME_MAX 100

typedef struct IDirectSoundBuffer ALSoundBufferStruct;

typedef struct DSBUFFERDESC ALBufferParamsStruct;
/*
    DWORD dwSize; 
    DWORD dwFlags; 
    DWORD dwBufferBytes; 
    DWORD dwReserved; 
    LPWAVEFORMATEX lpwfxFormat
    {
        WORD  wFormatTag;
        WORD  nChannels;
        DWORD nSamplesPerSec;
        DWORD nAvgBytesPerSec;
        WORD  nBlockAlign;
        WORD  wBitsPerSample;
        WORD  cbSize;
    }
*/

struct ALPlayBuf;
typedef struct ALPlayBuf {
    const ALuint bufid; // the hash id
    ALuint bytes;       // bytes to play
    UT_hash_handle hh;  // make this struct hashable
    struct ALPlayBuf *_avail_next;
} ALPlayBuf;

#define OPENAL_NUM_BUFFERS 4
typedef struct ALVoice {
    ALuint source;

    // playing data
    ALPlayBuf *queued_buffers;
    ALint _queued_total_bytes; // a maximum estimate -- actual value depends on OpenAL query

    // working data buffer
    ALbyte *data;
    ALsizei index;      // working buffer byte index
    ALsizei buffersize; // working buffer size (and OpenAL buffersize)

    // available buffers
    ALPlayBuf *avail_buffers;

    ALsizei replay_index;

    // sample parameters
    ALenum format;
    ALenum channels;
    ALenum type;
    ALuint rate;
} ALVoice;

#endif /* whole file */

