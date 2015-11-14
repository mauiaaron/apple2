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

#import "EmulatorPrefsController.h"
#import "EmulatorDiskController.h"
#import "EmulatorJoystickCalibrationView.h"
#import "EmulatorWindowController.h"
#import "common.h"

#define kApple2SavedPrefs @"kApple2SavedPrefs"
#define kApple2CPUSpeed @"kApple2CPUSpeed"
#define kApple2CPUSpeedIsMax @"kApple2CPUSpeedIsMax"
#define kApple2AltSpeed @"kApple2AltSpeed"
#define kApple2AltSpeedIsMax @"kApple2AltSpeedIsMax"
#define kApple2SoundcardConfig @"kApple2SoundcardConfig"
#define kApple2ColorConfig @"kApple2ColorConfig"
#define kApple2JoystickConfig @"kApple2JoystickConfig"
#define kApple2JoystickAutoRecenter @"kApple2JoystickAutoRecenter"
#define kApple2JoystickClipToRadius @"kApple2JoystickClipToRadius"
#define kApple2JoystickStep @"kApple2JoystickStep"

@interface EmulatorPrefsController ()

@property (assign) IBOutlet NSSlider *cpuSlider;
@property (assign) IBOutlet NSSlider *altSlider;
@property (assign) IBOutlet NSTextField *cpuSliderLabel;
@property (assign) IBOutlet NSTextField *altSliderLabel;
@property (assign) IBOutlet NSButton *cpuMaxChoice;
@property (assign) IBOutlet NSButton *altMaxChoice;

@property (assign) IBOutlet NSMatrix *soundCardChoice;

@property (assign) IBOutlet NSPopUpButton *colorChoice;

@property (assign) IBOutlet NSPopUpButton *joystickChoice;
@property (assign) IBOutlet NSButton *joystickRecenter;
@property (assign) IBOutlet NSButton *joystickClipToRadius;
@property (assign) IBOutlet NSTextField *joystickStepLabel;
@property (assign) IBOutlet NSStepper *joystickStepper;
@property (assign) IBOutlet NSTextField *joystickStepperLabel;
@property (assign) IBOutlet NSTextField *joystickKPadNotes;
@property (assign) IBOutlet NSTextField *joystickDeviceNotes;

@property (assign) IBOutlet NSTextField *button0Pressed;
@property (assign) IBOutlet NSTextField *button1Pressed;
@property (assign) IBOutlet EmulatorJoystickCalibrationView *joystickCalibrationView;

@end

@implementation EmulatorPrefsController

- (void)awakeFromNib
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    BOOL firstTime = ![defaults boolForKey:kApple2SavedPrefs];
    if (firstTime)
    {
        [defaults setBool:YES forKey:kApple2SavedPrefs];
        [defaults setDouble:1.0 forKey:kApple2CPUSpeed];
        [defaults setDouble:CPU_SCALE_SLOWEST forKey:kApple2AltSpeed];
        [defaults setBool:NO forKey:kApple2CPUSpeedIsMax];
        [defaults setBool:NO forKey:kApple2AltSpeedIsMax];
        [defaults setInteger:COLOR_INTERP forKey:kApple2ColorConfig];
        [defaults setInteger:JOY_KPAD forKey:kApple2JoystickConfig];
        [defaults setBool:YES forKey:kApple2JoystickAutoRecenter];
        [defaults removeObjectForKey:kApple2PrefStartupDiskA];
        [defaults removeObjectForKey:kApple2PrefStartupDiskB];
    }
    
    cpu_scale_factor = [defaults doubleForKey:kApple2CPUSpeed];
    [self.cpuSlider setDoubleValue:cpu_scale_factor];
    [self.cpuSliderLabel setStringValue:[NSString stringWithFormat:@"%.0f%%", cpu_scale_factor*100]];
    if ([defaults boolForKey:kApple2CPUSpeedIsMax])
    {
        cpu_scale_factor = CPU_SCALE_FASTEST;
        [self.cpuMaxChoice setState:NSOnState];
        [self.cpuSlider setEnabled:NO];
    }
    else
    {
        [self.cpuMaxChoice setState:NSOffState];
        [self.cpuSlider setEnabled:YES];
    }
    
    cpu_altscale_factor = [defaults doubleForKey:kApple2AltSpeed];
    [self.altSlider setDoubleValue:cpu_altscale_factor];
    [self.altSliderLabel setStringValue:[NSString stringWithFormat:@"%.0f%%", cpu_altscale_factor*100]];
    if ([defaults boolForKey:kApple2AltSpeedIsMax])
    {
        cpu_altscale_factor = CPU_SCALE_FASTEST;
        [self.altMaxChoice setState:NSOnState];
        [self.altSlider setEnabled:NO];
    }
    else
    {
        [self.altMaxChoice setState:NSOffState];
        [self.altSlider setEnabled:YES];
    }
    
