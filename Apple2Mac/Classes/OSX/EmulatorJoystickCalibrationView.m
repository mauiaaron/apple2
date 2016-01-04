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

#import "EmulatorJoystickCalibrationView.h"
#import "common.h"
#import <QuartzCore/QuartzCore.h>

@implementation EmulatorJoystickCalibrationView

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];
    [self drawView];
}

- (void)drawView
{
    static int pulseSize = 1;
    
    CGRect bounds = [self bounds];
    
    NSAssert(bounds.size.height == JOY_RANGE, @"view should be 256pts high");
    NSAssert(bounds.size.width == JOY_RANGE, @"view should be 256pts wide");
    
    CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
    CGContextSaveGState(context);
    CGContextSetLineWidth(context, 1);
    
    if (joy_clip_to_radius && (joy_mode == JOY_PCJOY))
    {
        CGContextSetRGBStrokeColor(context, 0, 0, 0, 1);
        CGContextAddEllipseInRect(context, bounds);
        CGContextStrokePath(context);
    }

    CGFloat x_val = joy_x;
    CGFloat y_val = JOY_RANGE-joy_y;
    
    CGContextSetRGBStrokeColor(context, 1, 0, 0, 1);
    CGRect cursor = CGRectMake(x_val-(pulseSize/2.f), y_val-(pulseSize/2.f), pulseSize, pulseSize);
    CGContextAddEllipseInRect(context, cursor);

    CGContextStrokePath(context);
    CGContextRestoreGState(context);
    
    pulseSize = ((pulseSize+1) & 0x7) +1;
}

- (void)keyUp:(NSEvent *)event
{
    [[self nextResponder] keyUp:event];
}

- (void)keyDown:(NSEvent *)event
{
    [[self nextResponder] keyDown:event];
}

@end
