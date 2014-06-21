//
//  AppDelegate.h
//  Apple2Mac
//
//  Created by Aaron Culliney on 6/16/14.
//  Copyright deadc0de.org 2014. All rights reserved.
//

#import "cocos2d.h"

@interface Apple2MacAppDelegate : NSObject <NSApplicationDelegate>

@property (nonatomic, weak) IBOutlet NSWindow    *window;
@property (nonatomic, weak) IBOutlet CCGLView    *glView;

- (IBAction)toggleFullScreen:(id)sender;

@end
