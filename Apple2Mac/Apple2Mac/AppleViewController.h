//
//  AppleViewController.h
//  Apple2Mac
//
//  Created by Jerome Vernet on 24/12/2015.
//  Copyright © 2015 deadc0de.org. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "iosPrefControllerViewController.h"

@interface AppleViewController : UIViewController
@property (nonatomic, assign) BOOL paused;
@property (assign) IBOutlet UIToolbar *mainToolBar;
@property (assign) IBOutlet iosPrefControllerViewController *viewPrefs;

-(IBAction)rebootItemSelected:(id)sender;
-(IBAction)prefsItemSelected:(id)sender;
-(IBAction)toggleCPUSpeedItemSelected:(id)sender;
-(IBAction)togglePauseItemSelected:(id)sender;
-(IBAction)diskInsert:(id)sender;

@end
