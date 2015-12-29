//
//  AppleViewController.m
//  Apple2Mac
//
//  Created by Jerome Vernet on 24/12/2015.
//  Copyright © 2015 deadc0de.org. All rights reserved.
//

#import "AppleViewController.h"
#import "common.h"
#import "modelUtil.h"

@interface AppleViewController ()

@end

@implementation AppleViewController

@synthesize paused = _paused;

- (void)viewDidLoad {
    [super viewDidLoad];
  //  [self mainToolBar ];
    // Do any additional setup after loading the view.
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}


- (IBAction)unwindToMainViewController:(UIStoryboardSegue*)sender
{ }

-(IBAction)rebootItemSelected:(id)sender{
    cpu65_reboot();
}

-(IBAction)prefsItemSelected:(id)sender{
    cpu_pause();
    //[self.viewPrefs ];
    //pause
    //show pref windows
    cpu_resume();
}

- (IBAction)toggleCPUSpeedItemSelected:(id)sender
{
    cpu_pause();
    timing_toggleCPUSpeed();
    if (video_backend && video_backend->animation_showCPUSpeed)
    {
        video_backend->animation_showCPUSpeed();
    }
    cpu_resume();
}

- (IBAction)togglePauseItemSelected:(id)sender
{
    NSAssert(pthread_main_np(), @"Pause emulation called from non-main thread");
    self.paused = !_paused;
}
- (void)setPaused:(BOOL)paused
{
    if (_paused == paused)
    {
        return;
    }
    
    _paused = paused;
    if (paused)
    {
                cpu_pause();
    }
    else
    {
        cpu_resume();
    }
    if (video_backend && video_backend->animation_showPaused)
    {
        video_backend->animation_showPaused();
    }
}

@end