#warning TODO : actually implement sound card choices
    [self.soundCardChoice deselectAllCells];
    [self.soundCardChoice selectCellAtRow:1 column:0];
    
    NSInteger mode = [defaults integerForKey:kApple2ColorConfig];
    if (! ((mode >= COLOR_NONE) && (mode < NUM_COLOROPTS)) )
    {
        mode = COLOR_NONE;
    }
    [self.colorChoice selectItemAtIndex:mode];
    color_mode = (color_mode_t)mode;
    
    mode = [defaults integerForKey:kApple2JoystickConfig];
    if (! ((mode >= JOY_PCJOY) && (mode < NUM_JOYOPTS)) )
    {
        mode = JOY_PCJOY;
    }
    joy_mode = (joystick_mode_t)mode;
    [self.joystickChoice selectItemAtIndex:mode];
    
#ifdef KEYPAD_JOYSTICK
    joy_auto_recenter = [defaults integerForKey:kApple2JoystickAutoRecenter];
    [self.joystickRecenter setState:joy_auto_recenter ? NSOnState : NSOffState];
    joy_step = [defaults integerForKey:kApple2JoystickStep];
    if (!joy_step)
    {
        joy_step = 1;
    }
    [self.joystickStepLabel setIntegerValue:joy_step];
    [self.joystickStepper setIntegerValue:joy_step];
#endif
    
    joy_clip_to_radius = [defaults boolForKey:kApple2JoystickClipToRadius];
    [self.joystickClipToRadius setState:joy_clip_to_radius ? NSOnState : NSOffState];
    
    [self _setupJoystickUI];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(drawJoystickCalibration:) name:(NSString *)kDrawTimerNotification object:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)_savePrefs
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:YES forKey:kApple2SavedPrefs];
    [defaults setDouble:cpu_scale_factor forKey:kApple2CPUSpeed];
    [defaults setDouble:cpu_altscale_factor forKey:kApple2AltSpeed];
    [defaults setBool:([self.cpuMaxChoice state] == NSOnState) forKey:kApple2CPUSpeedIsMax];
    [defaults setBool:([self.altMaxChoice state] == NSOnState) forKey:kApple2AltSpeedIsMax];
    [defaults setInteger:color_mode forKey:kApple2ColorConfig];
    [defaults setInteger:joy_mode forKey:kApple2JoystickConfig];
    [defaults setInteger:joy_step forKey:kApple2JoystickStep];
    [defaults setBool:joy_auto_recenter forKey:kApple2JoystickAutoRecenter];
    [defaults setBool:joy_clip_to_radius forKey:kApple2JoystickClipToRadius];
}

- (IBAction)sliderDidMove:(id)sender
{
    NSSlider *slider = (NSSlider *)sender;
    double value = [slider doubleValue];
    if (slider == self.cpuSlider)
    {
        cpu_scale_factor = value;
        [self.cpuSliderLabel setStringValue:[NSString stringWithFormat:@"%.0f%%", value*100]];
    }
    else
    {
        cpu_altscale_factor = value;
        [self.altSliderLabel setStringValue:[NSString stringWithFormat:@"%.0f%%", value*100]];
    }
    
    timing_initialize();
    
    [self _savePrefs];
}

- (IBAction)peggedChoiceChanged:(id)sender
{
    NSButton *maxButton = (NSButton *)sender;
    if (maxButton == self.cpuMaxChoice)
    {
        [self.cpuSlider setEnabled:([maxButton state] != NSOnState)];
        cpu_scale_factor = ([maxButton state] == NSOnState) ? CPU_SCALE_FASTEST : [self.cpuSlider doubleValue];
    }
    else
    {
        [self.altSlider setEnabled:([maxButton state] != NSOnState)];
        cpu_altscale_factor = ([maxButton state] == NSOnState) ? CPU_SCALE_FASTEST : [self.altSlider doubleValue];
    }
    
    timing_initialize();

    [self _savePrefs];
}

- (IBAction)colorChoiceChanged:(id)sender
{
    NSInteger mode = [self.colorChoice indexOfSelectedItem];
    if (! ((mode >= COLOR_NONE) && (mode < NUM_COLOROPTS)) )
    {
        mode = COLOR_NONE;
    }
    color_mode = (color_mode_t)mode;
    [self _savePrefs];
    
#warning HACK TODO FIXME need to refactor video resetting procedure
    video_reset();
    video_setpage(!!(softswitches & SS_SCREEN));
    video_redraw();
}

