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
#include "video/glvideo.h"

#define MAX_FINGERS 32
#define TOUCHJOY_TEMPLATE_COLS 5
#define TOUCHJOY_TEMPLATE_ROWS 5

// HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...
#define TOUCHJOY_FB_WIDTH ((TOUCHJOY_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) + INTERPOLATED_PIXEL_ADJUSTMENT)
#define TOUCHJOY_FB_HEIGHT (TOUCHJOY_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

enum {
    TOUCHED_NONE = -1,
    TOUCHED_BUTTON0 = 0,
    TOUCHED_BUTTON1,
    TOUCHED_BOTH,
};

static bool isAvailable = false;
static bool isEnabled = true;
static bool isVisible = true;

static char axisTemplate[TOUCHJOY_TEMPLATE_ROWS][TOUCHJOY_TEMPLATE_COLS+1] = {
    "|||||",
    "| @ |",
    "|@+@|",
    "| @ |",
    "|||||",
};

static char buttonTemplate[TOUCHJOY_TEMPLATE_ROWS][TOUCHJOY_TEMPLATE_COLS+1] = {
    "  |||",
    "  |@|",
    "|||||",
    "|@|+ ",
    "|||  ",
};

static const GLfloat model_width = 0.5;
static const GLfloat model_height = 0.5;
static glanim_t touchjoyAnimation = { 0 };

static int viewportWidth = 0;
static int viewportHeight = 0;

// touch axis variables

static demoModel *touchAxisObjModel = NULL;
static GLuint touchAxisObjVAOName = UNINITIALIZED_GL;
static GLuint touchAxisObjTextureName = UNINITIALIZED_GL;
static GLuint touchAxisObjPosBufferName = UNINITIALIZED_GL;
static GLuint touchAxisObjTexcoordBufferName = UNINITIALIZED_GL;
static GLuint touchAxisObjElementBufferName = UNINITIALIZED_GL;

static int touchAxisObjModelScreenX = 0;
static int touchAxisObjModelScreenY = 0;
static int touchAxisObjModelScreenXMax = 0;
static int touchAxisObjModelScreenYMax = 0;

static uint8_t touchAxisObjFB[TOUCHJOY_FB_WIDTH * TOUCHJOY_FB_HEIGHT] = { 0 };
static uint8_t touchAxisObjPixels[TOUCHJOY_FB_WIDTH * TOUCHJOY_FB_HEIGHT * 4] = { 0 };// RGBA8888
static bool axisTextureDirty = true;

static int trackingAxisIndex = TOUCHED_NONE;

// button object variables

static demoModel *buttonObjModel = NULL;
static GLuint buttonObjVAOName = UNINITIALIZED_GL;
static GLuint buttonObjTextureName = UNINITIALIZED_GL;
static GLuint buttonObjPosBufferName = UNINITIALIZED_GL;
static GLuint buttonObjTexcoordBufferName = UNINITIALIZED_GL;
static GLuint buttonObjElementBufferName = UNINITIALIZED_GL;

static int buttonObj0ScreenX = 0;
static int buttonObj0ScreenY = 0;
static int buttonObj0ScreenXMax = 0;
static int buttonObj0ScreenYMax = 0;

static int buttonObj1ScreenX = 0;
static int buttonObj1ScreenY = 0;
static int buttonObj1ScreenXMax = 0;
static int buttonObj1ScreenYMax = 0;

static uint8_t buttonObjFB[TOUCHJOY_FB_WIDTH * TOUCHJOY_FB_HEIGHT] = { 0 };
static uint8_t buttonObjPixels[TOUCHJOY_FB_WIDTH * TOUCHJOY_FB_HEIGHT * 4] = { 0 };// RGBA8888
static bool buttonTextureDirty = true;

static int trackingButton0Index = TOUCHED_NONE;
static int trackingButton1Index = TOUCHED_NONE;
static int trackingButtonBothIndex = TOUCHED_NONE;

// configurables for current touchjoy

static uint8_t button0Char = MOUSETEXT_OPENAPPLE;
static uint8_t button1Char = MOUSETEXT_CLOSEDAPPLE;
static touchjoy_axis_type_t touchjoy_axisType = AXIS_EMULATED_DEVICE;
static uint8_t upChar    = MOUSETEXT_UP;
static uint8_t leftChar  = MOUSETEXT_LEFT;
static uint8_t rightChar = MOUSETEXT_RIGHT;
static uint8_t downChar  = MOUSETEXT_DOWN;

