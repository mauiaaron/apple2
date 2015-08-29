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
#include "json_parse.h"

#if !INTERFACE_TOUCH
#error this is a touch interface module, possibly you mean to not compile this at all?
#endif

#define DEBUG_TOUCH_KBD 0
#if DEBUG_TOUCH_KBD
#   define TOUCH_KBD_LOG(...) LOG(__VA_ARGS__)
#else
#   define TOUCH_KBD_LOG(...)
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
static bool isCalibrating = false;  // Are we in calibration mode?
static bool allowLowercase = false; // show lowercase keyboard
static float minAlphaWhenOwnsScreen = 1/4.f;
static float minAlpha = 0.f;
static float maxAlpha = 1.f;

static char kbdTemplateUCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "@ @ @ @ @ ",
    "1234567890",
    "QWERTYUIOP",
    "ASDFG@HJKL",
    " ZXCVBNM @",
    "@@ spa. @@",
};

static char kbdTemplateLCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "@ @ @ @ @ ",
    "1234567890",
    "qwertyuiop",
    "asdfg@hjkl",
    " zxcvbnm @",
    "@@ SPA. @@",
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
    "     @    ",
    "    @@@   ",
    "     @    ",
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

    // find the CTRL visual(s) and render them engaged
    if (kbd.ctrlPressed) {
        for (unsigned int i=0, row=0; row<KBD_TEMPLATE_ROWS; row++, i++) {
            for (unsigned int col=0; col<KBD_TEMPLATE_COLS; col++, i++) {
                uint8_t ch = template[i];
                if (ch == ICONTEXT_CTRL) {
                    _rerender_character(col, row, hudKeyboard->pixelsAlt);
                }
            }
            ++i;
        }
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
        alpha = maxAlpha;
        if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
            alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
            if (alpha < minAlpha) {
                alpha = minAlpha;
            }
        }
    }

    return alpha;
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

    TOUCH_KBD_LOG("SCREEN TO KEYBOARD : kbdX:%d kbdXMax:%d kbdW:%d keyW:%d ... scrn:(%f,%f)->kybd:(%d,%d)", touchport.kbdX, touchport.kbdXMax, touchport.kbdW, keyW, x, y, *col, *row);
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

