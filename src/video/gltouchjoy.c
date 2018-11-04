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

    // TODO FIXME : support 2-players!
} touchport = { 0 };

#define AZIMUTH_CLASS(CLS, ...) \
    MODEL_CLASS(CLS, \
        GLuint vertShader; \
        GLuint fragShader; \
        GLuint program; \
        GLint uniformSolidColorIdx; \
        GLint uniformMVPIdx;);

AZIMUTH_CLASS(GLModelJoystickAzimuth);

static void gltouchjoy_applyPrefs(void);
static void gltouchjoy_shutdown(void);

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
    azimuthJoystick->uniformSolidColorIdx = UNINITIALIZED_GL;
    azimuthJoystick->uniformMVPIdx = UNINITIALIZED_GL;

    bool err = true;
    demoSource *vtxSource = NULL;
    demoSource *frgSource = NULL;
    do {
        // load/setup specific shaders

        vtxSource = glshader_createSource("SolidColor.vsh");
        if (!vtxSource) {
            LOG("Cannot compile vertex shader for joystick azimuth!");
            break;
        }

        frgSource = glshader_createSource("SolidColor.fsh");
        if (!frgSource) {
            LOG("Cannot compile fragment shader for joystick azimuth!");
            break;
        }

        // Build/use Program
        azimuthJoystick->program = glshader_buildProgram(vtxSource, frgSource, /*withTexcoord:*/false, &azimuthJoystick->vertShader, &azimuthJoystick->fragShader);

        // Get uniforms locations

        azimuthJoystick->uniformSolidColorIdx = glGetUniformLocation(azimuthJoystick->program, "solidColor");
        if (azimuthJoystick->uniformSolidColorIdx < 0) {
            LOG("OOPS, no solidColor uniform in Azimuth shader : %d", azimuthJoystick->uniformSolidColorIdx);
            break;
        }

        azimuthJoystick->uniformMVPIdx = glGetUniformLocation(azimuthJoystick->program, "modelViewProjectionMatrix");
        if (azimuthJoystick->uniformMVPIdx < 0) {
            LOG("OOPS, no modelViewProjectionMatrix in Azimuth shader : %d", azimuthJoystick->uniformMVPIdx);
            break;
        }

        err = false;
    } while (0);

    GL_MAYBELOG("build Aziumth joystick");

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

    // set the solid color values
    {
        float R = 0.f;
        float G = 0.f;
        float B = 0.f;
        float A = 1.f;
        switch (glhud_currentColorScheme) {
            case GREEN_ON_BLACK:
                G = 1.f;
                break;
            case GREEN_ON_BLUE:
                G = 1.f; // TODO FIXME : background colors ...
                break;
            case BLUE_ON_BLACK:
                B = 1.f;
                break;
            case WHITE_ON_BLACK:
                R = 1.f; G = 1.f; B = 1.f;
                break;
            case RED_ON_BLACK:
            default:
                R = 1.f;
                break;
        }
        glUniform4f(azimuthJoystick->uniformSolidColorIdx, R, G, B, A);
    }

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

    GL_MAYBELOG("Azimuth render");
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

        hudElement->pixels = CALLOC(AXIS_FB_WIDTH * AXIS_FB_HEIGHT * PIXEL_STRIDE, 1);
        hudElement->pixWidth = AXIS_FB_WIDTH;
        hudElement->pixHeight = AXIS_FB_HEIGHT;
    }

    const unsigned int row = (AXIS_TEMPLATE_COLS+1);

    uint8_t *rosetteChars = variant.curr->rosetteChars();
    for (unsigned int i=0; i<ROSETTE_ROWS; i++) {
        for (unsigned int j=0; j<ROSETTE_COLS; j++) {
            ((hudElement->tpl)+(row*i))[j] = rosetteChars[(i*ROSETTE_ROWS)+j];
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

        hudElement->pixels = CALLOC(BUTTON_FB_WIDTH * BUTTON_FB_HEIGHT * PIXEL_STRIDE, 1);
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

    gltouchjoy_shutdown();

    if (joyglobals.prefsChanged) {
        gltouchjoy_applyPrefs();
    }

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
    long lVal = 0;
    if (variant.curr->variant() == TOUCH_DEVICE_JOYSTICK_KEYPAD) {
        buttons.activeChar = prefs_parseLongValue(PREF_DOMAIN_JOYSTICK, PREF_KPAD_TOUCHDOWN_CHAR, &lVal, /*base:*/10) ? lVal : ICONTEXT_SPACE_VISUAL;
    } else {
        if (!prefs_parseLongValue(PREF_DOMAIN_JOYSTICK, PREF_JOY_TOUCHDOWN_CHAR, &lVal, /*base:*/10)) {
            buttons.activeChar = MOUSETEXT_OPENAPPLE;
        } else {
            if (lVal == TOUCH_BUTTON2) {
                buttons.activeChar = MOUSETEXT_CLOSEDAPPLE;
            } else if (lVal == TOUCH_BOTH) {
                buttons.activeChar = '+';
            } else {
                buttons.activeChar = MOUSETEXT_OPENAPPLE;
            }
        }
    }

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

    if (joyglobals.ownsScreen) {
        video_getAnimationDriver()->animation_showTouchJoystick();
    }
}

static void gltouchjoy_shutdown(void) {
    LOG("gltouchjoy_shutdown ...");
    if (!joyglobals.isAvailable) {
        return;
    }

    resetState();

    joyglobals.isAvailable = false;

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
    if (UNLIKELY(joyglobals.prefsChanged)) {
        gltouchjoy_setup(); // fully set up again on prefs change
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
    assert(video_isRenderThread());

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
    if (UNLIKELY(joyglobals.prefsChanged)) {
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

// ----------------------------------------------------------------------------

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

static void gltouchjoy_applyPrefs(void) {
    assert(video_isRenderThread());

    joyglobals.prefsChanged = false;

    bool bVal = false;
    float fVal = 0.f;
    long lVal = 0;

    const interface_device_t screenOwner = prefs_parseLongValue(PREF_DOMAIN_TOUCHSCREEN, PREF_SCREEN_OWNER, &lVal, /*base:*/10) ? lVal : TOUCH_DEVICE_NONE;
    joyglobals.ownsScreen = (screenOwner == TOUCH_DEVICE_JOYSTICK || screenOwner == TOUCH_DEVICE_JOYSTICK_KEYPAD);

    resetState();
    if (joyglobals.ownsScreen) {
        caps_lock = true; // HACK FOR NOW : force uppercase scancodes for touchjoy_kpad variant
        joyglobals.minAlpha = joyglobals.minAlphaWhenOwnsScreen;
        variant.curr = (screenOwner == TOUCH_DEVICE_JOYSTICK) ? variant.joys : variant.kpad;
    } else {
        joyglobals.minAlpha = 0.0;
    }

    joyglobals.showControls    = prefs_parseBoolValue (PREF_DOMAIN_JOYSTICK, PREF_SHOW_CONTROLS,    &bVal)     ? bVal : true;
    joyglobals.showAzimuth     = true;//prefs_parseBoolValue (PREF_DOMAIN_JOYSTICK, PREF_SHOW_AZIMUTH,     &bVal)     ? bVal : true;
    joyglobals.switchThreshold = prefs_parseLongValue (PREF_DOMAIN_JOYSTICK, PREF_SWITCH_THRESHOLD, &lVal, 10) ? lVal : BUTTON_SWITCH_THRESHOLD_DEFAULT;
    joyglobals.screenDivider   = prefs_parseFloatValue(PREF_DOMAIN_JOYSTICK, PREF_SCREEN_DIVISION,  &fVal)     ? fVal : 0.5f;
    joyglobals.axisIsOnLeft    = prefs_parseBoolValue (PREF_DOMAIN_JOYSTICK, PREF_AXIS_ON_LEFT,     &bVal)     ? bVal : true;
    axes.multiplier            = prefs_parseFloatValue(PREF_DOMAIN_JOYSTICK, PREF_AXIS_SENSITIVITY, &fVal)     ? fVal : 1.f;

    joyglobals.isCalibrating   = prefs_parseBoolValue (PREF_DOMAIN_TOUCHSCREEN, PREF_CALIBRATING,   &bVal)     ? bVal : false;

    variant.joys->prefsChanged(PREF_DOMAIN_JOYSTICK);
    variant.kpad->prefsChanged(PREF_DOMAIN_JOYSTICK);

    long width                 = prefs_parseLongValue (PREF_DOMAIN_INTERFACE, PREF_DEVICE_WIDTH,      &lVal, 10) ? lVal : (long)(SCANWIDTH*1.5);
    long height                = prefs_parseLongValue (PREF_DOMAIN_INTERFACE, PREF_DEVICE_HEIGHT,     &lVal, 10) ? lVal : (long)(SCANHEIGHT*1.5);
    bool isLandscape           = prefs_parseBoolValue (PREF_DOMAIN_INTERFACE, PREF_DEVICE_LANDSCAPE,  &bVal)     ? bVal : true;

    glhud_currentColorScheme = prefs_parseLongValue(PREF_DOMAIN_INTERFACE, PREF_SOFTHUD_COLOR, &lVal, 10) ? (interface_colorscheme_t)lVal : RED_ON_BLACK;

    gltouchjoy_reshape(width, height, isLandscape);
}

static void gltouchjoy_prefsChanged(const char *domain) {
    joyglobals.prefsChanged = true;
}

static void _init_gltouchjoy(void) {
    LOG("Registering OpenGL software touch joystick");

    axes.centerX = 240;
    axes.centerY = 160;
    axes.trackingIndex = TRACKING_NONE;

    buttons.centerX = 240;
    buttons.centerY = 160;
    buttons.trackingIndex = TRACKING_NONE;
    buttons.activeChar = MOUSETEXT_OPENAPPLE;

    joyglobals.prefsChanged = true; // force reload preferences/defaults

    video_getAnimationDriver()->animation_showTouchJoystick = &_animation_showTouchJoystick;
    video_getAnimationDriver()->animation_hideTouchJoystick = &_animation_hideTouchJoystick;

    glnode_registerNode(RENDER_LOW, (GLNode){
        .type = TOUCH_DEVICE_JOYSTICK,
        .setup = &gltouchjoy_setup,
        .shutdown = &gltouchjoy_shutdown,
        .render = &gltouchjoy_render,
        .onTouchEvent = &gltouchjoy_onTouchEvent,
    });

    prefs_registerListener(PREF_DOMAIN_JOYSTICK, &gltouchjoy_prefsChanged);
    prefs_registerListener(PREF_DOMAIN_TOUCHSCREEN, &gltouchjoy_prefsChanged);
    prefs_registerListener(PREF_DOMAIN_INTERFACE, &gltouchjoy_prefsChanged);
}

static __attribute__((constructor)) void __init_gltouchjoy(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_gltouchjoy);
}

void gltouchjoy_registerVariant(interface_device_t variantType, GLTouchJoyVariant *touchJoyVariant) {
    switch (variantType) {
        case TOUCH_DEVICE_JOYSTICK:
            variant.joys = touchJoyVariant;
            variant.curr = variant.joys;
            break;

        case TOUCH_DEVICE_JOYSTICK_KEYPAD:
            variant.kpad = touchJoyVariant;
            variant.curr = variant.kpad;
            break;

        default:
            assert(false && "invalid touch variant registration");
            break;
    }
}

