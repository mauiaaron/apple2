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

#include "video/gltouchjoy.h"

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

// ----------------------------------------------------------------------------

#warning FIXME TODO ... this can become a common helper function ...
static inline float _get_component_visibility(struct timespec timingBegin) {
    struct timespec now = { 0 };
    struct timespec deltat = { 0 };

    clock_gettime(CLOCK_MONOTONIC, &now);
    float alpha = joyglobals.minAlpha;
    deltat = timespec_diff(timingBegin, now, NULL);
    if (deltat.tv_sec == 0) {
        alpha = 1.0;
        if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
            alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
            if (alpha < joyglobals.minAlpha) {
                alpha = joyglobals.minAlpha;
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

    if (!joyglobals.showControls) {
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

    for (unsigned int i=0; i<ROSETTE_ROWS; i++) {
        for (unsigned int j=0; j<ROSETTE_COLS; j++) {
            ((hudElement->tpl)+(row*i*2))[j*2] = axes.rosetteChars[(i*ROSETTE_ROWS)+j];
        }
    }

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

        if (UNLIKELY(joyglobals.isShuttingDown)) {
            break;
        }

        struct timespec ts = { .tv_sec=0, .tv_nsec=buttons.tapDelayNanos };

        // sleep for the configured delay time
        pthread_mutex_unlock(&buttons.tapDelayMutex);
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&buttons.tapDelayMutex);

        // wait until touch up/cancel
        do {

            // now set the emulator's joystick button values (or keypad value)
            uint8_t displayChar = variant.curr->buttonPress();
            _setup_button_object_with_char(displayChar);

            if ( (buttons.trackingIndex == TOUCH_NONE) || joyglobals.isShuttingDown) {
                break;
            }

            pthread_cond_wait(&buttons.tapDelayCond, &buttons.tapDelayMutex);

            if ( (buttons.trackingIndex == TOUCH_NONE) || joyglobals.isShuttingDown) {
                break;
            }
            TOUCH_JOY_LOG(">>> [DELAYEDTAP] looping ...");
        } while (1);

        if (UNLIKELY(joyglobals.isShuttingDown)) {
            break;
        }

        // delay the ending of button tap or touch/move event by the configured delay time
        pthread_mutex_unlock(&buttons.tapDelayMutex);
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&buttons.tapDelayMutex);

        variant.curr->buttonRelease();

        TOUCH_JOY_LOG(">>> [DELAYEDTAP] end ...");
    } while (1);

    pthread_mutex_unlock(&buttons.tapDelayMutex);

    LOG(">>> [DELAYEDTAP] thread exit ...");

    return NULL;
}

// ----------------------------------------------------------------------------

static void gltouchjoy_setup(void) {
    LOG("gltouchjoy_setup ...");

    variant.kpad->resetState();
    variant.joys->resetState();

    mdlDestroyModel(&axes.model);
    mdlDestroyModel(&buttons.model);

    joyglobals.isShuttingDown = false;
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

    variant.kpad->resetState();
    variant.joys->resetState();

    joyglobals.isAvailable = false;

    joyglobals.isShuttingDown = true;
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

    if (joyglobals.axisIsOnLeft) {
        touchport.axisX = 0;
        touchport.axisY = 0;
        touchport.buttonY = 0;

        if (w >= touchport.width) {
            touchport.width = w;
            touchport.axisXMax = (w * joyglobals.screenDivider);
            touchport.buttonX = (w * joyglobals.screenDivider);
            touchport.buttonXMax = w;
        }
        if (h >= touchport.height) {
            touchport.height = h;
            touchport.axisYMax = h;
            touchport.buttonYMax = h;
        }
    } else {
        touchport.buttonX = 0;
        touchport.buttonY = 0;
        touchport.axisY = 0;

        if (w >= touchport.width) {
            touchport.width = w;
            touchport.buttonXMax = (w * joyglobals.screenDivider);
            touchport.axisX = (w * joyglobals.screenDivider);
            touchport.axisXMax = w;
        }
        if (h >= touchport.height) {
            touchport.height = h;
            touchport.buttonYMax = h;
            touchport.axisYMax = h;
        }
    }
}

static void gltouchjoy_resetJoystick(void) {
    // no-op
}

static inline bool _is_point_on_axis_side(int x, int y) {
    return (x >= touchport.axisX && x <= touchport.axisXMax && y >= touchport.axisY && y <= touchport.axisYMax);
}

static inline bool _is_point_on_button_side(int x, int y) {
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

static inline void _axis_touch_down(int x, int y) {
    axes.centerX = x;
    axes.centerY = y;

    _reset_model_position(axes.model, x, y, AXIS_OBJ_HALF_W, AXIS_OBJ_HALF_H);
    axes.modelDirty = true;

    TOUCH_JOY_LOG("---TOUCH %sDOWN (axis index %d) center:(%d,%d) -> joy(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "), axes.trackingIndex, axes.centerX, axes.centerY, joy_x, joy_y);
    variant.curr->axisDown();
}

