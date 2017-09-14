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
#endif

#include "common.h"

int main(int argc_, char *argv_[])
{
    int retVal = 1;
    argc = argc_;
    argv = argv_;
    
    cpu_pause();
    
    @autoreleasepool {
#if TARGET_OS_IPHONE
        retVal = UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
#else
        retVal = NSApplicationMain(argc, (const char **)argv);
#endif
    }

    return retVal;
}
