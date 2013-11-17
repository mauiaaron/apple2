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

#ifndef _SPEAKER_H_
#define _SPEAKER_H_

#ifdef APPLE2IX
#include "win-shim.h"
#endif

extern DWORD      soundtype;
extern double     g_fClksPerSpkrSample;

void    SpkrDestroy ();
void    SpkrInitialize ();
void    SpkrReinitialize ();
void    SpkrReset();
BOOL    SpkrSetEmulationType (HWND,DWORD);
void    SpkrUpdate (DWORD);
void    SpkrUpdate_Timer();
void    Spkr_SetErrorInc(const int nErrorInc);
void    Spkr_SetErrorMax(const int nErrorMax);
DWORD   SpkrGetVolume();
#ifdef APPLE2IX
void    SpkrSetVolume(short amplitude);
#else
void    SpkrSetVolume(DWORD dwVolume, DWORD dwVolumeMax);
#endif
void    Spkr_Mute();
void    Spkr_Demute();
bool    Spkr_IsActive();
bool    Spkr_DSInit();
void    Spkr_DSUninit();
#ifdef APPLE2IX
#define __int64
typedef struct {
    unsigned __int64 g_nSpkrLastCycle;
} SS_IO_Speaker;
#endif
DWORD   SpkrGetSnapshot(SS_IO_Speaker* pSS);
DWORD   SpkrSetSnapshot(SS_IO_Speaker* pSS);

#ifdef APPLE2IX
void SpkrToggle();
#else
BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
#endif

#endif /* whole file */

