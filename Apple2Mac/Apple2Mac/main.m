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
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

extern int argc;
extern const char **argv;

int main(int argc_, const char *argv_[])
{
	int retVal = 1;
    argc = argc_;
    argv = argv_;
    
#if TARGET_OS_IPHONE
    @autoreleasepool {
        retVal = UIApplicationMain(argc, argv, nil, nil);
    }
#else
	retVal = NSApplicationMain(argc, argv);
#endif
	
    return retVal;
}
