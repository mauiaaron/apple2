/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

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

/* Description: Mockingboard/Phasor emulation
 *
 * Author: Copyright (c) 2002-2006, Tom Charlesworth
 */

#define DSBCAPS_LOCSOFTWARE         0x00000008
#define DSBCAPS_CTRLVOLUME          0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY  0x00000100

#define DSBVOLUME_MIN               -10000
#define DSBVOLUME_MAX               0


// History:
//
// v1.12.07.1 (30 Dec 2005)
// - Update 6522 TIMERs after every 6502 opcode, giving more precise IRQs
// - Minimum TIMER freq is now 0x100 cycles
// - Added Phasor support
//
// v1.12.06.1 (16 July 2005)
// - Reworked 6522's ORB -> AY8910 decoder
// - Changed MB output so L=All voices from AY0 & AY2 & R=All voices from AY1 & AY3
// - Added crude support for Votrax speech chip (by using SSI263 phonemes)
//
// v1.12.04.1 (14 Sep 2004)
// - Switch MB output from dual-mono to stereo.
// - Relaxed TIMER1 freq from ~62Hz (period=0x4000) to ~83Hz (period=0x3000).
//
// 25 Apr 2004:
// - Added basic support for the SSI263 speech chip
//
// 15 Mar 2004:
// - Switched to MAME's AY8910 emulation (includes envelope support)
//
// v1.12.03 (11 Jan 2004)
// - For free-running 6522 timer1 IRQ, reload with current ACCESS_TIMER1 value.
//   (Fixes Ultima 4/5 playback speed problem.)
//
// v1.12.01 (24 Nov 2002)
// - Shaped the tone waveform more logarithmically
// - Added support for MB ena/dis switch on Config dialog
// - Added log file support
//
// v1.12.00 (17 Nov 2002)
// - Initial version (no AY8910 envelope support)
//

// Notes on Votrax chip (on original Mockingboards):
// From Crimewave (Penguin Software):
// . Init:
//   . DDRB = 0xFF
//   . PCR  = 0xB0
//   . IER  = 0x90
//   . ORB  = 0x03 (PAUSE0) or 0x3F (STOP)
// . IRQ:
//   . ORB  = Phoneme value
// . IRQ last phoneme complete:
//   . IER  = 0x10
//   . ORB  = 0x3F (STOP)
//

#include "common.h"
#ifdef APPLE2IX
#       if defined(__linux) && !defined(ANDROID)
#       include <sys/io.h>
#       endif

#if defined(FAILED)
#undef FAILED
#endif
static inline bool FAILED(int x) { return x != 0; }

#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define STILL_ACTIVE 259
extern bool GetExitCodeThread(pthread_t hThread, unsigned long *lpExitCode);
extern pthread_t CreateThread(void* unused_lpThreadAttributes, int unused_dwStackSize, void *(*lpStartAddress)(void *unused), void *lpParameter, unsigned long unused_dwCreationFlags, unsigned long *lpThreadId);
extern bool SetThreadPriority(pthread_t hThread, int nPriority);
#else
#include "StdAfx.h"
#endif



#define LOG_SSI263 0

#include <wchar.h>

#include "audio/AY8910.h"
#include "audio/SSI263Phonemes.h"


#define SY6522_DEVICE_A 0
#define SY6522_DEVICE_B 1

#define SLOT4 4
#define SLOT5 5

#define NUM_MB 2
#define NUM_DEVS_PER_MB 2
#define NUM_AY8910 (NUM_MB*NUM_DEVS_PER_MB)
#define NUM_SY6522 NUM_AY8910
#define NUM_VOICES_PER_AY8910 3
#define NUM_VOICES (NUM_AY8910*NUM_VOICES_PER_AY8910)


// Chip offsets from card base.
#define SY6522A_Offset	0x00
#define SY6522B_Offset	0x80
#define SSI263_Offset	0x40

#define Phasor_SY6522A_CS		4
#define Phasor_SY6522B_CS		7
#define Phasor_SY6522A_Offset	(1<<Phasor_SY6522A_CS)
#define Phasor_SY6522B_Offset	(1<<Phasor_SY6522B_CS)

typedef struct
{
	SY6522 sy6522;
	uint8_t nAY8910Number;
	uint8_t nAYCurrentRegister;
	uint8_t nTimerStatus;
	SSI263A SpeechChip;
} SY6522_AY8910;


// IFR & IER:
#define IxR_PERIPHERAL	(1<<1)
#define IxR_VOTRAX		(1<<4)	// TO DO: Get proper name from 6522 datasheet!
#define IxR_TIMER2		(1<<5)
#define IxR_TIMER1		(1<<6)

// ACR:
#define RUNMODE		(1<<6)	// 0 = 1-Shot Mode, 1 = Free Running Mode
#define RM_ONESHOT		(0<<6)
#define RM_FREERUNNING	(1<<6)


// SSI263A registers:
#define SSI_DURPHON	0x00
#define SSI_INFLECT	0x01
#define SSI_RATEINF	0x02
#define SSI_CTTRAMP	0x03
#define SSI_FILFREQ	0x04


// Support 2 MB's, each with 2x SY6522/AY8910 pairs.
static SY6522_AY8910 g_MB[NUM_AY8910] = { 0 };

// Timer vars
static unsigned long g_n6522TimerPeriod = 0;
#ifdef APPLE2IX
#define TIMERDEVICE_INVALID -1
#else
static const unsigned int TIMERDEVICE_INVALID = -1;
#endif
static unsigned int g_nMBTimerDevice = TIMERDEVICE_INVALID;	// SY6522 device# which is generating timer IRQ
static uint64_t g_uLastCumulativeCycles = 0;

// SSI263 vars:
static uint16_t g_nSSI263Device = 0;	// SSI263 device# which is generating phoneme-complete IRQ
static int g_nCurrentActivePhoneme = -1;
static bool g_bStopPhoneme = false;
static bool g_bVotraxPhoneme = false;

#ifdef APPLE2IX
static unsigned long SAMPLE_RATE = 0;
static float samplesScale = 1.f;
#else
static const unsigned long SAMPLE_RATE = 44100;	// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample
#endif

static short* ppAYVoiceBuffer[NUM_VOICES] = {0};

#ifdef APPLE2IX
bool g_bDisableDirectSoundMockingboard = false;
static uint64_t	g_nMB_InActiveCycleCount = 0;
#else
static uint64_t	g_nMB_InActiveCycleCount = 0;
#endif
static bool g_bMB_RegAccessedFlag = false;
static bool g_bMB_Active = false;

#ifdef APPLE2IX
//static pthread_t mockingboard_thread = (pthread_t)-1;
static pthread_t g_hThread = 0;
#else
static void *g_hThread = NULL;
#endif

static bool g_bMBAvailable = false;

//

static SS_CARDTYPE g_SoundcardType = CT_Empty;	// Use CT_Empty to mean: no soundcard
static bool g_bPhasorEnable = false;
static uint8_t g_nPhasorMode = 0;	// 0=Mockingboard emulation, 1=Phasor native

//-------------------------------------

#ifdef APPLE2IX
#define MB_CHANNELS 2
static unsigned long MB_BUF_SIZE = 0;
static unsigned short g_nMB_NumChannels = MB_CHANNELS;
static unsigned long g_dwDSBufferSize = 0;
#else
static const unsigned short g_nMB_NumChannels = 2;

static const unsigned long g_dwDSBufferSize = MAX_SAMPLES * sizeof(short) * g_nMB_NumChannels;
#endif

static const int16_t nWaveDataMin = (int16_t)0x8000;
static const int16_t nWaveDataMax = (int16_t)0x7FFF;

#ifdef APPLE2IX
static short *g_nMixBuffer = NULL;
#else
static short g_nMixBuffer[g_dwDSBufferSize / sizeof(short)];
#endif


static AudioBuffer_s *MockingboardVoice = NULL;

// HACK FIXME TODO : why 64?  do we really need this much?!
#define MAX_VOICES 64
static AudioBuffer_s *SSI263Voice[MAX_VOICES] = { 0 };

#ifdef APPLE2IX
static pthread_cond_t mockingboard_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mockingboard_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t quit_event = false;
#else
static const int g_nNumEvents = 2;
static void *g_hSSI263Event[g_nNumEvents] = {NULL};	// 1: Phoneme finished playing, 2: Exit thread
static unsigned long g_dwMaxPhonemeLen = 0;
#endif

// When 6522 IRQ is *not* active use 60Hz update freq for MB voices
static const double g_f6522TimerPeriod_NoIRQ = CLK_6502 / 60.0;		// Constant whatever the CLK is set to

//---------------------------------------------------------------------------

// External global vars:
bool g_bMBTimerIrqActive = false;
#ifdef _DEBUG
uint32_t g_uTimer1IrqCount = 0;	// DEBUG
#endif

//---------------------------------------------------------------------------

// Forward refs:
#ifdef APPLE2IX
static void* SSI263Thread(void *);
#else
static unsigned long SSI263Thread(void *);
#endif
static void Votrax_Write(uint8_t nDevice, uint8_t nValue);

#ifdef APPLE2IX
//---------------------------------------------------------------------------
// Windows Shim Code ...

pthread_t CreateThread(void* unused_lpThreadAttributes, int unused_dwStackSize, void *(*lpStartRoutine)(void *stuff), void *lpParameter, unsigned long unused_dwCreationFlags, unsigned long *lpThreadId)
{
    pthread_t a_thread = 0;
    int err = 0;
    if ((err = pthread_create(&a_thread, NULL, lpStartRoutine, lpParameter)))
    {
        ERRLOG("pthread_create");
    }

    return a_thread; 
}

bool SetThreadPriority(pthread_t thread, int unused_nPriority)
{
    // assuming time critical ...
#if defined(__APPLE__) || defined(ANDROID)
#warning possible FIXME possible TODO : set thread priority in Darwin/Mach ?
#else
    int policy = sched_getscheduler(getpid());

    int prio = 0;
    if ((prio = sched_get_priority_max(policy)) < 0)
    {
        ERRLOG("OOPS sched_get_priority_max");
        return 0;
    }

    int err = 0;
    if ((err = pthread_setschedprio(thread, prio)))
    {
        ERRLOG("OOPS pthread_setschedprio");
        return 0;
    }
#endif

    return 1;
}

