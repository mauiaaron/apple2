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

// OpenGL header includes

// #define USE_GL3W

#if defined(__APPLE__)
#   include <OpenGLES/ES2/gl.h>
#   include <OpenGLES/ES2/glext.h>
#elif defined(USE_GL3W)
#   include <GL3/gl3.h>
#   include <GL3/gl3w.h>
#else
#   define GLEW_STATIC
#   include <GL/glew.h>
#   define FREEGLUT_STATIC
#   include <GL/freeglut.h>
#endif

