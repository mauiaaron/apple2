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
#include "video/glhudmodel.h"

#define MODEL_DEPTH -0.03125

#define AXIS_TEMPLATE_COLS 5
#define AXIS_TEMPLATE_ROWS 5

#define BUTTON_TEMPLATE_COLS 1
#define BUTTON_TEMPLATE_ROWS 1

// HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...
#define AXIS_FB_WIDTH ((AXIS_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) + INTERPOLATED_PIXEL_ADJUSTMENT)
#define AXIS_FB_HEIGHT (AXIS_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

// HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...
#define BUTTON_FB_WIDTH ((BUTTON_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) + INTERPOLATED_PIXEL_ADJUSTMENT)
#define BUTTON_FB_HEIGHT (BUTTON_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define AXIS_OBJ_W        0.4
#define AXIS_OBJ_H        0.5
#define AXIS_OBJ_HALF_W   (AXIS_OBJ_W/2.f)
#define AXIS_OBJ_HALF_H   (AXIS_OBJ_H/2.f)

#define BUTTON_OBJ_W        0.2
#define BUTTON_OBJ_H        0.25
#define BUTTON_OBJ_HALF_W   (BUTTON_OBJ_W/2.f)
#define BUTTON_OBJ_HALF_H   (BUTTON_OBJ_H/2.f)

#define BUTTON_SWITCH_THRESHOLD_DEFAULT 44

enum {
    TOUCHED_NONE = -1,
    TOUCHED_BUTTON0 = 0,
    TOUCHED_BUTTON1,
    TOUCHED_BOTH,
};

static bool isAvailable = false; // Were there any OpenGL/memory errors on gltouchjoy initialization?
static bool isEnabled = true;    // Does player want touchjoy enabled?
static bool isVisible = true;    // Does player want touchjoy to have some form of visibility?
static float minAlpha = 0.0;     // Minimum alpha value of touchjoy components (at zero, will not draw)

static glanim_t touchjoyAnimation = { 0 };

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

// touch axis variables

static struct {

    GLModel *model;
    uint8_t northChar;
    uint8_t westChar;
    uint8_t eastChar;
    uint8_t southChar;
    bool modelDirty;

    touchjoy_axis_type_t type;

    int centerX;
    int centerY;
    int trackingIndex;
    struct timespec timingBegin;

} axes = { 0 };

// button object variables

static struct {
    GLModel *model;
    uint8_t activeChar;
    bool modelDirty;

    int touchDownButton;
    int northButton;
    int southButton;

    uint8_t char0;
    uint8_t char1;
    uint8_t charBoth;

    int switchThreshold;

    int centerX;
    int centerY;
    int trackingIndex;
    struct timespec timingBegin;

} buttons = { 0 };

// ----------------------------------------------------------------------------

static void _setup_axis_object(GLModel *parent) {
    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;

    if (hudElement->tpl == NULL) {
        // deferred construction ...
        const char axisTemplate[AXIS_TEMPLATE_ROWS][AXIS_TEMPLATE_COLS+1] = {
            "  @  ",
            "  |  ",
            "@-+-@",
            "  |  ",
            "  @  ",
        };

        const unsigned int size = sizeof(axisTemplate);
        hudElement->tpl = calloc(size, 1);
        hudElement->tplWidth = AXIS_TEMPLATE_COLS;
        hudElement->tplHeight = AXIS_TEMPLATE_ROWS;
        memcpy(hudElement->tpl, axisTemplate, size);

        hudElement->pixels = calloc(AXIS_FB_WIDTH * AXIS_FB_HEIGHT, 1);
        hudElement->pixWidth = AXIS_FB_WIDTH;
        hudElement->pixHeight = AXIS_FB_HEIGHT;
    }

    const unsigned int row = (AXIS_TEMPLATE_COLS+1);
    ((hudElement->tpl)+(row*0))[2] = axes.northChar;
    ((hudElement->tpl)+(row*2))[0] = axes.westChar;
    ((hudElement->tpl)+(row*2))[4] = axes.eastChar;
    ((hudElement->tpl)+(row*4))[2] = axes.southChar;

    glhud_setupDefault(parent);
}

static void *_create_touchjoy_hud(void) {
    GLModelHUDElement *hudElement = (GLModelHUDElement *)glhud_createDefault();
    hudElement->blackIsTransparent = true;
    return hudElement;
}

