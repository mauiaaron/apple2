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

#define UNINITIALIZED_GL (-31337)
#warning FIXME TODO : is there an official OpenGL value we can use to signify an uninitialized state?

enum {
    POS_ATTRIB_IDX,
    TEXCOORD_ATTRIB_IDX,
    NORMAL_ATTRIB_IDX,
};

typedef struct GLModel;

#define MODEL_CLASS(CLS, ...) \
    typedef struct CLS { \
        void *(*create)(void); \
        void (*setup)(struct GLModel *parent); \
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
#warning FIXME TODO : investigate whether we can just MACRO-inherit from GLModel rather than use custom pointer
    GLCustom *custom;
} GLModel;

GLModel *mdlLoadModel(const char *filepathname);

GLModel *mdlLoadQuadModel();

GLModel *mdlCreateQuad(GLfloat skew_x, GLfloat skew_y, GLfloat obj_w, GLfloat obj_h, GLfloat z, GLsizei tex_w, GLsizei tex_h, GLCustom clazz);

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
