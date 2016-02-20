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

#include "video/gltouchjoy.h"

#if !INTERFACE_TOUCH
#error this is a touch interface module, possibly you mean to not compile this at all?
#endif

#define MODEL_DEPTH -1/32.f

#define AXIS_TEMPLATE_COLS 3
#define AXIS_TEMPLATE_ROWS 3

#define BUTTON_TEMPLATE_COLS 1
#define BUTTON_TEMPLATE_ROWS 1

#define AXIS_FB_WIDTH (AXIS_TEMPLATE_COLS * FONT80_WIDTH_PIXELS)
#define AXIS_FB_HEIGHT (AXIS_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define BUTTON_FB_WIDTH (BUTTON_TEMPLATE_COLS * FONT80_WIDTH_PIXELS)
#define BUTTON_FB_HEIGHT (BUTTON_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define AXIS_OBJ_W        0.15
#define AXIS_OBJ_H        0.2
#define AXIS_OBJ_HALF_W   (AXIS_OBJ_W/2.f)
#define AXIS_OBJ_HALF_H   (AXIS_OBJ_H/2.f)

#define BUTTON_OBJ_W        0.075
#define BUTTON_OBJ_H        0.1
#define BUTTON_OBJ_HALF_W   (BUTTON_OBJ_W/2.f)
#define BUTTON_OBJ_HALF_H   (BUTTON_OBJ_H/2.f)

#define BUTTON_SWITCH_THRESHOLD_DEFAULT 22

GLTouchJoyGlobals joyglobals = { 0 };
GLTouchJoyAxes axes = { 0 };
GLTouchJoyButtons buttons = { 0 };

static struct {
    GLTouchJoyVariant *joys;
    GLTouchJoyVariant *kpad;
    GLTouchJoyVariant *curr;
} variant = { 0 };

// viewport touch
static struct {
    int width;
    int height;

    // Axis box
    int axisX;
    int axisXMax;
    int axisY;
    int axisYMax;

    // Button box
    int buttonX;
    int buttonXMax;
    int buttonY;
    int buttonYMax;

    // Are we in landscape mode
    bool landscape;

    // TODO FIXME : support 2-players!
} touchport = { 0 };

#define AZIMUTH_CLASS(CLS, ...) \
    MODEL_CLASS(CLS, \
        GLuint vertShader; \
        GLuint fragShader; \
        GLuint program; \
        GLint uniformMVPIdx;);

AZIMUTH_CLASS(GLModelJoystickAzimuth);

// ----------------------------------------------------------------------------
// joystick azimuth model

static void _azimuth_destroy_model(GLModel *parent) {

    GLModelJoystickAzimuth *azimuthJoystick = (GLModelJoystickAzimuth *)parent->custom;
    if (!azimuthJoystick) {
        return;
    }

    // detach and delete the Azimuth shaders
    // 2015/11/06 NOTE : Tegra 2 for mobile has a bug whereby you cannot detach/delete shaders immediately after
    // creating the program.  So we delete them during the shutdown sequence instead.
    // https://code.google.com/p/android/issues/detail?id=61832

    if (azimuthJoystick->program != UNINITIALIZED_GL) {
        glDetachShader(azimuthJoystick->program, azimuthJoystick->vertShader);
        glDetachShader(azimuthJoystick->program, azimuthJoystick->fragShader);
        glDeleteShader(azimuthJoystick->vertShader);
        glDeleteShader(azimuthJoystick->fragShader);

        azimuthJoystick->vertShader = UNINITIALIZED_GL;
        azimuthJoystick->fragShader = UNINITIALIZED_GL;

        glDeleteProgram(azimuthJoystick->program);
        azimuthJoystick->program = UNINITIALIZED_GL;
    }

    FREE(parent->custom);
}

