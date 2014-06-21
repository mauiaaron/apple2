//
//  HelloWorldLayer.m
//  Apple2Mac
//
//  Created by Aaron Culliney on 6/16/14.
//  Copyright deadc0de.org 2014. All rights reserved.
//


#include "common.h"

// Import the interfaces
#import "HelloWorldLayer.h"

// HelloWorldLayer implementation
@implementation HelloWorldLayer

+ (CCScene *)scene
{
	// 'scene' is an autorelease object.
	CCScene *scene = [CCScene node];
	
	// 'layer' is an autorelease object.
	HelloWorldLayer *layer = [HelloWorldLayer node];
	
	// add layer as a child to scene
	[scene addChild: layer];
	
	// return the scene
	return scene;
}

// on "init" you need to initialize your instance
- (id)init
{
	// always call "super" init
	// Apple recommends to re-assign "self" with the "super" return value
	if( (self=[super init]) ) {
		
		// create and initialize a Label
		CCLabelTTF *label = [CCLabelTTF labelWithString:@"Hello World" fontName:@"Marker Felt" fontSize:64];

		// ask director for the window size
		CGSize size = [[CCDirector sharedDirector] viewSize];
	
		// position the label on the center of the screen
		label.position =  ccp( size.width /2 , size.height/2 );
		
		// add the label as a child to this Layer
		[self addChild: label];
        
#warning TODO FIXME ...
#if 0
        // initialize emulator
        c_initialize_firsttime();
        
        // spin off CPU thread
        pthread_create(&cpu_thread_id, NULL, (void *) &cpu_thread, (void *)NULL);
#endif
	}
	return self;
}

@end
