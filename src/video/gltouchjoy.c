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

// general configurations

static bool isAvailable = false;
static bool isEnabled = true;
static bool isVisible = true;

static char axisTemplate[AXIS_TEMPLATE_ROWS][AXIS_TEMPLATE_COLS+1] = {
    "  @  ",
    "  |  ",
    "@-+-@",
    "  |  ",
    "  @  ",
};

static char buttonTemplate[BUTTON_TEMPLATE_ROWS][BUTTON_TEMPLATE_COLS+1] = {
    "@",
};

static glanim_t touchjoyAnimation = { 0 };

static int viewportWidth = 0;
static int viewportHeight = 0;

#warning FIXME TODO ... consolidate these into internal structs ....
static int axisSideX = 0;
static int axisSideXMax = 0;
static int axisSideY = 0;
static int axisSideYMax = 0;

static int buttSideX = 0;
static int buttSideXMax = 0;
static int buttSideY = 0;
static int buttSideYMax = 0;

// touch axis variables

static GLModel *touchAxisObjModel = NULL;

static uint8_t touchAxisObjFB[AXIS_FB_WIDTH * AXIS_FB_HEIGHT] = { 0 };
static bool axisModelDirty = true;

static int axisCenterX = 240;
static int axisCenterY = 160;
static int trackingAxisIndex = TOUCHED_NONE;
static struct timespec axisTimingBegin = { 0 };

static touchjoy_axis_type_t touchjoy_axisType = AXIS_EMULATED_DEVICE;
static uint8_t northChar = MOUSETEXT_UP;
static uint8_t westChar  = MOUSETEXT_LEFT;
static uint8_t eastChar  = MOUSETEXT_RIGHT;
static uint8_t southChar = MOUSETEXT_DOWN;

// button object variables

static GLModel *buttonObjModel = NULL;

static uint8_t buttonObjFB[BUTTON_FB_WIDTH * BUTTON_FB_HEIGHT] = { 0 };
static bool buttonModelDirty = true;

static int buttonCenterX = 240;
static int buttonCenterY = 160;
static int trackingButtonIndex = TOUCHED_NONE;
static struct timespec buttonTimingBegin = { 0 };

static int touchDownButton = TOUCHED_BUTTON0;
static int northButton = TOUCHED_BOTH;
static int southButton = TOUCHED_BUTTON1;

static uint8_t button0Char = MOUSETEXT_OPENAPPLE;
static uint8_t button1Char = MOUSETEXT_CLOSEDAPPLE;
static uint8_t buttonBothChar = '+';

static uint8_t buttonActiveChar = MOUSETEXT_OPENAPPLE;
static int buttonSwitchThreshold = BUTTON_SWITCH_THRESHOLD_DEFAULT;


// ----------------------------------------------------------------------------

static void _setup_object(char *submenu, unsigned int cols, unsigned int rows, uint8_t *fb, unsigned int fb_w, unsigned int fb_h, uint8_t *pixels) {

    // render template into indexed fb
    unsigned int submenu_width = cols;
    unsigned int submenu_height = rows;
    video_interface_print_submenu_centered_fb(fb, submenu_width, submenu_height, submenu, submenu_width, submenu_height);

    // generate RGBA_8888 from indexed color
    unsigned int count = fb_w * fb_h;
    for (unsigned int i=0, j=0; i<count; i++, j+=4) {
        uint8_t index = *(fb + i);
        uint32_t rgb = (((uint32_t)(colormap[index].red)   << 0 ) |
                        ((uint32_t)(colormap[index].green) << 8 ) |
                        ((uint32_t)(colormap[index].blue)  << 16));
        if (rgb == 0) {
            // make black transparent for joystick
        } else {
            rgb |=      ((uint32_t)0xff                    << 24);
        }
        *( (uint32_t*)(pixels + j) ) = rgb;
    }
}

static void _setup_axis_object(void) {
    axisTemplate[0][2] = northChar;
    axisTemplate[2][0] = westChar;
    axisTemplate[2][4] = eastChar;
    axisTemplate[4][2] = southChar;
    _setup_object(axisTemplate[0], AXIS_TEMPLATE_COLS, AXIS_TEMPLATE_ROWS, touchAxisObjFB, AXIS_FB_WIDTH, AXIS_FB_HEIGHT, touchAxisObjModel->texPixels);
    touchAxisObjModel->texDirty = true;
}

