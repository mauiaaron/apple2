//
//  EmulatorWindowController.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 9/27/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

// Based on sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#import "EmulatorWindowController.h"
#import "EmulatorFullscreenWindow.h"

@interface EmulatorWindowController ()

@property (nonatomic, assign) IBOutlet EmulatorGLView *view;
@property (nonatomic, retain) EmulatorFullscreenWindow *fullscreenWindow;
@property (nonatomic, retain) NSWindow *standardWindow;

@end


@implementation EmulatorWindowController

@synthesize view = _view;
@synthesize fullscreenWindow = _fullscreenWindow;
@synthesize standardWindow = _standardWindow;

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];

	if (self)
	{
		// Initialize to nil since it indicates app is not fullscreen
		self.fullscreenWindow = nil;
    }

	return self;
}

- (IBAction)toggleFullScreen:(id)sender
{
    if (self.fullscreenWindow)
    {
        [self goWindow];
    }
    else
    {
        [self goFullscreen];
    }
}

- (void)goFullscreen
{
	// If app is already fullscreen...
	if (self.fullscreenWindow)
	{
		//...don't do anything
		return;
	}

	// Allocate a new fullscreen window
	self.fullscreenWindow = [[[EmulatorFullscreenWindow alloc] init] autorelease];

	// Resize the view to screensize
	NSRect viewRect = [self.fullscreenWindow frame];

	// Set the view to the size of the fullscreen window
	[self.view setFrameSize: viewRect.size];

	// Set the view in the fullscreen window
	[self.fullscreenWindow setContentView:self.view];

	self.standardWindow = [self window];

	// Hide non-fullscreen window so it doesn't show up when switching out
	// of this app (i.e. with CMD-TAB)
	[self.standardWindow orderOut:self];

	// Set controller to the fullscreen window so that all input will go to
	// this controller (self)
	[self setWindow:self.fullscreenWindow];

	// Show the window and make it the key window for input
	[self.fullscreenWindow makeKeyAndOrderFront:self];
}

- (void)goWindow
{
	// If controller doesn't have a full screen window...
	if (self.fullscreenWindow == nil)
	{
		//...app is already windowed so don't do anything
		return;
	}

	// Get the rectangle of the original window
	NSRect viewRect = [self.standardWindow frame];
	
	// Set the view rect to the new size
	[self.view setFrame:viewRect];

	// Set controller to the standard window so that all input will go to
	// this controller (self)
	[self setWindow:self.standardWindow];

	// Set the content of the orginal window to the view
	[[self window] setContentView:self.view];

	// Show the window and make it the key window for input
	[[self window] makeKeyAndOrderFront:self];

	// Release/nilify fullscreenWindow
	self.fullscreenWindow = nil;
}

- (void)keyDown:(NSEvent *)event
{
	unichar c = [[event charactersIgnoringModifiers] characterAtIndex:0];

	switch (c)
	{
		case 27:
            NSLog(@"key ESC");
			return;
		case 'f':
            NSLog(@"key 'f'");
			return;
	}

	// Allow other character to be handled (or not and beep)
	[super keyDown:event];
}

@end
