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

// soundcore OpenSLES backend -- streaming audio

#include "common.h"

#include <SLES/OpenSLES.h>
#if defined(ANDROID)
#   include <SLES/OpenSLES_Android.h>
#else
#   error FIXME TODO this currently uses Android BufferQueue extensions...
#endif

#include "uthash.h"

#define DEBUG_OPENSL 0
#if DEBUG_OPENSL
#   define OPENSL_LOG(...) LOG(__VA_ARGS__)
#else
#   define OPENSL_LOG(...)
#endif

#define OPENSL_NUM_BUFFERS 4

typedef struct SLVoice {
    unsigned long voiceId;

    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerPlay;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLMuteSoloItf bqPlayerMuteSolo;
    SLVolumeItf bqPlayerVolume;

    // working data buffer
    uint8_t *ringBuffer;            // ringBuffer of total size : bufferSize+submitSize
    uint8_t *submitBuf;             // submitBuffer
    unsigned long bufferSize;       // ringBuffer non-overflow size
    unsigned long submitSize;       // buffer size OpenSLES expects/wants
    unsigned long writeHead;        // head of the writer of ringBuffer (speaker, mockingboard)
    unsigned long writeWrapCount;   // count of buffer wraps for the writer

    unsigned long spinLock;         // spinlock around reader variables
    unsigned long readHead;         // head of the reader of ringBuffer (OpenSLES callback)
    unsigned long readWrapCount;    // count of buffer wraps for the reader

    unsigned long replay_index;

    // misc
    unsigned long numChannels;
    bool backfillQuiet
} SLVoice;

typedef struct SLVoices {
    unsigned long voiceId;
    SLVoice *voice;
    UT_hash_handle hh;
} SLVoices;

typedef struct EngineContext_s {
    SLObjectItf engineObject;
    SLEngineItf engineEngine;
    SLObjectItf outputMixObject;
} EngineContext_s;

static SLVoices *voices = NULL;
static AudioBackend_s opensles_audio_backend = { 0 };

// ----------------------------------------------------------------------------
// AudioBuffer_s internal processing routines

// Check and resets underrun condition (readHead has advanced beyond writeHead)
static inline bool _underrun_check_and_manage(SLVoice *voice, OUTPARM unsigned long *workingBytes) {

    SPINLOCK_ACQUIRE(&voice->spinLock);
    unsigned long readHead = voice->readHead;
    unsigned long readWrapCount = voice->readWrapCount;
    SPINLOCK_RELINQUISH(&voice->spinLock);

    assert(readHead < voice->bufferSize);
    assert(voice->writeHead < voice->bufferSize);

    bool isUnder = false;
    if ( (readWrapCount > voice->writeWrapCount) ||
            ((readHead >= voice->writeHead) && (readWrapCount == voice->writeWrapCount)) )
    {
        isUnder = true;
        LOG("Buffer underrun ... queuing quiet samples ...");

        voice->writeHead = readHead;
        voice->writeWrapCount = readWrapCount;
        memset(voice->ringBuffer+voice->writeHead, 0x0, voice->submitSize);
        voice->writeHead += voice->submitSize;

        if (voice->writeHead >= voice->bufferSize) {
            voice->writeHead = voice->writeHead - voice->bufferSize;
            memset(voice->ringBuffer+voice->bufferSize, 0x0, voice->submitSize);
            memset(voice->ringBuffer, 0x0, voice->writeHead);
            ++voice->writeWrapCount;
        }
    }

    if (readHead <= voice->writeHead) {
        *workingBytes = voice->writeHead - readHead;
    } else {
        *workingBytes = voice->writeHead + (voice->bufferSize - readHead);
    }

    return isUnder;
}