static void _setup_button_object(void) {
    buttonTemplate[0][0] = buttonActiveChar;
    _setup_object(buttonTemplate[0], BUTTON_TEMPLATE_COLS, BUTTON_TEMPLATE_ROWS, buttonObjFB, BUTTON_FB_WIDTH, BUTTON_FB_HEIGHT, buttonObjModel->texPixels);
    buttonObjModel->texDirty = true;
}

static inline void _screen_to_model(float x, float y, float *screenX, float *screenY) {
    *screenX = (x/(viewportWidth>>1))-1.f;
    *screenY = ((viewportHeight-y)/(viewportHeight>>1))-1.f;
}

static void _model_to_screen(float screenCoords[4], GLModel *model) {

    float x0 = 1.0;
    float y0 = 1.0;
    float x1 = -1.0;
    float y1 = -1.0;

#warning NOTE: we possibly should use matrix calculations (but assuming HUD elements are identity/orthographic for now)
    GLfloat *positions = (GLfloat *)(model->positions);
    unsigned int stride = model->positionSize;
    unsigned int len = model->positionArraySize/_get_gl_type_size(model->positionType);
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
    float yFlip0 = viewportHeight - (y1 * viewportHeight);
    float yFlip1 = viewportHeight - (y0 * viewportHeight);

    screenCoords[0] = x0 * viewportWidth;
    screenCoords[1] = yFlip0;
    screenCoords[2] = x1 * viewportWidth;
    screenCoords[3] = yFlip1;
}

static void gltouchjoy_init(void) {
    LOG("gltouchjoy_init ...");

    mdlDestroyModel(&touchAxisObjModel);
    mdlDestroyModel(&buttonObjModel);

    touchAxisObjModel = mdlCreateQuad(-1.05, -1.0, AXIS_OBJ_W, AXIS_OBJ_H, MODEL_DEPTH, AXIS_FB_WIDTH, AXIS_FB_HEIGHT, GL_RGBA); // RGBA8888
    if (!touchAxisObjModel) {
        LOG("gltouchjoy not initializing axis");
        return;
    }
    _setup_axis_object();

    // button object

    buttonObjModel = mdlCreateQuad(1.05-BUTTON_OBJ_W, -1.0, BUTTON_OBJ_W, BUTTON_OBJ_H, MODEL_DEPTH, BUTTON_FB_WIDTH, BUTTON_FB_HEIGHT, GL_RGBA); // RGBA8888
    if (!buttonObjModel) {
        LOG("gltouchjoy not initializing buttons");
        return;
    }
    _setup_button_object();

    clock_gettime(CLOCK_MONOTONIC, &axisTimingBegin);
    clock_gettime(CLOCK_MONOTONIC, &buttonTimingBegin);

    isAvailable = true;
}

static void gltouchjoy_destroy(void) {
    LOG("gltouchjoy_destroy ...");
    if (!isAvailable) {
        return;
    }

    isAvailable = false;

    mdlDestroyModel(&touchAxisObjModel);
    mdlDestroyModel(&buttonObjModel);
}

