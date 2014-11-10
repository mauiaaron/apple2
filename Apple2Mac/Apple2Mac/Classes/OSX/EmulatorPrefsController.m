//
//  EmulatorPrefsController.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 10/18/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

#import "EmulatorPrefsController.h"
#import "EmulatorDiskController.h"
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
#define kApple2JoystickStep @"kApple2JoystickStep"

@interface EmulatorPrefsController ()

@property (assign) IBOutlet NSWindow *prefsWindow;

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
@property (assign) IBOutlet NSTextField *joystickStepLabel;
@property (assign) IBOutlet NSStepper *joystickStepper;

@property (assign) IBOutlet NSTextField *button0Pressed;
@property (assign) IBOutlet NSTextField *button1Pressed;
@property (assign) IBOutlet NSView *joystickCalibrationView;

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
    
    joy_auto_recenter = [defaults integerForKey:kApple2JoystickAutoRecenter];
    [self.joystickRecenter setState:joy_auto_recenter ? NSOnState : NSOffState];
    joy_step = [defaults integerForKey:kApple2JoystickStep];
    [self.joystickStepLabel setIntegerValue:joy_step];
    [self.joystickStepper setIntegerValue:joy_step];
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
    [defaults setBool:joy_auto_recenter forKey:kApple2JoystickAutoRecenter];
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
    
#warning HACK TODO FIXME ... refactor timing stuff
    timing_toggle_cpu_speed();
    timing_toggle_cpu_speed();
    
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
    
#warning HACK TODO FIXME ... refactor timing stuff
    timing_toggle_cpu_speed();
    timing_toggle_cpu_speed();

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
    video_set(0);
    video_setpage(!!(softswitches & SS_SCREEN));
    video_redraw();
}

- (IBAction)soundCardChoiceChanged:(id)sender
{
#warning TODO : make soundcard configurable at runtime
}

- (IBAction)joystickChoiceChanged:(id)sender
{
    NSInteger mode = [self.joystickChoice indexOfSelectedItem];
    if (! ((mode >= JOY_PCJOY) && (mode < NUM_JOYOPTS)) )
    {
        mode = JOY_PCJOY;
    }
    joy_mode = (joystick_mode_t)mode;
    [self _savePrefs];
}

- (IBAction)autoRecenterChoiceChanged:(id)sender
{
    joy_auto_recenter = ([self.joystickRecenter state] == NSOnState);
    [self _savePrefs];
}

- (IBAction)stepValueChanged:(id)sender
{
    joy_step = [self.joystickStepper intValue];
    [self.joystickStepLabel setIntegerValue:joy_step];
    [self _savePrefs];
}

- (IBAction)calibrateJoystick:(id)sender
{
}

@end
