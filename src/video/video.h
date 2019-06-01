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
void video_render(void) CALL_ON_UI_THREAD;

/*
 * Set the render thread ID.  Use with caution.
 */
void _video_setRenderThread(pthread_t id);

/*
 * Check if the current code is running on render thread.
 */
bool video_isRenderThread(void);

#define A2_DIRTY_FLAG 0x1 // Apple //e video is dirty
#define FB_DIRTY_FLAG 0x2 // Internal framebuffer is dirty

/*
 * True if dirty bit(s) are set for flag(s)
 */
bool video_isDirty(unsigned long flags);

/*
 * Called primary from VM routines to flag when graphics mode or video memory data changes
 */
void video_setDirty(unsigned long flags);

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
// Video scanner

static inline drawpage_mode_t video_currentMainMode(uint32_t currswitches) {
    if (currswitches & SS_TEXT) {
        return DRAWPAGE_TEXT;
    } else  {
        if (currswitches & SS_HIRES) {
            return DRAWPAGE_HIRES;
        } else {
            return DRAWPAGE_TEXT; // (LORES)
        }
    }
}

static inline drawpage_mode_t video_currentMixedMode(uint32_t currswitches) {
    if (currswitches & (SS_TEXT|SS_MIXED)) {
        return DRAWPAGE_TEXT;
    } else {
        if (currswitches & SS_HIRES) {
            return DRAWPAGE_HIRES;
        } else {
            return DRAWPAGE_TEXT; // (LORES)
        }
    }
}

static inline unsigned int video_currentPage(uint32_t currswitches) {
    // UTAIIe : 5-25
    if (currswitches & SS_80STORE) {
        return 0;
    }

    return !!(currswitches & SS_PAGE2);
}

/*
 * Update text flashing state
 */
void video_flashText(void);

/*
 * Reset video scanner
 */
void video_scannerReset(void);

/*
 * Update video scanner position and generate video output
 */
void video_scannerUpdate(void) CALL_ON_CPU_THREAD;

/*
 * Get current video scanner address
 */
uint16_t video_scannerAddress(OUTPARM bool *ptrIsVBL);

/*
 * Return current video scanner data
 */
uint8_t floating_bus(void);

/*
 * Video frame completion callbacks
 */

typedef void (*video_frame_callback_fn)(uint8_t textFlashCounter) CALL_ON_CPU_THREAD;

void video_registerFrameCallback(video_frame_callback_fn *cbPtr);

#if VIDEO_TRACING
void video_scannerTraceBegin(const char *trace_file, unsigned long frameCount);
void video_scannerTraceEnd(void);
void video_scannerTraceCheckpoint(void);
bool video_scannerTraceShouldStop(void);
#endif

// ----------------------------------------------------------------------------
// Video Backend API

typedef struct video_backend_s {
    const char *(*name)(void);
    void (*init)(void *context);
    void (*main_loop)(void);
    void (*render)(void);
    void (*shutdown)(void);

#if INTERFACE_CLASSIC
    // interface plotting callbacks
    void (*plotChar)(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const uint8_t c);
    void (*plotLine)(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *message);
#endif

    void (*flashText)(void);
    void (*flushScanline)(scan_data_t *scandata) CALL_ON_CPU_THREAD;
    void (*frameComplete)(void) CALL_ON_CPU_THREAD;

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
 * Register a video backend at the specific prioritization
 */
void video_registerBackend(video_backend_s *backend, long prio);

void video_printBackends(FILE *out);

void video_chooseBackend(const char *name);

video_backend_s *video_getCurrentBackend(void);

#endif /* !A2_VIDEO_H */

