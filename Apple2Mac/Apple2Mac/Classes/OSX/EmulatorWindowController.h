//
//  EmulatorWindowController.h
//  Apple2Mac
//
//  Created by Aaron Culliney on 9/27/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

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