bool GetExitCodeThread(pthread_t thread, unsigned long *lpExitCode)
{
#if defined(__APPLE__) || defined(ANDROID)
    int err = 0;
    if ( (err = pthread_join(thread, NULL)) ) {
        ERRLOG("OOPS pthread_join");
    }
    if (lpExitCode) {
        *lpExitCode = err;
    }
#else
    if (pthread_tryjoin_np(thread, NULL))
    {
        if (lpExitCode)
        {
            *lpExitCode = STILL_ACTIVE;
        }
    }
    else if (lpExitCode)
    {
        *lpExitCode = 0;
    }
#endif
    return 1;
}
#endif

//---------------------------------------------------------------------------

static void StartTimer(SY6522_AY8910* pMB)
{
	if((pMB->nAY8910Number & 1) != SY6522_DEVICE_A)
		return;

	if((pMB->sy6522.IER & IxR_TIMER1) == 0x00)
		return;

	uint16_t nPeriod = pMB->sy6522.TIMER1_LATCH.w;

//	if(nPeriod <= 0xff)		// Timer1L value has been written (but TIMER1H hasn't)
//		return;

	pMB->nTimerStatus = 1;

	// 6522 CLK runs at same speed as 6502 CLK
	g_n6522TimerPeriod = nPeriod;

	g_bMBTimerIrqActive = true;
	g_nMBTimerDevice = pMB->nAY8910Number;
}

//-----------------------------------------------------------------------------

static void StopTimer(SY6522_AY8910* pMB)
{
	pMB->nTimerStatus = 0;
	g_bMBTimerIrqActive = false;
	g_nMBTimerDevice = TIMERDEVICE_INVALID;
}

//-----------------------------------------------------------------------------

static void ResetSY6522(SY6522_AY8910* pMB)
{
	memset(&pMB->sy6522,0,sizeof(SY6522));

	if(pMB->nTimerStatus)
		StopTimer(pMB);

	pMB->nAYCurrentRegister = 0;
}

//-----------------------------------------------------------------------------

static void AY8910_Write(uint8_t nDevice, uint8_t nReg, uint8_t nValue, uint8_t nAYDevice)
{
	g_bMB_RegAccessedFlag = true;
	SY6522_AY8910* pMB = &g_MB[nDevice];

	if((nValue & 4) == 0)
	{
		// RESET: Reset AY8910 only
		AY8910_reset(nDevice+2*nAYDevice);
	}
	else
	{
		// Determine the AY8910 inputs
		int nBDIR = (nValue & 2) ? 1 : 0;
		const int nBC2 = 1;		// Hardwired to +5V
		int nBC1 = nValue & 1;

		int nAYFunc = (nBDIR<<2) | (nBC2<<1) | nBC1;
		enum {AY_NOP0, AY_NOP1, AY_INACTIVE, AY_READ, AY_NOP4, AY_NOP5, AY_WRITE, AY_LATCH};

		switch(nAYFunc)
		{
			case AY_INACTIVE:	// 4: INACTIVE
				break;

			case AY_READ:		// 5: READ FROM PSG (need to set DDRA to input)
				break;

			case AY_WRITE:		// 6: WRITE TO PSG
				_AYWriteReg(nDevice+2*nAYDevice, pMB->nAYCurrentRegister, pMB->sy6522.ORA);
				break;

			case AY_LATCH:		// 7: LATCH ADDRESS
				// http://www.worldofspectrum.org/forums/showthread.php?t=23327
				// Selecting an unused register number above 0x0f puts the AY into a state where
				// any values written to the data/address bus are ignored, but can be read back
				// within a few tens of thousands of cycles before they decay to zero.
				if(pMB->sy6522.ORA <= 0x0F)
					pMB->nAYCurrentRegister = pMB->sy6522.ORA & 0x0F;
				// else Pro-Mockingboard (clone from HK)
				break;
		}
	}
}

static void UpdateIFR(SY6522_AY8910* pMB)
{
	pMB->sy6522.IFR &= 0x7F;

	if(pMB->sy6522.IFR & pMB->sy6522.IER & 0x7F)
		pMB->sy6522.IFR |= 0x80;

	// Now update the IRQ signal from all 6522s
	// . OR-sum of all active TIMER1, TIMER2 & SPEECH sources (from all 6522s)
	unsigned int bIRQ = 0;
	for(unsigned int i=0; i<NUM_SY6522; i++)
		bIRQ |= g_MB[i].sy6522.IFR & 0x80;

	// NB. Mockingboard generates IRQ on both 6522s:
	// . SSI263's IRQ (A/!R) is routed via the 2nd 6522 (at $Cx80) and must generate a 6502 IRQ (not NMI)
	// . SC-01's IRQ (A/!R) is also routed via a (2nd?) 6522
	// Phasor's SSI263 appears to be wired directly to the 6502's IRQ (ie. not via a 6522)
	// . I assume Phasor's 6522s just generate 6502 IRQs (not NMIs)

	if (bIRQ)
	{
#ifdef APPLE2IX
            cpu65_interrupt(IS_6522);
#else
	    CpuIrqAssert(IS_6522);
#endif
	}
	else
	{
#ifdef APPLE2IX
            cpu65_uninterrupt(IS_6522);
#else
	    CpuIrqDeassert(IS_6522);
#endif
	}
}

static void SY6522_Write(uint8_t nDevice, uint8_t nReg, uint8_t nValue)
{
	g_bMB_Active = true;

	SY6522_AY8910* pMB = &g_MB[nDevice];

	switch (nReg)
	{
		case 0x00:	// ORB
			{
				nValue &= pMB->sy6522.DDRB;
				pMB->sy6522.ORB = nValue;

				if( (pMB->sy6522.DDRB == 0xFF) && (pMB->sy6522.PCR == 0xB0) )
				{
					// Votrax speech data
					Votrax_Write(nDevice, nValue);
					break;
				}

				if(g_bPhasorEnable)
				{
					int nAY_CS = (g_nPhasorMode & 1) ? (~(nValue >> 3) & 3) : 1;

					if(nAY_CS & 1)
						AY8910_Write(nDevice, nReg, nValue, 0);

					if(nAY_CS & 2)
						AY8910_Write(nDevice, nReg, nValue, 1);
				}
				else
				{
					AY8910_Write(nDevice, nReg, nValue, 0);
				}

				break;
			}
		case 0x01:	// ORA
			pMB->sy6522.ORA = nValue & pMB->sy6522.DDRA;
			break;
		case 0x02:	// DDRB
			pMB->sy6522.DDRB = nValue;
			break;
		case 0x03:	// DDRA
			pMB->sy6522.DDRA = nValue;
			break;
		case 0x04:	// TIMER1L_COUNTER
		case 0x06:	// TIMER1L_LATCH
			pMB->sy6522.TIMER1_LATCH.l = nValue;
			break;
		case 0x05:	// TIMER1H_COUNTER
			/* Initiates timer1 & clears time-out of timer1 */

			// Clear Timer Interrupt Flag.
			pMB->sy6522.IFR &= ~IxR_TIMER1;
			UpdateIFR(pMB);

			pMB->sy6522.TIMER1_LATCH.h = nValue;
			pMB->sy6522.TIMER1_COUNTER.w = pMB->sy6522.TIMER1_LATCH.w;

			StartTimer(pMB);
			break;
		case 0x07:	// TIMER1H_LATCH
			// Clear Timer1 Interrupt Flag.
			pMB->sy6522.TIMER1_LATCH.h = nValue;
			pMB->sy6522.IFR &= ~IxR_TIMER1;
			UpdateIFR(pMB);
			break;
		case 0x08:	// TIMER2L
			pMB->sy6522.TIMER2_LATCH.l = nValue;
			break;
		case 0x09:	// TIMER2H
			// Clear Timer2 Interrupt Flag.
			pMB->sy6522.IFR &= ~IxR_TIMER2;
			UpdateIFR(pMB);

			pMB->sy6522.TIMER2_LATCH.h = nValue;
			pMB->sy6522.TIMER2_COUNTER.w = pMB->sy6522.TIMER2_LATCH.w;
			break;
		case 0x0a:	// SERIAL_SHIFT
			break;
		case 0x0b:	// ACR
			pMB->sy6522.ACR = nValue;
			break;
		case 0x0c:	// PCR -  Used for Speech chip only
			pMB->sy6522.PCR = nValue;
			break;
		case 0x0d:	// IFR
			// - Clear those bits which are set in the lower 7 bits.
			// - Can't clear bit 7 directly.
			nValue |= 0x80;	// Set high bit
			nValue ^= 0x7F;	// Make mask
			pMB->sy6522.IFR &= nValue;
			UpdateIFR(pMB);
			break;
		case 0x0e:	// IER
			if(!(nValue & 0x80))
			{
				// Clear those bits which are set in the lower 7 bits.
				nValue ^= 0x7F;
				pMB->sy6522.IER &= nValue;
				UpdateIFR(pMB);
				
				// Check if timer has been disabled.
				if(pMB->sy6522.IER & IxR_TIMER1)
					break;

				if(pMB->nTimerStatus == 0)
					break;
				
				// Stop timer
				StopTimer(pMB);
			}
			else
			{
				// Set those bits which are set in the lower 7 bits.
				nValue &= 0x7F;
				pMB->sy6522.IER |= nValue;
				UpdateIFR(pMB);
				StartTimer(pMB);
			}
			break;
		case 0x0f:	// ORA_NO_HS
			break;
	}
}

//-----------------------------------------------------------------------------

