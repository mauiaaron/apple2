/*
 * Apple // emulator for *ix 
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

// Based on sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#import <Cocoa/Cocoa.h>
#import "EmulatorGLView.h"

#define CAPS_LOCK 0x39
#define SHIFT_LT 0x38
#define SHIFT_RT 0x3c
#define CTRL_LT 0x3b
#define CTRL_RT 0x3e
#define ALT_LT 0x3a
#define ALT_RT 0x3d

extern const NSString *kDrawTimerNotification;

@interface EmulatorWindowController : NSWindowController

@end
