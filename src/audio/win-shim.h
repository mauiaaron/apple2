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

#ifndef _WINSHIM_H_
#define _WINSHIM_H_

#include "common.h"

#ifdef __APPLE__
#import <MacTypes.h>
#endif

/*
 * This is mostly a shim for Windows-related stuff but also contains some AppleWin-isms
 *
 */

// 2013/09/19 - http://msdn.microsoft.com/en-us/library/windows/desktop/aa383751(v=vs.85).aspx

typedef unsigned int UINT;

typedef unsigned long *LPDWORD;

#if !defined(TRUE)
#define TRUE true
#endif
#if !defined(FALSE)
#define FALSE false
#endif

#define _ASSERT assert
#define Sleep(x) usleep(x)

typedef void *IUnknown;

#define INFINITE 0
#define WAIT_OBJECT_0 0x00000000L

#define LogFileOutput(...) LOG(__VA_ARGS__)

typedef void *(*LPTHREAD_START_ROUTINE)(void *unused);

pthread_t CreateThread(void* unused_lpThreadAttributes, int unused_dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, void *lpParameter, unsigned long unused_dwCreationFlags, LPDWORD lpThreadId);

#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_TIME_CRITICAL 15
bool SetThreadPriority(pthread_t hThread, int nPriority);

#define STILL_ACTIVE 259
bool GetExitCodeThread(pthread_t hThread, unsigned long *lpExitCode);

#endif /* whole file */