static void *_azimuth_create_model(GLModel *parent) {

    parent->custom = CALLOC(sizeof(GLModelJoystickAzimuth), 1);
    GLModelJoystickAzimuth *azimuthJoystick = (GLModelJoystickAzimuth *)parent->custom;

    if (!azimuthJoystick) {
        return NULL;
    }

    // degenerate the quad model into just a single line model ... (should not need to adjust allocated memory size
    // since we should be using less than what was originally allocated)
    parent->primType = GL_LINES;
    parent->numVertices = 2;
    GLsizei posTypeSize = getGLTypeSize(parent->positionType);
    parent->positionArraySize = (parent->positionSize * posTypeSize * parent->numVertices);
    parent->numElements = 2;
    GLsizei eltTypeSize = getGLTypeSize(parent->elementType);
    parent->elementArraySize = (eltTypeSize * parent->numElements);

    azimuthJoystick->vertShader = UNINITIALIZED_GL;
    azimuthJoystick->fragShader = UNINITIALIZED_GL;
    azimuthJoystick->program = UNINITIALIZED_GL;
    azimuthJoystick->uniformMVPIdx = UNINITIALIZED_GL;

    bool err = true;
    demoSource *vtxSource = NULL;
    demoSource *frgSource = NULL;
    do {
        // load/setup specific shaders

        vtxSource = glshader_createSource("SolidColor.vsh");
        if (!vtxSource) {
            ERRLOG("Cannot compile vertex shader for joystick azimuth!");
            break;
        }

        frgSource = glshader_createSource("SolidColor.fsh");
        if (!frgSource) {
            ERRLOG("Cannot compile fragment shader for joystick azimuth!");
            break;
        }

        // Build/use Program
        azimuthJoystick->program = glshader_buildProgram(vtxSource, frgSource, /*withTexcoord:*/false, &azimuthJoystick->vertShader, &azimuthJoystick->fragShader);

        azimuthJoystick->uniformMVPIdx = glGetUniformLocation(azimuthJoystick->program, "modelViewProjectionMatrix");
        if (azimuthJoystick->uniformMVPIdx < 0) {
            LOG("OOPS, no modelViewProjectionMatrix in Azimuth shader : %d", azimuthJoystick->uniformMVPIdx);
            break;
        }

        err = false;
    } while (0);

    GL_ERRLOG("build Aziumth joystick");

    if (vtxSource) {
        glshader_destroySource(vtxSource);
    }
    if (frgSource) {
        glshader_destroySource(frgSource);
    }

    if (err) {
        _azimuth_destroy_model(parent);
        azimuthJoystick = NULL;
    }

    return azimuthJoystick;
}

static void _azimuth_render(void) {
    if (!axes.azimuthModel) {
        return;
    }

    GLModelJoystickAzimuth *azimuthJoystick = (GLModelJoystickAzimuth *)axes.azimuthModel->custom;

    // use azimuth (SolidColor) program
    glUseProgram(azimuthJoystick->program);

    glUniformMatrix4fv(azimuthJoystick->uniformMVPIdx, 1, GL_FALSE, mvpIdentity);

    // NOTE : assuming we should just upload new postion data every time ...
    glBindBuffer(GL_ARRAY_BUFFER, axes.azimuthModel->posBufferName);
    glBufferData(GL_ARRAY_BUFFER, axes.azimuthModel->positionArraySize, axes.azimuthModel->positions, GL_DYNAMIC_DRAW);

    // Bind our vertex array object
#if USE_VAO
    glBindVertexArray(axes.azimuthModel->vaoName);
#else
    glBindBuffer(GL_ARRAY_BUFFER, axes.azimuthModel->posBufferName);

    GLsizei posTypeSize = getGLTypeSize(axes.azimuthModel->positionType);

    // Set up parmeters for position attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX, // What attibute index will this array feed in the vertex shader (see buildProgram)
                          axes.azimuthModel->positionSize, // How many elements are there per position?
                          axes.azimuthModel->positionType, // What is the type of this data?
                          GL_FALSE, // Do we want to normalize this data (0-1 range for fixed-pont types)
                          axes.azimuthModel->positionSize*posTypeSize, // What is the stride (i.e. bytes between positions)?
                          0); // What is the offset in the VBO to the position data?
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, axes.azimuthModel->elementBufferName);
#endif

    // draw it
    glDrawElements(axes.azimuthModel->primType, axes.azimuthModel->numElements, axes.azimuthModel->elementType, 0);

    // back to main framebuffer/quad program
    glUseProgram(mainShaderProgram);

    GL_ERRLOG("Azimuth render");
}

// ----------------------------------------------------------------------------

static void _setup_axis_hud(GLModel *parent) {
    if (UNLIKELY(!parent)) {
        LOG("gltouchjoy WARN : cannot setup axis object without parent");
        return;
    }

    if (!joyglobals.showControls) {
        return;
    }

    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;

    if (hudElement->tpl == NULL) {
        // deferred construction ...
        const char axisTemplate[AXIS_TEMPLATE_ROWS][AXIS_TEMPLATE_COLS+1] = {
            " @ ",
            "@+@",
            " @ ",
        };

        const unsigned int size = sizeof(axisTemplate);
        hudElement->tpl = CALLOC(size, 1);
        hudElement->tplWidth = AXIS_TEMPLATE_COLS;
        hudElement->tplHeight = AXIS_TEMPLATE_ROWS;
        memcpy(hudElement->tpl, axisTemplate, size);

        hudElement->pixels = CALLOC(AXIS_FB_WIDTH * AXIS_FB_HEIGHT, 1);
        hudElement->pixWidth = AXIS_FB_WIDTH;
        hudElement->pixHeight = AXIS_FB_HEIGHT;
    }

    const unsigned int row = (AXIS_TEMPLATE_COLS+1);

    for (unsigned int i=0; i<ROSETTE_ROWS; i++) {
        for (unsigned int j=0; j<ROSETTE_COLS; j++) {
            ((hudElement->tpl)+(row*i))[j] = axes.rosetteChars[(i*ROSETTE_ROWS)+j];
        }
    }

    glhud_setupDefault(parent);
}

