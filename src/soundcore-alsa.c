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


#include "soundcore-alsa.h"

// HACK : this is an ugly shoehorning into DirectSound assumptions...  Needta rework this once I have a better
// understanding of the workings of the sound system :)

static long alsa_create_sound_buffer(ALSABufferParamsStruct *params_struct, ALSASoundBufferStruct **soundbuf_struct, void *extra_data);
static long alsa_destroy_sound_buffer(ALSASoundBufferStruct **soundbuf_struct);

long SoundSystemCreate(const char *sound_device, SoundSystemStruct **sound_struct)
{
    assert(*sound_struct == NULL);

    snd_pcm_t *handle = NULL;
    int err = -1;

    do {
        if ((*sound_struct = malloc(sizeof(SoundSystemStruct))) == NULL)
        {
            ERRLOG("Not enough memory");
            break;
        }

        if ((err = snd_pcm_open(&handle, sound_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        {
            ERRLOG("Playback open error: %s", snd_strerror(err));
            break;
        }

        (*sound_struct)->implementation_specific = handle;
        (*sound_struct)->CreateSoundBuffer = alsa_create_sound_buffer;
        (*sound_struct)->DestroySoundBuffer = alsa_destroy_sound_buffer;

        return 0;
    } while(0);

    // ERRQUIT
    if (*sound_struct)
    {
        Free(*sound_struct);
    }

    return -1;
}

long SoundSystemDestroy(SoundSystemStruct **sound_struct)
{
    // ugly assumption : this sets the extern g_lpDS ...
    assert(*sound_struct != NULL);

    int err = 0;
    snd_pcm_t *handle = (snd_pcm_t*) (*sound_struct)->implementation_specific;
    assert(handle != NULL);

    if ((err = snd_pcm_drop(handle)) < 0)
    {
        ERRLOG("snd_pcm_drop: %s", snd_strerror(err));
    }
    if ((err = snd_pcm_close(handle))< 0)
    {
        ERRLOG("snd_pcm_drop: %s", snd_strerror(err));
    }

    Free(*sound_struct);

    return 0;
}

long SoundSystemEnumerate(char ***device_list, const int limit)
{
    assert(*device_list == NULL);

    // HACK NOTE : it appears that "plughw:0,0" is the magic Linux device ...
    *device_list = malloc(sizeof(char*)*2);
    (*device_list)[0] = strdup("plughw:0,0");
    unsigned int num_devices = 1;

#if 0

    char **hints = NULL;
    int err = -1;

    if ((err = snd_device_name_hint(-1, "pcm", (void***)&hints)) < 0)
    {
        ERRLOG("snd_device_name_hint: %s", snd_strerror(err));
        return err;
    }

    *device_list = malloc(sizeof(char*) * (limit+1));
    if (!*device_list)
    {
        ERRLOG("Not enough memory");
        return -1;
    }

    char** p = hints;
    unsigned int num_devices = 0;
    while (*p != NULL)
    {
        char *name = snd_device_name_get_hint(*p, "NAME");
        LOG("PCM device : %s", name);

        if ((name != NULL) && (0 != strcmp("null", name)))
        {
            if (num_devices < limit)
            {
                (*device_list)[num_devices++] = name;
            }
            else
            {
                LOG("Ignoring device : %s", name);
                Free(name);
            }
        }

        ++p;
    }

    snd_device_name_free_hint((void**)hints);
#endif

    (*device_list)[num_devices] = NULL; // sentinel
    return num_devices;
}

// ----------------------------------------------------------------------------

static long _alsa_create_volume_refs(snd_mixer_t **handle, snd_mixer_selem_id_t **sid, snd_mixer_elem_t **elem)
{
    // TODO http://stackoverflow.com/questions/6787318/set-alsa-master-volume-from-c-code#6787957
    // TODO NOTE that I had to add a free -- snd_mixer_selem_id_free()

    // TODO : iterate over mixers?
    static const char *card = "default";
    static const char *selem_name = "Master";

    int err = 0;
    do {
        if ((err = snd_mixer_open(handle, 0)) < 0)
        {
            ERRLOG("Error opening mixer: %s", snd_strerror(err));
            break;
        }

        if ((err = snd_mixer_attach(*handle, card)) < 0)
        {
            ERRLOG("Error attaching to mixer: %s", snd_strerror(err));
            break;
        }

        if ((err = snd_mixer_selem_register(*handle, NULL, NULL)) < 0)
        {
            ERRLOG("Error mixer register: %s", snd_strerror(err));
            break;
        }

        if ((err = snd_mixer_load(*handle)) < 0)
        {
            ERRLOG("Error mixer load: %s", snd_strerror(err));
            break;
        }

        if ((err = snd_mixer_selem_id_malloc(sid)) < 0)
        {
            ERRLOG("Error mixer register: %s", snd_strerror(err));
            break;
        }

        snd_mixer_selem_id_set_index(*sid, 0);
        snd_mixer_selem_id_set_name(*sid, selem_name);

        if ((*elem = snd_mixer_find_selem(*handle, *sid)) == NULL)
        {
            ERRLOG("Error mixer find volume: %s", snd_strerror(err));
            break;
        }

        return 0;
    } while (0);

    if (*handle) { snd_mixer_close(*handle); }
    if (*sid) { snd_mixer_selem_id_free(*sid); }
    return err ?: -1;
}

static long alsa_get_volume(void *_this, long *volume)
{
    assert(volume != NULL);

    snd_mixer_t *handle = NULL;
    snd_mixer_selem_id_t *sid = NULL;
    snd_mixer_elem_t *elem = NULL;
    long min = 0;
    long max = 0;

    int err = 0;
    do {
        if ((err = _alsa_create_volume_refs(&handle, &sid, &elem)) < 0)
        {
            break;
        }

        if ((err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max)) < 0)
        {
            ERRLOG("Error mixer get playback volume range: %s", snd_strerror(err));
            break;
        }

        if ((err = snd_mixer_selem_set_playback_volume_all(elem, *volume * max / 100)) < 0)
        {
            ERRLOG("Error setting playback volume: %s", snd_strerror(err));
            break;
        }
        
        return 0;
    } while (0);

    if (handle) { snd_mixer_close(handle); }
    if (sid) { snd_mixer_selem_id_free(sid); }
    return err;
}

static long alsa_set_volume(void *_this, long volume)
{
    // UNIMPLEMENTED
    return 0;
}

/*
 *   Underrun and suspend recovery
 */
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (err == -EPIPE)
    {
        ERRLOG("stream recovery from under-run...");
        err = snd_pcm_prepare(handle);
        if (err < 0)
        {
            ERRLOG("Can't recover from underrun, prepare failed: %s", snd_strerror(err));
        }

        return 0;
    }
    else if (err == -ESTRPIPE)
    {
        ERRLOG("stream recovery from SUSPENDED...");
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
        {
            struct timespec dt = { .tv_nsec=1, .tv_sec=0 };
            nanosleep(&dt, NULL); // wait until the suspend flag is released
        }

        if (err < 0)
        {
            err = snd_pcm_prepare(handle);
            if (err < 0)
            {
                ERRLOG("Can't recover from suspend, prepare failed: %s", snd_strerror(err));
            }
        }

        return 0;
    }
    else
    {
        ERRLOG("stream recovery from %d ?", err);
    }

    return err;
}

static long alsa_stop(void *_this)
{
    // this is a no-op at the moment, just let the sound flow!
    return 0;
}

static long alsa_restore(void *_this)
{
    return 0;
}

static long alsa_play(void *_this, unsigned long reserved1, unsigned long reserved2, unsigned long flags)
{
    // this is a no-op presumably because all the alsa setup give us a buffer ready to play
    return 0;
}

// returns buffer position in bytes
static long alsa_get_position(void *_this, unsigned long *play_cursor, unsigned long *unused_write_cursor)
{
    ALSAExtras *extras = (ALSAExtras*)_this;
    snd_pcm_t *handle = extras->handle;
    snd_pcm_sframes_t avail = 0;
    long err = 0;

    *play_cursor = 0;
    *unused_write_cursor = 0;

    do {
#if 0
        snd_pcm_state_t state = snd_pcm_state(handle);
        if (state == SND_PCM_STATE_XRUN)
        {
            err = xrun_recovery(handle, -EPIPE);
            if (err < 0)
            {
                ERRLOG("XRUN recovery failed: %s", snd_strerror(err));
                break;
            }
        }
        else if (state == SND_PCM_STATE_SUSPENDED)
        {
            err = xrun_recovery(handle, -ESTRPIPE);
            if (err < 0)
            {
                ERRLOG("SUSPEND recovery failed: %s", snd_strerror(err));
                break;
            }
        }
#endif

#if 0
        else if ( !((state == SND_PCM_STATE_PREPARED) || (state == SND_PCM_STATE_RUNNING)) )
        {
            ERRLOG("PCM state is not running!");
            break;
        }
#endif

        if ((avail = snd_pcm_avail_update(handle)) < 0)
        {
            if (avail == -EPIPE)
            {
                ERRLOG("NOTE snd_pcm_avail_update() < 0 ...");
                // underrun ... whole buffer available
                return 0;
            }
        }

        if (avail < 1024) // HACK MAGICK CONSTANT
        {
            ERRLOG("OOPS avail(%ld) < 1024 ... ", avail);
            ERRLOG("performing snd_pcm_wait() ...");
            err = snd_pcm_wait(handle, -1);
            if (err < 0)
            {
                ERRLOG("\tOOPS performing xrun_recovery() ...");
                if ((err = xrun_recovery(handle, err)) < 0)
                {
                    ERRLOG("snd_pcm_wait error: %s", snd_strerror(err));
                    break;
                }
            }
            break;
        }

        avail = extras->buffer_size - avail;
        *play_cursor = avail<<(extras->shift_per_sample);
        return 0;

    } while (0);

    return err ?: -1;
}

// HACK NOTE : audio_ptr2 is unused
// DS->Lock()
static long alsa_begin(void *_this, unsigned long unused, unsigned long write_bytes, void **audio_ptr1, unsigned long *audio_bytes1, void **unused_audio_ptr2, unsigned long *audio_bytes2, unsigned long flags_unused)
{
    int err = 0;

    ALSAExtras *extras = (ALSAExtras*)_this;
    const snd_pcm_channel_area_t *areas;
    uint8_t *bytes = NULL;

    snd_pcm_uframes_t offset_frames = 0;
    snd_pcm_uframes_t size_frames = write_bytes>>(extras->shift_per_sample); // HACK : assuming 16bit samples

    if (write_bytes == 0)
    {
        size_frames = extras->buffer_size;
    }

    *audio_ptr1 = NULL;
    if (unused_audio_ptr2)
    {
        *unused_audio_ptr2 = NULL;
    }
    *audio_bytes1 = 0;
    *audio_bytes2 = 0;

    do {
        snd_pcm_t *handle = extras->handle;
        while ((err = snd_pcm_mmap_begin(handle, &areas, &offset_frames, &size_frames)) < 0)
        {
            if ((err = xrun_recovery(handle, err)) < 0)
            {
                ERRLOG("MMAP begin avail error: %s", snd_strerror(err));
                break;
            }
        }

        //LOG("addr:%p first:%u offset:%lu frames:%lu", areas[0].addr, areas[0].first, offset_frames, size_frames);

        const snd_pcm_channel_area_t area = areas[0];// assuming mono ...
        assert((area.first % 8) == 0);

        bytes = (((uint8_t *)area.addr) + (area.first>>3));
        assert(extras->format_bits == area.step);
        assert(extras->bytes_per_sample == (area.step>>3));
        bytes += offset_frames * extras->bytes_per_sample;

        *audio_ptr1 = (void*)bytes;
        *audio_bytes1 = size_frames<<(extras->shift_per_sample);
        *audio_bytes2 = offset_frames;
        return 0;

    } while(0);

    return err ?: -1;
}

// DS->Unlock()
static long alsa_commit(void *_this, void *unused_audio_ptr1, unsigned long audio_bytes1, void *unused_audio_ptr2, unsigned long audio_bytes2)
{
    ALSAExtras *extras = (ALSAExtras*)_this;
    assert(unused_audio_ptr2 == NULL);
    int err = 0;

    snd_pcm_uframes_t size_frames = audio_bytes1>>(extras->shift_per_sample);
    snd_pcm_uframes_t offset_frames = audio_bytes2;
    do {
        snd_pcm_t *handle = extras->handle;
        err = snd_pcm_mmap_commit(handle, offset_frames, size_frames);
        if (err < 0 || (snd_pcm_uframes_t)err != size_frames)
        {
            if ((err = xrun_recovery(handle, err >= 0 ? -EPIPE : err)) < 0)
            {
                ERRLOG("MMAP commit error: %s", snd_strerror(err));
                break;
            }
        }

        if (snd_pcm_state(handle) != SND_PCM_STATE_RUNNING)
        {
            if ((err = snd_pcm_start(handle)) < 0)
            {
                ERRLOG("Start error: %s", snd_strerror(err));
                break;
            }
        }

        return 0;

    } while(0);

    return err ?: -1;
}

static long alsa_get_status(void *_this, unsigned long *status)
{
    snd_pcm_t *handle = ((ALSAExtras*)_this)->handle;

    if (snd_pcm_state(handle) == SND_PCM_STATE_RUNNING)
    {
        *status = DSBSTATUS_PLAYING;
    }
    else
    {
        *status = _DSBSTATUS_NOTPLAYING;
    }

    return 0;
}

// ----------------------------------------------------------------------------

static int set_hwparams(ALSAExtras *extras, ALSABufferParamsStruct *params_struct)
{
    int err = 0;

    int resample = 1; // enable alsa-lib resampling

    snd_pcm_t *handle = extras->handle;
    snd_pcm_hw_params_t *hwparams = extras->hwparams;

    /* choose all parameters */
    if ((err = snd_pcm_hw_params_any(handle, hwparams)) < 0)
    {
        ERRLOG("Broken configuration for playback: no configurations available: %s", snd_strerror(err));
        return err;
    }

    /* set hardware resampling */
    if ((err = snd_pcm_hw_params_set_rate_resample(handle, hwparams, resample)) < 0)
    {
        ERRLOG("Resampling setup failed for playback: %s", snd_strerror(err));
        return err;
    }

    /* set the interleaved read/write format */
    if ((err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_MMAP_NONINTERLEAVED)) < 0)
    {
        ERRLOG("Access type not available for playback: %s", snd_strerror(err));
        return err;
    }

    extras->format = SND_PCM_FORMAT_S16;
    /* set the sample format as signed 16bit samples */
    if ((err = snd_pcm_hw_params_set_format(handle, hwparams, extras->format)) < 0)
    {
        ERRLOG("Sample format not available for playback: %s", snd_strerror(err));
        return err;
    }
    extras->format_bits = snd_pcm_format_physical_width(extras->format);
    extras->bytes_per_sample = extras->format_bits / 8;
    extras->shift_per_sample = (extras->bytes_per_sample>>1); // HACK : ASSUMES 16bit samples ...

    /* set the count of channels */
    if ((err = snd_pcm_hw_params_set_channels(handle, hwparams, params_struct->lpwfxFormat->nChannels)) < 0)
    {
        ERRLOG("Channels count of 1 not available for playbacks: %s", snd_strerror(err));
        return err;
    }

    /* set the stream rate */
    unsigned int rrate = params_struct->lpwfxFormat->nSamplesPerSec;
    if ((err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rrate, 0)) < 0)
    {
        ERRLOG("Rate %luHz not available for playback: %s", params_struct->lpwfxFormat->nSamplesPerSec, snd_strerror(err));
        return err;
    }

    if (rrate != params_struct->lpwfxFormat->nSamplesPerSec)
    {
        ERRLOG("Rate doesn't match (requested %luHz, get %iHz)", params_struct->lpwfxFormat->nSamplesPerSec, err);
        return -EINVAL;
    }

    /* set the buffer size */
    int ignored;
    extras->buffer_size = params_struct->dwBufferBytes;
    err = snd_pcm_hw_params_set_buffer_size_near(handle, hwparams, &(extras->buffer_size));
    if (err < 0)
    {
        ERRLOG("Unable to set buffer size %d for playback: %s", (int)extras->buffer_size, snd_strerror(err));
    }

    snd_pcm_uframes_t size;
    if ((err = snd_pcm_hw_params_get_buffer_size(hwparams, &size)) < 0)
    {
        ERRLOG("Unable to get buffer size for playback: %s", snd_strerror(err));
        return err;
    }
    if (size != extras->buffer_size)
    {
        ERRLOG("Note: buffer_size fetch mismatch using %d -- (%d requested)", (int)size, (int)extras->buffer_size);
        extras->buffer_size = size;
    }

    extras->period_size = (extras->buffer_size)>>3;
    err = snd_pcm_hw_params_set_period_size_near(handle, hwparams, &(extras->period_size), &ignored);
    if (err < 0)
    {
        ERRLOG("Unable to set period size %d for playback: %s", (int)extras->period_size, snd_strerror(err));
    }

    if ((err = snd_pcm_hw_params_get_period_size(hwparams, &size, &ignored)) < 0)
    {
        ERRLOG("Unable to get period size for playback: %s", snd_strerror(err));
        return err;
    }
    if (size != extras->period_size)
    {
        ERRLOG("Note: period_size fetch mismatch using %d -- (%d requested)", (int)size, (int)extras->period_size);
        extras->period_size = size;
    }

    /* write the parameters to device */
    if ((err = snd_pcm_hw_params(handle, hwparams)) < 0)
    {
        ERRLOG("Unable to set hwparams for playback: %s", snd_strerror(err));
        return err;
    }

    return 0;
}

