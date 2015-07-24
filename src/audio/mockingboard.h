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
#include "audio/peripherals.h"

extern bool g_bDisableDirectSoundMockingboard;

typedef struct
{
    union
    {
        struct
        {
            uint8_t l;
            uint8_t h;
        };
        uint16_t w;
    };
} IWORD;

typedef struct
{
	uint8_t ORB;				// $00 - Port B
	uint8_t ORA;				// $01 - Port A (with handshaking)
	uint8_t DDRB;				// $02 - Data Direction Register B
	uint8_t DDRA;				// $03 - Data Direction Register A
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
	uint8_t SERIAL_SHIFT;		// $0A
	uint8_t ACR;				// $0B - Auxiliary Control Register
	uint8_t PCR;				// $0C - Peripheral Control Register
	uint8_t IFR;				// $0D - Interrupt Flag Register
	uint8_t IER;				// $0E - Interrupt Enable Register
	uint8_t ORA_NO_HS;			// $0F - Port A (without handshaking)
} SY6522;

typedef struct
{
	uint8_t DurationPhonome;
	uint8_t Inflection;		// I10..I3
	uint8_t RateInflection;
	uint8_t CtrlArtAmp;
	uint8_t FilterFreq;
	//
	uint8_t CurrentMode;		// b7:6=Mode; b0=D7 pin (for IRQ)
} SSI263A;

extern SS_CARDTYPE g_Slot4; // Mockingboard, Z80, Mouse in slot4
extern SS_CARDTYPE g_Slot5; // Mockingboard, Z80 in slot5

#define MB_UNITS_PER_CARD 2

typedef struct
{
	SY6522		RegsSY6522;
	uint8_t		RegsAY8910[16];
	SSI263A		RegsSSI263;
	uint8_t		nAYCurrentRegister;
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
extern uint32_t	g_uTimer1IrqCount;	// DEBUG
#endif

void	MB_Initialize();
void	MB_Reinitialize();
void	MB_Destroy();
bool    MB_ISEnabled(void);
void    MB_Reset();
void    MB_InitializeIO(char *pCxRomPeripheral, unsigned int uSlot4, unsigned int uSlot5);
void    MB_Mute();
void    MB_Demute();
void    MB_StartOfCpuExecute();
void    MB_EndOfVideoFrame();
void    MB_UpdateCycles(void);
SS_CARDTYPE MB_GetSoundcardType();
void    MB_SetSoundcardType(SS_CARDTYPE NewSoundcardType);
double  MB_GetFramePeriod();
bool    MB_IsActive();
unsigned long   MB_GetVolume();
void    MB_SetVolumeZeroToTen(unsigned long goesToTen);
void    MB_SetVolume(unsigned long dwVolume, unsigned long dwVolumeMax);
unsigned long   MB_GetSnapshot(SS_CARD_MOCKINGBOARD* pSS, unsigned long dwSlot);
unsigned long   MB_SetSnapshot(SS_CARD_MOCKINGBOARD* pSS, unsigned long dwSlot);
#ifdef APPLE2IX
void mb_io_initialize(unsigned int slot4, unsigned int slot5);
#endif

#endif // whole file