static inline void _button_touch_down(int x, int y) {
    variant.curr->setCurrButtonValue(buttons.touchDownChar, buttons.touchDownScancode);
    _signal_tap_delay();

    buttons.centerX = x;
    buttons.centerY = y;

    _reset_model_position(buttons.model, x, y, BUTTON_OBJ_HALF_W, BUTTON_OBJ_HALF_H);
    buttons.modelDirty = true;

    TOUCH_JOY_LOG("---TOUCH %sDOWN (buttons index %d) center:(%d,%d) -> buttons(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "), buttons.trackingIndex, buttons.centerX, buttons.centerY, joy_button0, joy_button1);
}

static inline void _axis_move(int x, int y) {
    x = (x - axes.centerX);
    y = (y - axes.centerY);
    TOUCH_JOY_LOG("---TOUCH MOVE ...tracking axis:%d (%d,%d) -> joy(0x%02X,0x%02X)", axes.trackingIndex, x, y, joy_x, joy_y);
    variant.curr->axisMove(x, y);
}

static inline void _button_move(int x, int y) {
    x -= buttons.centerX;
    y -= buttons.centerY;
    if ((y < -joyglobals.switchThreshold) || (y > joyglobals.switchThreshold)) {
        touchjoy_button_type_t theButtonChar = -1;
        int theButtonScancode = -1;
        if (y < 0) {
            theButtonChar = buttons.northChar;
            theButtonScancode = buttons.northScancode;
        } else {
            theButtonChar = buttons.southChar;
            theButtonScancode = buttons.southScancode;
        }

        variant.curr->setCurrButtonValue(theButtonChar, theButtonScancode);
        _signal_tap_delay();
        TOUCH_JOY_LOG("+++TOUCH MOVE ...tracking button:%d (%d,%d) -> buttons(0x%02X,0x%02X)", buttons.trackingIndex, x, y, joy_button0, joy_button1);
    }
}

static inline void _axis_touch_up(int x, int y) {
    x = (x - axes.centerX);
    y = (y - axes.centerY);
    if (buttons.trackingIndex > axes.trackingIndex) {
        --buttons.trackingIndex;
    }
    variant.curr->axisUp(x, y);
#if DEBUG_TOUCH_JOY
    bool resetIndex = false;
    if (buttons.trackingIndex > axes.trackingIndex) {
        // TODO FIXME : is resetting the pointer index just an Android-ism?
        resetIndex = true;
    }
    TOUCH_JOY_LOG("---TOUCH %sUP (axis went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset buttons index!)" : ""));
#endif
    axes.trackingIndex = TOUCH_NONE;
}

static inline void _button_touch_up(void) {
#if DEBUG_TOUCH_JOY
    bool resetIndex = false;
    if (axes.trackingIndex > buttons.trackingIndex) {
        // TODO FIXME : is resetting the pointer index just an Android-ism?
        resetIndex = true;
    }
    TOUCH_JOY_LOG("---TOUCH %sUP (buttons went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset axis index!)" : ""));
