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

#ifndef _MOCKINGBOARD_H__
#define _MOCKINGBOARD_H__

#ifdef APPLE2IX
#include "win-shim.h"
#include "peripherals.h"

extern bool g_bDisableDirectSoundMockingboard;

typedef struct
{
    union
    {
        struct
        {
            BYTE l;
            BYTE h;
        };
        USHORT w;
    };
} IWORD;

typedef struct
{
	BYTE ORB;				// $00 - Port B
	BYTE ORA;				// $01 - Port A (with handshaking)
	BYTE DDRB;				// $02 - Data Direction Register B
	BYTE DDRA;				// $03 - Data Direction Register A
	//
	// $04 - Read counter (L) / Write latch (L)
	// $05 - Read / Write & initiate count (H)
	// $06 - Read / Write & latch (L)
	// $07 - Read / Write & latch (H)
	// $08 - Read counter (L) / Write latch (L)
	// $09 - Read counter (H) / Write latch (H)
	IWORD TIMER1_COUNTER;
	IWORD TIMER1_LATCH;
	IWORD TIMER2_COUNTER;
	IWORD TIMER2_LATCH;
	//
	BYTE SERIAL_SHIFT;		// $0A
	BYTE ACR;				// $0B - Auxiliary Control Register
	BYTE PCR;				// $0C - Peripheral Control Register
	BYTE IFR;				// $0D - Interrupt Flag Register
	BYTE IER;				// $0E - Interrupt Enable Register
	BYTE ORA_NO_HS;			// $0F - Port A (without handshaking)
} SY6522;

typedef struct
{
	BYTE DurationPhonome;
	BYTE Inflection;		// I10..I3
	BYTE RateInflection;
	BYTE CtrlArtAmp;
	BYTE FilterFreq;
	//
	BYTE CurrentMode;		// b7:6=Mode; b0=D7 pin (for IRQ)
} SSI263A;

extern SS_CARDTYPE g_Slot4; // Mockingboard, Z80, Mouse in slot4
extern SS_CARDTYPE g_Slot5; // Mockingboard, Z80 in slot5

#define MB_UNITS_PER_CARD 2

typedef struct
{
	SY6522		RegsSY6522;
	BYTE		RegsAY8910[16];
	SSI263A		RegsSSI263;
	BYTE		nAYCurrentRegister;
	bool		bTimer1IrqPending;
	bool		bTimer2IrqPending;
	bool		bSpeechIrqPending;
} MB_Unit;

typedef struct
{
	SS_CARD_HDR	Hdr;
	MB_Unit		Unit[MB_UNITS_PER_CARD];
} SS_CARD_MOCKINGBOARD;
#endif

extern bool       g_bMBTimerIrqActive;
#ifdef _DEBUG
extern UINT32	g_uTimer1IrqCount;	// DEBUG
#endif

void	MB_Initialize();
void	MB_Reinitialize();
void	MB_Destroy();
void    MB_Reset();
void    MB_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot4, UINT uSlot5);
void    MB_Mute();
void    MB_Demute();
void    MB_StartOfCpuExecute();
void    MB_EndOfVideoFrame();
void    MB_UpdateCycles(ULONG uExecutedCycles);
SS_CARDTYPE MB_GetSoundcardType();
void    MB_SetSoundcardType(SS_CARDTYPE NewSoundcardType);
double  MB_GetFramePeriod();
bool    MB_IsActive();
DWORD   MB_GetVolume();
void    MB_SetVolume(DWORD dwVolume, DWORD dwVolumeMax);
DWORD   MB_GetSnapshot(SS_CARD_MOCKINGBOARD* pSS, DWORD dwSlot);
DWORD   MB_SetSnapshot(SS_CARD_MOCKINGBOARD* pSS, DWORD dwSlot);
#ifdef APPLE2IX
void mb_io_initialize(unsigned int slot4, unsigned int slot5);
#endif

#endif // whole file
