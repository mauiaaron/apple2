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

#ifndef _GLVIDEO_H_
#define _GLVIDEO_H_

// TODO: implement 3D CRT object, possibly with perspective drawing?
#define PERSPECTIVE 0

#include "video_util/modelUtil.h"
#include "video_util/matrixUtil.h"
#include "video_util/sourceUtil.h"

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

extern GLint texSamplerLoc;
extern GLint alphaValue;
extern GLuint mainShaderProgram;

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

#endif

