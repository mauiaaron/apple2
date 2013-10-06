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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// 2013/09/19 - http://msdn.microsoft.com/en-us/library/windows/desktop/aa383751(v=vs.85).aspx

typedef unsigned long DWORD;
typedef unsigned long ULONG;
typedef long LONG;
typedef long HRESULT;
typedef unsigned int UINT;
typedef bool BOOL;
typedef char TCHAR;
typedef short SHORT;
typedef unsigned short WORD;
typedef unsigned char BYTE;

typedef long *LPLONG;
typedef void *LPVOID;
typedef void *LPDVOID;
typedef DWORD *LPDWORD;

typedef char *GUID; // HACK
typedef GUID IID;
typedef IID* REFIID;

typedef GUID *LPGUID;
typedef char *LPCSTR;
typedef LPCSTR LPCTSTR;

typedef unsigned int UINT_PTR;

typedef void *HWND; // HACK

typedef int64_t __int64;

#define VOID void

// unneeded ???
#define __stdcall
#define WINAPI
#define CALLBACK
#define FAR

typedef bool BOOL;
#define TRUE true
#define FALSE false

extern FILE *g_fh;

#define _strdup strdup
#define _ASSERT assert
#define Sleep(x) sleep(x)

typedef void *IUnknown;

#define TEXT(X) X

#define MB_ICONEXCLAMATION 0x00000030L
#define MB_SETFOREGROUND 0x00010000L

#define MessageBox(window, message, group, flags) LOG("%s", message)

#endif /* whole file */

