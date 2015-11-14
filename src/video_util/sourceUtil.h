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

#include "glUtil.h"

typedef struct demoSourceRec {
    GLchar *string;
    GLsizei byteSize;
    GLenum shaderType; // Vertex or Fragment
} demoSource;

demoSource *srcLoadSource(const char *filepathname);

void srcDestroySource(demoSource *source);

#endif // __SOURCE_UTIL_H__
