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

@interface EmulatorPrefsController ()

@property (assign) IBOutlet NSSlider *cpuSlider;
@property (assign) IBOutlet NSSlider *altSlider;
@property (assign) IBOutlet NSTextField *cpuSliderLabel;
@property (assign) IBOutlet NSTextField *altSliderLabel;
@property (assign) IBOutlet NSButton *cpuMaxChoice;
@property (assign) IBOutlet NSButton *altMaxChoice;

@property (assign) IBOutlet NSMatrix *soundCardChoice;

@property (assign) IBOutlet NSPopUpButton *colorChoice;
@property (assign) IBOutlet NSPopUpButton *monochromeColorChoice;
@property (assign) IBOutlet NSButton *scanlinesChoice;

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

- (void)loadPrefsForDomain:(const char *)domain;

@end

static EmulatorPrefsController *prefsInstance = nil;

static void prefsChangeCallback(const char *domain)
{
    (void)domain;
    assert(prefsInstance);
    [prefsInstance loadPrefsForDomain:domain];
}

@implementation EmulatorPrefsController

- (void)awakeFromNib
{
    assert(!prefsInstance);
    prefsInstance = self;

    prefs_registerListener(PREF_DOMAIN_AUDIO, prefsChangeCallback);
    prefs_registerListener(PREF_DOMAIN_INTERFACE, prefsChangeCallback);
    prefs_registerListener(PREF_DOMAIN_JOYSTICK, prefsChangeCallback);
    prefs_registerListener(PREF_DOMAIN_KEYBOARD, prefsChangeCallback);
    //prefs_registerListener(PREF_DOMAIN_TOUCHSCREEN, prefsChangeCallback);
    prefs_registerListener(PREF_DOMAIN_VIDEO, prefsChangeCallback);
    prefs_registerListener(PREF_DOMAIN_VM, prefsChangeCallback);

    [self _setupJoystickUI];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(drawJoystickCalibration:) name:(NSString *)kDrawTimerNotification object:nil];
}

- (void)loadPrefsForDomain:(const char *)domain
{
    [self.cpuSlider setDoubleValue:cpu_scale_factor];
    [self.cpuSliderLabel setStringValue:[NSString stringWithFormat:@"%.0f%%", cpu_scale_factor*100]];
    if (cpu_scale_factor == CPU_SCALE_FASTEST)
    {
        [self.cpuMaxChoice setState:NSOnState];
        [self.cpuSlider setEnabled:NO];
    }
    else
    {
        [self.cpuMaxChoice setState:NSOffState];
        [self.cpuSlider setEnabled:YES];
    }
    
    [self.altSlider setDoubleValue:cpu_altscale_factor];
    [self.altSliderLabel setStringValue:[NSString stringWithFormat:@"%.0f%%", cpu_altscale_factor*100]];
    if (cpu_altscale_factor == CPU_SCALE_FASTEST)
    {
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

    long lVal = 0;
    NSInteger mode = prefs_parseLongValue(domain, PREF_COLOR_MODE, &lVal, /*base:*/10) ? getColorMode(lVal) : COLOR_MODE_DEFAULT;
    [self.colorChoice selectItemAtIndex:mode];

    [self.joystickChoice selectItemAtIndex:(NSInteger)joy_mode];
    
#ifdef KEYPAD_JOYSTICK
    [self.joystickRecenter setState:joy_auto_recenter ? NSOnState : NSOffState];

    [self.joystickStepLabel setIntegerValue:joy_step];
    [self.joystickStepper setIntegerValue:joy_step];
#endif
    
    [self.joystickClipToRadius setState:joy_clip_to_radius ? NSOnState : NSOffState];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (IBAction)sliderDidMove:(id)sender
{
    NSSlider *slider = (NSSlider *)sender;
    double value = [slider doubleValue];
    if (slider == self.cpuSlider)
    {
        prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, value);
        [self.cpuSliderLabel setStringValue:[NSString stringWithFormat:@"%.0f%%", value*100]];
    }
    else
    {
        prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, value);
        [self.altSliderLabel setStringValue:[NSString stringWithFormat:@"%.0f%%", value*100]];
    }
    
    prefs_sync(PREF_DOMAIN_VM);
    prefs_save();
}