// ----------------------------------------------------------------------------

static demoModel *_create_model(GLfloat skew_x, GLfloat skew_y) {

    /* 2...3
     *  .
     *   .
     *    .
     * 0...1
     */

    const GLfloat obj_positions[] = {
        skew_x,             skew_y,              -0.03125, 1.0,
        skew_x+model_width, skew_y,              -0.03125, 1.0,
        skew_x,             skew_y+model_height, -0.03125, 1.0,
        skew_x+model_width, skew_y+model_height, -0.03125, 1.0,
    };
    const GLfloat obj_texcoords[] = {
        0.0, 1.0,
        1.0, 1.0,
        0.0, 0.0,
        1.0, 0.0,
    };
    const GLushort indices[] = {
        0, 1, 2, 2, 1, 3
    };

    demoModel *obj = calloc(1, sizeof(demoModel));
    obj->numVertices = 4;
    obj->numElements = 6;

    obj->positions = malloc(sizeof(obj_positions));
    memcpy(obj->positions, &obj_positions[0], sizeof(obj_positions));
    obj->positionType = GL_FLOAT;
    obj->positionSize = 4; // x,y,z coordinates
    obj->positionArraySize = sizeof(obj_positions);

    obj->texcoords = malloc(sizeof(obj_texcoords));
    memcpy(obj->texcoords, &obj_texcoords[0], sizeof(obj_texcoords));
    obj->texcoordType = GL_FLOAT;
    obj->texcoordSize = 2; // s,t coordinates
    obj->texcoordArraySize = sizeof(obj_texcoords);

    obj->normals = NULL;
    obj->normalType = GL_NONE;
    obj->normalSize = GL_NONE;
    obj->normalArraySize = 0;

    obj->elements = malloc(sizeof(indices));
    memcpy(obj->elements, &indices[0], sizeof(indices));
    obj->elementType = GL_UNSIGNED_SHORT;
    obj->elementArraySize = sizeof(indices);

    return obj;
}

static void _create_VAO_VBOs(const demoModel *model, GLuint *vaoName, GLuint *posBufferName, GLuint *texcoordBufferName, GLuint *elementBufferName) {

    // Create a vertex array object (VAO) to cache model parameters
#if USE_VAO
    glGenVertexArrays(1, vaoName);
    glBindVertexArray(*vaoName);
#endif

    // Create a vertex buffer object (VBO) to store positions and load data
    glGenBuffers(1, posBufferName);
    glBindBuffer(GL_ARRAY_BUFFER, *posBufferName);
    glBufferData(GL_ARRAY_BUFFER, model->positionArraySize, model->positions, GL_STATIC_DRAW);

#if USE_VAO
    // Enable the position attribute for this VAO
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Get the size of the position type so we can set the stride properly
    GLsizei posTypeSize = _get_gl_type_size(model->positionType);

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

    if (model->texcoords) {
        // Create a VBO to store texcoords
        glGenBuffers(1, texcoordBufferName);
        glBindBuffer(GL_ARRAY_BUFFER, *texcoordBufferName);

        // Allocate and load texcoord data into the VBO
        glBufferData(GL_ARRAY_BUFFER, model->texcoordArraySize, model->texcoords, GL_STATIC_DRAW);

#if USE_VAO
        // Enable the texcoord attribute for this VAO
        glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

        // Get the size of the texcoord type so we can set the stride properly
        GLsizei texcoordTypeSize = _get_gl_type_size(model->texcoordType);

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
    glGenBuffers(1, elementBufferName);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *elementBufferName);

    // Allocate and load vertex array element data into VBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->elementArraySize, model->elements, GL_STATIC_DRAW);

    GL_ERRLOG("gltouchjoy finished creating VAO/VBOs");
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

    GL_ERRLOG("gltouchjoy destroying VAO/VBOs");
}

static GLuint _create_texture(GLvoid *pixels) {
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
    glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, TOUCHJOY_FB_WIDTH, TOUCHJOY_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    GL_ERRLOG("gltouchjoy texture");

    return texName;
}

