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

// Modified sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#ifndef __GL_UTIL_H__
#define __GL_UTIL_H__

#if defined(__APPLE__)
#   define USE_VAO 1
#   import <CoreFoundation/CoreFoundation.h>
#   import <TargetConditionals.h>
#   if TARGET_OS_IPHONE
#       import <OpenGLES/ES3/gl.h>
#       import <OpenGLES/ES3/glext.h>
#   else
#       import <OpenGL/OpenGL.h>
#       import <OpenGL/gl3.h>
#   endif
#elif defined(USE_GL3W)
#   include <GL3/gl3.h>
#   include <GL3/gl3w.h>
#elif defined(ANDROID)
// NOTE : 2015/04/01 ... Certain Android and Android-ish devices (*cough* Kindle *cough*) have buggy OpenGL VAO support,
// so don't rely on it.  Is it the future yet?
#   define USE_VAO 0
#   include <GLES2/gl2.h>
#   include <GLES2/gl2ext.h>
#else
#   define GLEW_STATIC
#   include <GL/glew.h>
#   define FREEGLUT_STATIC
#   include <GL/freeglut.h>
#endif

#if !defined(USE_VAO)
#define USE_VAO 1
#endif

// Global unified texture format constants ...

#define TEX_FORMAT GL_RGBA

#if USE_RGBA4444
#   define PIXEL_TYPE uint16_t
#   define MAX_SATURATION 0xf
#   define SHIFT_R 12
#   define SHIFT_G 8
#   define SHIFT_B 4
#   define SHIFT_A 0
#   define TEX_FORMAT_INTERNAL GL_RGBA4
#   define TEX_TYPE GL_UNSIGNED_SHORT_4_4_4_4
#else
// assuming RGBA8888 ...
#   define PIXEL_TYPE uint32_t
#   define MAX_SATURATION 0xff
#   define SHIFT_R 0
#   define SHIFT_G 8
#   define SHIFT_B 16
#   define SHIFT_A 24
#   define TEX_FORMAT_INTERNAL TEX_FORMAT
#   define TEX_TYPE GL_UNSIGNED_BYTE
#endif

static inline const char * GetGLErrorString(GLenum error) {
    const char *str;
    switch (error) {
        case GL_NO_ERROR:
            str = "GL_NO_ERROR";
            break;
        case GL_INVALID_ENUM:
            str = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            str = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            str = "GL_INVALID_OPERATION";
            break;
#if defined __gl_h_ || defined __gl3_h_
        case GL_OUT_OF_MEMORY:
            str = "GL_OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            str = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
#endif
#if defined __gl_h_
        case GL_STACK_OVERFLOW:
            str = "GL_STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            str = "GL_STACK_UNDERFLOW";
            break;
        case GL_TABLE_TOO_LARGE:
            str = "GL_TABLE_TOO_LARGE";
            break;
#endif
        default:
            str = "(ERROR: Unknown Error Enum)";
            break;
    }
    return str;
}

#endif // __GL_UTIL_H__
