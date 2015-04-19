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

#define MODEL_DEPTH -0.03125

#define KBD_TEMPLATE_COLS 10
#define KBD_TEMPLATE_ROWS 4

// HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...
#define KBD_FB_WIDTH ((KBD_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) + INTERPOLATED_PIXEL_ADJUSTMENT)
#define KBD_FB_HEIGHT (KBD_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define KBD_OBJ_W 2.0
#define KBD_OBJ_H 1.0

HUD_CLASS(GLModelHUDKeyboard,
    char *pixelsAlt; // alternate color pixels
);

static bool isAvailable = false; // Were there any OpenGL/memory errors on gltouchkbd initialization?
static bool isEnabled = true;    // Does player want touchkbd enabled?
static float minAlpha = 0.25;  // Minimum alpha value of touchkbd components (at zero, will not render)

static char kbdTemplateUCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "QWERTYUIOP",
    " ASDFGHJKL",
    "@ ZXCVBNM@",
    "@@ spa. @@",
};

static char kbdTemplateLCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "qwertyuiop",
    " asdfghjkl",
    "@ zxcvbnm@",
    "@@ SPA. @@",
};

static char kbdTemplateAlt1[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "1234567890",
    "@#%&*/-+()",
    "$?!\"'`:;,X",
    "XX\\XXX.^XX",
};

static char kbdTemplateAlt2[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "~=_<>{}[]|",
    "@#%&*/-+()",
    "$?!\"'`:;,X",
    "XX\\XXX.^XX",
};

// touch viewport

static struct {
    int width;
    int height;

    // Keyboard box
    int kbdX;
    int kbdXMax;
    int kbdY;
    int kbdYMax;

    int kbdW;
    int kbdH;

    // TODO FIXME : support 2-players!
} touchport = { 0 };

// keyboard variables

static struct {
    GLModel *model;
    bool modelDirty; // TODO : movement animation

    int selectedCol;
    int selectedRow;

    struct timespec timingBegin;
} kbd = { 0 };

// ----------------------------------------------------------------------------
// Misc internal methods

static inline void _switch_keyboard(GLModel *parent, char *template) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)parent->custom;
    memcpy(hudKeyboard->tpl, template, sizeof(kbdTemplateUCase/* assuming all the same size */));

    // setup the alternate color (AKA selected) pixels
    hudKeyboard->colorScheme = GREEN_ON_BLACK;
    glhud_setupDefault(parent);
    memcpy(hudKeyboard->pixelsAlt, hudKeyboard->pixels, (hudKeyboard->pixWidth * hudKeyboard->pixHeight));

    // setup normal color pixels
    hudKeyboard->colorScheme = RED_ON_BLACK;
    glhud_setupDefault(parent);
}

static inline void _switch_to_alt_keyboard(void) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;
    if (hudKeyboard->tpl[0] == 'Q' || hudKeyboard->tpl[0] == 'q') {
        _switch_keyboard(kbd.model, kbdTemplateAlt1[0]);
    } else if (hudKeyboard->tpl[0] == '1') {
        _switch_keyboard(kbd.model, kbdTemplateAlt2[0]);
    } else {
        _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
    }
}

static inline void _switch_to_alpha_keyboard(void) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;
    if (hudKeyboard->tpl[0] == 'Q') {
        caps_lock = false;
        _switch_keyboard(kbd.model, kbdTemplateLCase[0]);
    } else {
        caps_lock = true;
        _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
    }
}

static float _get_keyboard_visibility(void) {
    struct timespec now = { 0 };
    struct timespec deltat = { 0 };
    float alpha = minAlpha;

    clock_gettime(CLOCK_MONOTONIC, &now);
    alpha = minAlpha;
    deltat = timespec_diff(kbd.timingBegin, now, NULL);
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

static void _rerender_character(int col, int row, uint8_t *fb) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)(kbd.model->custom);

    // re-generate texture RGBA_8888 from indexed color
    const unsigned int colCount = 1;
    const unsigned int pixCharsWidth = (FONT80_WIDTH_PIXELS+1)*colCount;
    const unsigned int rowStride = hudKeyboard->pixWidth - pixCharsWidth;
    unsigned int srcIndex = (row * hudKeyboard->pixWidth * FONT_HEIGHT_PIXELS) + ((col * FONT80_WIDTH_PIXELS) + /*HACK FIXME:*/_INTERPOLATED_PIXEL_ADJUSTMENT_PRE);
    unsigned int dstIndex = srcIndex * 4;

    for (unsigned int i=0; i<FONT_HEIGHT_PIXELS; i++) {
        for (unsigned int j=0; j<pixCharsWidth; j++) {

            uint8_t colorIndex = fb[srcIndex];
            uint32_t rgb = (((uint32_t)(colormap[colorIndex].red)   << 0 ) |
                            ((uint32_t)(colormap[colorIndex].green) << 8 ) |
                            ((uint32_t)(colormap[colorIndex].blue)  << 16));
            if (rgb == 0 && hudKeyboard->blackIsTransparent) {
                // make black transparent
            } else {
                rgb |=      ((uint32_t)0xff                    << 24);
            }
            *( (uint32_t*)(kbd.model->texPixels + dstIndex) ) = rgb;

            srcIndex += 1;
            dstIndex += 4;
        }
        srcIndex += rowStride;
        dstIndex = srcIndex *4;
    }

    kbd.model->texDirty = true;
}

