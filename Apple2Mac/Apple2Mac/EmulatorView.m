//
//  EmulatorView.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 6/21/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

#import "EmulatorView.h"


void video_driver_init() {
    // TBD ... initialize main game GL view
}

void video_driver_shutdown() {
    // TBD ... destroy main game GL view
}

void video_sync(int ignored) {
    // TBD ...
}

@implementation EmulatorView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        // Initialization code here.
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];
    
    NSString*               hws = @"Hello World!";
    NSPoint                 p;
    NSMutableDictionary*    attribs;
    NSColor*                c;
    NSFont*                 fnt;
    
    p = NSMakePoint( 10, 100 );
    
    attribs = [[[NSMutableDictionary alloc] init] autorelease];
    
    c = [NSColor redColor];
    fnt = [NSFont fontWithName:@"Times Roman" size:48];
    
    [attribs setObject:c forKey:NSForegroundColorAttributeName];
    [attribs setObject:fnt forKey:NSFontAttributeName];
    
    [hws drawAtPoint:p withAttributes:attribs];
}

@end