// This callback handler is called presumably every time (or just prior to when) a buffer finishes playing and the
// system needs moar data (of the correct buffer size)
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {

    SLVoice *voice = (SLVoice *)context;

    // enqueue next buffer of correct size to OpenSLES
    // invariant : we can always read submitSize amount from the position of readHead

    SLresult result = SL_RESULT_SUCCESS;
    if (voice->backfillQuiet) {
        memcpy(voice->submitBuf, voice->ringBuffer+voice->readHead, voice->submitSize);
        result = (*bq)->Enqueue(bq, voice->submitBuf, voice->submitSize);
        memset(voice->ringBuffer+voice->readHead, 0x0, voice->submitSize);
    } else {
        result = (*bq)->Enqueue(bq, voice->ringBuffer+voice->readHead, voice->submitSize);
    }

    // now manage overflow/wrapping ... (it's easier to ask for buffer overflow forgiveness than permission ;-)

    unsigned long newReadHead = voice->readHead + voice->submitSize;
    unsigned long newReadWrapCount = voice->readWrapCount;

    if (newReadHead >= voice->bufferSize) {
        newReadHead = newReadHead - voice->bufferSize;
        if (voice->backfillQuiet) {
            memset(voice->ringBuffer+voice->bufferSize, 0x0, voice->submitSize);
            memset(voice->ringBuffer, 0x0, newReadHead);
        }
        ++newReadWrapCount;
    }

    SPINLOCK_ACQUIRE(&voice->spinLock);
    voice->readHead = newReadHead;
    voice->readWrapCount = newReadWrapCount;
    SPINLOCK_RELINQUISH(&voice->spinLock);

    if (result != SL_RESULT_SUCCESS) {
        LOG("WARNING: could not enqueue data to OpenSLES!");
        (*(voice->bqPlayerPlay))->SetPlayState(voice->bqPlayerPlay, SL_PLAYSTATE_STOPPED);
    }
}

static long _SLMaybeSubmitAndStart(SLVoice *voice) {
    SLuint32 state = 0;
    SLresult result = (*(voice->bqPlayerPlay))->GetPlayState(voice->bqPlayerPlay, &state);
    if (result != SL_RESULT_SUCCESS) {
        ERRLOG("OOPS, could not get source state : %lu", result);
    } else {
        if ((state != SL_PLAYSTATE_PLAYING) && (state != SL_PLAYSTATE_PAUSED)) {
            LOG("FORCING restart audio buffer queue playback ...");
            result = (*(voice->bqPlayerPlay))->SetPlayState(voice->bqPlayerPlay, SL_PLAYSTATE_PLAYING);
            bqPlayerCallback(voice->bqPlayerBufferQueue, voice);
        }
    }
    return result;
}

// ----------------------------------------------------------------------------
// AudioBuffer_s public API handlers

// returns queued+working sound buffer size in bytes
static long SLGetPosition(AudioBuffer_s *_this, OUTPARM unsigned long *bytes_queued) {
    *bytes_queued = 0;
    long err = 0;

    do {
        SLVoice *voice = (SLVoice*)_this->_internal;

        unsigned long workingBytes = 0;
        bool underrun = _underrun_check_and_manage(voice, &workingBytes);
        //bool overrun = _overrun_check_and_manage(voice);

        unsigned long queuedBytes = 0;
        if (!underrun) {
            //queuedBytes = voice->submitSize; // assume that there are always about this much actually queued
        }

        assert(workingBytes <= voice->bufferSize);
        *bytes_queued = workingBytes;
    } while (0);

    return err;
}

