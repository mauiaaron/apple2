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

// Based on sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CVDisplayLink.h>

#import "modelUtil.h"
#import "imageUtil.h"

@interface EmulatorGLView : NSOpenGLView
@end
