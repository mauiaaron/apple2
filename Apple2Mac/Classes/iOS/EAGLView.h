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

#import <UIKit/UIKit.h>

// This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
// The view content is basically an EAGL surface you render your OpenGL scene into.
// Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
@interface EAGLView : UIView

@property (nonatomic, readonly, getter=isAnimating, assign) BOOL animating;
@property (nonatomic, assign) NSInteger renderFrameInterval;

- (void)pauseRendering;
- (void)resumeRendering;

- (void)pauseEmulation;
- (void)resumeEmulation;

@end
