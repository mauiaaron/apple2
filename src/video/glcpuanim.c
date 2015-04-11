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

#include "common.h"
#include "video/glanimation.h"
#include "video/glvideo.h"

#define CPUTIMING_TEMPLATE_COLS 8
#define CPUTIMING_TEMPLATE_ROWS 3

// HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...
#define MESSAGE_FB_WIDTH ((CPUTIMING_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) + INTERPOLATED_PIXEL_ADJUSTMENT)
#define MESSAGE_FB_HEIGHT (CPUTIMING_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

static bool animation_subsystem_functional = false;

static char cputiming_template[CPUTIMING_TEMPLATE_ROWS][CPUTIMING_TEMPLATE_COLS+1] = {
    "||||||||",
    "| xxx% |",
    "||||||||",
};

static struct timespec cputiming_begin = { 0 };
static bool cputiming_enabled = true;
static bool texture_dirty = true;

static GLModel *cpuMessageObjModel = NULL;
static GLuint cpuMessageObjVAOName = UNINITIALIZED_GL;
static GLenum cpuMessageObjElementType = UNINITIALIZED_GL;
static GLuint cpuMessageObjNumElements = UNINITIALIZED_GL;
static GLuint cpuMessageObjTextureName = UNINITIALIZED_GL;
static GLuint cpuMessageObjPosBufferName = UNINITIALIZED_GL;
static GLuint cpuMessageObjTexcoordBufferName = UNINITIALIZED_GL;
static GLuint cpuMessageObjElementBufferName = UNINITIALIZED_GL;

static uint8_t cpuMessageFB[MESSAGE_FB_WIDTH * MESSAGE_FB_HEIGHT] = { 0 };
static uint8_t cpuMessagePixels[MESSAGE_FB_WIDTH * MESSAGE_FB_HEIGHT * 4] = { 0 };// RGBA8888
static glanim_t cpuMessageAnimation = { 0 };

static void _create_message_model(void) {

    const GLfloat messageObj_positions[] = {
        -0.3, -0.3, -0.0625, 1.0,
         0.3, -0.3, -0.0625, 1.0,
        -0.3,  0.3, -0.0625, 1.0,
         0.3,  0.3, -0.0625, 1.0,
    };
    const GLfloat messageObj_texcoords[] = {
        0.0, 1.0,
        1.0, 1.0,
        0.0, 0.0,
        1.0, 0.0,
    };
    const GLushort indices[] = {
        0, 1, 2, 2, 1, 3
    };

    GLModel *messageObj = calloc(1, sizeof(GLModel));
    messageObj->numVertices = 4;
    messageObj->numElements = 6;

    messageObj->positions = malloc(sizeof(messageObj_positions));
    memcpy(messageObj->positions, &messageObj_positions[0], sizeof(messageObj_positions));
    messageObj->positionType = GL_FLOAT;
    messageObj->positionSize = 4; // x,y,z coordinates
    messageObj->positionArraySize = sizeof(messageObj_positions);

    messageObj->texCoords = malloc(sizeof(messageObj_texcoords));
    memcpy(messageObj->texCoords, &messageObj_texcoords[0], sizeof(messageObj_texcoords));
    messageObj->texcoordType = GL_FLOAT;
    messageObj->texcoordSize = 2; // s,t coordinates
    messageObj->texcoordArraySize = sizeof(messageObj_texcoords);

    messageObj->normals = NULL;
    messageObj->normalType = GL_NONE;
    messageObj->normalSize = GL_NONE;
    messageObj->normalArraySize = 0;

    messageObj->elements = malloc(sizeof(indices));
    memcpy(messageObj->elements, &indices[0], sizeof(indices));
    messageObj->elementType = GL_UNSIGNED_SHORT;
    messageObj->elementArraySize = sizeof(indices);

    mdlDestroyModel(&cpuMessageObjModel);
    cpuMessageObjModel = messageObj;
}

