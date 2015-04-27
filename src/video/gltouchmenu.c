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

#define MODEL_DEPTH -1/32.f

#define MENU_TEMPLATE_COLS 2
#define MENU_TEMPLATE_ROWS 2

// HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...
#define MENU_FB_WIDTH ((MENU_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) + INTERPOLATED_PIXEL_ADJUSTMENT)
#define MENU_FB_HEIGHT (MENU_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define MENU_OBJ_W 1/2.f
#define MENU_OBJ_H 1/2.f

HUD_CLASS(GLModelHUDMenu,
    char *pixelsAlt; // alternate color pixels
);

static bool isAvailable = false; // Were there any OpenGL/memory errors on initialization?
static bool isEnabled = true;    // Does player want this enabled?
static float minAlpha = 1/4.f;   // Minimum alpha value of components (at zero, will not render)

static char topLeftTemplateHidden[MENU_TEMPLATE_ROWS][MENU_TEMPLATE_COLS+1] = {
    "++",
    "++",
};

static char topLeftTemplateShowing[MENU_TEMPLATE_ROWS][MENU_TEMPLATE_COLS+1] = {
    "++",
    "++",
};

static char topRightTemplateHidden[MENU_TEMPLATE_ROWS][MENU_TEMPLATE_COLS+1] = {
    "++",
    "++",
};

static char topRightTemplateShowing[MENU_TEMPLATE_ROWS][MENU_TEMPLATE_COLS+1] = {
    "++",
    "++",
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
    bool modelDirty; // TODO : movement animation
    bool isShowing;
} hudTopLeft = { 0 };

static struct {
    GLModel *model;
    bool modelDirty; // TODO : movement animation
    bool isShowing;
} hudTopRight = { 0 };

struct timespec timingBegin = { 0 };

// ----------------------------------------------------------------------------

static inline void _present_menu(GLModel *parent, char *template) {
    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)parent->custom;
    memcpy(hudMenu->tpl, template, sizeof(topLeftTemplateHidden/* assuming all templates the same size */));

    // setup the alternate color (AKA selected) pixels
    hudMenu->colorScheme = GREEN_ON_BLACK;
    glhud_setupDefault(parent);
    memcpy(hudMenu->pixelsAlt, hudMenu->pixels, (hudMenu->pixWidth * hudMenu->pixHeight));

    // setup normal color pixels
    hudMenu->colorScheme = RED_ON_BLACK;
    glhud_setupDefault(parent);
}

static inline void _show_top_left(void) {
    _present_menu(hudTopLeft.model, topLeftTemplateShowing[0]);
    hudTopLeft.isShowing = true;
}

static inline void _hide_top_left(void) {
    _present_menu(hudTopLeft.model, topLeftTemplateHidden[0]);
    hudTopLeft.isShowing = false;
}

static inline void _show_top_right(void) {
    _present_menu(hudTopRight.model, topRightTemplateShowing[0]);
    hudTopRight.isShowing = true;
}

static inline void _hide_top_right(void) {
    _present_menu(hudTopRight.model, topRightTemplateHidden[0]);
    hudTopRight.isShowing = false;
}

static float _get_menu_visibility(void) {
    struct timespec now = { 0 };
    struct timespec deltat = { 0 };
    float alpha = minAlpha;

    clock_gettime(CLOCK_MONOTONIC, &now);
    alpha = minAlpha;
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
    if (hudTopLeft.isShowing) {
        return (x >= touchport.topLeftX && x <= touchport.topLeftXMax && y >= touchport.topLeftY && y <= touchport.topLeftYMax);
    } else {
        return (x >= touchport.topLeftX && x <= touchport.topLeftXHalf && y >= touchport.topLeftY && y <= touchport.topLeftYHalf);
    }
}

static inline bool _is_point_on_right_menu(float x, float y) {
    if (hudTopRight.isShowing) {
        return (x >= touchport.topRightX && x <= touchport.topRightXMax && y >= touchport.topRightY && y <= touchport.topRightYMax);
    } else {
        return (x >= touchport.topRightXHalf && x <= touchport.topRightXMax && y >= touchport.topRightY && y <= touchport.topRightYHalf);
    }
}

