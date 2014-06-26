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


void video_driver_init() {
    // TBD ... initialize main game GL view
}

void video_driver_shutdown() {
    // TBD ... destroy main game GL view
}

void video_sync(int ignored) {
    // TBD ...
}


@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self testCPU];
}

extern int test_cpu(int, char **);

- (void)testCPU
{
    char *argv[] = {
        "-f",
        NULL
    };
    int argc = 0;
    for (char **p = &argv[0]; *p != NULL; p++) {
        ++argc;
    }
    int val = test_cpu(argc, argv);
}

@end