- (IBAction)peggedChoiceChanged:(id)sender
{
    NSButton *maxButton = (NSButton *)sender;
    if (maxButton == self.cpuMaxChoice)
    {
        [self.cpuSlider setEnabled:([maxButton state] != NSOnState)];
        double value = ([maxButton state] == NSOnState) ? CPU_SCALE_FASTEST : [self.cpuSlider doubleValue];
        prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, value);
    }
    else
    {
        [self.altSlider setEnabled:([maxButton state] != NSOnState)];
        double value = ([maxButton state] == NSOnState) ? CPU_SCALE_FASTEST : [self.altSlider doubleValue];
        prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, value);
    }

    prefs_sync(PREF_DOMAIN_VM);
    prefs_save();
}

- (IBAction)colorChoiceChanged:(id)sender
{
    NSInteger mode = [self.colorChoice indexOfSelectedItem];
    mode = getColorMode(mode);
    prefs_setLongValue(PREF_DOMAIN_VIDEO, PREF_COLOR_MODE, mode);
    prefs_sync(PREF_DOMAIN_VIDEO);
    prefs_save();
}

- (IBAction)monochromeColorChoiceChanged:(id)sender {
    NSInteger mode = [self.monochromeColorChoice indexOfSelectedItem];
    mode = getMonoMode(mode);
    prefs_setLongValue(PREF_DOMAIN_VIDEO, PREF_MONO_MODE, mode);
    prefs_sync(PREF_DOMAIN_VIDEO);
    prefs_save();
}

- (IBAction)scanlinesChoiceChanged:(id)sender
{
    NSControlStateValue state = [self.scanlinesChoice state];
    prefs_setBoolValue(PREF_DOMAIN_VIDEO, PREF_SHOW_HALF_SCANLINES, state == NSControlStateValueOn);
    prefs_sync(PREF_DOMAIN_VIDEO);
    prefs_save();
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
    mode = getJoyMode(mode);
    prefs_setLongValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_MODE, mode);
    prefs_sync(PREF_DOMAIN_JOYSTICK);
    prefs_save();
}

- (IBAction)autoRecenterChoiceChanged:(id)sender
{
    bool autoRecenter = ([self.joystickRecenter state] == NSOnState);
    prefs_setBoolValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_KPAD_AUTO_RECENTER, autoRecenter);
    prefs_sync(PREF_DOMAIN_JOYSTICK);
    prefs_save();
}

- (IBAction)clipToRadiusChoiceChanged:(id)sender
{
    bool clipToRadius = ([self.joystickClipToRadius state] == NSOnState);
    prefs_setBoolValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_CLIP_TO_RADIUS, clipToRadius);
    prefs_sync(PREF_DOMAIN_JOYSTICK);
    prefs_save();
}

- (IBAction)stepValueChanged:(id)sender
{
    long joyStep = [self.joystickStepper intValue];
    [self.joystickStepLabel setIntegerValue:joyStep];
    prefs_setLongValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_KPAD_STEP, joyStep);
    prefs_sync(PREF_DOMAIN_JOYSTICK);
    prefs_save();
}

#pragma mark -
#pragma Joystick calibration

- (void)drawJoystickCalibration:(NSNotification *)notification
{
    if (![self.joystickCalibrationView isHidden])
    {
        [self.joystickCalibrationView setNeedsDisplay:YES];
        [self.button0Pressed setHidden:!(run_args.joy_button0)];
        [self.button1Pressed setHidden:!(run_args.joy_button1)];
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
    [self.button0Pressed setHidden:!(run_args.joy_button0)];
    [self.button1Pressed setHidden:!(run_args.joy_button1)];
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