static void *_create_axis_hud(GLModel *parent) {
    parent->custom = glhud_createDefault();
    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;
    if (hudElement) {
        hudElement->blackIsTransparent = true;
        _setup_axis_hud(parent);
    }
    return hudElement;
}

static void _setup_button_hud(GLModel *parent) {
    if (UNLIKELY(!parent)) {
        LOG("gltouchjoy WARN : cannot setup button object without parent");
        return;
    }

    if (!joyglobals.showControls) {
        return;
    }

    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;

    if (hudElement->tpl == NULL) {
        // deferred construction ...
        const char buttonTemplate[BUTTON_TEMPLATE_ROWS][BUTTON_TEMPLATE_COLS+1] = {
            "@",
        };

        const unsigned int size = sizeof(buttonTemplate);
        hudElement->tpl = CALLOC(size, 1);
        hudElement->tplWidth = BUTTON_TEMPLATE_COLS;
        hudElement->tplHeight = BUTTON_TEMPLATE_ROWS;
        memcpy(hudElement->tpl, buttonTemplate, size);

        hudElement->pixels = CALLOC(BUTTON_FB_WIDTH * BUTTON_FB_HEIGHT, 1);
        hudElement->pixWidth = BUTTON_FB_WIDTH;
        hudElement->pixHeight = BUTTON_FB_HEIGHT;
    }

    const unsigned int row = (BUTTON_TEMPLATE_COLS+1);
    ((hudElement->tpl)+(row*0))[0] = buttons.activeChar;

    glhud_setupDefault(parent);
}

static void *_create_button_hud(GLModel *parent) {
    parent->custom = glhud_createDefault();
    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;
    if (hudElement) {
        hudElement->blackIsTransparent = true;
        _setup_button_hud(parent);
    }
    return hudElement;
}

static inline void _setup_button_object_with_char(char newChar) {
    if (buttons.activeChar != newChar) {
        buttons.activeChar = newChar;
        _setup_button_hud(buttons.model);
    }
}

// ----------------------------------------------------------------------------

static inline void resetState() {
    LOG("...");
    axes.trackingIndex = TRACKING_NONE;
    buttons.trackingIndex = TRACKING_NONE;
    variant.joys->resetState();
    variant.kpad->resetState();
}

static void gltouchjoy_setup(void) {
    LOG("...");

    resetState();

    mdlDestroyModel(&axes.model);
    mdlDestroyModel(&axes.azimuthModel);
    mdlDestroyModel(&buttons.model);

    joyglobals.isShuttingDown = false;

    // axis origin object

    axes.model = mdlCreateQuad((GLModelParams_s){
            .skew_x = -1.05,
            .skew_y = -1.0,
            .z = MODEL_DEPTH,
            .obj_w = AXIS_OBJ_W,
            .obj_h = AXIS_OBJ_H,
            .positionUsageHint = GL_DYNAMIC_DRAW, // positions can change
            .tex_w = AXIS_FB_WIDTH,
            .tex_h = AXIS_FB_HEIGHT,
            .texcoordUsageHint = GL_DYNAMIC_DRAW, // so can texture
        }, (GLCustom){
            .create = &_create_axis_hud,
            .destroy = &glhud_destroyDefault,
        });
    if (!axes.model) {
        LOG("gltouchjoy not initializing axis");
        return;
    }
    if (!axes.model->custom) {
        LOG("gltouchjoy axes initialization problem");
        return;
    }

    // axis azimuth object

    bool azimuthError = true;
    do {
        axes.azimuthModel = mdlCreateQuad((GLModelParams_s){
                .skew_x = -1.05,
                .skew_y = -1.0,
                .z = MODEL_DEPTH,
                .obj_w = AXIS_OBJ_W,
                .obj_h = AXIS_OBJ_H,
                .positionUsageHint = GL_DYNAMIC_DRAW, // positions can change
                .tex_w = 0,
                .tex_h = 0,
                .texcoordUsageHint = UNINITIALIZED_GL, // no texture data
            }, (GLCustom){
                .create = &_azimuth_create_model,
                .destroy = &_azimuth_destroy_model,
            });
        if (!axes.azimuthModel) {
            LOG("gltouchjoy azimuth model initialization problem");
            break;
        }
        if (!axes.azimuthModel->custom) {
            LOG("gltouchjoy azimuth custom model initialization problem");
            break;
        }

        azimuthError = false;
    } while (0);

    if (azimuthError) {
        mdlDestroyModel(&axes.azimuthModel);
    }

    // button object

    buttons.model = mdlCreateQuad((GLModelParams_s){
            .skew_x = 1.05-BUTTON_OBJ_W,
            .skew_y = -1.0,
            .z = MODEL_DEPTH,
            .obj_w = BUTTON_OBJ_W,
            .obj_h = BUTTON_OBJ_H,
            .positionUsageHint = GL_DYNAMIC_DRAW, // positions can change
            .tex_w = BUTTON_FB_WIDTH,
            .tex_h = BUTTON_FB_HEIGHT,
            .texcoordUsageHint = GL_DYNAMIC_DRAW, // so can texture
        }, (GLCustom){
            .create = &_create_button_hud,
            .destroy = &glhud_destroyDefault,
        });
    if (!buttons.model) {
        LOG("gltouchjoy not initializing buttons");
        return;
    }
    if (!buttons.model->custom) {
        LOG("gltouchjoy buttons initialization problem");
        return;
    }

    variant.joys->setup(&_setup_button_object_with_char);
    variant.kpad->setup(&_setup_button_object_with_char);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    axes.timingBegin = now;
    buttons.timingBegin = now;

    joyglobals.isAvailable = true;
}

