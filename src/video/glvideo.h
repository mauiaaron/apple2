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

#define UNINITIALIZED_GL 31337

#ifdef __APPLE__
#import <CoreFoundation/CoreFoundation.h>
#define USE_VAO 1
#endif

// TODO: implement 3D CRT object, possibly with perspective drawing?
#define PERSPECTIVE 0

// VAO optimization (may not be available on all platforms)
#ifdef ANDROID
#include "glanimation.h"
#warning Certain Android and Android-ish devices (*cough* Kindle *cough*) have buggy OpenGL VAO support ...
#define USE_VAO 0
#elif !defined(USE_VAO)
#define USE_VAO 1
#endif

#include "video_util/modelUtil.h"
#include "video_util/matrixUtil.h"
#include "video_util/sourceUtil.h"

enum {
    POS_ATTRIB_IDX,
    TEXCOORD_ATTRIB_IDX,
    NORMAL_ATTRIB_IDX,
};

enum {
    TEXTURE_ID_FRAMEBUFFER=0,
    TEXTURE_ID_MESSAGE,
#if TOUCH_JOYSTICK
    TEXTURE_ID_TOUCHJOY_AXIS,
    TEXTURE_ID_TOUCHJOY_BUTTON,
#endif
};

enum {
    TEXTURE_ACTIVE_FRAMEBUFFER     = GL_TEXTURE0,
    TEXTURE_ACTIVE_MESSAGE         = GL_TEXTURE1,
#if TOUCH_JOYSTICK
    TEXTURE_ACTIVE_TOUCHJOY_AXIS   = GL_TEXTURE2,
    TEXTURE_ACTIVE_TOUCHJOY_BUTTON = GL_TEXTURE3,
#endif
};

static inline GLsizei _get_gl_type_size(GLenum type) {
    switch (type) {
        case GL_BYTE:
            return sizeof(GLbyte);
        case GL_UNSIGNED_BYTE:
            return sizeof(GLubyte);
        case GL_SHORT:
            return sizeof(GLshort);
        case GL_UNSIGNED_SHORT:
            return sizeof(GLushort);
        case GL_INT:
            return sizeof(GLint);
        case GL_UNSIGNED_INT:
            return sizeof(GLuint);
        case GL_FLOAT:
            return sizeof(GLfloat);
    }
    return 0;
}

extern GLint uniformTex2Use;
extern GLint alphaValue;

#endif