static inline int64_t _tap_key_at_point(float x, float y) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;

    if (!_is_point_on_keyboard(x, y)) {
        joy_button0 = 0x0;
        joy_button1 = 0x0;
        // CTRL key is "sticky"
        _redraw_unselected(kbd.selectedCol, kbd.selectedRow);
        kbd.selectedCol = -1;
        kbd.selectedRow = -1;
        return false;
    }

    // get current key, col, row

    int col = -1;
    int row = -1;
    _screen_to_keyboard(x, y, &col, &row);
    const unsigned int indexRow = (hudKeyboard->tplWidth+1) * row;

    int key = (hudKeyboard->tpl+indexRow)[col];
    int scancode = -1;

    // handle key

    bool handled = true;
    bool isASCII = false;
    bool isCTRL = false;
    switch (key) {
        case ICONTEXT_LOWERCASE:
            key = -1;
            _switch_keyboard(kbd.model, kbdTemplateLCase[0]);
            break;

        case ICONTEXT_UPPERCASE:
            key = -1;
            _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
            break;

        case ICONTEXT_SHOWALT1:
            key = -1;
            if (allowLowercase && !isCalibrating) {
                kbdTemplateAlt[0][0] = ICONTEXT_LOWERCASE;
            } else {
                kbdTemplateAlt[0][0] = ICONTEXT_UPPERCASE;
            }
            _switch_keyboard(kbd.model, kbdTemplateAlt[0]);
            break;

        case ICONTEXT_NONACTIONABLE:
            scancode = 0;
            handled = isCalibrating;
            break;

        case ICONTEXT_CTRL:
            isCTRL = true;
            kbd.ctrlPressed = !kbd.ctrlPressed;
            _rerender_character(kbd.ctrlCol, kbd.ctrlRow, kbd.ctrlPressed ? hudKeyboard->pixelsAlt : hudKeyboard->pixels);
            col = -1;
            row = -1;
            scancode = SCODE_L_CTRL;
            break;

        case ICONTEXT_RETURN_L:
        case ICONTEXT_RETURN_R:
            key = ICONTEXT_RETURN_L;
            scancode = SCODE_RET;
            break;

        case ICONTEXT_ESC:
            scancode = SCODE_ESC;
            break;

        case MOUSETEXT_LEFT:
        case ICONTEXT_BACKSPACE:
            key = MOUSETEXT_LEFT;
            scancode = SCODE_L;
            break;

        case MOUSETEXT_RIGHT:
            scancode = SCODE_R;
            break;

        case MOUSETEXT_UP:
            scancode = SCODE_U;
            break;

        case MOUSETEXT_DOWN:
            scancode = SCODE_D;
            break;

        case MOUSETEXT_OPENAPPLE:
            joy_button0 = joy_button0 ? 0x0 : 0x80;
            scancode = SCODE_L_ALT;
            break;

        case MOUSETEXT_CLOSEDAPPLE:
            joy_button1 = joy_button1 ? 0x0 : 0x80;
            scancode = SCODE_R_ALT;
            break;

        case ICONTEXT_MENU_SPROUT:
            key = -1;
            _toggle_arrows();
            break;

        case ICONTEXT_GOTO:
            isASCII = true;
            key = '\t';
            break;

        case ICONTEXT_LEFTSPACE:
        case ICONTEXT_MIDSPACE:
        case ICONTEXT_RIGHTSPACE:
            isASCII = true;
            key = ' ';
            break;

        default: // ASCII
            isASCII = true;
            break;
    }

    assert(scancode < 0x80);
    if (isASCII) {
        assert(key < 0x80);
        scancode = c_keys_ascii_to_scancode(key);
        if (kbd.ctrlPressed) {
            c_keys_handle_input(scancode, /*pressed*/true, /*ASCII:*/false);
            c_keys_handle_input(scancode, /*pressed*/false, /*ASCII:*/false);
        } else {
            c_keys_handle_input(key, /*pressed:*/true,  /*ASCII:*/true);
        }
        if (key == ' ' && isCalibrating) {
            key = ICONTEXT_SPACE_VISUAL;
        }
    } else if (isCTRL) {
        c_keys_handle_input(scancode, /*pressed:*/kbd.ctrlPressed,  /*ASCII:*/false);
    } else if (scancode != -1) {
        // perform a press of other keys (ESC, Arrows, etc)
        c_keys_handle_input(scancode, /*pressed:*/true,  /*ASCII:*/false);
        c_keys_handle_input(scancode, /*pressed:*/false, /*ASCII:*/false);
    }

    // redraw previous selected key
    _redraw_unselected(kbd.selectedCol, kbd.selectedRow);

    kbd.selectedCol = col;
    kbd.selectedRow = row;

    // draw current selected key
    _redraw_selected(kbd.selectedCol, kbd.selectedRow);

    // return the key+scancode+handled
    int64_t flags = (handled ? 0x1LL : 0x0LL);
    if (key < 0 ) {
        key = 0;
    }
    if (scancode < 0) {
        scancode = 0;
    }

    key = key & 0xff;
    scancode = scancode & 0xff;

    flags |= ( (int64_t)((key << 8) | scancode) << TOUCH_FLAGS_ASCII_AND_SCANCODE_SHIFT);
    return flags;
}

// ----------------------------------------------------------------------------
// GLCustom functions

