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

/* Description: Speaker emulation
 *
 * Author: Various
 * Linux ALSA Port : Aaron Culliney
 */

#ifdef APPLE2IX
#include "common.h"
#include "cpu.h"
#include "win-shim.h"
#include "speaker.h"
#include "timing.h"
#include "soundcore.h"
#       ifdef  __linux
#       include <sys/io.h>
#       endif

#       if defined(__GNUC__)
#               pragma GCC diagnostic push
#               pragma GCC diagnostic ignored "-Wformat"
#       elif defined(__clang__)
#               pragma clang diagnostic push
#               pragma clang diagnostic ignored "-Wunused-variable"
#       endif

#else
#include "StdAfx.h"
#endif
#include <wchar.h>

// Notes:
//
// [OLD: 23.191 Apple CLKs == 44100Hz (CLK_6502/44100)]
// 23 Apple CLKS per PC sample (played back at 44.1KHz)
// 
//
// The speaker's wave output drives how much 6502 emulation is done in real-time, eg:
// If the speaker's wave buffer is running out of sample-data, then more 6502 cycles
// need to be executed to top-up the wave buffer.
// This is in contrast to the AY8910 voices, which can simply generate more data if
// their buffers are running low.
//

#define  SOUND_NONE    0
#define  SOUND_DIRECT  1
#ifndef APPLE2IX
#define  SOUND_SMART   2
#endif
#define  SOUND_WAVE    3

#ifdef APPLE2IX
#define g_nSPKR_NumChannels 1
#else
static const unsigned short g_nSPKR_NumChannels = 1;
#endif
static const DWORD g_dwDSSpkrBufferSize = MAX_SAMPLES * sizeof(short) * g_nSPKR_NumChannels;

//-------------------------------------

static short*	g_pSpeakerBuffer = NULL;

// Globals (SOUND_WAVE)
#ifdef APPLE2IX
#define SPKR_DATA_INIT 0x8000;
#else
const short		SPKR_DATA_INIT = (short)0x8000;
#endif

static short	g_nSpeakerData	= SPKR_DATA_INIT;
static UINT		g_nBufferIdx	= 0;

static short*	g_pRemainderBuffer = NULL;
static UINT		g_nRemainderBufferSize;		// Setup in SpkrInitialize()
static UINT		g_nRemainderBufferIdx;		// Setup in SpkrInitialize()


// Application-wide globals:
DWORD			soundtype		= SOUND_WAVE;
double		    g_fClksPerSpkrSample;		// Setup in SetClksPerSpkrSample()

// Globals
#ifndef APPLE2IX
static DWORD	lastcyclenum	= 0;
#endif
static DWORD	toggles			= 0;
static unsigned __int64	g_nSpkrQuietCycleCount = 0;
static unsigned __int64 g_nSpkrLastCycle = 0;
static bool g_bSpkrToggleFlag = false;
static VOICE SpeakerVoice = {0};
static bool g_bSpkrAvailable = false;


// Globals (SOUND_DIRECT/SOUND_SMART)
static BOOL		directio		= 0;
#ifndef APPLE2IX
static DWORD	lastdelta[2]	= {0,0};
static DWORD	quietcycles		= 0;
static DWORD	soundeffect		= 0;
static DWORD	totaldelta		= 0;
#endif

//-----------------------------------------------------------------------------

// Forward refs:
#ifdef APPLE2IX
static ULONG   Spkr_SubmitWaveBuffer_FullSpeed(short* pSpeakerBuffer, ULONG nNumSamples);
static ULONG   Spkr_SubmitWaveBuffer(short* pSpeakerBuffer, ULONG nNumSamples);
#else
ULONG   Spkr_SubmitWaveBuffer_FullSpeed(short* pSpeakerBuffer, ULONG nNumSamples);
ULONG   Spkr_SubmitWaveBuffer(short* pSpeakerBuffer, ULONG nNumSamples);
#endif
static void    Spkr_SetActive(bool bActive);

//=============================================================================

#ifndef APPLE2IX
static void DisplayBenchmarkResults ()
{
  DWORD totaltime = GetTickCount()-extbench;
  VideoRedrawScreen();
  TCHAR buffer[64];
  wsprintf(buffer,
           TEXT("This benchmark took %u.%02u seconds."),
           (unsigned)(totaltime / 1000),
           (unsigned)((totaltime / 10) % 100));
  MessageBox(g_hFrameWindow,
             buffer,
             TEXT("Benchmark Results"),
             MB_ICONINFORMATION | MB_SETFOREGROUND);
}
#endif

//=============================================================================

