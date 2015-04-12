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

#include "modelUtil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct modelHeader {
    char fileIdentifier[30];
    unsigned int majorVersion;
    unsigned int minorVersion;
} modelHeader;

typedef struct modelTOC {
    unsigned int attribHeaderSize;
    unsigned int byteElementOffset;
    unsigned int bytePositionOffset;
    unsigned int byteTexcoordOffset;
    unsigned int byteNormalOffset;
} modelTOC;

typedef struct modelAttrib {
    unsigned int byteSize;
    GLenum datatype;
    GLenum primType; //If index data
    unsigned int sizePerElement;
    unsigned int numElements;
} modelAttrib;

GLModel *mdlLoadModel(const char *filepathname) {
    if (!filepathname) {
        return NULL;
    }
    GLModel *model = (GLModel *)calloc(sizeof(GLModel), 1);
    if (!model) {
        return NULL;
    }

    size_t sizeRead = 0;
    int error = 0;
    FILE *curFile = fopen(filepathname, "r");

    if (!curFile) {
        mdlDestroyModel(&model);
        return NULL;
    }

    modelHeader header = { { 0 } };
    sizeRead = fread(&header, 1, sizeof(modelHeader), curFile);
    if (sizeRead != sizeof(modelHeader)) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    if (strncmp(header.fileIdentifier, "AppleOpenGLDemoModelWWDC2010", sizeof(header.fileIdentifier))) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    if (header.majorVersion != 0 && header.minorVersion != 1) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    modelTOC toc = { 0 };
    sizeRead = fread(&toc, 1, sizeof(modelTOC), curFile);
    if (sizeRead != sizeof(modelTOC)) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    if (toc.attribHeaderSize > sizeof(modelAttrib)) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    modelAttrib attrib = { 0 };
    error = fseek(curFile, toc.byteElementOffset, SEEK_SET);
    if (error < 0) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    sizeRead = fread(&attrib, 1, toc.attribHeaderSize, curFile);
    if (sizeRead != toc.attribHeaderSize) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    model->elementArraySize = attrib.byteSize;
    model->elementType = attrib.datatype;
    model->numElements = attrib.numElements;

    // OpenGL ES cannot use UNSIGNED_INT elements
    // so if the model has UI element...
    if (GL_UNSIGNED_INT == model->elementType) {
        // ...load the UI elements and convert to UNSIGNED_SHORT

        GLubyte *uiElements = (GLubyte *)malloc(model->elementArraySize);
        size_t ushortElementArraySize = model->numElements * sizeof(GLushort);
        model->elements = (GLubyte *)malloc(ushortElementArraySize);

        sizeRead = fread(uiElements, 1, model->elementArraySize, curFile);
        if (sizeRead != model->elementArraySize) {
            fclose(curFile);
            mdlDestroyModel(&model);
            return NULL;
        }

        GLuint elemNum = 0;
        for (elemNum = 0; elemNum < model->numElements; elemNum++) {
            // can't handle this model if an element is out of the UNSIGNED_INT range
            if (((GLuint *)uiElements)[elemNum] >= 0xFFFF) {
                fclose(curFile);
                mdlDestroyModel(&model);
                return NULL;
            }

            ((GLushort *)model->elements)[elemNum] = ((GLuint *)uiElements)[elemNum];
        }
        free(uiElements);

        model->elementType = GL_UNSIGNED_SHORT;
        model->elementArraySize = model->numElements * sizeof(GLushort);
    } else {
        model->elements = (GLubyte*)malloc(model->elementArraySize);

        sizeRead = fread(model->elements, 1, model->elementArraySize, curFile);

        if (sizeRead != model->elementArraySize) {
            fclose(curFile);
            mdlDestroyModel(&model);
            return NULL;
        }
    }

    fseek(curFile, toc.bytePositionOffset, SEEK_SET);
    sizeRead = fread(&attrib, 1, toc.attribHeaderSize, curFile);
    if (sizeRead != toc.attribHeaderSize) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    model->positionArraySize = attrib.byteSize;
    model->positionType = attrib.datatype;
    model->positionSize = attrib.sizePerElement;
    model->numVertices = attrib.numElements;
    model->positions = (GLubyte*)malloc(model->positionArraySize);

    sizeRead = fread(model->positions, 1, model->positionArraySize, curFile);
    if (sizeRead != model->positionArraySize) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    error = fseek(curFile, toc.byteTexcoordOffset, SEEK_SET);
    if (error < 0) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    sizeRead = fread(&attrib, 1, toc.attribHeaderSize, curFile);
    if (sizeRead != toc.attribHeaderSize) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    model->texcoordArraySize = attrib.byteSize;
    model->texcoordType = attrib.datatype;
    model->texcoordSize = attrib.sizePerElement;

    // must have the same number of texcoords as positions
    if (model->numVertices != attrib.numElements) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    model->texCoords = (GLubyte*)malloc(model->texcoordArraySize);

    sizeRead = fread(model->texCoords, 1, model->texcoordArraySize, curFile);
    if (sizeRead != model->texcoordArraySize) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    error = fseek(curFile, toc.byteNormalOffset, SEEK_SET);
    if (error < 0) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    sizeRead = fread(&attrib, 1, toc.attribHeaderSize, curFile);

    if (sizeRead !=  toc.attribHeaderSize) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    model->normalArraySize = attrib.byteSize;
    model->normalType = attrib.datatype;
    model->normalSize = attrib.sizePerElement;

    // must have the same number of normals as positions
    if (model->numVertices != attrib.numElements) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    model->normals = (GLubyte*)malloc(model->normalArraySize );

    sizeRead =  fread(model->normals, 1, model->normalArraySize , curFile);
    if (sizeRead != model->normalArraySize) {
        fclose(curFile);
        mdlDestroyModel(&model);
        return NULL;
    }

    fclose(curFile);

    return model;
}