static void _setup_object(char *submenu, uint8_t *fb, uint8_t *pixels) {

    // render template into indexed fb
    unsigned int submenu_width = TOUCHJOY_TEMPLATE_COLS;
    unsigned int submenu_height = TOUCHJOY_TEMPLATE_ROWS;
    video_interface_print_submenu_centered_fb(fb, submenu_width, submenu_height, submenu, submenu_width, submenu_height);

    // generate RGBA_8888 from indexed color
    unsigned int count = TOUCHJOY_FB_WIDTH * TOUCHJOY_FB_HEIGHT;
    for (unsigned int i=0, j=0; i<count; i++, j+=4) {
        uint8_t index = *(fb + i);
        uint32_t rgb = (((uint32_t)(colormap[index].red)   << 0 ) |
                        ((uint32_t)(colormap[index].green) << 8 ) |
                        ((uint32_t)(colormap[index].blue)  << 16));
        if (rgb == 0) {
            // make black transparent for joystick
        } else {
            rgb |=      ((uint32_t)0xff                    << 24);
        }
        *( (uint32_t*)(pixels + j) ) = rgb;
    }
}

static void _setup_axis_object(void) {
    axisTemplate[1][2] = upChar;
    axisTemplate[2][1] = leftChar;
    axisTemplate[2][3] = rightChar;
    axisTemplate[3][2] = downChar;
    _setup_object(axisTemplate[0], touchAxisObjFB, touchAxisObjPixels);
    axisTextureDirty = true;
}

static void _setup_button_object(void) {
    buttonTemplate[1][3] = button1Char;
    buttonTemplate[3][1] = button0Char;
    _setup_object(buttonTemplate[0], buttonObjFB, buttonObjPixels);
    buttonTextureDirty = true;
}

static void _model_to_screen(float screenCoords[4], demoModel *model) {

    float x0 = 1.0;
    float y0 = 1.0;
    float x1 = -1.0;
    float y1 = -1.0;

#if PERSPECTIVE
#error TODO FIXME we really should do it this way regardless of whether we are using PERSPECTIVE ...
    // project models to screen space for touch handling
    GLfloat mvp[16];
#else
#warning TODO FIXME ... should really use matrix calculations (assuming identity/orthographic for now)
    GLfloat *positions = (GLfloat *)(model->positions);
    unsigned int stride = model->positionSize;
    unsigned int len = model->positionArraySize/_get_gl_type_size(model->positionType);
    for (unsigned int i=0; i < len; i += stride) {
        float x = (positions[i] + 1.f) / 2.f;
        if (x < x0) {
            x0 = x;
        }
        if (x > x1) {
            x1 = x;
        }
        float y = (positions[i+1] + 1.f) / 2.f;
        LOG("\tmodel x:%f, y:%f", x, y);
        if (y < y0) {
            y0 = y;
        }
        if (y > y1) {
            y1 = y;
        }
    }
#endif

    // OpenGL screen origin is bottom-left (Android is top-left)
    float yFlip0 = viewportHeight - (y1 * viewportHeight);
    float yFlip1 = viewportHeight - (y0 * viewportHeight);

    screenCoords[0] = x0 * viewportWidth;
    screenCoords[1] = yFlip0;
    screenCoords[2] = x1 * viewportWidth;
    screenCoords[3] = yFlip1;
}

