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

#define KBD_TEMPLATE_COLS 10
#define KBD_TEMPLATE_ROWS 6

#define DEFAULT_CTRL_COL 2

#define ROW_WITH_ADJACENTS (KBD_TEMPLATE_ROWS-1)
#define _ROWOFF 2 // main keyboard row offset

#define KBD_FB_WIDTH (KBD_TEMPLATE_COLS * FONT80_WIDTH_PIXELS)
#define KBD_FB_HEIGHT (KBD_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define KBD_OBJ_W 2.0
#define KBD_OBJ_H 1.5

HUD_CLASS(GLModelHUDKeyboard,
    uint8_t *pixelsAlt; // alternate color pixels
);

static bool isAvailable = false; // Were there any OpenGL/memory errors on gltouchkbd initialization?
static bool isEnabled = true;    // Does player want touchkbd enabled?
static bool ownsScreen = false;  // Does the touchkbd currently own the screen to the exclusion?
static bool allowLowercase = false; // show lowercase keyboard
static float minAlphaWhenOwnsScreen = 1/4.f;
static float minAlpha = 0.f;

static char kbdTemplateUCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "@ @ @ @ @ ",
    "1234567890",
    "QWERTYUIOP",
    "ASDFG@HJKL",
    " ZXCVBNM @",
    "   spa. @@",
};

static char kbdTemplateLCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "@ @ @ @ @ ",
    "1234567890",
    "qwertyuiop",
    "asdfg@hjkl",
    " zxcvbnm @",
    "   SPA. @@",
};

static char kbdTemplateAlt[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "@ @ @ @ @ ",
    "1234567890",
    "@#%&*/-+()",
    "~=_<>X{}[]",
    "?!\"'`:;, X",
    "$|\\XXX.^XX",
};

static char kbdTemplateArrow[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "@ @ @ @ @ ",
    "          ",
    "    T@    ",
    "A   @@@   ",
    "    C@    ",
    "          ",
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
} touchport = { 0 };

// keyboard variables

static struct {
    GLModel *model;
    bool modelDirty; // TODO : movement animation

    int selectedCol;
    int selectedRow;

    int ctrlCol;
    int ctrlRow;

    bool ctrlPressed;

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

static inline void _switch_to_next_keyboard(void) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;
    char c = hudKeyboard->tpl[_ROWOFF*(KBD_TEMPLATE_COLS+1)];
    if (c == 'q') {
        _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
    } else if (c == 'Q') {
        _switch_keyboard(kbd.model, kbdTemplateAlt  [0]);
    } else if (allowLowercase) {
        _switch_keyboard(kbd.model, kbdTemplateLCase[0]);
    } else {
        _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
    }
}

static inline void _toggle_arrows(void) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;
    char c = hudKeyboard->tpl[_ROWOFF*(KBD_TEMPLATE_COLS+1)];
    if (c == ICONTEXT_NONACTIONABLE) {
        _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
    } else {
        _switch_keyboard(kbd.model, kbdTemplateArrow[0]);
    }
}

#warning FIXME TODO ... this can become a common helper function ...
static inline float _get_keyboard_visibility(void) {
    struct timespec now = { 0 };
    struct timespec deltat = { 0 };

    clock_gettime(CLOCK_MONOTONIC, &now);
    float alpha = minAlpha;
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

    // re-generate texture from indexed color
    const unsigned int colCount = 1;
    const unsigned int pixCharsWidth = FONT80_WIDTH_PIXELS*colCount;
    const unsigned int rowStride = hudKeyboard->pixWidth - pixCharsWidth;
    unsigned int srcIndex = (row * hudKeyboard->pixWidth * FONT_HEIGHT_PIXELS) + (col * FONT80_WIDTH_PIXELS);
    unsigned int dstIndex = srcIndex * sizeof(PIXEL_TYPE);

    for (unsigned int i=0; i<FONT_HEIGHT_PIXELS; i++) {
        for (unsigned int j=0; j<pixCharsWidth; j++) {

            uint8_t colorIndex = fb[srcIndex];
            uint32_t rgb = (((uint32_t)(colormap[colorIndex].red)   << SHIFT_R) |
                            ((uint32_t)(colormap[colorIndex].green) << SHIFT_G) |
                            ((uint32_t)(colormap[colorIndex].blue)  << SHIFT_B));
            if (rgb == 0 && hudKeyboard->blackIsTransparent) {
                // make black transparent
            } else {
                rgb |=      ((uint32_t)MAX_SATURATION               << SHIFT_A);
            }
            *( (uint32_t*)(kbd.model->texPixels + dstIndex) ) = rgb;

            srcIndex += 1;
            dstIndex += sizeof(PIXEL_TYPE);
        }
        srcIndex += rowStride;
        dstIndex = srcIndex * sizeof(PIXEL_TYPE);
    }

    kbd.model->texDirty = true;
}