static long SLLockBuffer(AudioBuffer_s *_this, unsigned long write_bytes, INOUT int16_t **audio_ptr, OUTPARM unsigned long *audio_bytes) {
    *audio_bytes = 0;
    *audio_ptr = NULL;
    long err = 0;

    //OPENSL_LOG("SLLockBuffer request for %ld bytes", write_bytes);

    do {
        SLVoice *voice = (SLVoice*)_this->_internal;

        if (write_bytes == 0) {
            LOG("HMMM ... writing full buffer!");
            write_bytes = voice->bufferSize;
        }

        unsigned long workingBytes = 0;
        _underrun_check_and_manage(voice, &workingBytes);
        unsigned long availableBytes = voice->bufferSize - workingBytes;

        assert(workingBytes <= voice->bufferSize);
        assert(voice->writeHead < voice->bufferSize);

        // TODO FIXME : maybe need to resurrect the 2 inner pointers and foist the responsibility onto the
        // speaker/mockingboard modules so we can actually write moar here?
        unsigned long writableBytes = MIN( availableBytes, ((voice->bufferSize+voice->submitSize) - voice->writeHead) );

        if (write_bytes > writableBytes) {
            OPENSL_LOG("NOTE truncating audio buffer (call again to write complete requested buffer) ...");
            write_bytes = writableBytes;
        }

        *audio_ptr = (int16_t *)(voice->ringBuffer+voice->writeHead);
        *audio_bytes = write_bytes;
    } while (0);

    return err;
}

static long SLUnlockBuffer(AudioBuffer_s *_this, unsigned long audio_bytes) {
    long err = 0;

    do {
        SLVoice *voice = (SLVoice*)_this->_internal;

        unsigned long previousWriteHead = voice->writeHead;

        voice->writeHead += audio_bytes;

        assert((voice->writeHead <= (voice->bufferSize + voice->submitSize)) && "OOPS, real overflow in queued sound data!");

        if (voice->writeHead >= voice->bufferSize) {
            // copy data from overflow into beginning of buffer
            voice->writeHead = voice->writeHead - voice->bufferSize;
            ++voice->writeWrapCount;
            memcpy(voice->ringBuffer, voice->ringBuffer+voice->bufferSize, voice->writeHead);
        } else if (previousWriteHead < voice->submitSize) {
            // copy data in beginning of buffer into overflow position
            unsigned long copyNumBytes = MIN(audio_bytes, voice->submitSize-previousWriteHead);
            memcpy(voice->ringBuffer+voice->bufferSize+previousWriteHead, voice->ringBuffer+previousWriteHead, copyNumBytes);
        }

        err = _SLMaybeSubmitAndStart(voice);
    } while (0);

    return err;
}

// HACK Part I : done once for mockingboard that has semiauto repeating phonemes ...
static long SLUnlockStaticBuffer(AudioBuffer_s *_this, unsigned long audio_bytes) {
    SLVoice *voice = (SLVoice*)_this->_internal;
    voice->replay_index = audio_bytes;
    return 0;
}

// HACK Part II : replay mockingboard phoneme ...
static long SLReplay(AudioBuffer_s *_this) {
    SLVoice *voice = (SLVoice*)_this->_internal;

    SPINLOCK_ACQUIRE(&voice->spinLock);
    voice->readHead = 0;
    voice->writeHead = voice->replay_index;
    SPINLOCK_RELINQUISH(&voice->spinLock);

    long err = _SLMaybeSubmitAndStart(voice);
#warning FIXME TODO ... how do we handle mockingboard for new OpenSLES buffer queue codepath?
    return err;
}

static long SLGetStatus(AudioBuffer_s *_this, OUTPARM unsigned long *status) {
    *status = -1;
    SLresult result = SL_RESULT_UNKNOWN_ERROR;

    do {
        SLVoice* voice = (SLVoice*)_this->_internal;

        SLuint32 state = 0;
        result = (*(voice->bqPlayerPlay))->GetPlayState(voice->bqPlayerPlay, &state);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, could not get source state : %lu", result);
            break;
        }

        if ((state == SL_PLAYSTATE_PLAYING) || (state == SL_PLAYSTATE_PAUSED)) {
            *status = AUDIO_STATUS_PLAYING;
        } else {
            *status = AUDIO_STATUS_NOTPLAYING;
        }
    } while (0);

    return (long)result;
}

// ----------------------------------------------------------------------------
// SLVoice is the AudioBuffer_s->_internal