static uint8_t SY6522_Read(uint8_t nDevice, uint8_t nReg)
{
//	g_bMB_RegAccessedFlag = true;
	g_bMB_Active = true;

	SY6522_AY8910* pMB = &g_MB[nDevice];
	uint8_t nValue = 0x00;

	switch (nReg)
	{
		case 0x00:	// ORB
			nValue = pMB->sy6522.ORB;
			break;
		case 0x01:	// ORA
			nValue = pMB->sy6522.ORA;
			break;
		case 0x02:	// DDRB
			nValue = pMB->sy6522.DDRB;
			break;
		case 0x03:	// DDRA
			nValue = pMB->sy6522.DDRA;
			break;
		case 0x04:	// TIMER1L_COUNTER
			nValue = pMB->sy6522.TIMER1_COUNTER.l;
			pMB->sy6522.IFR &= ~IxR_TIMER1;		// Also clears Timer1 Interrupt Flag
			UpdateIFR(pMB);
			break;
		case 0x05:	// TIMER1H_COUNTER
			nValue = pMB->sy6522.TIMER1_COUNTER.h;
			break;
		case 0x06:	// TIMER1L_LATCH
			nValue = pMB->sy6522.TIMER1_LATCH.l;
			break;
		case 0x07:	// TIMER1H_LATCH
			nValue = pMB->sy6522.TIMER1_LATCH.h;
			break;
		case 0x08:	// TIMER2L
			nValue = pMB->sy6522.TIMER2_COUNTER.l;
			pMB->sy6522.IFR &= ~IxR_TIMER2;		// Also clears Timer2 Interrupt Flag
			UpdateIFR(pMB);
			break;
		case 0x09:	// TIMER2H
			nValue = pMB->sy6522.TIMER2_COUNTER.h;
			break;
		case 0x0a:	// SERIAL_SHIFT
			break;
		case 0x0b:	// ACR
			nValue = pMB->sy6522.ACR;
			break;
		case 0x0c:	// PCR
			nValue = pMB->sy6522.PCR;
			break;
		case 0x0d:	// IFR
			nValue = pMB->sy6522.IFR;
			break;
		case 0x0e:	// IER
			nValue = 0x80;	// Datasheet says this is 0x80|IER
			break;
		case 0x0f:	// ORA_NO_HS
			nValue = pMB->sy6522.ORA;
			break;
	}

	return nValue;
}

//---------------------------------------------------------------------------

#ifdef APPLE2IX
static void SSI263_Play(unsigned int nPhoneme);
#else
void SSI263_Play(unsigned int nPhoneme);
#endif

#if 0
typedef struct
{
	uint8_t DurationPhonome;
	uint8_t Inflection;		// I10..I3
	uint8_t RateInflection;
	uint8_t CtrlArtAmp;
	uint8_t FilterFreq;
	//
	uint8_t CurrentMode;
} SSI263A;
#endif

//static SSI263A nSpeechChip;

// Duration/Phonome
const uint8_t DURATION_MODE_MASK = 0xC0;
const uint8_t PHONEME_MASK = 0x3F;

const uint8_t MODE_PHONEME_TRANSITIONED_INFLECTION = 0xC0;	// IRQ active
const uint8_t MODE_PHONEME_IMMEDIATE_INFLECTION = 0x80;	// IRQ active
const uint8_t MODE_FRAME_IMMEDIATE_INFLECTION = 0x40;		// IRQ active
const uint8_t MODE_IRQ_DISABLED = 0x00;

// Rate/Inflection
const uint8_t RATE_MASK = 0xF0;
const uint8_t INFLECTION_MASK_H = 0x08;	// I11
const uint8_t INFLECTION_MASK_L = 0x07;	// I2..I0

// Ctrl/Art/Amp
const uint8_t CONTROL_MASK = 0x80;
const uint8_t ARTICULATION_MASK = 0x70;
const uint8_t AMPLITUDE_MASK = 0x0F;

static uint8_t SSI263_Read(uint8_t nDevice, uint8_t nReg)
{
	SY6522_AY8910* pMB = &g_MB[nDevice];

	// Regardless of register, just return inverted A/!R in bit7
	// . A/!R is low for IRQ

	return pMB->SpeechChip.CurrentMode << 7;
}

static void SSI263_Write(uint8_t nDevice, uint8_t nReg, uint8_t nValue)
{
	SY6522_AY8910* pMB = &g_MB[nDevice];

	switch(nReg)
	{
	case SSI_DURPHON:
#if LOG_SSI263
		LOG("DUR   = 0x%02X, PHON = 0x%02X\n\n", nValue>>6, nValue&PHONEME_MASK);
#endif

		// Datasheet is not clear, but a write to DURPHON must clear the IRQ
		if(g_bPhasorEnable)
		{
#ifdef APPLE2IX
                    cpu65_uninterrupt(IS_SPEECH);
#else
		    CpuIrqDeassert(IS_SPEECH);
#endif
		}
		else
		{
			pMB->sy6522.IFR &= ~IxR_PERIPHERAL;
			UpdateIFR(pMB);
		}
		pMB->SpeechChip.CurrentMode &= ~1;	// Clear SSI263's D7 pin

		pMB->SpeechChip.DurationPhonome = nValue;

		g_nSSI263Device = nDevice;

		// Phoneme output not dependent on CONTROL bit
		if(g_bPhasorEnable)
		{
			if(nValue || (g_nCurrentActivePhoneme<0))
				SSI263_Play(nValue & PHONEME_MASK);
		}
		else
		{
			SSI263_Play(nValue & PHONEME_MASK);
		}
		break;
	case SSI_INFLECT:
#if LOG_SSI263
		LOG("INF   = 0x%02X\n", nValue);
#endif
		pMB->SpeechChip.Inflection = nValue;
		break;
	case SSI_RATEINF:
#if LOG_SSI263
		LOG("RATE  = 0x%02X, INF = 0x%02X\n", nValue>>4, nValue&0x0F);
#endif
		pMB->SpeechChip.RateInflection = nValue;
		break;
	case SSI_CTTRAMP:
#if LOG_SSI263
		LOG("CTRL  = %d, ART = 0x%02X, AMP=0x%02X\n", nValue>>7, (nValue&ARTICULATION_MASK)>>4, nValue&AMPLITUDE_MASK);
#endif
		if((pMB->SpeechChip.CtrlArtAmp & CONTROL_MASK) && !(nValue & CONTROL_MASK))	// H->L
			pMB->SpeechChip.CurrentMode = pMB->SpeechChip.DurationPhonome & DURATION_MODE_MASK;
		pMB->SpeechChip.CtrlArtAmp = nValue;
		break;
	case SSI_FILFREQ:
#if LOG_SSI263
		LOG("FFREQ = 0x%02X\n", nValue);
#endif
		pMB->SpeechChip.FilterFreq = nValue;
		break;
	default:
		break;
	}
}

//-------------------------------------

static uint8_t Votrax2SSI263[MAX_VOICES] = 
{
	0x02,	// 00: EH3 jackEt -> E1 bEnt
	0x0A,	// 01: EH2 Enlist -> EH nEst
	0x0B,	// 02: EH1 hEAvy -> EH1 bElt
	0x00,	// 03: PA0 no sound -> PA
	0x28,	// 04: DT buTTer -> T Tart
	0x08,	// 05: A2 mAde -> A mAde
	0x08,	// 06: A1 mAde -> A mAde
	0x2F,	// 07: ZH aZure -> Z Zero
	0x0E,	// 08: AH2 hOnest -> AH gOt
	0x07,	// 09: I3 inhibIt -> I sIx
	0x07,	// 0A: I2 Inhibit -> I sIx
	0x07,	// 0B: I1 inhIbit -> I sIx
	0x37,	// 0C: M Mat -> More
	0x38,	// 0D: N suN -> N NiNe
	0x24,	// 0E: B Bag -> B Bag
	0x33,	// 0F: V Van -> V Very
	//
	0x32,	// 10: CH* CHip -> SCH SHip (!)
	0x32,	// 11: SH SHop ->  SCH SHip
	0x2F,	// 12: Z Zoo -> Z Zero
	0x10,	// 13: AW1 lAWful -> AW Office
	0x39,	// 14: NG thiNG -> NG raNG
	0x0F,	// 15: AH1 fAther -> AH1 fAther
	0x13,	// 16: OO1 lOOking -> OO lOOk
	0x13,	// 17: OO bOOK -> OO lOOk
	0x20,	// 18: L Land -> L Lift
	0x29,	// 19: K triCK -> Kit
	0x25,	// 1A: J* juDGe -> D paiD (!)
	0x2C,	// 1B: H Hello -> HF Heart
	0x26,	// 1C: G Get -> KV taG
	0x34,	// 1D: F Fast -> F Four
	0x25,	// 1E: D paiD -> D paiD
	0x30,	// 1F: S paSS -> S Same
	//
	0x08,	// 20: A dAY -> A mAde
	0x09,	// 21: AY dAY -> AI cAre
	0x03,	// 22: Y1 Yard -> YI Year
	0x1B,	// 23: UH3 missIOn -> UH3 nUt
	0x0E,	// 24: AH mOp -> AH gOt
	0x27,	// 25: P Past -> P Pen
	0x11,	// 26: O cOld -> O stOre
	0x07,	// 27: I pIn -> I sIx
	0x16,	// 28: U mOve -> U tUne
	0x05,	// 29: Y anY -> AY plEAse
	0x28,	// 2A: T Tap -> T Tart
	0x1D,	// 2B: R Red -> R Roof
	0x01,	// 2C: E mEEt -> E mEEt
	0x23,	// 2D: W Win -> W Water
	0x0C,	// 2E: AE dAd -> AE dAd
	0x0D,	// 2F: AE1 After -> AE1 After
	//
	0x10,	// 30: AW2 sAlty -> AW Office
	0x1A,	// 31: UH2 About -> UH2 whAt
	0x19,	// 32: UH1 Uncle -> UH1 lOve
	0x18,	// 33: UH cUp -> UH wOnder
	0x11,	// 34: O2 fOr -> O stOre
	0x11,	// 35: O1 abOArd -> O stOre
	0x14,	// 36: IU yOU -> IU yOU
	0x14,	// 37: U1 yOU -> IU yOU
	0x35,	// 38: THV THe -> THV THere
	0x36,	// 39: TH THin -> TH wiTH
	0x1C,	// 3A: ER bIrd -> ER bIrd
	0x0A,	// 3B: EH gEt -> EH nEst
	0x01,	// 3C: E1 bE -> E mEEt
	0x10,	// 3D: AW cAll -> AW Office
	0x00,	// 3E: PA1 no sound -> PA
	0x00,	// 3F: STOP no sound -> PA
};

