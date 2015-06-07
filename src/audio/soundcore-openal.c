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

// HACK NOTE: DO NOT REFACTOR (yet) ...
// OK this is more hackish than it needs to be because it's attempting to mimic a DirectSound backend ...Oh God, Why?...
// Here I must confess that because of general ignorance of the mockingboard and other soundcard code at this time,
// there is a need to track any changes/fixes implemented in AppleWin...

#include "common.h"
#include "audio/soundcore-openal.h"
#include "audio/alhelpers.h"

static long OpenALCreateSoundBuffer(ALBufferParamsStruct *params, ALSoundBufferStruct **soundbuf_struct, void *extra_data);
static long OpenALDestroySoundBuffer(ALSoundBufferStruct **soundbuf_struct);

typedef struct ALVoices {
    const ALuint source;
    ALVoice *voice;
    UT_hash_handle hh;
} ALVoices;

static ALVoices *voices = NULL;

// ----------------------------------------------------------------------------
// uthash of OpenAL buffers

static ALPlayBuf *PlaylistEnqueue(ALVoice *voice, ALuint bytes)
{
    ALPlayBuf *node = voice->avail_buffers;
    if (node == NULL)
    {
        ERRLOG("OOPS, sound playback overflow!");
        return NULL;
    }
    //LOG("Really enqueing OpenAL buffer %u", node->bufid);

    ALPlayBuf *node2 = NULL;
    HASH_FIND_INT(voice->queued_buffers, &node->bufid, node2);
    if (node2 != NULL)
    {
        ERRLOG("OOPS, confused ... ALPlayBuf already added!");
        return NULL;
    }

    // remove from list / place on hash
    voice->avail_buffers = node->_avail_next;
    node->_avail_next = NULL;
    HASH_ADD_INT(voice->queued_buffers, bufid, node);

    node->bytes = bytes;

    voice->_queued_total_bytes += node->bytes;
    voice->index = 0;
    assert(voice->_queued_total_bytes > 0);

#if 0
    ALPlayBuf *tmp = voice->queued_buffers;
    unsigned int count = HASH_COUNT(voice->queued_buffers);
    LOG("\t(numqueued: %d)", count);
    for (unsigned int i = 0; i < count; i++, tmp = tmp->hh.next)
    {
        LOG("\t(bufid : %u)", tmp->bufid);
    }
#endif

    return node;
}

static ALPlayBuf *PlaylistGet(ALVoice *voice, ALuint bufid)
{
    ALPlayBuf *node = NULL;
    HASH_FIND_INT(voice->queued_buffers, &bufid, node);
    return node;
}

static void PlaylistDequeue(ALVoice *voice, ALPlayBuf *node)
{
    //LOG("Dequeing OpenAL buffer %u", node->bufid);

    // remove from hash / place on list
    HASH_DEL(voice->queued_buffers, node);              // remove from hash
    node->_avail_next = voice->avail_buffers;
    voice->avail_buffers = node;

    voice->_queued_total_bytes -= node->bytes;
    assert(voice->_queued_total_bytes >= 0);
    node->bytes = 0;

#if 0
    ALPlayBuf *tmp = voice->queued_buffers;
    unsigned int count = HASH_COUNT(voice->queued_buffers);
    LOG("\t(numqueued: %d)", count);
    for (unsigned int i = 0; i < count; i++, tmp = tmp->hh.next)
    {
        LOG("\t(bufid : %u)", tmp->bufid);
    }
#endif
}

// ----------------------------------------------------------------------------