static inline void _rerender_adjacents(int col, int row, uint8_t *fb) {
    if (row != ROW_WITH_ADJACENTS) {
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
    const unsigned int keyW = touchport.kbdW / hudKeyboard->tplWidth;
    const unsigned int keyH = touchport.kbdH / hudKeyboard->tplHeight;

    *col = (x - touchport.kbdX) / keyW;
    if (*col < 0) {
        *col = 0;
    } else if (*col >= hudKeyboard->tplWidth) {
        *col = hudKeyboard->tplWidth-1;
    }
    *row = (y - touchport.kbdY) / keyH;
    if (*row < 0) {
        *row = 0;
    }

    LOG("SCREEN TO KEYBOARD : kbdX:%d kbdXMax:%d kbdW:%d keyW:%d ... scrn:(%f,%f)->kybd:(%d,%d)", touchport.kbdX, touchport.kbdXMax, touchport.kbdW, keyW, x, y, *col, *row);
}

static inline void _redraw_unselected(int col, int row) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;
    if ((col >= 0) && (row >= 0)) {
        _rerender_character(col, row, hudKeyboard->pixels);
        if (row == ROW_WITH_ADJACENTS) {
            _rerender_adjacents(col, row, hudKeyboard->pixels);
        }
    }
}

static inline void _redraw_selected(int col, int row) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;
    if ((col >= 0) && (row >= 0)) {
        _rerender_character(col, row, hudKeyboard->pixelsAlt);
        if (row == ROW_WITH_ADJACENTS) {
            _rerender_adjacents(col, row, hudKeyboard->pixelsAlt);
        }
    }
}

