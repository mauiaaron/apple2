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

#include "common.h"
#include "video/glvideo.h"
#include "video/glhudmodel.h"
#include "video/glnode.h"

#if !INTERFACE_TOUCH
#error this is a touch interface module, possibly you mean to not compile this at all?
#endif

#define MODEL_DEPTH -1/32.f

#define MENU_TEMPLATE_COLS 10
#define MENU_TEMPLATE_ROWS 2

#define MENU_FB_WIDTH (MENU_TEMPLATE_COLS * FONT80_WIDTH_PIXELS)
#define MENU_FB_HEIGHT (MENU_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define MENU_OBJ_W 2.0
#define MENU_OBJ_H 0.5 // NOTE : intent is to complement touch keyboard height

HUD_CLASS(GLModelHUDMenu,
    uint8_t *pixelsAlt; // alternate color pixels
);

static bool isAvailable = false; // Were there any OpenGL/memory errors on initialization?
static bool isEnabled = true;    // Does player want this enabled?
static float minAlpha = 1/4.f;   // Minimum alpha value of components (at zero, will not render)

// NOTE : intent is to match touch keyboard width
static uint8_t topMenuTemplate[MENU_TEMPLATE_ROWS][MENU_TEMPLATE_COLS+1] = {
    "++      ++",
    "++      ++",
};

// touch viewport

static struct {
    int width;
    int height;

    // top left hitbox
    int topLeftX;
    int topLeftXHalf;
    int topLeftXMax;
    int topLeftY;
    int topLeftYHalf;
    int topLeftYMax;

    // top right hitbox
    int topRightX;
    int topRightXHalf;
    int topRightXMax;
    int topRightY;
    int topRightYHalf;
    int topRightYMax;

} touchport = { 0 };

// touch menu variables

static struct {
    GLModel *model;
    bool topLeftShowing;
    bool topRightShowing;
} menu = { 0 };

static struct timespec timingBegin = { 0 };

// ----------------------------------------------------------------------------

static inline void _present_menu(GLModel *parent) {
    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)parent->custom;
    memcpy(hudMenu->tpl, topMenuTemplate, sizeof(topMenuTemplate));

    // setup the alternate color (AKA selected) pixels
    hudMenu->colorScheme = GREEN_ON_BLACK;
    glhud_setupDefault(parent);
    memcpy(hudMenu->pixelsAlt, hudMenu->pixels, (hudMenu->pixWidth * hudMenu->pixHeight));

    // setup normal color pixels
    hudMenu->colorScheme = RED_ON_BLACK;
    glhud_setupDefault(parent);
}

static inline void _show_top_left(void) {
    topMenuTemplate[0][0]  = ICONTEXT_MENU_SPROUT;
    topMenuTemplate[0][1]  = MOUSETEXT_RIGHT;

    if (joydriver_ownsScreen()) {
        topMenuTemplate[1][0] = ICONTEXT_UPPERCASE;
        if (joydriver_getTouchVariant() == EMULATED_JOYSTICK) {
            topMenuTemplate[1][1] = ICONTEXT_MENU_TOUCHJOY_KPAD;
        } else {
            topMenuTemplate[1][1] = ICONTEXT_MENU_TOUCHJOY;
        }
    } else {
        topMenuTemplate[1][0] = ICONTEXT_MENU_TOUCHJOY;
        topMenuTemplate[1][1] = ICONTEXT_MENU_TOUCHJOY_KPAD;
    }

    menu.topLeftShowing = true;
    _present_menu(menu.model);
}

static inline void _hide_top_left(void) {
    topMenuTemplate[0][0] = ICONTEXT_MENU_SPROUT;
    topMenuTemplate[0][1] = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[1][0] = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[1][1] = ICONTEXT_NONACTIONABLE;
    menu.topLeftShowing = false;
    _present_menu(menu.model);
}

static inline void _show_top_right(void) {
    topMenuTemplate[0][MENU_TEMPLATE_COLS-2] = MOUSETEXT_LEFT;
    topMenuTemplate[0][MENU_TEMPLATE_COLS-1] = ICONTEXT_MENU_SPROUT;
    topMenuTemplate[1][MENU_TEMPLATE_COLS-2] = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[1][MENU_TEMPLATE_COLS-1] = MOUSETEXT_CHECKMARK;
    menu.topRightShowing = true;
    _present_menu(menu.model);
}

static inline void _hide_top_right(void) {
    topMenuTemplate[0][MENU_TEMPLATE_COLS-2] = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[0][MENU_TEMPLATE_COLS-1] = ICONTEXT_MENU_SPROUT;
    topMenuTemplate[1][MENU_TEMPLATE_COLS-2] = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[1][MENU_TEMPLATE_COLS-1] = ICONTEXT_NONACTIONABLE;
    menu.topRightShowing = false;
    _present_menu(menu.model);
}

