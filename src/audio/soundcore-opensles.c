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

#include "playqueue.h"
#include "uthash.h"

#define DEBUG_OPENSL 0
#if DEBUG_OPENSL
#   define OPENSL_LOG(...) LOG(__VA_ARGS__)
#else
#   define OPENSL_LOG(...)
#endif

#define OPENSL_NUM_BUFFERS 4

typedef struct SLVoice {
    unsigned int voiceId;
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerPlay;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLMuteSoloItf bqPlayerMuteSolo;
    SLVolumeItf bqPlayerVolume;

    // OpenSLES buffer queue management
    PlayQueue_s *playq;
    pthread_mutex_t bqThreadLock;
    long currentNodeId;
    unsigned long currentNumBytes;
    SLmillisecond startingPosition;
    SLmillisecond currentBufferDuration;
    long queuedTotalBytes; // a maximum estimate -- actual value depends on query
    bool bufferIsPlaying;

    // working data buffer
    uint8_t *data;
    size_t index;      // working buffer byte index
    size_t buffersize; // working buffer size

    size_t replay_index;

    // misc
    uint16_t nChannels;
    unsigned long nSamplesPerSec;
} SLVoice;

typedef struct SLVoices {
    unsigned int voiceId;
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
// AudioBuffer_s processing routines

static long _SLGetCurrentQueuedBytes(SLVoice *voice, unsigned int *bytes_queued) {

    *bytes_queued = 0;
    long err = 0;
    do {

        SLmillisecond position = 0;
        long play_offset = 0;

        // ------------------------------ LOCK
        pthread_mutex_lock(&(voice->bqThreadLock));

        SLresult result = (*(voice->bqPlayerPlay))->GetPosition(voice->bqPlayerPlay, &position);
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("Could not get position of current sample");
        }

        assert(position >= 0);
        assert(voice->startingPosition >= 0);

        SLmillisecond positionInCurrentBuffer = 0;
        if (position < voice->startingPosition) {
            OPENSL_LOG("OpenSLES wrapping position!");
        } else {
            positionInCurrentBuffer = position - voice->startingPosition;
        }

        if (voice->currentBufferDuration) {
            float scale = positionInCurrentBuffer/(float)(voice->currentBufferDuration);
            if (scale > 1.f) {
                //OPENSL_LOG("OOPS scale > 1.f!");
                play_offset = voice->currentNumBytes;
            } else {
                play_offset = (long)(voice->currentNumBytes * scale);
            }

            //OPENSL_LOG("totalQueuedBytes:%ld currentNumBytes:%ld startingPosition:%ld position:%ld inCurrentBuff:%ld scale:%f (play_offset:%ld)", voice->queuedTotalBytes, voice->currentNumBytes, voice->startingPosition, position, positionInCurrentBuffer, scale, play_offset);
        } else {
            //OPENSL_LOG("totalQueuedBytes:%ld currentNumBytes:%ld startingPosition:%ld position:%ld inCurrentBuff:%ld (play_offset:%ld)", voice->queuedTotalBytes, voice->currentNumBytes, voice->startingPosition, position, positionInCurrentBuffer, play_offset);
        }
        long q = voice->queuedTotalBytes - play_offset;

        pthread_mutex_unlock(&(voice->bqThreadLock));
        // ---------------------------- UNLOCK

        if (q > 0) {
            *bytes_queued = q;
        }
    } while (0);

    return err;
}

// returns queued+working sound buffer size in bytes
static long SLGetPosition(AudioBuffer_s *_this, OUTPARM unsigned long *bytes_queued) {
    *bytes_queued = 0;
    long err = 0;

    do {
        SLVoice *voice = (SLVoice*)_this->_internal;

        unsigned int queued = 0;
        long err = _SLGetCurrentQueuedBytes(voice, &queued);
        if (err) {
            break;
        }

        *bytes_queued = queued + voice->index;
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
            write_bytes = voice->buffersize;
        }

        unsigned int bytes_queued = 0;
        err = _SLGetCurrentQueuedBytes(voice, &bytes_queued);
        if (err) {
            break;
        }

        if ((bytes_queued == 0) && (voice->index == 0)) {
            LOG("Buffer underrun ... queuing quiet samples ...");
            int quiet_size = voice->buffersize>>2/* 1/4 buffer */;
            memset(voice->data, 0x0, quiet_size);
            voice->index += quiet_size;
        }
#if 0
        else if (bytes_queued + voice->index < (voice->buffersize>>3)/* 1/8 buffer */)
        {
            LOG("Potential underrun ...");
        }
#endif

        unsigned int remaining = voice->buffersize - voice->index;
        if (write_bytes > remaining) {
            write_bytes = remaining;
        }

        *audio_ptr = (int16_t *)(voice->data+voice->index);
        *audio_bytes = write_bytes;
    } while (0);

    return err;
}

