/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2014, 2015 Aaron Culliney
 *
 */

// Based on sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#import "EmulatorGLView.h"

// Apple //e common routines
#import "common.h"

#if TARGET_OS_MAC
#define USE_DISPLAYLINK 0
#define BROKEN_DISPLAYLINK 1
#else
// iOS uses CADisplayLink
#define USE_DISPLAYLINK 1
#endif

#define SUPPORT_RETINA_RESOLUTION 1

const NSString *kDrawTimerNotification = @"kDrawTimerNotification";

@interface EmulatorGLView ()

#if USE_DISPLAYLINK
@property (nonatomic, assign) CVDisplayLinkRef displayLink;
#else
@property (nonatomic, retain) NSTimer *timer;
#endif

- (void)initGL;

@end


@implementation EmulatorGLView

#if USE_DISPLAYLINK
@synthesize displayLink = _displayLink;
#else
@synthesize timer = _timer;
#endif

#pragma mark CVDisplayLink / NSTimer stuff

#if USE_DISPLAYLINK
- (CVReturn)getFrameForTime:(const CVTimeStamp *)outputTime
{
    // There is no autorelease pool when this method is called
    // because it will be called from a background thread.
    // It's important to create one or app can leak objects.
    @autoreleasepool {
        // We draw on a secondary thread through the display link
        // When resizing the view, -reshape is called automatically on the main
        // thread. Add a mutex around to avoid the threads accessing the context
        // simultaneously when resizing
        
#if BROKEN_DISPLAYLINK
#warning ASC NOTE 2014/09/27 multi-threaded OpenGL on Mac considered harmful to developer sanity
        // 2014/09/27 Kinda defeats the purpose of using CVDisplayLink ... but keeps it from crashing to dispatch to main queue
        dispatch_async(dispatch_get_main_queue(), ^{
#endif
        [[self openGLContext] makeCurrentContext];
        [self drawView];
#if BROKEN_DISPLAYLINK
        });
#endif
   }
    return kCVReturnSuccess;
}

// This is the renderer output callback function
static CVReturn displayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *now, const CVTimeStamp *outputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext)
{
    CVReturn result = [(EmulatorGLView *)displayLinkContext getFrameForTime:outputTime];
    return result;
}

#else // use NSTimer
- (void)targetMethod:(NSTimer *)theTimer
{
    NSAssert([NSThread isMainThread], @"Timer fired on non-main thread!");
    [[self openGLContext] makeCurrentContext];
    [self drawView];
}

#endif

#pragma mark -

- (void)awakeFromNib
{
    NSOpenGLPixelFormatAttribute attrs[] =
    {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
        // Must specify the 3.2 Core Profile to use OpenGL 3.2
        NSOpenGLPFAOpenGLProfile,
        NSOpenGLProfileVersion3_2Core,
        0
    };
    
    NSOpenGLPixelFormat *pf = [[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs] autorelease];
    
    if (!pf)
    {
        NSLog(@"No OpenGL pixel format");
    }
       
    NSOpenGLContext* context = [[[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil] autorelease];
    
#if defined(DEBUG)
    // When we're using a CoreProfile context, crash if we call a legacy OpenGL function
    // This will make it much more obvious where and when such a function call is made so
    // that we can remove such calls.
    // Without this we'd simply get GL_INVALID_OPERATION error for calling legacy functions
    // but it would be more difficult to see where that function was called.
    CGLEnable([context CGLContextObj], kCGLCECrashOnRemovedFunctions);
#endif
    
    [self setPixelFormat:pf];
    [self setOpenGLContext:context];
    
#if SUPPORT_RETINA_RESOLUTION
    // Opt-In to Retina resolution
    [self setWantsBestResolutionOpenGLSurface:YES];
#endif // SUPPORT_RETINA_RESOLUTION
}

- (void)prepareOpenGL
{
    [super prepareOpenGL];
    
    // Make all the OpenGL calls to setup rendering  
    // and build the necessary rendering objects
    [self initGL];
    
#if USE_DISPLAYLINK
    // Create a display link capable of being used with all active displays
    CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
    
    // Set the renderer output callback function
    CVDisplayLinkSetOutputCallback(_displayLink, &displayLinkCallback, self);
    
    // Set the display link for the current renderer
    CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);
    
    // Activate the display link
    CVDisplayLinkStart(_displayLink);
#else
    [self.timer invalidate];
    self.timer = nil;
    self.timer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0 target:self selector:@selector(targetMethod:) userInfo:nil repeats:YES];
#endif
    
    // Register to be notified when the window closes so we can stop the displaylink
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowWillClose:) name:NSWindowWillCloseNotification object:[self window]];
}

