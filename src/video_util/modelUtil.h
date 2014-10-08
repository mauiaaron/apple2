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

#ifndef __MODEL_UTIL_H__
#define __MODEL_UTIL_H__

#include "glUtil.h"

typedef struct demoModelRec {
    GLuint numVertices;

    GLubyte *positions;
    GLenum positionType;
    GLuint positionSize;
    GLsizei positionArraySize;

    GLubyte *texcoords;
    GLenum texcoordType;
    GLuint texcoordSize;
    GLsizei texcoordArraySize;

    GLubyte *normals;
    GLenum normalType;
    GLuint normalSize;
    GLsizei normalArraySize;

    GLubyte *elements;
    GLenum elementType;
    GLuint numElements;
    GLsizei elementArraySize;

    GLenum primType;

} demoModel;

demoModel *mdlLoadModel(const char *filepathname);

demoModel *mdlLoadQuadModel();

void mdlDestroyModel(demoModel *model);

#endif //__MODEL_UTIL_H__
