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

// Modified sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#ifndef __GL_UTIL_H__
#define __GL_UTIL_H__

#if defined(__APPLE__)
#   include <TargetConditionals.h>
#   if TARGET_OS_IPHONE
#       import <OpenGLES/ES2/gl.h>
#       import <OpenGLES/ES2/glext.h>
#   else
#       import <OpenGL/OpenGL.h>
#       import <OpenGL/gl3.h>
#   endif
#elif defined(USE_GL3W)
#   include <GL3/gl3.h>
#   include <GL3/gl3w.h>
#elif defined(ANDROID)
#   include <GLES2/gl2.h>
#   include <GLES2/gl2ext.h>
#else
#   define GLEW_STATIC
#   include <GL/glew.h>
#   define FREEGLUT_STATIC
#   include <GL/freeglut.h>
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