static void _create_message_VAO_VBOs(const GLModel *messageModel, GLuint *messageVAOName, GLuint *posBufferName, GLuint *texcoordBufferName, GLuint *elementBufferName) {

    // Create a vertex array object (VAO) to cache model parameters
#if USE_VAO
    glGenVertexArrays(1, messageVAOName);
    glBindVertexArray(*messageVAOName);
#endif

    // Create a vertex buffer object (VBO) to store positions and load data
    glGenBuffers(1, posBufferName);
    glBindBuffer(GL_ARRAY_BUFFER, *posBufferName);
    glBufferData(GL_ARRAY_BUFFER, messageModel->positionArraySize, messageModel->positions, GL_STATIC_DRAW);

#if USE_VAO
    // Enable the position attribute for this VAO
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Get the size of the position type so we can set the stride properly
    GLsizei posTypeSize = _get_gl_type_size(messageModel->positionType);

    // Set up parmeters for position attribute in the VAO including,
    //  size, type, stride, and offset in the currenly bound VAO
    // This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX,        // What attibute index will this array feed in the vertex shader (see buildProgram)
                          messageModel->positionSize,    // How many elements are there per position?
                          messageModel->positionType,    // What is the type of this data?
                          GL_FALSE,                // Do we want to normalize this data (0-1 range for fixed-pont types)
                          messageModel->positionSize*posTypeSize, // What is the stride (i.e. bytes between positions)?
                          0);    // What is the offset in the VBO to the position data?
#endif

    if (messageModel->texCoords) {
        // Create a VBO to store texcoords
        glGenBuffers(1, texcoordBufferName);
        glBindBuffer(GL_ARRAY_BUFFER, *texcoordBufferName);

        // Allocate and load texcoord data into the VBO
        glBufferData(GL_ARRAY_BUFFER, messageModel->texcoordArraySize, messageModel->texCoords, GL_STATIC_DRAW);

#if USE_VAO
        // Enable the texcoord attribute for this VAO
        glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

        // Get the size of the texcoord type so we can set the stride properly
        GLsizei texcoordTypeSize = _get_gl_type_size(messageModel->texcoordType);

        // Set up parmeters for texcoord attribute in the VAO including,
        //   size, type, stride, and offset in the currenly bound VAO
        // This also attaches the texcoord VBO to VAO
        glVertexAttribPointer(TEXCOORD_ATTRIB_IDX,    // What attibute index will this array feed in the vertex shader (see buildProgram)
                              messageModel->texcoordSize,    // How many elements are there per texture coord?
                              messageModel->texcoordType,    // What is the type of this data in the array?
                              GL_TRUE,                // Do we want to normalize this data (0-1 range for fixed-point types)
                              messageModel->texcoordSize*texcoordTypeSize,  // What is the stride (i.e. bytes between texcoords)?
                              0);    // What is the offset in the VBO to the texcoord data?
#endif
    }

    // Create a VBO to vertex array elements
    // This also attaches the element array buffer to the VAO
    glGenBuffers(1, elementBufferName);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *elementBufferName);

    // Allocate and load vertex array element data into VBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, messageModel->elementArraySize, messageModel->elements, GL_STATIC_DRAW);

    GL_ERRLOG("finished creating VAO/VBOs");
}

static void _destroy_VAO_VBOs(GLuint vaoName, GLuint posBufferName, GLuint texcoordBufferName, GLuint elementBufferName) {

    // Bind the VAO so we can get data from it
#if USE_VAO
    glBindVertexArray(vaoName);

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
    glDeleteVertexArrays(1, &vaoName);
#else
    glDeleteBuffers(1, &posBufferName);
    glDeleteBuffers(1, &texcoordBufferName);
    glDeleteBuffers(1, &elementBufferName);
#endif

    GL_ERRLOG("destroying VAO/VBOs");
}

static void _create_message_texture(void) {
    cpuMessageObjTextureName = UNINITIALIZED_GL;

    // Create a texture object to apply to model
    glGenTextures(1, &cpuMessageObjTextureName);
    glBindTexture(GL_TEXTURE_2D, cpuMessageObjTextureName);

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
    glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, MESSAGE_FB_WIDTH, MESSAGE_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)cpuMessagePixels);

    GL_ERRLOG("finished creating message texture");
}

