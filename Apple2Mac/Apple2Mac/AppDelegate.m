//
//  AppDelegate.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 6/21/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

#import "AppDelegate.h"

#import "common.h"

extern void c_initialize_firsttime(void);

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
#if 0
    c_initialize_firsttime();
    
    // spin off cpu thread
    pthread_create(&cpu_thread_id, NULL, (void *) &cpu_thread, (void *)NULL);
#endif
}

@end