static void _setup_touchkbd_hud(GLModel *parent) {
    GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)parent->custom;

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
    if (!ownsScreen) {
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

static int64_t gltouchkbd_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {

    if (!isAvailable) {
        return 0x0LL;
    }
    if (!isEnabled) {
        return 0x0LL;
    }
    if (!ownsScreen) {
        return 0x0LL;
    }

    float x = x_coords[pointer_idx];
    float y = y_coords[pointer_idx];

    int64_t flags = TOUCH_FLAGS_KBD | TOUCH_FLAGS_HANDLED;

    switch (action) {
        case TOUCH_DOWN:
        case TOUCH_POINTER_DOWN:
            break;

        case TOUCH_MOVE:
            break;

        case TOUCH_UP:
        case TOUCH_POINTER_UP:
            {
                int64_t handledAndData = _tap_key_at_point(x, y);
                flags |= ((handledAndData & 0x01) ? TOUCH_FLAGS_KEY_TAP : 0x0LL);
                flags |= (handledAndData & TOUCH_FLAGS_ASCII_AND_SCANCODE_MASK);
            }
            break;

        case TOUCH_CANCEL:
            LOG("---KBD TOUCH CANCEL");
            return 0x0LL;

        default:
            LOG("!!!KBD UNKNOWN TOUCH EVENT : %d", action);
            return 0x0LL;
    }

    clock_gettime(CLOCK_MONOTONIC, &kbd.timingBegin);

    return flags;
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
        if (allowLowercase) {
            caps_lock = false;
        } else {
            caps_lock = true;
        }
    } else {
        // reset visuals
        minAlpha = 0.0;
        if (kbd.model) {
            GLModelHUDKeyboard *hudKeyboard = (GLModelHUDKeyboard *)kbd.model->custom;
            hudKeyboard->colorScheme = RED_ON_BLACK;
            glhud_setupDefault(kbd.model);
        }

        // reset CTRL state upon leaving this touch device
        kbd.ctrlPressed = false;
        c_keys_handle_input(SCODE_L_CTRL, /*pressed:*/false, /*ASCII:*/false);
    }
}

static bool gltouchkbd_ownsScreen(void) {
    return ownsScreen;
}

static void gltouchkbd_setVisibilityWhenOwnsScreen(float inactiveAlpha, float activeAlpha) {
    minAlphaWhenOwnsScreen = inactiveAlpha;
    maxAlpha = activeAlpha;
    if (ownsScreen) {
        minAlpha = minAlphaWhenOwnsScreen;
    }
}

static void gltouchkbd_setLowercaseEnabled(bool enabled) {
    allowLowercase = enabled;
    if (allowLowercase) {
        caps_lock = false;
    } else {
        caps_lock = true;
    }
}

static void gltouchkbd_beginCalibration(void) {
    video_clear();
    isCalibrating = true;
}

static void gltouchkbd_endCalibration(void) {
    video_redraw();
    isCalibrating = false;
}