static void gltouchjoy_init(void) {
    LOG("gltouchjoy_init ...");

    mdlDestroyModel(touchAxisObjModel);
    mdlDestroyModel(buttonObjModel);

    touchAxisObjModel = _create_model(-1.05, -1.0);
    touchAxisObjVAOName = UNINITIALIZED_GL;
    touchAxisObjPosBufferName = UNINITIALIZED_GL;
    touchAxisObjTexcoordBufferName = UNINITIALIZED_GL;
    touchAxisObjElementBufferName = UNINITIALIZED_GL;
    _create_VAO_VBOs(touchAxisObjModel, &touchAxisObjVAOName, &touchAxisObjPosBufferName, &touchAxisObjTexcoordBufferName, &touchAxisObjElementBufferName);
    if (touchAxisObjPosBufferName == UNINITIALIZED_GL || touchAxisObjTexcoordBufferName == UNINITIALIZED_GL || touchAxisObjElementBufferName == UNINITIALIZED_GL)
    {
        LOG("gltouchjoy not initializing axis");
        return;
    }

    touchAxisObjTextureName = _create_texture(touchAxisObjPixels);
    if (touchAxisObjTextureName == UNINITIALIZED_GL) {
        LOG("gltouchjoy not initializing axis: texture error");
        return;
    }
    _setup_axis_object();

    float screenCoords[4] = { 0 };

    _model_to_screen(screenCoords, touchAxisObjModel);
    touchAxisObjModelScreenX = (int)screenCoords[0];
    touchAxisObjModelScreenY = (int)screenCoords[1];
    touchAxisObjModelScreenXMax = (int)screenCoords[2];
    touchAxisObjModelScreenYMax = (int)screenCoords[3];
    LOG("axis screen coords: [%d,%d] -> [%d,%d]", touchAxisObjModelScreenX, touchAxisObjModelScreenY, touchAxisObjModelScreenXMax, touchAxisObjModelScreenYMax);

    // button object

    buttonObjModel = _create_model(1.05-model_width, -1.0);
    buttonObjVAOName = UNINITIALIZED_GL;
    buttonObjPosBufferName = UNINITIALIZED_GL;
    buttonObjTexcoordBufferName = UNINITIALIZED_GL;
    buttonObjElementBufferName = UNINITIALIZED_GL;
    _create_VAO_VBOs(buttonObjModel, &buttonObjVAOName, &buttonObjPosBufferName, &buttonObjTexcoordBufferName, &buttonObjElementBufferName);
    if (buttonObjPosBufferName == UNINITIALIZED_GL || buttonObjTexcoordBufferName == UNINITIALIZED_GL || buttonObjElementBufferName == UNINITIALIZED_GL)
    {
        LOG("gltouchjoy not initializing buttons");
        return;
    }

    buttonObjTextureName = _create_texture(buttonObjPixels);
    if (buttonObjTextureName == UNINITIALIZED_GL) {
        LOG("not initializing buttons: texture error");
        return;
    }
    _setup_button_object();

    // NOTE : button model is a composite of both button 0 and button 1 (with ability to press both with one touch)

    _model_to_screen(screenCoords, buttonObjModel);

    int button0W = ((int)screenCoords[2] - (int)screenCoords[0]) >>1;
    buttonObj0ScreenX = (int)screenCoords[0]+button0W;
    buttonObj0ScreenY = (int)screenCoords[1];
    buttonObj0ScreenXMax = (int)screenCoords[2];
    buttonObj0ScreenYMax = (int)screenCoords[3];
    LOG("button 0 screen coords: [%d,%d] -> [%d,%d]", buttonObj0ScreenX, buttonObj0ScreenY, buttonObj0ScreenXMax, buttonObj0ScreenYMax);

    int button1H = ((int)screenCoords[3] - (int)screenCoords[1]) >>1;
    buttonObj1ScreenX = (int)screenCoords[0];
    buttonObj1ScreenY = (int)screenCoords[1]+button1H;
    buttonObj1ScreenXMax = (int)screenCoords[2];
    buttonObj1ScreenYMax = (int)screenCoords[3];
    LOG("button 1 screen coords: [%d,%d] -> [%d,%d]", buttonObj1ScreenX, buttonObj1ScreenY, buttonObj1ScreenXMax, buttonObj1ScreenYMax);

    isAvailable = true;
}

static void gltouchjoy_destroy(void) {
    LOG("gltouchjoy_destroy ...");
    if (!isAvailable) {
        return;
    }

    isAvailable = false;

    glDeleteTextures(1, &touchAxisObjTextureName);
    touchAxisObjTextureName = UNINITIALIZED_GL;
    _destroy_VAO_VBOs(touchAxisObjVAOName, touchAxisObjPosBufferName, touchAxisObjTexcoordBufferName, touchAxisObjElementBufferName);
    touchAxisObjVAOName = UNINITIALIZED_GL;
    touchAxisObjPosBufferName = UNINITIALIZED_GL;
    touchAxisObjTexcoordBufferName = UNINITIALIZED_GL;
    touchAxisObjElementBufferName = UNINITIALIZED_GL;
    mdlDestroyModel(touchAxisObjModel);
    touchAxisObjModel = NULL;

    glDeleteTextures(1, &buttonObjTextureName);
    buttonObjTextureName = UNINITIALIZED_GL;
    _destroy_VAO_VBOs(buttonObjVAOName, buttonObjPosBufferName, buttonObjTexcoordBufferName, buttonObjElementBufferName);
    buttonObjVAOName = UNINITIALIZED_GL;
    buttonObjPosBufferName = UNINITIALIZED_GL;
    buttonObjTexcoordBufferName = UNINITIALIZED_GL;
    buttonObjElementBufferName = UNINITIALIZED_GL;
    mdlDestroyModel(buttonObjModel);
    buttonObjModel = NULL;
}

