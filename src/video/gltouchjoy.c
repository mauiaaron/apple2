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

#define BUTTON_OBJ_W        0.075
#define BUTTON_OBJ_H        0.1

#define BUTTON_SWITCH_THRESHOLD_DEFAULT 22

GLTouchJoyGlobals joyglobals = { 0 };

typedef struct variant_params_s {
    GLModel *model; // origin model/texture
    GLModel *azimuthModel; // azimuth model
    void (*setupButtonModel)(char);

    int centerX;
    int centerY;
    int trackingIndex;
    GLfloat obj_w;
    GLfloat obj_h;
    struct timespec timingBegin;

    uint8_t activeChar;
    bool modelDirty;
} variant_params_s;

static variant_params_s axes = { 0 };
static variant_params_s butt = { 0 };

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
    int buttX;
    int buttXMax;
    int buttY;
    int buttYMax;
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

static void _azimuth_render(GLModel *azimuthModel) {
    if (!azimuthModel) {
        return;
    }

    GLModelJoystickAzimuth *azimuthJoystick = (GLModelJoystickAzimuth *)azimuthModel->custom;

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
    glBindBuffer(GL_ARRAY_BUFFER, azimuthModel->posBufferName);
    glBufferData(GL_ARRAY_BUFFER, azimuthModel->positionArraySize, azimuthModel->positions, GL_DYNAMIC_DRAW);

    // Bind our vertex array object
#if USE_VAO
    glBindVertexArray(azimuthModel->vaoName);
#else
    glBindBuffer(GL_ARRAY_BUFFER, azimuthModel->posBufferName);

    GLsizei posTypeSize = getGLTypeSize(azimuthModel->positionType);

    // Set up parmeters for position attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX, // What attibute index will this array feed in the vertex shader (see buildProgram)
                          azimuthModel->positionSize, // How many elements are there per position?
                          azimuthModel->positionType, // What is the type of this data?
                          GL_FALSE, // Do we want to normalize this data (0-1 range for fixed-pont types)
                          azimuthModel->positionSize*posTypeSize, // What is the stride (i.e. bytes between positions)?
                          0); // What is the offset in the VBO to the position data?
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, azimuthModel->elementBufferName);
#endif

    // draw it
    glDrawElements(azimuthModel->primType, azimuthModel->numElements, azimuthModel->elementType, 0);

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

    uint8_t *rosetteChars = (parent == axes.model)
        ? variant.curr->axisRosetteChars()
        : variant.curr->buttRosetteChars();

    if (rosetteChars) {
        for (unsigned int i=0; i<ROSETTE_ROWS; i++) {
            for (unsigned int j=0; j<ROSETTE_COLS; j++) {
                ((hudElement->tpl)+(row*i))[j] = rosetteChars[(i*ROSETTE_ROWS)+j];
            }
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
    ((hudElement->tpl)+(row*0))[0] = butt.activeChar;

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

static inline void _setup_button_object_nop(char newChar) {
    (void)newChar;
}

static inline void _setup_button_object_with_char(char newChar) {
    if (butt.activeChar != newChar) {
        butt.activeChar = newChar;
        _setup_button_hud(butt.model);
    }
}

// ----------------------------------------------------------------------------

static inline void resetState() {
    LOG("...");
    axes.trackingIndex = TRACKING_NONE;
    butt.trackingIndex = TRACKING_NONE;
    variant.joys->resetState();
    variant.kpad->resetState();
}

static void _setup_axis_models(variant_params_s *params) {
    params->model = mdlCreateQuad((GLModelParams_s){
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
    if (!params->model) {
        LOG("gltouchjoy not initializing axis");
        return;
    }
    if (!params->model->custom) {
        LOG("gltouchjoy axes initialization problem");
        return;
    }
    _setup_axis_hud(params->model);// HACK : twice to ensure correct template ...
    params->obj_w = AXIS_OBJ_W;
    params->obj_h = AXIS_OBJ_H;

    // axis azimuth object

    bool azimuthError = true;
    do {
        params->azimuthModel = mdlCreateQuad((GLModelParams_s){
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
        if (!params->azimuthModel) {
            LOG("gltouchjoy azimuth model initialization problem");
            break;
        }
        if (!params->azimuthModel->custom) {
            LOG("gltouchjoy azimuth custom model initialization problem");
            break;
        }
        azimuthError = false;
    } while (0);

    if (azimuthError) {
        mdlDestroyModel(&params->azimuthModel);
    }
}

static void gltouchjoy_setup(void) {
    LOG("...");

    gltouchjoy_shutdown();

    if (joyglobals.prefsChanged) {
        gltouchjoy_applyPrefs();
    }

    // axis origin object
    _setup_axis_models(&axes);

    // button object
    long lVal = 0;
    if (variant.curr->variant() == TOUCH_DEVICE_JOYSTICK_KEYPAD) {
        _setup_axis_models(&butt);
        butt.setupButtonModel = &_setup_button_object_nop;
    } else {
        butt.setupButtonModel = &_setup_button_object_with_char;
        if (!prefs_parseLongValue(PREF_DOMAIN_JOYSTICK, PREF_JOY_TOUCHDOWN_CHAR, &lVal, /*base:*/10)) {
            butt.activeChar = MOUSETEXT_OPENAPPLE;
        } else {
            if (lVal == TOUCH_BUTTON2) {
                butt.activeChar = MOUSETEXT_CLOSEDAPPLE;
            } else if (lVal == TOUCH_BOTH) {
                butt.activeChar = '+';
            } else {
                butt.activeChar = MOUSETEXT_OPENAPPLE;
            }
        }

        butt.azimuthModel = NULL;
        butt.model = mdlCreateQuad((GLModelParams_s){
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
        if (!butt.model) {
            LOG("gltouchjoy not initializing buttons");
            return;
        }
        if (!butt.model->custom) {
            LOG("gltouchjoy buttons initialization problem");
            return;
        }
        butt.obj_w = BUTTON_OBJ_W;
        butt.obj_h = BUTTON_OBJ_H;
    }

    variant.joys->setup();
    variant.kpad->setup();

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    axes.timingBegin = now;
    butt.timingBegin = now;

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
    mdlDestroyModel(&butt.model);
    mdlDestroyModel(&butt.azimuthModel);
}

static void _render_glmodels(variant_params_s *params, int textureActive, int textureId) {
    float alpha = 1.f;
    if (params->trackingIndex == TRACKING_NONE) {
        alpha = glhud_getTimedVisibility(params->timingBegin, joyglobals.minAlpha, 1.0);
        if (alpha < joyglobals.minAlpha) {
            alpha = joyglobals.minAlpha;
        }
    }
    if (alpha > 0.0) {
        GLModel *model = params->model;
        glUniform1f(alphaValue, alpha);

        glActiveTexture(textureActive);
        glBindTexture(GL_TEXTURE_2D, model->textureName);
        if (model->texDirty) {
            model->texDirty = false;
            _HACKAROUND_GLTEXIMAGE2D_PRE(textureActive, model->textureName);
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, model->texWidth, model->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, model->texPixels);
        }
        if (params->modelDirty) {
            params->modelDirty = false;
            glBindBuffer(GL_ARRAY_BUFFER, model->posBufferName);
            glBufferData(GL_ARRAY_BUFFER, model->positionArraySize, model->positions, GL_DYNAMIC_DRAW);
        }
        glUniform1i(texSamplerLoc, textureId);
        glhud_renderDefault(model);
    }

    if (joyglobals.showAzimuth && params->trackingIndex != TRACKING_NONE) {
        _azimuth_render(params->azimuthModel);
    }
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
    _render_glmodels(&axes, TEXTURE_ACTIVE_TOUCHJOY_AXIS, TEXTURE_ID_TOUCHJOY_AXIS);

    // draw button(s)
    char active = variant.curr->buttActiveChar();
    butt.setupButtonModel(active);
    _render_glmodels(&butt, TEXTURE_ACTIVE_TOUCHJOY_BUTTON, TEXTURE_ID_TOUCHJOY_BUTTON);
}

static void gltouchjoy_reshape(int w, int h, bool landscape) {
    LOG("w:%d h:%d landscape:%d", w, h, landscape);
    assert(video_isRenderThread());

    swizzleDimensions(&w, &h, landscape);
    touchport.width = w;
    touchport.height = h;

    touchport.axisY = 0;
    touchport.axisYMax = h;
    touchport.buttY = 0;
    touchport.buttYMax = h;

    if (joyglobals.axisIsOnLeft) {
        touchport.axisX = 0;
        touchport.axisXMax = (w * joyglobals.screenDivider);
        touchport.buttX = (w * joyglobals.screenDivider);
        touchport.buttXMax = w;
    } else {
        touchport.buttX = 0;
        touchport.buttXMax = (w * joyglobals.screenDivider);
        touchport.axisX = (w * joyglobals.screenDivider);
        touchport.axisXMax = w;
    }
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

static void _touch_down(variant_params_s *params, int pointer_idx, int x, int y, void(*downFn)(void)) {
    params->trackingIndex = pointer_idx;
    params->centerX = x;
    params->centerY = y;
    _reset_model_position(params->model, x, y, (params->obj_w/2.f), (params->obj_h/2.f), params->azimuthModel);
    params->modelDirty = true;
    downFn();
}

static void _touch_move(variant_params_s *params, float *x_coords, float *y_coords, void (*moveFn)(int, int)) {
    int x = (int)x_coords[params->trackingIndex];
    int y = (int)y_coords[params->trackingIndex];

    if (joyglobals.showAzimuth && params->azimuthModel) {
        float centerX = 0.f;
        float centerY = 0.f;
        glhud_screenToModel(x, y, touchport.width, touchport.height, &centerX, &centerY);
        GLfloat *quadAzimuth = (GLfloat *)params->azimuthModel->positions;
        quadAzimuth[4 +0] = centerX;
        quadAzimuth[4 +1] = centerY;
    };

    x -= params->centerX;
    y -= params->centerY;
    moveFn(x, y);
}

static void _touch_up(variant_params_s *params, float *x_coords, float *y_coords, variant_params_s *altParams, void(*upFn)(int, int), const char *altTag) {

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    params->timingBegin = now;

    int x = (int)x_coords[params->trackingIndex] - params->centerX;
    int y = (int)y_coords[params->trackingIndex] - params->centerY;

    if (altParams->trackingIndex > params->trackingIndex) {
        TOUCH_JOY_LOG("\t!!! : DECREMENTING %s.trackingIndex", altTag);
        --altParams->trackingIndex;
    }

    upFn(x, y);
    params->trackingIndex = TRACKING_NONE;
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

    switch (action) {
        case TOUCH_DOWN:
        case TOUCH_POINTER_DOWN:
            TOUCH_JOY_LOG("------DOWN:");
            {
                int x = (int)x_coords[pointer_idx];
                int y = (int)y_coords[pointer_idx];
                if (_is_point_on_axis_side(x, y)) {
                    if (pointer_idx == butt.trackingIndex) {
                        TOUCH_JOY_LOG("\t!!! : INCREMENTING buttons.trackingIndex");
                        ++butt.trackingIndex;
                    }
                    if (axes.trackingIndex != TRACKING_NONE) {
                        TOUCH_JOY_LOG("\t!!! : IGNORING OTHER AXIS TOUCH DOWN %d", pointer_idx);
                    } else {
                        _touch_down(&axes, pointer_idx, x, y, variant.curr->axisDown);
                    }
                } else {
                    if (pointer_idx == axes.trackingIndex) {
                        TOUCH_JOY_LOG("\t!!! : INCREMENTING axes.trackingIndex");
                        ++axes.trackingIndex;
                    }
                    if (butt.trackingIndex != TRACKING_NONE) {
                        TOUCH_JOY_LOG("\t!!! : IGNORING OTHER BUTTON TOUCH DOWN %d", pointer_idx);
                    } else {
                        _touch_down(&butt, pointer_idx, x, y, variant.curr->buttonDown);
                    }
                }
            }
            break;

        case TOUCH_MOVE:
            TOUCH_JOY_LOG("------MOVE:");
            if (axes.trackingIndex >= 0) {
                _touch_move(&axes, x_coords, y_coords, variant.curr->axisMove);
            }
            if (butt.trackingIndex >= 0) {
                _touch_move(&butt, x_coords, y_coords, variant.curr->buttonMove);
            }
            break;

        case TOUCH_UP:
        case TOUCH_POINTER_UP:
            TOUCH_JOY_LOG("------UP:");
            if (pointer_idx == axes.trackingIndex) {
                _touch_up(&axes, x_coords, y_coords, /*alt:*/&butt, variant.curr->axisUp, "buttons");
            } else if (pointer_idx == butt.trackingIndex) {
                _touch_up(&butt, x_coords, y_coords, /*alt:*/&axes, variant.curr->buttonUp, "axis");
            } else {
                if (pointer_count == 1) {
                    TOUCH_JOY_LOG("\t!!! : RESETTING TOUCH JOYSTICK STATE MACHINE");
                    resetState();
                } else {
                    TOUCH_JOY_LOG("\t!!! : IGNORING OTHER TOUCH UP %d", pointer_idx);
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

    int x;
    int y;

    x = touchport.axisX + ((touchport.axisXMax - touchport.axisX)/2);
    y = touchport.axisY + ((touchport.axisYMax - touchport.axisY)/2);
    _reset_model_position(axes.model, x, y, (axes.obj_w/2.f), (axes.obj_h/2.f), NULL);
    axes.modelDirty = true;

    x = touchport.buttX + ((touchport.buttXMax - touchport.buttX)/2);
    y = touchport.buttY + ((touchport.buttYMax - touchport.buttY)/2);
    _reset_model_position(butt.model, x, y, (butt.obj_w/2.f), (butt.obj_h/2.f), NULL);
    butt.modelDirty = true;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    axes.timingBegin = now;
    butt.timingBegin = now;
}

static void _animation_hideTouchJoystick(void) {
    if (!joyglobals.isAvailable) {
        return;
    }
    axes.timingBegin = (struct timespec){ 0 };
    butt.timingBegin = (struct timespec){ 0 };
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

    joyglobals.tapDelayFrames  = prefs_parseLongValue (PREF_DOMAIN_JOYSTICK, PREF_JOY_TAP_DELAY, &lVal, 10)    ? lVal : BUTTON_TAP_DELAY_FRAMES_DEFAULT;
    if (joyglobals.tapDelayFrames < 0) {
        joyglobals.tapDelayFrames = 0;
    } else if (joyglobals.tapDelayFrames > BUTTON_TAP_DELAY_FRAMES_MAX) {
        joyglobals.tapDelayFrames = BUTTON_TAP_DELAY_FRAMES_MAX;
    }

    joyglobals.showControls    = prefs_parseBoolValue (PREF_DOMAIN_JOYSTICK, PREF_SHOW_CONTROLS,    &bVal)     ? bVal : true;
    joyglobals.showAzimuth     = true;//prefs_parseBoolValue (PREF_DOMAIN_JOYSTICK, PREF_SHOW_AZIMUTH,     &bVal)     ? bVal : true;
    joyglobals.switchThreshold = prefs_parseLongValue (PREF_DOMAIN_JOYSTICK, PREF_SWITCH_THRESHOLD, &lVal, 10) ? lVal : BUTTON_SWITCH_THRESHOLD_DEFAULT;
    joyglobals.screenDivider   = prefs_parseFloatValue(PREF_DOMAIN_JOYSTICK, PREF_SCREEN_DIVISION,  &fVal)     ? fVal : 0.5f;
    joyglobals.axisIsOnLeft    = prefs_parseBoolValue (PREF_DOMAIN_JOYSTICK, PREF_AXIS_ON_LEFT,     &bVal)     ? bVal : true;
    joyglobals.axisMultiplier  = prefs_parseFloatValue(PREF_DOMAIN_JOYSTICK, PREF_AXIS_SENSITIVITY, &fVal)     ? fVal : 1.f;

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
    axes.activeChar = ' ';

    butt.centerX = 240;
    butt.centerY = 160;
    butt.trackingIndex = TRACKING_NONE;
    butt.activeChar = ' ';

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