static void _opensl_destroyVoice(SLVoice *voice) {

    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (voice->bqPlayerObject != NULL) {
        (*(voice->bqPlayerObject))->Destroy(voice->bqPlayerObject);
        voice->bqPlayerObject = NULL;
        voice->bqPlayerPlay = NULL;
        voice->bqPlayerBufferQueue = NULL;
        voice->bqPlayerMuteSolo = NULL;
        voice->bqPlayerVolume = NULL;
    }

    if (voice->ringBuffer) {
        FREE(voice->ringBuffer);
    }

    memset(voice, 0, sizeof(*voice));
    FREE(voice);
}

static SLVoice *_opensl_createVoice(unsigned long numChannels, const EngineContext_s *ctx) {
    SLVoice *voice = NULL;

    do {

        //
        // General buffer memory management
        //

        voice = calloc(1, sizeof(*voice));
        if (voice == NULL) {
            ERRLOG("OOPS, Out of memory!");
            break;
        }

        assert(numChannels == 1 || numChannels == 2);
        voice->numChannels = numChannels;

        SLuint32 channelMask = 0;
        if (numChannels == 2) {
            channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
            voice->submitSize = android_stereoBufferSubmitSizeSamples * opensles_audio_backend.systemSettings.bytesPerSample * numChannels;
            voice->bufferSize = opensles_audio_backend.systemSettings.stereoBufferSizeSamples * opensles_audio_backend.systemSettings.bytesPerSample * numChannels;
            voice->backfillQuiet = true;
            LOG("ideal stereo submission bufsize is %lu (bytes:%lu)", (unsigned long)android_stereoBufferSubmitSizeSamples, (unsigned long)voice->submitSize);
        } else {
            channelMask = SL_SPEAKER_FRONT_CENTER;
            voice->submitSize = android_monoBufferSubmitSizeSamples * opensles_audio_backend.systemSettings.bytesPerSample;
            voice->bufferSize = opensles_audio_backend.systemSettings.monoBufferSizeSamples * opensles_audio_backend.systemSettings.bytesPerSample;
            LOG("ideal mono submission bufsize is %lu (bytes:%lu)", (unsigned long)android_monoBufferSubmitSizeSamples, (unsigned long)voice->submitSize);
        }

        // Allocate enough space for the temp buffer (including a maximum allowed overflow)
        voice->ringBuffer = malloc(voice->bufferSize + voice->submitSize/*max overflow*/);
        if (voice->ringBuffer == NULL) {
            ERRLOG("OOPS, Error allocating %lu bytes", (unsigned long)voice->bufferSize+voice->submitSize);
            break;
        }

        voice->submitBuf = malloc(voice->submitSize);
        if (voice->submitBuf == NULL) {
            ERRLOG("OOPS, Error allocating %lu bytes", (unsigned long)voice->submitSize);
            break;
        }

        //
        // OpenSLES buffer queue player setup
        //

        SLresult result = SL_RESULT_UNKNOWN_ERROR;

        // configure audio source
        SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            .locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            .numBuffers = 2,
#warning FIXME TODO ... verify 2 numBuffers is best
        };
        SLDataFormat_PCM format_pcm = {
            .formatType = SL_DATAFORMAT_PCM,
            .numChannels = numChannels,
            .samplesPerSec = opensles_audio_backend.systemSettings.sampleRateHz * 1000,
            .bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16,
            .containerSize = SL_PCMSAMPLEFORMAT_FIXED_16,
            .channelMask = channelMask,
            .endianness = SL_BYTEORDER_LITTLEENDIAN,
        };
        SLDataSource audioSrc = {
            .pLocator = &loc_bufq,
            .pFormat = &format_pcm,
        };

        // configure audio sink
        SLDataLocator_OutputMix loc_outmix = {
            .locatorType = SL_DATALOCATOR_OUTPUTMIX,
            .outputMix = ctx->outputMixObject,
        };
        SLDataSink audioSnk = {
            .pLocator = &loc_outmix,
            .pFormat = NULL,
        };

        // create audio player
#define _NUM_INTERFACES 3
        const SLInterfaceID ids[_NUM_INTERFACES] = {
            SL_IID_BUFFERQUEUE,
            SL_IID_EFFECTSEND,
            //SL_IID_MUTESOLO,
            SL_IID_VOLUME,
        };
        const SLboolean req[_NUM_INTERFACES] = {
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
            //numChannels == 1 ? SL_BOOLEAN_FALSE : SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
        };

        result = (*(ctx->engineEngine))->CreateAudioPlayer(ctx->engineEngine, &(voice->bqPlayerObject), &audioSrc, &audioSnk, _NUM_INTERFACES, ids, req);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, could not create the BufferQueue player object : %lu", result);
            break;
        }

        // realize the player
        result = (*(voice->bqPlayerObject))->Realize(voice->bqPlayerObject, /*asynchronous_realization:*/SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, could not realize the BufferQueue player object : %lu", result);
            break;
        }

        // get the play interface
        result = (*(voice->bqPlayerObject))->GetInterface(voice->bqPlayerObject, SL_IID_PLAY, &(voice->bqPlayerPlay));
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, could not get the play interface : %lu", result);
            break;
        }

        // get the buffer queue interface
        result = (*(voice->bqPlayerObject))->GetInterface(voice->bqPlayerObject, SL_IID_BUFFERQUEUE, &(voice->bqPlayerBufferQueue));
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, could not get the BufferQueue play interface : %lu", result);
            break;
        }

        // register callback on the buffer queue
        result = (*(voice->bqPlayerBufferQueue))->RegisterCallback(voice->bqPlayerBufferQueue, bqPlayerCallback, voice);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, could not register BufferQueue callback : %lu", result);
            break;
        }

