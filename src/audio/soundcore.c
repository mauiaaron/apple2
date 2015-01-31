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
#include "audio/win-shim.h"

//-----------------------------------------------------------------------------

#define MAX_SOUND_DEVICES 100

static char **sound_devices = NULL;
static long num_sound_devices = 0;

LPDIRECTSOUND g_lpDS = NULL;

//-------------------------------------

// Used for muting & fading:

#define uMAX_VOICES 66
static UINT g_uNumVoices = 0;
static VOICE* g_pVoices[uMAX_VOICES] = {NULL};

static VOICE* g_pSpeakerVoice = NULL;

//-------------------------------------

bool g_bDSAvailable = false;

bool g_bDisableDirectSound = false;
FILE *g_fh = NULL;

__attribute__((constructor))
static void _init_soundcore() {
    g_fh = error_log;
}

//-----------------------------------------------------------------------------

bool DSGetLock(LPDIRECTSOUNDBUFFER pVoice, DWORD dwOffset, DWORD dwBytes,
                      SHORT** ppDSLockedBuffer0, DWORD* pdwDSLockedBufferSize0,
                      SHORT** ppDSLockedBuffer1, DWORD* pdwDSLockedBufferSize1)
{
    DWORD nStatus = 0;
    HRESULT hr = pVoice->GetStatus(pVoice->_this, &nStatus);
    if(hr != DS_OK)
        return false;

    if(nStatus & DSBSTATUS_BUFFERLOST)
    {
        do
        {
            hr = pVoice->Restore(pVoice->_this);
            if(hr == DSERR_BUFFERLOST)
                Sleep(10);
        }
        while(hr != DS_OK);
    }

    // Get write only pointer(s) to sound buffer
    if(dwBytes == 0)
    {
        if(FAILED(hr = pVoice->Lock(pVoice->_this, 0, 0,
                                (void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0,
                                (void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1,
                                DSBLOCK_ENTIREBUFFER)))
            return false;
    }
    else
    {
        if(FAILED(hr = pVoice->Lock(pVoice->_this, dwOffset, dwBytes,
                                (void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0,
                                (void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1,
                                0)))
            return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

HRESULT DSGetSoundBuffer(VOICE* pVoice, DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels)
{
    WAVEFORMATEX wavfmt;
    DSBUFFERDESC dsbdesc;

    wavfmt.wFormatTag = WAVE_FORMAT_PCM;
    wavfmt.nChannels = nChannels;
    wavfmt.nSamplesPerSec = nSampleRate;
    wavfmt.wBitsPerSample = 16;
    wavfmt.nBlockAlign = wavfmt.nChannels==1 ? 2 : 4;
    wavfmt.nAvgBytesPerSec = wavfmt.nBlockAlign * wavfmt.nSamplesPerSec;

    memset (&dsbdesc, 0, sizeof (dsbdesc));
    dsbdesc.dwSize = sizeof (dsbdesc);
    dsbdesc.dwBufferBytes = dwBufferSize;
    dsbdesc.lpwfxFormat = &wavfmt;
    dsbdesc.dwFlags = dwFlags | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS;

    // Are buffers released when g_lpDS OR pVoice->lpDSBvoice is released?
    // . From DirectX doc:
    //   "Buffer objects are owned by the device object that created them. When the
    //    device object is released, all buffers created by that object are also released..."
        if (pVoice->lpDSBvoice)
        {
            g_lpDS->DestroySoundBuffer(&pVoice->lpDSBvoice);
            //DSReleaseSoundBuffer(pVoice);
        }
    HRESULT hr = g_lpDS->CreateSoundBuffer(&dsbdesc, &pVoice->lpDSBvoice, g_lpDS);
    if(FAILED(hr))
        return hr;

    //

    _ASSERT(g_uNumVoices < uMAX_VOICES);
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

    for(UINT i=0; i<g_uNumVoices; i++)
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

bool DSZeroVoiceBuffer(PVOICE Voice, char* pszDevName, DWORD dwBufferSize)
{
    DWORD dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
    SHORT* pDSLockedBuffer;


        DWORD argX = 0;
    HRESULT hr = Voice->lpDSBvoice->Stop(Voice->lpDSBvoice->_this);
    if(FAILED(hr))
    {
        if(g_fh) fprintf(g_fh, "%s: DSStop failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }
    hr = !DSGetLock(Voice->lpDSBvoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, &argX);
    if(FAILED(hr))
    {
        if(g_fh) fprintf(g_fh, "%s: DSGetLock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    _ASSERT(dwDSLockedBufferSize == dwBufferSize);
    memset(pDSLockedBuffer, 0x00, dwDSLockedBufferSize);

    hr = Voice->lpDSBvoice->Unlock(Voice->lpDSBvoice->_this, (void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, argX);
    if(FAILED(hr))
    {
        if(g_fh) fprintf(g_fh, "%s: DSUnlock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    hr = Voice->lpDSBvoice->Play(Voice->lpDSBvoice->_this,0,0,0);
    if(FAILED(hr))
    {
        if(g_fh) fprintf(g_fh, "%s: DSPlay failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

bool DSZeroVoiceWritableBuffer(PVOICE Voice, char* pszDevName, DWORD dwBufferSize)
{
    DWORD dwDSLockedBufferSize0=0, dwDSLockedBufferSize1=0;
    SHORT *pDSLockedBuffer0, *pDSLockedBuffer1;


    HRESULT hr = DSGetLock(Voice->lpDSBvoice,
                            0, dwBufferSize,
                            &pDSLockedBuffer0, &dwDSLockedBufferSize0,
                            &pDSLockedBuffer1, &dwDSLockedBufferSize1);
        hr = !hr;
    if(FAILED(hr))
    {
        if(g_fh) fprintf(g_fh, "%s: DSGetLock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    memset(pDSLockedBuffer0, 0x00, dwDSLockedBufferSize0);
    if(pDSLockedBuffer1)
        memset(pDSLockedBuffer1, 0x00, dwDSLockedBufferSize1);

    hr = Voice->lpDSBvoice->Unlock(Voice->lpDSBvoice->_this, (void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
                                    (void*)pDSLockedBuffer1, dwDSLockedBufferSize1);
    if(FAILED(hr))
    {
        if(g_fh) fprintf(g_fh, "%s: DSUnlock failed (%08X)\n",pszDevName,(unsigned int)hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------

static UINT g_uDSInitRefCount = 0;

bool DSInit()
{
    if (!g_fh)
    {
        g_fh = stderr;
    }
    if(g_bDSAvailable)
    {
        g_uDSInitRefCount++;
        return true;        // Already initialised successfully
    }

        if (sound_devices)
        {
            LOG("Destroying old device names...");
            char **ptr = sound_devices;
            while (*ptr)
            {
                FREE(*ptr);
                ++ptr;
            }
            FREE(sound_devices);
            sound_devices = NULL;
        }
    num_sound_devices = SoundSystemEnumerate(&sound_devices, MAX_SOUND_DEVICES);
        HRESULT hr = (num_sound_devices <= 0);
    if(FAILED(hr))
    {
        if(g_fh) fprintf(g_fh, "DSEnumerate failed (%08X)\n",(unsigned int)hr);
        return false;
    }

    if(g_fh)
    {
        fprintf(g_fh, "Number of sound devices = %ld\n",num_sound_devices);
    }

    bool bCreatedOK = false;
    for(int x=0; x<num_sound_devices; x++)
    {
                if (g_lpDS)
                {
                    SoundSystemDestroy((SoundSystemStruct**)&g_lpDS);
                }
                hr = (int)SoundSystemCreate(sound_devices[x], (SoundSystemStruct**)&g_lpDS);
        if(SUCCEEDED(hr))
        {
            bCreatedOK = true;
            break;
        }

        if(g_fh) fprintf(g_fh, "DSCreate failed for sound device #%d (%08X)\n",x,(unsigned int)hr);
    }
    if(!bCreatedOK)
    {
        if(g_fh) fprintf(g_fh, "DSCreate failed for all sound devices\n");
        return false;
    }

    g_bDSAvailable = true;

    g_uDSInitRefCount = 1;

    return true;
}

//-----------------------------------------------------------------------------

void DSUninit()
{
    if(!g_bDSAvailable)
        return;

    _ASSERT(g_uDSInitRefCount);

    if(g_uDSInitRefCount == 0)
        return;

    g_uDSInitRefCount--;

    if(g_uDSInitRefCount)
        return;

    //

    _ASSERT(g_uNumVoices == 0);

        SoundSystemDestroy((SoundSystemStruct**)&g_lpDS);
    g_bDSAvailable = false;
}

//-----------------------------------------------------------------------------

LONG NewVolume(DWORD dwVolume, DWORD dwVolumeMax)
{
    float fVol = (float) dwVolume / (float) dwVolumeMax;    // 0.0=Max, 1.0=Min

    return (LONG) ((float) DSBVOLUME_MIN * fVol);
}

//=============================================================================

static int g_nErrorInc = 20;    // Old: 1
static int g_nErrorMax = 200;    // Old: 50

int SoundCore_GetErrorInc()
{
    return g_nErrorInc;
}

void SoundCore_SetErrorInc(const int nErrorInc)
{
    g_nErrorInc = nErrorInc < g_nErrorMax ? nErrorInc : g_nErrorMax;
    if(g_fh) fprintf(g_fh, "Speaker/MB Error Inc = %d\n", g_nErrorInc);
}

int SoundCore_GetErrorMax()
{
    return g_nErrorMax;
}

void SoundCore_SetErrorMax(const int nErrorMax)
{
    g_nErrorMax = nErrorMax < MAX_SAMPLES ? nErrorMax : MAX_SAMPLES;
    if(g_fh) fprintf(g_fh, "Speaker/MB Error Max = %d\n", g_nErrorMax);
}