static void _render_object(demoModel *model, GLuint vaoName, GLuint posBufferName, GLuint texcoordBufferName, GLuint elementBufferName) {

    // Bind our vertex array object
#if USE_VAO
    glBindVertexArray(vaoName);
#else
    glBindBuffer(GL_ARRAY_BUFFER, posBufferName);

    GLsizei posTypeSize      = _get_gl_type_size(model->positionType);
    GLsizei texcoordTypeSize = _get_gl_type_size(model->texcoordType);

    // Set up parmeters for position attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX,                                   // What attibute index will this array feed in the vertex shader (see buildProgram)
                          model->positionSize,                 // How many elements are there per position?
                          model->positionType,                 // What is the type of this data?
                          GL_FALSE,                                         // Do we want to normalize this data (0-1 range for fixed-pont types)
                          model->positionSize*posTypeSize,     // What is the stride (i.e. bytes between positions)?
                          0);                                               // What is the offset in the VBO to the position data?
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Set up parmeters for texcoord attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the texcoord VBO to VAO
    glBindBuffer(GL_ARRAY_BUFFER, texcoordBufferName);
    glVertexAttribPointer(TEXCOORD_ATTRIB_IDX,                              // What attibute index will this array feed in the vertex shader (see buildProgram)
                          model->texcoordSize,                 // How many elements are there per texture coord?
                          model->texcoordType,                 // What is the type of this data in the array?
                          GL_TRUE,                                          // Do we want to normalize this data (0-1 range for fixed-point types)
                          model->texcoordSize*texcoordTypeSize,// What is the stride (i.e. bytes between texcoords)?
                          0);                                               // What is the offset in the VBO to the texcoord data?
    glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferName);
#endif

    glUniform1f(alphaValue, 1.0);

    // Draw the object
    glDrawElements(GL_TRIANGLES, model->numElements, model->elementType, 0);
    GL_ERRLOG("gltouchjoy render");
}

static void gltouchjoy_render(void) {
    if (!isAvailable) {
        return;
    }
    if (!isEnabled) {
        return;
    }
    if (!isVisible) {
        return;
    }

    glViewport(0, 0, viewportWidth, viewportHeight);

    glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_AXIS);
    glBindTexture(GL_TEXTURE_2D, touchAxisObjTextureName);
    if (axisTextureDirty) {
        axisTextureDirty = false;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, TOUCHJOY_FB_WIDTH, TOUCHJOY_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)touchAxisObjPixels);
    }
    glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHJOY_AXIS);
    _render_object(touchAxisObjModel, touchAxisObjVAOName, touchAxisObjPosBufferName, touchAxisObjTexcoordBufferName, touchAxisObjElementBufferName);

    glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_BUTTON);
    glBindTexture(GL_TEXTURE_2D, buttonObjTextureName);
    if (buttonTextureDirty) {
        buttonTextureDirty = false;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, TOUCHJOY_FB_WIDTH, TOUCHJOY_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)buttonObjPixels);
    }
    glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHJOY_BUTTON);
    _render_object(buttonObjModel, buttonObjVAOName, buttonObjPosBufferName, buttonObjTexcoordBufferName, buttonObjElementBufferName);
}

static void gltouchjoy_reshape(int w, int h) {
    LOG("gltouchjoy_reshape(%d, %d)", w, h);

    if (w > viewportWidth) {
        viewportWidth = w;
    }
    if (h > viewportHeight) {
        viewportHeight = h;
    }
}

static void gltouchjoy_resetJoystick(void) {
    // no-op
}

static inline bool _is_point_on_axis(float x, float y) {
    return (x >= touchAxisObjModelScreenX && x <= touchAxisObjModelScreenXMax && y >= touchAxisObjModelScreenY && y <= touchAxisObjModelScreenYMax);
}

