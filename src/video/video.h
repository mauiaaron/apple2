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
    void (*reshape)(int width, int height);
    void (*render)(void);
    void (*shutdown)(bool emulatorShuttingDown);
} video_backend_s;

/*
 * The registered video backend (renderer).
 */
extern video_backend_s *video_backend;

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
extern A2Color_s colormap[256];

#endif /* !A2_VIDEO_H */

