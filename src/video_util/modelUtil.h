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

#ifndef __MODEL_UTIL_H__
#define __MODEL_UTIL_H__

#include "common.h"

enum {
    POS_ATTRIB_IDX,
    TEXCOORD_ATTRIB_IDX,
#if 0
    NORMAL_ATTRIB_IDX,
#endif
};

typedef struct GLModel;

#define MODEL_CLASS(CLS, ...) \
    typedef struct CLS { \
        void *(*create)(struct GLModel *parent); \
        void (*destroy)(struct GLModel *parent); \
        __VA_ARGS__ \
    } CLS;

MODEL_CLASS(GLCustom);

typedef struct GLModel {

    GLuint numVertices;

    GLvoid *positions;
    GLenum positionType;
    GLuint positionSize;
    GLsizei positionArraySize;
    GLenum positionUsageHint;

    GLvoid *texCoords;
    GLenum texcoordType;
    GLuint texcoordSize;
    GLsizei texcoordArraySize;
    GLenum texcoordUsageHint;

#if 0
    GLvoid *normals;
    GLenum normalType;
    GLuint normalSize;
    GLsizei normalArraySize;
    GLenum normalUsageHint;
#endif

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
#warning FIXME TODO : investigate whether we can just MACRO-inherit from GLModel rather than use custom pointer
    GLCustom *custom;
} GLModel;

#if 0
GLModel *mdlLoadModel(const char *filepathname);

GLModel *mdlLoadQuadModel();
#endif

typedef struct GLModelParams_s {
    // positions
    GLfloat skew_x;
    GLfloat skew_y;
    GLfloat z;
    GLfloat obj_w;
    GLfloat obj_h;
    GLenum positionUsageHint;

    // texture
    GLsizei tex_w;
    GLsizei tex_h;
    GLenum texcoordUsageHint;

} GLModelParams_s;

GLModel *mdlCreateQuad(GLModelParams_s parms, GLCustom clazz);

void mdlDestroyModel(INOUT GLModel **model);

static inline GLsizei getGLTypeSize(GLenum type) {
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
    return sizeof(GLvoid);
}

#endif //__MODEL_UTIL_H__