static void Votrax_Write(uint8_t nDevice, uint8_t nValue)
{
	g_bVotraxPhoneme = true;

	// !A/R: Acknowledge receipt of phoneme data (signal goes from high to low)
	SY6522_AY8910* pMB = &g_MB[nDevice];
	pMB->sy6522.IFR &= ~IxR_VOTRAX;
	UpdateIFR(pMB);

	g_nSSI263Device = nDevice;

	SSI263_Play(Votrax2SSI263[nValue & PHONEME_MASK]);
}

//===========================================================================

static void MB_Update()
{
#ifdef APPLE2IX
    if (!audio_isAvailable) {
        return;
    }

        static int nNumSamplesError = 0;
        if (!MockingboardVoice)
        {
            nNumSamplesError = 0;
            return;
        }

        if (!MockingboardVoice->bActive || !g_bMB_Active)
        {
            nNumSamplesError = 0;
            return;
        }
#else
	char szDbg[200];
	if (!MockingboardVoice->bActive)
		return;
#endif

	if (is_fullspeed)
	{
		// Keep AY reg writes relative to the current 'frame'
		// - Required for Ultima3:
		//   . Tune ends
		//   . is_fullspeed:=true (disk-spinning) for ~50 frames
		//   . U3 sets AY_ENABLE:=0xFF (as a side-effect, this sets is_fullspeed:=false)
		//   o Without this, the write to AY_ENABLE gets ignored (since AY8910's /g_uLastCumulativeCycles/ was last set 50 frame ago)
		AY8910UpdateSetCycles();

		// TODO:
		// If any AY regs have changed then push them out to the AY chip

		return;
	}

	//

	if (!g_bMB_RegAccessedFlag)
	{
		if(!g_nMB_InActiveCycleCount)
		{
			g_nMB_InActiveCycleCount = cycles_count_total;
		}
#ifdef APPLE2IX
		else if(cycles_count_total - g_nMB_InActiveCycleCount > (uint64_t)cycles_persec_target/10)
#else
		else if(cycles_count_total - g_nMB_InActiveCycleCount > (uint64_t)cycles_persec_target/10)
#endif
		{
			// After 0.1 sec of Apple time, assume MB is not active
			g_bMB_Active = false;
		}
	}
	else
	{
		g_nMB_InActiveCycleCount = 0;
		g_bMB_RegAccessedFlag = false;
		g_bMB_Active = true;
	}

	//

#ifndef APPLE2IX
	static unsigned long dwByteOffset = (unsigned long)-1;
	static int nNumSamplesError = 0;
#endif

	const double n6522TimerPeriod = MB_GetFramePeriod();

	const double nIrqFreq = cycles_persec_target / n6522TimerPeriod + 0.5;			// Round-up
	const int nNumSamplesPerPeriod = (int) ((double)SAMPLE_RATE / nIrqFreq);	// Eg. For 60Hz this is 735
	int nNumSamples = nNumSamplesPerPeriod + nNumSamplesError;					// Apply correction
	if(nNumSamples <= 0)
		nNumSamples = 0;
	if(nNumSamples > 2*nNumSamplesPerPeriod)
		nNumSamples = 2*nNumSamplesPerPeriod;

	if(nNumSamples)
		for(int nChip=0; nChip<NUM_AY8910; nChip++)
			AY8910Update(nChip, &ppAYVoiceBuffer[nChip*NUM_VOICES_PER_AY8910], nNumSamples);

	//

	unsigned long dwDSLockedBufferSize0 = 0;
	int16_t *pDSLockedBuffer0 = NULL;

	unsigned long dwCurrentPlayCursor, dwCurrentWriteCursor;
#ifdef APPLE2IX
        //dwCurrentWriteCursor = 0;
	int hr = MockingboardVoice->GetCurrentPosition(MockingboardVoice, &dwCurrentPlayCursor);
#else
	int hr = MockingboardVoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
#endif
	if(FAILED(hr))
		return;

#if 0
	if(dwByteOffset == (unsigned long)-1)
	{
		// First time in this func

		dwByteOffset = dwCurrentWriteCursor;
	}
	else
	{
		// Check that our offset isn't between Play & Write positions

		if(dwCurrentWriteCursor > dwCurrentPlayCursor)
		{
			// |-----PxxxxxW-----|
			if((dwByteOffset > dwCurrentPlayCursor) && (dwByteOffset < dwCurrentWriteCursor))
			{
#ifndef APPLE2IX
				double fTicksSecs = (double)GetTickCount() / 1000.0;
				sprintf(szDbg, "%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X xxx\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
				OutputDebugString(szDbg);
				if (g_fh) fprintf(g_fh, szDbg);
#endif

				dwByteOffset = dwCurrentWriteCursor;
			}
		}
		else
		{
			// |xxW----------Pxxx|
			if((dwByteOffset > dwCurrentPlayCursor) || (dwByteOffset < dwCurrentWriteCursor))
			{
#ifndef APPLE2IX
				double fTicksSecs = (double)GetTickCount() / 1000.0;
				sprintf(szDbg, "%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X XXX\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
				OutputDebugString(szDbg);
				if (g_fh) fprintf(g_fh, szDbg);
#endif

				dwByteOffset = dwCurrentWriteCursor;
			}
		}
	}

	int nBytesRemaining = dwByteOffset - dwCurrentPlayCursor;
#else
	int nBytesRemaining = (int)dwCurrentPlayCursor;
        //LOG("Mockingboard : sound buffer position : %d", nBytesRemaining);
#endif
	if(nBytesRemaining < 0)
		nBytesRemaining += g_dwDSBufferSize;

	// Calc correction factor so that play-buffer doesn't under/overflow
	if((unsigned int)nBytesRemaining < g_dwDSBufferSize / 4)
		nNumSamplesError += SOUNDCORE_ERROR_INC;				// < 0.25 of buffer remaining
	else if((unsigned int)nBytesRemaining > g_dwDSBufferSize / 2)
		nNumSamplesError -= SOUNDCORE_ERROR_INC;				// > 0.50 of buffer remaining
	else
		nNumSamplesError = 0;						// Acceptable amount of data in buffer

#ifdef APPLE2IX
	if(nNumSamplesError < -SOUNDCORE_ERROR_MAX) nNumSamplesError = -SOUNDCORE_ERROR_MAX;
	if(nNumSamplesError >  SOUNDCORE_ERROR_MAX) nNumSamplesError =  SOUNDCORE_ERROR_MAX;

        static time_t dbg_print = 0;
        time_t now = time(NULL);
        if (dbg_print != now)
        {
            dbg_print = now;
            LOG("cycles_speaker_feedback:%d nNumSamplesError:%d n6522TimerPeriod:%f nIrqFreq:%f nNumSamplesPerPeriod:%d nNumSamples:%d nBytesRemaining:%d ", cycles_speaker_feedback, nNumSamplesError, n6522TimerPeriod, nIrqFreq, nNumSamplesPerPeriod, nNumSamples, nBytesRemaining);
        }
#endif

	const double fAttenuation = g_bPhasorEnable ? 2.0/3.0 : 1.0;

	for(int i=0; i<nNumSamples; i++)
	{
		// Mockingboard stereo (all voices on an AY8910 wire-or'ed together)
		// L = Address.b7=0, R = Address.b7=1
		int nDataL = 0, nDataR = 0;

		for(unsigned int j=0; j<NUM_VOICES_PER_AY8910; j++)
		{
			// Slot4
			nDataL += (int) ((double)ppAYVoiceBuffer[0*NUM_VOICES_PER_AY8910+j][i] * fAttenuation);
			nDataR += (int) ((double)ppAYVoiceBuffer[1*NUM_VOICES_PER_AY8910+j][i] * fAttenuation);

			// Slot5
			nDataL += (int) ((double)ppAYVoiceBuffer[2*NUM_VOICES_PER_AY8910+j][i] * fAttenuation);
			nDataR += (int) ((double)ppAYVoiceBuffer[3*NUM_VOICES_PER_AY8910+j][i] * fAttenuation);
		}

		// Cap the superpositioned output
		if(nDataL < nWaveDataMin)
			nDataL = nWaveDataMin;
		else if(nDataL > nWaveDataMax)
			nDataL = nWaveDataMax;

		if(nDataR < nWaveDataMin)
			nDataR = nWaveDataMin;
		else if(nDataR > nWaveDataMax)
			nDataR = nWaveDataMax;

#ifdef APPLE2IX
		g_nMixBuffer[i*g_nMB_NumChannels+0] = (short)nDataL * samplesScale;
		g_nMixBuffer[i*g_nMB_NumChannels+1] = (short)nDataR * samplesScale;
#else
		g_nMixBuffer[i*g_nMB_NumChannels+0] = (short)nDataL;	// L
		g_nMixBuffer[i*g_nMB_NumChannels+1] = (short)nDataR;	// R
#endif
	}

	//

        const unsigned long originalRequestedBufSize = (unsigned long)nNumSamples*sizeof(short)*g_nMB_NumChannels;
        unsigned long requestedBufSize = originalRequestedBufSize;
        unsigned long bufIdx = 0;
        unsigned long counter = 0;

        if (!nNumSamples) {
            return;
        }

        // make at least 2 attempts to submit data (could be at a ringBuffer boundary)
        do {
            if (MockingboardVoice->Lock(MockingboardVoice, requestedBufSize, &pDSLockedBuffer0, &dwDSLockedBufferSize0)) {
                return;
            }

            {
                unsigned long modTwo = (dwDSLockedBufferSize0 % 2);
                assert(modTwo == 0);
            }
            memcpy(pDSLockedBuffer0, &g_nMixBuffer[bufIdx/sizeof(short)], dwDSLockedBufferSize0);
            MockingboardVoice->Unlock(MockingboardVoice, dwDSLockedBufferSize0);
            bufIdx += dwDSLockedBufferSize0;
            requestedBufSize -= dwDSLockedBufferSize0;
            assert(requestedBufSize <= originalRequestedBufSize);
            ++counter;
        } while (bufIdx < originalRequestedBufSize && counter < 2);

        assert(bufIdx == originalRequestedBufSize);

#ifndef APPLE2IX
	dwByteOffset = (dwByteOffset + (unsigned long)nNumSamples*sizeof(short)*g_nMB_NumChannels) % g_dwDSBufferSize;
#endif

#ifdef RIFF_MB
	RiffPutSamples(&g_nMixBuffer[0], nNumSamples);
#endif
}

//-----------------------------------------------------------------------------

#ifdef APPLE2IX
static void* SSI263Thread(void *lpParameter)
#else
static unsigned long SSI263Thread(void *lpParameter)
#endif
{
        const unsigned long nsecWait = NANOSECONDS_PER_SECOND / audio_backend->systemSettings.sampleRateHz;
        const struct timespec wait = { .tv_sec=0, .tv_nsec=nsecWait };

	while(1)
	{
#ifdef APPLE2IX
            int err =0;

            pthread_mutex_lock(&mockingboard_mutex);
            err = pthread_cond_timedwait(&mockingboard_cond, &mockingboard_mutex, &wait);
            if (err && (err != ETIMEDOUT))
            {
                ERRLOG("OOPS pthread_cond_timedwait");
            }
            pthread_mutex_unlock(&mockingboard_mutex);

            if (quit_event)
            {
                break;
            }

            // poll to see if any samples finished ...
            bool sample_finished = false;
            for (unsigned int i=0; i<MAX_VOICES; i++)
            {
		if (SSI263Voice[i] && SSI263Voice[i]->bActive)
                {
                    unsigned long status = 0;
                    SSI263Voice[i]->GetStatus(SSI263Voice[i], &status);

                    if (status & AUDIO_STATUS_NOTPLAYING)
                    {
                        sample_finished = true;
                        break;
                    }
                }
            }
            if (!sample_finished)
            {
                continue;
            }
#else
		unsigned long dwWaitResult = WaitForMultipleObjects( 
								g_nNumEvents,		// number of handles in array
								g_hSSI263Event,		// array of event handles
								false,				// wait until any one is signaled
								0);

		if((dwWaitResult < 0x0L) || (dwWaitResult > 0x0L+g_nNumEvents-1))
			continue;

		dwWaitResult -= 0x0L;			// Determine event # that signaled

		if(dwWaitResult == (g_nNumEvents-1))	// Termination event
			break;
#endif
		// Phoneme completed playing

		if (g_bStopPhoneme)
		{
			g_bStopPhoneme = false;
			continue;
		}

#if LOG_SSI263
		//if(g_fh) fprintf(g_fh, "IRQ: Phoneme complete (0x%02X)\n\n", g_nCurrentActivePhoneme);
#endif

		SSI263Voice[g_nCurrentActivePhoneme]->bActive = false;
		g_nCurrentActivePhoneme = -1;

		// Phoneme complete, so generate IRQ if necessary
		SY6522_AY8910* pMB = &g_MB[g_nSSI263Device];

		if(g_bPhasorEnable)
		{
			if((pMB->SpeechChip.CurrentMode != MODE_IRQ_DISABLED))
			{
				pMB->SpeechChip.CurrentMode |= 1;	// Set SSI263's D7 pin

				// Phasor's SSI263.IRQ line appears to be wired directly to IRQ (Bypassing the 6522)
#ifdef APPLE2IX
                                cpu65_interrupt(IS_SPEECH);
#else
				CpuIrqAssert(IS_SPEECH);
#endif
			}
		}
		else
		{
			if((pMB->SpeechChip.CurrentMode != MODE_IRQ_DISABLED) && (pMB->sy6522.PCR == 0x0C))
			{
				pMB->sy6522.IFR |= IxR_PERIPHERAL;
				UpdateIFR(pMB);
				pMB->SpeechChip.CurrentMode |= 1;	// Set SSI263's D7 pin
			}
		}

		//

		if(g_bVotraxPhoneme && (pMB->sy6522.PCR == 0xB0))
		{
			// !A/R: Time-out of old phoneme (signal goes from low to high)

			pMB->sy6522.IFR |= IxR_VOTRAX;
			UpdateIFR(pMB);

			g_bVotraxPhoneme = false;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------

static void SSI263_Play(unsigned int nPhoneme)
{
    return; // SSI263 voices are currently deadc0de
#if 0
#if 1
	int hr;

	if(g_nCurrentActivePhoneme >= 0)
	{
		// A write to DURPHON before previous phoneme has completed
		g_bStopPhoneme = true;
#if !defined(APPLE2IX)
		hr = SSI263Voice[g_nCurrentActivePhoneme]->Stop();
#endif
	}

	g_nCurrentActivePhoneme = nPhoneme;

#ifdef APPLE2IX
	hr = SSI263Voice[g_nCurrentActivePhoneme]->Replay(SSI263Voice[g_nCurrentActivePhoneme]);
#else
	hr = SSI263Voice[g_nCurrentActivePhoneme]->SetCurrentPosition(0);
	if(FAILED(hr))
		return;

	hr = SSI263Voice[g_nCurrentActivePhoneme]->Play(0,0,0);	// Not looping
#endif
	if(FAILED(hr))
		return;

	SSI263Voice[g_nCurrentActivePhoneme]->bActive = true;
#else
	int hr;
	bool bPause;

	if(nPhoneme == 1)
		nPhoneme = 2;	// Missing this sample, so map to phoneme-2

	if(nPhoneme == 0)
	{
		bPause = true;
	}
	else
	{
//		nPhoneme--;
		nPhoneme-=2;	// Missing phoneme-1
		bPause = false;
	}

	unsigned long dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
	int16_t* pDSLockedBuffer;

	hr = SSI263Voice->Stop();

	if(DSGetLock(SSI263Voice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, 0))
		return;

	unsigned int nPhonemeShortLength = g_nPhonemeInfo[nPhoneme].nLength;
	unsigned int nPhonemeByteLength = g_nPhonemeInfo[nPhoneme].nLength * sizeof(int16_t);

	if(bPause)
	{
		// 'pause' length is length of 1st phoneme (arbitrary choice, since don't know real length)
		memset(pDSLockedBuffer, 0, g_dwMaxPhonemeLen);
	}
	else
	{
		memcpy(pDSLockedBuffer, &g_nPhonemeData[g_nPhonemeInfo[nPhoneme].nOffset], nPhonemeByteLength);
		memset(&pDSLockedBuffer[nPhonemeShortLength], 0, g_dwMaxPhonemeLen-nPhonemeByteLength);
	}

#if 0
	DSBPOSITIONNOTIFY PositionNotify;

	PositionNotify.dwOffset = nPhonemeByteLength - 1;		// End of phoneme
	PositionNotify.hEventNotify = g_hSSI263Event[0];

	hr = SSI263Voice.lpDSNotify->SetNotificationPositions(1, &PositionNotify);
	if(FAILED(hr))
	{
		DirectSound_ErrorText(hr);
		return;
	}
#endif

#ifdef APPLE2IX
	hr = SSI263Voice->Unlock(SSI263Voice, dwDSLockedBufferSize);
#else
	hr = SSI263Voice->Unlock((void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);
#endif
	if(FAILED(hr))
		return;

	hr = SSI263Voice->Play(0,0,0);	// Not looping
	if(FAILED(hr))
		return;

	SSI263Voice.bActive = true;
#endif
#endif
}

//-----------------------------------------------------------------------------

static bool MB_DSInit()
{
#ifdef APPLE2IX
	LOG("MB_DSInit : %d\n", g_bMBAvailable);
#else
	LOG("MB_DSInit\n", g_bMBAvailable);
#endif
#ifdef NO_DIRECT_X

	return false;

#else // NO_DIRECT_X

	//
	// Create single Mockingboard voice
	//

	unsigned long dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
	int16_t* pDSLockedBuffer;

	if(!audio_isAvailable)
		return false;

	int hr = audio_createSoundBuffer(&MockingboardVoice);
	LOG("MB_DSInit: DSGetSoundBuffer(), hr=0x%08X\n", (unsigned int)hr);
	if(FAILED(hr))
	{
		LOG("MB: DSGetSoundBuffer failed (%08X)\n",(unsigned int)hr);
		return false;
	}

        SAMPLE_RATE = audio_backend->systemSettings.sampleRateHz;
        MB_BUF_SIZE = audio_backend->systemSettings.stereoBufferSizeSamples * audio_backend->systemSettings.bytesPerSample * MB_CHANNELS;
        g_dwDSBufferSize = MB_BUF_SIZE;
        g_nMixBuffer = MALLOC(MB_BUF_SIZE / audio_backend->systemSettings.bytesPerSample);

#ifndef APPLE2IX
	bool bRes = DSZeroVoiceBuffer(&MockingboardVoice, (char*)"MB", g_dwDSBufferSize);
	LOG("MB_DSInit: DSZeroVoiceBuffer(), res=%d\n", bRes ? 1 : 0);
	if (!bRes)
		return false;
#endif

	MockingboardVoice->bActive = true;

	// Volume might've been setup from value in Registry
	if(!MockingboardVoice->nVolume)
		MockingboardVoice->nVolume = DSBVOLUME_MAX;

#if !defined(APPLE2IX)
	hr = MockingboardVoice->SetVolume(MockingboardVoice->nVolume);
#endif
	LOG("MB_DSInit: SetVolume(), hr=0x%08X\n", (unsigned int)hr);

	//---------------------------------

	//
	// Create SSI263 voice
	//

#if 0
	g_dwMaxPhonemeLen = 0;
	for(int i=0; i<sizeof(g_nPhonemeInfo) / sizeof(PHONEME_INFO); i++)
		if(g_dwMaxPhonemeLen < g_nPhonemeInfo[i].nLength)
			g_dwMaxPhonemeLen = g_nPhonemeInfo[i].nLength;
	g_dwMaxPhonemeLen *= sizeof(int16_t);
#endif

        return true;
#if 0

#ifdef APPLE2IX
        int err = 0;
        if ((err = pthread_mutex_init(&mockingboard_mutex, NULL)))
        {
            ERRLOG("OOPS pthread_mutex_init");
        }

        if ((err = pthread_cond_init(&mockingboard_cond, NULL)))
        {
            ERRLOG("OOPS pthread_cond_init");
        }
#else
	g_hSSI263Event[0] = CreateEvent(NULL,	// lpEventAttributes
									false,	// bManualReset (false = auto-reset)
									false,	// bInitialState (false = non-signaled)
									NULL);	// lpName
	LOG("MB_DSInit: CreateEvent(), g_hSSI263Event[0]=0x%08X\n", (uint32_t)g_hSSI263Event[0]);

	g_hSSI263Event[1] = CreateEvent(NULL,	// lpEventAttributes
									false,	// bManualReset (false = auto-reset)
									false,	// bInitialState (false = non-signaled)
									NULL);	// lpName
	LOG("MB_DSInit: CreateEvent(), g_hSSI263Event[1]=0x%08X\n", (uint32_t)g_hSSI263Event[1]);

	if((g_hSSI263Event[0] == NULL) || (g_hSSI263Event[1] == NULL))
	{
		LOG("SSI263: CreateEvent failed\n");
		return false;
	}
#endif

	for(int i=0; i<MAX_VOICES; i++)
	{
		unsigned int nPhoneme = i;
		bool bPause;

		if(nPhoneme == 1)
			nPhoneme = 2;	// Missing this sample, so map to phoneme-2

		if(nPhoneme == 0)
		{
			bPause = true;
		}
		else
		{
//			nPhoneme--;
			nPhoneme-=2;	// Missing phoneme-1
			bPause = false;
		}

		unsigned int nPhonemeByteLength = g_nPhonemeInfo[nPhoneme].nLength * audio_backend->systemSettings.bytesPerSample;
                if (nPhonemeByteLength > audio_backend->systemSettings.monoBufferSizeSamples) {
                    RELEASE_ERRLOG("!!!!!!!!!!!!!!!!!!!!! phoneme length > buffer size !!!!!!!!!!!!!!!!!!!!!");
#warning ^^^^^^^^^^ require vigilence here around this change ... we used to be able to specify the exact buffer size ...
                }

		// NB. DSBCAPS_LOCSOFTWARE required for
		hr = audio_createSoundBuffer(&SSI263Voice[i], 1);
		LOG("MB_DSInit: (%02d) DSGetSoundBuffer(), hr=0x%08X\n", i, (unsigned int)hr);
		if(FAILED(hr))
		{
			LOG("SSI263: DSGetSoundBuffer failed (%08X)\n",(unsigned int)hr);
			return false;
		}

		hr = SSI263Voice[i]->Lock(SSI263Voice[i], 0, &pDSLockedBuffer, &dwDSLockedBufferSize);
		//LOG("MB_DSInit: (%02d) DSGetLock(), res=%d\n", i, hr ? 1 : 0);	// WARNING: Lock acquired && doing heavy-weight logging
		if(FAILED(hr))
		{
			LOG("SSI263: DSGetLock failed (%08X)\n",(unsigned int)hr);
			return false;
		}

		if(bPause)
		{
			// 'pause' length is length of 1st phoneme (arbitrary choice, since don't know real length)
#ifdef APPLE2IX
			memset(pDSLockedBuffer, 0x00, dwDSLockedBufferSize);
#else
			memset(pDSLockedBuffer, 0x00, nPhonemeByteLength);
#endif
		}
		else
		{
#ifdef APPLE2IX
			memcpy(pDSLockedBuffer, &g_nPhonemeData[g_nPhonemeInfo[nPhoneme].nOffset], dwDSLockedBufferSize);
#else
			memcpy(pDSLockedBuffer, &g_nPhonemeData[g_nPhonemeInfo[nPhoneme].nOffset], nPhonemeByteLength);
#endif
		}

#ifdef APPLE2IX
                // Assume no way to get notification of sound finished, instead we will poll from mockingboard thread ...
#else
 		hr = SSI263Voice[i]->QueryInterface(IID_IDirectSoundNotify, (void **)&SSI263Voice[i]->lpDSNotify);
		//LOG("MB_DSInit: (%02d) QueryInterface(), hr=0x%08X\n", i, hr);	// WARNING: Lock acquired && doing heavy-weight logging
		if(FAILED(hr))
		{
			LOG("SSI263: QueryInterface failed (%08X)\n",hr);
			return false;
		}

		DSBPOSITIONNOTIFY PositionNotify;

//		PositionNotify.dwOffset = nPhonemeByteLength - 1;	// End of buffer
		PositionNotify.dwOffset = DSBPN_OFFSETSTOP;			// End of buffer
		PositionNotify.hEventNotify = g_hSSI263Event[0];

		hr = SSI263Voice[i]->lpDSNotify->SetNotificationPositions(1, &PositionNotify);
		//LOG("MB_DSInit: (%02d) SetNotificationPositions(), hr=0x%08X\n", i, hr);	// WARNING: Lock acquired && doing heavy-weight logging
		if(FAILED(hr))
		{
			LOG("SSI263: SetNotifyPos failed (%08X)\n",hr);
			return false;
		}
#endif

#ifdef APPLE2IX
		hr = SSI263Voice[i]->UnlockStaticBuffer(SSI263Voice[i], dwDSLockedBufferSize);
#else
		hr = SSI263Voice[i]->Unlock((void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);
#endif
		LOG("MB_DSInit: (%02d) Unlock(),hr=0x%08X\n", i, (unsigned int)hr);
		if(FAILED(hr))
		{
			LOG("SSI263: DSUnlock failed (%08X)\n",(unsigned int)hr);
			return false;
		}

		SSI263Voice[i]->bActive = false;
		SSI263Voice[i]->nVolume = MockingboardVoice->nVolume;		// Use same volume as MB
#if !defined(APPLE2IX)
		hr = SSI263Voice[i]->SetVolume(SSI263Voice[i]->nVolume);
#endif
		LOG("MB_DSInit: (%02d) SetVolume(), hr=0x%08X\n", i, (unsigned int)hr);
	}

	//

	unsigned long dwThreadId;

	g_hThread = CreateThread(NULL,				// lpThreadAttributes
								0,				// dwStackSize
								SSI263Thread,
								NULL,			// lpParameter
								0,				// dwCreationFlags : 0 = Run immediately
								&dwThreadId);	// lpThreadId
	LOG("MB_DSInit: CreateThread(), g_hThread=0x%08X\n", (uint32_t)g_hThread);

	bool bRes2 = SetThreadPriority(g_hThread, THREAD_PRIORITY_TIME_CRITICAL);
	LOG("MB_DSInit: SetThreadPriority(), bRes=%d\n", bRes2 ? 1 : 0);

	return true;

#endif
#endif // NO_DIRECT_X
}

static void MB_DSUninit()
{
	if(g_hThread)
	{
		unsigned long dwExitCode;
#ifdef APPLE2IX
                quit_event = true;
                pthread_cond_signal(&mockingboard_cond);
#else
		SetEvent(g_hSSI263Event[g_nNumEvents-1]);	// Signal to thread that it should exit
#endif

		do
		{
			if(GetExitCodeThread(g_hThread, &dwExitCode))
			{
				if(dwExitCode == STILL_ACTIVE)
					usleep(10);
				else
					break;
			}
		}
		while(1);

#ifdef APPLE2IX
		g_hThread = 0;
                pthread_mutex_destroy(&mockingboard_mutex);
                pthread_cond_destroy(&mockingboard_cond);
#else
		CloseHandle(g_hThread);
		g_hThread = NULL;
#endif
	}

	//

	if(MockingboardVoice && MockingboardVoice->bActive)
	{
#if !defined(APPLE2IX)
		MockingboardVoice->Stop();
#endif
		MockingboardVoice->bActive = false;
	}

	audio_destroySoundBuffer(&MockingboardVoice);

	//

	for(int i=0; i<MAX_VOICES; i++)
	{
		if(SSI263Voice[i] && SSI263Voice[i]->bActive)
		{
#if !defined(APPLE2IX)
			SSI263Voice[i]->Stop();
#endif
			SSI263Voice[i]->bActive = false;
		}

		audio_destroySoundBuffer(&SSI263Voice[i]);
	}

	//
        FREE(g_nMixBuffer);

#ifndef APPLE2IX
	if(g_hSSI263Event[0])
	{
		CloseHandle(g_hSSI263Event[0]);
		g_hSSI263Event[0] = NULL;
	}

	if(g_hSSI263Event[1])
	{
		CloseHandle(g_hSSI263Event[1]);
		g_hSSI263Event[1] = NULL;
	}
#endif
}

//=============================================================================

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//=============================================================================

void MB_Initialize()
{
#ifdef APPLE2IX
    assert(pthread_self() == cpu_thread_id);
    memset(SSI263Voice, 0x0, sizeof(AudioBuffer_s *) * MAX_VOICES);
#endif
	if (g_bDisableDirectSoundMockingboard)
	{
		//MockingboardVoice->bMute = true;
		g_SoundcardType = CT_Empty;
	}
	else
	{
		memset(&g_MB,0,sizeof(g_MB));

		g_bMBAvailable = MB_DSInit();
                if (!g_bMBAvailable) {
                    //MockingboardVoice->bMute = true;
                    g_SoundcardType = CT_Empty;
                    return;
                }


		int i;
		for(i=0; i<NUM_VOICES; i++)
                {
#ifdef APPLE2IX
			ppAYVoiceBuffer[i] = MALLOC(sizeof(short) * SAMPLE_RATE); // Buffer can hold a max of 1 seconds worth of samples
#else
			ppAYVoiceBuffer[i] = new short [SAMPLE_RATE];	// Buffer can hold a max of 1 seconds worth of samples
#endif
                }

		AY8910_InitAll((int)cycles_persec_target, SAMPLE_RATE);
		LOG("MB_Initialize: AY8910_InitAll()\n");

		for(i=0; i<NUM_AY8910; i++)
			g_MB[i].nAY8910Number = i;

		//

		LOG("MB_Initialize: MB_DSInit(), g_bMBAvailable=%d\n", g_bMBAvailable);

		MB_Reset();
		LOG("MB_Initialize: MB_Reset()\n");
	}
}

//-----------------------------------------------------------------------------

// NB. Called when /cycles_persec_target/ changes
void MB_Reinitialize()
{
	AY8910_InitClock((int)cycles_persec_target, SAMPLE_RATE);
}

//-----------------------------------------------------------------------------

void MB_Destroy()
{
    assert(pthread_self() == cpu_thread_id);
	MB_DSUninit();

	for(int i=0; i<NUM_VOICES; i++)
        {
#ifdef APPLE2IX
		FREE(ppAYVoiceBuffer[i]);
#else
		delete [] ppAYVoiceBuffer[i];
#endif
        }
}

void MB_SetEnabled(bool enabled) {
    g_bDisableDirectSoundMockingboard = !enabled;
}

bool MB_ISEnabled(void) {
    return (MockingboardVoice != NULL);
}

//-----------------------------------------------------------------------------

static void ResetState()
{
	g_n6522TimerPeriod = 0;
	g_nMBTimerDevice = TIMERDEVICE_INVALID;
	g_uLastCumulativeCycles = 0;

	g_nSSI263Device = 0;
	g_nCurrentActivePhoneme = -1;
	g_bStopPhoneme = false;
	g_bVotraxPhoneme = false;

	g_nMB_InActiveCycleCount = 0;
	g_bMB_RegAccessedFlag = false;
	g_bMB_Active = false;

	//g_bMBAvailable = false;

	//g_SoundcardType = CT_Empty;
	//g_bPhasorEnable = false;
	g_nPhasorMode = 0;
}

void MB_Reset()
{
	if(!audio_isAvailable)
		return;

	for(int i=0; i<NUM_AY8910; i++)
	{
		ResetSY6522(&g_MB[i]);
		AY8910_reset(i);
	}

	ResetState();
	MB_Reinitialize();	// Reset CLK for AY8910s
}

//-----------------------------------------------------------------------------

#ifdef APPLE2IX
#define MemReadFloatingBus floating_bus
#define nAddr ea
GLUE_C_READ(MB_Read)
#else
static uint8_t MB_Read(uint16_t PC, uint16_t nAddr, uint8_t bWrite, uint8_t nValue, unsigned long nCyclesLeft)
#endif
{
	MB_UpdateCycles();

#ifdef _DEBUG
	if(!IS_APPLE2 && !MemCheckSLOTCXROM())
	{
		assert(0);	// Card ROM disabled, so IORead_Cxxx() returns the internal ROM
		return mem[nAddr];
	}

	if(g_SoundcardType == CT_Empty)
	{
		assert(0);	// Card unplugged, so IORead_Cxxx() returns the floating bus
		return MemReadFloatingBus();
	}
#endif

	uint8_t nMB = ((nAddr>>8)&0xf) - SLOT4;
	uint8_t nOffset = nAddr&0xff;

	if(g_bPhasorEnable)
	{
		if(nMB != 0)	// Slot4 only
                {
			return MemReadFloatingBus();
                }

		int CS;
		if(g_nPhasorMode & 1)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else															// Mockingboard Mode
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2

		uint8_t nRes = 0;

		if(CS & 1)
			nRes |= SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf);

		if(CS & 2)
			nRes |= SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf);

		bool bAccessedDevice = (CS & 3) ? true : false;

		if((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x05)))
		{
			nRes |= SSI263_Read(nMB, nAddr&0xf);
			bAccessedDevice = true;
		}

		return bAccessedDevice ? nRes : MemReadFloatingBus();
	}

	if(nOffset <= (SY6522A_Offset+0x0F))
        {
		return SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf);
        }
	else if((nOffset >= SY6522B_Offset) && (nOffset <= (SY6522B_Offset+0x0F)))
        {
		return SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf);
        }
	else if((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x05)))
        {
		return SSI263_Read(nMB, nAddr&0xf);
        }
	else
        {
		return MemReadFloatingBus();
        }
}

//-----------------------------------------------------------------------------

#ifdef APPLE2IX
#define nValue b
GLUE_C_WRITE(MB_Write)
#else
static uint8_t MB_Write(uint16_t PC, uint16_t nAddr, uint8_t bWrite, uint8_t nValue, unsigned long nCyclesLeft)
#endif
{
	MB_UpdateCycles();

#ifdef _DEBUG
	if(!IS_APPLE2 && !MemCheckSLOTCXROM())
	{
		assert(0);	// Card ROM disabled, so IORead_Cxxx() returns the internal ROM
#ifdef APPLE2IX
                return;
#else
		return 0;
#endif
	}

	if(g_SoundcardType == CT_Empty)
	{
		assert(0);	// Card unplugged, so IORead_Cxxx() returns the floating bus
#ifdef APPLE2IX
                return;
#else
		return 0;
#endif
	}
#endif

	uint8_t nMB = ((nAddr>>8)&0xf) - SLOT4;
	uint8_t nOffset = nAddr&0xff;

	if(g_bPhasorEnable)
	{
		if(nMB != 0)	// Slot4 only
                {
#ifdef APPLE2IX
                    return;
#else
			return 0;
#endif
                }

		int CS;

		if(g_nPhasorMode & 1)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else															// Mockingboard Mode
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2

		if(CS & 1)
			SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf, nValue);

		if(CS & 2)
			SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf, nValue);

		if((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x05)))
			SSI263_Write(nMB*2+1, nAddr&0xf, nValue);		// Second 6522 is used for speech chip