static void _setup_button_object(GLModel *parent) {
    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;

    if (hudElement->tpl == NULL) {
        // deferred construction ...
        const char buttonTemplate[BUTTON_TEMPLATE_ROWS][BUTTON_TEMPLATE_COLS+1] = {
            "@",
        };

        const unsigned int size = sizeof(buttonTemplate);
        hudElement->tpl = calloc(size, 1);
        hudElement->tplWidth = BUTTON_TEMPLATE_COLS;
        hudElement->tplHeight = BUTTON_TEMPLATE_ROWS;
        memcpy(hudElement->tpl, buttonTemplate, size);

        hudElement->pixels = calloc(BUTTON_FB_WIDTH * BUTTON_FB_HEIGHT, 1);
        hudElement->pixWidth = BUTTON_FB_WIDTH;
        hudElement->pixHeight = BUTTON_FB_HEIGHT;
    }

    const unsigned int row = (BUTTON_TEMPLATE_COLS+1);
    ((hudElement->tpl)+(row*0))[0] = buttons.activeChar;

    glhud_setupDefault(parent);
}

static inline void _screen_to_model(float x, float y, float *screenX, float *screenY) {
    *screenX = (x/(touchport.width>>1))-1.f;
    *screenY = ((touchport.height-y)/(touchport.height>>1))-1.f;
}

static void _model_to_screen(float screenCoords[4], GLModel *model) {

    float x0 = 1.0;
    float y0 = 1.0;
    float x1 = -1.0;
    float y1 = -1.0;

#warning NOTE: we possibly should use matrix calculations (but assuming HUD elements are identity/orthographic for now)
    GLfloat *positions = (GLfloat *)(model->positions);
    unsigned int stride = model->positionSize;
    unsigned int len = model->positionArraySize/getGLTypeSize(model->positionType);
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

    // OpenGL screen origin is bottom-left (Android is top-left)
    float yFlip0 = touchport.height - (y1 * touchport.height);
    float yFlip1 = touchport.height - (y0 * touchport.height);

    screenCoords[0] = x0 * touchport.width;
    screenCoords[1] = yFlip0;
    screenCoords[2] = x1 * touchport.width;
    screenCoords[3] = yFlip1;
}

static void gltouchjoy_init(void) {
    LOG("gltouchjoy_init ...");

    mdlDestroyModel(&axes.model);
    mdlDestroyModel(&buttons.model);

    axes.model = mdlCreateQuad(-1.05, -1.0, AXIS_OBJ_W, AXIS_OBJ_H, MODEL_DEPTH, AXIS_FB_WIDTH, AXIS_FB_HEIGHT, GL_RGBA/*RGBA_8888*/, (GLCustom){
            .create = &_create_touchjoy_hud,
            .setup = &_setup_axis_object,
            .destroy = &glhud_destroyDefault,
            });
    if (!axes.model) {
        LOG("gltouchjoy not initializing axis");
        return;
    }

    // button object

    buttons.model = mdlCreateQuad(1.05-BUTTON_OBJ_W, -1.0, BUTTON_OBJ_W, BUTTON_OBJ_H, MODEL_DEPTH, BUTTON_FB_WIDTH, BUTTON_FB_HEIGHT, GL_RGBA/*RGBA_8888*/, (GLCustom){
            .create = &_create_touchjoy_hud,
            .setup = &_setup_button_object,
            .destroy = &glhud_destroyDefault,
            });
    if (!buttons.model) {
        LOG("gltouchjoy not initializing buttons");
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &axes.timingBegin);
    clock_gettime(CLOCK_MONOTONIC, &buttons.timingBegin);

    isAvailable = true;
}

