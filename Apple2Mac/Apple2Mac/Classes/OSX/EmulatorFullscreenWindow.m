//
//  EmulatorFullscreenWindow.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 9/27/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

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