static void gltouchjoy_shutdown(void) {
    LOG("gltouchjoy_shutdown ...");
    if (!joyglobals.isAvailable) {
        return;
    }

    resetState();

    joyglobals.isAvailable = false;
    joyglobals.isShuttingDown = true;

    variant.joys->shutdown();
    variant.kpad->shutdown();

    mdlDestroyModel(&axes.model);
    mdlDestroyModel(&axes.azimuthModel);
    mdlDestroyModel(&buttons.model);
}

static void gltouchjoy_render(void) {
    if (!joyglobals.isAvailable) {
        return;
    }
    if (!joyglobals.isEnabled) {
        return;
    }
    if (!joyglobals.ownsScreen) {
        return;
    }
    if (!joyglobals.showControls) {
        return;
    }

    glViewport(0, 0, touchport.width, touchport.height); // NOTE : show these HUD elements beyond the A2 framebuffer dimensions

    // draw axis
    float alpha = 1.f;
    if (axes.trackingIndex == TRACKING_NONE) {
        alpha = glhud_getTimedVisibility(axes.timingBegin, joyglobals.minAlpha, 1.0);
        if (alpha < joyglobals.minAlpha) {
            alpha = joyglobals.minAlpha;
        }
    }
    if (alpha > 0.0) {
        glUniform1f(alphaValue, alpha);

        glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_AXIS);
        glBindTexture(GL_TEXTURE_2D, axes.model->textureName);
        if (axes.model->texDirty) {
            axes.model->texDirty = false;
            _HACKAROUND_GLTEXIMAGE2D_PRE(TEXTURE_ACTIVE_TOUCHJOY_AXIS, axes.model->textureName);
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, axes.model->texWidth, axes.model->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, axes.model->texPixels);
        }
        if (axes.modelDirty) {
            axes.modelDirty = false;
            glBindBuffer(GL_ARRAY_BUFFER, axes.model->posBufferName);
            glBufferData(GL_ARRAY_BUFFER, axes.model->positionArraySize, axes.model->positions, GL_DYNAMIC_DRAW);
        }
        glUniform1i(texSamplerLoc, TEXTURE_ID_TOUCHJOY_AXIS);
        glhud_renderDefault(axes.model);
    }

    if (joyglobals.showAzimuth && axes.trackingIndex != TRACKING_NONE) {
        _azimuth_render();
    }

    // draw button(s)

    alpha = 1.f;
    if (buttons.trackingIndex == TRACKING_NONE) {
        alpha = glhud_getTimedVisibility(buttons.timingBegin, joyglobals.minAlpha, 1.0);
        if (alpha < joyglobals.minAlpha) {
            alpha = joyglobals.minAlpha;
        }
    }
    if (alpha > 0.0) {
        glUniform1f(alphaValue, alpha);

        glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_BUTTON);
        glBindTexture(GL_TEXTURE_2D, buttons.model->textureName);
        if (buttons.model->texDirty) {
            buttons.model->texDirty = false;
            _HACKAROUND_GLTEXIMAGE2D_PRE(TEXTURE_ACTIVE_TOUCHJOY_BUTTON, buttons.model->textureName);
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, buttons.model->texWidth, buttons.model->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, buttons.model->texPixels);
        }
        if (buttons.modelDirty) {
            buttons.modelDirty = false;
            glBindBuffer(GL_ARRAY_BUFFER, buttons.model->posBufferName);
            glBufferData(GL_ARRAY_BUFFER, buttons.model->positionArraySize, buttons.model->positions, GL_DYNAMIC_DRAW);
        }
        glUniform1i(texSamplerLoc, TEXTURE_ID_TOUCHJOY_BUTTON);
        glhud_renderDefault(buttons.model);
    }
}

static void gltouchjoy_reshape(int w, int h, bool landscape) {
    LOG("w:%d h:%d landscape:%d", w, h, landscape);

    touchport.landscape = landscape;
    swizzleDimensions(&w, &h, landscape);
    touchport.width = w;
    touchport.height = h;

    touchport.axisY = 0;
    touchport.axisYMax = h;
    touchport.buttonY = 0;
    touchport.buttonYMax = h;

    if (joyglobals.axisIsOnLeft) {
        touchport.axisX = 0;
        touchport.axisXMax = (w * joyglobals.screenDivider);
        touchport.buttonX = (w * joyglobals.screenDivider);
        touchport.buttonXMax = w;
    } else {
        touchport.buttonX = 0;
        touchport.buttonXMax = (w * joyglobals.screenDivider);
        touchport.axisX = (w * joyglobals.screenDivider);
        touchport.axisXMax = w;
    }
}

