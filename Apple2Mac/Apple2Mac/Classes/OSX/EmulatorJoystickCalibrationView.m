//
//  EmulatorJoystickCalibrationView.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 11/30/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

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
    
    NSAssert(bounds.size.height == 256.f, @"view should be 256pts high");
    NSAssert(bounds.size.width == 256.f, @"view should be 256pts wide");
    
    CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
    CGContextSaveGState(context);
    CGContextSetLineWidth(context, 1);
    CGContextSetRGBStrokeColor(context, 0, 0, 0, 1);
    
    CGContextAddEllipseInRect(context, bounds);
    CGContextStrokePath(context);

    CGFloat x_val = joy_x;
    CGFloat y_val = 256-joy_y;
    
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
