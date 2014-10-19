//
//  EmulatorDiskController.h
//  Apple2Mac
//
//  Created by Aaron Culliney on 10/18/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

#import <Cocoa/Cocoa.h>

typedef void (^DiskCompletionHandler)(NSOpenPanel *openPanel, NSInteger result);

@interface EmulatorDiskController : NSWindowController

+ (NSSet *)emulatorFileTypes;
+ (NSString *)extensionForPath:(NSString *)path;
+ (void)chooseDiskForWindow:(NSWindow *)window completionHandler:(DiskCompletionHandler)handler;

@end