long SoundSystemCreate(const char *sound_device, SoundSystemStruct **sound_struct)
{
    assert(*sound_struct == NULL);
    assert(voices == NULL);

    ALCcontext *ctx = NULL;

    do {

        if ((ctx = InitAL()) == NULL)
        {
            ERRLOG("OOPS, OpenAL initialize failed");
            break;
        }

        if (alIsExtensionPresent("AL_SOFT_buffer_samples"))
        {
            LOG("AL_SOFT_buffer_samples supported, good!");
        }
        else
        {
            LOG("WARNING - AL_SOFT_buffer_samples extension not supported... Proceeding anyway...");
        }

        if ((*sound_struct = malloc(sizeof(SoundSystemStruct))) == NULL)
        {
            ERRLOG("OOPS, Not enough memory");
            break;
        }

        (*sound_struct)->implementation_specific = ctx;
        (*sound_struct)->CreateSoundBuffer = (int (*)(DSBUFFERDESC *, LPDIRECTSOUNDBUFFER *, void *))OpenALCreateSoundBuffer;
        (*sound_struct)->DestroySoundBuffer = (int (*)(LPDIRECTSOUNDBUFFER *))OpenALDestroySoundBuffer;

        return 0;
    } while(0);

    // ERRQUIT
    if (*sound_struct)
    {
        FREE(*sound_struct);
    }

    return -1;
}

long SoundSystemDestroy(SoundSystemStruct **sound_struct)
{
    // ugly assumption : this sets the extern g_lpDS ...
    assert(*sound_struct != NULL);

    ALCcontext *ctx = (ALCcontext*) (*sound_struct)->implementation_specific;
    assert(ctx != NULL);
    (*sound_struct)->implementation_specific = NULL;
    FREE(*sound_struct);

    CloseAL();

    return 0;
}

long SoundSystemEnumerate(char ***device_list, const int limit)
{
    assert(*device_list == NULL);
    *device_list = malloc(sizeof(char*)*2);
    (*device_list)[0] = strdup("unused-by-OpenAL");
    unsigned int num_devices = 1;
    (*device_list)[num_devices] = NULL; // sentinel
    return num_devices;
}

// pause all audio
long SoundSystemPause()
{
    ALVoices *vnode = NULL;
    ALVoices *tmp = NULL;
    int err = 0;

    HASH_ITER(hh, voices, vnode, tmp) {
        alSourcePause(vnode->source);
        err = alGetError();
        if (err != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Failed to pause source : 0x%08x", err);
        }
    }

    return 0;
}