#endif
    if (axes.trackingIndex > buttons.trackingIndex) {
        --axes.trackingIndex;
    }
    buttons.trackingIndex = TOUCH_NONE;
    _signal_tap_delay();
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
            {
                int x = (int)x_coords[pointer_idx];
                int y = (int)y_coords[pointer_idx];
                if (_is_point_on_axis_side(x, y)) {
                    axisConsumed = true;
                    axes.trackingIndex = pointer_idx;
                    _axis_touch_down(x, y);
                } else if (_is_point_on_button_side(x, y)) {
                    buttonConsumed = true;
                    buttons.trackingIndex = pointer_idx;
                    _button_touch_down(x, y);
                } else {
                    assert(false && "should either be on axis or button side");
                }
            }
            break;

        case TOUCH_MOVE:
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
            if (pointer_idx == axes.trackingIndex) {
                int x = (int)x_coords[axes.trackingIndex];
                int y = (int)y_coords[axes.trackingIndex];
                _axis_touch_up(x, y);
            } else if (pointer_idx == buttons.trackingIndex) {
                _button_touch_up();
            } else {
                if (pointer_count == 1) {
                    // last pointer up completely resets state
                    LOG("!!!! ... RESETTING TOUCH JOYSTICK STATE MACHINE");
                    variant.joys->resetState();
                    variant.kpad->resetState();
                }
            }
            break;

        case TOUCH_CANCEL:
            LOG("---TOUCH CANCEL");
            variant.joys->resetState();
            variant.kpad->resetState();
            break;

        default:
            LOG("!!! UNKNOWN TOUCH EVENT : %d", action);
            break;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (axisConsumed) {
        axes.timingBegin = now;
    }
    if (buttonConsumed) {
        buttons.timingBegin = now;
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
    if (joyglobals.ownsScreen) {
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

static void _animation_showTouchJoystick(void) {
    if (!joyglobals.isAvailable) {
        return;
    }

    int x = touchport.axisX + ((touchport.axisXMax - touchport.axisX)/2);
    int y = touchport.axisY + ((touchport.axisYMax - touchport.axisY)/2);
    _reset_model_position(axes.model, x, y, AXIS_OBJ_HALF_W, AXIS_OBJ_HALF_H);
    axes.modelDirty = true;

    x = touchport.buttonX + ((touchport.buttonXMax - touchport.buttonX)/2);
    y = touchport.buttonY + ((touchport.buttonYMax - touchport.buttonY)/2);
    _reset_model_position(buttons.model, x, y, BUTTON_OBJ_HALF_W, BUTTON_OBJ_HALF_H);
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
        currButtonDisplayChar = '+';
    }
    _setup_button_object_with_char(currButtonDisplayChar);
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

static void gltouchjoy_setButtonSwitchThreshold(int delta) {
    joyglobals.switchThreshold = delta;
}

static void gltouchjoy_setTouchVariant(touchjoy_variant_t variantType) {
    variant.curr->resetState();

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

    variant.curr->resetState();
}

static touchjoy_variant_t gltouchjoy_getTouchVariant(void) {
    return variant.curr->variant();
}

static void gltouchjoy_setTouchAxisTypes(uint8_t rosetteChars[(ROSETTE_ROWS * ROSETTE_COLS)], int rosetteScancodes[(ROSETTE_ROWS * ROSETTE_COLS)]) {
    memcpy(axes.rosetteChars,     rosetteChars,     sizeof(uint8_t)*(ROSETTE_ROWS * ROSETTE_COLS));
    memcpy(axes.rosetteScancodes, rosetteScancodes, sizeof(int)    *(ROSETTE_ROWS * ROSETTE_COLS));
    _setup_axis_object(axes.model);
}

static void gltouchjoy_setScreenDivision(float screenDivider) {
    joyglobals.screenDivider = screenDivider;
    // force reshape here to apply changes ...
    gltouchjoy_reshape(touchport.width, touchport.height);
}

static void gltouchjoy_setAxisOnLeft(bool axisIsOnLeft) {
    joyglobals.axisIsOnLeft = axisIsOnLeft;
    // force reshape here to apply changes ...
    gltouchjoy_reshape(touchport.width, touchport.height);
}

static void gltouchjoy_beginCalibration(void) {
    video_clear();
    joyglobals.isCalibrating = true;
}

static void gltouchjoy_endCalibration(void) {
    video_redraw();
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
    axes.trackingIndex = TOUCH_NONE;

    axes.rosetteChars[0]     = ' ';
    axes.rosetteScancodes[0] = -1;
    axes.rosetteChars[1]     = MOUSETEXT_UP;
    axes.rosetteScancodes[1] = -1;
    axes.rosetteChars[2]     = ' ';
    axes.rosetteScancodes[2] = -1;

    axes.rosetteChars[3]     = MOUSETEXT_LEFT;
    axes.rosetteScancodes[3] = -1;
    axes.rosetteChars[4]     = '+';
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
    buttons.trackingIndex = TOUCH_NONE;

    buttons.touchDownChar = TOUCH_BUTTON0;
    buttons.touchDownScancode = -1;

    buttons.southChar = TOUCH_BUTTON1;
    buttons.southScancode = -1;

    buttons.northChar = TOUCH_BOTH;
    buttons.northScancode = -1;

    buttons.activeChar = MOUSETEXT_OPENAPPLE;

    buttons.tapDelayThreadId = 0;
    buttons.tapDelayMutex = (pthread_mutex_t){ 0 };
    buttons.tapDelayCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    buttons.tapDelayNanos = BUTTON_TAP_DELAY_NANOS_DEFAULT;

    joyglobals.isEnabled = true;
    joyglobals.ownsScreen = true;
    joyglobals.showControls = true;
    joyglobals.screenDivider = 0.5f;
    joyglobals.axisIsOnLeft = true;
    joyglobals.switchThreshold = BUTTON_SWITCH_THRESHOLD_DEFAULT;

    video_backend->animation_showTouchJoystick = &_animation_showTouchJoystick;
    video_backend->animation_hideTouchJoystick = &_animation_hideTouchJoystick;

    joydriver_isTouchJoystickAvailable = &gltouchjoy_isTouchJoystickAvailable;
    joydriver_setTouchJoystickEnabled = &gltouchjoy_setTouchJoystickEnabled;
    joydriver_setTouchJoystickOwnsScreen = &gltouchjoy_setTouchJoystickOwnsScreen;
    joydriver_ownsScreen = &gltouchjoy_ownsScreen;
    joydriver_setShowControls = &gltouchjoy_setShowControls;
    joydriver_setTouchButtonTypes = &gltouchjoy_setTouchButtonTypes;
    joydriver_setTapDelay = &gltouchjoy_setTapDelay;
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
        .setup = &gltouchjoy_setup,
        .shutdown = &gltouchjoy_shutdown,
        .render = &gltouchjoy_render,
        .reshape = &gltouchjoy_reshape,
        .onTouchEvent = &gltouchjoy_onTouchEvent,
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