static inline void _tap_key_at_point(float x, float y) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;

    if (!_is_point_on_keyboard(x, y)) {
        joy_button0 = 0x0;
        joy_button1 = 0x0;
        // CTRL key is "sticky"
        _redraw_unselected(kbd.selectedCol, kbd.selectedRow);
        kbd.selectedCol = -1;
        kbd.selectedRow = -1;
        return;
    }

    // get current key, col, row

    int col = -1;
    int row = -1;
    _screen_to_keyboard(x, y, &col, &row);
    const unsigned int indexRow = (hudKeyboard->tplWidth+1) * row;
    int key = (hudKeyboard->tpl+indexRow)[col];

    // handle key

    bool isASCII = false;
    bool isCTRL = false;
    switch (key) {
        case ICONTEXT_LOWERCASE:
        case ICONTEXT_UPPERCASE:
        case ICONTEXT_SHOWALT1:
            key = -1;
            _switch_to_next_keyboard();
            break;

        case ICONTEXT_NONACTIONABLE:
            key = -1;
            break;

        case ICONTEXT_CTRL:
            isCTRL = true;
            kbd.ctrlPressed = !kbd.ctrlPressed;
            _rerender_character(kbd.ctrlCol, kbd.ctrlRow, kbd.ctrlPressed ? hudKeyboard->pixelsAlt : hudKeyboard->pixels);
            col = -1;
            row = -1;
            key = SCODE_L_CTRL;
            break;

        case ICONTEXT_RETURN_L:
        case ICONTEXT_RETURN_R:
            key = SCODE_RET;
            break;

        case ICONTEXT_ESC:
            key = SCODE_ESC;
            break;

        case MOUSETEXT_LEFT:
        case ICONTEXT_BACKSPACE:
            key = SCODE_L;
            break;

        case MOUSETEXT_RIGHT:
            key = SCODE_R;
            break;

        case MOUSETEXT_UP:
            key = SCODE_U;
            break;

        case MOUSETEXT_DOWN:
            key = SCODE_D;
            break;

        case MOUSETEXT_OPENAPPLE:
            joy_button0 = joy_button0 ? 0x0 : 0x80;
            key = -1;
            break;

        case MOUSETEXT_CLOSEDAPPLE:
            joy_button1 = joy_button1 ? 0x0 : 0x80;
            key = -1;
            break;

        case ICONTEXT_MENU_SPROUT:
            key = -1;
            _toggle_arrows();
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
        if (kbd.ctrlPressed) {
            key = c_keys_ascii_to_scancode(key);
            c_keys_handle_input(key, /*pressed*/true, /*ASCII:*/false);
            c_keys_handle_input(key, /*pressed*/false, /*ASCII:*/false);
        } else {
            c_keys_handle_input(key, /*pressed:*/true,  /*ASCII:*/true);
        }
    } else if (isCTRL) {
        c_keys_handle_input(key, /*pressed:*/kbd.ctrlPressed,  /*ASCII:*/false);
    } else if (key != -1) {
        // perform a press of other keys (ESC, Arrows, etc)
        c_keys_handle_input(key, /*pressed:*/true,  /*ASCII:*/false);
        c_keys_handle_input(key, /*pressed:*/false, /*ASCII:*/false);
    }

    // redraw previous selected key
    _redraw_unselected(kbd.selectedCol, kbd.selectedRow);

    kbd.selectedCol = col;
    kbd.selectedRow = row;

    // draw current selected key
    _redraw_selected(kbd.selectedCol, kbd.selectedRow);
}

// ----------------------------------------------------------------------------
// GLCustom functions

