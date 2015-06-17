/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Core sound related functionality
 *
 * Author: Tom Charlesworth
 * Linux ALSA/OpenAL Port : Aaron Culliney
 */

#include "common.h"

//-----------------------------------------------------------------------------

#define MAX_SOUND_DEVICES 100

static char **sound_devices = NULL;
static long num_sound_devices = 0;
static AudioContext_s *audioContext = NULL;

bool audio_isAvailable = false;
AudioBackend_s *audio_backend = NULL;

//-----------------------------------------------------------------------------

bool DSGetLock(AudioBuffer_s *pVoice, unsigned long dwOffset, unsigned long dwBytes,
                      int16_t** ppDSLockedBuffer0, unsigned long* pdwDSLockedBufferSize0,
                      int16_t** ppDSLockedBuffer1, unsigned long* pdwDSLockedBufferSize1)
{
    unsigned long nStatus = 0;
    int hr = pVoice->GetStatus(pVoice->_this, &nStatus);
    if(hr)
        return false;

    // Get write only pointer(s) to sound buffer
    if(dwBytes == 0)
    {
        if( (hr = pVoice->Lock(pVoice->_this, 0, 0,
                                (void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0,
                                (void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1,
                                0)))
            return false;
    }
    else
    {
        if( (hr = pVoice->Lock(pVoice->_this, dwOffset, dwBytes,
                                (void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0,
                                (void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1,
                                0)))
            return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------

bool DSZeroVoiceBuffer(AudioBuffer_s *pVoice, char* pszDevName, unsigned long dwBufferSize)
{
    unsigned long dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
    int16_t* pDSLockedBuffer;


        unsigned long argX = 0;
    int hr = pVoice->Stop(pVoice->_this);
    if(hr)
    {
        LOG("%s: DSStop failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }
    hr = !DSGetLock(pVoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, &argX);
    if(hr)
    {
        LOG("%s: DSGetLock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    assert(dwDSLockedBufferSize == dwBufferSize);
    memset(pDSLockedBuffer, 0x00, dwDSLockedBufferSize);

    hr = pVoice->Unlock(pVoice->_this, (void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, argX);
    if(hr)
    {
        LOG("%s: DSUnlock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    hr = pVoice->Play(pVoice->_this,0,0,0);
    if(hr)
    {
        LOG("%s: DSPlay failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

bool DSZeroVoiceWritableBuffer(AudioBuffer_s *pVoice, char* pszDevName, unsigned long dwBufferSize)
{
    unsigned long dwDSLockedBufferSize0=0, dwDSLockedBufferSize1=0;
    int16_t *pDSLockedBuffer0, *pDSLockedBuffer1;


    int hr = DSGetLock(pVoice,
                            0, dwBufferSize,
                            &pDSLockedBuffer0, &dwDSLockedBufferSize0,
                            &pDSLockedBuffer1, &dwDSLockedBufferSize1);
        hr = !hr;
    if(hr)
    {
        LOG("%s: DSGetLock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    memset(pDSLockedBuffer0, 0x00, dwDSLockedBufferSize0);
    if(pDSLockedBuffer1)
        memset(pDSLockedBuffer1, 0x00, dwDSLockedBufferSize1);

    hr = pVoice->Unlock(pVoice->_this, (void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
                                    (void*)pDSLockedBuffer1, dwDSLockedBufferSize1);
    if(hr)
    {
        LOG("%s: DSUnlock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

static void _destroy_enumerated_sound_devices(void) {
    if (sound_devices) {
        LOG("Destroying old device names...");
        char **ptr = sound_devices;
        while (*ptr) {
            FREE(*ptr);
            ++ptr;
        }
        FREE(sound_devices);
        sound_devices = NULL;
    }
}

bool audio_init(void) {
    if (audio_isAvailable) {
        return true;
    }

    _destroy_enumerated_sound_devices();
    num_sound_devices = audio_backend->enumerateDevices(&sound_devices, MAX_SOUND_DEVICES);
    long err = (num_sound_devices <= 0);

    do {
        if (err) {
            LOG("enumerate sound devices failed : %d\n", err);
            break;
        }

        LOG("Number of sound devices = %ld\n", num_sound_devices);
        bool createdAudioContext = false;
        for (int i=0; i<num_sound_devices; i++) {
            if (audioContext) {
                audio_backend->shutdown(audioContext);
            }
            err = audio_backend->init(sound_devices[i], (AudioContext_s**)&audioContext);
            if (!err) {
                createdAudioContext = true;
                break;
            }

            LOG("warning : failed to create sound device:%d err:%d\n", i, err);
        }

        if (!createdAudioContext) {
            LOG("Failed to create an audio context!\n");
            err = true;
            break;
        }

        audio_isAvailable = true;
    } while (0);

    return err;
}

void audio_shutdown(void) {
    _destroy_enumerated_sound_devices();

    if (!audio_isAvailable) {
        return;
    }

    audio_backend->shutdown(&audioContext);
    audio_isAvailable = false;
}

//=============================================================================

static int g_nErrorInc = 20;    // Old: 1
static int g_nErrorMax = 200;    // Old: 50

int SoundCore_GetErrorInc(void)
{
    return g_nErrorInc;
}

void SoundCore_SetErrorInc(const int nErrorInc)
{
    g_nErrorInc = nErrorInc < g_nErrorMax ? nErrorInc : g_nErrorMax;
    LOG("Speaker/MB Error Inc = %d\n", g_nErrorInc);
}

int SoundCore_GetErrorMax(void)
{
    return g_nErrorMax;
}

void SoundCore_SetErrorMax(const int nErrorMax)
{
    g_nErrorMax = nErrorMax < MAX_SAMPLES ? nErrorMax : MAX_SAMPLES;
    LOG("Speaker/MB Error Max = %d\n", g_nErrorMax);
}

