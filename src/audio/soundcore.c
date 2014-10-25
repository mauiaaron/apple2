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

#ifdef APPLE2IX

#include "audio/win-shim.h"

#else
#include "StdAfx.h"
#endif

//-----------------------------------------------------------------------------

#define MAX_SOUND_DEVICES 100

#ifdef APPLE2IX
static char **sound_devices = NULL;
#else
static char *sound_devices[MAX_SOUND_DEVICES];
static GUID sound_device_guid[MAX_SOUND_DEVICES];
#endif
static long num_sound_devices = 0;

#ifdef APPLE2IX
LPDIRECTSOUND g_lpDS = NULL;
#else
static LPDIRECTSOUND g_lpDS = NULL;
#endif

//-------------------------------------

// Used for muting & fading:

#ifdef APPLE2IX
#define uMAX_VOICES 66
#else
static const UINT uMAX_VOICES = 66;	// 64 phonemes + spkr + mockingboard
#endif
static UINT g_uNumVoices = 0;
static VOICE* g_pVoices[uMAX_VOICES] = {NULL};

static VOICE* g_pSpeakerVoice = NULL;

//-------------------------------------

bool g_bDSAvailable = false;

#ifdef APPLE2IX
bool g_bDisableDirectSound = false;
FILE *g_fh = NULL;

__attribute__((constructor))
static void _init_soundcore() {
    g_fh = error_log;
}
#endif

//-----------------------------------------------------------------------------

#ifndef APPLE2IX
static BOOL CALLBACK DSEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName,  LPVOID lpContext)
{
	int i = num_sound_devices;
	if(i == MAX_SOUND_DEVICES)
		return TRUE;
	if(lpGUID != NULL)
		memcpy(&sound_device_guid[i], lpGUID, sizeof (GUID));
	sound_devices[i] = _strdup(lpszDesc);

	if(g_fh) fprintf(g_fh, "%d: %s - %s\n",i,lpszDesc,lpszDrvName);

	num_sound_devices++;
	return TRUE;
}
#endif

//-----------------------------------------------------------------------------

#ifdef _DEBUG
static char *DirectSound_ErrorText (HRESULT error)
{
    switch( error )
    {
    case DSERR_ALLOCATED:
        return "Allocated";
    case DSERR_CONTROLUNAVAIL:
        return "Control Unavailable";
    case DSERR_INVALIDPARAM:
        return "Invalid Parameter";
    case DSERR_INVALIDCALL:
        return "Invalid Call";
    case DSERR_GENERIC:
        return "Generic";
    case DSERR_PRIOLEVELNEEDED:
        return "Priority Level Needed";
    case DSERR_OUTOFMEMORY:
        return "Out of Memory";
    case DSERR_BADFORMAT:
        return "Bad Format";
    case DSERR_UNSUPPORTED:
        return "Unsupported";
    case DSERR_NODRIVER:
        return "No Driver";
    case DSERR_ALREADYINITIALIZED:
        return "Already Initialized";
    case DSERR_NOAGGREGATION:
        return "No Aggregation";
    case DSERR_BUFFERLOST:
        return "Buffer Lost";
    case DSERR_OTHERAPPHASPRIO:
        return "Other Application Has Priority";
    case DSERR_UNINITIALIZED:
        return "Uninitialized";
    case DSERR_NOINTERFACE:
        return "No Interface";
    default:
        return "Unknown";
    }
}
#endif

//-----------------------------------------------------------------------------

