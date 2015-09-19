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

static google_breakpad::ExceptionHandler *eh = nullptr;

static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded) {
    // WARNING : should only do minimal work from within a crashing context ...
    //LOG("Dump path: %s\n", descriptor.path());
    return succeeded;
}

extern "C" {
    static void initializeBreakpadHandler(const char *dumpDir) {
        LOG("Initializing Breakpad with dump dir : %s", dumpDir);
        google_breakpad::MinidumpDescriptor descriptor(dumpDir);
        eh = new google_breakpad::ExceptionHandler(descriptor, NULL, dumpCallback, NULL, true, -1);
    }

    __attribute__((constructor(CTOR_PRIORITY_EARLY)))
    static void _breakpad_registration(void) {
        LOG("Registering Breakpad as handler");
        initializeCrashHandler = &initializeBreakpadHandler;
    }
}

