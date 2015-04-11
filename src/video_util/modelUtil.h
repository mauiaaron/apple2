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

#include "common.h"

#define UNINITIALIZED_GL 31337

typedef struct GLCustom {
    void (*dtor)(INOUT struct GLCustom **custom);   // custom data destructor
    // overridden objects must at least implement interface methods
} GLCustom;

typedef struct GLModel {
    GLuint numVertices;

    GLvoid *positions;
    GLenum positionType;
    GLuint positionSize;
    GLsizei positionArraySize;

    GLvoid *texCoords;
    GLenum texcoordType;
    GLuint texcoordSize;
    GLsizei texcoordArraySize;

    GLvoid *normals;
    GLenum normalType;
    GLuint normalSize;
    GLsizei normalArraySize;

    GLvoid *elements;
    GLenum elementType;
    GLuint numElements;
    GLsizei elementArraySize;

    GLsizei texWidth;
    GLsizei texHeight;
    GLsizei texFormat;
    GLvoid *texPixels;
    bool texDirty;

    GLenum primType;

    // GL generated data
#if USE_VAO
    GLuint vaoName;
#endif
    GLuint textureName;
    GLuint posBufferName;
    GLuint texcoordBufferName;
    GLuint elementBufferName;

    // Custom
    GLCustom *custom;
} GLModel;

GLModel *mdlLoadModel(const char *filepathname);

GLModel *mdlLoadQuadModel();

GLModel *mdlCreateQuad(GLfloat skew_x, GLfloat skew_y, GLfloat obj_w, GLfloat obj_h, GLfloat z, GLsizei tex_w, GLsizei tex_h, GLenum tex_format);

void mdlDestroyModel(INOUT GLModel **model);

#endif //__MODEL_UTIL_H__