static void _render_object(GLModel *model) {

    // Bind our vertex array object
#if USE_VAO
    glBindVertexArray(model->vaoName);
#else
    glBindBuffer(GL_ARRAY_BUFFER, model->posBufferName);

    GLsizei posTypeSize      = _get_gl_type_size(model->positionType);
    GLsizei texcoordTypeSize = _get_gl_type_size(model->texcoordType);

    // Set up parmeters for position attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the position VBO to the VAO
    glVertexAttribPointer(POS_ATTRIB_IDX,                                   // What attibute index will this array feed in the vertex shader (see buildProgram)
                          model->positionSize,                 // How many elements are there per position?
                          model->positionType,                 // What is the type of this data?
                          GL_FALSE,                                         // Do we want to normalize this data (0-1 range for fixed-pont types)
                          model->positionSize*posTypeSize,     // What is the stride (i.e. bytes between positions)?
                          0);                                               // What is the offset in the VBO to the position data?
    glEnableVertexAttribArray(POS_ATTRIB_IDX);

    // Set up parmeters for texcoord attribute in the VAO including, size, type, stride, and offset in the currenly
    // bound VAO This also attaches the texcoord VBO to VAO
    glBindBuffer(GL_ARRAY_BUFFER, model->texcoordBufferName);
    glVertexAttribPointer(TEXCOORD_ATTRIB_IDX,                              // What attibute index will this array feed in the vertex shader (see buildProgram)
                          model->texcoordSize,                 // How many elements are there per texture coord?
                          model->texcoordType,                 // What is the type of this data in the array?
                          GL_TRUE,                                          // Do we want to normalize this data (0-1 range for fixed-point types)
                          model->texcoordSize*texcoordTypeSize,// What is the stride (i.e. bytes between texcoords)?
                          0);                                               // What is the offset in the VBO to the texcoord data?
    glEnableVertexAttribArray(TEXCOORD_ATTRIB_IDX);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->elementBufferName);
#endif

    // Draw the object
    glDrawElements(model->primType, model->numElements, model->elementType, 0);
    GL_ERRLOG("gltouchjoy render");
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

    // NOTE : show these HUD elements beyond the framebuffer dimensions
    glViewport(0, 0, viewportWidth, viewportHeight);

    struct timespec now = { 0 };
    const float min_alpha = 0.0625;
    float alpha = min_alpha;
    struct timespec deltat = { 0 };

    // draw axis

    clock_gettime(CLOCK_MONOTONIC, &now);
    alpha = min_alpha;
    deltat = timespec_diff(axisTimingBegin, now, NULL);
    if (deltat.tv_sec == 0) {
        alpha = 1.0;
        if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
            alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
            if (alpha < min_alpha) {
                alpha = min_alpha;
            }
        }
    }
    glUniform1f(alphaValue, alpha);

    glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_AXIS);
    glBindTexture(GL_TEXTURE_2D, touchAxisObjModel->textureName);
    if (touchAxisObjModel->texDirty) {
        touchAxisObjModel->texDirty = false;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, AXIS_FB_WIDTH, AXIS_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, touchAxisObjModel->texPixels);
    }
    if (axisModelDirty) {
        axisModelDirty = false;
        glBindBuffer(GL_ARRAY_BUFFER, touchAxisObjModel->posBufferName);
        glBufferData(GL_ARRAY_BUFFER, touchAxisObjModel->positionArraySize, touchAxisObjModel->positions, GL_DYNAMIC_DRAW);
    }
    glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHJOY_AXIS);
    _render_object(touchAxisObjModel);

    // draw button(s)

    clock_gettime(CLOCK_MONOTONIC, &now);
    alpha = min_alpha;
    deltat = timespec_diff(buttonTimingBegin, now, NULL);
    if (deltat.tv_sec == 0) {
        alpha = 1.0;
        if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
            alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
            if (alpha < min_alpha) {
                alpha = min_alpha;
            }
        }
    }
    glUniform1f(alphaValue, alpha);

    glActiveTexture(TEXTURE_ACTIVE_TOUCHJOY_BUTTON);
    glBindTexture(GL_TEXTURE_2D, buttonObjModel->textureName);
    if (buttonObjModel->texDirty) {
        buttonObjModel->texDirty = false;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, BUTTON_FB_WIDTH, BUTTON_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, buttonObjModel->texPixels);
    }
    if (buttonModelDirty) {
        buttonModelDirty = false;
        glBindBuffer(GL_ARRAY_BUFFER, buttonObjModel->posBufferName);
        glBufferData(GL_ARRAY_BUFFER, buttonObjModel->positionArraySize, buttonObjModel->positions, GL_DYNAMIC_DRAW);
    }
    glUniform1i(uniformTex2Use, TEXTURE_ID_TOUCHJOY_BUTTON);
    _render_object(buttonObjModel);
}