static void gltouchjoy_destroy(void) {
    LOG("gltouchjoy_destroy ...");
    if (!isAvailable) {
        return;
    }

    isAvailable = false;

    mdlDestroyModel(&axes.model);
    mdlDestroyModel(&buttons.model);
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

    struct timespec now = { 0 };
    struct timespec deltat = { 0 };
    float alpha = minAlpha;

    // draw axis

    clock_gettime(CLOCK_MONOTONIC, &now);
    alpha = minAlpha;
    deltat = timespec_diff(axes.timingBegin, now, NULL);
    if (deltat.tv_sec == 0) {
        alpha = 1.0;
        if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
            alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
            if (alpha < minAlpha) {
                alpha = minAlpha;
            }
        }
    }

    if (alpha > 0.0) {
        glViewport(0, 0, touchport.width, touchport.height); // NOTE : show these HUD elements beyond the framebuffer dimensions
        glUniform1f(alphaValue, alpha);

        glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_AXIS);
        glBindTexture(GL_TEXTURE_2D, axes.model->textureName);
        if (axes.model->texDirty) {
            axes.model->texDirty = false;
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, AXIS_FB_WIDTH, AXIS_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, axes.model->texPixels);
        }
        if (axes.modelDirty) {
            axes.modelDirty = false;
            glBindBuffer(GL_ARRAY_BUFFER, axes.model->posBufferName);
            glBufferData(GL_ARRAY_BUFFER, axes.model->positionArraySize, axes.model->positions, GL_DYNAMIC_DRAW);
        }
        glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHJOY_AXIS);
        glhud_renderDefault(axes.model);
    }

    // draw button(s)

    clock_gettime(CLOCK_MONOTONIC, &now);
    alpha = minAlpha;
    deltat = timespec_diff(buttons.timingBegin, now, NULL);
    if (deltat.tv_sec == 0) {
        alpha = 1.0;
        if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
            alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
            if (alpha < minAlpha) {
                alpha = minAlpha;
            }
        }
    }

    if (alpha > 0.0) {
        glViewport(0, 0, touchport.width, touchport.height); // NOTE : show these HUD elements beyond the framebuffer dimensions
        glUniform1f(alphaValue, alpha);

        glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_BUTTON);
        glBindTexture(GL_TEXTURE_2D, buttons.model->textureName);
        if (buttons.model->texDirty) {
            buttons.model->texDirty = false;
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, BUTTON_FB_WIDTH, BUTTON_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, buttons.model->texPixels);
        }
        if (buttons.modelDirty) {
            buttons.modelDirty = false;
            glBindBuffer(GL_ARRAY_BUFFER, buttons.model->posBufferName);
            glBufferData(GL_ARRAY_BUFFER, buttons.model->positionArraySize, buttons.model->positions, GL_DYNAMIC_DRAW);
        }
        glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHJOY_BUTTON);
        glhud_renderDefault(buttons.model);
    }
}

static void gltouchjoy_reshape(int w, int h) {
    LOG("gltouchjoy_reshape(%d, %d)", w, h);

    touchport.axisX = 0;

    if (w > touchport.width) {
        touchport.width = w;
        touchport.axisXMax = w>>1;
        touchport.buttonX = w>>1;
        touchport.buttonXMax = w;
    }
    if (h > touchport.height) {
        touchport.height = h;
        touchport.axisY = h>>1;
        touchport.axisYMax = h;
        touchport.buttonY = h>>1;
        touchport.buttonYMax = h;
    }
}

static void gltouchjoy_resetJoystick(void) {
    // no-op
}

static inline bool _is_point_on_axis_side(float x, float y) {
    return (x >= touchport.axisX && x <= touchport.axisXMax && y >= touchport.axisY && y <= touchport.axisYMax);
}

static inline bool _is_point_on_button_side(float x, float y) {
    return (x >= touchport.buttonX && x <= touchport.buttonXMax && y >= touchport.buttonY && y <= touchport.buttonYMax);
}

static inline void _reset_model_position(GLModel *model, float touchX, float touchY, float objHalfW, float objHalfH) {

    float centerX = 0.f;
    float centerY = 0.f;
    _screen_to_model(touchX, touchY, &centerX, &centerY);

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
}

static inline void _move_joystick_axis(int x, int y) {

    x = (x - axes.centerX) + 0x80;
    y = (y - axes.centerY) + 0x80;

    if (x < 0) {
        x = 0;
    }
    if (x > 0xff) {
        x = 0xff;
    }
    if (y < 0) {
        y = 0;
    }
    if (y > 0xff) {
        y = 0xff;
    }

    joy_x = x;
    joy_y = y;
}

static inline void _setup_button_object_with_char(char newChar) {
    if (buttons.activeChar != newChar) {
        buttons.activeChar = newChar;
        _setup_button_object(buttons.model);
    }
}