static void gltouchkbd_loadAltKbd(const char *kbdPath) {
    JSON_s parsedData = { 0 };
    int tokCount = json_createFromFile(kbdPath, &parsedData);

    do {
        if (tokCount < 0) {
            break;
        }

        // we are expecting a very specific layout ... abort if anything is not correct
        int idx=0;

        // begin with array
        if (parsedData.jsonTokens[idx].type != JSMN_ARRAY) {
            ERRLOG("Keyboard JSON must start with array");
            break;
        }
        ++idx;

        // next is a global comment string
        if (parsedData.jsonTokens[idx].type != JSMN_STRING) {
            ERRLOG("Expecting a comment string at JSON token position 1");
            break;
        }
        ++idx;

        // next is the dictionary of special strings
        if (parsedData.jsonTokens[idx].type != JSMN_OBJECT) {
            ERRLOG("Expecting a dictionary at JSON token position 2");
            break;
        }
        const int dictCount = parsedData.jsonTokens[idx].size;
        ++idx;
        const int dictBegin = idx;

        // verify all dictionary keys/vals are strings
        bool allStrings = true;
        const int dictEnd = dictBegin + (dictCount*2);
        for (; idx<dictEnd; idx+=2) {
            if (parsedData.jsonTokens[idx].type != JSMN_STRING) {
                allStrings = false;
                break;
            }
            if (parsedData.jsonTokens[idx+1].type != JSMN_STRING) {
                allStrings = false;
                break;
            }
        }
        if (!allStrings) {
            ERRLOG("Specials dictionary should only contain strings");
            break;
        }

        // next is reserved0 array
        if (parsedData.jsonTokens[idx].type != JSMN_ARRAY) {
            ERRLOG("Expecting a reserved array at JSON token position 3");
            break;
        }
        ++idx;
        idx += parsedData.jsonTokens[idx-1].size;

        // next is reserved1 array
        if (parsedData.jsonTokens[idx].type != JSMN_ARRAY) {
            ERRLOG("Expecting a reserved array at JSON token position 4");
            break;
        }
        ++idx;
        idx += parsedData.jsonTokens[idx-1].size;

        // next are the character rows
        int row = 0;
        while (idx < tokCount) {
            if ( !((parsedData.jsonTokens[idx].type == JSMN_ARRAY) && (parsedData.jsonTokens[idx].size == KBD_TEMPLATE_COLS) && (parsedData.jsonTokens[idx].parent == 0)) ) {
                ERRLOG("Expecting an array of ten items at keyboard row %d", row+1);
                break;
            }

            ++idx;
            const int count = idx+KBD_TEMPLATE_COLS;
            for (int col=0; idx<count; col++, idx++) {

                if (parsedData.jsonTokens[idx].type != JSMN_STRING) {
                    ERRLOG("Unexpected non-string at keyboard row %d", row+1);
                    break;
                }

                int start = parsedData.jsonTokens[idx].start;
                int end   = parsedData.jsonTokens[idx].end;
                assert(end >= start && "bug");
                const int size  = end - start;

                if (size == 1) {
                    kbdTemplateArrow[row][col] = parsedData.jsonString[start];
                    continue;
                } else if (size == 0) {
                    kbdTemplateArrow[row][col] = ICONTEXT_NONACTIONABLE;
                    continue;
                } else if (size < 0) {
                    assert(false && "negative size coming from jsmn!");
                    continue;
                }

                // assign special interface/mousetext visuals
                char *key = &parsedData.jsonString[start];

                bool foundMatch = false;
                for (int i=dictBegin; i<dictEnd; i+=2) {
                    start = parsedData.jsonTokens[i].start;
                    end   = parsedData.jsonTokens[i].end;

                    assert(end >= start && "bug");

                    if (end - start != size) {
                        continue;
                    }

                    foundMatch = (strncmp(key, &parsedData.jsonString[start], size) == 0);

                    if (foundMatch) {
                        start = parsedData.jsonTokens[i+1].start;
                        uint8_t ch = (uint8_t)strtol(&parsedData.jsonString[start], NULL, /*base:*/16);
                        kbdTemplateArrow[row][col] = ch;
                        break;
                    }
                }
                if (!foundMatch) {
                    ERRLOG("no match for found multichar value in keyboard row %d", row+1);
                }
            }

            ++row;
        }

        if (row != KBD_TEMPLATE_ROWS) {
            ERRLOG("Did not find expected number of keyboard rows");
        } else {
            LOG("Parsed keyboard at %s", kbdPath);
        }

    } while (0);

    json_destroy(&parsedData);
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
// Constructors

static void _initialize_keyboard_templates(void) {

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

    kbdTemplateUCase[_ROWOFF+3][0] = ICONTEXT_GOTO;
    kbdTemplateLCase[_ROWOFF+3][0] = ICONTEXT_GOTO;

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
}

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_gltouchkbd(void) {
    LOG("Registering OpenGL software touch keyboard");

    _initialize_keyboard_templates();

    video_backend->animation_showTouchKeyboard = &_animation_showTouchKeyboard;
    video_backend->animation_hideTouchKeyboard = &_animation_hideTouchKeyboard;

    keydriver_isTouchKeyboardAvailable = &gltouchkbd_isTouchKeyboardAvailable;
    keydriver_setTouchKeyboardEnabled = &gltouchkbd_setTouchKeyboardEnabled;
    keydriver_setTouchKeyboardOwnsScreen = &gltouchkbd_setTouchKeyboardOwnsScreen;
    keydriver_ownsScreen = &gltouchkbd_ownsScreen;
    keydriver_setVisibilityWhenOwnsScreen = &gltouchkbd_setVisibilityWhenOwnsScreen;
    keydriver_setLowercaseEnabled = &gltouchkbd_setLowercaseEnabled;
    keydriver_beginCalibration = &gltouchkbd_beginCalibration;
    keydriver_endCalibration = &gltouchkbd_endCalibration;
    keydriver_loadAltKbd = &gltouchkbd_loadAltKbd;

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

