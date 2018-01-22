/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2014, 2015 Aaron Culliney
 *
 */

#if TARGET_OS_IPHONE
#   import "AppDelegate.h"
#elif TARGET_OS_MAC
#   import <AppKit/NSApplication.h>
#else
#   error what new devilry is this?
#endif

#include "common.h"

int main(int argc_, char *argv_[])
{
    int retVal = 1;
    argc = argc_;
    argv = argv_;

#if !TESTING
    cpu_pause();
#endif

    @autoreleasepool {
#if TARGET_OS_IPHONE
        retVal = UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
#else
        retVal = NSApplicationMain(argc, (const char **)argv);
#endif
    }

    return retVal;
}
