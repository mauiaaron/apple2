/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#ifndef _MISC_H_
#define _MISC_H_

enum {
    CTOR_PRIORITY_FIRST = 101,
    CTOR_PRIORITY_EARLY = 111,
    CTOR_PRIORITY_LATE  = 201,
};

// top installation directory
extern const char *data_dir;

#if MOBILE_DEVICE
#   define HOMEDIR data_dir
#else
#   define HOMEDIR getenv("HOME")
#endif

// global ref to CLI args
extern char **argv;
extern int argc;

// locale
extern const char *locale;

//
// Emulator lifecycle
//

typedef void (*startup_callback_f)(void);

// register a startup function
void emulator_registerStartupCallback(long order, startup_callback_f callback);

// start emulator (CPU, audio, and video)
void emulator_start(void);

// shutdown emulator in preparation for app exit
void emulator_shutdown(void);

// is emulator shutting down?
bool emulator_isShuttingDown(void);

//
// Emulator state save/restore
//

typedef struct StateHelper_s {
    int fd;
    int version;
    int diskFdA;
    int diskFdB;
    bool (*save)(int fd, const uint8_t * outbuf, ssize_t outmax);
    bool (*load)(int fd, uint8_t * inbuf, ssize_t inmax);
} StateHelper_s;

// save current emulator state
bool emulator_saveState(int fdState);

// load emulator state from save file descriptor
bool emulator_loadState(int fdState, int fdA, int fdB);

// extract path(s) and readonly status(es) from save state file into json
bool emulator_stateExtractDiskPaths(int fdState, JSON_ref json);

//
// Crash handling ...
//

typedef struct CrashHandler_s {

    /**
     * Initialize crash handler (if available)
     */
    void (*init)(const char *dumpDir);

    /**
     * Shutdown crash handler (if available)
     */
    void (*shutdown)(void);

    /**
     * Processes a crash dump (assuming this is a non-crashing contest).
     * Returns success value. On failure, the outputFile may contain the reason processing failed
     */
    bool (*processCrash)(const char *crash, const char *symbolsPath, const FILE *outputFile);

} CrashHandler_s;

extern CrashHandler_s *crashHandler;

#endif
