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
#define KBD_TEMPLATE_ROWS 7

#define ROW_WITH_ADJACENTS (KBD_TEMPLATE_ROWS-1)
#define _ROWOFF 3 // actual keyboard row offset

// HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...
#define KBD_FB_WIDTH ((KBD_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) + INTERPOLATED_PIXEL_ADJUSTMENT)
#define KBD_FB_HEIGHT (KBD_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

#define KBD_OBJ_W 2.0
#define KBD_OBJ_H 1.75

HUD_CLASS(GLModelHUDKeyboard,
    char *pixelsAlt; // alternate color pixels
);

static bool isAvailable = false; // Were there any OpenGL/memory errors on gltouchkbd initialization?
static bool isEnabled = true;    // Does player want touchkbd enabled?
static bool ownsScreen = true;   // Does the touchkbd currently own the screen to the exclusion?
static bool allowLowercase = true; // show lowercase keyboard
static float minAlphaWhenOwnsScreen = 1/4.f;
static float minAlpha = 1/4.f;

static char kbdTemplateUCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "    .     ",
    "@  . .  @ ",
    "    .     ",
    "QWERTYUIOP",
    " ASDFGHJKL",
    "@ ZXCVBNM@",
    "@@ spa. @@",
};

static char kbdTemplateLCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "    .     ",
    "@  . .  @ ",
    "    .     ",
    "qwertyuiop",
    " asdfghjkl",
    "@ zxcvbnm@",
    "@@ SPA. @@",
};

static char kbdTemplateAlt1[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "    .     ",
    "@  . .  @ ",
    "    .     ",
    "1234567890",
    "@#%&*/-+()",
    "$?!\"'`:;,X",
    "XX\\XXX.^XX",
};

