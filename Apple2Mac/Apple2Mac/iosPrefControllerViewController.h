//
//  iosPrefControllerViewController.h
//  Apple2Mac
//
//  Created by Jerome Vernet on 24/12/2015.
//  Copyright © 2015 deadc0de.org. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface iosPrefControllerViewController : UIViewController<UIPickerViewDataSource, UIPickerViewDelegate>
{
    NSArray *_pickerData;
}
@property (weak, nonatomic) IBOutlet UIPickerView *videoModePicker;
@property (retain, nonatomic) IBOutlet UISlider *CPUSpeed;
@property (retain, nonatomic) IBOutlet UISwitch *CPUMax;


@end




