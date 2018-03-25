/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

/*
 * Apple //e core sound system support. Source inspired/derived from AppleWin.
 *
 * 2015/10/01 AUDIO LIFECYCLE WARNING : CPU thread owns all audio, otherwise bad things may happen in system sound layer
 * (OpenAL/OpenSLES/ALSA/etc)
 */

#include "common.h"

#define MAX_SOUND_DEVICES 100

static AudioContext_s *audioContext = NULL;

bool audio_isAvailable = false;
float audio_latencySecs = 0.25f;


typedef struct backend_node_s {
    struct backend_node_s *next;
    long order;
    AudioBackend_s *backend;
} backend_node_s;

static backend_node_s *head = NULL;

static AudioBackend_s *currentBackend = NULL;

//-----------------------------------------------------------------------------

long audio_createSoundBuffer(INOUT AudioBuffer_s **audioBuffer) {
    // CPU thread owns audio lifecycle (see note above)
    ASSERT_ON_CPU_THREAD();

    if (!audio_isAvailable) {
        *audioBuffer = NULL;
        return -1;
    }

    if (*audioBuffer) {
        audio_destroySoundBuffer(audioBuffer);
    }

    long err = 0;
    do {
        if (!audioContext) {
            LOG("Cannot create sound buffer, no context");
            err = -1;
            break;
        }
        err = audioContext->CreateSoundBuffer(audioContext, audioBuffer);
        if (err) {
            break;
        }
    } while (0);

    return err;
}

void audio_destroySoundBuffer(INOUT AudioBuffer_s **audioBuffer) {
    // CPU thread owns audio lifecycle (see note above)
    ASSERT_ON_CPU_THREAD();
    if (audioContext) {
        audioContext->DestroySoundBuffer(audioContext, audioBuffer);
    }
}

bool audio_init(void) {
    // CPU thread owns audio lifecycle (see note above)
    ASSERT_ON_CPU_THREAD();
    if (audio_isAvailable) {
        return true;
    }

    do {
        if (audioContext) {
            currentBackend->shutdown(&audioContext);
        }

        long err = currentBackend->setup((AudioContext_s**)&audioContext);
        if (err) {
            LOG("Failed to create an audio context!");
            break;
        }

        audio_isAvailable = true;
    } while (0);

    return audio_isAvailable;
}

void audio_shutdown(void) {
    // CPU thread owns audio lifecycle (see note above)
    ASSERT_ON_CPU_THREAD();
    if (!audio_isAvailable) {
        return;
    }
    currentBackend->shutdown(&audioContext);
    audio_isAvailable = false;
}

void audio_pause(void) {
    // CPU thread owns audio lifecycle (see note above)
    // Deadlock on Kindle Fire 1st Gen if audio_pause() and audio_resume() happen off CPU thread ...
#if TARGET_OS_MAC || TARGET_OS_PHONE
#   warning FIXME TODO : this assert is firing on iOS port ... but the assert is valid ... fix soon 
#else
    ASSERT_ON_CPU_THREAD();
#endif
    if (!audio_isAvailable) {
        return;
    }
    currentBackend->pause(audioContext);
}

void audio_resume(void) {
    // CPU thread owns audio lifecycle (see note above)
    ASSERT_ON_CPU_THREAD();
    if (!audio_isAvailable) {
        return;
    }
    currentBackend->resume(audioContext);
}

void audio_setLatency(float latencySecs) {
    audio_latencySecs = latencySecs;
}

float audio_getLatency(void) {
    return audio_latencySecs;
}

//-----------------------------------------------------------------------------

void audio_registerBackend(AudioBackend_s *backend, long order) {
    backend_node_s *node = MALLOC(sizeof(backend_node_s));
    assert(node);
    node->next = NULL;
    node->order = order;
    node->backend = backend;

    backend_node_s *p0 = NULL;
    backend_node_s *p = head;
    while (p && (order > p->order)) {
        p0 = p;
        p = p->next;
    }
    if (p0) {
        p0->next = node;
    } else {
        head = node;
    }
    node->next = p;

    currentBackend = head->backend;
}

void audio_printBackends(FILE *out) {
    backend_node_s *p = head;
    int count = 0;
    while (p) {
        const char *name = p->backend->name();
        if (count++) {
            fprintf(out, "|");
        }
        fprintf(out, "%s", name);
        p = p->next;
    }
}

static const char *_null_backend_name(void);
void audio_chooseBackend(const char *name) {
    if (!name) {
        name = _null_backend_name();
    }

    backend_node_s *p = head;
    while (p) {
        const char *bname = p->backend->name();
        if (strcasecmp(name, bname) == 0) {
            currentBackend = p->backend;
            LOG("Setting current audio backend to %s", name);
            break;
        }
        p = p->next;
    }
}

AudioBackend_s *audio_getCurrentBackend(void) {
    return currentBackend;
}

static const char *_null_backend_name(void) {
    return "none";
}

static long _null_backend_setup(INOUT AudioContext_s **audio_context) {
    *audio_context = NULL;
    return -1;
}

static long _null_backend_shutdown(INOUT AudioContext_s **audio_context) {
    *audio_context = NULL;
    return -1;
}

static long _null_backend_pause(AudioContext_s *audio_context) {
    return -1;
}

static long _null_backend_resume(AudioContext_s *audio_context) {
    return -1;
}

static void _init_soundcore(void) {
    LOG("Initializing audio subsystem");
    static AudioBackend_s null_backend = { { 0 } };
    null_backend.name     = &_null_backend_name;
    null_backend.setup    = &_null_backend_setup;
    null_backend.shutdown = &_null_backend_shutdown;
    null_backend.pause    = &_null_backend_pause;
    null_backend.resume   = &_null_backend_resume;
    audio_registerBackend(&null_backend, AUD_PRIO_NULL);
}

static __attribute__((constructor)) void __init_soundcore(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_soundcore);
}

