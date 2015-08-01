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
#include "video/glnode.h"

#if !INTERFACE_TOUCH
#error this is a touch interface module, possibly you mean to not compile this at all?
#endif

#define DEBUG_TOUCH_JOY 0
#if DEBUG_TOUCH_JOY
#   define TOUCH_JOY_LOG(...) LOG(__VA_ARGS__)
#else
#   define TOUCH_JOY_LOG(...)
#endif

#define MODEL_DEPTH -1/32.f

#define AXIS_TEMPLATE_COLS 5
#define AXIS_TEMPLATE_ROWS 5

#define BUTTON_TEMPLATE_COLS 1
#define BUTTON_TEMPLATE_ROWS 1

#define AXIS_FB_WIDTH (AXIS_TEMPLATE_COLS * FONT80_WIDTH_PIXELS)
#define AXIS_FB_HEIGHT (AXIS_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define BUTTON_FB_WIDTH (BUTTON_TEMPLATE_COLS * FONT80_WIDTH_PIXELS)
#define BUTTON_FB_HEIGHT (BUTTON_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define AXIS_OBJ_W        0.4
#define AXIS_OBJ_H        0.5
#define AXIS_OBJ_HALF_W   (AXIS_OBJ_W/2.f)
#define AXIS_OBJ_HALF_H   (AXIS_OBJ_H/2.f)

#define BUTTON_OBJ_W        0.2
#define BUTTON_OBJ_H        0.25
#define BUTTON_OBJ_HALF_W   (BUTTON_OBJ_W/2.f)
#define BUTTON_OBJ_HALF_H   (BUTTON_OBJ_H/2.f)

#define BUTTON_SWITCH_THRESHOLD_DEFAULT 22
#define BUTTON_TAP_DELAY_NANOS_DEFAULT 50000000

static bool isAvailable = false; // Were there any OpenGL/memory errors on gltouchjoy initialization?
static bool isShuttingDown = false;
static bool isEnabled = true;    // Does player want touchjoy enabled?
static bool ownsScreen = true;   // Does the touchjoy currently own the screen?
static float minAlphaWhenOwnsScreen = 0;
static float minAlpha = 0;

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

    touchjoy_variant_t type;

    int centerX;
    int centerY;
    float multiplier;
    int trackingIndex;
    struct timespec timingBegin;

} axes = { 0 };

// button object variables

static struct {
    GLModel *model;
    uint8_t activeChar;
    bool modelDirty;

    touchjoy_button_type_t touchDownButton;
    touchjoy_button_type_t northButton;
    touchjoy_button_type_t southButton;

    uint8_t char0;
    uint8_t char1;
    uint8_t charBoth;

    int switchThreshold;

    int centerX;
    int centerY;
    int trackingIndex;
    struct timespec timingBegin;

    pthread_t tapDelayThreadId;
    pthread_mutex_t tapDelayMutex;
    pthread_cond_t tapDelayCond;
    unsigned int tapDelayNanos;

    volatile uint8_t currButtonValue0;
    volatile uint8_t currButtonValue1;
    volatile uint8_t currButtonChar;

} buttons = { 0 };

// ----------------------------------------------------------------------------

#warning FIXME TODO ... this can become a common helper function ...
static inline float _get_component_visibility(struct timespec timingBegin) {
    struct timespec now = { 0 };
    struct timespec deltat = { 0 };

    clock_gettime(CLOCK_MONOTONIC, &now);
    float alpha = minAlpha;
    deltat = timespec_diff(timingBegin, now, NULL);
    if (deltat.tv_sec == 0) {
        alpha = 1.0;
        if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
            alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
            if (alpha < minAlpha) {
                alpha = minAlpha;
            }
        }
    }

    return alpha;
}

static void _setup_axis_object(GLModel *parent) {
    if (UNLIKELY(!parent)) {
        LOG("gltouchjoy WARN : cannot setup axis object without parent");
        return;
    }

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
    if (hudElement) {
        hudElement->blackIsTransparent = true;
    }
    return hudElement;
}

