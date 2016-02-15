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

#include "video/glhudmodel.h"
#include "video/glnode.h"

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

#define KBD_TEMPLATE_COLS 10 // 10 cols for a mobile keyboard in portrait seems to be a defacto standard across iOS/Android ...
#define KBD_TEMPLATE_ROWS 8

#define DEFAULT_CTRL_COL 2

#define CTRLROW 2 // first non-empty default row
#define MAINROW 4 // main keyboard row offset
#define SWITCHCOL 0

#define KBD_FB_WIDTH (KBD_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) // 10 * 7 == 70
#define KBD_FB_HEIGHT (KBD_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS) // 8 * 16 == 128

#define KBD_OBJ_W GL_MODEL_MAX // model width fits screen
#define KBD_OBJ_H_LANDSCAPE GL_MODEL_MAX

static bool isAvailable = false; // Were there any OpenGL/memory errors on gltouchkbd initialization?
static bool isEnabled = true;    // Does player want touchkbd enabled?
static bool ownsScreen = false;  // Does the touchkbd currently own the screen to the exclusion?
static bool isCalibrating = false;  // Are we in calibration mode?
static bool allowLowercase = false; // show lowercase keyboard
static float minAlphaWhenOwnsScreen = 1/4.f;
static float minAlpha = 0.f;
static float maxAlpha = 1.f;

static uint8_t kbdTemplateUCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "          ",
    "          ",
    "@ @ @ @ @ ",
    "1234567890",
    "QWERTYUIOP",
    "ASDFG@HJKL",
    " ZXCVBNM @",
    "@@ spa. @@",
};

static uint8_t kbdTemplateLCase[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "          ",
    "          ",
    "@ @ @ @ @ ",
    "1234567890",
    "qwertyuiop",
    "asdfg@hjkl",
    " zxcvbnm @",
    "@@ SPA. @@",
};

static uint8_t kbdTemplateAlt[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "          ",
    "          ",
    "@ @ @ @ @ ",
    "1234567890",
    "@#%&*/-+()",
    "~=_<>X{}[]",
    "?!\"'`:;, X",
    "$|\\XXX.^XX",
};

static uint8_t kbdTemplateUserAlt[KBD_TEMPLATE_ROWS][KBD_TEMPLATE_COLS+1] = {
    "          ",
    "          ",
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

    // raw device dimensions
    int rawWidth;
    int rawHeight;
    int isLandscape;
} touchport = { 0 };

// keyboard variables

static struct {
    GLModel *model;

    GLfloat modelHeight;
    GLfloat modelSkewY;
    bool modelDirty;

    int selectedCol;
    int selectedRow;

    int ctrlCol;
    int ctrlRow;

    bool ctrlPressed;

    unsigned int glyphMultiplier;
    float portraitHeightScale;
    float portraitPositionScale;

    struct timespec timingBegin;

    // pending changes requiring reinitialization
    unsigned int nextGlyphMultiplier;
} kbd = { 0 };

// ----------------------------------------------------------------------------
// Misc internal methods