static void _setup_touchkbd_hud(GLModel *parent) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)parent->custom;

    for (unsigned int i=0; i<(_ROWOFF-1); i++) {
        for (unsigned int j=0; j<KBD_TEMPLATE_COLS; j++) {
            kbdTemplateUCase[i][j] = ICONTEXT_NONACTIONABLE;
            kbdTemplateLCase[i][j] = ICONTEXT_NONACTIONABLE;
            kbdTemplateAlt  [i][j] = ICONTEXT_NONACTIONABLE;
        }
    }
    for (unsigned int i=0; i<KBD_TEMPLATE_ROWS; i++) {
        for (unsigned int j=0; j<KBD_TEMPLATE_COLS; j++) {
            if (kbdTemplateArrow[i][j] == ' ') {
                kbdTemplateArrow[i][j] = ICONTEXT_NONACTIONABLE;
            }
        }
    }

    kbdTemplateLCase[0][0] = ICONTEXT_UPPERCASE;
    kbdTemplateUCase[0][0] = ICONTEXT_SHOWALT1;
    kbdTemplateAlt  [0][0] = ICONTEXT_UPPERCASE;
    kbdTemplateArrow[0][0] = ICONTEXT_UPPERCASE;

    kbdTemplateUCase[0][2] = ICONTEXT_CTRL;
    kbdTemplateLCase[0][2] = ICONTEXT_CTRL;
    kbdTemplateAlt  [0][2] = ICONTEXT_CTRL;
    kbdTemplateArrow[0][2] = ICONTEXT_CTRL;

    kbdTemplateUCase[0][4] = ICONTEXT_ESC;
    kbdTemplateLCase[0][4] = ICONTEXT_ESC;
    kbdTemplateAlt  [0][4] = ICONTEXT_ESC;
    kbdTemplateArrow[0][4] = ICONTEXT_ESC;

    kbdTemplateUCase[0][6] = MOUSETEXT_OPENAPPLE;
    kbdTemplateLCase[0][6] = MOUSETEXT_OPENAPPLE;
    kbdTemplateAlt  [0][6] = MOUSETEXT_OPENAPPLE;
    kbdTemplateArrow[0][6] = MOUSETEXT_OPENAPPLE;

    kbdTemplateUCase[0][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateLCase[0][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateAlt  [0][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateArrow[0][8] = MOUSETEXT_CLOSEDAPPLE;

    kbdTemplateArrow[2][5] = MOUSETEXT_UP;
    kbdTemplateArrow[3][4] = MOUSETEXT_LEFT;
    kbdTemplateArrow[3][6] = MOUSETEXT_RIGHT;
    kbdTemplateArrow[4][5] = MOUSETEXT_DOWN;

    kbdTemplateUCase[3][5] = ICONTEXT_MENU_SPROUT;
    kbdTemplateLCase[3][5] = ICONTEXT_MENU_SPROUT;
    kbdTemplateAlt  [3][5] = ICONTEXT_MENU_SPROUT;
    kbdTemplateArrow[3][5] = ICONTEXT_MENU_SPROUT;

    kbdTemplateUCase[_ROWOFF+2][0] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+2][0] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+2][8] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+2][8] = ICONTEXT_NONACTIONABLE;
    kbdTemplateAlt  [_ROWOFF+2][8] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateLCase[_ROWOFF+2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateAlt  [_ROWOFF+2][9] = ICONTEXT_BACKSPACE;

    // last row specials

    kbdTemplateLCase[_ROWOFF+3][0] = ICONTEXT_NONACTIONABLE;
    kbdTemplateUCase[_ROWOFF+3][0] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+3][1] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+3][1] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+3][2] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+3][2] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateLCase[_ROWOFF+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateAlt  [_ROWOFF+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateUCase[_ROWOFF+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateLCase[_ROWOFF+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateAlt  [_ROWOFF+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateUCase[_ROWOFF+3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateLCase[_ROWOFF+3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateAlt  [_ROWOFF+3][5] = ICONTEXT_RIGHTSPACE;

    kbdTemplateUCase[_ROWOFF+3][7] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+3][7] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+3][8] = ICONTEXT_RETURN_L;
    kbdTemplateLCase[_ROWOFF+3][8] = ICONTEXT_RETURN_L;
    kbdTemplateAlt  [_ROWOFF+3][8] = ICONTEXT_RETURN_L;

    kbdTemplateUCase[_ROWOFF+3][9] = ICONTEXT_RETURN_R;
    kbdTemplateLCase[_ROWOFF+3][9] = ICONTEXT_RETURN_R;
    kbdTemplateAlt  [_ROWOFF+3][9] = ICONTEXT_RETURN_R;

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
        hudKeyboard->opaquePixelHalo = true;
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

    kbd.model = mdlCreateQuad(-1.0, -1.0, KBD_OBJ_W, KBD_OBJ_H, MODEL_DEPTH, KBD_FB_WIDTH, KBD_FB_HEIGHT, (GLCustom){
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
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, kbd.model->texWidth, kbd.model->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, kbd.model->texPixels);
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
        touchport.kbdY = h * (1.f - (KBD_OBJ_H/2.f));
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
    if (!ownsScreen) {
        return false;
    }

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

static void gltouchkbd_setTouchKeyboardOwnsScreen(bool pwnd) {
    ownsScreen = pwnd;
    if (ownsScreen) {
        minAlpha = minAlphaWhenOwnsScreen;
    } else {
        minAlpha = 0.0;
    }
}

static void _animation_showTouchKeyboard(void) {
    if (!isAvailable) {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &kbd.timingBegin);
}

static void _animation_hideTouchKeyboard(void) {
    if (!isAvailable) {
        return;
    }
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
    keydriver_setTouchKeyboardOwnsScreen = &gltouchkbd_setTouchKeyboardOwnsScreen;

    kbd.selectedCol = -1;
    kbd.selectedRow = -1;

    kbd.ctrlCol = DEFAULT_CTRL_COL;
    kbd.ctrlRow = 0;

    glnode_registerNode(RENDER_LOW, (GLNode){
        .setup = &gltouchkbd_setup,
        .shutdown = &gltouchkbd_shutdown,
        .render = &gltouchkbd_render,
        .reshape = &gltouchkbd_reshape,
        .onTouchEvent = &gltouchkbd_onTouchEvent,
    });
}