static void _setup_button_object(GLModel *parent) {
    if (UNLIKELY(!parent)) {
        LOG("gltouchjoy WARN : cannot setup button object without parent");
        return;
    }

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

static inline void _setup_button_object_with_char(char newChar) {
    if (buttons.activeChar != newChar) {
        buttons.activeChar = newChar;
        _setup_button_object(buttons.model);
    }
}

// ----------------------------------------------------------------------------

// Tap Delay Thread : delays processing of touch-down so that a different joystick button/key can be fired

static inline void _signal_tap_delay(void) {
    pthread_mutex_lock(&buttons.tapDelayMutex);
    pthread_cond_signal(&buttons.tapDelayCond);
    pthread_mutex_unlock(&buttons.tapDelayMutex);
}

static void *_button_tap_delayed_thread(void *dummyptr) {
    LOG(">>> [DELAYEDTAP] thread start ...");

    pthread_mutex_lock(&buttons.tapDelayMutex);

    do {
        pthread_cond_wait(&buttons.tapDelayCond, &buttons.tapDelayMutex);
        TOUCH_JOY_LOG(">>> [DELAYEDTAP] begin ...");

        if (UNLIKELY(isShuttingDown)) {
            break;
        }

        struct timespec ts = { .tv_sec=0, .tv_nsec=buttons.tapDelayNanos };

        // sleep for the configured delay time
        pthread_mutex_unlock(&buttons.tapDelayMutex);
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&buttons.tapDelayMutex);

        // set the emulator's joystick button values until touch up
        do {
            joy_button0 = buttons.currButtonValue0;
            joy_button1 = buttons.currButtonValue1;
            _setup_button_object_with_char(buttons.currButtonChar);

            if ( (buttons.trackingIndex == TOUCH_NONE) || isShuttingDown) {
                break;
            }
            pthread_cond_wait(&buttons.tapDelayCond, &buttons.tapDelayMutex);

            TOUCH_JOY_LOG(">>> [DELAYEDTAP] looping ...");
        } while (1);

        if (UNLIKELY(isShuttingDown)) {
            break;
        }

        // delay the ending of button tap or touch/move event by the configured delay time
        pthread_mutex_unlock(&buttons.tapDelayMutex);
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&buttons.tapDelayMutex);

        joy_button0 = 0x0;
        joy_button1 = 0x0;
        TOUCH_JOY_LOG(">>> [DELAYEDTAP] end ...");
    } while (1);

    pthread_mutex_unlock(&buttons.tapDelayMutex);

    LOG(">>> [DELAYEDTAP] thread exit ...");

    return NULL;
}

// ----------------------------------------------------------------------------