static void cpuanim_init(void) {
    LOG("gldriver_animation_init ...");
    _create_message_model();

    cpuMessageObjVAOName = UNINITIALIZED_GL;
    cpuMessageObjPosBufferName = UNINITIALIZED_GL;
    cpuMessageObjTexcoordBufferName = UNINITIALIZED_GL;
    cpuMessageObjElementBufferName = UNINITIALIZED_GL;
    _create_message_VAO_VBOs(cpuMessageObjModel, &cpuMessageObjVAOName, &cpuMessageObjPosBufferName, &cpuMessageObjTexcoordBufferName, &cpuMessageObjElementBufferName);
    if (cpuMessageObjPosBufferName == UNINITIALIZED_GL || cpuMessageObjTexcoordBufferName == UNINITIALIZED_GL || cpuMessageObjElementBufferName == UNINITIALIZED_GL)
    {
        LOG("not initializing CPU speed animations");
        return;
    }

    cpuMessageObjNumElements = cpuMessageObjModel->numElements;
    cpuMessageObjElementType = cpuMessageObjModel->elementType;

    _create_message_texture();

    animation_subsystem_functional = true;
}

static void cpuanim_destroy(void) {
    LOG("gldriver_animation_destroy ...");
    if (!animation_subsystem_functional) {
        return;
    }

    animation_subsystem_functional = false;

    // cleanup cputiming message objects
    glDeleteTextures(1, &cpuMessageObjTextureName);
    cpuMessageObjTextureName = UNINITIALIZED_GL;

    _destroy_VAO_VBOs(cpuMessageObjVAOName, cpuMessageObjPosBufferName, cpuMessageObjTexcoordBufferName, cpuMessageObjElementBufferName);
    cpuMessageObjVAOName = UNINITIALIZED_GL;
    cpuMessageObjPosBufferName = UNINITIALIZED_GL;
    cpuMessageObjTexcoordBufferName = UNINITIALIZED_GL;
    cpuMessageObjElementBufferName = UNINITIALIZED_GL;

    mdlDestroyModel(&cpuMessageObjModel);
    cpuMessageObjModel = NULL;
}

static void _render_message_object(GLuint vaoName, GLuint posBufferName, GLuint texcoordBufferName, GLuint elementBufferName) {

    // Bind our vertex array object
#if USE_VAO
    glBindVertexArray(vaoName);
#else
    glBindBuffer(GL_ARRAY_BUFFER, posBufferName);

    GLsizei posTypeSize      = _get_gl_type_size(cpuMessageObjModel->positionType);
    GLsizei texcoordTypeSize = _get_gl_type_size(cpuMessageObjModel->texcoordType);

    // Set up parmeters for position attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX,                                   // What attibute index will this array feed in the vertex shader (see buildProgram)
                          cpuMessageObjModel->positionSize,                 // How many elements are there per position?
                          cpuMessageObjModel->positionType,                 // What is the type of this data?
                          GL_FALSE,                                         // Do we want to normalize this data (0-1 range for fixed-pont types)
                          cpuMessageObjModel->positionSize*posTypeSize,     // What is the stride (i.e. bytes between positions)?
                          0);                                               // What is the offset in the VBO to the position data?
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Set up parmeters for texcoord attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the texcoord VBO to VAO
    glBindBuffer(GL_ARRAY_BUFFER, texcoordBufferName);
    glVertexAttribPointer(TEXCOORD_ATTRIB_IDX,                              // What attibute index will this array feed in the vertex shader (see buildProgram)
                          cpuMessageObjModel->texcoordSize,                 // How many elements are there per texture coord?
                          cpuMessageObjModel->texcoordType,                 // What is the type of this data in the array?
                          GL_TRUE,                                          // Do we want to normalize this data (0-1 range for fixed-point types)
                          cpuMessageObjModel->texcoordSize*texcoordTypeSize,// What is the stride (i.e. bytes between texcoords)?
                          0);                                               // What is the offset in the VBO to the texcoord data?
    glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferName);