static inline void _move_button_axis(int x, int y) {

    x -= buttons.centerX;
    y -= buttons.centerY;

    if ((y < -buttons.switchThreshold) || (y > buttons.switchThreshold)) {
        if (y < 0) {
            //LOG("\tbutton neg y threshold (%d)", y);
            if (buttons.northButton == TOUCHED_BUTTON0) {
                joy_button0 = 0x80;
                joy_button1 = 0;
                _setup_button_object_with_char(buttons.char0);
            } else if (buttons.northButton == TOUCHED_BUTTON1) {
                joy_button0 = 0;
                joy_button1 = 0x80;
                _setup_button_object_with_char(buttons.char1);
            } else if (buttons.northButton == TOUCHED_BOTH) {
                joy_button0 = 0x80;
                joy_button1 = 0x80;
                _setup_button_object_with_char(buttons.charBoth);
            } else {
                joy_button0 = 0;
                joy_button1 = 0;
            }
        } else {
            //LOG("\tbutton pos y threshold (%d)", y);
            if (buttons.southButton == TOUCHED_BUTTON0) {
                joy_button0 = 0x80;
                joy_button1 = 0;
                _setup_button_object_with_char(buttons.char0);
            } else if (buttons.southButton == TOUCHED_BUTTON1) {
                joy_button0 = 0;
                joy_button1 = 0x80;
                _setup_button_object_with_char(buttons.char1);
            } else if (buttons.southButton == TOUCHED_BOTH) {
                joy_button0 = 0x80;
                joy_button1 = 0x80;
                _setup_button_object_with_char(buttons.charBoth);
            } else {
                joy_button0 = 0;
                joy_button1 = 0;
            }
        }
    }
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
        case TOUCH_POINTER_DOWN:
            if (_is_point_on_axis_side(x, y)) {
                axisConsumed = true;
                axes.trackingIndex = pointer_idx;
                axes.centerX = (int)x;
                axes.centerY = (int)y;
                _reset_model_position(axes.model, x, y, AXIS_OBJ_HALF_W, AXIS_OBJ_HALF_H);
                axes.modelDirty = true;
                LOG("---TOUCH %sDOWN (axis index %d) center:(%d,%d) -> joy(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "),
                        axes.trackingIndex, axes.centerX, axes.centerY, joy_x, joy_y);
            } else if (_is_point_on_button_side(x, y)) {
                buttonConsumed = true;
                buttons.trackingIndex = pointer_idx;
                if (buttons.touchDownButton == TOUCHED_BUTTON0) {
                    joy_button0 = 0x80;
                    _setup_button_object_with_char(buttons.char0);
                } else if (buttons.touchDownButton == TOUCHED_BUTTON1) {
                    joy_button1 = 0x80;
                    _setup_button_object_with_char(buttons.char1);
                } else if (buttons.touchDownButton == TOUCHED_BOTH) {
                    joy_button0 = 0x80;
                    joy_button1 = 0x80;
                    _setup_button_object_with_char(buttons.charBoth);
                }
                buttons.centerX = (int)x;
                buttons.centerY = (int)y;
                _reset_model_position(buttons.model, x, y, BUTTON_OBJ_HALF_W, BUTTON_OBJ_HALF_H);
                buttons.modelDirty = true;
                LOG("---TOUCH %sDOWN (buttons index %d) center:(%d,%d) -> buttons(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "),
                        buttons.trackingIndex, buttons.centerX, buttons.centerY, joy_button0, joy_button1);
            } else {
                // not tracking tap/gestures originating from control-gesture portion of screen
            }
            break;

        case TOUCH_MOVE:
            if (axes.trackingIndex >= 0) {
                axisConsumed = true;
                _move_joystick_axis((int)x_coords[axes.trackingIndex], (int)y_coords[axes.trackingIndex]);
                LOG("---TOUCH MOVE ...tracking axis:%d (%d,%d) -> joy(0x%02X,0x%02X)", axes.trackingIndex,
                        (int)x_coords[axes.trackingIndex], (int)y_coords[axes.trackingIndex], joy_x, joy_y);
            }
            if (buttons.trackingIndex >= 0) {
                buttonConsumed = true;
                _move_button_axis((int)x_coords[buttons.trackingIndex], (int)y_coords[buttons.trackingIndex]);
                LOG("+++TOUCH MOVE ...tracking button:%d (%d,%d) -> buttons(0x%02X,0x%02X)", buttons.trackingIndex,
                        (int)x_coords[buttons.trackingIndex], (int)y_coords[buttons.trackingIndex], joy_button0, joy_button1);
            }
            break;

        case TOUCH_UP:
        case TOUCH_POINTER_UP:
            if (pointer_idx == axes.trackingIndex) {
                axisConsumed = true;
                bool resetIndex = false;
                if (buttons.trackingIndex > axes.trackingIndex) {
                    // TODO FIXME : is resetting the pointer index just an Android-ism?
                    resetIndex = true;
                    --buttons.trackingIndex;
                }
                axes.trackingIndex = TOUCHED_NONE;
                joy_x = HALF_JOY_RANGE;
                joy_y = HALF_JOY_RANGE;
                LOG("---TOUCH %sUP (axis went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset buttons index!)" : ""));
            } else if (pointer_idx == buttons.trackingIndex) {
                buttonConsumed = true;
                bool resetIndex = false;
                if (axes.trackingIndex > buttons.trackingIndex) {
                    // TODO FIXME : is resetting the pointer index just an Android-ism?
                    resetIndex = true;
                    --axes.trackingIndex;
                }
                buttons.trackingIndex = TOUCHED_NONE;
                joy_button0 = 0x0;
                joy_button1 = 0x0;
                LOG("---TOUCH %sUP (buttons went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset axis index!)" : ""));
            } else {
                // not tracking tap/gestures originating from control-gesture portion of screen
            }
            break;

        case TOUCH_CANCEL:
            LOG("---TOUCH CANCEL");
            axes.trackingIndex = TOUCHED_NONE;
            buttons.trackingIndex = TOUCHED_NONE;
            joy_button0 = 0x0;
            joy_button1 = 0x0;
            joy_x = HALF_JOY_RANGE;
            joy_y = HALF_JOY_RANGE;
            break;

        default:
            LOG("!!! UNKNOWN TOUCH EVENT : %d", action);
            break;
    }

    if (axisConsumed) {
        clock_gettime(CLOCK_MONOTONIC, &axes.timingBegin);
    }
    if (buttonConsumed) {
        clock_gettime(CLOCK_MONOTONIC, &buttons.timingBegin);
    }

    return (axisConsumed || buttonConsumed);
}