static void InternalBeep (DWORD frequency, DWORD duration)
{
#ifdef APPLE2IX
#   if defined(__i386__)
    if (duration)
    {
        // HACK FIXME TODO : this needs to be properly tested since I don't have a PC with a speaker anymore...

        // 9/8/2013 -- from http://www.johnath.com/beep/beep.c
        /* I don't know where this number comes from, I admit that freely.  A
         * wonderful human named Raine M. Ekman used it in a program that played
         * a tune at the console, and apparently, it's how the kernel likes its
         * sound requests to be phrased.  If you see Raine, thank him for me.
         *
         * June 28, email from Peter Tirsek (peter at tirsek dot com):
         *
         * This number represents the fixed frequency of the original PC XT's
         * timer chip (the 8254 AFAIR), which is approximately 1.193 MHz. This
         * number is divided with the desired frequency to obtain a counter value,
         * that is subsequently fed into the timer chip, tied to the PC speaker.
         * The chip decreases this counter at every tick (1.193 MHz) and when it
         * reaches zero, it toggles the state of the speaker (on/off, or in/out),
         * resets the counter to the original value, and starts over. The end
         * result of this is a tone at approximately the desired frequency. :)
         */
        frequency = 1193180/frequency;

        // http://ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
        asm (
            "pushl %%eax;"
            "movb  $0x0B6, %%al;"
            "outb  %%al, $0x43;"
            "movl  %0, %%eax;"
            "outb  %%al, $0x42;"
            "movb  %%ah, %%al;"
            "outb  %%al, $0x42;"
            "inb   $0x61, %%al;"
            "orb   $0x3, %%al;"
            "outb  %%al, $0x61;"
            "popl  %%eax;"
            : : "r" (frequency) /*%0*/ : );
    }
    else
    {
        asm (
            "pushl   %eax;"
            "inb     $0x61, %al;"
            "andb    $0x0FC, %al;"
            "outb    %al, $0x61;"
            "popl    %eax;"
            );
    }
#   elif defined(__x86_64__)
    // X64 Direct Access TODO ...
#   endif
#else
#ifdef _X86_
  if (directio)
    if (duration) {
      frequency = 1193180/frequency;
      __asm {
        push eax
        mov  al,0B6h
        out  43h,al
        mov  eax,frequency
        out  42h,al
        mov  al,ah
        out  42h,al
        in   al,61h
        or   al,3
        out  61h,al
        pop  eax
      }
    }
    else
      __asm {
        push eax
        in   al,61h
        and  al,0FCh
        out  61h,al
        pop  eax
      }
  else
#endif
    Beep(frequency,duration);
#endif
}

//=============================================================================

static void InternalClick ()
{
#ifdef APPLE2IX
    // TODO : I don't have a PC with a physical speaker anymore... can I
    // test this with a virtual speaker Linux driver?
#   if defined(__i386__)
    asm (
        "pushl %eax;"
        "inb   $0x61, %al;"
        "xorb  $0x2, %al;"
        "outb  %al, $0x61;"
        "popl  %eax;"
        );
#   elif defined(__x86_64__)
    // X64 Direct Access TODO ...
#   endif
#else
#ifdef _X86_
  if (directio)
    __asm {
      push eax
      in   al,0x61
      xor  al,2
      out  0x61,al
      pop  eax
    }
  else {
#endif
    Beep(37,(DWORD)-1);
    Beep(0,0);
#ifdef _X86_
  }
#endif
#endif
}

//=============================================================================

static void SetClksPerSpkrSample()
{
//	// 23.191 clks for 44.1Khz (when 6502 CLK=1.0Mhz)
//	g_fClksPerSpkrSample = g_fCurrentCLK6502 / (double)SPKR_SAMPLE_RATE;

	// Use integer value: Better for MJ Mahon's RT.SYNTH.DSK (integer multiples of 1.023MHz Clk)
	// . 23 clks @ 1.023MHz
	g_fClksPerSpkrSample = (double) (UINT) (g_fCurrentCLK6502 / (double)SPKR_SAMPLE_RATE);
}

//=============================================================================