#endif

    // Draw the message object
    glDrawElements(GL_TRIANGLES, cpuMessageObjNumElements, cpuMessageObjElementType, 0);
    GL_ERRLOG("CPU message render");
}

static void cpuanim_render(void) {
    if (!animation_subsystem_functional) {
        return;
    }
    if (!cputiming_enabled) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    float alpha = 0.95;
    struct timespec deltat = timespec_diff(cputiming_begin, now, NULL);
    if (deltat.tv_sec >= 1) {
        cputiming_enabled = false;
        return;
    } else if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
        alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
        if (alpha < 0.0) {
            alpha = 0.0;
        }
    }
    //LOG("alpha : %f", alpha);
    glUniform1f(alphaValue, alpha);

    glActiveTexture(TEXTURE_ACTIVE_MESSAGE);
    glBindTexture(GL_TEXTURE_2D, cpuMessageObjTextureName);
    if (texture_dirty) {
        texture_dirty = false;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, MESSAGE_FB_WIDTH, MESSAGE_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)cpuMessagePixels);
    }
    glUniform1i(uniformTex2Use, TEXTURE_ID_MESSAGE);
    _render_message_object(cpuMessageObjVAOName, cpuMessageObjPosBufferName, cpuMessageObjTexcoordBufferName, cpuMessageObjElementBufferName);
}

static void cpuanim_reshape(int w, int h) {
    // no-op
}

static void cpuanim_show(void) {
    if (!animation_subsystem_functional) {
        return;
    }

    char buf[8];
    double scale = (alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor);
    int percentScale = scale * 100;
    if (percentScale < 100.0) {
        snprintf(buf, 3, "%d", percentScale);
        cputiming_template[1][2] = ' ';
        cputiming_template[1][3] = buf[0];
        cputiming_template[1][4] = buf[1];
    } else if (scale == CPU_SCALE_FASTEST) {
        cputiming_template[1][2] = 'm';
        cputiming_template[1][3] = 'a';
        cputiming_template[1][4] = 'x';
    } else {
        snprintf(buf, 4, "%d", percentScale);
        cputiming_template[1][2] = buf[0];
        cputiming_template[1][3] = buf[1];
        cputiming_template[1][4] = buf[2];
    }

    // render template into indexed fb
    unsigned int submenu_width = CPUTIMING_TEMPLATE_COLS;
    unsigned int submenu_height = CPUTIMING_TEMPLATE_ROWS;
    char *submenu = cputiming_template[0];
    video_interface_print_submenu_centered_fb(cpuMessageFB, submenu_width, submenu_height, submenu, submenu_width, submenu_height);

    // generate RGBA_8888 from indexed color
    unsigned int count = MESSAGE_FB_WIDTH * MESSAGE_FB_HEIGHT;
    for (unsigned int i=0, j=0; i<count; i++, j+=4) {
        uint8_t index = *(cpuMessageFB + i);
        *( (uint32_t*)(cpuMessagePixels + j) ) = (uint32_t)(
                                                  ((uint32_t)(colormap[index].red)   << 0 ) |
                                                  ((uint32_t)(colormap[index].green) << 8 ) |
                                                  ((uint32_t)(colormap[index].blue)  << 16) |
                                                  ((uint32_t)0xff                    << 24)
                                                  );
    }

    cputiming_enabled = true;
    texture_dirty = true;
    clock_gettime(CLOCK_MONOTONIC, &cputiming_begin);
}

__attribute__((constructor))
static void _init_glcpuanim(void) {
    LOG("Registering CPU speed animations");
    video_animation_show_cpuspeed = &cpuanim_show;
    cpuMessageAnimation.ctor = &cpuanim_init;
    cpuMessageAnimation.dtor = &cpuanim_destroy;
    cpuMessageAnimation.render = &cpuanim_render;
    cpuMessageAnimation.reshape = &cpuanim_reshape;
    gldriver_register_animation(&cpuMessageAnimation);
}