- (IBAction)soundCardChoiceChanged:(id)sender
{
#warning TODO : make soundcard configurable at runtime
}

#pragma mark -
#pragma mark Joystick preferences

- (void)_setupJoystickUI
{
    [self.joystickRecenter setHidden:(joy_mode == JOY_PCJOY)];
    [self.joystickClipToRadius setHidden:(joy_mode != JOY_PCJOY)];
    [self.joystickStepLabel setHidden:(joy_mode == JOY_PCJOY)];
    [self.joystickStepper setHidden:(joy_mode == JOY_PCJOY)];
    [self.joystickStepperLabel setHidden:(joy_mode == JOY_PCJOY)];
    [self.joystickKPadNotes setHidden:(joy_mode == JOY_PCJOY)];
    [self.joystickDeviceNotes setHidden:(joy_mode != JOY_PCJOY)];
}

- (IBAction)joystickChoiceChanged:(id)sender
{
    NSInteger mode = [self.joystickChoice indexOfSelectedItem];
    if (! ((mode >= JOY_PCJOY) && (mode < NUM_JOYOPTS)) )
    {
        mode = JOY_PCJOY;
    }
    joy_mode = (joystick_mode_t)mode;
    [self _setupJoystickUI];
    [self _savePrefs];
}

- (IBAction)autoRecenterChoiceChanged:(id)sender
{
    joy_auto_recenter = ([self.joystickRecenter state] == NSOnState);
    [self _savePrefs];
}

- (IBAction)clipToRadiusChoiceChanged:(id)sender
{
    joy_clip_to_radius = ([self.joystickClipToRadius state] == NSOnState);
    [self _savePrefs];
}

- (IBAction)stepValueChanged:(id)sender
{
    joy_step = [self.joystickStepper intValue];
    [self.joystickStepLabel setIntegerValue:joy_step];
    [self _savePrefs];
}

#pragma mark -
#pragma Joystick calibration

- (void)drawJoystickCalibration:(NSNotification *)notification
{
    if (![self.joystickCalibrationView isHidden])
    {
        [self.joystickCalibrationView setNeedsDisplay:YES];
        [self.button0Pressed setHidden:!(joy_button0)];
        [self.button1Pressed setHidden:!(joy_button1)];
    }
}

- (void)flagsChanged:(NSEvent*)event
{
    // NOTE : yay, awesome! checking for ([event modifierFlags] & NSAlternateKeyMask) does not work properly if both were pressed and then one ALT key is unpressed ... kudoes to Apple for an excellent key-handling API /sarc
    static BOOL leftAltEngaged = NO;
    static BOOL rightAltEngaged = NO;
    switch ([event keyCode])
    {
        case ALT_LT:
            leftAltEngaged = !leftAltEngaged;
            c_keys_handle_input(SCODE_L_ALT, /*pressed:*/leftAltEngaged, /*cooked:*/0);
            break;
        case ALT_RT:
            rightAltEngaged = !rightAltEngaged;
            c_keys_handle_input(SCODE_R_ALT, /*pressed:*/rightAltEngaged, /*cooked:*/0);
            break;
        default:
            break;
    }
    if (!([event modifierFlags] & NSAlternateKeyMask))
    {
        // But we can trust the system state if no alt modifier exists ... this resets b0rken edge cases
        leftAltEngaged = NO;
        rightAltEngaged = NO;
    }
    [self.button0Pressed setHidden:!(joy_button0)];
    [self.button1Pressed setHidden:!(joy_button1)];
}

- (void)_handleKeyEvent:(NSEvent *)event pressed:(BOOL)pressed
{
    unichar c = [[event charactersIgnoringModifiers] characterAtIndex:0];
    int scode = (int)c;
    switch (scode)
    {
        case NSUpArrowFunctionKey:
            c_keys_handle_input(SCODE_U, pressed, /*cooked:*/0);
            break;
        case NSDownArrowFunctionKey:
            c_keys_handle_input(SCODE_D, pressed, /*cooked:*/0);
            break;
        case NSLeftArrowFunctionKey:
            c_keys_handle_input(SCODE_L, pressed, /*cooked:*/0);
            break;
        case NSRightArrowFunctionKey:
            c_keys_handle_input(SCODE_R, pressed, /*cooked:*/0);
            break;
        default:
            break;
    }
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