GLModel *mdlLoadQuadModel(void) {
    GLfloat posArray[] = {
        -200.0f, 0.0f, -200.0f,
         200.0f, 0.0f, -200.0f,
         200.0f, 0.0f,  200.0f,
        -200.0f, 0.0f,  200.0f
    };

    GLfloat texcoordArray[] = {
        0.0f,  1.0f,
        1.0f,  1.0f,
        1.0f,  0.0f,
        0.0f,  0.0f
    };

    GLfloat normalArray[] = {
        0.0f, 0.0f, 1.0,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };

    GLushort elementArray[] = {
        0, 2, 1,
        0, 3, 2
    };

    GLModel *model = (GLModel *)calloc(sizeof(GLModel), 1);

    if (!model) {
        return NULL;
    }

    model->positionType = GL_FLOAT;
    model->positionSize = 3;
    model->positionArraySize = sizeof(posArray);
    model->positions = (GLubyte*)malloc(model->positionArraySize);
    memcpy(model->positions, posArray, model->positionArraySize);

    model->texcoordType = GL_FLOAT;
    model->texcoordSize = 2;
    model->texcoordArraySize = sizeof(texcoordArray);
    model->texCoords = (GLubyte*)malloc(model->texcoordArraySize);
    memcpy(model->texCoords, texcoordArray, model->texcoordArraySize );

    model->normalType = GL_FLOAT;
    model->normalSize = 3;
    model->normalArraySize = sizeof(normalArray);
    model->normals = (GLubyte*)malloc(model->normalArraySize);
    memcpy(model->normals, normalArray, model->normalArraySize);

    model->elementArraySize = sizeof(elementArray);
    model->elements    = (GLubyte*)malloc(model->elementArraySize);
    memcpy(model->elements, elementArray, model->elementArraySize);

    model->primType = GL_TRIANGLES;

    model->numElements = sizeof(elementArray) / sizeof(GLushort);
    model->elementType = GL_UNSIGNED_SHORT;
    model->numVertices = model->positionArraySize / (model->positionSize * sizeof(GLfloat));

    return model;
}

static void _quadCreateVAOAndVBOs(GLModel *model) {

    // Create a vertex array object (VAO) to cache model parameters
#if USE_VAO
    glGenVertexArrays(1, &(model->vaoName));
    glBindVertexArray(model->vaoName);
#endif

    // Create a vertex buffer object (VBO) to store positions and load data
    glGenBuffers(1, &(model->posBufferName));
    glBindBuffer(GL_ARRAY_BUFFER, model->posBufferName);
    glBufferData(GL_ARRAY_BUFFER, model->positionArraySize, model->positions, GL_DYNAMIC_DRAW);

#if USE_VAO
    // Enable the position attribute for this VAO
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Get the size of the position type so we can set the stride properly
    GLsizei posTypeSize = getGLTypeSize(model->positionType);

    // Set up parmeters for position attribute in the VAO including,
    //  size, type, stride, and offset in the currenly bound VAO
    // This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX,        // What attibute index will this array feed in the vertex shader (see buildProgram)
                          model->positionSize,    // How many elements are there per position?
                          model->positionType,    // What is the type of this data?
                          GL_FALSE,                // Do we want to normalize this data (0-1 range for fixed-pont types)
                          model->positionSize*posTypeSize, // What is the stride (i.e. bytes between positions)?
                          0);    // What is the offset in the VBO to the position data?
