/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
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
#endif
};

enum {
    TEXTURE_ACTIVE_FRAMEBUFFER     = GL_TEXTURE0,
    TEXTURE_ACTIVE_MESSAGE         = GL_TEXTURE1,
#if INTERFACE_TOUCH
    TEXTURE_ACTIVE_TOUCHJOY_AXIS   = GL_TEXTURE2,
    TEXTURE_ACTIVE_TOUCHJOY_BUTTON = GL_TEXTURE3,
#endif
};

extern GLint uniformTex2Use;
extern GLint alphaValue;

#endif