static void gltouchjoy_resetJoystick(void) {
    // no-op
}

static inline bool _is_point_on_axis_side(int x, int y) {
    return (x >= touchport.axisX && x <= touchport.axisXMax && y >= touchport.axisY && y <= touchport.axisYMax);
}

static inline void _reset_model_position(GLModel *model, float touchX, float touchY, float objHalfW, float objHalfH, GLModel *azimuthModel) {

    float centerX = 0.f;
    float centerY = 0.f;
    glhud_screenToModel(touchX, touchY, touchport.width, touchport.height, &centerX, &centerY);

    /* 2...3
     *  .
     *   .
     *    .
     * 0...1
     */

    GLfloat *quad = (GLfloat *)(model->positions);
    quad[0 +0] = centerX-objHalfW;
    quad[0 +1] = centerY-objHalfH;
    quad[4 +0] = centerX+objHalfW;
    quad[4 +1] = centerY-objHalfH;
    quad[8 +0] = centerX-objHalfW;
    quad[8 +1] = centerY+objHalfH;
    quad[12+0] = centerX+objHalfW;
    quad[12+1] = centerY+objHalfH;

    if (azimuthModel) {
        GLfloat *quadAzimuth = (GLfloat *)(azimuthModel->positions);
        quadAzimuth[0 +0] = centerX;
        quadAzimuth[0 +1] = centerY;
        quadAzimuth[4 +0] = centerX;
        quadAzimuth[4 +1] = centerY;
    }
}

static inline void _axis_touch_down(interface_touch_event_t action, int x, int y) {
    axes.centerX = x;
    axes.centerY = y;

    _reset_model_position(axes.model, x, y, AXIS_OBJ_HALF_W, AXIS_OBJ_HALF_H, axes.azimuthModel);
    axes.modelDirty = true;

    TOUCH_JOY_LOG("---TOUCH %sDOWN (axis index %d) center:(%d,%d) -> joy(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "), axes.trackingIndex, axes.centerX, axes.centerY, joy_x, joy_y);
    variant.curr->axisDown();
}

static inline void _button_touch_down(interface_touch_event_t action, int x, int y) {
    buttons.centerX = x;
    buttons.centerY = y;

    _reset_model_position(buttons.model, x, y, BUTTON_OBJ_HALF_W, BUTTON_OBJ_HALF_H, NULL);
    buttons.modelDirty = true;

    TOUCH_JOY_LOG("---TOUCH %sDOWN (buttons index %d) center:(%d,%d) -> buttons(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "), buttons.trackingIndex, buttons.centerX, buttons.centerY, joy_button0, joy_button1);
    variant.curr->buttonDown();
}

static inline void _axis_move(int x, int y) {

    if (joyglobals.showAzimuth && axes.azimuthModel) {
        float centerX = 0.f;
        float centerY = 0.f;
        glhud_screenToModel(x, y, touchport.width, touchport.height, &centerX, &centerY);
        GLfloat *quadAzimuth = (GLfloat *)axes.azimuthModel->positions;
        quadAzimuth[4 +0] = centerX;
        quadAzimuth[4 +1] = centerY;
    };

    x -= axes.centerX;
    y -= axes.centerY;
    TOUCH_JOY_LOG("---TOUCH MOVE ...tracking axis:%d (%d,%d) -> joy(0x%02X,0x%02X)", axes.trackingIndex, x, y, joy_x, joy_y);
    variant.curr->axisMove(x, y);
}

static inline void _button_move(int x, int y) {
    x -= buttons.centerX;
    y -= buttons.centerY;
    TOUCH_JOY_LOG("+++TOUCH MOVE ...tracking button:%d (%d,%d) -> buttons(0x%02X,0x%02X)", buttons.trackingIndex, x, y, joy_button0, joy_button1);
    variant.curr->buttonMove(x, y);
}

static inline void _axis_touch_up(interface_touch_event_t action, int x, int y) {
#if DEBUG_TOUCH_JOY
    bool resetIndex = false;
    if (buttons.trackingIndex > axes.trackingIndex) {
        // TODO FIXME : is resetting the pointer index just an Android-ism?
        resetIndex = true;
    }
    TOUCH_JOY_LOG("---TOUCH %sUP (axis went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset buttons index!)" : ""));
#endif
    x -= axes.centerX;
    y -= axes.centerY;
    if (buttons.trackingIndex > axes.trackingIndex) {
        TOUCH_JOY_LOG("!!! : DECREMENTING buttons.trackingIndex");
        --buttons.trackingIndex;
    }
    variant.curr->axisUp(x, y);
    axes.trackingIndex = TRACKING_NONE;
}