#endif

    if (model->texCoords) {
        // Create a VBO to store texcoords
        glGenBuffers(1, &(model->texcoordBufferName));
        glBindBuffer(GL_ARRAY_BUFFER, model->texcoordBufferName);

        // Allocate and load texcoord data into the VBO
        glBufferData(GL_ARRAY_BUFFER, model->texcoordArraySize, model->texCoords, GL_DYNAMIC_DRAW);

#if USE_VAO
        // Enable the texcoord attribute for this VAO
        glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

        // Get the size of the texcoord type so we can set the stride properly
        GLsizei texcoordTypeSize = getGLTypeSize(model->texcoordType);

        // Set up parmeters for texcoord attribute in the VAO including,
        //   size, type, stride, and offset in the currenly bound VAO
        // This also attaches the texcoord VBO to VAO
        glVertexAttribPointer(TEXCOORD_ATTRIB_IDX,    // What attibute index will this array feed in the vertex shader (see buildProgram)
                              model->texcoordSize,    // How many elements are there per texture coord?
                              model->texcoordType,    // What is the type of this data in the array?
                              GL_TRUE,                // Do we want to normalize this data (0-1 range for fixed-point types)
                              model->texcoordSize*texcoordTypeSize,  // What is the stride (i.e. bytes between texcoords)?
                              0);    // What is the offset in the VBO to the texcoord data?
#endif
    }

    // Create a VBO to vertex array elements
    // This also attaches the element array buffer to the VAO
    glGenBuffers(1, &(model->elementBufferName));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->elementBufferName);

    // Allocate and load vertex array element data into VBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->elementArraySize, model->elements, GL_DYNAMIC_DRAW);

    GL_ERRLOG("quad creation of VAO/VBOs");
}

static GLuint _quadCreateTexture(GLModel *model) {

    GLuint texName = UNINITIALIZED_GL;

    // Create a texture object to apply to model
    glGenTextures(1, &texName);
    glBindTexture(GL_TEXTURE_2D, texName);

    // Set up filter and wrap modes for this texture object
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Indicate that pixel rows are tightly packed
    //  (defaults to stride of 4 which is kind of only good for
    //  RGBA or FLOAT data types)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // register texture with OpenGL
    glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/model->texFormat, model->texWidth, model->texHeight, /*border*/0, /*format*/model->texFormat, GL_UNSIGNED_BYTE, NULL);

    GL_ERRLOG("quad texture creation");

    return texName;
}