#ifdef APPLE2IX
                return;
#else
		return 0;
#endif
	}

	if(nOffset <= (SY6522A_Offset+0x0F))
		SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf, nValue);
	else if((nOffset >= SY6522B_Offset) && (nOffset <= (SY6522B_Offset+0x0F)))
		SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf, nValue);
	else if((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x05)))
		SSI263_Write(nMB*2+1, nAddr&0xf, nValue);		// Second 6522 is used for speech chip

#ifdef APPLE2IX
        return;
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------

#ifdef APPLE2IX
GLUE_C_READ(PhasorIO)
#else
static uint8_t PhasorIO(uint16_t PC, uint16_t nAddr, uint8_t bWrite, uint8_t nValue, unsigned long nCyclesLeft)
#endif
{
	if(!g_bPhasorEnable)
        {
		return MemReadFloatingBus();
        }

	if(g_nPhasorMode < 2)
		g_nPhasorMode = nAddr & 1;

	double fCLK = (nAddr & 4) ? CLK_6502*2 : CLK_6502;

	AY8910_InitClock((int)fCLK, SAMPLE_RATE);

	return MemReadFloatingBus();
}

//-----------------------------------------------------------------------------
#ifdef APPLE2IX
// HACK NOTE TODO FIXME : hardcoded for now (until we have dynamic emulation for other cards in these slots) ...
//SS_CARDTYPE g_Slot4 = CT_Phasor;
//SS_CARDTYPE g_Slot5 = CT_Empty;
SS_CARDTYPE g_Slot4 = CT_MockingboardC;
SS_CARDTYPE g_Slot5 = CT_MockingboardC;