static float _get_menu_visibility(void) {
    struct timespec now = { 0 };
    struct timespec deltat = { 0 };
    float alpha = minAlpha;

    clock_gettime(CLOCK_MONOTONIC, &now);
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

static inline bool _is_point_on_left_menu(float x, float y) {
    if (menu.topLeftShowing) {
        return (x >= touchport.topLeftX && x <= touchport.topLeftXMax && y >= touchport.topLeftY && y <= touchport.topLeftYMax);
    } else {
        return (x >= touchport.topLeftX && x <= touchport.topLeftXHalf && y >= touchport.topLeftY && y <= touchport.topLeftYHalf);
    }
}

static inline bool _is_point_on_right_menu(float x, float y) {
    if (menu.topRightShowing) {
        return (x >= touchport.topRightX && x <= touchport.topRightXMax && y >= touchport.topRightY && y <= touchport.topRightYMax);
    } else {
        return (x >= touchport.topRightXHalf && x <= touchport.topRightXMax && y >= touchport.topRightY && y <= touchport.topRightYHalf);
    }
}

#warning FIXME TODO : make this function generic _screen_to_model() ?
static inline void _screen_to_menu(float x, float y, OUTPARM int *col, OUTPARM int *row) {

    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)(menu.model->custom);
    const unsigned int keyW = touchport.width / hudMenu->tplWidth;
    const unsigned int keyH = touchport.topLeftYMax / hudMenu->tplHeight;

    *col = x / keyW;
    if (*col < 0) {
        *col = 0;
    } else if (*col >= hudMenu->tplWidth) {
        *col = hudMenu->tplWidth-1;
    }
    *row = y / keyH;
    if (*row < 0) {
        *row = 0;
    }

    //LOG("SCREEN TO MENU : menuX:%d menuXMax:%d menuW:%d keyW:%d ... scrn:(%f,%f)->kybd:(%d,%d)", touchport.topLeftX, touchport.topLeftXMax, touchport.width, keyW, x, y, *col, *row);
}

static inline bool _sprout_menu(float x, float y) {

    if (! (_is_point_on_left_menu(x, y) || _is_point_on_right_menu(x, y)) ) {
        return false;
    }

    int col = -1;
    int row = -1;

    _screen_to_menu(x, y, &col, &row);
    bool isTopLeft = (col <= 1);
    bool isTopRight = (col >= MENU_TEMPLATE_COLS-2);

    if (isTopLeft) {

        // hide other
        _hide_top_right();

        // maybe show this one
        if (menu.topLeftShowing) {
            if (col == 0 && row == 0) {
                _hide_top_left();
            }
        } else {
            if (col == 0 && row == 0) {
                _show_top_left();
            }
        }

        return menu.topLeftShowing;
    } else if (isTopRight) {

        // hide other
        _hide_top_left();

        // maybe show this one
        if (menu.topRightShowing) {
            if (col == MENU_TEMPLATE_COLS-1 && row == 0) {
                _hide_top_right();
            }
        } else {
            if (col == MENU_TEMPLATE_COLS-1 && row == 0) {
                _show_top_right();
            }
        }
        return menu.topRightShowing;
    } else {
        RELEASE_ERRLOG("This should not happen");
        return false;
    }
}

static inline int64_t _tap_menu_item(float x, float y) {
    if (! (_is_point_on_left_menu(x, y) || _is_point_on_right_menu(x, y)) ) {
        return 0x0LL;
    }

    int col = -1;
    int row = -1;

    _screen_to_menu(x, y, &col, &row);

    uint8_t selectedItem = topMenuTemplate[row][col];

    int64_t flags = TOUCH_FLAGS_KEY_TAP;
    switch (selectedItem) {

        case MOUSETEXT_LEFT:
            LOG("decreasing cpu speed...");
            flags |= TOUCH_FLAGS_CPU_SPEED_DEC;
            break;

        case MOUSETEXT_RIGHT:
            LOG("increasing cpu speed...");
            flags |= TOUCH_FLAGS_CPU_SPEED_INC;
            break;

        case MOUSETEXT_CHECKMARK:
            LOG("showing main menu...");
            flags |= TOUCH_FLAGS_REQUEST_HOST_MENU;
            _hide_top_right();
            break;

        case ICONTEXT_MENU_TOUCHJOY:
            LOG("switching to joystick  ...");
            flags |= TOUCH_FLAGS_INPUT_DEVICE_CHANGE;
            flags |= TOUCH_FLAGS_JOY;
            _hide_top_left();
            break;

        case ICONTEXT_MENU_TOUCHJOY_KPAD:
            LOG("switching to keypad joystick  ...");
            flags |= TOUCH_FLAGS_INPUT_DEVICE_CHANGE;
            flags |= TOUCH_FLAGS_JOY_KPAD;
            _hide_top_left();
            break;

        case ICONTEXT_UPPERCASE:
            LOG("switching to keyboard  ...");
            flags |= TOUCH_FLAGS_INPUT_DEVICE_CHANGE;
            flags |= TOUCH_FLAGS_KBD;
            _hide_top_left();
            break;

        case ICONTEXT_MENU_SPROUT:
            LOG("sprout ...");
            break;

        case ICONTEXT_NONACTIONABLE:
        default:
            LOG("nonactionable ...");
            flags = 0x0LL;
            _hide_top_left();
            _hide_top_right();
            break;
    }

    return flags;
}