static void InitRemainderBuffer()
{
#ifdef APPLE2IX
        free(g_pRemainderBuffer);
#else
	delete [] g_pRemainderBuffer;
#endif

	SetClksPerSpkrSample();

	g_nRemainderBufferSize = (UINT) g_fClksPerSpkrSample;
	if ((double)g_nRemainderBufferSize != g_fClksPerSpkrSample)
		g_nRemainderBufferSize++;

#ifdef APPLE2IX
	g_pRemainderBuffer = calloc(g_nRemainderBufferSize, sizeof(short));
#else
	g_pRemainderBuffer = new short [g_nRemainderBufferSize];
	memset(g_pRemainderBuffer, 0, g_nRemainderBufferSize);
#endif

	g_nRemainderBufferIdx = 0;
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//=============================================================================

void SpkrDestroy ()
{
	Spkr_DSUninit();

	//

	if(soundtype == SOUND_WAVE)
	{
#ifdef APPLE2IX
		free(g_pSpeakerBuffer);
		free(g_pRemainderBuffer);
#else
		delete [] g_pSpeakerBuffer;
		delete [] g_pRemainderBuffer;
#endif
		
		g_pSpeakerBuffer = NULL;
		g_pRemainderBuffer = NULL;
	}
	else
	{
		InternalBeep(0,0);
	}
}

//=============================================================================

void SpkrInitialize ()
{
#ifdef APPLE2IX
    if (soundtype == SOUND_DIRECT)
    {
#    if defined(__i386__)
        if (ioperm(0x42, 1, 1) || ioperm(0x61, 1, 1))
        {
            ERRLOG("Cannot get direct port access to PC speaker, attempting to use sound card...");
            soundtype = SOUND_WAVE;
        }
        else
        {
            LOG("Audio is direct access to PC speaker...");
        }
#    elif defined(__x86_64__)
        // X64 Direct Access TODO ...
        soundtype = SOUND_WAVE;
#    else
        soundtype = SOUND_WAVE;
#    endif
    }
#else
	if(g_fh)
	{
		fprintf(g_fh, "Spkr Config: soundtype = %d ",soundtype);
		switch(soundtype)
		{
			case SOUND_NONE:   fprintf(g_fh, "(NONE)\n"); break;
			case SOUND_DIRECT: fprintf(g_fh, "(DIRECT)\n"); break;
			case SOUND_SMART:  fprintf(g_fh, "(SMART)\n"); break;
			case SOUND_WAVE:   fprintf(g_fh, "(WAVE)\n"); break;
			default:           fprintf(g_fh, "(UNDEFINED!)\n"); break;
		}
	}
#endif

	if(g_bDisableDirectSound)
	{
		SpeakerVoice.bMute = true;
	}
	else
	{
		g_bSpkrAvailable = Spkr_DSInit();
	}

	//

	if (soundtype == SOUND_WAVE)
	{
		InitRemainderBuffer();

#ifdef APPLE2IX
		g_pSpeakerBuffer = calloc(SPKR_SAMPLE_RATE, sizeof(short));	// Buffer can hold a max of 1 seconds worth of samples
#else
		g_pSpeakerBuffer = new short [SPKR_SAMPLE_RATE];	// Buffer can hold a max of 1 seconds worth of samples
#endif
	}

	//

#ifdef APPLE2IX
// initialized direct sound above
#else
  // IF NONE IS, THEN DETERMINE WHETHER WE HAVE DIRECT ACCESS TO THE PC SPEAKER PORT
  if (soundtype != SOUND_WAVE)	// *** TO DO: Need way of determining if DirectX init failed ***
  {
    if (soundtype == SOUND_WAVE)
      soundtype = SOUND_SMART;
#ifdef _X86_
    _try
	{
      __asm {
        in  al,0x61
        xor al,2
        out 0x61,al
        xor al,2
        out 0x61,al
      }
      directio = 1;
    }
    _except (EXCEPTION_EXECUTE_HANDLER)
	{
      directio = 0;
    }
#else
    directio = 0;
#endif
    if ((!directio) && (soundtype == SOUND_DIRECT))
      soundtype = SOUND_SMART;
  }

#endif
}

//=============================================================================

// NB. Called when /g_fCurrentCLK6502/ changes
void SpkrReinitialize ()
{
	if (soundtype == SOUND_WAVE)
	{
		InitRemainderBuffer();
	}
}

//=============================================================================

void SpkrReset()
{
	g_nBufferIdx = 0;
	g_nSpkrQuietCycleCount = 0;
	g_bSpkrToggleFlag = false;

	InitRemainderBuffer();
	Spkr_SubmitWaveBuffer(NULL, 0);
	Spkr_SetActive(false);
	Spkr_Demute();
}

//=============================================================================

BOOL SpkrSetEmulationType (HWND window, DWORD newtype)
{
  if (soundtype != SOUND_NONE)
    SpkrDestroy();
  soundtype = newtype;
  if (soundtype != SOUND_NONE)
    SpkrInitialize();
  if (soundtype != newtype)
    switch (newtype) {

      case SOUND_DIRECT:
        MessageBox(window,
                   TEXT("Direct emulation is not available because the ")
                   TEXT("operating system you are using does not allow ")
                   TEXT("direct control of the speaker."),
                   TEXT("Configuration"),
                   MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        return 0;

      case SOUND_WAVE:
        MessageBox(window,
                   TEXT("The emulator is unable to initialize a waveform ")
                   TEXT("output device.  Make sure you have a sound card ")
                   TEXT("and a driver installed and that windows is ")
                   TEXT("correctly configured to use the driver.  Also ")
                   TEXT("ensure that no other program is currently using ")
                   TEXT("the device."),
                   TEXT("Configuration"),
                   MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        return 0;

    }
  return 1;
}

//=============================================================================

static void ReinitRemainderBuffer(UINT nCyclesRemaining)
{
	if(nCyclesRemaining == 0)
		return;

	for(g_nRemainderBufferIdx=0; g_nRemainderBufferIdx<nCyclesRemaining; g_nRemainderBufferIdx++)
		g_pRemainderBuffer[g_nRemainderBufferIdx] = g_nSpeakerData;

	_ASSERT(g_nRemainderBufferIdx < g_nRemainderBufferSize);
}

static void UpdateRemainderBuffer(ULONG* pnCycleDiff)
{
	if(g_nRemainderBufferIdx)
	{
		while((g_nRemainderBufferIdx < g_nRemainderBufferSize) && *pnCycleDiff)
		{
			g_pRemainderBuffer[g_nRemainderBufferIdx] = g_nSpeakerData;
			g_nRemainderBufferIdx++;
			(*pnCycleDiff)--;
		}

		if(g_nRemainderBufferIdx == g_nRemainderBufferSize)
		{
			g_nRemainderBufferIdx = 0;
			signed long nSampleMean = 0;
			for(UINT i=0; i<g_nRemainderBufferSize; i++)
				nSampleMean += (signed long) g_pRemainderBuffer[i];
			nSampleMean /= (signed long) g_nRemainderBufferSize;

			if(g_nBufferIdx < SPKR_SAMPLE_RATE-1)
				g_pSpeakerBuffer[g_nBufferIdx++] = (short) nSampleMean;
		}
	}
}

static void UpdateSpkr()
{
  if(!g_bFullSpeed || SoundCore_GetTimerState())
  {
	  ULONG nCycleDiff = (ULONG) (g_nCumulativeCycles - g_nSpkrLastCycle);

	  UpdateRemainderBuffer(&nCycleDiff);

	  ULONG nNumSamples = (ULONG) ((double)nCycleDiff / g_fClksPerSpkrSample);

	  ULONG nCyclesRemaining = (ULONG) ((double)nCycleDiff - (double)nNumSamples * g_fClksPerSpkrSample);

	  while((nNumSamples--) && (g_nBufferIdx < SPKR_SAMPLE_RATE-1))
		  g_pSpeakerBuffer[g_nBufferIdx++] = g_nSpeakerData;

	  ReinitRemainderBuffer(nCyclesRemaining);	// Partially fill 1Mhz sample buffer
  }

  g_nSpkrLastCycle = g_nCumulativeCycles;
}

//=============================================================================

// Called by emulation code when Speaker I/O reg is accessed
//

#ifdef APPLE2IX
#define nCyclesLeft cpu65_cycle_count
void SpkrToggle()
#else
BYTE __stdcall SpkrToggle (WORD, WORD, BYTE, BYTE, ULONG nCyclesLeft)
#endif
{
  g_bSpkrToggleFlag = true;

  if(!g_bFullSpeed)
	Spkr_SetActive(true);

  //

#ifndef APPLE2IX
  needsprecision = cumulativecycles;	// ?

  if (extbench)
  {
    DisplayBenchmarkResults();
    extbench = 0;
  }
#endif

  if (soundtype == SOUND_WAVE)
  {
	  CpuCalcCycles(nCyclesLeft);

	  UpdateSpkr();

	  g_nSpeakerData = ~g_nSpeakerData;
  }
  else if (soundtype != SOUND_NONE)
  {

    // IF WE ARE CURRENTLY PLAYING A SOUND EFFECT OR ARE IN DIRECT
    // EMULATION MODE, TOGGLE THE SPEAKER
#ifdef APPLE2IX
            InternalClick();
#else
    if ((soundeffect > 2) || (soundtype == SOUND_DIRECT))
	{
      if (directio)
	  {
        __asm
		{
          push eax
          in   al,0x61
          xor  al,2
          out  0x61,al
          pop  eax
        }
	  }
      else
	  {
        Beep(37,(DWORD)-1);
        Beep(0,0);
      }
	}

    // SAVE INFORMATION ABOUT THE FREQUENCY OF SPEAKER TOGGLING FOR POSSIBLE
    // LATER USE BY SOUND AVERAGING
    if (lastcyclenum)
	{
      toggles++;
      DWORD delta = cyclenum-lastcyclenum;

      // DETERMINE WHETHER WE ARE PLAYING A SOUND EFFECT
      if (directio &&
          ((delta < 250) ||
           (lastdelta[0] && lastdelta[1] &&
            (delta-lastdelta[0] > 250) && (lastdelta[0]-delta > 250) &&
            (delta-lastdelta[1] > 250) && (lastdelta[1]-delta > 250))))
        soundeffect = MIN(35,soundeffect+2);

      lastdelta[1] = lastdelta[0];
      lastdelta[0] = delta;
      totaldelta  += delta;
    }
    lastcyclenum = cyclenum;

#endif
  }

#ifndef APPLE2IX
  return MemReadFloatingBus(nCyclesLeft);
#endif
}

//=============================================================================

// Called by ContinueExecution()
void SpkrUpdate(DWORD totalcycles)
{
  if(!g_bSpkrToggleFlag)
  {
	  if(!g_nSpkrQuietCycleCount)
	  {
		  g_nSpkrQuietCycleCount = g_nCumulativeCycles;
	  }
	  else if(g_nCumulativeCycles - g_nSpkrQuietCycleCount > (unsigned __int64)g_fCurrentCLK6502/5)
	  {
		  // After 0.2 sec of Apple time, deactivate spkr voice
		  // . This allows emulator to auto-switch to full-speed g_nAppMode for fast disk access
		  Spkr_SetActive(false);
	  }
  }
  else
  {
      g_nSpkrQuietCycleCount = 0;
      g_bSpkrToggleFlag = false;
  }

  //

  if (soundtype == SOUND_WAVE)
  {
	  UpdateSpkr();
	  ULONG nSamplesUsed;

	  if(g_bFullSpeed)
		  nSamplesUsed = Spkr_SubmitWaveBuffer_FullSpeed(g_pSpeakerBuffer, g_nBufferIdx);
	  else
		  nSamplesUsed = Spkr_SubmitWaveBuffer(g_pSpeakerBuffer, g_nBufferIdx);

	  _ASSERT(nSamplesUsed <= g_nBufferIdx);
	  memmove(g_pSpeakerBuffer, &g_pSpeakerBuffer[nSamplesUsed], g_nBufferIdx-nSamplesUsed);	// FIXME-TC: _Size * 2
	  g_nBufferIdx -= nSamplesUsed;
  }
#ifndef APPLE2IX
  else
  {

    // IF WE ARE NOT PLAYING A SOUND EFFECT, PERFORM FREQUENCY AVERAGING
    static DWORD currenthertz = 0;
    static BOOL  lastfull     = 0;
    static DWORD lasttoggles  = 0;
    static DWORD lastval      = 0;
    if ((soundeffect > 2) || (soundtype == SOUND_DIRECT)) {
      lastval = 0;
      if (currenthertz && (soundeffect > 4)) {
        InternalBeep(0,0);
        currenthertz = 0;
      }
    }
    else if (toggles && totaldelta) {
      DWORD newval = 1000000*toggles/totaldelta;
      if (lastval && lastfull &&
          (newval-currenthertz > 50) &&
          (currenthertz-newval > 50)) {
        InternalBeep(newval,(DWORD)-1);
        currenthertz = newval;
        lasttoggles  = 0;
      }
      lastfull     = (totaldelta+((totaldelta/toggles) << 1) >= totalcycles);
      lasttoggles += toggles;
      lastval      = newval;
    }
    else if (currenthertz) {
      InternalBeep(0,0);
      currenthertz = 0;
      lastfull     = 0;
      lasttoggles  = 0;
      lastval      = 0;
    }
    else if (lastval) {
      currenthertz = (lasttoggles > 4) ? lastval : 0;
      if (currenthertz)
        InternalBeep(lastval,(DWORD)-1);
      else
        InternalClick();
      lastfull     = 0;
      lasttoggles  = 0;
      lastval      = 0;
    }

    // RESET THE FREQUENCY GATHERING VARIABLES
    lastcyclenum = 0;
    lastdelta[0] = 0;
    lastdelta[1] = 0;
    quietcycles  = toggles ? 0 : (quietcycles+totalcycles);
    toggles      = 0;
    totaldelta   = 0;
    if (soundeffect)
      soundeffect--;

  }
#endif
}

// Called from SoundCore_TimerFunc() for FADE_OUT
void SpkrUpdate_Timer()
{
	if (soundtype == SOUND_WAVE)
	{
		UpdateSpkr();
		ULONG nSamplesUsed;

		nSamplesUsed = Spkr_SubmitWaveBuffer_FullSpeed(g_pSpeakerBuffer, g_nBufferIdx);

		_ASSERT(nSamplesUsed <=	g_nBufferIdx);
		memmove(g_pSpeakerBuffer, &g_pSpeakerBuffer[nSamplesUsed], g_nBufferIdx-nSamplesUsed);	// FIXME-TC: _Size * 2
		g_nBufferIdx -=	nSamplesUsed;
	}
}

//=============================================================================

#ifndef APPLE2IX
static DWORD dwByteOffset = (DWORD)-1;
#endif
static int nNumSamplesError = 0;
static int nDbgSpkrCnt = 0;

// FullSpeed g_nAppMode, 2 cases:
// i) Short burst of full-speed, so PlayCursor doesn't complete sound from previous fixed-speed session.
// ii) Long burst of full-speed, so PlayCursor completes sound from previous fixed-speed session.

// Try to:
// 1) Output remaining samples from SpeakerBuffer (from previous fixed-speed session)
// 2) Output pad samples to keep the VoiceBuffer topped-up

// If nNumSamples>0 then these are from previous fixed-speed session.
// - Output these before outputting zero-pad samples.

static ULONG Spkr_SubmitWaveBuffer_FullSpeed(short* pSpeakerBuffer, ULONG nNumSamples)
{
	//char szDbg[200];
	nDbgSpkrCnt++;

	if(!SpeakerVoice.bActive)
		return nNumSamples;

	// pSpeakerBuffer can't be NULL, as reset clears g_bFullSpeed, so 1st SpkrUpdate() never calls here
	_ASSERT(pSpeakerBuffer != NULL);

	//

	DWORD dwDSLockedBufferSize0, dwDSLockedBufferSize1;
	SHORT *pDSLockedBuffer0, *pDSLockedBuffer1;
	//bool bBufferError = false;

	DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
#ifdef APPLE2IX
	HRESULT hr = SpeakerVoice.lpDSBvoice->GetCurrentPosition(SpeakerVoice.lpDSBvoice->_this, &dwCurrentPlayCursor, &dwCurrentWriteCursor);
#else
	HRESULT hr = SpeakerVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
#endif
	if(FAILED(hr))
		return nNumSamples;

#if 0
	if(dwByteOffset == (DWORD)-1)
	{
		// First time in this func (probably after re-init (Spkr_SubmitWaveBuffer()))

		dwByteOffset = dwCurrentPlayCursor + (g_dwDSSpkrBufferSize/8)*3;	// Ideal: 0.375 is between 0.25 & 0.50 full
		dwByteOffset %= g_dwDSSpkrBufferSize;
		//sprintf(szDbg, "[Submit_FS] PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X [REINIT]\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples); OutputDebugString(szDbg);
	}
	else
	{
		// Check that our offset isn't between Play & Write positions

		if(dwCurrentWriteCursor > dwCurrentPlayCursor)
		{
			// |-----PxxxxxW-----|
			if((dwByteOffset > dwCurrentPlayCursor) && (dwByteOffset < dwCurrentWriteCursor))
			{
				//sprintf(szDbg, "[Submit_FS] PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X xxx\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples); OutputDebugString(szDbg);
				dwByteOffset = dwCurrentWriteCursor;
			}
		}
		else
		{
			// |xxW----------Pxxx|
			if((dwByteOffset > dwCurrentPlayCursor) || (dwByteOffset < dwCurrentWriteCursor))
			{
				//sprintf(szDbg, "[Submit_FS] PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X XXX\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples); OutputDebugString(szDbg);
				dwByteOffset = dwCurrentWriteCursor;
			}
		}
	}

	// Calc bytes remaining to be played
	int nBytesRemaining = dwByteOffset - dwCurrentPlayCursor;
#else
        int nBytesRemaining = dwCurrentPlayCursor;
#endif
	if(nBytesRemaining < 0)
		nBytesRemaining += g_dwDSSpkrBufferSize;
	if((nBytesRemaining == 0) && (dwCurrentPlayCursor != dwCurrentWriteCursor))
		nBytesRemaining = g_dwDSSpkrBufferSize;		// Case when complete buffer is to be played

	//

	UINT nNumPadSamples = 0;

	if(nBytesRemaining < g_dwDSSpkrBufferSize / 4)
	{
		// < 1/4 of play-buffer remaining (need *more* data)
		nNumPadSamples = ((g_dwDSSpkrBufferSize / 4) - nBytesRemaining) / sizeof(short);

		if(nNumPadSamples > nNumSamples)
			nNumPadSamples -= nNumSamples;
		else
			nNumPadSamples = 0;

		// NB. If nNumPadSamples>0 then all nNumSamples are to be used
	}

	//

	UINT nBytesFree = g_dwDSSpkrBufferSize - nBytesRemaining;	// Calc free buffer space
	ULONG nNumSamplesToUse = nNumSamples + nNumPadSamples;

	if(nNumSamplesToUse * sizeof(short) > nBytesFree)
		nNumSamplesToUse = nBytesFree / sizeof(short);

	//

	if(nNumSamplesToUse >= 128)	// Limit the buffer unlock/locking to a minimum
	{
		if(!DSGetLock(SpeakerVoice.lpDSBvoice,
#ifdef APPLE2IX
							/*unused*/ 0, (DWORD)nNumSamplesToUse*sizeof(short),
#else
							dwByteOffset, (DWORD)nNumSamplesToUse*sizeof(short),
#endif
							&pDSLockedBuffer0, &dwDSLockedBufferSize0,
							&pDSLockedBuffer1, &dwDSLockedBufferSize1))
			return nNumSamples;

		//

		DWORD dwBufferSize0 = 0;
		DWORD dwBufferSize1 = 0;

		if(nNumSamples)
		{
			//sprintf(szDbg, "[Submit_FS] C=%08X, PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X ***\n", nDbgSpkrCnt, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples); OutputDebugString(szDbg);

			if(nNumSamples*sizeof(short) <= dwDSLockedBufferSize0)
			{
				dwBufferSize0 = nNumSamples*sizeof(short);
				dwBufferSize1 = 0;
			}
			else
			{
				dwBufferSize0 = dwDSLockedBufferSize0;
				dwBufferSize1 = nNumSamples*sizeof(short) - dwDSLockedBufferSize0;

				if(dwBufferSize1 > dwDSLockedBufferSize1)
					dwBufferSize1 = dwDSLockedBufferSize1;
			}
			
			memcpy(pDSLockedBuffer0, &pSpeakerBuffer[0], dwBufferSize0);
#ifdef RIFF_SPKR
			RiffPutSamples(pDSLockedBuffer0, dwBufferSize0/sizeof(short));
#endif
			nNumSamples = dwBufferSize0/sizeof(short);

			if(pDSLockedBuffer1 && dwBufferSize1)
			{
				memcpy(pDSLockedBuffer1, &pSpeakerBuffer[dwDSLockedBufferSize0/sizeof(short)], dwBufferSize1);
#ifdef RIFF_SPKR
				RiffPutSamples(pDSLockedBuffer1, dwBufferSize1/sizeof(short));
#endif
				nNumSamples += dwBufferSize1/sizeof(short);
			}
		}

		if(nNumPadSamples)
		{
			//sprintf(szDbg, "[Submit_FS] C=%08X, PC=%08X, WC=%08X, Diff=%08X, Off=%08X, PS=%08X, Data=%04X\n", nDbgSpkrCnt, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumPadSamples, g_nSpeakerData); OutputDebugString(szDbg);

			dwBufferSize0 = dwDSLockedBufferSize0 - dwBufferSize0;
			dwBufferSize1 = dwDSLockedBufferSize1 - dwBufferSize1;

			if(dwBufferSize0)
			{
				wmemset((wchar_t*)pDSLockedBuffer0, (wchar_t)g_nSpeakerData, dwBufferSize0/sizeof(wchar_t));
#ifdef RIFF_SPKR
				RiffPutSamples(pDSLockedBuffer0, dwBufferSize0/sizeof(short));
#endif
			}

			if(pDSLockedBuffer1)
			{
				wmemset((wchar_t*)pDSLockedBuffer1, (wchar_t)g_nSpeakerData, dwBufferSize1/sizeof(wchar_t));
#ifdef RIFF_SPKR
				RiffPutSamples(pDSLockedBuffer1, dwBufferSize1/sizeof(short));
#endif
			}
		}

		// Commit sound buffer
#ifdef APPLE2IX
		hr = SpeakerVoice.lpDSBvoice->Unlock(SpeakerVoice.lpDSBvoice->_this, (void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
#else
		hr = SpeakerVoice.lpDSBvoice->Unlock((void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
#endif
											(void*)pDSLockedBuffer1, dwDSLockedBufferSize1);
		if(FAILED(hr))
			return nNumSamples;

#ifndef APPLE2IX
		dwByteOffset = (dwByteOffset + (DWORD)nNumSamplesToUse*sizeof(short)*g_nSPKR_NumChannels) % g_dwDSSpkrBufferSize;
#endif
	}

	return nNumSamples;
}

//-----------------------------------------------------------------------------

static ULONG Spkr_SubmitWaveBuffer(short* pSpeakerBuffer, ULONG nNumSamples)
{
	char szDbg[200];
	nDbgSpkrCnt++;

	if(!SpeakerVoice.bActive)
		return nNumSamples;

	if(pSpeakerBuffer == NULL)
	{
		// Re-init from SpkrReset()
#ifndef APPLE2IX
            // HACK FIXME TODO : believe this whole if statement is unnecessary?
		dwByteOffset = (DWORD)-1;
#endif
		nNumSamplesError = 0;

		// Don't call DSZeroVoiceBuffer() - get noise with "VIA AC'97 Enhanced Audio Controller"
		// . I guess SpeakerVoice.Stop() isn't really working and the new zero buffer causes noise corruption when submitted.
		DSZeroVoiceWritableBuffer(&SpeakerVoice, "Spkr", g_dwDSSpkrBufferSize);

		return 0;
	}

	//

	DWORD dwDSLockedBufferSize0, dwDSLockedBufferSize1;
	SHORT *pDSLockedBuffer0, *pDSLockedBuffer1;
	bool bBufferError = false;

	DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
#ifdef APPLE2IX
	HRESULT hr = SpeakerVoice.lpDSBvoice->GetCurrentPosition(SpeakerVoice.lpDSBvoice->_this, &dwCurrentPlayCursor, &dwCurrentWriteCursor);
#else
	HRESULT hr = SpeakerVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
#endif
	if(FAILED(hr))
		return nNumSamples;

#if 0
	if(dwByteOffset == (DWORD)-1)
	{
		// First time in this func (probably after re-init (above))

		dwByteOffset = dwCurrentPlayCursor + (g_dwDSSpkrBufferSize/8)*3;	// Ideal: 0.375 is between 0.25 & 0.50 full
		dwByteOffset %= g_dwDSSpkrBufferSize;
		//sprintf(szDbg, "[Submit]   PC=%08X, WC=%08X, Diff=%08X, Off=%08X [REINIT]\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset); OutputDebugString(szDbg);
	}
	else
	{
		// Check that our offset isn't between Play & Write positions

		if(dwCurrentWriteCursor > dwCurrentPlayCursor)
		{
			// |-----PxxxxxW-----|
			if((dwByteOffset > dwCurrentPlayCursor) && (dwByteOffset < dwCurrentWriteCursor))
			{
				double fTicksSecs = (double)GetTickCount() / 1000.0;
#ifdef APPLE2IX
				LOG("%010.3f: [Submit]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X xxx\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
#else
				sprintf(szDbg, "%010.3f: [Submit]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X xxx\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
				OutputDebugString(szDbg);
				if (g_fh) fprintf(g_fh, szDbg);
#endif

				dwByteOffset = dwCurrentWriteCursor;
				nNumSamplesError = 0;
				bBufferError = true;
			}
		}
		else
		{
			// |xxW----------Pxxx|
			if((dwByteOffset > dwCurrentPlayCursor) || (dwByteOffset < dwCurrentWriteCursor))
			{
				double fTicksSecs = (double)GetTickCount() / 1000.0;
#ifdef APPLE2IX
				LOG("%010.3f: [Submit]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X XXX\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
#else
				sprintf(szDbg, "%010.3f: [Submit]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X XXX\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
				OutputDebugString(szDbg);
				if (g_fh) fprintf(g_fh, szDbg);
#endif

				dwByteOffset = dwCurrentWriteCursor;
				nNumSamplesError = 0;
				bBufferError = true;
			}
		}
	}

	// Calc bytes remaining to be played
	int nBytesRemaining = dwByteOffset - dwCurrentPlayCursor;
#else
        int nBytesRemaining = dwCurrentPlayCursor;
#endif
	if(nBytesRemaining < 0)
		nBytesRemaining += g_dwDSSpkrBufferSize;
	if((nBytesRemaining == 0) && (dwCurrentPlayCursor != dwCurrentWriteCursor))
		nBytesRemaining = g_dwDSSpkrBufferSize;		// Case when complete buffer is to be played

	// Calc correction factor so that play-buffer doesn't under/overflow
	const int nErrorInc = SoundCore_GetErrorInc();
	if(nBytesRemaining < g_dwDSSpkrBufferSize / 4)
        {
#ifdef APPLE2IX
            //LOG("execute more cycles due to potential underflow...");
#endif
		nNumSamplesError += nErrorInc;				// < 1/4 of play-buffer remaining (need *more* data)
        }
	else if(nBytesRemaining > g_dwDSSpkrBufferSize / 2)
        {
#ifdef APPLE2IX
            //LOG("execute less cycles due to potential overflow...");
#endif
		nNumSamplesError -= nErrorInc;				// > 1/2 of play-buffer remaining (need *less* data)
        }
	else
        {
		nNumSamplesError = 0;						// Acceptable amount of data in buffer
        }

	const int nErrorMax = SoundCore_GetErrorMax();				// Cap feedback to +/-nMaxError units
	if(nNumSamplesError < -nErrorMax) nNumSamplesError = -nErrorMax;
	if(nNumSamplesError >  nErrorMax) nNumSamplesError =  nErrorMax;
	g_nCpuCyclesFeedback = (int) ((double)nNumSamplesError * g_fClksPerSpkrSample);

	//

	UINT nBytesFree = g_dwDSSpkrBufferSize - nBytesRemaining;	// Calc free buffer space
	ULONG nNumSamplesToUse = nNumSamples;

	if(nNumSamplesToUse * sizeof(short) > nBytesFree)
		nNumSamplesToUse = nBytesFree / sizeof(short);

	if(bBufferError)
		pSpeakerBuffer = &pSpeakerBuffer[nNumSamples - nNumSamplesToUse];

	//

	if(nNumSamplesToUse)
	{
		//sprintf(szDbg, "[Submit]    C=%08X, PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X +++\n", nDbgSpkrCnt, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamplesToUse); OutputDebugString(szDbg);

		if(!DSGetLock(SpeakerVoice.lpDSBvoice,
#ifdef APPLE2IX
							/*unused*/ 0, (DWORD)nNumSamplesToUse*sizeof(short),
#else
							dwByteOffset, (DWORD)nNumSamplesToUse*sizeof(short),
#endif
							&pDSLockedBuffer0, &dwDSLockedBufferSize0,
							&pDSLockedBuffer1, &dwDSLockedBufferSize1))
			return nNumSamples;

		memcpy(pDSLockedBuffer0, &pSpeakerBuffer[0], dwDSLockedBufferSize0);
#ifdef RIFF_SPKR
		RiffPutSamples(pDSLockedBuffer0, dwDSLockedBufferSize0/sizeof(short));
#endif

		if(pDSLockedBuffer1)
		{
			memcpy(pDSLockedBuffer1, &pSpeakerBuffer[dwDSLockedBufferSize0/sizeof(short)], dwDSLockedBufferSize1);
#ifdef RIFF_SPKR
			RiffPutSamples(pDSLockedBuffer1, dwDSLockedBufferSize1/sizeof(short));
#endif
		}

		// Commit sound buffer
#ifdef APPLE2IX
		hr = SpeakerVoice.lpDSBvoice->Unlock(SpeakerVoice.lpDSBvoice->_this, (void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
#else
		hr = SpeakerVoice.lpDSBvoice->Unlock((void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
#endif
											(void*)pDSLockedBuffer1, dwDSLockedBufferSize1);
		if(FAILED(hr))
			return nNumSamples;

#ifndef APPLE2IX
		dwByteOffset = (dwByteOffset + (DWORD)nNumSamplesToUse*sizeof(short)*g_nSPKR_NumChannels) % g_dwDSSpkrBufferSize;
#endif
	}

	return bBufferError ? nNumSamples : nNumSamplesToUse;
}

//-----------------------------------------------------------------------------

void Spkr_Mute()
{
	if(SpeakerVoice.bActive && !SpeakerVoice.bMute)
	{
#ifdef APPLE2IX
		SpeakerVoice.lpDSBvoice->SetVolume(SpeakerVoice.lpDSBvoice->_this, DSBVOLUME_MIN);
#else
		SpeakerVoice.lpDSBvoice->SetVolume(DSBVOLUME_MIN);
#endif
		SpeakerVoice.bMute = true;
	}
}

void Spkr_Demute()
{
	if(SpeakerVoice.bActive && SpeakerVoice.bMute)
	{
#ifdef APPLE2IX
		SpeakerVoice.lpDSBvoice->SetVolume(SpeakerVoice.lpDSBvoice->_this, SpeakerVoice.nVolume);
#else
		SpeakerVoice.lpDSBvoice->SetVolume(SpeakerVoice.nVolume);
#endif
		SpeakerVoice.bMute = false;
	}
}

//-----------------------------------------------------------------------------

static bool g_bSpkrRecentlyActive = false;

static void Spkr_SetActive(bool bActive)
{
	if(!SpeakerVoice.bActive)
		return;

	if(bActive)
	{
		// Called by SpkrToggle() or SpkrReset()
		g_bSpkrRecentlyActive = true;
		SpeakerVoice.bRecentlyActive = true;
	}
	else
	{
		// Called by SpkrUpdate() after 0.2s of speaker inactivity
		g_bSpkrRecentlyActive = false;
		SpeakerVoice.bRecentlyActive = false;
	}
}

bool Spkr_IsActive()
{
	return g_bSpkrRecentlyActive;
}

//-----------------------------------------------------------------------------

DWORD SpkrGetVolume()
{
	return SpeakerVoice.dwUserVolume;
}

void SpkrSetVolume(DWORD dwVolume, DWORD dwVolumeMax)
{
	SpeakerVoice.dwUserVolume = dwVolume;

	SpeakerVoice.nVolume = NewVolume(dwVolume, dwVolumeMax);

	if(SpeakerVoice.bActive)
#ifdef APPLE2IX
		SpeakerVoice.lpDSBvoice->SetVolume(SpeakerVoice.lpDSBvoice->_this, SpeakerVoice.nVolume);
#else
		SpeakerVoice.lpDSBvoice->SetVolume(SpeakerVoice.nVolume);
#endif
}

//=============================================================================

bool Spkr_DSInit()
{
	//
	// Create single Apple speaker voice
	//

	if(!g_bDSAvailable)
		return false;

	SpeakerVoice.bIsSpeaker = true;

	HRESULT hr = DSGetSoundBuffer(&SpeakerVoice, DSBCAPS_CTRLVOLUME, g_dwDSSpkrBufferSize, SPKR_SAMPLE_RATE, 1);
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "Spkr: DSGetSoundBuffer failed (%08X)\n",hr);
		return false;
	}

#ifdef APPLE2IX
        // HACK FIXME TODO : need to revisit whether the below is necessary or not....
	SpeakerVoice.bActive = true;
#else
	if(!DSZeroVoiceBuffer(&SpeakerVoice, "Spkr", g_dwDSSpkrBufferSize))
		return false;

	SpeakerVoice.bActive = true;

	// Volume might've been setup from value in Registry
	if(!SpeakerVoice.nVolume)
		SpeakerVoice.nVolume = DSBVOLUME_MAX;

	SpeakerVoice.lpDSBvoice->SetVolume(SpeakerVoice.nVolume);

	//

	DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
	hr = SpeakerVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
	if(SUCCEEDED(hr) && (dwCurrentPlayCursor == dwCurrentWriteCursor))
	{
		// KLUDGE: For my WinXP PC with "VIA AC'97 Enhanced Audio Controller"
		// . Not required for my Win98SE/WinXP PC with PCI "Soundblaster Live!"
		Sleep(200);

		hr = SpeakerVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
#ifndef APPLE2IX
		char szDbg[100];
		sprintf(szDbg, "[DSInit] PC=%08X, WC=%08X, Diff=%08X\n", dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor); OutputDebugString(szDbg);
#endif
	}

#endif
	return true;
}

void Spkr_DSUninit()
{
	if(SpeakerVoice.lpDSBvoice && SpeakerVoice.bActive)
	{
#ifdef APPLE2IX
		SpeakerVoice.lpDSBvoice->Stop(SpeakerVoice.lpDSBvoice->_this);
#else
		SpeakerVoice.lpDSBvoice->Stop();
#endif
		SpeakerVoice.bActive = false;
	}

	DSReleaseSoundBuffer(&SpeakerVoice);
}

//=============================================================================

DWORD SpkrGetSnapshot(SS_IO_Speaker* pSS)
{
	pSS->g_nSpkrLastCycle = g_nSpkrLastCycle;
	return 0;
}

DWORD SpkrSetSnapshot(SS_IO_Speaker* pSS)
{
	g_nSpkrLastCycle = pSS->g_nSpkrLastCycle;
	return 0;
}

#ifdef APPLE2IX
#       if defined(__GNUC__)
#               pragma GCC diagnostic pop
#       elif defined(__clang__)
#               pragma clang diagnostic pop
#       endif
#endif
