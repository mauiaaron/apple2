//
//  EmulatorWindow.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 10/13/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

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