static inline void _button_touch_up(interface_touch_event_t action, int x, int y) {
#if DEBUG_TOUCH_JOY
    bool resetIndex = false;
    if (axes.trackingIndex > buttons.trackingIndex) {
        // TODO FIXME : is resetting the pointer index just an Android-ism?
        resetIndex = true;
    }
    TOUCH_JOY_LOG("---TOUCH %sUP (buttons went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset axis index!)" : ""));
#endif
    x -= buttons.centerX;
    y -= buttons.centerY;
    if (axes.trackingIndex > buttons.trackingIndex) {
        TOUCH_JOY_LOG("!!! : DECREMENTING axes.trackingIndex");
        --axes.trackingIndex;
    }
    variant.curr->buttonUp(x, y);
    buttons.trackingIndex = TRACKING_NONE;
}


static int64_t gltouchjoy_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {
    if (!joyglobals.isAvailable) {
        return 0x0LL;
    }
    if (!joyglobals.isEnabled) {
        return 0x0LL;
    }
    if (!joyglobals.ownsScreen) {
        return 0x0LL;
    }

    bool axisConsumed = false;
    bool buttonConsumed = false;

    switch (action) {
        case TOUCH_DOWN:
        case TOUCH_POINTER_DOWN:
            TOUCH_JOY_LOG("------DOWN:");
            {
                int x = (int)x_coords[pointer_idx];
                int y = (int)y_coords[pointer_idx];
                if (_is_point_on_axis_side(x, y)) {
                    if (pointer_idx == buttons.trackingIndex) {
                        TOUCH_JOY_LOG("!!! : INCREMENTING buttons.trackingIndex");
                        ++buttons.trackingIndex;
                    }
                    if (axes.trackingIndex != TRACKING_NONE) {
                        TOUCH_JOY_LOG("!!! : IGNORING OTHER AXIS TOUCH DOWN %d", pointer_idx);
                    } else {
                        axisConsumed = true;
                        axes.trackingIndex = pointer_idx;
                        _axis_touch_down(action, x, y);
                    }
                } else {
                    if (pointer_idx == axes.trackingIndex) {
                        TOUCH_JOY_LOG("!!! : INCREMENTING axes.trackingIndex");
                        ++axes.trackingIndex;
                    }
                    if (buttons.trackingIndex != TRACKING_NONE) {
                        TOUCH_JOY_LOG("!!! : IGNORING OTHER BUTTON TOUCH DOWN %d", pointer_idx);
                    } else {
                        buttonConsumed = true;
                        buttons.trackingIndex = pointer_idx;
                        _button_touch_down(action, x, y);
                    }
                }
            }
            break;

        case TOUCH_MOVE:
            TOUCH_JOY_LOG("------MOVE:");
            if (axes.trackingIndex >= 0) {
                axisConsumed = true;
                int x = (int)x_coords[axes.trackingIndex];
                int y = (int)y_coords[axes.trackingIndex];
                _axis_move(x, y);
            }
            if (buttons.trackingIndex >= 0) {
                buttonConsumed = true;
                int x = (int)x_coords[buttons.trackingIndex];
                int y = (int)y_coords[buttons.trackingIndex];
                _button_move(x, y);
            }
            break;

        case TOUCH_UP:
        case TOUCH_POINTER_UP:
            {
                TOUCH_JOY_LOG("------UP:");
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                if (pointer_idx == axes.trackingIndex) {
                    axes.timingBegin = now;
                    int x = (int)x_coords[axes.trackingIndex];
                    int y = (int)y_coords[axes.trackingIndex];
                    _axis_touch_up(action, x, y);
                } else if (pointer_idx == buttons.trackingIndex) {
                    buttons.timingBegin = now;
                    int x = (int)x_coords[buttons.trackingIndex];
                    int y = (int)y_coords[buttons.trackingIndex];
                    _button_touch_up(action, x, y);
                } else {
                    if (pointer_count == 1) {
                        TOUCH_JOY_LOG("!!! : RESETTING TOUCH JOYSTICK STATE MACHINE");
                        resetState();
                    } else {
                        TOUCH_JOY_LOG("!!! : IGNORING OTHER TOUCH UP %d", pointer_idx);
                    }
                }
            }
            break;

        case TOUCH_CANCEL:
            LOG("---TOUCH CANCEL");
            resetState();
            break;

        default:
            LOG("!!! UNKNOWN TOUCH EVENT : %d", action);
            break;
    }

    return TOUCH_FLAGS_HANDLED | TOUCH_FLAGS_JOY;
}

static bool gltouchjoy_isTouchJoystickAvailable(void) {
    return joyglobals.isAvailable;
}

static void gltouchjoy_setTouchJoystickEnabled(bool enabled) {
    joyglobals.isEnabled = enabled;
}

static void gltouchjoy_setTouchJoystickOwnsScreen(bool pwnd) {
    joyglobals.ownsScreen = pwnd;
    resetState();
    if (joyglobals.ownsScreen) {
        caps_lock = true; // HACK FOR NOW : force uppercase scancodes for touchjoy_kpad variant
        joyglobals.minAlpha = joyglobals.minAlphaWhenOwnsScreen;
    } else {
        joyglobals.minAlpha = 0.0;
    }
}