static SLresult _send_buffer_to_opensles(SLVoice *voice, PlayNode_s *node) {

    voice->currentNodeId = node->nodeId;
    voice->currentNumBytes = node->numBytes;

    // calculate the new buffer duration
    unsigned long numSamples = (node->numBytes>>1);
    if (voice->nChannels == 2) {
        unsigned long numSamplesStereo = numSamples;
        numSamples = (numSamplesStereo>>1);
    } else if (voice->nChannels !=1) {
        assert(false && "only mono or stereo supported");
    }
    voice->currentBufferDuration = ( numSamples / (voice->nSamplesPerSec/1000.f) );

    OPENSL_LOG("Enqueing OpenSL buffer %ld (%lu bytes, %lu millis)", node->nodeId, node->numBytes, voice->currentBufferDuration, node->bytes);

    // enqueue buffer to OpenSLES
    SLresult result = (*(voice->bqPlayerBufferQueue))->Enqueue(voice->bqPlayerBufferQueue, node->bytes, node->numBytes);
    if (result != SL_RESULT_SUCCESS) {
        ERRLOG("OOPS ... buffer queue callback enqueue reports %ld", result); // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT
    }

    return result;
}

static long _SLSubmitBuffer(SLVoice *voice) {
    SLresult err = 0;

    do {
        // Micro-manage play queue locally to understand the total bytes-in-play
        PlayNode_s playNode = {
            .nodeId = INVALID_NODE_ID,
            .numBytes = voice->index,
            .bytes = (uint8_t *)(voice->data),
        };

        //OPENSL_LOG("_SLSubmitBuffer : %ld bytes", voice->index);

        bool isCurrentlyPlaying = false;

        // ------------------------------ LOCK
        pthread_mutex_lock(&(voice->bqThreadLock));

        err = voice->playq->Enqueue(voice->playq, &playNode);
        if (err) {
            pthread_mutex_unlock(&(voice->bqThreadLock));
            break;
        }

        voice->queuedTotalBytes += voice->index;
        voice->index = 0;
        assert(voice->queuedTotalBytes > 0);

        isCurrentlyPlaying = voice->bufferIsPlaying;

        err = SL_RESULT_UNKNOWN_ERROR;
        if (!isCurrentlyPlaying) {
            err = _send_buffer_to_opensles(voice, &playNode);
            if (err != SL_RESULT_SUCCESS) {
                ERRLOG("OOPS ... buffer queue callback enqueue reports %ld", err); // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT
                pthread_mutex_unlock(&(voice->bqThreadLock));
                break;
            }
        }

        voice->bufferIsPlaying = true;

        pthread_mutex_unlock(&(voice->bqThreadLock));
        // ---------------------------- UNLOCK

        SLuint32 state = 0;
        err = (*(voice->bqPlayerPlay))->GetPlayState(voice->bqPlayerPlay, &state);
        if (err != SL_RESULT_SUCCESS) {
            ERRLOG("OOPS, could not get source state : %lu", err);
            break;
        }

        if ((state != SL_PLAYSTATE_PLAYING) && (state != SL_PLAYSTATE_PAUSED)) {
            LOG("Restarting playback (was %lu) ...", state);

            err = (*(voice->bqPlayerPlay))->SetPlayState(voice->bqPlayerPlay, SL_PLAYSTATE_PLAYING);
            if (err != SL_RESULT_SUCCESS) {
                ERRLOG("OOPS, Failed to pause source : %lu", err);
                break;
            }
        }

    } while (0);

    return err;
}