#warning FIXME TODO ... make this a generic GLModelHUDElement function
static void _rerender_character(int col, int row) {
    GLModelHUDElement *hudKeyboard = (GLModelHUDElement *)(kbd.model->custom);

    // In English, this changes one glyph within the keyboard texture data to be the (un)selected color.  It handles
    // scaling from indexed color to RGBA8888 (4x) and then possibly scaling to 2x or greater.

    const unsigned int fb_w = hudKeyboard->pixWidth;
    const unsigned int pixelSize = sizeof(PIXEL_TYPE);
    const unsigned int glyphScale = hudKeyboard->glyphMultiplier;
    const unsigned int dstPointStride = pixelSize * glyphScale;
    const unsigned int dstRowStride = fb_w * dstPointStride;
    const unsigned int texSubRowStride = dstRowStride + (dstRowStride * (glyphScale-1));
    const unsigned int indexedIdx = (row * fb_w * FONT_HEIGHT_PIXELS) + (col * FONT80_WIDTH_PIXELS);
    unsigned int texIdx = ((row * fb_w * FONT_HEIGHT_PIXELS * /*1 row:*/glyphScale) + (col * FONT80_WIDTH_PIXELS)) * dstPointStride;

    for (unsigned int i=0; i<FONT_HEIGHT_PIXELS; i++, texIdx+=texSubRowStride) {
        for (unsigned int j=0; j<FONT80_WIDTH_PIXELS; j++, texIdx+=dstPointStride) {
            // HACK : red <-> green swap of texture data
            PIXEL_TYPE rgba = *((PIXEL_TYPE *)(kbd.model->texPixels + texIdx));
            PIXEL_TYPE r = (rgba >> SHIFT_R) & MAX_SATURATION;
            PIXEL_TYPE g = (rgba >> SHIFT_G) & MAX_SATURATION;
#if USE_RGBA4444
#error fixme
#else
            rgba = ( ((rgba>>SHIFT_B)<<SHIFT_B) | (r << SHIFT_G) | (g << SHIFT_R) );
#endif
            // scale texture data 1x, 2x, ...
            unsigned int dstIdx = texIdx;
            for (unsigned int k=0; k<glyphScale; k++, dstIdx+=dstRowStride) {
                for (unsigned int l=0; l<glyphScale; l++, dstIdx+=pixelSize) {
                    *( (PIXEL_TYPE *)(kbd.model->texPixels + dstIdx) ) = rgba;
                }
                dstIdx -= dstPointStride;
            }
        }
        texIdx -= (FONT80_WIDTH_PIXELS * dstPointStride);
    }

    kbd.model->texDirty = true;
}

// non-destructively adjust model height/position
static void _reset_model(void) {

    /* 2...3
     *  .
     *   .
     *    .
     * 0...1
     */
    const GLfloat skew_x = -GL_MODEL_HALF;
    const GLfloat skew_y = kbd.modelSkewY;
    const GLfloat obj_w = KBD_OBJ_W;
    const GLfloat obj_h = kbd.modelHeight;

    GLfloat *quad = (GLfloat *)(kbd.model->positions);
    quad[0 +0] = skew_x;        quad[0 +1] = skew_y;
    quad[4 +0] = skew_x+obj_w;  quad[4 +1] = skew_y;
    quad[8 +0] = skew_x;        quad[8 +1] = skew_y+obj_h;
    quad[12+0] = skew_x+obj_w;  quad[12+1] = skew_y+obj_h;

    kbd.modelDirty = true;
}

static inline void _rerender_selected(int col, int row) {
    if ((col >= 0) && (row >= 0)) {
        _rerender_character(col, row);

        // rerender certain adjacent keys ...
        GLModelHUDElement *hudKeyboard = (GLModelHUDElement *)(kbd.model->custom);
        const unsigned int indexRow = (hudKeyboard->tplWidth+1) * row;
        uint8_t key = (hudKeyboard->tpl+indexRow)[col];
        switch (key) {
            case ICONTEXT_LEFTSPACE:
                _rerender_character(col+1, row);
                _rerender_character(col+2, row);
                break;
            case ICONTEXT_MIDSPACE:
                _rerender_character(col-1, row);
                _rerender_character(col+1, row);
                break;
            case ICONTEXT_RIGHTSPACE:
                _rerender_character(col-2, row);
                _rerender_character(col-1, row);
                break;
            case ICONTEXT_RETURN_L:
                _rerender_character(col+1, row);
                break;
            case ICONTEXT_RETURN_R:
                _rerender_character(col-1, row);
                break;
            default:
                break;
        }
    }
}