static bool gltouchjoy_ownsScreen(void) {
    return joyglobals.ownsScreen;
}

static void gltouchjoy_setShowControls(bool showControls) {
    joyglobals.showControls = showControls;
}

static void gltouchjoy_setShowAzimuth(bool showAzimuth) {
    joyglobals.showAzimuth = showAzimuth;
}

static void _animation_showTouchJoystick(void) {
    if (!joyglobals.isAvailable) {
        return;
    }

    int x = touchport.axisX + ((touchport.axisXMax - touchport.axisX)/2);
    int y = touchport.axisY + ((touchport.axisYMax - touchport.axisY)/2);
    _reset_model_position(axes.model, x, y, AXIS_OBJ_HALF_W, AXIS_OBJ_HALF_H, NULL);
    axes.modelDirty = true;

    x = touchport.buttonX + ((touchport.buttonXMax - touchport.buttonX)/2);
    y = touchport.buttonY + ((touchport.buttonYMax - touchport.buttonY)/2);
    _reset_model_position(buttons.model, x, y, BUTTON_OBJ_HALF_W, BUTTON_OBJ_HALF_H, NULL);
    buttons.modelDirty = true;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    axes.timingBegin = now;
    buttons.timingBegin = now;
}

static void _animation_hideTouchJoystick(void) {
    if (!joyglobals.isAvailable) {
        return;
    }
    axes.timingBegin = (struct timespec){ 0 };
    buttons.timingBegin = (struct timespec){ 0 };
}

static void gltouchjoy_setTouchButtonTypes(
        touchjoy_button_type_t touchDownChar, int touchDownScancode,
        touchjoy_button_type_t northChar, int northScancode,
        touchjoy_button_type_t southChar, int southScancode)
{
    buttons.touchDownChar     = touchDownChar;
    buttons.touchDownScancode = touchDownScancode;

    buttons.southChar     = southChar;
    buttons.southScancode = southScancode;

    buttons.northChar     = northChar;
    buttons.northScancode = northScancode;

    buttons.activeChar = ICONTEXT_NONACTIONABLE;

    char currButtonDisplayChar = touchDownChar;
    if (touchDownChar == TOUCH_BUTTON0) {
        currButtonDisplayChar = MOUSETEXT_OPENAPPLE;
    } else if (touchDownChar == TOUCH_BUTTON1) {
        currButtonDisplayChar = MOUSETEXT_CLOSEDAPPLE;
    } else if (touchDownChar == TOUCH_BOTH) {
        currButtonDisplayChar = ICONTEXT_MENU_TOUCHJOY;
    } else if (touchDownScancode < 0) {
        currButtonDisplayChar = ' ';
    }
    _setup_button_object_with_char(currButtonDisplayChar);
}

static void gltouchjoy_setTouchAxisSensitivity(float multiplier) {
    axes.multiplier = multiplier;
}

static void gltouchjoy_setButtonSwitchThreshold(int delta) {
    joyglobals.switchThreshold = delta;
}

static void gltouchjoy_setTouchVariant(touchjoy_variant_t variantType) {
    resetState();

    switch (variantType) {
        case EMULATED_JOYSTICK:
            variant.curr = variant.joys;
            break;

        case EMULATED_KEYPAD:
            variant.curr = variant.kpad;
            break;

        default:
            assert(false && "touch variant set to invalid");
            break;
    }

    resetState();
}

static touchjoy_variant_t gltouchjoy_getTouchVariant(void) {
    return variant.curr->variant();
}

static void gltouchjoy_setTouchAxisTypes(uint8_t rosetteChars[(ROSETTE_ROWS * ROSETTE_COLS)], int rosetteScancodes[(ROSETTE_ROWS * ROSETTE_COLS)]) {
    memcpy(axes.rosetteChars,     rosetteChars,     sizeof(uint8_t)*(ROSETTE_ROWS * ROSETTE_COLS));
    memcpy(axes.rosetteScancodes, rosetteScancodes, sizeof(int)    *(ROSETTE_ROWS * ROSETTE_COLS));
    _setup_axis_hud(axes.model);
}

static void gltouchjoy_setScreenDivision(float screenDivider) {
    joyglobals.screenDivider = screenDivider;
    // force reshape here to apply changes ...
    gltouchjoy_reshape(touchport.width, touchport.height, touchport.landscape);
}

static void gltouchjoy_setAxisOnLeft(bool axisIsOnLeft) {
    joyglobals.axisIsOnLeft = axisIsOnLeft;
    // force reshape here to apply changes ...
    gltouchjoy_reshape(touchport.width, touchport.height, touchport.landscape);
}

static void gltouchjoy_beginCalibration(void) {
    joyglobals.isCalibrating = true;
}

static void gltouchjoy_endCalibration(void) {
    joyglobals.isCalibrating = false;
}

