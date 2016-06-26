//
//  iosPrefControllerViewController.m
//  Apple2Mac
//
//  Created by Jerome Vernet on 24/12/2015.
//  Copyright © 2015 deadc0de.org. All rights reserved.
//

#import "iosPrefControllerViewController.h"


#import "common.h"
#import "modelUtil.h"


@implementation iosPrefControllerViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    self._videoMode  = [[NSArray alloc]  initWithObjects:@"Mono",@"Color", nil];
    
    // Connect data
    self.videoModePicker.dataSource = self;
    self.videoModePicker.delegate = self;

    
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
       // [defaults setInteger:JOY_KPAD forKey:kApple2JoystickConfig];
        [defaults setBool:YES forKey:kApple2JoystickAutoRecenter];
        [defaults removeObjectForKey:kApple2PrefStartupDiskA];
        [defaults removeObjectForKey:kApple2PrefStartupDiskB];
    }
    
    cpu_scale_factor = [defaults doubleForKey:kApple2CPUSpeed];
    [self.cpuSlider setValue:cpu_scale_factor animated:NO];
    self.cpuSliderLabel.text=[NSString stringWithFormat:@"%.0f%%", cpu_scale_factor*100];
    if ([defaults boolForKey:kApple2CPUSpeedIsMax])
    {
        cpu_scale_factor = CPU_SCALE_FASTEST;
        [self.cpuMaxChoice setOn:YES];
        [self.cpuSlider setEnabled:NO];
    }
    else
    {
        [self.cpuMaxChoice setOn:NO];
        [self.cpuSlider setEnabled:YES];
    }
    
    cpu_altscale_factor = [defaults doubleForKey:kApple2AltSpeed];
    [self.altSlider setValue:cpu_altscale_factor];
    self.altSliderLabel.text = [NSString stringWithFormat:@"%.0f%%", cpu_altscale_factor*100];
    if ([defaults boolForKey:kApple2AltSpeedIsMax])
    {
        cpu_altscale_factor = CPU_SCALE_FASTEST;
        [self.altMaxChoice setOn:YES];
        [self.altSlider setEnabled:NO];
    }
    else
    {
        [self.altMaxChoice setOn:NO];
        [self.altSlider setEnabled:YES];
    }

    NSInteger mode = [defaults integerForKey:kApple2ColorConfig];
    if (! ((mode >= COLOR_NONE) && (mode < NUM_COLOROPTS)) )
    {
        mode = COLOR_NONE;
    }
    //[self.videoModePicker d:mode];
    //color_mode = (color_mode_t)mode;

    mode = [defaults integerForKey:kApple2JoystickConfig];
    if (! ((mode >= JOY_PCJOY) && (mode < NUM_JOYOPTS)) )
    {
        mode = JOY_PCJOY;
    }
    joy_mode = (joystick_mode_t)mode;
//    [self.joystickChoice selectItemAtIndex:mode];
/*
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
*/
    
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

// The number of columns of data
- (int)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 1;
}

// The number of rows of data
- (int)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
    return 2;
    //_pickerData.count;
}

// The data to return for the row and component (column) that's being passed in
- (NSString*)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component
{
     return [self._videoMode objectAtIndex:row];
}

// Catpure the picker view selection
- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    // This method is triggered whenever the user makes a change to the picker selection.
    // The parameter named row and component represents what was selected.
    NSLog(@"Selected Row %d", row);
}

- (void)_savePrefs
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:YES forKey:kApple2SavedPrefs];
    [defaults setDouble:cpu_scale_factor forKey:kApple2CPUSpeed];
    [defaults setDouble:cpu_altscale_factor forKey:kApple2AltSpeed];
   // [defaults setBool:([self.cpuMaxChoice state] == NSOnState) forKey:kApple2CPUSpeedIsMax];
   // [defaults setBool:([self.altMaxChoice state] == NSOnState) forKey:kApple2AltSpeedIsMax];
    // [defaults setInteger:color_mode forKey:kApple2ColorConfig];
    [defaults setInteger:joy_mode forKey:kApple2JoystickConfig];
   // [defaults setInteger:joy_step forKey:kApple2JoystickStep];
   // [defaults setBool:joy_auto_recenter forKey:kApple2JoystickAutoRecenter];
    [defaults setBool:joy_clip_to_radius forKey:kApple2JoystickClipToRadius];
}
- (IBAction)sliderDidMove:(id)sender
{
    UISlider *slider = (UISlider *)sender;
    double value = slider.value;
    if (slider == self.cpuSlider)
    {
        cpu_scale_factor = value;
        self.cpuSliderLabel.text=[NSString stringWithFormat:@"%.0f%%", value*100];
    }
    else
    {
        cpu_altscale_factor = value;
        self.altSliderLabel.text=[NSString stringWithFormat:@"%.0f%%", value*100];
    }
    
    timing_initialize();
    
    [self _savePrefs];
}

- (IBAction)peggedChoiceChanged:(id)sender
{
    UISwitch *maxButton = (UISwitch *)sender;
    if (maxButton == self.cpuMaxChoice)
    {
        [self.cpuSlider setEnabled:([maxButton state] != YES)];
        cpu_scale_factor = ([maxButton state] == NO) ? CPU_SCALE_FASTEST : self.cpuSlider.value;
    }
    else
    {
        [self.altSlider setEnabled:([maxButton state] != YES)];
        cpu_altscale_factor = ([maxButton state] == YES) ? CPU_SCALE_FASTEST : self.altSlider.value;
    }
    
    timing_initialize();
    
    [self _savePrefs];
}
/*
 #pragma mark - Navigation
 
 // In a storyboard-based application, you will often want to do a little preparation before navigation
 - (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
 // Get the new view controller using [segue destinationViewController].
 // Pass the selected object to the new view controller.
 }
 */


- (void)dealloc {
  /*  [_videoModePicker release];
    [_cpuSlider release];
    [_altSlider release];
    [_cpuSliderLabel release];
    [_altSlider release];
    [_cpuMaxChoice release];
    [_altMaxChoice release];
   */
    [super dealloc];
}

-(IBAction)goodbye:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:nil];
    cpu_resume();
}
- (IBAction)unwindToMainViewController:(UIStoryboardSegue*)sender
{

}

@end


