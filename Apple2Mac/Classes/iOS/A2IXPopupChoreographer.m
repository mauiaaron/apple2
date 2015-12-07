/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2015 Aaron Culliney
 *
 */

#import "A2IXPopupChoreographer.h"

@interface A2IXPopupChoreographer () <UIPopoverControllerDelegate, UITableViewDelegate>

@property (nonatomic, retain) UIPopoverController *popover;
@property (nonatomic, retain) UIViewController *mainMenuVC;

@end

@implementation A2IXPopupChoreographer

+ (A2IXPopupChoreographer *)sharedInstance
{
    static A2IXPopupChoreographer *mainMenuChoreographer = nil;
    static dispatch_once_t onceToken = 0L;
    dispatch_once(&onceToken, ^{
        mainMenuChoreographer = [[A2IXPopupChoreographer alloc] init];
    });
    return mainMenuChoreographer;
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        UIStoryboard *storyboard = [UIStoryboard storyboardWithName:@"Main" bundle:[NSBundle mainBundle]];
        self.mainMenuVC = (UIViewController *)[storyboard instantiateViewControllerWithIdentifier:@"MainMenu"];
    }
    return self;
}

- (void)dealloc
{
    self.popover = nil;
    self.mainMenuVC = nil;
    [super dealloc];
}

- (void)showMainMenuFromView:(UIView *)view
{
    @synchronized(self) {
        if (!self.popover)
        {
            // TODO : pause emulation
            UIPopoverController *pop = [[UIPopoverController alloc] initWithContentViewController:self.mainMenuVC];
            [pop setDelegate:self];
            self.popover = pop;
            [pop release];
            CGRect aRect = CGRectMake(0, 0, 640, 480);
            [pop presentPopoverFromRect:aRect inView:view permittedArrowDirections:UIPopoverArrowDirectionUnknown animated:YES];
            
            UITableViewController *tableVC = self.mainMenuVC.childViewControllers.firstObject;
            tableVC.tableView.delegate = self;
        }
    }
}

- (void)dismissMainMenu
{
    @synchronized(self) {
        [self.popover dismissPopoverAnimated:YES];
        [self popoverControllerDidDismissPopover:nil];
    }
}


#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    switch (indexPath.row) {
        case 0:
            NSLog(@"Show emulator settings ...");
            break;
        case 1:
            NSLog(@"Show load disk image ...");
            break;
        case 2:
            NSLog(@"Show save/restore ...");
            break;
        case 3:
            NSLog(@"Show reboot/quit ...");
            break;
        default:
            NSAssert(false, @"This should not happen ...");
            break;
    }
}


#pragma mark - UIPopoverControllerDelegate

/* Called on the delegate when the popover controller will dismiss the popover. Return NO to prevent the dismissal of the view.
 */
- (BOOL)popoverControllerShouldDismissPopover:(UIPopoverController *)popoverController
{
    return YES;
}

/* Called on the delegate when the user has taken action to dismiss the popover. This is not called when -dismissPopoverAnimated: is called directly.
 */
- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
{
    @synchronized(self) {
        self.popover = nil;
        // TODO : restart emulation
    }
}

/* -popoverController:willRepositionPopoverToRect:inView: is called on your delegate when the popover may require a different view or rectangle
 */
- (void)popoverController:(UIPopoverController *)popoverController willRepositionPopoverToRect:(inout CGRect *)rect inView:(inout UIView **)view
{
    
}

@end