static long SLUnlockBuffer(AudioBuffer_s *_this, unsigned long audio_bytes) {
    long err = 0;

    do {
        SLVoice *voice = (SLVoice*)_this->_internal;

        unsigned int bytes_queued = 0;
        err = _SLGetCurrentQueuedBytes(voice, &bytes_queued);
        if (err) {
            break;
        }

        voice->index += audio_bytes;

        assert((voice->index < voice->buffersize) && "OOPS, overflow in queued sound data");

        if (bytes_queued >= (voice->buffersize>>2)/*quarter buffersize*/) {
            // keep accumulating data into working buffer
            //OPENSL_LOG("accumulating more data %lu -> (queued:%u/buffersize:%u) prequeued:%u", audio_bytes, bytes_queued, voice->buffersize, bytes_queued+voice->index);
            break;
        } else {
            //OPENSL_LOG("possibly submit %lu -> (queued:%u/buffersize:%u)", audio_bytes, bytes_queued, voice->buffersize);
        }

        if (! (voice->playq->CanEnqueue(voice->playq)) ) {
            //LOG("no free audio buffers"); // keep accumulating ...
            break;
        }

        // Submit working buffer

        err = _SLSubmitBuffer(voice);
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
    voice->index = voice->replay_index;
    long err = _SLSubmitBuffer(voice);
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

// this callback handler is called every time a buffer finishes playing
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    SLVoice *voice = (SLVoice *)context;

    SLresult result = SL_RESULT_UNKNOWN_ERROR;

    assert(voice != NULL);
    assert(bq == voice->bqPlayerBufferQueue);
    assert(pthread_self() != cpu_thread_id);

    // ------------------------------ LOCK
    pthread_mutex_lock(&(voice->bqThreadLock));

    do {
        // reset starting position to current best estimate
        result = (*(voice->bqPlayerPlay))->GetPosition(voice->bqPlayerPlay, &(voice->startingPosition));
        if (result != SL_RESULT_SUCCESS) {
            ERRLOG("Could not get position of current sample");
            voice->startingPosition = 0;
        }

        // dequeue finished buffer and reset in-play stats
        PlayNode_s head = { 0 };
        voice->playq->Dequeue(voice->playq, &head);
        assert(voice->currentNodeId == head.nodeId);

        voice->queuedTotalBytes -= head.numBytes;
        voice->currentNodeId = -1;
        voice->currentNumBytes = 0;
        voice->currentBufferDuration = 0;

        // get current queue head
        long err = voice->playq->GetHead(voice->playq, &head);
        if (err) {
            RELEASE_ERRLOG("Could not get head and size of queue!");
            voice->bufferIsPlaying = false;
            break;
        }

        result = _send_buffer_to_opensles(voice, &head);
        if (result != SL_RESULT_SUCCESS) {
            RELEASE_ERRLOG("Could not submit buffer to OpenSLES!");
            voice->bufferIsPlaying = false;
            break;
        }
    } while (0);

    pthread_mutex_unlock(&(voice->bqThreadLock));
    // ---------------------------- UNLOCK
}

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

    if (voice->data) {
        FREE(voice->data);
    }

    playq_destroyPlayQueue(&(voice->playq));

    memset(voice, 0, sizeof(*voice));
    FREE(voice);
}

static SLVoice *_opensl_createVoice(const AudioParams_s *params, const EngineContext_s *ctx) {
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

        long longBuffers[OPENSL_NUM_BUFFERS];
        for (unsigned int i=0; i<OPENSL_NUM_BUFFERS; i++) {
            longBuffers[i] = i+1;
        }
        voice->playq = playq_createPlayQueue(longBuffers, OPENSL_NUM_BUFFERS);
        if (!voice->playq) {
            ERRLOG("OOPS, Not enough memory for PlayQueue");
            break;
        }

        assert(params->nSamplesPerSec == SPKR_SAMPLE_RATE);
        assert(params->nChannels == 1 || params->nChannels == 2);

        voice->nChannels = params->nChannels;
        voice->nSamplesPerSec = params->nSamplesPerSec;
        SLuint32 channelMask = 0;
        if (voice->nChannels == 2) {
            channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
        } else {
            channelMask = SL_SPEAKER_FRONT_CENTER;
        }

        // Allocate enough space for the temp buffer
        voice->buffersize = params->dwBufferBytes;
        voice->data = malloc(voice->buffersize);
        if (voice->data == NULL) {
            ERRLOG("OOPS, Error allocating %d bytes", voice->buffersize);
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
            .numChannels = params->nChannels,
            .samplesPerSec = SL_SAMPLINGRATE_22_05,
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
            //params->nChannels == 1 ? SL_BOOLEAN_FALSE : SL_BOOLEAN_TRUE,
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

        return voice;

    } while(0);

    // ERR
    if (voice) {
        _opensl_destroyVoice(voice);
    }

    return NULL;
}

// ----------------------------------------------------------------------------

static long opensl_destroySoundBuffer(INOUT AudioBuffer_s **soundbuf_struct) {
    if (!*soundbuf_struct) {
        return 0;
    }

    LOG("opensl_destroySoundBuffer ...");
    SLVoice *voice = (SLVoice *)((*soundbuf_struct)->_internal);
    unsigned int voiceId = voice->voiceId;

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

static long opensl_createSoundBuffer(const AudioParams_s *params, INOUT AudioBuffer_s **soundbuf_struct, const AudioContext_s *audio_context) {
    LOG("opensl_createSoundBuffer ...");
    assert(*soundbuf_struct == NULL);

    SLVoice *voice = NULL;

    do {

        EngineContext_s *ctx = (EngineContext_s *)(audio_context->_internal);
        assert(ctx != NULL);

        if ((voice = _opensl_createVoice(params, ctx)) == NULL)
        {
            ERRLOG("OOPS, Cannot create new voice");
            break;
        }

        SLVoices *vnode = calloc(1, sizeof(SLVoices));
        if (!vnode) {
            ERRLOG("OOPS, Not enough memory");
            break;
        }

        static unsigned int counter = 0;
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
        opensl_destroySoundBuffer(soundbuf_struct);
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