#define IO_Null NULL

void mb_io_initialize(unsigned int slot4, unsigned int slot5)
{
    MB_InitializeIO(NULL, slot4, slot5);
}

//typedef uint8_t (*iofunction)(uint16_t nPC, uint16_t nAddr, uint8_t nWriteFlag, uint8_t nWriteValue, unsigned long nCyclesLeft);
typedef void (*iofunction)();
static void RegisterIoHandler(unsigned int uSlot, iofunction IOReadC0, iofunction IOWriteC0, iofunction IOReadCx, iofunction IOWriteCx, void *unused_lpSlotParameter, uint8_t* unused_pExpansionRom)
{

    // card softswitches
    unsigned int base_addr = 0xC080 + (uSlot<<4); // uSlot == 4 => 0xC0C0 , uSlot == 5 => 0xC0D0
    if (IOReadC0)
    {
        assert(IOWriteC0);
        for (unsigned int i = 0; i < 16; i++)
        {
            cpu65_vmem_r[base_addr+i] = IOReadC0;
            cpu65_vmem_w[base_addr+i] = IOWriteC0;
        }
    }

    // card page
    base_addr = 0xC000 + (uSlot<<8); // uSlot == 4 => 0xC400 , uSlot == 5 => 0xC500
    for (unsigned int i = 0; i < 0x100; i++)
    {
        //cpu65_vmem_r[base_addr+i] = IOReadCx; -- CANNOT DO THIS HERE -- DEPENDS ON cxrom softswitch
        cpu65_vmem_w[base_addr+i] = IOWriteCx;
    }
}
#endif