#warning FIXME TODO : make this function generic _screen_to_model() ?
static inline void _screen_to_menu(float x, float y, OUTPARM int *col, OUTPARM int *row, OUTPARM bool *isTopLeft) {

    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)(/* assuming both have same width/height */hudTopLeft.model->custom);

    unsigned int keyW = (touchport.topLeftXMax - touchport.topLeftX) / (hudMenu->tplWidth+1/* interpolated adjustment HACK NOTE FIXME TODO */);
    unsigned int keyH = (touchport.topLeftYMax - touchport.topLeftY) / (hudMenu->tplHeight);
    const int xOff = (keyW * 0.5); // HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...

    hudMenu = NULL;
    if (x < touchport.width/2) {
        *isTopLeft = true;
        hudMenu = (GLModelHUDMenu *)hudTopLeft.model->custom;
        *col = (x - (touchport.topLeftX+xOff)) / keyW;
        *row = (y - touchport.topLeftY) / keyH;
        LOG("SCREEN TO MENU : xOff:%d topLeftX:%d topLeftXMax:%d keyW:%d ... scrn:(%d,%d)->menu:(%d,%d)", xOff, touchport.topLeftX, touchport.topLeftXMax, keyW, (int)x, (int)y, *col, *row);
    } else {
        *isTopLeft = false;
        hudMenu = (GLModelHUDMenu *)hudTopRight.model->custom;
        *col = (x - (touchport.topRightX+xOff)) / keyW;
        *row = (y - touchport.topRightY) / keyH;
        LOG("SCREEN TO MENU : xOff:%d topRightX:%d topRightXMax:%d keyW:%d ... scrn:(%d,%d)->menu:(%d,%d)", xOff, touchport.topRightX, touchport.topRightXMax,  keyW, (int)x, (int)y, *col, *row);
    }

    if (*col < 0) {
        *col = 0;
    } /* interpolated adjustment HACK NOTE FIXME TODO */ else if (*col >= hudMenu->tplWidth) {
        *col = hudMenu->tplWidth-1;
    }
    if (*row < 0) {
        *row = 0;
    }
}

static void _increase_cpu_speed(void) {
    pthread_mutex_lock(&interface_mutex);

    int percent_scale = (int)round(cpu_scale_factor * 100.0);
    if (percent_scale >= 100) {
        percent_scale += 25;
    } else {
        percent_scale += 5;
    }
    cpu_scale_factor = percent_scale/100.0;

    if (cpu_scale_factor > CPU_SCALE_FASTEST) {
        cpu_scale_factor = CPU_SCALE_FASTEST;
    }

    LOG("native set emulation percentage to %f", cpu_scale_factor);

    if (video_backend->animation_showCPUSpeed) {
        video_backend->animation_showCPUSpeed();
    }

#warning HACK TODO FIXME ... refactor timing stuff
    timing_toggle_cpu_speed();
    timing_toggle_cpu_speed();

    pthread_mutex_unlock(&interface_mutex);
}

void _decrease_cpu_speed(void) {
    pthread_mutex_lock(&interface_mutex);

    int percent_scale = (int)round(cpu_scale_factor * 100.0);
    if (cpu_scale_factor == CPU_SCALE_FASTEST) {
        cpu_scale_factor = CPU_SCALE_FASTEST0;
        percent_scale = (int)round(cpu_scale_factor * 100);
    } else {
        if (percent_scale > 100) {
            percent_scale -= 25;
        } else {
            percent_scale -= 5;
        }
    }
    cpu_scale_factor = percent_scale/100.0;

    if (cpu_scale_factor < CPU_SCALE_SLOWEST) {
        cpu_scale_factor = CPU_SCALE_SLOWEST;
    }

    LOG("native set emulation percentage to %f", cpu_scale_factor);

    if (video_backend->animation_showCPUSpeed) {
        video_backend->animation_showCPUSpeed();
    }

#warning HACK TODO FIXME ... refactor timing stuff
    timing_toggle_cpu_speed();
    timing_toggle_cpu_speed();

    pthread_mutex_unlock(&interface_mutex);
}

