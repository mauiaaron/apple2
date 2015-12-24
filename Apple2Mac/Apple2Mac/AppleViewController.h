//
//  AppleViewController.h
//  Apple2Mac
//
//  Created by Jerome Vernet on 24/12/2015.
//  Copyright © 2015 deadc0de.org. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface AppleViewController : UIViewController

@property (assign) IBOutlet UIToolbar *mainToolBar;

-(IBAction)rebootItem:(id)sender;
-(IBAction)prefsItem:(id)sender;

@end