- (void)windowWillClose:(NSNotification*)notification
{
    // Stop the display link when the window is closing because default
    // OpenGL render buffers will be destroyed.  If display link continues to
    // fire without renderbuffers, OpenGL draw calls will set errors.
#if USE_DISPLAYLINK
    CVDisplayLinkStop(_displayLink);
#else
    [self.timer invalidate];
    self.timer = nil;
#endif
}

- (void)initGL
{
    // The reshape function may have changed the thread to which our OpenGL
    // context is attached before prepareOpenGL and initGL are called.  So call
    // makeCurrentContext to ensure that our OpenGL context current to this 
    // thread (i.e. makeCurrentContext directs all OpenGL calls on this thread
    // to [self openGLContext])
    [[self openGLContext] makeCurrentContext];
    
    emulator_start();
    
    // Synchronize buffer swaps with vertical refresh rate
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
    
    // Init our renderer.  Use 0 for the defaultFBO which is appropriate for
    // OSX (but not iOS since iOS apps must create their own FBO)
#if TARGET_OS_MAC
    video_backend->init(0);
#elif TARGET_OS_IPHONE
#   error "FBO FIXME TODO"
    video_backend->init(otherFBO);
#else
#   error "unknown/unsupported Apple platform
#endif
}

- (void)reshape
{    
    [super reshape];
    
    // We draw on a secondary thread through the display link. However, when
    // resizing the view, -drawRect is called on the main thread.
    // Add a mutex around to avoid the threads accessing the context
    // simultaneously when resizing.
    CGLLockContext([[self openGLContext] CGLContextObj]);

    // Get the view size in Points
    NSRect viewRectPoints = [self bounds];
    
#if SUPPORT_RETINA_RESOLUTION

    // Rendering at retina resolutions will reduce aliasing, but at the potential
    // cost of framerate and battery life due to the GPU needing to render more
    // pixels.

    // Any calculations the renderer does which use pixel dimentions, must be
    // in "retina" space.  [NSView convertRectToBacking] converts point sizes
    // to pixel sizes.  Thus the renderer gets the size in pixels, not points,
    // so that it can set it's viewport and perform and other pixel based
    // calculations appropriately.
    // viewRectPixels will be larger (2x) than viewRectPoints for retina displays.
    // viewRectPixels will be the same as viewRectPoints for non-retina displays
    NSRect viewRectPixels = [self convertRectToBacking:viewRectPoints];
    
#else //if !SUPPORT_RETINA_RESOLUTION
    
    // App will typically render faster and use less power rendering at
    // non-retina resolutions since the GPU needs to render less pixels.  There
    // is the cost of more aliasing, but it will be no-worse than on a Mac
    // without a retina display.
    
    // Points:Pixels is always 1:1 when not supporting retina resolutions
    NSRect viewRectPixels = viewRectPoints;
    
#endif // !SUPPORT_RETINA_RESOLUTION
    
    // Set the new dimensions in our renderer
    video_backend->reshape((int)viewRectPixels.size.width, (int)viewRectPixels.size.height);
    
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void)renewGState
{    
    // Called whenever graphics state updated (such as window resize)
    
    // OpenGL rendering is not synchronous with other rendering on the OSX.
    // Therefore, call disableScreenUpdatesUntilFlush so the window server
    // doesn't render non-OpenGL content in the window asynchronously from
    // OpenGL content, which could cause flickering.  (non-OpenGL content
    // includes the title bar and drawing done by the app with other APIs)
    [[self window] disableScreenUpdatesUntilFlush];
    [super renewGState];
}

- (void)drawRect:(NSRect)theRect
{
    NSAssert([NSThread isMainThread], @"drawRect called on non-main thread!");
    // Called during resize operations
    // Avoid flickering during resize by drawing
    [[self openGLContext] makeCurrentContext];
    [self drawView];
}

- (void)drawView
{
    CGLLockContext([[self openGLContext] CGLContextObj]);
    video_backend->render();
    CGLFlushDrawable([[self openGLContext] CGLContextObj]);
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
    [[NSNotificationCenter defaultCenter] postNotificationName:(NSString *)kDrawTimerNotification object:nil];
}

- (void)dealloc
{
    // Stop the display link BEFORE releasing anything in the view
    // otherwise the display link thread may call into the view and crash
    // when it encounters something that has been release
#if USE_DISPLAYLINK
    CVDisplayLinkStop(_displayLink);
    CVDisplayLinkRelease(_displayLink);
#else
    [self.timer invalidate];
    self.timer = nil;
#endif

    // shut down common OpenGL stuff AFTER display link has been released
    emulator_shutdown();
    
    [super dealloc];
}

#pragma mark -
#pragma mark Application Delegate methods

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)application
{
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)application
{
    disk6_eject(0);
    disk6_eject(1);
    return NSTerminateNow;
}

@end
