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
#import "common.h"

#define CAPS_LOCK 0x39
#define SHIFT_LT 0x38
#define SHIFT_RT 0x3c
#define CTRL_LT 0x3b
#define CTRL_RT 0x3e
#define ALT_LT 0x3a
#define ALT_RT 0x3d

@interface EmulatorWindowController ()

@property (assign) IBOutlet EmulatorGLView *view;
@property (assign) IBOutlet NSWindow *disksWindow;
@property (assign) IBOutlet NSWindow *prefsWindow;

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

- (void)dealloc
{
#warning TODO FIXME ... probably should exit emulator if this gets invoked ...
    [super dealloc];
}

- (IBAction)reboot:(id)sender
{
    [[self disksWindow] close];
    cpu65_reboot();
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

- (IBAction)toggleCPUSpeed:(id)sender
{
    timing_toggle_cpu_speed();
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

- (void)flagsChanged:(NSEvent*)event
{
    static BOOL modified_caps_lock = NO;
    switch ([event keyCode])
    {
        case CAPS_LOCK:
        {
            if ([event modifierFlags] & NSAlphaShiftKeyMask)
            {
                modified_caps_lock = YES;
                caps_lock = true;
            }
            else
            {
                caps_lock = false;
            }
        }

        case SHIFT_LT:
            c_keys_handle_input(SCODE_L_SHIFT, ([event modifierFlags] & NSShiftKeyMask), 0);
            break;

        case SHIFT_RT:
            c_keys_handle_input(SCODE_R_SHIFT, ([event modifierFlags] & NSShiftKeyMask), 0);
            break;

        case CTRL_LT:
            c_keys_handle_input(SCODE_L_CTRL, ([event modifierFlags] & NSControlKeyMask), 0);
            break;

        case CTRL_RT:
            c_keys_handle_input(SCODE_R_CTRL, ([event modifierFlags] & NSControlKeyMask), 0);
            break;

        case ALT_LT:
            c_keys_handle_input(SCODE_L_ALT, ([event modifierFlags] & NSAlternateKeyMask), 0);
            break;

        case ALT_RT:
            c_keys_handle_input(SCODE_R_ALT, ([event modifierFlags] & NSAlternateKeyMask), 0);
            break;

        default:
            break;
    }
}

- (void)_handleKeyEvent:(NSEvent *)event pressed:(BOOL)pressed
{
    unichar c = [[event charactersIgnoringModifiers] characterAtIndex:0];
    int scode = (int)c;
    
    int cooked = 0;
    switch (scode)
    {
        case 0x1b:
            scode = SCODE_ESC;
            break;
            
        case NSUpArrowFunctionKey:
            scode = SCODE_U;
            break;
        case NSDownArrowFunctionKey:
            scode = SCODE_D;
            break;
        case NSLeftArrowFunctionKey:
            scode = SCODE_L;
            break;
        case NSRightArrowFunctionKey:
            scode = SCODE_R;
            break;

        case NSF1FunctionKey:
            scode = SCODE_F1;
            break;
        case NSF2FunctionKey:
            scode = SCODE_F2;
            break;
        case NSF3FunctionKey:
            scode = SCODE_F3;
            break;
        case NSF4FunctionKey:
            scode = SCODE_F4;
            break;
        case NSF5FunctionKey:
            scode = SCODE_F5;
            break;
        case NSF6FunctionKey:
            scode = SCODE_F6;
            break;
        case NSF7FunctionKey:
            scode = SCODE_F7;
            break;
        case NSF8FunctionKey:
            scode = SCODE_F8;
            break;
        case NSF9FunctionKey:
            scode = SCODE_F9;
            break;
        case NSF10FunctionKey:
            scode = SCODE_F10;
            break;
        case NSF11FunctionKey:
            scode = SCODE_F11;
            break;
        case NSF12FunctionKey:
            scode = SCODE_F12;
            break;

        case NSInsertFunctionKey:
            scode = SCODE_INS;
            break;
        case NSDeleteFunctionKey:
            scode = SCODE_DEL;
            break;
        case NSHomeFunctionKey:
            scode = SCODE_HOME;
            break;
        case NSEndFunctionKey:
            scode = SCODE_END;
            break;

        case NSPageUpFunctionKey:
            scode = SCODE_PGUP;
            break;
        case NSPageDownFunctionKey:
            scode = SCODE_PGDN;
            break;

        case NSPrintScreenFunctionKey:
            scode = SCODE_PRNT;
            break;
        case NSPauseFunctionKey:
            scode = SCODE_PAUSE;
            break;
        case NSBreakFunctionKey:
            scode = SCODE_BRK;
            break;
            
        default:
            if ([event modifierFlags] & NSControlKeyMask)
            {
                scode = c_keys_ascii_to_scancode(scode);
                cooked = 0;
            }
            else
            {
                cooked = 1;
            }
            break;
    }
    
    c_keys_handle_input(scode, pressed, cooked);
}

- (void)keyUp:(NSEvent *)event
{
    [self _handleKeyEvent:event pressed:NO];
    
    // Allow other character to be handled (or not and beep)
    //[super keyDown:event];
}

- (void)keyDown:(NSEvent *)event
{
    [self _handleKeyEvent:event pressed:YES];

    // Allow other character to be handled (or not and beep)
    //[super keyDown:event];
}

@end
