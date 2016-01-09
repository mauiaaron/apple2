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
 
#import "AppDelegate.h"
#import "EAGLView.h"
#import "common.h"

@implementation AppDelegate

#pragma mark - Application lifecycle

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options
{    
    // Override point for customization after application launch.
    
    [self.window makeKeyAndVisible];
    
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    [view resumeRendering];
    [view resumeEmulation];

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of
    // temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application
    // and it begins the transition to the background state.  Use this method to pause ongoing tasks, disable timers,
    // and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    [view pauseRendering];
    [view pauseEmulation];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application
    // state information to restore your application to its current state in case it is terminated later.  If your
    // application supports background execution, called instead of applicationWillTerminate: when the user quits.
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    [view pauseRendering];
    [view pauseEmulation];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    // Called as part of transition from the background to the inactive state: here you can undo many of the changes
    // made on entering the background.
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    [view resumeRendering];
    [view resumeEmulation];
}

- (void)applicationDidBecomeActive:(UIApplication *)application 
{
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application
    // was previously in the background, optionally refresh the user interface.
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    [view resumeRendering];
    [view resumeEmulation];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Called when the application is about to terminate.  See also applicationDidEnterBackground:
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    self.window.rootViewController.view = [[UIView alloc] init];
    
    [view pauseRendering];
    [view pauseEmulation];
    
    [view release];
}

#pragma mark - Memory management

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
    // prolly not much we can do here at the moment since we run a tight ship...
    LOG("...");
}

@end

