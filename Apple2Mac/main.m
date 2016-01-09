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

extern int argc;
extern const char **argv;

int main(int argc_, const char *argv_[])
{
    int retVal = 1;
    argc = argc_;
    argv = argv_;
    
    @autoreleasepool {
#if TARGET_OS_IPHONE
        retVal = UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
#else
        retVal = NSApplicationMain(argc, argv);
#endif
    }

    return retVal;
}