static void gltouchjoy_setup(void) {
    LOG("gltouchjoy_setup ...");

    mdlDestroyModel(&axes.model);
    mdlDestroyModel(&buttons.model);

    isShuttingDown = false;
    assert((buttons.tapDelayThreadId == 0) && "setup called multiple times!");
    pthread_create(&buttons.tapDelayThreadId, NULL, (void *)&_button_tap_delayed_thread, (void *)NULL);

    axes.model = mdlCreateQuad(-1.05, -1.0, AXIS_OBJ_W, AXIS_OBJ_H, MODEL_DEPTH, AXIS_FB_WIDTH, AXIS_FB_HEIGHT, (GLCustom){
            .create = &_create_touchjoy_hud,
            .setup = &_setup_axis_object,
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

    // button object

    buttons.model = mdlCreateQuad(1.05-BUTTON_OBJ_W, -1.0, BUTTON_OBJ_W, BUTTON_OBJ_H, MODEL_DEPTH, BUTTON_FB_WIDTH, BUTTON_FB_HEIGHT, (GLCustom){
            .create = &_create_touchjoy_hud,
            .setup = &_setup_button_object,
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

    clock_gettime(CLOCK_MONOTONIC, &axes.timingBegin);
    clock_gettime(CLOCK_MONOTONIC, &buttons.timingBegin);

    isAvailable = true;
}

static void gltouchjoy_shutdown(void) {
    LOG("gltouchjoy_shutdown ...");
    if (!isAvailable) {
        return;
    }

    isAvailable = false;

    isShuttingDown = true;
    pthread_cond_signal(&buttons.tapDelayCond);
    if (pthread_join(buttons.tapDelayThreadId, NULL)) {
        ERRLOG("OOPS: pthread_join tap delay thread ...");
    }
    buttons.tapDelayThreadId = 0;
    buttons.tapDelayMutex = (pthread_mutex_t){ 0 };
    buttons.tapDelayCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

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

    glViewport(0, 0, touchport.width, touchport.height); // NOTE : show these HUD elements beyond the A2 framebuffer dimensions

    // draw axis

    float alpha = _get_component_visibility(axes.timingBegin);
    if (alpha > 0.0) {
        glUniform1f(alphaValue, alpha);

        glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_AXIS);
        glBindTexture(GL_TEXTURE_2D, axes.model->textureName);
        if (axes.model->texDirty) {
            axes.model->texDirty = false;
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, axes.model->texWidth, axes.model->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, axes.model->texPixels);
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

    alpha = _get_component_visibility(buttons.timingBegin);
    if (alpha > 0.0) {
        glUniform1f(alphaValue, alpha);

        glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_BUTTON);
        glBindTexture(GL_TEXTURE_2D, buttons.model->textureName);
        if (buttons.model->texDirty) {
            buttons.model->texDirty = false;
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, buttons.model->texWidth, buttons.model->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, buttons.model->texPixels);
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
    touchport.axisY = 0;
    touchport.buttonY = 0;

    if (w > touchport.width) {
        touchport.width = w;
        touchport.axisXMax = w>>1;
        touchport.buttonX = w>>1;
        touchport.buttonXMax = w;
    }
    if (h > touchport.height) {
        touchport.height = h;
        touchport.axisYMax = h;
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
}

static inline void _move_joystick_axis(int x, int y) {

    x = (x - axes.centerX);
    y = (y - axes.centerY);

    if (axes.multiplier != 1.f) {
        x = (int) ((float)x * axes.multiplier);
        y = (int) ((float)y * axes.multiplier);
    }

    x += 0x80;
    y += 0x80;

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

static inline void _set_current_joy_button_values(int theButton) {
    if (theButton == TOUCH_BUTTON0) {
        buttons.currButtonValue0 = 0x80;
        buttons.currButtonValue1 = 0;
        buttons.currButtonChar = buttons.char0;
    } else if (theButton == TOUCH_BUTTON1) {
        buttons.currButtonValue0 = 0;
        buttons.currButtonValue1 = 0x80;
        buttons.currButtonChar = buttons.char1;
    } else if (theButton == TOUCH_BOTH) {
        buttons.currButtonValue0 = 0x80;
        buttons.currButtonValue1 = 0x80;
        buttons.currButtonChar = buttons.charBoth;
    } else {
        buttons.currButtonValue0 = 0;
        buttons.currButtonValue1 = 0;
    }
    _signal_tap_delay();
}

static inline void _move_button_axis(int x, int y) {

    x -= buttons.centerX;
    y -= buttons.centerY;

    if ((y < -buttons.switchThreshold) || (y > buttons.switchThreshold)) {
        int theButton = -1;
        if (y < 0) {
            theButton = buttons.northButton;
        } else {
            theButton = buttons.southButton;
        }

        _set_current_joy_button_values(theButton);
    }
}

static int64_t gltouchjoy_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {

    if (!isAvailable) {
        return 0x0LL;
    }
    if (!isEnabled) {
        return 0x0LL;
    }
    if (!ownsScreen) {
        return 0x0LL;
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
                TOUCH_JOY_LOG("---TOUCH %sDOWN (axis index %d) center:(%d,%d) -> joy(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "),
                        axes.trackingIndex, axes.centerX, axes.centerY, joy_x, joy_y);
            } else if (_is_point_on_button_side(x, y)) {
                buttonConsumed = true;
                buttons.trackingIndex = pointer_idx;
                _set_current_joy_button_values(buttons.touchDownButton);
                buttons.centerX = (int)x;
                buttons.centerY = (int)y;
                _reset_model_position(buttons.model, x, y, BUTTON_OBJ_HALF_W, BUTTON_OBJ_HALF_H);
                buttons.modelDirty = true;
                TOUCH_JOY_LOG("---TOUCH %sDOWN (buttons index %d) center:(%d,%d) -> buttons(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "),
                        buttons.trackingIndex, buttons.centerX, buttons.centerY, joy_button0, joy_button1);
            } else {
                // ...
            }
            break;

        case TOUCH_MOVE:
            if (axes.trackingIndex >= 0) {
                axisConsumed = true;
                _move_joystick_axis((int)x_coords[axes.trackingIndex], (int)y_coords[axes.trackingIndex]);
                TOUCH_JOY_LOG("---TOUCH MOVE ...tracking axis:%d (%d,%d) -> joy(0x%02X,0x%02X)", axes.trackingIndex,
                        (int)x_coords[axes.trackingIndex], (int)y_coords[axes.trackingIndex], joy_x, joy_y);
            }
            if (buttons.trackingIndex >= 0) {
                buttonConsumed = true;
                _move_button_axis((int)x_coords[buttons.trackingIndex], (int)y_coords[buttons.trackingIndex]);
                TOUCH_JOY_LOG("+++TOUCH MOVE ...tracking button:%d (%d,%d) -> buttons(0x%02X,0x%02X)", buttons.trackingIndex,
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
                axes.trackingIndex = TOUCH_NONE;
                joy_x = HALF_JOY_RANGE;
                joy_y = HALF_JOY_RANGE;
                TOUCH_JOY_LOG("---TOUCH %sUP (axis went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset buttons index!)" : ""));
            } else if (pointer_idx == buttons.trackingIndex) {
                buttonConsumed = true;
                bool resetIndex = false;
                if (axes.trackingIndex > buttons.trackingIndex) {
                    // TODO FIXME : is resetting the pointer index just an Android-ism?
                    resetIndex = true;
                    --axes.trackingIndex;
                }
                buttons.trackingIndex = TOUCH_NONE;
                _signal_tap_delay();
                TOUCH_JOY_LOG("---TOUCH %sUP (buttons went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset axis index!)" : ""));
            } else {
                // not tracking tap/gestures originating from control-gesture portion of screen
            }
            break;

        case TOUCH_CANCEL:
            LOG("---TOUCH CANCEL");
            axes.trackingIndex = TOUCH_NONE;
            buttons.trackingIndex = TOUCH_NONE;
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

    return TOUCH_FLAGS_HANDLED | TOUCH_FLAGS_JOY;
}

static bool gltouchjoy_isTouchJoystickAvailable(void) {
    return isAvailable;
}

static void gltouchjoy_setTouchJoystickEnabled(bool enabled) {
    isEnabled = enabled;
}

static void gltouchjoy_setTouchJoystickOwnsScreen(bool pwnd) {
    ownsScreen = pwnd;
    if (ownsScreen) {
        minAlpha = minAlphaWhenOwnsScreen;
    } else {
        minAlpha = 0.0;
    }
}

static bool gltouchjoy_ownsScreen(void) {
    return ownsScreen;
}

static void _animation_showTouchJoystick(void) {
    if (!isAvailable) {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &axes.timingBegin);
    clock_gettime(CLOCK_MONOTONIC, &buttons.timingBegin);
}

static void _animation_hideTouchJoystick(void) {
    if (!isAvailable) {
        return;
    }
    axes.timingBegin = (struct timespec){ 0 };
    buttons.timingBegin = (struct timespec){ 0 };
}

static void gltouchjoy_setTouchButtonValues(char char0, char char1, char charBoth) {
    buttons.char0 = char0;
    buttons.char1 = char1;
    buttons.charBoth = charBoth;
}

static void gltouchjoy_setTouchButtonTypes(touchjoy_button_type_t touchDownButton, touchjoy_button_type_t northButton, touchjoy_button_type_t southButton) {
    buttons.touchDownButton = touchDownButton;
    buttons.southButton = southButton;
    buttons.northButton = northButton;
}

static void gltouchjoy_setTapDelay(float secs) {
    if (UNLIKELY(secs < 0.f)) {
        ERRLOG("Clamping tap delay to 0.0 secs");
    }
    if (UNLIKELY(secs > 1.f)) {
        ERRLOG("Clamping tap delay to 1.0 secs");
    }
    buttons.tapDelayNanos = (unsigned int)((float)NANOSECONDS_PER_SECOND * secs);
}

static void gltouchjoy_setTouchAxisSensitivity(float multiplier) {
    axes.multiplier = multiplier;
}

static void gltouchjoy_setTouchVariant(touchjoy_variant_t axisType) {
    axes.type = axisType;
    _setup_axis_object(axes.model);
}

static touchjoy_variant_t gltouchjoy_getTouchVariant(void) {
    return axes.type;
}

static void gltouchjoy_setTouchAxisValues(char north, char west, char east, char south) {
    axes.northChar = north;
    axes.westChar  = west;
    axes.eastChar  = east;
    axes.southChar = south;
    _setup_axis_object(axes.model);
}

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_gltouchjoy(void) {
    LOG("Registering OpenGL software touch joystick");

    axes.centerX = 240;
    axes.centerY = 160;
    axes.multiplier = 1.f;
    axes.trackingIndex = TOUCH_NONE;
    axes.type = EMULATED_JOYSTICK;
    axes.northChar = MOUSETEXT_UP;
    axes.westChar  = MOUSETEXT_LEFT;
    axes.eastChar  = MOUSETEXT_RIGHT;
    axes.southChar = MOUSETEXT_DOWN;

    buttons.centerX = 240;
    buttons.centerY = 160;
    buttons.trackingIndex = TOUCH_NONE;

    buttons.touchDownButton = TOUCH_BUTTON0;
    buttons.southButton = TOUCH_BUTTON1;
    buttons.northButton = TOUCH_BOTH;
    buttons.char0 = MOUSETEXT_OPENAPPLE;
    buttons.char1 = MOUSETEXT_CLOSEDAPPLE;
    buttons.charBoth = '+';

    buttons.activeChar = MOUSETEXT_OPENAPPLE;
    buttons.switchThreshold = BUTTON_SWITCH_THRESHOLD_DEFAULT;

    buttons.tapDelayThreadId = 0;
    buttons.tapDelayMutex = (pthread_mutex_t){ 0 };
    buttons.tapDelayCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    buttons.tapDelayNanos = BUTTON_TAP_DELAY_NANOS_DEFAULT;

    video_backend->animation_showTouchJoystick = &_animation_showTouchJoystick;
    video_backend->animation_hideTouchJoystick = &_animation_hideTouchJoystick;

    joydriver_isTouchJoystickAvailable = &gltouchjoy_isTouchJoystickAvailable;
    joydriver_setTouchJoystickEnabled = &gltouchjoy_setTouchJoystickEnabled;
    joydriver_setTouchJoystickOwnsScreen = &gltouchjoy_setTouchJoystickOwnsScreen;
    joydriver_ownsScreen = &gltouchjoy_ownsScreen;
    joydriver_setTouchButtonValues = &gltouchjoy_setTouchButtonValues;
    joydriver_setTouchButtonTypes = &gltouchjoy_setTouchButtonTypes;
    joydriver_setTapDelay = &gltouchjoy_setTapDelay;
    joydriver_setTouchAxisSensitivity = &gltouchjoy_setTouchAxisSensitivity;
    joydriver_setTouchVariant = &gltouchjoy_setTouchVariant;
    joydriver_getTouchVariant = &gltouchjoy_getTouchVariant;
    joydriver_setTouchAxisValues = &gltouchjoy_setTouchAxisValues;

    glnode_registerNode(RENDER_LOW, (GLNode){
        .setup = &gltouchjoy_setup,
        .shutdown = &gltouchjoy_shutdown,
        .render = &gltouchjoy_render,
        .reshape = &gltouchjoy_reshape,
        .onTouchEvent = &gltouchjoy_onTouchEvent,
    });
}

void gldriver_joystick_reset(void) {
#warning FIXME TODO expunge this olde API ...
}