static int set_swparams(ALSAExtras *extras, ALSABufferParamsStruct *params_struct)
{
    int err = -1;

    int period_event = 0; // produce poll event after each period
    snd_pcm_t *handle = extras->handle;
    snd_pcm_sw_params_t *swparams = extras->swparams;

    /* get the current swparams */
    if ((err = snd_pcm_sw_params_current(handle, swparams)) < 0)
    {
        ERRLOG("Unable to determine current swparams for playback: %s", snd_strerror(err));
        return err;
    }

    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    if ((err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (extras->buffer_size / extras->period_size) * extras->period_size)) < 0)
    {
        ERRLOG("Unable to set start threshold mode for playback: %s", snd_strerror(err));
        return err;
    }

    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    if ((err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? extras->buffer_size : extras->period_size)) < 0)
    {
        ERRLOG("Unable to set avail min for playback: %s", snd_strerror(err));
        return err;
    }

    /* enable period events when requested */
    if (period_event)
    {
        if ((err = snd_pcm_sw_params_set_period_event(handle, swparams, 1)) < 0)
        {
            ERRLOG("Unable to set period event: %s", snd_strerror(err));
            return err;
        }
    }

    /* write the parameters to the playback device */
    if ((err = snd_pcm_sw_params(handle, swparams)) < 0)
    {
        ERRLOG("Unable to set swparams for playback: %s", snd_strerror(err));
        return err;
    }

    return 0;
}