long SoundSystemUnpause()
{
    ALVoices *vnode = NULL;
    ALVoices *tmp = NULL;
    int err = 0;

    HASH_ITER(hh, voices, vnode, tmp) {
        alSourcePlay(vnode->source);
        err = alGetError();
        if (err != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Failed to pause source : 0x%08x", err);
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------

/* Destroys a voice object, deleting the source and buffers. No error handling
 * since these calls shouldn't fail with a properly-made voice object. */
static void DeleteVoice(ALVoice *voice)
{
    alDeleteSources(1, &voice->source);
    if (alGetError() != AL_NO_ERROR)
    {
        ERRLOG("OOPS, Failed to delete source");
    }

    if (voice->data)
    {
        FREE(voice->data);
    }

    ALPlayBuf *node = NULL;
    ALPlayBuf *tmp = NULL;
    HASH_ITER(hh, voice->queued_buffers, node, tmp) {
        PlaylistDequeue(voice, node);
    }

    while (voice->avail_buffers)
    {
        node = voice->avail_buffers;
        alDeleteBuffers(1, &node->bufid);
        if (alGetError() != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Failed to delete object IDs");
        }
        voice->avail_buffers = node->_avail_next;
        FREE(node);
    }

    memset(voice, 0, sizeof(*voice));
    FREE(voice);
}

/* Creates a new voice object, and allocates the needed OpenAL source and
 * buffer objects. Error checking is simplified for the purposes of this
 * example, and will cause an abort if needed. */
static ALVoice *NewVoice(ALBufferParamsStruct *params)
{
    ALVoice *voice = NULL;

    do {
        voice = calloc(1, sizeof(*voice));
        if (voice == NULL)
        {
            ERRLOG("OOPS, Out of memory!");
            break;
        }

        ALuint buffers[OPENAL_NUM_BUFFERS];
        alGenBuffers(OPENAL_NUM_BUFFERS, buffers);
        if (alGetError() != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Could not create buffers");
            break;
        }

        alGenSources(1, &voice->source);
        if (alGetError() != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Could not create source");
            break;
        }

        // Set parameters so mono sources play out the front-center speaker and won't distance attenuate.
        alSource3i(voice->source, AL_POSITION, 0, 0, -1);
        if (alGetError() != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Could not set AL_POSITION source parameter");
            break;
        }
        alSourcei(voice->source, AL_SOURCE_RELATIVE, AL_TRUE);
        if (alGetError() != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Could not set AL_SOURCE_RELATIVE source parameter");
            break;
        }
        alSourcei(voice->source, AL_ROLLOFF_FACTOR, 0);
        if (alGetError() != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Could not set AL_ROLLOFF_FACTOR source parameter");
            break;
        }

#if 0
        alSourcei(voice->source, AL_STREAMING, AL_TRUE);
        if (alGetError() != AL_NO_ERROR)
        {
            ERRLOG("OOPS, Could not set AL_STREAMING source parameter");
            break;
        }
#endif

        voice->avail_buffers = NULL;
        for (unsigned int i=0; i<OPENAL_NUM_BUFFERS; i++)
        {
            ALPlayBuf immutableNode = { .bufid = buffers[i] };
            ALPlayBuf *node = calloc(1, sizeof(ALPlayBuf));
            if (!node)
            {
                ERRLOG("OOPS, Not enough memory");
                break;
            }
            memcpy(node, &immutableNode, sizeof(ALPlayBuf));
            node->_avail_next = voice->avail_buffers;
            voice->avail_buffers = node;
        }

        voice->rate = (ALuint)params->lpwfxFormat->nSamplesPerSec;

        // Emulator supports only mono and stereo 
        if (params->lpwfxFormat->nChannels == 2)
        {
            voice->format = AL_FORMAT_STEREO16;
        }
        else
        {
            voice->format = AL_FORMAT_MONO16;
        }

        /* Allocate enough space for the temp buffer, given the format */
        voice->buffersize = (ALsizei)params->dwBufferBytes;
        voice->data = malloc(voice->buffersize);
        if (voice->data == NULL)
        {
            ERRLOG("OOPS, Error allocating %d bytes", voice->buffersize);
            break;
        }

        LOG("\tRate     : 0x%08x", voice->rate);
        LOG("\tFormat   : 0x%08x", voice->format);
        LOG("\tbuffersize : %d", voice->buffersize);

        return voice;

    } while(0);

    // ERR
    if (voice)
    {
        DeleteVoice(voice);
    }

    return NULL;
}

// ----------------------------------------------------------------------------

static long ALGetVolume(void *_this, long *volume)
{
    LOG("ALGetVolume ...");
    if (volume)
    {
        *volume = 0;
    }
    return 0;
}

static long ALSetVolume(void *_this, long volume)
{
    LOG("ALSetVolume ...");
    return 0;
}

static long ALStop(void *_this)
{
    LOG("ALStop ...");
    return 0;
}

static long ALRestore(void *_this)
{
    LOG("ALRestore ...");
    return 0;
}

static long ALPlay(void *_this, unsigned long reserved1, unsigned long reserved2, unsigned long flags)
{
    LOG("ALPlay ...");

    return 0;
}

static int _ALProcessPlayBuffers(ALVoice *voice, ALuint *bytes_queued)
{
    ALint processed = 0;
    int err = 0;
    *bytes_queued = 0;

    alGetSourcei(voice->source, AL_BUFFERS_PROCESSED, &processed);
    if ((err = alGetError()) != AL_NO_ERROR)
    {
        ERRLOG("OOPS, error in checking processed buffers : 0x%08x", err);
        return err;
    }

    if ((processed == 0) && (HASH_COUNT(voice->queued_buffers) >= OPENAL_NUM_BUFFERS))
    {
        //LOG("All audio buffers processing...");
    }

    while (processed > 0)
    {
        --processed;
        ALuint bufid = 0;
        alSourceUnqueueBuffers(voice->source, 1, &bufid);
        if ((err = alGetError()) != AL_NO_ERROR)
        {
            ERRLOG("OOPS, OpenAL error dequeuing buffer : 0x%08x", err);
            return err;
        }

        //LOG("Attempting to dequeue %u ...", bufid);
        ALPlayBuf *node = PlaylistGet(voice, bufid);
        if (!node)
        {
            ERRLOG("OOPS, OpenAL bufid %u not found in playlist...", bufid);
            ALPlayBuf *tmp = voice->queued_buffers;
            unsigned int count = HASH_COUNT(voice->queued_buffers);
            LOG("\t(numqueued: %d)", count);
            for (unsigned int i = 0; i < count; i++, tmp = tmp->hh.next)
            {
                LOG("\t(bufid : %u)", tmp->bufid);
            }
            continue;
        }

        PlaylistDequeue(voice, node);
    }

    ALint play_offset = 0;
    alGetSourcei(voice->source, AL_BYTE_OFFSET, &play_offset);
    if ((err = alGetError()) != AL_NO_ERROR)
    {
        ERRLOG("OOPS, alGetSourcei AL_BYTE_OFFSET : 0x%08x", err);
        return err;
    }
    assert((play_offset >= 0)/* && (play_offset < voice->buffersize)*/);

    long q = voice->_queued_total_bytes/* + voice->index*/ - play_offset;

    if (q >= 0) {
        *bytes_queued = (ALuint)q;
    }

    return 0;
}

// returns queued+working sound buffer size in bytes 
static long ALGetPosition(void *_this, unsigned long *bytes_queued, unsigned long *unused_write_cursor)
{
    ALVoice *voice = (ALVoice*)_this;
    *bytes_queued = 0;
    if (unused_write_cursor)
    {
        *unused_write_cursor = 0;
    }

    ALuint queued = 0;
    int err = _ALProcessPlayBuffers(voice, &queued);
    if (err)
    {
        return err;
    }
    static int last_queued = 0;
    if (queued != last_queued)
    {
        last_queued = queued;
        //LOG("OpenAL bytes queued : %u", queued);
    }

    *bytes_queued = queued + voice->index;

    return 0;
}

// DS->Lock()
static long ALBegin(void *_this, unsigned long unused, unsigned long write_bytes, void **audio_ptr1, unsigned long *audio_bytes1, void **unused_audio_ptr2, unsigned long *unused_audio_bytes2, unsigned long flags_unused)
{
    ALVoice *voice = (ALVoice*)_this;

    if (unused_audio_ptr2)
    {
        *unused_audio_ptr2 = NULL;
    }

    if (write_bytes == 0)
    {
        write_bytes = voice->buffersize;
    }

    ALuint bytes_queued = 0;
    int err = _ALProcessPlayBuffers(voice, &bytes_queued);
    if (err)
    {
        return err;
    }

    if ((bytes_queued == 0) && (voice->index == 0))
    {
        LOG("Buffer underrun ... queuing quiet samples ...");
        int quiet_size = voice->buffersize>>2/* 1/4 buffer */;
        memset(voice->data, 0x0, quiet_size);
        voice->index += quiet_size;
    }
    else if (bytes_queued + voice->index < (voice->buffersize>>3)/* 1/8 buffer */)
    {
        LOG("Potential underrun ...");
    }

    ALsizei remaining = voice->buffersize - voice->index;
    if (write_bytes > remaining)
    {
        write_bytes = remaining;
    }

    *audio_ptr1 = voice->data+voice->index;
    *audio_bytes1 = write_bytes;

    return 0;
}

static int _ALSubmitBufferToOpenAL(ALVoice *voice)
{
    int err = 0;

    ALPlayBuf *node = PlaylistEnqueue(voice, voice->index);
    if (!node)
    {
        return -1;
    }
    //LOG("Enqueing OpenAL buffer %u (%u bytes)", node->bufid, node->bytes);
    alBufferData(node->bufid, voice->format, voice->data, node->bytes, voice->rate);

    if ((err = alGetError()) != AL_NO_ERROR)
    {
        PlaylistDequeue(voice, node);
        ERRLOG("OOPS, Error alBufferData : 0x%08x", err);
        return err;
    }

    alSourceQueueBuffers(voice->source, 1, &node->bufid);
    if ((err = alGetError()) != AL_NO_ERROR)
    {
        PlaylistDequeue(voice, node);
        ERRLOG("OOPS, Error buffering data : 0x%08x", err);
        return err;
    }

    ALint state = 0;
    alGetSourcei(voice->source, AL_SOURCE_STATE, &state);
    if ((err = alGetError()) != AL_NO_ERROR)
    {
        ERRLOG("OOPS, Error checking source state : 0x%08x", err);
        return err;
    }
    if ((state != AL_PLAYING) && (state != AL_PAUSED))
    {
        // 2013/11/17 NOTE : alSourcePlay() is expensive and causes audio artifacts, only invoke if needed
        LOG("Restarting playback (was 0x%08x) ...", state);
        alSourcePlay(voice->source);
        if ((err = alGetError()) != AL_NO_ERROR)
        {
            LOG("Error starting playback : 0x%08x", err);
            return err;
        }
    }

    return 0;
}

// DS->Unlock()
static long ALCommit(void *_this, void *unused_audio_ptr1, unsigned long audio_bytes1, void *unused_audio_ptr2, unsigned long unused_audio_bytes2)
{
    ALVoice *voice = (ALVoice*)_this;
    int err = 0;

    ALuint bytes_queued = 0;
    err = _ALProcessPlayBuffers(voice, &bytes_queued);
    if (err)
    {
        return err;
    }

    voice->index += audio_bytes1;

    while (voice->index > voice->buffersize)
    {
        // hopefully this is DEADC0DE or we've overwritten voice->data buffer ...
        ERRLOG("OOPS, overflow in queued sound data");
        assert(false);
    }

    if (bytes_queued >= (voice->buffersize>>2)/*quarter buffersize*/)
    {
        // keep accumulating data into working buffer
        return 0;
    }

    if (HASH_COUNT(voice->queued_buffers) >= (OPENAL_NUM_BUFFERS))
    {
        //LOG("no free audio buffers"); // keep accumulating ...
        return 0;
    }

    // ---------------------------
    // Submit working buffer to OpenAL

    err = _ALSubmitBufferToOpenAL(voice);
    if (err)
    {
        return err;
    }

    return 0;
}

// HACK Part I : done once for mockingboard that has semiauto repeating phonemes ...
static long ALCommitStaticBuffer(void *_this, unsigned long audio_bytes1)
{
    ALVoice *voice = (ALVoice*)_this;
    voice->replay_index = (ALsizei)audio_bytes1;
    return 0;
}

// HACK Part II : replay mockingboard phoneme ...
static long ALReplay(void *_this)
{
    ALVoice *voice = (ALVoice*)_this;
    voice->index = voice->replay_index;

    int err = 0;

    err = _ALSubmitBufferToOpenAL(voice);
    if (err)
    {
        return err;
    }

    return 0;
}

static long ALGetStatus(void *_this, unsigned long *status)
{
    ALVoice* voice = (ALVoice*)_this;

    int err = 0;
    ALint state = 0;
    alGetSourcei(voice->source, AL_SOURCE_STATE, &state);
    if ((err = alGetError()) != AL_NO_ERROR)
    {
        ERRLOG("OOPS, Error checking source state : 0x%08x", err);
        return err;
    }

    if ((state == AL_PLAYING) || (state == AL_PAUSED))
    {
        *status = DSBSTATUS_PLAYING;
    }
    else
    {
        *status = _DSBSTATUS_NOTPLAYING;
    }

    return 0;
}

static long OpenALCreateSoundBuffer(ALBufferParamsStruct *params, ALSoundBufferStruct **soundbuf_struct, void *extra_data)
{
    LOG("OpenALCreateSoundBuffer ...");
    assert(*soundbuf_struct == NULL);

    const SoundSystemStruct *sound_struct = (SoundSystemStruct*)extra_data;
    ALCcontext *ctx = (ALCcontext*)(sound_struct->implementation_specific);
    assert(ctx != NULL);

    ALVoice *voice = NULL;

    do {

        if ((voice = NewVoice(params)) == NULL)
        {
            ERRLOG("OOPS, Cannot create new voice");
            break;
        }

        ALVoices immutableNode = { .source = voice->source };
        ALVoices *vnode = calloc(1, sizeof(ALVoices));
        if (!vnode)
        {
            ERRLOG("OOPS, Not enough memory");
            break;
        }
        memcpy(vnode, &immutableNode, sizeof(ALVoices));
        vnode->voice = voice;
        HASH_ADD_INT(voices, source, vnode);

        if ((*soundbuf_struct = malloc(sizeof(ALSoundBufferStruct))) == NULL)
        {
            ERRLOG("OOPS, Not enough memory");
            break;
        }

        (*soundbuf_struct)->_this = voice;
        (*soundbuf_struct)->SetVolume          = (int (*)(void *, long))ALSetVolume;
        (*soundbuf_struct)->GetVolume          = (int (*)(void *, LPLONG))ALGetVolume;
        (*soundbuf_struct)->GetCurrentPosition = (int (*)(void *, LPDWORD, LPDWORD))ALGetPosition;
        (*soundbuf_struct)->Stop               = (int (*)(void *))ALStop;
        (*soundbuf_struct)->Restore            = (int (*)(void *))ALRestore;
        (*soundbuf_struct)->Play               = (int (*)(void *, unsigned long, unsigned long, unsigned long))ALPlay;
        (*soundbuf_struct)->Lock               = (int (*)(void *, unsigned long, unsigned long, LPVOID *, LPDWORD, LPVOID *, LPDWORD, unsigned long))ALBegin;
        (*soundbuf_struct)->Unlock             = (int (*)(void *, LPVOID, unsigned long, LPVOID, unsigned long))ALCommit;
        (*soundbuf_struct)->GetStatus          = (int (*)(void *, LPDWORD))ALGetStatus;

        // mockingboard-specific hacks
        (*soundbuf_struct)->UnlockStaticBuffer = (int (*)(void *, unsigned long))ALCommitStaticBuffer;
        (*soundbuf_struct)->Replay             = (int (*)(void *))ALReplay;

        return 0;
    } while(0);

    if (*soundbuf_struct)
    {
        OpenALDestroySoundBuffer(soundbuf_struct);
    }
    else if (voice)
    {
        DeleteVoice(voice);
    }

    return -1;
}

static long OpenALDestroySoundBuffer(ALSoundBufferStruct **soundbuf_struct)
{
    if (!*soundbuf_struct) {
        // already dealloced
        return 0;
    }
    LOG("OpenALDestroySoundBuffer ...");
    ALVoice *voice = (*soundbuf_struct)->_this;
    ALint source = voice->source;

    DeleteVoice(voice);

    ALVoices *vnode = NULL;
    HASH_FIND_INT(voices, &source, vnode);
    if (vnode) {
        HASH_DEL(voices, vnode);
        FREE(vnode);
    }

    FREE(*soundbuf_struct);
    return 0;
}

