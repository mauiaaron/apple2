//
//  CPUTestAppDelegate.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 6/21/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

#import "CPUTestAppDelegate.h"

#import "common.h"

extern int test_cpu(int, char **);
extern int test_vm(int argc, char **argv);
extern int test_display(int argc, char **argv);

@implementation CPUTestAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    dispatch_async(dispatch_get_main_queue(), ^{
        char *local_argv[] = {
            "-f",
            NULL
        };
        int local_argc = 0;
        for (char **p = &local_argv[0]; *p != NULL; p++) {
            ++local_argc;
        }
        
#if defined(TEST_CPU)
        test_cpu(local_argc, local_argv);
#elif defined(TEST_VM)
        test_vm(local_argc, local_argv);
#elif defined(TEST_DISPLAY)
        test_display(local_argc, local_argv);
#elif defined(TEST_DISK)
        test_disk(local_argc, local_argv);
#else
#error "OOPS, no tests specified"
#endif
    });
}

@end
