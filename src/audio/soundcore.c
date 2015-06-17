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

long DSGetLock(AudioBuffer_s *pVoice, unsigned long dwOffset, unsigned long dwBytes, INOUT int16_t **ppDSLockedBuffer0, INOUT unsigned long *pdwDSLockedBufferSize0, INOUT int16_t **ppDSLockedBuffer1, INOUT unsigned long *pdwDSLockedBufferSize1) {

    long err = 0;
    do {
        if (dwBytes == 0) {
            err = pVoice->Lock(pVoice->_this, 0, 0, (void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0, (void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1, 0);
            if (err) {
                break;
            }
        } else {
            err = pVoice->Lock(pVoice->_this, dwOffset, dwBytes, (void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0, (void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1, 0);
            if (err) {
                break;
            }
        }
    } while (0);

    return err;
}

long DSGetSoundBuffer(INOUT AudioBuffer_s **pVoice, unsigned long dwFlags, unsigned long dwBufferSize, unsigned long nSampleRate, int nChannels) {
    AudioParams_s params = { 0 };

    params.nChannels = nChannels;
    params.nSamplesPerSec = nSampleRate;
    params.wBitsPerSample = 16;
    params.nBlockAlign = (params.nChannels == 1) ? 2 : 4;
    params.nAvgBytesPerSec = params.nBlockAlign * params.nSamplesPerSec;
    params.dwBufferBytes = dwBufferSize;

    if (*pVoice) {
        DSReleaseSoundBuffer(pVoice);
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

void DSReleaseSoundBuffer(INOUT AudioBuffer_s **pVoice) {
    audioContext->DestroySoundBuffer(pVoice);
}

bool audio_init(void) {
    if (audio_isAvailable) {
        return true;
    }

    char **sound_devices = NULL;
    long num_sound_devices = audio_backend->enumerateDevices(&sound_devices, MAX_SOUND_DEVICES);
    long err = (num_sound_devices <= 0);

    do {
        if (err) {
            LOG("enumerate sound devices failed to find any devices : %ld", err);
            break;
        }

        LOG("Number of sound devices = %ld", num_sound_devices);
        bool createdAudioContext = false;
        for (int i=0; i<num_sound_devices; i++) {
            if (audioContext) {
                audio_backend->shutdown(&audioContext);
            }
            err = audio_backend->init(sound_devices[i], (AudioContext_s**)&audioContext);
            if (!err) {
                createdAudioContext = true;
                break;
            }

            LOG("warning : failed to create sound device:%d err:%ld", i, err);
        }

        if (!createdAudioContext) {
            LOG("Failed to create an audio context!");
            err = true;
            break;
        }

        LOG("Created an audio context!");
        audio_isAvailable = true;
    } while (0);

    if (num_sound_devices > 0) {
        char **p = sound_devices;
        while (*p) {
            FREE(*p);
            ++p;
        }
        FREE(sound_devices);
        sound_devices = NULL;
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

