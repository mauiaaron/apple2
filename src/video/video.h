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

typedef struct video_backend_s {
    void (*init)(void *context);
    void (*main_loop)(void);
    void (*render)(void);
    void (*shutdown)(void);
    video_animation_s *anim;
} video_backend_s;

/*
 * Color structure
 */
typedef struct A2Color_s {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} A2Color_s;

/*
 * Reference to the internal 8bit-indexed color format
 */
extern A2Color_s colormap[];

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

void video_registerBackend(video_backend_s *backend, long prio);

video_backend_s *video_getCurrentBackend(void);

#endif /* !A2_VIDEO_H */