static inline bool _sprout_menu(float x, float y) {

    if (! (_is_point_on_left_menu(x, y) || _is_point_on_right_menu(x, y)) ) {
        return false;
    }

    bool isTopLeft = false;
    int col = -1;
    int row = -1;

    _screen_to_menu(x, y, &col, &row, &isTopLeft);

    if (isTopLeft) {

        // hide other
        _hide_top_right();

        // maybe show this one
        if (!hudTopLeft.isShowing) {
            if (col == 0 && row == 0) {
                _show_top_left();
            }
        }

        return hudTopLeft.isShowing;
    } else {

        // hide other
        _hide_top_left();

        // maybe show this one
        if (!hudTopRight.isShowing) {
            if (col == 1 && row == 0) {
                _show_top_right();
            }
        }
        return hudTopRight.isShowing;
    }
}

static inline bool _tap_menu_item(float x, float y) {
    if (! (_is_point_on_left_menu(x, y) || _is_point_on_right_menu(x, y)) ) {
        return false;
    }

    bool isTopLeft = false;
    int col = -1;
    int row = -1;

    _screen_to_menu(x, y, &col, &row, &isTopLeft);

    int selectedItem = -1;
    if (isTopLeft && hudTopLeft.isShowing) {
        selectedItem = topLeftTemplateShowing[row][col];
    } else if (!isTopLeft && hudTopRight.isShowing) {
        selectedItem = topRightTemplateShowing[row][col];
    }

    switch (selectedItem) {

        case MOUSETEXT_LEFT:
            LOG("decreasing cpu speed...");
            _decrease_cpu_speed();
            break;

        case MOUSETEXT_RIGHT:
            LOG("increasing cpu speed...");
            _increase_cpu_speed();
            break;

        case MOUSETEXT_CHECKMARK:
            LOG("showing main menu...");
            if (video_backend->hostenv_showMainMenu) {
                video_backend->hostenv_showMainMenu();
            }
            _hide_top_right();
            break;

        case ICONTEXT_MENU_TOUCHJOY:
            LOG("showing touch joystick ...");
            keydriver_setTouchKeyboardOwnsScreen(false);
            if (video_backend->animation_hideTouchKeyboard) {
                video_backend->animation_hideTouchKeyboard();
            }
            joydriver_setTouchJoystickOwnsScreen(true);
            if (video_backend->animation_showTouchJoystick) {
                video_backend->animation_showTouchJoystick();
            }
            _hide_top_left();
            break;

        case ICONTEXT_UPPERCASE:
            LOG("showing touch keyboard ...");
            joydriver_setTouchJoystickOwnsScreen(false);
            if (video_backend->animation_hideTouchJoystick) {
                video_backend->animation_hideTouchJoystick();
            }
            keydriver_setTouchKeyboardOwnsScreen(true);
            if (video_backend->animation_showTouchKeyboard) {
                video_backend->animation_showTouchKeyboard();
            }
            _hide_top_left();
            break;

        case ICONTEXT_MENU_SPROUT:
            LOG("sprout ...");
            break;

        case ICONTEXT_NONACTIONABLE:
        default:
            LOG("nonactionable ...");
            _hide_top_left();
            _hide_top_right();
            break;
    }

    return true;
}

// ----------------------------------------------------------------------------
// GLCustom functions

static void _setup_touchmenu_top_left(GLModel *parent) {
    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)parent->custom;

    hudMenu->tplWidth  = MENU_TEMPLATE_COLS;
    hudMenu->tplHeight = MENU_TEMPLATE_ROWS;
    hudMenu->pixWidth  = MENU_FB_WIDTH;
    hudMenu->pixHeight = MENU_FB_HEIGHT;

    topLeftTemplateHidden[0][0]  = ICONTEXT_MENU_SPROUT;
    topLeftTemplateHidden[0][1]  = ICONTEXT_NONACTIONABLE;
    topLeftTemplateHidden[1][0]  = ICONTEXT_NONACTIONABLE;
    topLeftTemplateHidden[1][1]  = ICONTEXT_NONACTIONABLE;

    topLeftTemplateShowing[0][0] = ICONTEXT_MENU_SPROUT;
    topLeftTemplateShowing[0][1] = MOUSETEXT_RIGHT;
    topLeftTemplateShowing[1][1] = ICONTEXT_UPPERCASE;
    topLeftTemplateShowing[1][0] = ICONTEXT_MENU_TOUCHJOY;

    const unsigned int size = sizeof(topLeftTemplateHidden);
    hudMenu->tpl = calloc(size, 1);
    hudMenu->pixels = calloc(MENU_FB_WIDTH * MENU_FB_HEIGHT, 1);
    hudMenu->pixelsAlt = calloc(MENU_FB_WIDTH * MENU_FB_HEIGHT, 1);

    _present_menu(parent, topLeftTemplateHidden[0]);
}

