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

// Based on sample code from https://developer.apple.com/library/mac/samplecode/GLEssentials/Introduction/Intro.html

#include "sourceUtil.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

demoSource *srcLoadSource(const char *filepathname) {
    demoSource *source = (demoSource *)CALLOC(sizeof(demoSource), 1);

    // Check the file name suffix to determine what type of shader this is
    const char *suffixBegin = filepathname + strlen(filepathname) - 4;

    if (!strncmp(suffixBegin, ".fsh", 4)) {
        source->shaderType = GL_FRAGMENT_SHADER;
    } else if (!strncmp(suffixBegin, ".vsh", 4)) {
        source->shaderType = GL_VERTEX_SHADER;
    } else {
        // Unknown suffix
        source->shaderType = 0;
    }

    FILE *curFile = fopen(filepathname, "r");

    // Get the size of the source
    fseek(curFile, 0, SEEK_END);
    GLsizei fileSize = (GLsizei)ftell(curFile);

    // Add 1 to the file size to include the null terminator for the string
    source->byteSize = fileSize + 1;

    // Alloc memory for the string
    source->string = MALLOC(source->byteSize);

    // Read entire file into the string from beginning of the file
    fseek(curFile, 0, SEEK_SET);
    fread(source->string, 1, fileSize, curFile);

    fclose(curFile);

    // Insert null terminator
    source->string[fileSize] = 0;

    return source;
}

void srcDestroySource(demoSource *source) {
    FREE(source->string);
    FREE(source);
}

