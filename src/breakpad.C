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

// Breakpad crash handling

#if !defined(__cplusplus)
#error Breakpad requires C++
#endif

#include "common.h"
#include "client/linux/handler/exception_handler.h"
#if EMBEDDED_STACKWALKER
#   include "processor/stackwalk_common.h"
#endif

static CrashHandler_s breakpadHandler = { 0 };

static google_breakpad::ExceptionHandler *eh = nullptr;

static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded) {
    // WARNING : should only do minimal work from within a crashing context ...
    LOG("Dump path: %s", descriptor.path());
    return succeeded;
}

extern "C" {
    static void initBreakpadHandler(const char *dumpDir) {
        LOG("Initializing Breakpad with dump dir : %s", dumpDir);
        google_breakpad::MinidumpDescriptor descriptor(dumpDir);
        eh = new google_breakpad::ExceptionHandler(descriptor, NULL, dumpCallback, NULL, true, -1);
    }

    __attribute__((destructor(255)))
    static void shutdownBreakpadHandler(void) {
        if (eh) {
            delete eh;
            eh = nullptr;
            LOG("Unregistering Breakpad...");
        }
    }

    static bool processCrashWithBreakpad(const char *crash, const char *symbolsPath, const FILE *outputFile) {
#if EMBEDDED_STACKWALKER
        LOG("Running breakpad stackwalker on crash ...");
        stackwalker_setOutputFile(outputFile);
        int err = stackwalker_main(crash, symbolsPath, /*machineReadable:*/true);
        stackwalker_setOutputFile(NULL);
        return err == 0;
#else
#warning NOT COMPILING WITH EMBEDDED STACKWALKER
        return false;
#endif
    }

    __attribute__((constructor(CTOR_PRIORITY_EARLY)))
    static void _breakpad_registration(void) {
        LOG("Registering Breakpad as native crash handler");
        breakpadHandler.init = &initBreakpadHandler;
        breakpadHandler.shutdown = &shutdownBreakpadHandler;
        breakpadHandler.processCrash = &processCrashWithBreakpad;
        crashHandler = &breakpadHandler;
    }
}