static void _setup_touchmenu_top_right(GLModel *parent) {
    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)parent->custom;

    hudMenu->tplWidth  = MENU_TEMPLATE_COLS;
    hudMenu->tplHeight = MENU_TEMPLATE_ROWS;
    hudMenu->pixWidth  = MENU_FB_WIDTH;
    hudMenu->pixHeight = MENU_FB_HEIGHT;

    topRightTemplateHidden[0][0]  = ICONTEXT_NONACTIONABLE;
    topRightTemplateHidden[0][1]  = ICONTEXT_MENU_SPROUT;
    topRightTemplateHidden[1][0]  = ICONTEXT_NONACTIONABLE;
    topRightTemplateHidden[1][1]  = ICONTEXT_NONACTIONABLE;

    topRightTemplateShowing[0][0] = MOUSETEXT_LEFT;
    topRightTemplateShowing[0][1] = ICONTEXT_MENU_SPROUT;
    topRightTemplateShowing[1][0] = ICONTEXT_NONACTIONABLE;
    topRightTemplateShowing[1][1] = MOUSETEXT_CHECKMARK;

    const unsigned int size = sizeof(topRightTemplateHidden);
    hudMenu->tpl = calloc(size, 1);
    hudMenu->pixels = calloc(MENU_FB_WIDTH * MENU_FB_HEIGHT, 1);
    hudMenu->pixelsAlt = calloc(MENU_FB_WIDTH * MENU_FB_HEIGHT, 1);

    _present_menu(parent, topRightTemplateHidden[0]);
}

static void *_create_touchmenu(void) {
    GLModelHUDMenu *hudMenu = (GLModelHUDMenu *)calloc(sizeof(GLModelHUDMenu), 1);
    if (hudMenu) {
        hudMenu->blackIsTransparent = true;
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

    mdlDestroyModel(&hudTopLeft.model);
    hudTopLeft.model = mdlCreateQuad(-1.0, 1.0-MENU_OBJ_H, MENU_OBJ_W, MENU_OBJ_H, MODEL_DEPTH, MENU_FB_WIDTH, MENU_FB_HEIGHT, GL_RGBA/*RGBA_8888*/, (GLCustom){
            .create = &_create_touchmenu,
            .setup = &_setup_touchmenu_top_left,
            .destroy = &_destroy_touchmenu,
            });
    if (!hudTopLeft.model) {
        LOG("gltouchmenu initialization problem");
        return;
    }
    if (!hudTopLeft.model->custom) {
        LOG("gltouchmenu HUD initialization problem");
        return;
    }

    mdlDestroyModel(&hudTopRight.model);
    hudTopRight.model = mdlCreateQuad(1.0-MENU_OBJ_W, 1.0-MENU_OBJ_H, MENU_OBJ_W, MENU_OBJ_H, MODEL_DEPTH, MENU_FB_WIDTH, MENU_FB_HEIGHT, GL_RGBA/*RGBA_8888*/, (GLCustom){
            .create = &_create_touchmenu,
            .setup = &_setup_touchmenu_top_right,
            .destroy = &_destroy_touchmenu,
            });
    if (!hudTopRight.model) {
        LOG("gltouchmenu initialization problem");
        return;
    }
    if (!hudTopRight.model->custom) {
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

    mdlDestroyModel(&hudTopLeft.model);
    mdlDestroyModel(&hudTopRight.model);
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

    // render top left sprouting menu

    glActiveTexture(TEXTURE_ACTIVE_TOUCHMENU_LEFT);
    glBindTexture(GL_TEXTURE_2D, hudTopLeft.model->textureName);
    if (hudTopLeft.model->texDirty) {
        hudTopLeft.model->texDirty = false;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, hudTopLeft.model->texWidth, hudTopLeft.model->texHeight, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, hudTopLeft.model->texPixels);
    }
    if (hudTopLeft.modelDirty) {
        hudTopLeft.modelDirty = false;
        glBindBuffer(GL_ARRAY_BUFFER, hudTopLeft.model->posBufferName);
        glBufferData(GL_ARRAY_BUFFER, hudTopLeft.model->positionArraySize, hudTopLeft.model->positions, GL_DYNAMIC_DRAW);
    }
    glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHMENU_LEFT);
    glhud_renderDefault(hudTopLeft.model);

    // render top right sprouting menu

    glActiveTexture(TEXTURE_ACTIVE_TOUCHMENU_RIGHT);
    glBindTexture(GL_TEXTURE_2D, hudTopRight.model->textureName);
    if (hudTopRight.model->texDirty) {
        hudTopRight.model->texDirty = false;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, hudTopRight.model->texWidth, hudTopRight.model->texHeight, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, hudTopRight.model->texPixels);
    }
    if (hudTopRight.modelDirty) {
        hudTopRight.modelDirty = false;
        glBindBuffer(GL_ARRAY_BUFFER, hudTopRight.model->posBufferName);
        glBufferData(GL_ARRAY_BUFFER, hudTopRight.model->positionArraySize, hudTopRight.model->positions, GL_DYNAMIC_DRAW);
    }
    glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHMENU_RIGHT);
    glhud_renderDefault(hudTopRight.model);

    GL_ERRLOG("gltouchmenu_render");
}