#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
        // get the mute/solo interface
        result = (*(voice->bqPlayerObject))->GetInterface(voice->bqPlayerObject, SL_IID_MUTESOLO, &(voice->bqPlayerMuteSolo));
        assert(SL_RESULT_SUCCESS == result);
        (void)result;
#endif

        // get the volume interface
        result = (*(voice->bqPlayerObject))->GetInterface(voice->bqPlayerObject, SL_IID_VOLUME, &(voice->bqPlayerVolume));
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, could not get the BufferQueue volume interface : %lu", result);
            break;
        }

        // Force OpenSLES to start playback
        unsigned long workingBytes;
        _underrun_check_and_manage(voice, &workingBytes);
        _SLMaybeSubmitAndStart(voice);

        return voice;

    } while(0);

    // ERR
    if (voice) {
        _opensl_destroyVoice(voice);
    }

    return NULL;
}

// ----------------------------------------------------------------------------

static long opensl_destroySoundBuffer(const struct AudioContext_s *sound_system, INOUT AudioBuffer_s **soundbuf_struct) {
    if (!*soundbuf_struct) {
        return 0;
    }

    LOG("opensl_destroySoundBuffer ...");
    SLVoice *voice = (SLVoice *)((*soundbuf_struct)->_internal);
    unsigned long voiceId = voice->voiceId;

    _opensl_destroyVoice(voice);

    SLVoices *vnode = NULL;
    HASH_FIND_INT(voices, &voiceId, vnode);
    if (vnode) {
        HASH_DEL(voices, vnode);
        FREE(vnode);
    }

    FREE(*soundbuf_struct);
    return 0;
}