static inline void _rerender_adjacents(int col, int row, uint8_t *fb) {
    if (row != 3) {
        return;
    }
    if ((col == 0) || (col == 3) || (col == 4) || (col == 8)) {
        _rerender_character(col+1, row, fb);
    }
    if ((col == 1) || (col == 4) || (col == 5) || (col == 9)) {
        _rerender_character(col-1, row, fb);
    }
    if (col == 3) {
        _rerender_character(col+2, row, fb);
    }
    if (col == 5) {
        _rerender_character(col-2, row, fb);
    }
}

static inline bool _is_point_on_keyboard(float x, float y) {
    return (x >= touchport.kbdX && x <= touchport.kbdXMax && y >= touchport.kbdY && y <= touchport.kbdYMax);
}

static inline void _screen_to_keyboard(float x, float y, OUTPARM int *col, OUTPARM int *row) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)(kbd.model->custom);
    const unsigned int keyW = touchport.kbdW / (hudKeyboard->tplWidth+1/* interpolated adjustment HACK NOTE FIXME TODO */);
    const unsigned int keyH = touchport.kbdH / (hudKeyboard->tplHeight);
    const int xOff = (keyW * 0.5); // HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...

    *col = (x - (touchport.kbdX+xOff)) / keyW;
    if (*col < 0) {
        *col = 0;
    } /* interpolated adjustment HACK NOTE FIXME TODO */ else if (*col >= hudKeyboard->tplWidth) {
        *col = hudKeyboard->tplWidth-1;
    }
    *row = (y - touchport.kbdY) / keyH;
    if (*row < 0) {
        *row = 0;
    }

    LOG("SCREEN TO KEYBOARD : xOff:%d kbdX:%d kbdXMax:%d kbdW:%d keyW:%d ... scrn:(%f,%f)->kybd:(%d,%d)", xOff, touchport.kbdX, touchport.kbdXMax, touchport.kbdW, keyW, x, y, *col, *row);
}

static inline void _tap_key_at_point(float x, float y) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;

    // redraw previous selected key

    int col = kbd.selectedCol;
    int row = kbd.selectedRow;
    if ((col >= 0) && (row >= 0)) {
        _rerender_character(col, row, hudKeyboard->pixels);
        if (row == 3) {
            _rerender_adjacents(col, row, hudKeyboard->pixels);
        }
    }

    // get current key, col, row

    if (!_is_point_on_keyboard(x, y)) {
        return;
    }
    _screen_to_keyboard(x, y, &col, &row);
    const unsigned int indexRow = (hudKeyboard->tplWidth+1) * row;
    int key = (hudKeyboard->tpl+indexRow)[col];

    // draw current selected key

    if ((col >= 0) && (row >= 0)) {
        _rerender_character(col, row, hudKeyboard->pixelsAlt);
        if (row == 3) {
            _rerender_adjacents(col, row, hudKeyboard->pixelsAlt);
        }
    }
    kbd.selectedCol = col;
    kbd.selectedRow = row;

    // handle key

    bool isASCII = false;
    switch (key) {
        case ICONTEXT_LOWERCASE:
            key = -1;
            _switch_to_alpha_keyboard();
            break;

        case ICONTEXT_UPPERCASE:
            key = -1;
            _switch_to_alpha_keyboard();
            break;

        case ICONTEXT_SHOWALT1:
            key = -1;
            _switch_to_alt_keyboard();
            break;

        case ICONTEXT_SHOWALT2:
            key = -1;
            _switch_to_alt_keyboard();
            break;

        case ICONTEXT_NONACTIONABLE:
            key = -1;
            break;

        case ICONTEXT_RETURN_L:
        case ICONTEXT_RETURN_R:
            key = SCODE_RET;
            break;

        case ICONTEXT_TAB:
            key = SCODE_TAB;
            break;

        case ICONTEXT_BACKSPACE:
            key = SCODE_L;
            break;

        case ICONTEXT_LEFTSPACE:
        case ICONTEXT_MIDSPACE:
        case ICONTEXT_RIGHTSPACE:
            key = ' ';
        default: // ASCII
            isASCII = true;
            break;
    }

    assert(key < 0x80);
    if (isASCII) {
        c_keys_handle_input(key, /*pressed:*/true,  /*ASCII:*/true);
    } else {
#warning HACK FIXME TODO : handle CTRL/ALT combos
        c_keys_handle_input(key, /*pressed:*/true,  /*ASCII:*/false);
        c_keys_handle_input(key, /*pressed:*/false, /*ASCII:*/false);
    }
}