GLModel *mdlCreateQuad(GLfloat skew_x, GLfloat skew_y, GLfloat obj_w, GLfloat obj_h, GLfloat z, GLsizei tex_w, GLsizei tex_h, GLenum tex_format, GLCustom clazz) {

    /* 2...3
     *  .
     *   .
     *    .
     * 0...1
     */

    const GLfloat obj_positions[] = {
        skew_x,        skew_y,       z, 1.0,
        skew_x+obj_w,  skew_y,       z, 1.0,
        skew_x,        skew_y+obj_h, z, 1.0,
        skew_x+obj_w,  skew_y+obj_h, z, 1.0,
    };
    const GLfloat obj_texcoords[] = {
        0.f, 1.f,
        1.f, 1.f,
        0.f, 0.f,
        1.f, 0.f,
    };
    const GLushort indices[] = {
        0, 1, 2, 2, 1, 3
    };

    GLModel *model = NULL;

    do {
        model = calloc(1, sizeof(GLModel));
        if (!model) {
            break;
        }
        model->numVertices = 4;
        model->numElements = 6;
        model->primType = GL_TRIANGLES;

        model->positions = malloc(sizeof(obj_positions));
        if (!(model->positions)) {
            break;
        }
        memcpy(model->positions, &obj_positions[0], sizeof(obj_positions));
        model->positionType = GL_FLOAT;
        model->positionSize = 4; // x,y,z coordinates
        model->positionArraySize = sizeof(obj_positions);

        if (tex_w > 0 && tex_h > 0) {
            model->texCoords = malloc(sizeof(obj_texcoords));
            if (!(model->texCoords)) {
                break;
            }
            memcpy(model->texCoords, &obj_texcoords[0], sizeof(obj_texcoords));
            model->texcoordType = GL_FLOAT;
            model->texcoordSize = 2; // s,t coordinates
            model->texcoordArraySize = sizeof(obj_texcoords);
        }

        {
            // NO NORMALS for now
            model->normals = NULL;
            model->normalType = GL_NONE;
            model->normalSize = GL_NONE;
            model->normalArraySize = 0;
        }

        model->elements = malloc(sizeof(indices));
        if (!(model->elements)) {
            break;
        }
        memcpy(model->elements, &indices[0], sizeof(indices));
        model->elementType = GL_UNSIGNED_SHORT;
        model->elementArraySize = sizeof(indices);

#if USE_VAO
        model->vaoName = UNINITIALIZED_GL;
#endif
        model->posBufferName = UNINITIALIZED_GL;
        model->texcoordBufferName = UNINITIALIZED_GL;
        model->elementBufferName = UNINITIALIZED_GL;

        _quadCreateVAOAndVBOs(model);
        if (model->posBufferName == UNINITIALIZED_GL || model->texcoordBufferName == UNINITIALIZED_GL || model->elementBufferName == UNINITIALIZED_GL) {
            LOG("Error creating model buffers!");
            break;
        }

        model->texDirty = true;
        model->texWidth = tex_w;
        model->texHeight = tex_h;
        model->texFormat = tex_format;
        if (tex_format == GL_RGBA) {
            model->texPixels = (GLvoid *)calloc(tex_w*tex_h*4, 1);
        } else {
            ERRQUIT("non-GL_RBGA format textures untested ... FIXME!");
        }
        if (model->texPixels == NULL) {
            break;
        }
        model->textureName = _quadCreateTexture(model);
        if (model->textureName == UNINITIALIZED_GL) {
            LOG("Error creating model texture!");
            break;
        }

        model->custom = NULL;
        if (clazz.create) {
            model->custom = clazz.create();
            if (model->custom) {
                model->custom->create = NULL;
                model->custom->setup = clazz.setup;
                model->custom->destroy = clazz.destroy;
                model->custom->setup(model);
            }
        }

        GL_ERRLOG("quad creation");

        return model;
    } while (0);

    ERRLOG("error in quad creation");
    if (model) {
        mdlDestroyModel(&model);
    }

    return NULL;
}

void mdlDestroyModel(INOUT GLModel **model) {
    if (!model || (!*model)) {
        return;
    }

    GLModel *m = *model;

    FREE(m->elements);
    FREE(m->positions);
    FREE(m->normals);
    FREE(m->texCoords);
    FREE(m->texPixels);

    if (m->textureName != UNINITIALIZED_GL) {
        glDeleteTextures(1, &(m->textureName));
        m->textureName = UNINITIALIZED_GL;
    }
#if USE_VAO
    if (m->vaoName != UNINITIALIZED_GL) {
        glBindVertexArray(m->vaoName);

        // For every possible attribute set in the VAO
        for (GLuint index = 0; index < 16; index++) {
            // Get the VBO set for that attibute
            GLuint bufName = 0;
            glGetVertexAttribiv(index , GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, (GLint*)&bufName);

            // If there was a VBO set...
            if (bufName) {
                //...delete the VBO
                glDeleteBuffers(1, &bufName);
            }
        }

        // Get any element array VBO set in the VAO
        {
            GLuint bufName = 0;
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint*)&bufName);

            // If there was a element array VBO set in the VAO
            if (bufName) {
                //...delete the VBO
                glDeleteBuffers(1, &bufName);
            }
        }

        // Finally, delete the VAO
        glDeleteVertexArrays(1, &(m->vaoName));
    }
#else
    if (m->posBufferName != UNINITIALIZED_GL) {
        glDeleteBuffers(1, &(m->posBufferName));
        m->posBufferName = UNINITIALIZED_GL;
    }
    if (m->texcoordBufferName != UNINITIALIZED_GL) {
        glDeleteBuffers(1, &(m->texcoordBufferName));
        m->texcoordBufferName = UNINITIALIZED_GL;
    }
    if (m->elementBufferName != UNINITIALIZED_GL) {
        glDeleteBuffers(1, &(m->elementBufferName));
        m->elementBufferName = UNINITIALIZED_GL;
    }
#endif

    if (m->custom) {
        m->custom->destroy(m);
    }

    FREE(*model);
}