static long alsa_create_sound_buffer(ALSABufferParamsStruct *params_struct, ALSASoundBufferStruct **soundbuf_struct, void *extra_data)
{
    assert(*soundbuf_struct == NULL);

    const SoundSystemStruct *sound_struct = (SoundSystemStruct*)extra_data;
    snd_pcm_t *handle = (snd_pcm_t*)(sound_struct->implementation_specific);
    assert(handle != NULL);

    ALSAExtras *extras = NULL;
    int err = -1;
#if 0
    ALSASoundStructExtras *extras = NULL;
#endif

    do {

        if ((extras = calloc(1, sizeof(ALSAExtras))) == NULL)
        {
            ERRLOG("Not enough memory");
            break;
        }

        extras->handle = handle;
        snd_pcm_hw_params_alloca(&(extras->hwparams));
        snd_pcm_sw_params_alloca(&(extras->swparams));

        if ((*soundbuf_struct = malloc(sizeof(ALSASoundBufferStruct))) == NULL)
        {
            ERRLOG("Not enough memory");
            break;
        }

        (*soundbuf_struct)->_this = extras;
        (*soundbuf_struct)->SetVolume              = alsa_set_volume;
        (*soundbuf_struct)->GetVolume              = alsa_get_volume;
        (*soundbuf_struct)->GetCurrentPosition     = alsa_get_position;
        (*soundbuf_struct)->Stop                   = alsa_stop;
        (*soundbuf_struct)->Restore                = alsa_restore;
        (*soundbuf_struct)->Play                   = alsa_play;
        (*soundbuf_struct)->Lock                   = alsa_begin;
        (*soundbuf_struct)->Unlock                 = alsa_commit;
        (*soundbuf_struct)->GetStatus              = alsa_get_status;

        // configuration ...

        if ((err = set_hwparams(extras, params_struct)) < 0)
        {
            ERRLOG("Setting of hwparams failed: %s", snd_strerror(err));
            break;
        }

        if ((err = set_swparams(extras, params_struct)) < 0)
        {
            ERRLOG("Setting of swparams failed: %s", snd_strerror(err));
            break;
        }

#ifndef NDEBUG
        snd_output_t *output = NULL;
        err = snd_output_stdio_attach(&output, stdout, 0);
        if (err < 0)
        {
            LOG("Output failed: %s", snd_strerror(err));
        }
        snd_pcm_dump(handle, output);
#endif
        return 0;

    } while(0);

    if (extras)
    {
        Free(extras);
    }
    if (*soundbuf_struct)
    {
        alsa_destroy_sound_buffer(soundbuf_struct);
    }

    return err ?: -1;
}

static long alsa_destroy_sound_buffer(ALSASoundBufferStruct **soundbuf_struct)
{
#if 0
    ALSASoundStructExtras *extras = (ALSASoundStructExtras*)((*soundbuf_struct)->implementation_specific);
    Free(extras->area->addr);
    Free(extras->area);
    Free(extras);
    Free(hwparams);
    Free(swparams);
#endif

    //snd_pcm_hw_free(handle); TODO : are we leaking anything ???
    Free(*soundbuf_struct);

    return 0;
}

