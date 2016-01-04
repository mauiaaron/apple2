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

#import <Foundation/Foundation.h>

@interface A2IXPopupChoreographer : NSObject

+ (A2IXPopupChoreographer *)sharedInstance;

- (void)showMainMenuFromView:(UIView *)view;

- (void)dismissMainMenu;

@end