// ----------------------------------------------------------------------------
// GLCustom functions

static void _setup_touchmenu(GLModel *parent) {
    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)parent->custom;

    hudMenu->tplWidth  = MENU_TEMPLATE_COLS;
    hudMenu->tplHeight = MENU_TEMPLATE_ROWS;
    hudMenu->pixWidth  = MENU_FB_WIDTH;
    hudMenu->pixHeight = MENU_FB_HEIGHT;

    topMenuTemplate[0][0]  = ICONTEXT_MENU_SPROUT;
    topMenuTemplate[0][1]  = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[1][0]  = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[1][1]  = ICONTEXT_NONACTIONABLE;

    topMenuTemplate[0][MENU_TEMPLATE_COLS-2] = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[0][MENU_TEMPLATE_COLS-1] = ICONTEXT_MENU_SPROUT;
    topMenuTemplate[1][MENU_TEMPLATE_COLS-2] = ICONTEXT_NONACTIONABLE;
    topMenuTemplate[1][MENU_TEMPLATE_COLS-1] = ICONTEXT_NONACTIONABLE;

    for (unsigned int row=0; row<MENU_TEMPLATE_ROWS; row++) {
        for (unsigned int col=2; col<MENU_TEMPLATE_COLS-2; col++) {
            topMenuTemplate[row][col] = ICONTEXT_NONACTIONABLE;
        }
    }

    const unsigned int size = sizeof(topMenuTemplate);
    hudMenu->tpl = calloc(size, 1);
    hudMenu->pixels = calloc(MENU_FB_WIDTH * MENU_FB_HEIGHT, 1);
    hudMenu->pixelsAlt = calloc(MENU_FB_WIDTH * MENU_FB_HEIGHT, 1);

    _present_menu(parent);
}

static void *_create_touchmenu(void) {
    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)calloc(sizeof(GLModelHUDMenu), 1);
    if (hudMenu) {
        hudMenu->blackIsTransparent = true;
        hudMenu->opaquePixelHalo = true;
    }
    return hudMenu;
}

static void _destroy_touchmenu(GLModel *parent) {
    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)parent->custom;
    if (!hudMenu) {
        return;
    }
    FREE(hudMenu->pixelsAlt);
    glhud_destroyDefault(parent);
}

// ----------------------------------------------------------------------------
// GLNode functions

static void gltouchmenu_setup(void) {
    LOG("gltouchmenu_setup ...");

    mdlDestroyModel(&menu.model);
    menu.model = mdlCreateQuad(-1.0, 1.0-MENU_OBJ_H, MENU_OBJ_W, MENU_OBJ_H, MODEL_DEPTH, MENU_FB_WIDTH, MENU_FB_HEIGHT, (GLCustom){
            .create = &_create_touchmenu,
            .setup = &_setup_touchmenu,
            .destroy = &_destroy_touchmenu,
            });
    if (!menu.model) {
        LOG("gltouchmenu initialization problem");
        return;
    }
    if (!menu.model->custom) {
        LOG("gltouchmenu HUD initialization problem");
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &timingBegin);

    isAvailable = true;

    GL_ERRLOG("gltouchmenu_setup");
}

static void gltouchmenu_shutdown(void) {
    LOG("gltouchmenu_shutdown ...");
    if (!isAvailable) {
        return;
    }

    isAvailable = false;

    mdlDestroyModel(&menu.model);
}

