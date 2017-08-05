/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#ifndef A2_VIDEO_H
#define A2_VIDEO_H

/*
 * Animation
 */
typedef struct video_animation_s {

#if INTERFACE_TOUCH
    // touch HUD functions
    void (*animation_showTouchKeyboard)(void);
    void (*animation_hideTouchKeyboard)(void);
    void (*animation_showTouchJoystick)(void);
    void (*animation_hideTouchJoystick)(void);
    void (*animation_showTouchMenu)(void);
    void (*animation_hideTouchMenu)(void);
#endif

    // misc animations
    void (*animation_showMessage)(char *message, unsigned int cols, unsigned int rows);
    void (*animation_showPaused)(void);
    void (*animation_showCPUSpeed)(void);
    void (*animation_showDiskChosen)(int drive);
    void (*animation_showTrackSector)(int drive, int track, int sect);

} video_animation_s;

/*
 * Prepare the video system, converting console to graphics mode, or opening X window, or whatever.
 */
void video_init(void);

/*
 * Enters emulator-managed main video loop (if backend rendering system requires it).  Currently only used by desktop
 * X11 and desktop OpenGL/GLUT.
 */
void video_main_loop(void);

/*
 * Shutdown video system.  Should only be called on the render thread (unless render thread is in emulator-managed main
 * video loop).
 */
void video_shutdown(void);

/*
 * Begin a render pass (only for non-emulator-managed main video).  This should only be called on the render thread.
 */
void video_render(void);

/*
 * Set the render thread ID.  Use with caution.
 */
void _video_setRenderThread(pthread_t id);

/*
 * Check if the current code is running on render thread.
 */
bool video_isRenderThread(void);

/*
 * Clear the current display.
 */
void video_clear(void);

#define A2_DIRTY_FLAG 0x1 // Apple //e video is dirty
#define FB_DIRTY_FLAG 0x2 // Internal framebuffer is dirty

/*
 * True if dirty bit(s) are set for flag(s)
 */
bool video_isDirty(unsigned long flags);

/*
 * Atomically set dirty bit(s), return previous bit(s) value
 */
unsigned long video_setDirty(unsigned long flags);

/*
 * Atomically clear dirty bit(s), return previous bit(s) value
 */
unsigned long video_clearDirty(unsigned long flags);

/*
 * State save support for video subsystem.
 */
bool video_saveState(StateHelper_s *helper);

/*
 * State restore support for video subsystem.
 */
bool video_loadState(StateHelper_s *helper);

/*
 * Get current animation driver
 */
video_animation_s *video_getAnimationDriver(void);


// ----------------------------------------------------------------------------
// Video Backend API

typedef struct video_backend_s {
    void (*init)(void *context);
    void (*main_loop)(void);
    void (*render)(void);
    void (*shutdown)(void);
    video_animation_s *anim;
} video_backend_s;

#if VIDEO_X11
// X11 scaling ...
typedef enum a2_video_mode_t {
    VIDEO_FULLSCREEN = 0,
    VIDEO_1X,
    VIDEO_2X,
    NUM_VIDOPTS
} a2_video_mode_t;

extern a2_video_mode_t a2_video_mode;
#endif

enum {
    VID_PRIO_GRAPHICS_GL = 10,
    VID_PRIO_GRAPHICS_X  = 20,
    VID_PRIO_TERMINAL    = 30,
    VID_PRIO_NULL        = 100,
};

/*
 * Register a video backend at the specific prioritization, regardless of user choice.
 */
void video_registerBackend(video_backend_s *backend, long prio);

#endif /* !A2_VIDEO_H */

