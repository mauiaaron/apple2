//
//  disksViewController.h
//  Apple2Mac
//
//  Created by Jerome Vernet on 31/12/2015.
//  Copyright © 2015 deadc0de.org. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface disksViewController : UIViewController<UIPickerViewDataSource, UIPickerViewDelegate>
@property (retain, nonatomic) IBOutlet UIPickerView *disk1Picker;
@property (retain, nonatomic) IBOutlet UIPickerView *disk2Picker;

@property (strong,nonatomic) NSArray *_disks;
@property (retain,nonatomic) NSString *path;
@end
