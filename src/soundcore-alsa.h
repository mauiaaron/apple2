/*
 * Apple // emulator for Linux
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#ifndef _SOUNDCORE_ALSA_H_
#define _SOUNDCORE_ALSA_H_

#include "common.h"
#include "soundcore.h"
#include <alsa/asoundlib.h>

#undef DSBVOLUME_MIN
#define DSBVOLUME_MIN 0

#undef DSBVOLUME_MAX
#define DSBVOLUME_MAX 100

typedef struct IDirectSoundBuffer ALSASoundBufferStruct;

typedef struct DSBUFFERDESC ALSABufferParamsStruct;

typedef struct {
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_channel_area_t *area;
} ALSASoundStructExtras;

#endif /* whole file */