// ----------------------------------------------------------------------------
// GLCustom functions

static void _setup_touchkbd_hud(GLModel *parent) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)parent->custom;

    kbdTemplateUCase[1][0] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[1][0] = ICONTEXT_NONACTIONABLE;

    // 3rd row specials

    kbdTemplateUCase[2][0] = ICONTEXT_LOWERCASE;
    kbdTemplateLCase[2][0] = ICONTEXT_UPPERCASE;

    kbdTemplateUCase[2][1] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[2][1] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateLCase[2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateAlt1 [2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateAlt2 [2][9] = ICONTEXT_BACKSPACE;

    // 4th row specials

    kbdTemplateUCase[3][0] = ICONTEXT_SHOWALT1;
    kbdTemplateLCase[3][0] = ICONTEXT_SHOWALT1;
    kbdTemplateAlt1 [3][0] = ICONTEXT_SHOWALT1;
    kbdTemplateAlt2 [3][0] = ICONTEXT_UPPERCASE;

    kbdTemplateUCase[3][1] = ICONTEXT_SHOWALT2;
    kbdTemplateLCase[3][1] = ICONTEXT_SHOWALT2;
    kbdTemplateAlt1 [3][1] = ICONTEXT_SHOWALT2;
    kbdTemplateAlt2 [3][1] = ICONTEXT_LOWERCASE;

    kbdTemplateUCase[3][2] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[3][2] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateLCase[3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateAlt1 [3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateAlt2 [3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateUCase[3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateLCase[3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateAlt1 [3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateAlt2 [3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateUCase[3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateLCase[3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateAlt1 [3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateAlt2 [3][5] = ICONTEXT_RIGHTSPACE;

    kbdTemplateUCase[3][7] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[3][7] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[3][8] = ICONTEXT_RETURN_L;
    kbdTemplateLCase[3][8] = ICONTEXT_RETURN_L;
    kbdTemplateAlt1 [3][8] = ICONTEXT_RETURN_L;
    kbdTemplateAlt2 [3][8] = ICONTEXT_RETURN_L;

    kbdTemplateUCase[3][9] = ICONTEXT_RETURN_R;
    kbdTemplateLCase[3][9] = ICONTEXT_RETURN_R;
    kbdTemplateAlt1 [3][9] = ICONTEXT_RETURN_R;
    kbdTemplateAlt2 [3][9] = ICONTEXT_RETURN_R;

    hudKeyboard->tplWidth  = KBD_TEMPLATE_COLS;
    hudKeyboard->tplHeight = KBD_TEMPLATE_ROWS;
    hudKeyboard->pixWidth  = KBD_FB_WIDTH;
    hudKeyboard->pixHeight = KBD_FB_HEIGHT;

    const unsigned int size = sizeof(kbdTemplateUCase/* assuming all the same */);
    hudKeyboard->tpl = calloc(size, 1);
    hudKeyboard->pixels = calloc(KBD_FB_WIDTH * KBD_FB_HEIGHT, 1);
    hudKeyboard->pixelsAlt = calloc(KBD_FB_WIDTH * KBD_FB_HEIGHT, 1);

    _switch_keyboard(parent, kbdTemplateUCase[0]);
}

static void *_create_touchkbd_hud(void) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)calloc(sizeof(GLModelHUDKeyboard), 1);
    if (hudKeyboard) {
        hudKeyboard->blackIsTransparent = true;
    }
    return hudKeyboard;
}

static void _destroy_touchkbd_hud(GLModel *parent) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)parent->custom;
    if (!hudKeyboard) {
        return;
    }
    FREE(hudKeyboard->pixelsAlt);
    glhud_destroyDefault(parent);
}

// ----------------------------------------------------------------------------
// GLNode functions

static void gltouchkbd_setup(void) {
    LOG("gltouchkbd_setup ...");

    mdlDestroyModel(&kbd.model);

    kbd.model = mdlCreateQuad(-1.0, -1.0, KBD_OBJ_W, KBD_OBJ_H, MODEL_DEPTH, KBD_FB_WIDTH, KBD_FB_HEIGHT, GL_RGBA/*RGBA_8888*/, (GLCustom){
            .create = &_create_touchkbd_hud,
            .setup = &_setup_touchkbd_hud,
            .destroy = &_destroy_touchkbd_hud,
            });
    if (!kbd.model) {
        LOG("gltouchkbd initialization problem");
        return;
    }
    if (!kbd.model->custom) {
        LOG("gltouchkbd HUD initialization problem");
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &kbd.timingBegin);

    isAvailable = true;
}

static void gltouchkbd_shutdown(void) {
    LOG("gltouchkbd_shutdown ...");
    if (!isAvailable) {
        return;
    }

    isAvailable = false;

    mdlDestroyModel(&kbd.model);
}

static void gltouchkbd_render(void) {
    if (!isAvailable) {
        return;
    }
    if (!isEnabled) {
        return;
    }

    float alpha = _get_keyboard_visibility();

    if (alpha > 0.0) {
        // draw touch keyboard

        glViewport(0, 0, touchport.width, touchport.height); // NOTE : show these HUD elements beyond the A2 framebuffer dimensions
        glUniform1f(alphaValue, alpha);

        glActiveTexture(TEXTURE_ACTIVE_TOUCHKBD);
        glBindTexture(GL_TEXTURE_2D, kbd.model->textureName);
        if (kbd.model->texDirty) {
            kbd.model->texDirty = false;
            GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)(kbd.model->custom);
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, hudKeyboard->pixWidth, hudKeyboard->pixHeight, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, kbd.model->texPixels);
        }
        if (kbd.modelDirty) {
            kbd.modelDirty = false;
            glBindBuffer(GL_ARRAY_BUFFER, kbd.model->posBufferName);
            glBufferData(GL_ARRAY_BUFFER, kbd.model->positionArraySize, kbd.model->positions, GL_DYNAMIC_DRAW);
        }
        glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHKBD);
        glhud_renderDefault(kbd.model);
    }
}

static void gltouchkbd_reshape(int w, int h) {
    LOG("gltouchkbd_reshape(%d, %d)", w, h);

    touchport.kbdX = 0;

    if (w > touchport.width) {
        touchport.width = w;
        touchport.kbdXMax = w * (KBD_OBJ_W/2.f);
    }
    if (h > touchport.height) {
        touchport.height = h;
        touchport.kbdY = h * (KBD_OBJ_H/2.f);
        touchport.kbdYMax = h;
    }

    touchport.kbdW = touchport.kbdXMax - touchport.kbdX;
    touchport.kbdH = touchport.kbdYMax - touchport.kbdY;
}

static bool gltouchkbd_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {

    if (!isAvailable) {
        return false;
    }
    if (!isEnabled) {
        return false;
    }

    float alpha = _get_keyboard_visibility();
    if (alpha > 0.f) {
        float x = x_coords[pointer_idx];
        float y = y_coords[pointer_idx];

        switch (action) {
            case TOUCH_DOWN:
            case TOUCH_POINTER_DOWN:
                break;

            case TOUCH_MOVE:
                break;

            case TOUCH_UP:
            case TOUCH_POINTER_UP:
                _tap_key_at_point(x, y);
                break;

            case TOUCH_CANCEL:
                LOG("---KBD TOUCH CANCEL");
                return false;

            default:
                LOG("!!!KBD UNKNOWN TOUCH EVENT : %d", action);
                return false;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &kbd.timingBegin);

    return true;
}

// ----------------------------------------------------------------------------
// Animation and settings handling

static bool gltouchkbd_isTouchKeyboardAvailable(void) {
    return isAvailable;
}

static void gltouchkbd_setTouchKeyboardEnabled(bool enabled) {
    isEnabled = enabled;
}

static void _animation_showTouchKeyboard(void) {
    clock_gettime(CLOCK_MONOTONIC, &kbd.timingBegin);
}

static void _animation_hideTouchKeyboard(void) {
    kbd.timingBegin = (struct timespec){ 0 };
}

// ----------------------------------------------------------------------------
// Constructor

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_gltouchkbd(void) {
    LOG("Registering OpenGL software touch keyboard");

    video_backend->animation_showTouchKeyboard = &_animation_showTouchKeyboard;
    video_backend->animation_hideTouchKeyboard = &_animation_hideTouchKeyboard;

    keydriver_isTouchKeyboardAvailable = &gltouchkbd_isTouchKeyboardAvailable;
    keydriver_setTouchKeyboardEnabled = &gltouchkbd_setTouchKeyboardEnabled;

    kbd.selectedCol = -1;
    kbd.selectedRow = -1;

    glnode_registerNode(RENDER_LOW, (GLNode){
        .setup = &gltouchkbd_setup,
        .shutdown = &gltouchkbd_shutdown,
        .render = &gltouchkbd_render,
        .reshape = &gltouchkbd_reshape,
        .onTouchEvent = &gltouchkbd_onTouchEvent,
    });
}