void MB_InitializeIO(char *unused_pCxRomPeripheral, unsigned int uSlot4, unsigned int uSlot5)
{
	// Mockingboard: Slot 4 & 5
	// Phasor      : Slot 4
	// <other>     : Slot 4 & 5

	if (g_Slot4 != CT_MockingboardC && g_Slot4 != CT_Phasor)
	{
		MB_SetSoundcardType(CT_Empty);
		return;
	}

	if (g_Slot4 == CT_MockingboardC)
		RegisterIoHandler(uSlot4, IO_Null, IO_Null, MB_Read, MB_Write, NULL, NULL);
	else	// Phasor
		RegisterIoHandler(uSlot4, PhasorIO, PhasorIO, MB_Read, MB_Write, NULL, NULL);

	if (g_Slot5 == CT_MockingboardC)
		RegisterIoHandler(uSlot5, IO_Null, IO_Null, MB_Read, MB_Write, NULL, NULL);

	MB_SetSoundcardType(g_Slot4);
}

//-----------------------------------------------------------------------------

void MB_Mute()
{
	if(g_SoundcardType == CT_Empty)
		return;

	if(MockingboardVoice->bActive && !MockingboardVoice->bMute)
	{
#if !defined(APPLE2IX)
		MockingboardVoice->SetVolume(DSBVOLUME_MIN);
#endif
		MockingboardVoice->bMute = true;
	}

#if !defined(APPLE2IX)
	if(g_nCurrentActivePhoneme >= 0)
		SSI263Voice[g_nCurrentActivePhoneme]->SetVolume(DSBVOLUME_MIN);
#endif
}

