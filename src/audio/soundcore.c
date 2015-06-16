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
static SoundSystem_s *g_lpDS = NULL;

//-------------------------------------

// Used for muting & fading:

#define uMAX_VOICES 66
static unsigned int g_uNumVoices = 0;
static VOICE* g_pVoices[uMAX_VOICES] = {NULL};

static VOICE* g_pSpeakerVoice = NULL;

//-------------------------------------

bool audio_isAvailable = false;

audio_backend_s *audio_backend = NULL;

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

int DSGetSoundBuffer(VOICE* pVoice, unsigned long dwFlags, unsigned long dwBufferSize, unsigned long nSampleRate, int nChannels)
{
    AudioParams_s params = { 0 };

    params.nChannels = nChannels;
    params.nSamplesPerSec = nSampleRate;
    params.wBitsPerSample = 16;
    params.nBlockAlign = (params.nChannels == 1) ? 2 : 4;
    params.nAvgBytesPerSec = params.nBlockAlign * params.nSamplesPerSec;
    params.dwBufferBytes = dwBufferSize;

    // Are buffers released when g_lpDS OR pVoice->lpDSBvoice is released?
    // . From DirectX doc:
    //   "Buffer objects are owned by the device object that created them. When the
    //    device object is released, all buffers created by that object are also released..."
        if (pVoice->lpDSBvoice)
        {
            g_lpDS->DestroySoundBuffer(&pVoice->lpDSBvoice);
            //DSReleaseSoundBuffer(pVoice);
        }
    int hr = g_lpDS->CreateSoundBuffer(&params, &pVoice->lpDSBvoice, g_lpDS);
    if(hr)
        return hr;

    //

    assert(g_uNumVoices < uMAX_VOICES);
    if(g_uNumVoices < uMAX_VOICES)
        g_pVoices[g_uNumVoices++] = pVoice;

    if(pVoice->bIsSpeaker)
        g_pSpeakerVoice = pVoice;

    return hr;
}

void DSReleaseSoundBuffer(VOICE* pVoice)
{
    if(pVoice->bIsSpeaker)
        g_pSpeakerVoice = NULL;

    for(unsigned int i=0; i<g_uNumVoices; i++)
    {
        if(g_pVoices[i] == pVoice)
        {
            g_pVoices[i] = g_pVoices[g_uNumVoices-1];
            g_pVoices[g_uNumVoices-1] = NULL;
            g_uNumVoices--;
            break;
        }
    }

        if (g_lpDS)
        {
            g_lpDS->DestroySoundBuffer(&pVoice->lpDSBvoice);
        }
}

//-----------------------------------------------------------------------------

bool DSZeroVoiceBuffer(PVOICE Voice, char* pszDevName, unsigned long dwBufferSize)
{
    unsigned long dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
    int16_t* pDSLockedBuffer;


        unsigned long argX = 0;
    int hr = Voice->lpDSBvoice->Stop(Voice->lpDSBvoice->_this);
    if(hr)
    {
        LOG("%s: DSStop failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }
    hr = !DSGetLock(Voice->lpDSBvoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, &argX);
    if(hr)
    {
        LOG("%s: DSGetLock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    assert(dwDSLockedBufferSize == dwBufferSize);
    memset(pDSLockedBuffer, 0x00, dwDSLockedBufferSize);

    hr = Voice->lpDSBvoice->Unlock(Voice->lpDSBvoice->_this, (void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, argX);
    if(hr)
    {
        LOG("%s: DSUnlock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    hr = Voice->lpDSBvoice->Play(Voice->lpDSBvoice->_this,0,0,0);
    if(hr)
    {
        LOG("%s: DSPlay failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

bool DSZeroVoiceWritableBuffer(PVOICE Voice, char* pszDevName, unsigned long dwBufferSize)
{
    unsigned long dwDSLockedBufferSize0=0, dwDSLockedBufferSize1=0;
    int16_t *pDSLockedBuffer0, *pDSLockedBuffer1;


    int hr = DSGetLock(Voice->lpDSBvoice,
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

    hr = Voice->lpDSBvoice->Unlock(Voice->lpDSBvoice->_this, (void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
                                    (void*)pDSLockedBuffer1, dwDSLockedBufferSize1);
    if(hr)
    {
        LOG("%s: DSUnlock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

static unsigned int g_uDSInitRefCount = 0;

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

bool audio_init(void)
{
    if(audio_isAvailable)
    {
        g_uDSInitRefCount++;
        return true;        // Already initialised successfully
    }

    _destroy_enumerated_sound_devices();
    num_sound_devices = audio_backend->enumerateDevices(&sound_devices, MAX_SOUND_DEVICES);
        int hr = (num_sound_devices <= 0);
    if(hr)
    {
        LOG("DSEnumerate failed (%08X)\n",(unsigned int)hr);
        return false;
    }

    LOG("Number of sound devices = %ld\n",num_sound_devices);

    bool bCreatedOK = false;
    for(int x=0; x<num_sound_devices; x++)
    {
                if (g_lpDS)
                {
                    audio_backend->shutdown((SoundSystem_s**)&g_lpDS);
                }
                hr = (int)audio_backend->init(sound_devices[x], (SoundSystem_s**)&g_lpDS);
        if(hr == 0)
        {
            bCreatedOK = true;
            break;
        }

        LOG("DSCreate failed for sound device #%d (%08X)\n",x,(unsigned int)hr);
    }
    if(!bCreatedOK)
    {
        LOG("DSCreate failed for all sound devices\n");
        return false;
    }

    audio_isAvailable = true;

    g_uDSInitRefCount = 1;

    return true;
}

//-----------------------------------------------------------------------------

void audio_shutdown(void)
{
    _destroy_enumerated_sound_devices();

    if(!audio_isAvailable)
        return;

    assert(g_uDSInitRefCount);

    if(g_uDSInitRefCount == 0)
        return;

    g_uDSInitRefCount--;

    if(g_uDSInitRefCount)
        return;

    //

    assert(g_uNumVoices == 0);

        audio_backend->shutdown((SoundSystem_s**)&g_lpDS);
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

