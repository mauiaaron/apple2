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
