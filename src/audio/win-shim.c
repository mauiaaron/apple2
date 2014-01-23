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

#include "common.h"
#include "audio/win-shim.h"

pthread_t CreateThread(void* unused_lpThreadAttributes, int unused_dwStackSize, LPTHREAD_START_ROUTINE lpStartRoutine, LPVOID lpParameter, DWORD unused_dwCreationFlags, LPDWORD lpThreadId)
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

    return 1;
}

bool GetExitCodeThread(pthread_t thread, unsigned long *lpExitCode)
{
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
    return 1;
}