static inline void _switch_keyboard(GLModel *parent, uint8_t *template) {
    GLModelHUDElement *hudKeyboard = (GLModelHUDElement *)parent->custom;
    memcpy(hudKeyboard->tpl, template, sizeof(kbdTemplateUCase/* assuming all the same size */));

    // setup normal color pixels
    hudKeyboard->colorScheme = RED_ON_BLACK;
    glhud_setupDefault(parent);

    // find the CTRL visual(s) and render them engaged
    if (kbd.ctrlPressed) {
        for (unsigned int i=0, row=0; row<KBD_TEMPLATE_ROWS; row++, i++) {
            for (unsigned int col=0; col<KBD_TEMPLATE_COLS; col++, i++) {
                uint8_t ch = template[i];
                if (ch == ICONTEXT_CTRL) {
                    _rerender_character(col, row);
                }
            }
            ++i;
        }
    }
}

static inline bool _is_point_on_keyboard(float x, float y) {
    return (x >= touchport.kbdX && x <= touchport.kbdXMax && y >= touchport.kbdY && y <= touchport.kbdYMax);
}

static inline void _screen_to_keyboard(float x, float y, OUTPARM int *col, OUTPARM int *row) {
    GLModelHUDElement *hudKeyboard = (GLModelHUDElement *)(kbd.model->custom);
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
    } else if (*row >= hudKeyboard->tplHeight) {
        *row = hudKeyboard->tplHeight-1;
    }

    TOUCH_KBD_LOG("SCREEN TO KEYBOARD : kbdX:%d kbdXMax:%d kbdW:%d keyW:%d ... scrn:(%f,%f)->kybd:(%d,%d)", touchport.kbdX, touchport.kbdXMax, touchport.kbdW, keyW, x, y, *col, *row);
}