static void gltouchmenu_render(void) {
    if (!isAvailable) {
        return;
    }
    if (!isEnabled) {
        return;
    }

    float alpha = _get_menu_visibility();
    if (alpha <= 0.0) {
        return;
    }

    glViewport(0, 0, touchport.width, touchport.height); // NOTE : show these HUD elements beyond the A2 framebuffer dimensions
    glUniform1f(alphaValue, alpha);

    // render top sprouting menu(s)

    glActiveTexture(TEXTURE_ACTIVE_TOUCHMENU);
    glBindTexture(GL_TEXTURE_2D, menu.model->textureName);
    if (menu.model->texDirty) {
        menu.model->texDirty = false;
        _HACKAROUND_GLTEXIMAGE2D_PRE(TEXTURE_ACTIVE_TOUCHMENU, menu.model->textureName);
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, menu.model->texWidth, menu.model->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, menu.model->texPixels);
    }
    glUniform1i(texSamplerLoc, TEXTURE_ID_TOUCHMENU);
    glhud_renderDefault(menu.model);

    GL_ERRLOG("gltouchmenu_render");
}

static void gltouchmenu_reshape(int w, int h) {
    LOG("gltouchmenu_reshape(%d, %d)", w, h);

    touchport.topLeftX = 0;
    touchport.topLeftY = 0;
    touchport.topRightY = 0;

    if (w > touchport.width) {
        touchport.width         = w;
        const unsigned int keyW = touchport.width / MENU_TEMPLATE_COLS;
        touchport.topLeftXHalf  = keyW;
        touchport.topLeftXMax   = keyW*2;
        touchport.topRightX     = w - (keyW*2);
        touchport.topRightXHalf = w - keyW;
        touchport.topRightXMax  = w;
    }
    if (h > touchport.height) {
        touchport.height        = h;
        const unsigned int menuH = h * (MENU_OBJ_H/2.0);
        touchport.topLeftYHalf  = menuH/2;
        touchport.topLeftYMax   = menuH;
        touchport.topRightYHalf = menuH/2;
        touchport.topRightYMax  = menuH;
    }
}

static int64_t gltouchmenu_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {

    if (!isAvailable) {
        return 0x0;
    }
    if (!isEnabled) {
        return 0x0;
    }

    //LOG("gltouchmenu_onTouchEvent ...");

    float x = x_coords[pointer_idx];
    float y = y_coords[pointer_idx];

    bool handled = (_is_point_on_left_menu(x, y) || _is_point_on_right_menu(x, y));
    int flags = TOUCH_FLAGS_MENU;

    switch (action) {
        case TOUCH_DOWN:
        case TOUCH_POINTER_DOWN:
            _sprout_menu(x, y);
            break;

        case TOUCH_MOVE:
            break;

        case TOUCH_UP:
        case TOUCH_POINTER_UP:
            flags |= _tap_menu_item(x, y);
            break;

        case TOUCH_CANCEL:
            LOG("---MENU TOUCH CANCEL");
            return 0x0;

        default:
            LOG("!!!MENU UNKNOWN TOUCH EVENT : %d", action);
            return 0x0;
    }

    if (handled) {
        clock_gettime(CLOCK_MONOTONIC, &timingBegin);
        flags |= TOUCH_FLAGS_HANDLED;
    }

    return flags;
}

// ----------------------------------------------------------------------------
// Animation and settings handling

static bool gltouchmenu_isTouchMenuAvailable(void) {
    return isAvailable;
}

static void gltouchmenu_setTouchMenuEnabled(bool enabled) {
    isEnabled = enabled;
}

static void _animation_showTouchMenu(void) {
    clock_gettime(CLOCK_MONOTONIC, &timingBegin);
}

static void _animation_hideTouchMenu(void) {
    _hide_top_left();
    _hide_top_right();
    timingBegin = (struct timespec){ 0 };
}

static void gltouchmenu_setTouchMenuVisibility(float alpha) {
    minAlpha = alpha;
}

// ----------------------------------------------------------------------------
// Constructor

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_gltouchmenu(void) {
    LOG("Registering OpenGL software touch menu");

    video_backend->animation_showTouchMenu = &_animation_showTouchMenu;
    video_backend->animation_hideTouchMenu = &_animation_hideTouchMenu;

    interface_isTouchMenuAvailable = &gltouchmenu_isTouchMenuAvailable;
    interface_setTouchMenuEnabled = &gltouchmenu_setTouchMenuEnabled;
    interface_setTouchMenuVisibility = &gltouchmenu_setTouchMenuVisibility;

    glnode_registerNode(RENDER_TOP, (GLNode){
        .setup = &gltouchmenu_setup,
        .shutdown = &gltouchmenu_shutdown,
        .render = &gltouchmenu_render,
        .reshape = &gltouchmenu_reshape,
        .onTouchEvent = &gltouchmenu_onTouchEvent,
    });
}

