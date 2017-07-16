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

#import <AVFoundation/AVFoundation.h>
#import "AppDelegate.h"
#import "EAGLView.h"
#import "common.h"

@implementation AppDelegate

#pragma mark - Application lifecycle

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options
{    
    [self recursivelyCopyBundleResources];
    
    [self activateAudioSession];
    
    [self.window makeKeyAndVisible];
    
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    [view resumeRendering];
    [view resumeEmulation];

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    [self deactivateAudioSession];
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    [view pauseRendering];
    [view pauseEmulation];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // ...
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    [self activateAudioSession];
    EAGLView *view = (EAGLView *)self.window.rootViewController.view;
    [view resumeRendering];
    [view resumeEmulation];
}

- (void)applicationDidBecomeActive:(UIApplication *)application 
{
    // ...
}

- (void)applicationWillTerminate:(UIApplication *)application
{
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

#pragma mark - resource management

- (void)activateAudioSession
{
    NSError *error = nil;
    [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&error];
    if (error)
    {
        LOG("Error setting AVAudioSessionCategoryAmbient : %s", [[error description] UTF8String]);
        error = nil;
    }
    [[AVAudioSession sharedInstance] setActive:YES error:&error];
    if (error)
    {
        LOG("Error activating AVAudioSession : %s", [[error description] UTF8String]);
        error = nil;
    }
}

- (void)deactivateAudioSession
{
    NSError *error = nil;
    [[AVAudioSession sharedInstance] setActive:NO error:&error];
    if (error)
    {
        LOG("Error deactivating AVAudioSession : %s", [[error description] UTF8String]);
    }
}

#pragma mark - Export bundle resources to a location where we have R/W access

- (void)recursivelyCopyBundleResources
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, /*expandTilde:*/YES);
    NSString *documentsDir = [paths objectAtIndex:0];
    
    // copy disks directory into apple2ix directory
    NSString *apple2ix = @"apple2ix";
    NSString *disks[] = { @"disks", @"external-disks", NULL};
    
    NSString **str = &disks[0];
    while (*str) {
        NSString *apple2ixDirString = [documentsDir stringByAppendingPathComponent:apple2ix];
        data_dir = strdup([apple2ixDirString UTF8String]);
        
        NSString *documentsPath = [apple2ixDirString stringByAppendingPathComponent:*str];
        NSString *resourcesPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:*str];
        [self copyDirectoryFrom:resourcesPath to:documentsPath];
        ++str;
    }
}

- (void)copyDirectoryFrom:(NSString *)resourcesPath to:(NSString *)documentsPath
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    NSError *error = nil;
    if (![fileManager fileExistsAtPath:documentsPath])
    {
        [fileManager createDirectoryAtPath:documentsPath withIntermediateDirectories:YES attributes:nil error:&error];
    }

    if (error)
    {
        LOG("Could not create directory. Error: %s", [[error description] UTF8String]);
        return;
    }
    
    NSArray *fileList = [fileManager contentsOfDirectoryAtPath:resourcesPath error:&error];
    if (error)
    {
        LOG("Could not list contents of bundle. Error: %s", [[error description] UTF8String]);
        return;
    }
    
    for (NSString *s in fileList)
    {
        NSString *documentsFile = [documentsPath stringByAppendingPathComponent:s];
        NSString *resourcesFile = [resourcesPath stringByAppendingPathComponent:s];
        
        BOOL isDirectory = false;
        if (![fileManager fileExistsAtPath:resourcesFile isDirectory:&isDirectory])
        {
            assert(NO);
            continue;
        }
        
        if (isDirectory)
        {
            [self copyDirectoryFrom:resourcesFile to:documentsFile];
        }
        else
        {
            [fileManager copyItemAtPath:resourcesFile toPath:documentsFile error:&error];
            if (error)
            {
                LOG("Could not copy file. Error: %s", [[error description] UTF8String]);
            }
        }
    }
}

@end

