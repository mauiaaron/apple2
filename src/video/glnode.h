/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#ifndef _GLNODE_H_
#define _GLNODE_H_

#include "common.h"
#include "video/video.h"
#include "video_util/modelUtil.h"
#include "video_util/matrixUtil.h"
#include "video_util/sourceUtil.h"

// TODO: implement 3D CRT object, possibly with perspective drawing?
#define PERSPECTIVE 0

#define PREF_PORTRAIT_HEIGHT_SCALE "portraitHeightScale"
#define PREF_PORTRAIT_POSITION_SCALE "portraitPositionScale"

enum {
    TEXTURE_ID_FRAMEBUFFER=0,
    TEXTURE_ID_MESSAGE,
#if INTERFACE_TOUCH
    TEXTURE_ID_TOUCHJOY_AXIS,
    TEXTURE_ID_TOUCHJOY_BUTTON,
    TEXTURE_ID_TOUCHKBD,
    TEXTURE_ID_TOUCHMENU,
#endif
    TEXTURE_ID_MAX,
};

enum {
    TEXTURE_ACTIVE_FRAMEBUFFER     = GL_TEXTURE0,
    TEXTURE_ACTIVE_MESSAGE         = GL_TEXTURE1,
#if INTERFACE_TOUCH
    TEXTURE_ACTIVE_TOUCHJOY_AXIS   = GL_TEXTURE2,
    TEXTURE_ACTIVE_TOUCHJOY_BUTTON = GL_TEXTURE3,
    TEXTURE_ACTIVE_TOUCHKBD        = GL_TEXTURE4,
    TEXTURE_ACTIVE_TOUCHMENU       = GL_TEXTURE5,
#endif
    TEXTURE_ACTIVE_MAX,
};

// Important common shader values
extern GLint texSamplerLoc;
extern GLint alphaValue;
extern GLuint mainShaderProgram;
extern GLfloat mvpIdentity[16]; // Common Model View Projection matrix

// http://stackoverflow.com/questions/13676070/how-to-properly-mix-drawing-calls-and-changes-of-a-sampler-value-with-a-single-s
// https://developer.qualcomm.com/forum/qdevnet-forums/mobile-gaming-graphics-optimization-adreno/8896
extern bool hackAroundBrokenAdreno200;
extern bool hackAroundBrokenAdreno205;
#define _HACKAROUND_GLDRAW_PRE() \
    ({ \
        if (hackAroundBrokenAdreno200) { \
            glUseProgram(0); \
            glUseProgram(mainShaderProgram); \
        } \
     })

#define _HACKAROUND_GLTEXIMAGE2D_PRE(ACTIVE, NAME) \
    ({ \
        if (hackAroundBrokenAdreno205) { \
            /* Adreno 205 driver (HTC Desire) is even more broken than the 200!  It appears that we must delete and recreate textures every time we upload new pixels! */ \
            glBindTexture(GL_TEXTURE_2D, 0); \
            glDeleteTextures(1, &(NAME)); \
            glGenTextures(1, &(NAME)); \
            glActiveTexture((ACTIVE)); \
            glBindTexture(GL_TEXTURE_2D, (NAME)); \
            /* HACK NOTE : these should match what is (currently hardcoded) in modelUtil.c */ \
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); \
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); \
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); \
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); \
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); \
        } \
     })

#if INTERFACE_TOUCH
#   define TRACKING_NONE (-1)
#endif

typedef enum glnode_render_order_t {
    RENDER_BOTTOM=0,    // e.g., the //e framebuffer node itself
    RENDER_LOW   =1,    // e.g., the touchjoy and touchkbd
    RENDER_MIDDLE=10,   // e.g., floating messages
    RENDER_TOP   =20,   // e.g., the HUD menu items
} glnode_render_order_t;

// renderable and potentially interactive node
typedef struct GLNode {
    void (*setup)(void);
    void (*shutdown)(void);
    void (*render)(void);
#if INTERFACE_TOUCH
    interface_device_t type;
    int64_t (*onTouchEvent)(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords);
#endif
} GLNode;

// registers a node with manager
void glnode_registerNode(glnode_render_order_t order, GLNode node);

// swizzle width/height if they don't match landscape/portrait
static void inline swizzleDimensions(int *w, int *h, bool landscape) {
    if ( (landscape && (*w < *h)) || (!landscape && (*w > *h)) ) {
        int x = *w;
        *w = *h;
        *h = x;
    }
}

// animations
extern video_animation_s glnode_animations;

#endif // whole file

