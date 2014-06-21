//
//  main.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 6/21/14.
//  Copyright 2014 deadc0de.org All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "cocos2d.h"

extern int argc;
extern char **argv;

int main(int _argc, char *_argv[])
{
    argc = _argc;
    argv = _argv;

	[CCGLView load_];
    return NSApplicationMain(argc,  (const char **) argv);
}