static bool gltouchjoy_isTouchJoystickAvailable(void) {
    return isAvailable;
}

static void gltouchjoy_setTouchJoyEnabled(bool enabled) {
    isEnabled = enabled;
}

static void gltouchjoy_setTouchButtonValues(char char0, char char1) {
    buttons.char0 = char0;
    buttons.char1 = char1;
}

static void gltouchjoy_setTouchAxisType(touchjoy_axis_type_t axisType) {
    axes.type = axisType;
    _setup_axis_object(axes.model);
}

static void gltouchjoy_setTouchAxisValues(char north, char west, char east, char south) {
    axes.northChar = north;
    axes.westChar  = west;
    axes.eastChar  = east;
    axes.southChar = south;
    if (axes.type == AXIS_EMULATED_KEYBOARD) {
        _setup_axis_object(axes.model);
    }
}

__attribute__((constructor))
static void _init_gltouchjoy(void) {
    LOG("Registering OpenGL software touch joystick");

    axes.centerX = 240;
    axes.centerY = 160;
    axes.trackingIndex = TOUCHED_NONE;
    axes.type = AXIS_EMULATED_DEVICE;
    axes.northChar = MOUSETEXT_UP;
    axes.westChar  = MOUSETEXT_LEFT;
    axes.eastChar  = MOUSETEXT_RIGHT;
    axes.southChar = MOUSETEXT_DOWN;

    buttons.centerX = 240;
    buttons.centerY = 160;
    buttons.trackingIndex = TOUCHED_NONE;
    buttons.touchDownButton = TOUCHED_BUTTON0;
    buttons.northButton = TOUCHED_BOTH;
    buttons.southButton = TOUCHED_BUTTON1;
    buttons.char0 = MOUSETEXT_OPENAPPLE;
    buttons.char1 = MOUSETEXT_CLOSEDAPPLE;
    buttons.charBoth = '+';
    buttons.activeChar = MOUSETEXT_OPENAPPLE;
    buttons.switchThreshold = BUTTON_SWITCH_THRESHOLD_DEFAULT;

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
#warning FIXME TODO expunge this olde API ...
}