static long opensl_createSoundBuffer(const AudioContext_s *audio_context, unsigned long numChannels, INOUT AudioBuffer_s **soundbuf_struct) {
    LOG("opensl_createSoundBuffer ...");
    assert(*soundbuf_struct == NULL);

    SLVoice *voice = NULL;

    do {

        EngineContext_s *ctx = (EngineContext_s *)(audio_context->_internal);
        assert(ctx != NULL);

        if ((voice = _opensl_createVoice(numChannels, ctx)) == NULL)
        {
            ERRLOG("OOPS, Cannot create new voice");
            break;
        }

        SLVoices *vnode = calloc(1, sizeof(SLVoices));
        if (!vnode) {
            ERRLOG("OOPS, Not enough memory");
            break;
        }

        static unsigned long counter = 0;
        vnode->voiceId = __sync_add_and_fetch(&counter, 1);
        voice->voiceId = vnode->voiceId;

        vnode->voice = voice;
        HASH_ADD_INT(voices, voice->voiceId, vnode);

        if ((*soundbuf_struct = malloc(sizeof(AudioBuffer_s))) == NULL) {
            ERRLOG("OOPS, Not enough memory");
            break;
        }

        (*soundbuf_struct)->_internal          = voice;
        (*soundbuf_struct)->GetCurrentPosition = &SLGetPosition;
        (*soundbuf_struct)->Lock               = &SLLockBuffer;
        (*soundbuf_struct)->Unlock             = &SLUnlockBuffer;
        (*soundbuf_struct)->GetStatus          = &SLGetStatus;
        // mockingboard-specific hacks
        (*soundbuf_struct)->UnlockStaticBuffer = &SLUnlockStaticBuffer;
        (*soundbuf_struct)->Replay             = &SLReplay;

        return 0;
    } while(0);

    if (*soundbuf_struct) {
        opensl_destroySoundBuffer(audio_context, soundbuf_struct);
    } else if (voice) {
        _opensl_destroyVoice(voice);
    }

    return -1;
}

// ----------------------------------------------------------------------------

