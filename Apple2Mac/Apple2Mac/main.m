//
//  main.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 6/21/14.
//  Copyright (c) 2014 deadc0de.org. All rights reserved.
//

#import <Cocoa/Cocoa.h>

extern int argc;
extern const char **argv;

int main(int argc_, const char *argv_[])
{
    argc = argc_;
    argv = argv_;
    return NSApplicationMain(argc, argv);
}