static inline bool _is_point_on_button0(float x, float y) {
    return (x >= buttonObj0ScreenX && x <= buttonObj0ScreenXMax && y >= buttonObj0ScreenY && y <= buttonObj0ScreenYMax);
}

static inline bool _is_point_on_button1(float x, float y) {
    return (x >= buttonObj1ScreenX && x <= buttonObj1ScreenXMax && y >= buttonObj1ScreenY && y <= buttonObj1ScreenYMax);
}

static inline void _move_joystick_axis(float x, float y) {

    int buttonW = touchAxisObjModelScreenXMax - touchAxisObjModelScreenX;
    float halfJoyX = buttonW/2.f;
    int buttonH = touchAxisObjModelScreenYMax - touchAxisObjModelScreenY;
    float halfJoyY = buttonH/2.f;

    float x_mod = 256.f/buttonW;
    float y_mod = 256.f/buttonH;

    float x0 = (x - touchAxisObjModelScreenX) * x_mod;
    float y0 = (y - touchAxisObjModelScreenY) * y_mod;

    joy_x = (uint16_t)x0;
    if (joy_x > 0xff) {
        joy_x = 0xff;
    }
    joy_y = (uint16_t)y0;
    if (joy_y > 0xff) {
        joy_y = 0xff;
    }

    LOG("\tjoystick : (%d,%d)", joy_x, joy_y);
}

