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

long audio_createSoundBuffer(INOUT AudioBuffer_s **audioBuffer, unsigned long numChannels) {

    AudioSettings_s *settings = &audio_backend->systemSettings;

    if (*audioBuffer) {
        audio_destroySoundBuffer(audioBuffer);
    }

    long err = 0;
    do {
        if (!audioContext) {
            ERRLOG("Cannot create sound buffer, no context");
            err = -1;
            break;
        }
        err = audioContext->CreateSoundBuffer(audioContext, numChannels, audioBuffer);
        if (err) {
            break;
        }
    } while (0);

    return err;
}

void audio_destroySoundBuffer(INOUT AudioBuffer_s **audioBuffer) {
    if (audioContext) {
        audioContext->DestroySoundBuffer(audioContext, audioBuffer);
    }
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

