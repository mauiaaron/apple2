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

#ifndef __SOURCE_UTIL_H__
#define __SOURCE_UTIL_H__

#include "common.h"

typedef struct demoSourceRec {
    GLchar *string;
    GLsizei byteSize;
    GLenum shaderType; // Vertex or Fragment
} demoSource;

// Create a shader source object from a shader source file
extern demoSource *glshader_createSource(const char *filepathname);

// Destroy a shader source object
extern void glshader_destroySource(demoSource *source);

// Builds a GL program from shader sources
extern GLuint glshader_buildProgram(demoSource *vertexSource, demoSource *fragmentSource, /*bool hasNormal, */bool hasTexcoord, OUTPARM GLuint *vertexShader, OUTPARM GLuint *fragShader);

#endif // __SOURCE_UTIL_H__