static bool gltouchjoy_isCalibrating(void) {
    return joyglobals.isCalibrating;
}

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_gltouchjoy(void) {
    LOG("Registering OpenGL software touch joystick");

    axes.centerX = 240;
    axes.centerY = 160;
    axes.multiplier = 1.f;
    axes.trackingIndex = TRACKING_NONE;

    axes.rosetteChars[0]     = ' ';
    axes.rosetteScancodes[0] = -1;
    axes.rosetteChars[1]     = MOUSETEXT_UP;
    axes.rosetteScancodes[1] = -1;
    axes.rosetteChars[2]     = ' ';
    axes.rosetteScancodes[2] = -1;

    axes.rosetteChars[3]     = MOUSETEXT_LEFT;
    axes.rosetteScancodes[3] = -1;
    axes.rosetteChars[4]     = ICONTEXT_MENU_TOUCHJOY;
    axes.rosetteScancodes[4] = -1;
    axes.rosetteChars[5]     = MOUSETEXT_RIGHT;
    axes.rosetteScancodes[5] = -1;

    axes.rosetteChars[6]     = ' ';
    axes.rosetteScancodes[6] = -1;
    axes.rosetteChars[7]     = MOUSETEXT_DOWN;
    axes.rosetteScancodes[7] = -1;
    axes.rosetteChars[8]     = ' ';
    axes.rosetteScancodes[8] = -1;

    buttons.centerX = 240;
    buttons.centerY = 160;
    buttons.trackingIndex = TRACKING_NONE;

    buttons.touchDownChar = TOUCH_BUTTON0;
    buttons.touchDownScancode = -1;

    buttons.southChar = TOUCH_BUTTON1;
    buttons.southScancode = -1;

    buttons.northChar = TOUCH_BOTH;
    buttons.northScancode = -1;

    buttons.activeChar = MOUSETEXT_OPENAPPLE;

    joyglobals.isEnabled = true;
    joyglobals.ownsScreen = true;
    joyglobals.showControls = true;
    joyglobals.showAzimuth = true;
    joyglobals.screenDivider = 0.5f;
    joyglobals.axisIsOnLeft = true;
    joyglobals.switchThreshold = BUTTON_SWITCH_THRESHOLD_DEFAULT;

    video_animations->animation_showTouchJoystick = &_animation_showTouchJoystick;
    video_animations->animation_hideTouchJoystick = &_animation_hideTouchJoystick;

    joydriver_isTouchJoystickAvailable = &gltouchjoy_isTouchJoystickAvailable;
    joydriver_setTouchJoystickEnabled = &gltouchjoy_setTouchJoystickEnabled;
    joydriver_setTouchJoystickOwnsScreen = &gltouchjoy_setTouchJoystickOwnsScreen;
    joydriver_ownsScreen = &gltouchjoy_ownsScreen;
    joydriver_setShowControls = &gltouchjoy_setShowControls;
    joydriver_setShowAzimuth = &gltouchjoy_setShowAzimuth;
    joydriver_setTouchButtonTypes = &gltouchjoy_setTouchButtonTypes;
    joydriver_setTouchAxisSensitivity = &gltouchjoy_setTouchAxisSensitivity;
    joydriver_setButtonSwitchThreshold = &gltouchjoy_setButtonSwitchThreshold;
    joydriver_setTouchVariant = &gltouchjoy_setTouchVariant;
    joydriver_getTouchVariant = &gltouchjoy_getTouchVariant;
    joydriver_setTouchAxisTypes = &gltouchjoy_setTouchAxisTypes;
    joydriver_setScreenDivision = &gltouchjoy_setScreenDivision;
    joydriver_setAxisOnLeft = &gltouchjoy_setAxisOnLeft;
    joydriver_beginCalibration = &gltouchjoy_beginCalibration;
    joydriver_endCalibration = &gltouchjoy_endCalibration;
    joydriver_isCalibrating = &gltouchjoy_isCalibrating;

    glnode_registerNode(RENDER_LOW, (GLNode){
        .type = TOUCH_DEVICE_JOYSTICK,
        .setup = &gltouchjoy_setup,
        .shutdown = &gltouchjoy_shutdown,
        .render = &gltouchjoy_render,
        .reshape = &gltouchjoy_reshape,
        .onTouchEvent = &gltouchjoy_onTouchEvent,
        .setData = NULL,
    });
}

void gltouchjoy_registerVariant(touchjoy_variant_t variantType, GLTouchJoyVariant *touchJoyVariant) {
    switch (variantType) {
        case EMULATED_JOYSTICK:
            variant.joys = touchJoyVariant;
            variant.curr = variant.kpad;
            break;

        case EMULATED_KEYPAD:
            variant.kpad = touchJoyVariant;
            variant.curr = variant.kpad;
            break;

        default:
            assert(false && "invalid touch variant registration");
            break;
    }
}


void gldriver_joystick_reset(void) {
#warning FIXME TODO expunge this olde API ...
}