static void gltouchmenu_reshape(int w, int h) {
    LOG("gltouchmenu_reshape(%d, %d)", w, h);

    touchport.topLeftX = 0;
    touchport.topLeftY = 0;
    touchport.topRightY = 0;

    if (w > touchport.width) {
        const int menuPixelW = w * (MENU_OBJ_W/2.f);
        touchport.width         = w;
        touchport.topLeftXHalf  = menuPixelW/2;
        touchport.topLeftXMax   = menuPixelW;
        touchport.topRightX     = w - menuPixelW;
        touchport.topRightXHalf = w - (menuPixelW/2);
        touchport.topRightXMax  = w;
    }
    if (h > touchport.height) {
        const int menuPixelH = h * (MENU_OBJ_H/2.f);
        touchport.height = h;
        touchport.topLeftYHalf  = menuPixelH/2;
        touchport.topLeftYMax   = menuPixelH;
        touchport.topRightYHalf = menuPixelH/2;
        touchport.topRightYMax  = menuPixelH;
    }
}

static bool gltouchmenu_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {

    if (!isAvailable) {
        return false;
    }
    if (!isEnabled) {
        return false;
    }

    LOG("gltouchmenu_onTouchEvent ...");

    float x = x_coords[pointer_idx];
    float y = y_coords[pointer_idx];

    bool handled = (_is_point_on_left_menu(x, y) || _is_point_on_right_menu(x, y));

    switch (action) {
        case TOUCH_DOWN:
        case TOUCH_POINTER_DOWN:
            _sprout_menu(x, y);
            break;

        case TOUCH_MOVE:
            break;

        case TOUCH_UP:
        case TOUCH_POINTER_UP:
            _tap_menu_item(x, y);
            break;

        case TOUCH_CANCEL:
            LOG("---MENU TOUCH CANCEL");
            break;

        default:
            LOG("!!!MENU UNKNOWN TOUCH EVENT : %d", action);
            break;
    }

    if (handled) {
        clock_gettime(CLOCK_MONOTONIC, &timingBegin);
    }

    return handled;
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
    timingBegin = (struct timespec){ 0 };
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

    glnode_registerNode(RENDER_TOP, (GLNode){
        .setup = &gltouchmenu_setup,
        .shutdown = &gltouchmenu_shutdown,
        .render = &gltouchmenu_render,
        .reshape = &gltouchmenu_reshape,
        .onTouchEvent = &gltouchmenu_onTouchEvent,
    });
}