static inline int64_t _tap_key_at_point(float x, float y) {
    GLModelHUDElement *hudKeyboard = (GLModelHUDElement *)kbd.model->custom;

    // redraw previous selected key (if any)
    _rerender_selected(kbd.selectedCol, kbd.selectedRow);

    if (!_is_point_on_keyboard(x, y)) {
        joy_button0 = 0x0;
        joy_button1 = 0x0;
        kbd.selectedCol = -1;
        kbd.selectedRow = -1;
        return false;
    }

    // get current key, col, row

    int col = -1;
    int row = -1;
    _screen_to_keyboard(x, y, &col, &row);
    const unsigned int indexRow = (hudKeyboard->tplWidth+1) * row;

    uint8_t key = (hudKeyboard->tpl+indexRow)[col];
    uint8_t scancode = 0;

    // handle key

    bool handled = true;
    bool isASCII = false;
    bool isCTRL = false;
    switch (key) {
        case ICONTEXT_LOWERCASE:
            key = 0;
            _switch_keyboard(kbd.model, kbdTemplateLCase[0]);
            break;

        case ICONTEXT_UPPERCASE:
            key = 0;
            _switch_keyboard(kbd.model, kbdTemplateUCase[0]);
            break;

        case ICONTEXT_SHOWALT1:
            key = 0;
            if (allowLowercase && !isCalibrating) {
                kbdTemplateAlt[CTRLROW][SWITCHCOL] = ICONTEXT_LOWERCASE;
            } else {
                kbdTemplateAlt[CTRLROW][SWITCHCOL] = ICONTEXT_UPPERCASE;
            }
            _switch_keyboard(kbd.model, kbdTemplateAlt[0]);
            break;

        case ICONTEXT_NONACTIONABLE:
            scancode = 0;
            handled = false;
            break;

        case ICONTEXT_CTRL:
            isCTRL = true;
            kbd.ctrlPressed = !kbd.ctrlPressed;
            _rerender_character(kbd.ctrlCol, kbd.ctrlRow);
            col = -1;
            row = -1;
            scancode = SCODE_L_CTRL;
            break;

        case MOUSETEXT_RETURN:
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
            key = 0;
            kbd.ctrlPressed = false;
            _switch_keyboard(kbd.model, kbdTemplateUserAlt[0]);
            break;

        case ICONTEXT_GOTO:
            isASCII = true;
            key = '\t';
            break;

        case ICONTEXT_LEFTSPACE:
        case ICONTEXT_MIDSPACE:
        case ICONTEXT_RIGHTSPACE:
        case ICONTEXT_SPACE_VISUAL:
            isASCII = true;
            key = ' ';
            break;

        default: // ASCII
            isASCII = true;
            if (key >= 0x80) {
                RELEASE_LOG("unhandled key code...");
                key = 0;
            }
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

    // draw current selected key (if any)
    kbd.selectedCol = col;
    kbd.selectedRow = row;
    _rerender_selected(kbd.selectedCol, kbd.selectedRow);

    // return the key+scancode+handled
    int64_t flags = (handled ? TOUCH_FLAGS_HANDLED : 0x0LL);

    key = key & 0xff;
    scancode = scancode & 0xff;

    flags |= ( (int64_t)((key << 8) | scancode) << TOUCH_FLAGS_ASCII_AND_SCANCODE_SHIFT);
    return flags;
}

// ----------------------------------------------------------------------------
// GLCustom functions

static void *_create_touchkbd_hud(GLModel *parent) {

    parent->custom = glhud_createCustom(sizeof(GLModelHUDElement));
    GLModelHUDElement *hudKeyboard = (GLModelHUDElement *)parent->custom;

    if (!hudKeyboard) {
        return NULL;
    }

    hudKeyboard->blackIsTransparent = true;
    hudKeyboard->opaquePixelHalo = true;
    hudKeyboard->glyphMultiplier = kbd.glyphMultiplier;

    hudKeyboard->tplWidth  = KBD_TEMPLATE_COLS;
    hudKeyboard->tplHeight = KBD_TEMPLATE_ROWS;
    hudKeyboard->pixWidth  = KBD_FB_WIDTH;
    hudKeyboard->pixHeight = KBD_FB_HEIGHT;

    const unsigned int size = sizeof(kbdTemplateUCase/* assuming all the same dimensions */);
    hudKeyboard->tpl = MALLOC(size);
    hudKeyboard->pixels = MALLOC(KBD_FB_WIDTH * KBD_FB_HEIGHT);

    memcpy(hudKeyboard->tpl, kbdTemplateUCase[0], sizeof(kbdTemplateUCase/* assuming all the same dimensions */));

    // setup normal color pixels
    hudKeyboard->colorScheme = RED_ON_BLACK;

    glhud_setupDefault(parent);

    return hudKeyboard;
}

// ----------------------------------------------------------------------------
// GLNode functions

static void gltouchkbd_shutdown(void) {
    LOG("gltouchkbd_shutdown ...");
    if (!isAvailable) {
        return;
    }

    isAvailable = false;

    mdlDestroyModel(&kbd.model);
    kbd.selectedCol = -1;
    kbd.selectedRow = -1;
    kbd.ctrlPressed = false;

    kbd.nextGlyphMultiplier = 0;
}

static void gltouchkbd_setup(void) {
    LOG("gltouchkbd_setup ... %u", sizeof(kbd));

    gltouchkbd_shutdown();

    kbd.model = mdlCreateQuad((GLModelParams_s){
            .skew_x = -GL_MODEL_HALF,
            .skew_y = kbd.modelSkewY,
            .z = MODEL_DEPTH,
            .obj_w = KBD_OBJ_W,
            .obj_h = kbd.modelHeight,
            .positionUsageHint = GL_DYNAMIC_DRAW, // positions might change
            .tex_w = KBD_FB_WIDTH * kbd.glyphMultiplier,
            .tex_h = KBD_FB_HEIGHT * kbd.glyphMultiplier,
            .texcoordUsageHint = GL_DYNAMIC_DRAW, // but key texture does
        }, (GLCustom){
            .create = &_create_touchkbd_hud,
            .destroy = &glhud_destroyDefault,
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

    if (kbd.nextGlyphMultiplier) {
        kbd.glyphMultiplier = kbd.nextGlyphMultiplier;
        kbd.nextGlyphMultiplier = 0;
        gltouchkbd_setup();
    }

    float alpha = glhud_getTimedVisibility(kbd.timingBegin, minAlpha, maxAlpha);
    if (alpha < minAlpha) {
        alpha = minAlpha;
        _rerender_selected(kbd.selectedCol, kbd.selectedRow);
        kbd.selectedCol = -1;
        kbd.selectedRow = -1;
    }

    if (alpha > 0.0) {
        // draw touch keyboard

        glViewport(0, 0, touchport.width, touchport.height); // NOTE : show these HUD elements beyond the A2 framebuffer dimensions
        glUniform1f(alphaValue, alpha);

        glActiveTexture(TEXTURE_ACTIVE_TOUCHKBD);
        glBindTexture(GL_TEXTURE_2D, kbd.model->textureName);
        if (kbd.model->texDirty) {
            kbd.model->texDirty = false;
            _HACKAROUND_GLTEXIMAGE2D_PRE(TEXTURE_ACTIVE_TOUCHKBD, kbd.model->textureName);
            glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, kbd.model->texWidth, kbd.model->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, kbd.model->texPixels);
        }
        if (kbd.modelDirty) {
            kbd.modelDirty = false;
            glBindBuffer(GL_ARRAY_BUFFER, kbd.model->posBufferName);
            glBufferData(GL_ARRAY_BUFFER, kbd.model->positionArraySize, kbd.model->positions, GL_DYNAMIC_DRAW);
        }
        glUniform1i(texSamplerLoc, TEXTURE_ID_TOUCHKBD);
        glhud_renderDefault(kbd.model);
    }
}

static void gltouchkbd_reshape(int w, int h, bool landscape) {
    LOG("w:%d h:%d landscape:%d", w, h, landscape);

    touchport.rawWidth = w;
    touchport.rawHeight = h;
    touchport.isLandscape = landscape;

    touchport.kbdX = 0;

    swizzleDimensions(&w, &h, landscape);
    touchport.width = w;
    touchport.height = h;

    kbd.modelHeight = landscape ? KBD_OBJ_H_LANDSCAPE : (GL_MODEL_MAX * kbd.portraitHeightScale);
    kbd.modelSkewY = -GL_MODEL_HALF + ((GL_MODEL_MAX - kbd.modelHeight) * kbd.portraitPositionScale);

    touchport.kbdXMax = w * (KBD_OBJ_W/GL_MODEL_MAX);

    touchport.kbdY = h * (GL_MODEL_HALF - (kbd.modelHeight/GL_MODEL_MAX));
    touchport.kbdY = h * ((GL_MODEL_MAX - (kbd.modelSkewY + GL_MODEL_HALF + kbd.modelHeight)) / GL_MODEL_MAX);
    touchport.kbdYMax = h * ((GL_MODEL_MAX - (kbd.modelSkewY + GL_MODEL_HALF)) / GL_MODEL_MAX);

    LOG("modelHeight:%f modelSkewY:%f kbdY:%d kbdYMax:%d", kbd.modelHeight, kbd.modelSkewY, touchport.kbdY, touchport.kbdYMax);

    touchport.kbdW = touchport.kbdXMax - touchport.kbdX;
    touchport.kbdH = touchport.kbdYMax - touchport.kbdY;

    if (kbd.model) {
        _reset_model();
    }
}

static void gltouchkbd_setData(const char *jsonData) {
    JSON_s parsedData = { 0 };
    int tokCount = json_createFromString(jsonData, &parsedData);

    do {
        if (tokCount < 0) {
            break;
        }

        json_mapParseFloatValue(&parsedData, PREF_PORTRAIT_HEIGHT_SCALE, &kbd.portraitHeightScale);
        json_mapParseFloatValue(&parsedData, PREF_PORTRAIT_POSITION_SCALE, &kbd.portraitPositionScale);

        gltouchkbd_reshape(touchport.rawWidth, touchport.rawHeight, touchport.isLandscape);
    } while (0);

    json_destroy(&parsedData);
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

    int64_t flags = TOUCH_FLAGS_KBD;

    clock_gettime(CLOCK_MONOTONIC, &kbd.timingBegin);

    static int trackingIndex = TRACKING_NONE;

    switch (action) {
        case TOUCH_DOWN:
        case TOUCH_POINTER_DOWN:
            if (/*isOnKeyboardModel:*/true) {// TODO FIXME : nonactionable areas could defer to joystick ...
                trackingIndex = pointer_idx;
                flags |= TOUCH_FLAGS_HANDLED;
            }
            break;

        case TOUCH_MOVE:
            flags |= ((pointer_idx == trackingIndex) ? TOUCH_FLAGS_HANDLED : 0);
            break;

        case TOUCH_UP:
        case TOUCH_POINTER_UP:
            {
                if (trackingIndex == pointer_idx) {
                    int64_t handledAndData = _tap_key_at_point(x, y);
                    flags |= ((handledAndData & TOUCH_FLAGS_HANDLED) ? (TOUCH_FLAGS_HANDLED|TOUCH_FLAGS_KEY_TAP) : 0x0LL);
                    flags |= (handledAndData & TOUCH_FLAGS_ASCII_AND_SCANCODE_MASK);
                    trackingIndex = TRACKING_NONE;
                }
            }
            break;

        case TOUCH_CANCEL:
            trackingIndex = TRACKING_NONE;
            LOG("---KBD TOUCH CANCEL");
            return 0x0LL;

        default:
            trackingIndex = TRACKING_NONE;
            LOG("!!!KBD UNKNOWN TOUCH EVENT : %d", action);
            return 0x0LL;
    }

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

        kbd.selectedCol = -1;
        kbd.selectedRow = -1;

        if (kbd.model) {
            GLModelHUDElement *hudKeyboard = (GLModelHUDElement *)kbd.model->custom;
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

static void gltouchkbd_setGlyphScale(int glyphScale) {
    if (glyphScale == 0) {
        glyphScale = 1;
    }
    kbd.nextGlyphMultiplier = glyphScale;
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
    if (allowLowercase && ownsScreen) {
        caps_lock = false;
    } else {
        caps_lock = true;
    }
}

static void gltouchkbd_beginCalibration(void) {
    isCalibrating = true;
}

static void gltouchkbd_endCalibration(void) {
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

        // next are the character rows
        int row = 0;
        while (idx < tokCount) {
            if (row < 2) {
                if ( !((parsedData.jsonTokens[idx].type == JSMN_ARRAY) && (parsedData.jsonTokens[idx].parent == 0)) ) {
                    ERRLOG("Expecting a reserved array at keyboard row %d", row+1);
                    break;
                }
                if (parsedData.jsonTokens[idx].size != KBD_TEMPLATE_COLS) {
                    // backwards compatibility when we only had the lower 6 rows for keyboard data ...
                    LOG("You can now edit %d columns of row %d in your custom keyboard", KBD_TEMPLATE_COLS, row+1);
                    for (int col=0; col<KBD_TEMPLATE_COLS; col++) {
                        kbdTemplateUserAlt[row][col] = ICONTEXT_NONACTIONABLE;
                    }
                    ++row;
                    ++idx;
                    idx += parsedData.jsonTokens[idx-1].size;
                    continue;
                }
            } else if ( !((parsedData.jsonTokens[idx].type == JSMN_ARRAY) && (parsedData.jsonTokens[idx].size == KBD_TEMPLATE_COLS) && (parsedData.jsonTokens[idx].parent == 0)) ) {
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
                    kbdTemplateUserAlt[row][col] = parsedData.jsonString[start];
                    continue;
                } else if (size == 0) {
                    kbdTemplateUserAlt[row][col] = ICONTEXT_NONACTIONABLE;
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
                        kbdTemplateUserAlt[row][col] = ch;
                        break;
                    }
                }
                if (!foundMatch) {
                    ERRLOG("no match for found multichar value in keyboard row %d", row+1);
                }
            }

            ++row;

            if (row >= KBD_TEMPLATE_ROWS) {
                break;
            }
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

    for (unsigned int i=0; i<(MAINROW-1); i++) {
        for (unsigned int j=0; j<KBD_TEMPLATE_COLS; j++) {
            kbdTemplateUCase[i][j] = ICONTEXT_NONACTIONABLE;
            kbdTemplateLCase[i][j] = ICONTEXT_NONACTIONABLE;
            kbdTemplateAlt  [i][j] = ICONTEXT_NONACTIONABLE;
        }
    }
    for (unsigned int i=0; i<KBD_TEMPLATE_ROWS; i++) {
        for (unsigned int j=0; j<KBD_TEMPLATE_COLS; j++) {
            if (kbdTemplateUserAlt[i][j] == ' ') {
                kbdTemplateUserAlt[i][j] = ICONTEXT_NONACTIONABLE;
            }
        }
    }

    kbdTemplateLCase[CTRLROW+0][0] = ICONTEXT_UPPERCASE;
    kbdTemplateUCase[CTRLROW+0][0] = ICONTEXT_SHOWALT1;
    kbdTemplateAlt  [CTRLROW+0][0] = ICONTEXT_UPPERCASE;
    kbdTemplateUserAlt[CTRLROW+0][0] = ICONTEXT_UPPERCASE;

    kbdTemplateUCase[CTRLROW+0][2] = ICONTEXT_CTRL;
    kbdTemplateLCase[CTRLROW+0][2] = ICONTEXT_CTRL;
    kbdTemplateAlt  [CTRLROW+0][2] = ICONTEXT_CTRL;
    kbdTemplateUserAlt[CTRLROW+0][2] = ICONTEXT_CTRL;

    kbdTemplateUCase[CTRLROW+0][4] = ICONTEXT_ESC;
    kbdTemplateLCase[CTRLROW+0][4] = ICONTEXT_ESC;
    kbdTemplateAlt  [CTRLROW+0][4] = ICONTEXT_ESC;
    kbdTemplateUserAlt[CTRLROW+0][4] = ICONTEXT_ESC;

    kbdTemplateUCase[CTRLROW+0][6] = MOUSETEXT_OPENAPPLE;
    kbdTemplateLCase[CTRLROW+0][6] = MOUSETEXT_OPENAPPLE;
    kbdTemplateAlt  [CTRLROW+0][6] = MOUSETEXT_OPENAPPLE;
    kbdTemplateUserAlt[CTRLROW+0][6] = MOUSETEXT_OPENAPPLE;

    kbdTemplateUCase[CTRLROW+0][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateLCase[CTRLROW+0][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateAlt  [CTRLROW+0][8] = MOUSETEXT_CLOSEDAPPLE;
    kbdTemplateUserAlt[CTRLROW+0][8] = MOUSETEXT_CLOSEDAPPLE;

    kbdTemplateUserAlt[CTRLROW+2][5] = MOUSETEXT_UP;
    kbdTemplateUserAlt[CTRLROW+3][4] = MOUSETEXT_LEFT;
    kbdTemplateUserAlt[CTRLROW+3][6] = MOUSETEXT_RIGHT;
    kbdTemplateUserAlt[CTRLROW+4][5] = MOUSETEXT_DOWN;

    kbdTemplateUCase[CTRLROW+3][5] = ICONTEXT_MENU_SPROUT;
    kbdTemplateLCase[CTRLROW+3][5] = ICONTEXT_MENU_SPROUT;
    kbdTemplateAlt  [CTRLROW+3][5] = ICONTEXT_MENU_SPROUT;
    kbdTemplateUserAlt[CTRLROW+3][5] = ICONTEXT_UPPERCASE;

    kbdTemplateUCase[MAINROW+2][0] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[MAINROW+2][0] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[MAINROW+2][8] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[MAINROW+2][8] = ICONTEXT_NONACTIONABLE;
    kbdTemplateAlt  [MAINROW+2][8] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[MAINROW+2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateLCase[MAINROW+2][9] = ICONTEXT_BACKSPACE;
    kbdTemplateAlt  [MAINROW+2][9] = ICONTEXT_BACKSPACE;

    // last row specials

    kbdTemplateLCase[MAINROW+3][0] = ICONTEXT_NONACTIONABLE;
    kbdTemplateUCase[MAINROW+3][0] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[MAINROW+3][1] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[MAINROW+3][1] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[MAINROW+3][2] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[MAINROW+3][2] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[MAINROW+3][0] = ICONTEXT_GOTO;
    kbdTemplateLCase[MAINROW+3][0] = ICONTEXT_GOTO;

    kbdTemplateUCase[MAINROW+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateLCase[MAINROW+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateAlt  [MAINROW+3][3] = ICONTEXT_LEFTSPACE;
    kbdTemplateUCase[MAINROW+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateLCase[MAINROW+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateAlt  [MAINROW+3][4] = ICONTEXT_MIDSPACE;
    kbdTemplateUCase[MAINROW+3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateLCase[MAINROW+3][5] = ICONTEXT_RIGHTSPACE;
    kbdTemplateAlt  [MAINROW+3][5] = ICONTEXT_RIGHTSPACE;

    kbdTemplateUCase[MAINROW+3][7] = ICONTEXT_NONACTIONABLE;
    kbdTemplateLCase[MAINROW+3][7] = ICONTEXT_NONACTIONABLE;

    kbdTemplateUCase[MAINROW+3][8] = ICONTEXT_RETURN_L;
    kbdTemplateLCase[MAINROW+3][8] = ICONTEXT_RETURN_L;
    kbdTemplateAlt  [MAINROW+3][8] = ICONTEXT_RETURN_L;

    kbdTemplateUCase[MAINROW+3][9] = ICONTEXT_RETURN_R;
    kbdTemplateLCase[MAINROW+3][9] = ICONTEXT_RETURN_R;
    kbdTemplateAlt  [MAINROW+3][9] = ICONTEXT_RETURN_R;
}

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_gltouchkbd(void) {
    LOG("Registering OpenGL software touch keyboard");

    _initialize_keyboard_templates();

    video_animations->animation_showTouchKeyboard = &_animation_showTouchKeyboard;
    video_animations->animation_hideTouchKeyboard = &_animation_hideTouchKeyboard;

    keydriver_isTouchKeyboardAvailable = &gltouchkbd_isTouchKeyboardAvailable;
    keydriver_setTouchKeyboardEnabled = &gltouchkbd_setTouchKeyboardEnabled;
    keydriver_setTouchKeyboardOwnsScreen = &gltouchkbd_setTouchKeyboardOwnsScreen;
    keydriver_ownsScreen = &gltouchkbd_ownsScreen;
    keydriver_setVisibilityWhenOwnsScreen = &gltouchkbd_setVisibilityWhenOwnsScreen;
    keydriver_setLowercaseEnabled = &gltouchkbd_setLowercaseEnabled;
    keydriver_beginCalibration = &gltouchkbd_beginCalibration;
    keydriver_endCalibration = &gltouchkbd_endCalibration;
    keydriver_loadAltKbd = &gltouchkbd_loadAltKbd;
    keydriver_setGlyphScale = &gltouchkbd_setGlyphScale;

    kbd.portraitHeightScale = 0.5f;
    kbd.portraitPositionScale = 0.f;

    kbd.selectedCol = -1;
    kbd.selectedRow = -1;

    kbd.ctrlCol = DEFAULT_CTRL_COL;
    kbd.ctrlRow = CTRLROW;

    kbd.glyphMultiplier = 1;

    glnode_registerNode(RENDER_LOW, (GLNode){
        .type = TOUCH_DEVICE_KEYBOARD,
        .setup = &gltouchkbd_setup,
        .shutdown = &gltouchkbd_shutdown,
        .render = &gltouchkbd_render,
        .reshape = &gltouchkbd_reshape,
        .onTouchEvent = &gltouchkbd_onTouchEvent,
        .setData = &gltouchkbd_setData,
    });
}

