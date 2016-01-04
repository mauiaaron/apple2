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

#import "EmulatorWindow.h"

@implementation EmulatorWindow

- (void)keyUp:(NSEvent *)event
{
    [[self windowController] keyUp:event];
}

- (void)keyDown:(NSEvent *)event
{
    [[self windowController] keyDown:event];
}

@end
