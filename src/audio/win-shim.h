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

typedef unsigned long DWORD;
#ifdef __APPLE__
typedef UInt32 ULONG;
typedef SInt32 HRESULT;
typedef signed char BOOL;
#else
typedef unsigned long ULONG;
typedef long HRESULT;
typedef bool BOOL;
#endif
typedef long LONG;
typedef unsigned int UINT;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef char TCHAR;
typedef uint8_t UCHAR;
typedef int16_t INT16;
typedef short SHORT;
typedef unsigned short USHORT;
typedef unsigned short WORD;
typedef unsigned char BYTE;

typedef long *LPLONG;
typedef void *LPVOID;
typedef void *LPDVOID;
typedef char *LPBYTE;
typedef DWORD *LPDWORD;

typedef char *GUID; // HACK
typedef GUID IID;
#if !defined(__APPLE__)
typedef IID* REFIID;
#endif

typedef GUID *LPGUID;
typedef char *LPCSTR;
typedef LPCSTR LPCTSTR;

typedef unsigned int UINT_PTR;

typedef void *HWND; // HACK

typedef int64_t __int64;

typedef void* HANDLE;

#define VOID void

// unneeded ???
#define __stdcall
#define WINAPI
#define CALLBACK
#define FAR

#if !defined(TRUE)
#define TRUE true
#endif
#if !defined(FALSE)
#define FALSE false
#endif

#define _strdup strdup
#define _ASSERT assert
#define Sleep(x) usleep(x)

typedef void *IUnknown;

#define TEXT(X) X

#define MB_ICONEXCLAMATION 0x00000030L
#define MB_SETFOREGROUND 0x00010000L

#define MessageBox(window, message, group, flags) LOG("%s", message)

#define INFINITE 0
#define WAIT_OBJECT_0 0x00000000L

#define LogFileOutput(...) LOG(__VA_ARGS__)

typedef LPVOID (*LPTHREAD_START_ROUTINE)(LPVOID unused);

pthread_t CreateThread(void* unused_lpThreadAttributes, int unused_dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD unused_dwCreationFlags, LPDWORD lpThreadId);

#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_TIME_CRITICAL 15
bool SetThreadPriority(pthread_t hThread, int nPriority);

#define STILL_ACTIVE 259
bool GetExitCodeThread(pthread_t hThread, unsigned long *lpExitCode);

#endif /* whole file */

