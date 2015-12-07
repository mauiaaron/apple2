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

#import "EAGLView.h"
#import "common.h"
#import "modelUtil.h"

#import "A2IXPopupChoreographer.h"

#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>

@interface EAGLView ()

@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic, retain) CADisplayLink *displayLink;
@property (nonatomic, assign) GLuint defaultFBOName;
@property (nonatomic, assign) GLuint colorRenderbuffer;

@end

@implementation EAGLView

@synthesize renderFrameInterval = _renderFrameInterval;

// Must return the CAEAGLLayer class so that CA allocates an EAGLLayer backing for this view
+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

// -initWithCoder: is sent when unarchived from storyboard file
- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self)
    {
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        
        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];
        
        
        _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
        if (!_context || ![EAGLContext setCurrentContext:_context])
        {
            [self release];
            return nil;
        }
        
        // Create default framebuffer object. The backing will be allocated for the
        // current layer in -resizeFromLayer
        glGenFramebuffers(1, &_defaultFBOName);
        
        glGenRenderbuffers(1, &_colorRenderbuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBOName);
        glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
        
        // This call associates the storage for the current render buffer with the
        // EAGLDrawable (our CAEAGLLayer) allowing us to draw into a buffer that
        // will later be rendered to the screen wherever the layer is (which
        // corresponds with our view).
        [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(id<EAGLDrawable>)self.layer];
        
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorRenderbuffer);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
            [self release];
            return nil;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBOName);
        
        // start emulator from paused state
        cpu_pause();
        emulator_start();
        video_backend->init(0);
        
        _animating = NO;
        _renderFrameInterval = 1;
        _displayLink = nil;
    }
    
    return self;
}

- (void)dealloc
{
    [self pauseRendering];
    [_displayLink release];
    _displayLink = nil;
    
    // shut down common OpenGL stuff AFTER display link has been released
    emulator_shutdown();
    
    if (_defaultFBOName != UNINITIALIZED_GL)
    {
        glDeleteFramebuffers(1, &_defaultFBOName);
        _defaultFBOName = UNINITIALIZED_GL;
    }
    if (_colorRenderbuffer != UNINITIALIZED_GL)
    {
        glDeleteRenderbuffers(1, &_colorRenderbuffer);
        _colorRenderbuffer = UNINITIALIZED_GL;
    }

    if ([EAGLContext currentContext] == _context)
    {
        [EAGLContext setCurrentContext:nil];
    }
    
    [_context release];
    _context = nil;
    
    [super dealloc];
}

- (void)pauseEmulation
{
    cpu_pause();
}

- (void)resumeEmulation
{
    cpu_resume();
}

- (void)drawView:(id)sender
{   
    [EAGLContext setCurrentContext:_context];
    
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBOName);
    video_backend->render();
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    
    [_context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)layoutSubviews
{
    // Allocate color buffer backing based on the current layer size
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(id<EAGLDrawable>)self.layer];
    
    // The pixel dimensions of the CAEAGLLayer
    GLint backingWidth = 0;
    GLint backingHeight = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);
    
    video_backend->reshape((int)backingWidth, (int)backingHeight);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return;
    }
    
    [self drawView:nil];
}

- (void)setRenderFrameInterval:(NSInteger)frameInterval
{
    // Frame interval defines how many display frames must pass between each time the
    // display link fires. The display link will only fire 30 times a second when the
    // frame internal is two on a display that refreshes 60 times a second. The default
    // frame interval setting of one will fire 60 times a second when the display refreshes
    // at 60 times a second. A frame interval setting of less than one results in undefined
    // behavior.
    if (frameInterval >= 1)
    {
        _renderFrameInterval = frameInterval;
        if (_animating)
        {
            [self pauseRendering];
            [self resumeRendering];
        }
    }
}

- (void)resumeRendering
{
    if (!_animating)
    {
        _animating = YES;
        
        // Create the display link and set the callback to our drawView method
        _displayLink = [[CADisplayLink displayLinkWithTarget:self selector:@selector(drawView:)] retain];

        // Set it to our _renderFrameInterval
        [_displayLink setFrameInterval:_renderFrameInterval];

        // Have the display link run on the default runn loop (and the main thread)
        [_displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    }
}

- (void)pauseRendering
{
    if (_animating)
    {
        [_displayLink invalidate];
        [_displayLink release];
        _displayLink = nil;
        _animating = NO;
    }
}

#pragma mark - Touch Handling

- (BOOL)isMultipleTouchEnabled
{
    return YES;
}

static inline void _handleTouch(EAGLView *self, SEL _cmd, UITouch *touch, interface_touch_event_t eventType) {
    
    CGPoint location = [touch locationInView:self];
    float x[1] = { location.x };
    float y[1] = { location.y };
    
    int64_t flags = interface_onTouchEvent(eventType, (int)1, (int)0, x, y);

    do {
        if ((flags & TOUCH_FLAGS_HANDLED) == 0)
        {
            break;
        }
        
        if ((flags & TOUCH_FLAGS_REQUEST_HOST_MENU) != 0)
        {
            // requested host menu
            [[A2IXPopupChoreographer sharedInstance] showMainMenuFromView:self];
        }
        
        if ((flags & TOUCH_FLAGS_KEY_TAP) != 0)
        {
            // tapped key
        }
        
        if ((flags & TOUCH_FLAGS_MENU) == 0)
        {
            // touch menu was tapped
            break;
        }
        
        // touched menu item ...
        if ((flags & TOUCH_FLAGS_INPUT_DEVICE_CHANGE) != 0)
        {
            if ((flags & TOUCH_FLAGS_KBD) != 0)
            {
                keydriver_setTouchKeyboardOwnsScreen(true);
                joydriver_setTouchJoystickOwnsScreen(false);
                video_backend->animation_showTouchKeyboard();
            }
            else if ((flags & TOUCH_FLAGS_JOY) != 0)
            {
                keydriver_setTouchKeyboardOwnsScreen(false);
                joydriver_setTouchJoystickOwnsScreen(true);
                joydriver_setTouchVariant(EMULATED_JOYSTICK);
                video_backend->animation_showTouchJoystick();
            }
            else if ((flags & TOUCH_FLAGS_JOY_KPAD) != 0)
            {
                keydriver_setTouchKeyboardOwnsScreen(false);
                joydriver_setTouchJoystickOwnsScreen(true);
                joydriver_setTouchVariant(EMULATED_KEYPAD);
                video_backend->animation_showTouchJoystick();
            }
            else
            {
                // switch to next variant ...
            }
        }
        else if ((flags & TOUCH_FLAGS_CPU_SPEED_DEC) != 0)
        {
            // handle cpu decrement
        }
        else if ((flags & TOUCH_FLAGS_CPU_SPEED_INC) != 0)
        {
            // handle cpu increment
        }
    } while (NO);
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    LOG("...");
    for (UITouch *touch in touches)
    {
        _handleTouch(self, _cmd, touch, TOUCH_DOWN);
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    LOG("...");
    for (UITouch *touch in touches)
    {
        _handleTouch(self, _cmd, touch, TOUCH_MOVE);
    }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    LOG("...");
    for (UITouch *touch in touches)
    {
        _handleTouch(self, _cmd, touch, TOUCH_UP);
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    LOG("...");
    for (UITouch *touch in touches)
    {
        _handleTouch(self, _cmd, touch, TOUCH_CANCEL);
    }
}

@end