static bool gltouchjoy_onTouchEvent(joystick_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {

    if (!isAvailable) {
        return false;
    }
    if (!isEnabled) {
        return false;
    }

    bool axisConsumed = false;
    bool buttonConsumed = false;

    float x = x_coords[pointer_idx];
    float y = y_coords[pointer_idx];

    switch (action) {
        case TOUCH_DOWN:
            LOG("---TOUCH DOWN");
        case TOUCH_POINTER_DOWN:
            if (action == TOUCH_POINTER_DOWN) {
                LOG("---TOUCH POINTER DOWN");
            }
            if (_is_point_on_axis(x, y)) {
                if (trackingAxisIndex >= 0) {
                    // already tracking a different pointer for the axis ...
                    axisConsumed = true;
                    trackingAxisIndex = pointer_idx;
                    LOG("\taxis event : saw %d but already tracking %d", pointer_idx, trackingAxisIndex);
                    _move_joystick_axis(x, y);
                } else {
                    axisConsumed = true;
                    trackingAxisIndex = pointer_idx;
                    LOG("\taxis event : begin tracking %d", trackingAxisIndex);
                    _move_joystick_axis(x, y);
                }
            } else {
                bool isOn0 = _is_point_on_button0(x, y);
                bool isOn1 = _is_point_on_button1(x, y);
                if (isOn0 && isOn1) {
                    trackingButton0Index = TOUCHED_NONE;
                    trackingButton1Index = TOUCHED_NONE;
                    trackingButtonBothIndex = pointer_idx;
                    buttonConsumed = true;
                    joy_button0 = 0x80;
                    joy_button1 = 0x80;
                    LOG("\tbutton0&1 event : index:%d joy_button0:%02X joy_button1:%02X", pointer_idx, joy_button0, joy_button1);
                } else if (isOn0) {
                    trackingButton0Index = pointer_idx;
                    trackingButton1Index = TOUCHED_NONE;
                    trackingButtonBothIndex = TOUCHED_NONE;
                    buttonConsumed = true;
                    joy_button0 = 0x80;
                    joy_button1 = 0x0;
                    LOG("\tbutton0 event : index:%d joy_button0:%02X", pointer_idx, joy_button0);
                } else if (isOn1) {
                    trackingButton0Index = TOUCHED_NONE;
                    trackingButton1Index = pointer_idx;
                    trackingButtonBothIndex = TOUCHED_NONE;
                    buttonConsumed = true;
                    joy_button0 = 0x0;
                    joy_button1 = 0x80;
                    LOG("\tbutton1 event : index:%d joy_button1:%02X", pointer_idx, joy_button1);
                }
            }
            break;

        case TOUCH_MOVE:
            LOG("---TOUCH MOVE");
            if (trackingAxisIndex >= 0) {
                axisConsumed = true;
                x = x_coords[trackingAxisIndex];
                y = y_coords[trackingAxisIndex];
                LOG("\t...tracking axis:%d (count:%d)", trackingAxisIndex, pointer_count);
                _move_joystick_axis(x, y);
            }
            break;

        case TOUCH_UP:
            LOG("---TOUCH UP");
        case TOUCH_POINTER_UP:
            if (action == TOUCH_POINTER_UP) {
                LOG("---TOUCH POINTER UP");
            }
            if (pointer_idx == trackingAxisIndex) {
                axisConsumed = true;
                trackingAxisIndex = TOUCHED_NONE;
                joy_x = HALF_JOY_RANGE;
                joy_y = HALF_JOY_RANGE;
                LOG("\taxis went up");
            } else if (pointer_idx == trackingButtonBothIndex) {
                buttonConsumed = true;
                trackingButtonBothIndex = TOUCHED_NONE;
                joy_button0 = 0x0;
                joy_button1 = 0x0;
                LOG("\tbuttons up joy_button0:%02X joy_button1:%02X", joy_button0, joy_button1);
            } else if (pointer_idx == trackingButton0Index) {
                buttonConsumed = true;
                trackingButton0Index = TOUCHED_NONE;
                joy_button0 = 0x0;
                LOG("\tbutton0 up joy_button0:%02X", joy_button0);
            } else if (pointer_idx == trackingButton1Index) {
                buttonConsumed = true;
                trackingButton1Index = TOUCHED_NONE;
                joy_button1 = 0x0;
                LOG("\tbutton1 up joy_button1:%02X", joy_button1);
            }
            break;

        case TOUCH_CANCEL:
            LOG("---TOUCH CANCEL");
            trackingAxisIndex = TOUCHED_NONE;
            trackingButton0Index = TOUCHED_NONE;
            trackingButton1Index = TOUCHED_NONE;
            trackingButtonBothIndex = TOUCHED_NONE;
            joy_button0 = 0x0;
            joy_button1 = 0x0;
            joy_x = HALF_JOY_RANGE;
            joy_y = HALF_JOY_RANGE;
            break;

        default:
            LOG("gltouchjoy saw unknown touch event : %d", action);
            break;
    }

    if (axisConsumed || buttonConsumed) {
        LOG("\tconsumed event");
        return true;
    } else {
        LOG("\tDID NOT consume...");
        return false;
    }
}

static bool gltouchjoy_isTouchJoystickAvailable(void) {
    return isAvailable;
}

static void gltouchjoy_setTouchJoyEnabled(bool enabled) {
    isEnabled = enabled;
}

void gltouchjoy_setTouchButtonValues(char button0Val, char button1Val) {
    button0Char = button0Val;
    button1Char = button1Val;
    _setup_button_object();
}

void gltouchjoy_setTouchAxisType(touchjoy_axis_type_t axisType) {
    touchjoy_axisType = axisType;
    _setup_axis_object();
}

void gltouchjoy_setTouchAxisValues(char up, char left, char right, char down) {
    upChar    = up;
    leftChar  = left;
    rightChar = right;
    downChar  = down;
    if (touchjoy_axisType == AXIS_EMULATED_KEYBOARD) {
        _setup_axis_object();
    }
}

__attribute__((constructor))
static void _init_gltouchjoy(void) {
    LOG("Registering OpenGL software touch joystick");

    joydriver_onTouchEvent  = &gltouchjoy_onTouchEvent;
    joydriver_isTouchJoystickAvailable = &gltouchjoy_isTouchJoystickAvailable;
    joydriver_setTouchJoyEnabled = &gltouchjoy_setTouchJoyEnabled;
    joydriver_setTouchButtonValues = &gltouchjoy_setTouchButtonValues;
    joydriver_setTouchAxisType = &gltouchjoy_setTouchAxisType;
    joydriver_setTouchAxisValues = &gltouchjoy_setTouchAxisValues;

    touchjoyAnimation.ctor = &gltouchjoy_init;
    touchjoyAnimation.dtor = &gltouchjoy_destroy;
    touchjoyAnimation.render = &gltouchjoy_render;
    touchjoyAnimation.reshape = &gltouchjoy_reshape;
    gldriver_register_animation(&touchjoyAnimation);
}

void gldriver_joystick_reset(void) {
#warning FIXME
#warning TODO
#warning expunge
#warning this
#warning API
#warning ...
}
