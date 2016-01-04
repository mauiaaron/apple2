/*
 * Apple // emulator for *ix 
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#import <Cocoa/Cocoa.h>

typedef void (^DiskCompletionHandler)(NSOpenPanel *openPanel, NSInteger result);

@interface EmulatorDiskController : NSWindowController

+ (NSSet *)emulatorFileTypes;
+ (NSString *)extensionForPath:(NSString *)path;
+ (void)chooseDiskForWindow:(NSWindow *)window completionHandler:(DiskCompletionHandler)handler;

@end