static long opensles_systemShutdown(AudioContext_s **audio_context) {
    assert(*audio_context != NULL);

    EngineContext_s *ctx = (EngineContext_s *)((*audio_context)->_internal);
    assert(ctx != NULL);

    // destroy output mix object, and invalidate all associated interfaces
    if (ctx->outputMixObject != NULL) {
        (*(ctx->outputMixObject))->Destroy(ctx->outputMixObject);
        ctx->outputMixObject = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (ctx->engineObject != NULL) {
        (*(ctx->engineObject))->Destroy(ctx->engineObject);
        ctx->engineObject = NULL;
        ctx->engineEngine = NULL;
    }

    FREE(ctx);
    FREE(*audio_context);

    return 0;
}

static long opensles_systemSetup(INOUT AudioContext_s **audio_context) {
    assert(*audio_context == NULL);
    assert(voices == NULL);

    EngineContext_s *ctx = NULL;
    SLresult result = -1;

    opensles_audio_backend.systemSettings.sampleRateHz = android_deviceSampleRateHz;
    opensles_audio_backend.systemSettings.bytesPerSample = 2;

    if (android_deviceSampleRateHz <= 22050/*sentinel in DevicePropertyCalculator.java*/) {
        // HACK NOTE : assuming this is a low-end Gingerbread device ... try to push for a lower submit size to improve
        // latency ... this is less aggressive than calculations made in DevicePropertyCalculator.java
        android_monoBufferSubmitSizeSamples >>= 1;
        android_stereoBufferSubmitSizeSamples >>= 1;
        opensles_audio_backend.systemSettings.monoBufferSizeSamples = android_deviceSampleRateHz * 0.3/*sec*/;
        opensles_audio_backend.systemSettings.stereoBufferSizeSamples = android_deviceSampleRateHz * 0.3/*sec*/;
    } else {
        opensles_audio_backend.systemSettings.monoBufferSizeSamples = android_deviceSampleRateHz * 0.125/*sec*/;
        opensles_audio_backend.systemSettings.stereoBufferSizeSamples = android_deviceSampleRateHz * 0.125/*sec*/;
    }
#warning TODO FIXME ^^^^^ need a dynamic bufferSize calculation/calibration routine to determine optimal buffer size for device ... may also need a user-initiated calibration too

    do {
        //
        // Engine creation ...
        //
        ctx = calloc(1, sizeof(EngineContext_s));
        if (!ctx) {
            result = -1;
            break;
        }

        // create basic engine
        result = slCreateEngine(&(ctx->engineObject), 0, NULL, /*engineMixIIDCount:*/0, /*engineMixIIDs:*/NULL, /*engineMixReqs:*/NULL);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("Could not create OpenSLES Engine : %lu", result);
            break;
        }

        // realize the engine
        result = (*(ctx->engineObject))->Realize(ctx->engineObject, /*asynchronous_realization:*/SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("Could not realize the OpenSLES Engine : %lu", result);
            break;
        }

        // get the actual engine interface
        result = (*(ctx->engineObject))->GetInterface(ctx->engineObject, SL_IID_ENGINE, &(ctx->engineEngine));
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("Could not get the OpenSLES Engine : %lu", result);
            break;
        }

        //
        // Output Mix ...
        //

        result = (*(ctx->engineEngine))->CreateOutputMix(ctx->engineEngine, &(ctx->outputMixObject), 0, NULL, NULL);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("Could not create output mix : %lu", result);
            break;
        }

        // realize the output mix
        result = (*(ctx->outputMixObject))->Realize(ctx->outputMixObject, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("Could not realize the output mix : %lu", result);
            break;
        }

        // create soundcore API wrapper
        if ((*audio_context = malloc(sizeof(AudioContext_s))) == NULL) {
            result = -1;
            ERRLOG("OOPS, Not enough memory");
            break;
        }

        (*audio_context)->_internal = ctx;
        (*audio_context)->CreateSoundBuffer = &opensl_createSoundBuffer;
        (*audio_context)->DestroySoundBuffer = &opensl_destroySoundBuffer;

    } while (0);

    if (result != SL_RESULT_SUCCESS) {
        if (ctx) {
            AudioContext_s *ctxPtr = malloc(sizeof(AudioContext_s));
            ctxPtr->_internal = ctx;
            opensles_systemShutdown(&ctxPtr);
        }
        assert(*audio_context == NULL);
        LOG("OpenSLES sound output disabled");
    }

    return result;
}

// pause all audio
static long opensles_systemPause(void) {
    LOG("OpenSLES pausing play");

    SLVoices *vnode = NULL;
    SLVoices *tmp = NULL;

    HASH_ITER(hh, voices, vnode, tmp) {
        SLVoice *voice = vnode->voice;
        SLresult result = (*(voice->bqPlayerPlay))->SetPlayState(voice->bqPlayerPlay, SL_PLAYSTATE_PAUSED);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, Failed to pause source : %lu", result);
        }
    }

    return 0;
}

static long opensles_systemResume(void) {
    LOG("OpenSLES resuming play");

    SLVoices *vnode = NULL;
    SLVoices *tmp = NULL;
    int err = 0;

    HASH_ITER(hh, voices, vnode, tmp) {
        SLVoice *voice = vnode->voice;
        SLresult result = (*(voice->bqPlayerPlay))->SetPlayState(voice->bqPlayerPlay, SL_PLAYSTATE_PLAYING);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, Failed to resume source : %lu", result);
        }
    }

    return 0;
}

__attribute__((constructor(CTOR_PRIORITY_EARLY)))
static void _init_opensl(void) {
    LOG("Initializing OpenSLES sound system");

    assert(audio_backend == NULL && "there can only be one!");

    opensles_audio_backend.setup            = &opensles_systemSetup;
    opensles_audio_backend.shutdown         = &opensles_systemShutdown;
    opensles_audio_backend.pause            = &opensles_systemPause;
    opensles_audio_backend.resume           = &opensles_systemResume;

    audio_backend = &opensles_audio_backend;
}