static void gltouchjoy_reshape(int w, int h) {
    LOG("gltouchjoy_reshape(%d, %d)", w, h);

    axisSideX = 0;

    if (w > viewportWidth) {
        viewportWidth = w;
        axisSideXMax = w>>1;
        buttSideX = w>>1;
        buttSideXMax = w;
    }
    if (h > viewportHeight) {
        viewportHeight = h;
        axisSideY = h>>1;
        axisSideYMax = h;
        buttSideY = h>>1;
        buttSideYMax = h;
    }
}

static void gltouchjoy_resetJoystick(void) {
    // no-op
}

static inline bool _is_point_on_axis_side(float x, float y) {
    return (x >= axisSideX && x <= axisSideXMax && y >= axisSideY && y <= axisSideYMax);
}

static inline bool _is_point_on_button_side(float x, float y) {
    return (x >= buttSideX && x <= buttSideXMax && y >= buttSideY && y <= buttSideYMax);
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

    x = (x - axisCenterX) + 0x80;
    y = (y - axisCenterY) + 0x80;

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
    if (buttonActiveChar != newChar) {
        buttonActiveChar = newChar;
        _setup_button_object();
    }
}

static inline void _move_button_axis(int x, int y) {

    x -= buttonCenterX;
    y -= buttonCenterY;

    if ((y < -buttonSwitchThreshold) || (y > buttonSwitchThreshold)) {
        if (y < 0) {
            //LOG("\tbutton neg y threshold (%d)", y);
            if (northButton == TOUCHED_BUTTON0) {
                joy_button0 = 0x80;
                joy_button1 = 0;
                _setup_button_object_with_char(button0Char);
            } else if (northButton == TOUCHED_BUTTON1) {
                joy_button0 = 0;
                joy_button1 = 0x80;
                _setup_button_object_with_char(button1Char);
            } else if (northButton == TOUCHED_BOTH) {
                joy_button0 = 0x80;
                joy_button1 = 0x80;
                _setup_button_object_with_char(buttonBothChar);
            } else {
                joy_button0 = 0;
                joy_button1 = 0;
            }
        } else {
            //LOG("\tbutton pos y threshold (%d)", y);
            if (southButton == TOUCHED_BUTTON0) {
                joy_button0 = 0x80;
                joy_button1 = 0;
                _setup_button_object_with_char(button0Char);
            } else if (southButton == TOUCHED_BUTTON1) {
                joy_button0 = 0;
                joy_button1 = 0x80;
                _setup_button_object_with_char(button1Char);
            } else if (southButton == TOUCHED_BOTH) {
                joy_button0 = 0x80;
                joy_button1 = 0x80;
                _setup_button_object_with_char(buttonBothChar);
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
                trackingAxisIndex = pointer_idx;
                axisCenterX = (int)x;
                axisCenterY = (int)y;
                _reset_model_position(touchAxisObjModel, x, y, AXIS_OBJ_HALF_W, AXIS_OBJ_HALF_H);
                axisModelDirty = true;
                LOG("---TOUCH %sDOWN (axis index %d) center:(%d,%d) -> joy(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "),
                        trackingAxisIndex, axisCenterX, axisCenterY, joy_x, joy_y);
            } else if (_is_point_on_button_side(x, y)) {
                buttonConsumed = true;
                trackingButtonIndex = pointer_idx;
                if (touchDownButton == TOUCHED_BUTTON0) {
                    joy_button0 = 0x80;
                    _setup_button_object_with_char(button0Char);
                } else if (touchDownButton == TOUCHED_BUTTON1) {
                    joy_button1 = 0x80;
                    _setup_button_object_with_char(button1Char);
                } else if (touchDownButton == TOUCHED_BOTH) {
                    joy_button0 = 0x80;
                    joy_button1 = 0x80;
                    _setup_button_object_with_char(buttonBothChar);
                }
                buttonCenterX = (int)x;
                buttonCenterY = (int)y;
                _reset_model_position(buttonObjModel, x, y, BUTTON_OBJ_HALF_W, BUTTON_OBJ_HALF_H);
                buttonModelDirty = true;
                LOG("---TOUCH %sDOWN (buttons index %d) center:(%d,%d) -> buttons(0x%02X,0x%02X)", (action == TOUCH_DOWN ? "" : "POINTER "),
                        trackingButtonIndex, buttonCenterX, buttonCenterY, joy_button0, joy_button1);
            } else {
                // not tracking tap/gestures originating from control-gesture portion of screen
            }
            break;

        case TOUCH_MOVE:
            if (trackingAxisIndex >= 0) {
                axisConsumed = true;
                _move_joystick_axis((int)x_coords[trackingAxisIndex], (int)y_coords[trackingAxisIndex]);
                LOG("---TOUCH MOVE ...tracking axis:%d (%d,%d) -> joy(0x%02X,0x%02X)", trackingAxisIndex,
                        (int)x_coords[trackingAxisIndex], (int)y_coords[trackingAxisIndex], joy_x, joy_y);
            }
            if (trackingButtonIndex >= 0) {
                buttonConsumed = true;
                _move_button_axis((int)x_coords[trackingButtonIndex], (int)y_coords[trackingButtonIndex]);
                LOG("+++TOUCH MOVE ...tracking button:%d (%d,%d) -> buttons(0x%02X,0x%02X)", trackingButtonIndex,
                        (int)x_coords[trackingButtonIndex], (int)y_coords[trackingButtonIndex], joy_button0, joy_button1);
            }
            break;

        case TOUCH_UP:
        case TOUCH_POINTER_UP:
            if (pointer_idx == trackingAxisIndex) {
                axisConsumed = true;
                bool resetIndex = false;
                if (trackingButtonIndex > trackingAxisIndex) {
                    // TODO FIXME : is resetting the pointer index just an Android-ism?
                    resetIndex = true;
                    --trackingButtonIndex;
                }
                trackingAxisIndex = TOUCHED_NONE;
                joy_x = HALF_JOY_RANGE;
                joy_y = HALF_JOY_RANGE;
                LOG("---TOUCH %sUP (axis went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset buttons index!)" : ""));
            } else if (pointer_idx == trackingButtonIndex) {
                buttonConsumed = true;
                bool resetIndex = false;
                if (trackingAxisIndex > trackingButtonIndex) {
                    // TODO FIXME : is resetting the pointer index just an Android-ism?
                    resetIndex = true;
                    --trackingAxisIndex;
                }
                trackingButtonIndex = TOUCHED_NONE;
                joy_button0 = 0x0;
                joy_button1 = 0x0;
                LOG("---TOUCH %sUP (buttons went up)%s", (action == TOUCH_UP ? "" : "POINTER "), (resetIndex ? " (reset axis index!)" : ""));
            } else {
                // not tracking tap/gestures originating from control-gesture portion of screen
            }
            break;

        case TOUCH_CANCEL:
            LOG("---TOUCH CANCEL");
            trackingAxisIndex = TOUCHED_NONE;
            trackingButtonIndex = TOUCHED_NONE;
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
        clock_gettime(CLOCK_MONOTONIC, &axisTimingBegin);
    }
    if (buttonConsumed) {
        clock_gettime(CLOCK_MONOTONIC, &buttonTimingBegin);
    }

    return (axisConsumed || buttonConsumed);
}

static bool gltouchjoy_isTouchJoystickAvailable(void) {
    return isAvailable;
}

static void gltouchjoy_setTouchJoyEnabled(bool enabled) {
    isEnabled = enabled;
}

static void gltouchjoy_setTouchButtonValues(char c0, char c1) {
    button0Char = c0;
    button1Char = c1;
    _setup_button_object();
}

static void gltouchjoy_setTouchAxisType(touchjoy_axis_type_t axisType) {
    touchjoy_axisType = axisType;
    _setup_axis_object();
}

static void gltouchjoy_setTouchAxisValues(char north, char west, char east, char south) {
    northChar = north;
    westChar  = west;
    eastChar  = east;
    southChar = south;
    if (touchjoy_axisType == AXIS_EMULATED_KEYBOARD) {
        _setup_axis_object();
    }
}

__attribute__((constructor))
static void _init_gltouchjoy(void) {
    LOG("Registering OpenGL software touch joystick");

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