bool DSGetLock(LPDIRECTSOUNDBUFFER pVoice, DWORD dwOffset, DWORD dwBytes,
					  SHORT** ppDSLockedBuffer0, DWORD* pdwDSLockedBufferSize0,
					  SHORT** ppDSLockedBuffer1, DWORD* pdwDSLockedBufferSize1)
{
	DWORD nStatus = 0;
#ifdef APPLE2IX
	HRESULT hr = pVoice->GetStatus(pVoice->_this, &nStatus);
#else
	HRESULT hr = pVoice->GetStatus(&nStatus);
#endif
	if(hr != DS_OK)
		return false;

	if(nStatus & DSBSTATUS_BUFFERLOST)
	{
		do
		{
#ifdef APPLE2IX
			hr = pVoice->Restore(pVoice->_this);
#else
			hr = pVoice->Restore();
#endif
			if(hr == DSERR_BUFFERLOST)
				Sleep(10);
		}
		while(hr != DS_OK);
	}

	// Get write only pointer(s) to sound buffer
	if(dwBytes == 0)
	{
#ifdef APPLE2IX
		if(FAILED(hr = pVoice->Lock(pVoice->_this, 0, 0,
#else
		if(FAILED(hr = pVoice->Lock(0, 0,
#endif
								(void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0,
								(void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1,
								DSBLOCK_ENTIREBUFFER)))
			return false;
	}
	else
	{
#ifdef APPLE2IX
		if(FAILED(hr = pVoice->Lock(pVoice->_this, dwOffset, dwBytes,
#else
		if(FAILED(hr = pVoice->Lock(dwOffset, dwBytes,
#endif
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
#ifdef APPLE2IX
        if (pVoice->lpDSBvoice)
        {
            g_lpDS->DestroySoundBuffer(&pVoice->lpDSBvoice);
            //DSReleaseSoundBuffer(pVoice);
        }
	HRESULT hr = g_lpDS->CreateSoundBuffer(&dsbdesc, &pVoice->lpDSBvoice, g_lpDS);
#else
	HRESULT hr = g_lpDS->CreateSoundBuffer(&dsbdesc, &pVoice->lpDSBvoice, NULL);
#endif
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

#ifdef APPLE2IX
        if (g_lpDS)
        {
            g_lpDS->DestroySoundBuffer(&pVoice->lpDSBvoice);
        }
#else
	SAFE_RELEASE(pVoice->lpDSBvoice);
#endif
}

//-----------------------------------------------------------------------------

bool DSZeroVoiceBuffer(PVOICE Voice, char* pszDevName, DWORD dwBufferSize)
{
	DWORD dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
	SHORT* pDSLockedBuffer;


#ifdef APPLE2IX
        DWORD argX = 0;
	HRESULT hr = Voice->lpDSBvoice->Stop(Voice->lpDSBvoice->_this);
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "%s: DSStop failed (%08X)\n",pszDevName,(unsigned int)hr);
		return false;
	}
	hr = !DSGetLock(Voice->lpDSBvoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, &argX);
#else

	hr = DSGetLock(Voice->lpDSBvoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, 0);
#endif
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "%s: DSGetLock failed (%08X)\n",pszDevName,(unsigned int)hr);
		return false;
	}

	_ASSERT(dwDSLockedBufferSize == dwBufferSize);
	memset(pDSLockedBuffer, 0x00, dwDSLockedBufferSize);

#ifdef APPLE2IX
	hr = Voice->lpDSBvoice->Unlock(Voice->lpDSBvoice->_this, (void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, argX);
#else
	hr = Voice->lpDSBvoice->Unlock((void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);
#endif
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "%s: DSUnlock failed (%08X)\n",pszDevName,(unsigned int)hr);
		return false;
	}

#ifdef APPLE2IX
	hr = Voice->lpDSBvoice->Play(Voice->lpDSBvoice->_this,0,0,0);
#else
	hr = Voice->lpDSBvoice->Play(0,0,DSBPLAY_LOOPING);
#endif
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
#ifdef APPLE2IX
        hr = !hr;
#endif
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "%s: DSGetLock failed (%08X)\n",pszDevName,(unsigned int)hr);
		return false;
	}

	memset(pDSLockedBuffer0, 0x00, dwDSLockedBufferSize0);
	if(pDSLockedBuffer1)
		memset(pDSLockedBuffer1, 0x00, dwDSLockedBufferSize1);

#ifdef APPLE2IX
	hr = Voice->lpDSBvoice->Unlock(Voice->lpDSBvoice->_this, (void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
#else
	hr = Voice->lpDSBvoice->Unlock((void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
#endif
									(void*)pDSLockedBuffer1, dwDSLockedBufferSize1);
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "%s: DSUnlock failed (%08X)\n",pszDevName,(unsigned int)hr);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

static bool g_bTimerActive = false;
#ifndef APPLE2IX
static eFADE g_FadeType = FADE_NONE;
static UINT_PTR g_nTimerID = 0;
#endif

//-------------------------------------

#ifndef APPLE2IX
static VOID CALLBACK SoundCore_TimerFunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
#endif

#ifndef APPLE2IX
static bool SoundCore_StartTimer()
{
	if(g_bTimerActive)
		return true;

	g_nTimerID = SetTimer(NULL, 0, 1, SoundCore_TimerFunc);	// 1ms interval
	if(g_nTimerID == 0)
	{
		fprintf(stderr, "Error creating timer\n");
		_ASSERT(0);
		return false;
	}

	g_bTimerActive = true;
	return true;
}
#endif

static void SoundCore_StopTimer()
{
#ifdef APPLE2IX
    // using our own timing and nanosleep() ...
#else
	if(!g_bTimerActive)
		return;

	if(KillTimer(NULL, g_nTimerID) == FALSE)
	{
		fprintf(stderr, "Error killing timer\n");
		_ASSERT(0);
		return;
	}

	g_bTimerActive = false;
#endif
}

bool SoundCore_GetTimerState()
{
	return g_bTimerActive;
}

//-------------------------------------

// [OLD: Used to fade volume in/out]
// FADE_OUT : Just keep filling speaker soundbuffer with same value
// FADE_IN  : Switch to FADE_NONE & StopTimer()

#ifndef APPLE2IX
static VOID CALLBACK SoundCore_TimerFunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	if((g_pSpeakerVoice == NULL) || (g_pSpeakerVoice->bActive == false))
		g_FadeType = FADE_NONE;

	// Timer expired
	if(g_FadeType == FADE_NONE)
	{
		SoundCore_StopTimer();
		return;
	}

	//

#if 1
	if(g_FadeType == FADE_IN)
		g_FadeType = FADE_NONE;
	else
		SpkrUpdate_Timer();
#else
	const LONG nFadeUnit_Fast = (DSBVOLUME_MAX - DSBVOLUME_MIN) / 10;
	const LONG nFadeUnit_Slow = (DSBVOLUME_MAX - DSBVOLUME_MIN) / 1000;	// Less noisy for 'silence'

	LONG nFadeUnit = g_pSpeakerVoice->bRecentlyActive ? nFadeUnit_Fast : nFadeUnit_Slow;
	LONG nFadeVolume = g_pSpeakerVoice->nFadeVolume;

	if(g_FadeType == FADE_IN)
	{
		if(nFadeVolume == g_pSpeakerVoice->nVolume)
		{
			g_FadeType = FADE_NONE;
			SoundCore_StopTimer();
			return;
		}

		nFadeVolume += nFadeUnit;

		if(nFadeVolume > g_pSpeakerVoice->nVolume)
			nFadeVolume = g_pSpeakerVoice->nVolume;
	}
	else // FADE_OUT
	{
		if(nFadeVolume == DSBVOLUME_MIN)
		{
			g_FadeType = FADE_NONE;
			SoundCore_StopTimer();
			return;
		}

		nFadeVolume -= nFadeUnit;

		if(nFadeVolume < DSBVOLUME_MIN)
			nFadeVolume = DSBVOLUME_MIN;
	}

	g_pSpeakerVoice->nFadeVolume = nFadeVolume;
	g_pSpeakerVoice->lpDSBvoice->SetVolume(nFadeVolume);
#endif
}
#endif

//-----------------------------------------------------------------------------

#ifndef APPLE2IX
void SoundCore_SetFade(eFADE FadeType)
{
	static int nLastMode = -1;

	if(g_nAppMode == MODE_DEBUG)
		return;

	// Fade in/out just for speaker, the others are demuted/muted
	if(FadeType != FADE_NONE)
	{
		for(UINT i=0; i<g_uNumVoices; i++)
		{
			// Note: Kludge for fading speaker if curr/last g_nAppMode is/was MODE_LOGO:
			// . Bug in DirectSound? SpeakerVoice.lpDSBvoice->SetVolume() doesn't work without this!
			// . See SoundCore_TweakVolumes() - could be this?
			if((g_pVoices[i]->bIsSpeaker) && (g_nAppMode != MODE_LOGO) && (nLastMode != MODE_LOGO))
			{
				g_pVoices[i]->lpDSBvoice->GetVolume(&g_pVoices[i]->nFadeVolume);
				g_FadeType = FadeType;
				SoundCore_StartTimer();
			}
			else if(FadeType == FADE_OUT)
			{
				g_pVoices[i]->lpDSBvoice->SetVolume(DSBVOLUME_MIN);
				g_pVoices[i]->bMute = true;
			}
			else // FADE_IN
			{
				g_pVoices[i]->lpDSBvoice->SetVolume(g_pVoices[i]->nVolume);
				g_pVoices[i]->bMute = false;
			}
		}
	}
	else // FadeType == FADE_NONE
	{
		if( (g_FadeType != FADE_NONE) &&	// Currently fading-in/out
			(g_pSpeakerVoice && g_pSpeakerVoice->bActive) )
		{
			g_FadeType = FADE_NONE;			// TimerFunc will call StopTimer()
			g_pSpeakerVoice->lpDSBvoice->SetVolume(g_pSpeakerVoice->nVolume);
		}
	}

	nLastMode = g_nAppMode;
}
#endif

//-----------------------------------------------------------------------------

// If AppleWin started by double-clicking a .dsk, then our window won't have focus when volumes are set (so gets ignored).
// Subsequent setting (to the same volume) will get ignored, as DirectSound thinks that volume is already set.

#ifndef APPLE2IX
void SoundCore_TweakVolumes()
{
	for (UINT i=0; i<g_uNumVoices; i++)
	{
		g_pVoices[i]->lpDSBvoice->SetVolume(g_pVoices[i]->nVolume-1);
		g_pVoices[i]->lpDSBvoice->SetVolume(g_pVoices[i]->nVolume);
	}
}
#endif

//-----------------------------------------------------------------------------

static UINT g_uDSInitRefCount = 0;

bool DSInit()
{
#ifdef APPLE2IX
    if (!g_fh)
    {
        g_fh = stderr;
    }
#endif
	if(g_bDSAvailable)
	{
		g_uDSInitRefCount++;
		return true;		// Already initialised successfully
	}

#ifdef APPLE2IX
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
#else
	HRESULT hr = DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumProc, NULL);
#endif
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "DSEnumerate failed (%08X)\n",(unsigned int)hr);
		return false;
	}

	if(g_fh)
	{
#if !defined(APPLE2IX)
		fprintf(g_fh, "Number of sound devices = %d\n",num_sound_devices);
#endif
	}

	bool bCreatedOK = false;
	for(int x=0; x<num_sound_devices; x++)
	{
#ifdef APPLE2IX
                if (g_lpDS)
                {
                    SoundSystemDestroy((SoundSystemStruct**)&g_lpDS);
                }
                hr = (int)SoundSystemCreate(sound_devices[x], (SoundSystemStruct**)&g_lpDS);
#else
		hr = DirectSoundCreate(&sound_device_guid[x], &g_lpDS, NULL);
#endif
		if(SUCCEEDED(hr))
		{
#if !defined(APPLE2IX)
			if(g_fh) fprintf(g_fh, "DSCreate succeeded for sound device #%d\n",x);
#endif
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

#ifndef APPLE2IX
	hr = g_lpDS->SetCooperativeLevel(g_hFrameWindow, DSSCL_NORMAL);
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "SetCooperativeLevel failed (%08X)\n",hr);
		return false;
	}

	DSCAPS DSCaps;
    ZeroMemory(&DSCaps, sizeof(DSCAPS));
    DSCaps.dwSize = sizeof(DSCAPS);
	hr = g_lpDS->GetCaps(&DSCaps);
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "GetCaps failed (%08X)\n",hr);
		// Not fatal: so continue...
	}
#endif

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

#ifdef APPLE2IX
        SoundSystemDestroy((SoundSystemStruct**)&g_lpDS);
#else
	SAFE_RELEASE(g_lpDS);
#endif
	g_bDSAvailable = false;

	SoundCore_StopTimer();
}

//-----------------------------------------------------------------------------

LONG NewVolume(DWORD dwVolume, DWORD dwVolumeMax)
{
	float fVol = (float) dwVolume / (float) dwVolumeMax;	// 0.0=Max, 1.0=Min

	return (LONG) ((float) DSBVOLUME_MIN * fVol);
}

//=============================================================================

static int g_nErrorInc = 20;	// Old: 1
static int g_nErrorMax = 200;	// Old: 50

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

//=============================================================================

#ifndef APPLE2IX
static DWORD g_dwAdviseToken;
static IReferenceClock *g_pRefClock = NULL;
static HANDLE g_hSemaphore = NULL;
static bool g_bRefClockTimerActive = false;
static DWORD g_dwLastUsecPeriod = 0;
#endif


bool SysClk_InitTimer()
{
#ifdef APPLE2IX
        // Not using timers ...
        return false;
#else
	g_hSemaphore = CreateSemaphore(NULL, 0, 1, NULL);		// Max count = 1
	if (g_hSemaphore == NULL)
	{
		fprintf(stderr, "Error creating semaphore\n");
		return false;
	}

	if (CoCreateInstance(CLSID_SystemClock, NULL, CLSCTX_INPROC,
                         IID_IReferenceClock, (LPVOID*)&g_pRefClock) != S_OK)
	{
		fprintf(stderr, "Error initialising COM\n");
		return false;	// Fails for Win95!
	}

	return true;
#endif
}

void SysClk_UninitTimer()
{
#ifdef APPLE2IX
        // Not using timers ...
#else
	SysClk_StopTimer();

	SAFE_RELEASE(g_pRefClock);

	if (CloseHandle(g_hSemaphore) == 0)
		fprintf(stderr, "Error closing semaphore handle\n");
#endif
}

//

void SysClk_WaitTimer()
{
#ifdef APPLE2IX
        // Not using timers ...
#else
	if(!g_bRefClockTimerActive)
		return;

	WaitForSingleObject(g_hSemaphore, INFINITE);
#endif
}

//

void SysClk_StartTimerUsec(DWORD dwUsecPeriod)
{
#ifdef APPLE2IX
        // Not using timers ...
#else
	if(g_bRefClockTimerActive && (g_dwLastUsecPeriod == dwUsecPeriod))
		return;

	SysClk_StopTimer();

	REFERENCE_TIME rtPeriod = (REFERENCE_TIME) (dwUsecPeriod * 10);	// In units of 100ns
	REFERENCE_TIME rtNow;

	HRESULT hr = g_pRefClock->GetTime(&rtNow);
	// S_FALSE : Returned time is the same as the previous value

	if ((hr != S_OK) && (hr != S_FALSE))
	{
		_ASSERT(0);
		return;
	}

	if (g_pRefClock->AdvisePeriodic(rtNow, rtPeriod, g_hSemaphore, &g_dwAdviseToken) != S_OK)
	{
		fprintf(stderr, "Error creating timer\n");
		_ASSERT(0);
		return;
	}

	g_dwLastUsecPeriod = dwUsecPeriod;
	g_bRefClockTimerActive = true;
#endif
}

void SysClk_StopTimer()
{
#ifdef APPLE2IX
        // Not using timers ...
#else
	if(!g_bRefClockTimerActive)
		return;

	if (g_pRefClock->Unadvise(g_dwAdviseToken) != S_OK)
	{
		fprintf(stderr, "Error deleting timer\n");
		_ASSERT(0);
		return;
	}

	g_bRefClockTimerActive = false;
#endif
}

