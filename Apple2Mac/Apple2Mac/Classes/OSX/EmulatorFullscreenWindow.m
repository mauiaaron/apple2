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

#import "EmulatorFullscreenWindow.h"

@implementation EmulatorFullscreenWindow

- (id)init
{
    // Create a screen-sized window on the display you want to take over
    NSRect screenRect = [[NSScreen mainScreen] frame];

    // Initialize the window making it size of the screen and borderless
    self = [super initWithContentRect:screenRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];

    // Set the window level to be above the menu bar to cover everything else
    [self setLevel:NSMainMenuWindowLevel+1];

    // Set opaque
    [self setOpaque:YES];

    // Hide this when user switches to another window (or app)
    [self setHidesOnDeactivate:YES];

    return self;
}

- (BOOL)canBecomeKeyWindow
{
    // Return yes so that this borderless window can receive input
    return YES;
}

- (void)keyUp:(NSEvent *)event
{
    [[self windowController] keyUp:event];
}

- (void)keyDown:(NSEvent *)event
{
    [[self windowController] keyDown:event];
}

@end
