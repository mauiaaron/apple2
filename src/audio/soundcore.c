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
 */

#include "common.h"

#define MAX_SOUND_DEVICES 100

static AudioContext_s *audioContext = NULL;

bool audio_isAvailable = false;
AudioBackend_s *audio_backend = NULL;

//-----------------------------------------------------------------------------

long audio_createSoundBuffer(INOUT AudioBuffer_s **pVoice, unsigned long dwBufferSize, unsigned long nSampleRate, int nChannels) {
    AudioParams_s params = { 0 };

    params.nChannels = nChannels;
    params.nSamplesPerSec = nSampleRate;
    params.wBitsPerSample = 16;
    params.nBlockAlign = (params.nChannels == 1) ? 2 : 4;
    params.nAvgBytesPerSec = params.nBlockAlign * params.nSamplesPerSec;
    params.dwBufferBytes = dwBufferSize;

    if (*pVoice) {
        audio_destroySoundBuffer(pVoice);
    }

    long err = 0;
    do {
        err = audioContext->CreateSoundBuffer(&params, pVoice, audioContext);
        if (err) {
            break;
        }
    } while (0);

    return err;
}

void audio_destroySoundBuffer(INOUT AudioBuffer_s **audioBuffer) {
    audioContext->DestroySoundBuffer(audioBuffer);
}

bool audio_init(void) {
    if (audio_isAvailable) {
        return true;
    }

    if (audioContext) {
        audio_backend->shutdown(&audioContext);
    }

    long err = audio_backend->setup((AudioContext_s**)&audioContext);
    if (err) {
        LOG("Failed to create an audio context!");
    } else {
        audio_isAvailable = true;
    }

    return err;
}

void audio_shutdown(void) {
    if (!audio_isAvailable) {
        return;
    }
    audio_backend->shutdown(&audioContext);
    audio_isAvailable = false;
}

void audio_pause(void) {
    audio_backend->pause();
}

void audio_resume(void) {
    audio_backend->resume();
}