//-----------------------------------------------------------------------------

void MB_Demute()
{
	if(g_SoundcardType == CT_Empty)
		return;

	if(MockingboardVoice->bActive && MockingboardVoice->bMute)
	{
#if !defined(APPLE2IX)
		MockingboardVoice->SetVolume(MockingboardVoice->nVolume);
#endif
		MockingboardVoice->bMute = false;
	}

#if !defined(APPLE2IX)
	if(g_nCurrentActivePhoneme >= 0)
		SSI263Voice[g_nCurrentActivePhoneme]->SetVolume(SSI263Voice[g_nCurrentActivePhoneme]->nVolume);
#endif
}

//-----------------------------------------------------------------------------

// Called by CpuExecute() before doing CPU emulation
void MB_StartOfCpuExecute()
{
	g_uLastCumulativeCycles = cycles_count_total;
}

// Called by ContinueExecution() at the end of every video frame
void MB_EndOfVideoFrame()
{
    SCOPE_TRACE_AUDIO("MB_EndOfVideoFrame");
	if(g_SoundcardType == CT_Empty)
		return;

        if (!g_bMBAvailable) {
            return;
        }

	if(!g_bMBTimerIrqActive)
		MB_Update();
}

//-----------------------------------------------------------------------------

// Called by CpuExecute() after every N opcodes (N = ~1000 @ 1MHz)
void MB_UpdateCycles(void)
{
    SCOPE_TRACE_AUDIO("MB_UpdateCycles");
	if(g_SoundcardType == CT_Empty)
		return;

	timing_checkpoint_cycles();
	uint64_t uCycles = cycles_count_total - g_uLastCumulativeCycles;
	g_uLastCumulativeCycles = cycles_count_total;
	//assert(uCycles < 0x10000);
        if (uCycles >= 0x10000) {
            printf("OOPS!!! Mockingboard failed assert!\n");
            return;
        }
	uint16_t nClocks = (uint16_t) uCycles;

	for(unsigned int i=0; i<NUM_SY6522; i++)
	{
		SY6522_AY8910* pMB = &g_MB[i];

		uint16_t OldTimer1 = pMB->sy6522.TIMER1_COUNTER.w;
#ifndef APPLE2IX
		uint16_t OldTimer2 = pMB->sy6522.TIMER2_COUNTER.w;
#endif

		pMB->sy6522.TIMER1_COUNTER.w -= nClocks;
		pMB->sy6522.TIMER2_COUNTER.w -= nClocks;

		// Check for counter underflow
		bool bTimer1Underflow = (!(OldTimer1 & 0x8000) && (pMB->sy6522.TIMER1_COUNTER.w & 0x8000));
#ifndef APPLE2IX
		bool bTimer2Underflow = (!(OldTimer2 & 0x8000) && (pMB->sy6522.TIMER2_COUNTER.w & 0x8000));
#endif

		if( bTimer1Underflow && (g_nMBTimerDevice == i) && g_bMBTimerIrqActive )
		{
#ifdef _DEBUG
			g_uTimer1IrqCount++;	// DEBUG
#endif

			pMB->sy6522.IFR |= IxR_TIMER1;
			UpdateIFR(pMB);

			if((pMB->sy6522.ACR & RUNMODE) == RM_ONESHOT)
			{
				// One-shot mode
				// - Phasor's playback code uses one-shot mode
				// - Willy Byte sets to one-shot to stop the timer IRQ
				StopTimer(pMB);
			}
			else
			{
				// Free-running mode
				// - Ultima4/5 change ACCESS_TIMER1 after a couple of IRQs into tune
				pMB->sy6522.TIMER1_COUNTER.w = pMB->sy6522.TIMER1_LATCH.w;
				StartTimer(pMB);
			}

			MB_Update();
		}
		else if ( bTimer1Underflow
					&& !g_bMBTimerIrqActive								// StopTimer() has been called
					&& (pMB->sy6522.IFR & IxR_TIMER1)					// IRQ
					&& ((pMB->sy6522.ACR & RUNMODE) == RM_ONESHOT) )	// One-shot mode
		{
			// Fix for Willy Byte - need to confirm that 6522 really does this!
			// . It never accesses IER/IFR/TIMER1 regs to clear IRQ
			pMB->sy6522.IFR &= ~IxR_TIMER1;		// Deassert the TIMER IRQ
			UpdateIFR(pMB);
		}
	}
}

//-----------------------------------------------------------------------------

SS_CARDTYPE MB_GetSoundcardType()
{
	return g_SoundcardType;
}

void MB_SetSoundcardType(SS_CARDTYPE NewSoundcardType)
{
//	if ((NewSoundcardType == SC_UNINIT) || (g_SoundcardType == NewSoundcardType))
	if (g_SoundcardType == NewSoundcardType)
		return;

	g_SoundcardType = NewSoundcardType;

	if(g_SoundcardType == CT_Empty)
		MB_Mute();

	g_bPhasorEnable = (g_SoundcardType == CT_Phasor);
}

//-----------------------------------------------------------------------------

double MB_GetFramePeriod()
{
	return (g_bMBTimerIrqActive||(g_MB[0].sy6522.IFR & IxR_TIMER1)) ? (double)g_n6522TimerPeriod : g_f6522TimerPeriod_NoIRQ;
}

bool MB_IsActive()
{
	if(!MockingboardVoice->bActive)
		return false;

	// Ignore /g_bMBTimerIrqActive/ as timer's irq handler will access 6522 regs affecting /g_bMB_Active/

	return g_bMB_Active;
}

//-----------------------------------------------------------------------------
#if defined(APPLE2IX)
void MB_SetVolumeZeroToTen(unsigned long goesToTen) {
    samplesScale = goesToTen/10.f;
}
#else
static long NewVolume(unsigned long dwVolume, unsigned long dwVolumeMax)
{
    float fVol = (float) dwVolume / (float) dwVolumeMax;    // 0.0=Max, 1.0=Min

    return (long) ((float) DSBVOLUME_MIN * fVol);
}

void MB_SetVolume(unsigned long dwVolume, unsigned long dwVolumeMax)
{
	MockingboardVoice->nVolume = NewVolume(dwVolume, dwVolumeMax);

#if !defined(APPLE2IX)
	if(MockingboardVoice->bActive)
		MockingboardVoice->SetVolume(MockingboardVoice->nVolume);
#endif
}
#endif

//===========================================================================

unsigned long MB_GetSnapshot(SS_CARD_MOCKINGBOARD* pSS, unsigned long dwSlot)
{
#ifdef APPLE2IX
    // Saving and loading state currently unimplemented
    return 0;
#else
	pSS->Hdr.UnitHdr.dwLength = sizeof(SS_CARD_DISK2);
	pSS->Hdr.UnitHdr.dwVersion = MAKE_VERSION(1,0,0,0);

	pSS->Hdr.dwSlot = dwSlot;
	pSS->Hdr.dwType = CT_MockingboardC;

	unsigned int nMbCardNum = dwSlot - SLOT4;
	unsigned int nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	for(unsigned int i=0; i<MB_UNITS_PER_CARD; i++)
	{
		memcpy(&pSS->Unit[i].RegsSY6522, &pMB->sy6522, sizeof(SY6522));
		memcpy(&pSS->Unit[i].RegsAY8910, AY8910_GetRegsPtr(nDeviceNum), 16);
		memcpy(&pSS->Unit[i].RegsSSI263, &pMB->SpeechChip, sizeof(SSI263A));
		pSS->Unit[i].nAYCurrentRegister = pMB->nAYCurrentRegister;

		nDeviceNum++;
		pMB++;
	}

	return 0;
#endif
}

unsigned long MB_SetSnapshot(SS_CARD_MOCKINGBOARD* pSS, unsigned long dwSlot_unused)
{
#ifdef APPLE2IX
    // Saving and loading state currently unimplemented
    return 0;
#else
	if(pSS->Hdr.UnitHdr.dwVersion != MAKE_VERSION(1,0,0,0))
		return -1;

	unsigned int nMbCardNum = pSS->Hdr.dwSlot - SLOT4;
	unsigned int nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	g_nSSI263Device = 0;
	g_nCurrentActivePhoneme = -1;

	for(unsigned int i=0; i<MB_UNITS_PER_CARD; i++)
	{
		memcpy(&pMB->sy6522, &pSS->Unit[i].RegsSY6522, sizeof(SY6522));
		memcpy(AY8910_GetRegsPtr(nDeviceNum), &pSS->Unit[i].RegsAY8910, 16);
		memcpy(&pMB->SpeechChip, &pSS->Unit[i].RegsSSI263, sizeof(SSI263A));
		pMB->nAYCurrentRegister = pSS->Unit[i].nAYCurrentRegister;

		StartTimer(pMB);	// Attempt to start timer

		//

		// Crude - currently only support a single speech chip
		// FIX THIS:
		// . Speech chip could be Votrax instead
		// . Is this IRQ compatible with Phasor?
		if(pMB->SpeechChip.DurationPhonome)
		{
			g_nSSI263Device = nDeviceNum;

			if((pMB->SpeechChip.CurrentMode != MODE_IRQ_DISABLED) && (pMB->sy6522.PCR == 0x0C) && (pMB->sy6522.IER & IxR_PERIPHERAL))
			{
				pMB->sy6522.IFR |= IxR_PERIPHERAL;
				UpdateIFR(pMB);
				pMB->SpeechChip.CurrentMode |= 1;	// Set SSI263's D7 pin
			}
		}

		nDeviceNum++;
		pMB++;
	}

	return 0;
#endif
}

