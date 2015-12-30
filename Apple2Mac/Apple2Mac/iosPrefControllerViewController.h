//
//  iosPrefControllerViewController.h
//  Apple2Mac
//
//  Created by Jerome Vernet on 24/12/2015.
//  Copyright © 2015 deadc0de.org. All rights reserved.
//

#import <UIKit/UIKit.h>

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
#define kApple2PrefStartupDiskA @"kApple2PrefStartupDiskA"
#define kApple2PrefStartupDiskAProtected @"kApple2PrefStartupDiskAProtected"
#define kApple2PrefStartupDiskB @"kApple2PrefStartupDiskB"
#define kApple2PrefStartupDiskBProtected @"kApple2PrefStartupDiskBProtected"
#define kApple2JoystickStep @"kApple2JoystickStep"


@interface iosPrefControllerViewController : UIViewController<UIPickerViewDataSource, UIPickerViewDelegate>

@property (retain, nonatomic) IBOutlet UIPickerView *videoModePicker;
@property (retain, nonatomic) IBOutlet UISlider *cpuSlider;
@property (retain, nonatomic) IBOutlet UISlider *altSlider;
@property (retain, nonatomic) IBOutlet UILabel *cpuSliderLabel;
@property (retain, nonatomic) IBOutlet UILabel *altSliderLabel;
@property (retain, nonatomic) IBOutlet UISwitch *cpuMaxChoice;
@property (retain, nonatomic) IBOutlet UISwitch *altMaxChoice;

@property (strong,nonatomic) NSArray *_videoMode;


@end