static char kbdTemplateAlt2[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "    .     ",
    "@  . .  @ ",
    "    .     ",
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

static inline void _switch_to_next_keyboard(void) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;
    char c = hudKeyboard->tpl[_ROWOFF*(KBD_TEMPLATE_COLS+1)];
    if (c == 'q') {
        _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
    } else if (c == 'Q') {
        _switch_keyboard(kbd.model, kbdTemplateAlt1[0]);
    } else if (c == '1') {
        _switch_keyboard(kbd.model, kbdTemplateAlt2[0]);
    } else if (allowLowercase) {
        _switch_keyboard(kbd.model, kbdTemplateLCase[0]);
    } else {
        _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
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
        if (row == ROW_WITH_ADJACENTS) {
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
        if (row == ROW_WITH_ADJACENTS) {
            _rerender_adjacents(col, row, hudKeyboard->pixelsAlt);
        }
    }
    kbd.selectedCol = col;
    kbd.selectedRow = row;

    // handle key
    joy_button0 = 0x0;
    joy_button1 = 0x0;

    bool isASCII = false;
    switch (key) {
        case ICONTEXT_LOWERCASE:
        case ICONTEXT_UPPERCASE:
        case ICONTEXT_SHOWALT1:
        case ICONTEXT_SHOWALT2:
            key = -1;
            _switch_to_next_keyboard();
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
            joy_button0 = 0x80;
            key = -1;
            break;

        case MOUSETEXT_CLOSEDAPPLE:
            joy_button1 = 0x80;
            key = -1;
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

    for (unsigned int i=0; i<_ROWOFF; i++) {
        for (unsigned int j=0; j<KBD_TEMPLATE_COLS; j++) {
            kbdTemplateUCase[i][j] = ICONTEXT_NONACTIONABLE;
            kbdTemplateLCase[i][j] = ICONTEXT_NONACTIONABLE;
            kbdTemplateAlt1 [i][j] = ICONTEXT_NONACTIONABLE;
            kbdTemplateAlt2 [i][j] = ICONTEXT_NONACTIONABLE;
        }
    }

    kbdTemplateUCase[0][4] = MOUSETEXT_UP;
    kbdTemplateLCase[0][4] = MOUSETEXT_UP;
    kbdTemplateAlt1 [0][4] = MOUSETEXT_UP;
    kbdTemplateAlt2 [0][4] = MOUSETEXT_UP;

    kbdTemplateUCase[1][3] = MOUSETEXT_LEFT;
    kbdTemplateLCase[1][3] = MOUSETEXT_LEFT;
    kbdTemplateAlt1 [1][3] = MOUSETEXT_LEFT;
    kbdTemplateAlt2 [1][3] = MOUSETEXT_LEFT;
    kbdTemplateUCase[1][5] = MOUSETEXT_RIGHT;
    kbdTemplateLCase[1][5] = MOUSETEXT_RIGHT;
    kbdTemplateAlt1 [1][5] = MOUSETEXT_RIGHT;
    kbdTemplateAlt2 [1][5] = MOUSETEXT_RIGHT;

    kbdTemplateUCase[1][0] = MOUSETEXT_OPENAPPLE;
    kbdTemplateLCase[1][0] = MOUSETEXT_OPENAPPLE;
    kbdTemplateAlt1 [1][0] = MOUSETEXT_OPENAPPLE;
    kbdTemplateAlt2 [1][0] = MOUSETEXT_OPENAPPLE;
    kbdTemplateUCase[1][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateLCase[1][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateAlt1 [1][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateAlt2 [1][8] = MOUSETEXT_CLOSEDAPPLE;

    kbdTemplateUCase[2][4] = MOUSETEXT_DOWN;
    kbdTemplateLCase[2][4] = MOUSETEXT_DOWN;
    kbdTemplateAlt1 [2][4] = MOUSETEXT_DOWN;
    kbdTemplateAlt2 [2][4] = MOUSETEXT_DOWN;

    // 2nd row specials

    kbdTemplateUCase[_ROWOFF+1][0] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+1][0] = ICONTEXT_NONACTIONABLE;

    // 3rd row specials

    kbdTemplateUCase[_ROWOFF+2][0] = ICONTEXT_LOWERCASE;
    kbdTemplateLCase[_ROWOFF+2][0] = ICONTEXT_UPPERCASE;

    kbdTemplateUCase[_ROWOFF+2][1] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+2][1] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateLCase[_ROWOFF+2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateAlt1 [_ROWOFF+2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateAlt2 [_ROWOFF+2][9] = ICONTEXT_BACKSPACE;

    // 4th row specials

    kbdTemplateUCase[_ROWOFF+3][0] = ICONTEXT_SHOWALT1;
    kbdTemplateLCase[_ROWOFF+3][0] = ICONTEXT_SHOWALT1;
    kbdTemplateAlt1 [_ROWOFF+3][0] = ICONTEXT_SHOWALT1;
    kbdTemplateAlt2 [_ROWOFF+3][0] = ICONTEXT_UPPERCASE;

    kbdTemplateUCase[_ROWOFF+3][1] = ICONTEXT_SHOWALT2;
    kbdTemplateLCase[_ROWOFF+3][1] = ICONTEXT_SHOWALT2;
    kbdTemplateAlt1 [_ROWOFF+3][1] = ICONTEXT_SHOWALT2;
    kbdTemplateAlt2 [_ROWOFF+3][1] = ICONTEXT_LOWERCASE;

    kbdTemplateUCase[_ROWOFF+3][2] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+3][2] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateLCase[_ROWOFF+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateAlt1 [_ROWOFF+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateAlt2 [_ROWOFF+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateUCase[_ROWOFF+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateLCase[_ROWOFF+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateAlt1 [_ROWOFF+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateAlt2 [_ROWOFF+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateUCase[_ROWOFF+3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateLCase[_ROWOFF+3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateAlt1 [_ROWOFF+3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateAlt2 [_ROWOFF+3][5] = ICONTEXT_RIGHTSPACE;

    kbdTemplateUCase[_ROWOFF+3][7] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[_ROWOFF+3][7] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[_ROWOFF+3][8] = ICONTEXT_RETURN_L;
    kbdTemplateLCase[_ROWOFF+3][8] = ICONTEXT_RETURN_L;
    kbdTemplateAlt1 [_ROWOFF+3][8] = ICONTEXT_RETURN_L;
    kbdTemplateAlt2 [_ROWOFF+3][8] = ICONTEXT_RETURN_L;

    kbdTemplateUCase[_ROWOFF+3][9] = ICONTEXT_RETURN_R;
    kbdTemplateLCase[_ROWOFF+3][9] = ICONTEXT_RETURN_R;
    kbdTemplateAlt1 [_ROWOFF+3][9] = ICONTEXT_RETURN_R;
    kbdTemplateAlt2 [_ROWOFF+3][9] = ICONTEXT_RETURN_R;

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
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, kbd.model->texWidth, kbd.model->texHeight, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, kbd.model->texPixels);
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

    glnode_registerNode(RENDER_LOW, (GLNode){
        .setup = &gltouchkbd_setup,
        .shutdown = &gltouchkbd_shutdown,
        .render = &gltouchkbd_render,
        .reshape = &gltouchkbd_reshape,
        .onTouchEvent = &gltouchkbd_onTouchEvent,
    });
}

