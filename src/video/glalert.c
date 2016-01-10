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

#define MODEL_DEPTH -0.0625

static bool isEnabled = true;
static pthread_mutex_t messageMutex = PTHREAD_MUTEX_INITIALIZER;
static char *nextMessage = NULL;
static unsigned int nextMessageCols = 0;
static unsigned int nextMessageRows = 0;
static struct timespec messageTimingBegin = { 0 };
static GLModel *messageModel = NULL;

// ----------------------------------------------------------------------------

static void *_create_alert(GLModel *parent) {
    parent->custom = glhud_createDefault();
    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;
    if (hudElement) {
        hudElement->colorScheme = RED_ON_BLACK;
        hudElement->blackIsTransparent = false;
        hudElement->opaquePixelHalo = false;
    }
    return hudElement;
}

static inline void _set_alpha(unsigned int dstIdx) {
#if USE_RGBA4444
#error fixme verify ...
#else
    PIXEL_TYPE rgba = *((PIXEL_TYPE *)(messageModel->texPixels + dstIdx));
    rgba &= (~(0xFF << SHIFT_A));
    *( (PIXEL_TYPE *)(messageModel->texPixels + dstIdx) ) = rgba;
#endif
}

static void _alertToModel(char *message, unsigned int messageCols, unsigned int messageRows) {
    assert(message);

    isEnabled = false;

    do {
        // create model object

        mdlDestroyModel(&messageModel);

        const unsigned int fbWidth = (messageCols * FONT80_WIDTH_PIXELS);
        const unsigned int fbHeight = (messageRows * FONT_HEIGHT_PIXELS);

        messageModel = mdlCreateQuad((GLModelParams_s){
                .skew_x = -0.3,
                .skew_y = -0.3,
                .z = MODEL_DEPTH,
                .obj_w = 0.7,
                .obj_h = 0.7,
                .positionUsageHint = GL_STATIC_DRAW, // positions don't change
                .tex_w = fbWidth,
                .tex_h = fbHeight,
                .texcoordUsageHint = GL_DYNAMIC_DRAW, // but texture (message pixels) does
            }, (GLCustom){
                .create = &_create_alert,
                .destroy = &glhud_destroyDefault,
            });
        if (!messageModel) {
            LOG("OOPS cannot create animation message HUD model!");
            break;
        }

        // setup custom message HUD elements

        GLModelHUDElement *hudElement = (GLModelHUDElement *)messageModel->custom;
        hudElement->tplWidth = messageCols;
        hudElement->tplHeight = messageRows;
        hudElement->tpl = message;
        hudElement->pixWidth = fbWidth;
        hudElement->pixHeight = fbHeight;
        hudElement->pixels = MALLOC(fbWidth * fbHeight);
        if (!hudElement->pixels) {
            LOG("OOPS cannot create animation message intermediate framebuffer!");
            break;
        }
        glhud_setupDefault(messageModel);

        if (1) {
            // HACK : removing opaque borders is based on well-known glyph bitmaps in font.txt
            const unsigned int glyphTopBorder = 4;
            const unsigned int glyphXBorder = 3;
            const unsigned int heightMultiplier = 2; // each bitmap pixel is scaled 2x
            const unsigned int pixStride = sizeof(PIXEL_TYPE);
            unsigned int dstIdx=0;

            // clean up top border
            const unsigned int topBorderCount = (fbWidth * pixStride * ((glyphTopBorder*heightMultiplier)-1));
            for (; dstIdx < topBorderCount; dstIdx+=pixStride) {
                _set_alpha(dstIdx);
            }

            // clean up left and right edges
            const unsigned int innerHeight = (fbHeight - /* 1/2 top and 1/2 bottom border: */FONT_HEIGHT_PIXELS) + (2*heightMultiplier);
            const unsigned int lrBorderWidth = ((glyphXBorder-1) * pixStride);
            for (unsigned int pixRow=0; pixRow<innerHeight; pixRow++) {
                for (unsigned int ltIdx=0; ltIdx < (glyphXBorder-1); ltIdx++, dstIdx+=pixStride) {
                    _set_alpha(dstIdx);
                }
                dstIdx += (fbWidth * pixStride) - (lrBorderWidth*2);
                for (unsigned int rtIdx=0; rtIdx < (glyphXBorder-1); rtIdx++, dstIdx+=pixStride) {
                    _set_alpha(dstIdx);
                }
            }

            assert(dstIdx < (fbHeight * fbWidth * pixStride) && "overflow detected");

            // clean up bottom border
            for (; dstIdx<(fbHeight * fbWidth * pixStride); dstIdx+=pixStride) {
                _set_alpha(dstIdx);
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &messageTimingBegin);
        isEnabled = true;
        return;

    } while (0);

    // error
    mdlDestroyModel(&messageModel);
}

// ----------------------------------------------------------------------------
// GLNode functions

static void alert_init(void) {
    // no-op
}

static void alert_shutdown(void) {
    LOG("alert_shutdown ...");
    mdlDestroyModel(&messageModel);
}

static void alert_render(void) {
    if (nextMessage) {
        pthread_mutex_lock(&messageMutex);
        char *message = nextMessage;
        int cols = nextMessageCols;
        int rows = nextMessageRows;
        nextMessage = NULL;
        pthread_mutex_unlock(&messageMutex);
        _alertToModel(message, cols, rows);
    }

    if (!isEnabled) {
        return;
    }

    if (!messageModel) {
        return;
    }

    struct timespec now = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &now);

    float alpha = 0.95;
    struct timespec deltat = timespec_diff(messageTimingBegin, now, NULL);
    if (deltat.tv_sec >= 1) {
        isEnabled = false;
        return;
    } else if (deltat.tv_nsec >= NANOSECONDS_PER_SECOND/2) {
        alpha -= ((float)deltat.tv_nsec-(NANOSECONDS_PER_SECOND/2)) / (float)(NANOSECONDS_PER_SECOND/2);
        if (alpha < 0.0) {
            alpha = 0.0;
        }
    }
    //LOG("alpha : %f", alpha);
    glUniform1f(alphaValue, alpha);

    glActiveTexture(TEXTURE_ACTIVE_MESSAGE);
    glBindTexture(GL_TEXTURE_2D, messageModel->textureName);
    if (messageModel->texDirty) {
        messageModel->texDirty = false;
        _HACKAROUND_GLTEXIMAGE2D_PRE(TEXTURE_ACTIVE_MESSAGE, messageModel->textureName);
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, TEX_FORMAT_INTERNAL, messageModel->texWidth, messageModel->texHeight, /*border*/0, TEX_FORMAT, TEX_TYPE, messageModel->texPixels);
    }
    glUniform1i(texSamplerLoc, TEXTURE_ID_MESSAGE);
    glhud_renderDefault(messageModel);
}

static void alert_reshape(int w, int h) {
    // no-op
}

#if INTERFACE_TOUCH
static int64_t alert_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {
    return false; // non-interactive element ...
}
#endif

// ----------------------------------------------------------------------------
// Various animations

static void _animation_showMessage(char *messageTemplate, unsigned int cols, unsigned int rows) {
    // frame the message with interface border characters
    const unsigned int framedCols = cols+2;
    const unsigned int framedRows = rows+2;
    const unsigned int framedStride = framedCols+1/*\0*/;
    const unsigned int sourceStride = cols+1/*\0*/;

    char *message = CALLOC(framedStride*framedRows, 1);
    if (!message) {
        LOG("OOPS cannot create memory for animation message!");
        return;
    }
    memset(message, '|', framedCols);
    unsigned int indexSource = 0;
    unsigned int indexFramed = 0;
    unsigned int row = 0;
    for (; row<rows; row++) {
        indexSource = row*sourceStride;
        indexFramed = (row+1)*framedStride;
        *(message+indexFramed) = '|';
        memcpy(message+indexFramed+1, messageTemplate+indexSource, cols);
        *(message+indexFramed+1+cols) = '|';
    }
    indexFramed = (row+1)*framedStride;
    memset(message+indexFramed, '|', framedCols);

    pthread_mutex_lock(&messageMutex);
    FREE(nextMessage); // drop this one if it exists
    nextMessage = message;
    nextMessageCols = framedCols;
    nextMessageRows = framedRows;
    pthread_mutex_unlock(&messageMutex);
}

static void _animation_showPaused(void) {
#define PAUSED_ANIMATION_ROWS 1
#define PAUSED_ANIMATION_COLS 3
    static char pausedTemplate[PAUSED_ANIMATION_ROWS][PAUSED_ANIMATION_COLS+1] = {
        " @ ",
    };
    pausedTemplate[0][1] = MOUSETEXT_HOURGLASS;
    _animation_showMessage(pausedTemplate[0], PAUSED_ANIMATION_COLS, PAUSED_ANIMATION_ROWS);
}

static void _animation_showCPUSpeed(void) {

#define CPU_ANIMATION_ROWS 1
#define CPU_ANIMATION_COLS 6
    static char cpuTemplate[CPU_ANIMATION_ROWS][CPU_ANIMATION_COLS+1] = {
        " xxx% ",
    };

    char buf[8] = { 0 };
    double scale = (alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor);
    int percentScale = scale * 100;
    if (percentScale < 100.0) {
        snprintf(buf, 3, "%d", percentScale);
        cpuTemplate[0][1] = ' ';
        cpuTemplate[0][2] = buf[0];
        cpuTemplate[0][3] = buf[1];
    } else if (scale == CPU_SCALE_FASTEST) {
        cpuTemplate[0][1] = 'm';
        cpuTemplate[0][2] = 'a';
        cpuTemplate[0][3] = 'x';
    } else {
        snprintf(buf, 4, "%d", percentScale);
        cpuTemplate[0][1] = buf[0];
        cpuTemplate[0][2] = buf[1];
        cpuTemplate[0][3] = buf[2];
    }

    _animation_showMessage(cpuTemplate[0], CPU_ANIMATION_COLS, CPU_ANIMATION_ROWS);
}

static void _animation_showDiskChosen(int drive) {

    char *template = NULL;
#define DISK_ANIMATION_ROWS 3
#define DISK_ANIMATION_COLS 3
    unsigned int shownCols = DISK_ANIMATION_COLS;

    if (disk6.disk[drive].is_protected) {
        static char diskInsertedTemplate[DISK_ANIMATION_ROWS][DISK_ANIMATION_COLS+1] = {
            "DD ",
            "DD ",
            " >1",
        };
        template = diskInsertedTemplate[0];
    } else {
        shownCols = DISK_ANIMATION_COLS+1;
        static char diskInsertedTemplate[DISK_ANIMATION_ROWS][DISK_ANIMATION_COLS+2] = {
            "DD  ",
            "DD  ",
            " >1L",
        };
        diskInsertedTemplate[2][3] = ICONTEXT_UNLOCK;
        template = diskInsertedTemplate[0];
    }

    const unsigned int x = (shownCols+1);// stride
    (template+x*0)[0] = ICONTEXT_DISK_UL;
    (template+x*0)[1] = ICONTEXT_DISK_UR;
    (template+x*1)[0] = ICONTEXT_DISK_LL;
    (template+x*1)[1] = ICONTEXT_DISK_LR;

    (template+x*2)[1] = ICONTEXT_GOTO;
    (template+x*2)[2] = (drive == 0) ? '1' : '2';

    _animation_showMessage(template, shownCols, DISK_ANIMATION_ROWS);
}

static void _animation_showTrackSector(int drive, int track, int sect) {

#define DISK_TRACK_SECTOR_ROWS 3
#define DISK_TRACK_SECTOR_COLS 8

    static char diskTrackSectorTemplate[DISK_TRACK_SECTOR_ROWS][DISK_TRACK_SECTOR_COLS+1] = {
        "        ",
        "D / TT/S",
        "        ",
    };
    char *template = diskTrackSectorTemplate[0];

    char c = diskTrackSectorTemplate[1][2];
    switch (c) {
        case '/':
            c = '-';
            break;
        case '-':
            c = '\\';
            break;
        case '\\':
            c = '|';
            break;
        case '|':
            c = '/';
            break;
        default:
            assert(false && "should not happen");
            break;
    }
    snprintf(&diskTrackSectorTemplate[1][0], DISK_TRACK_SECTOR_COLS+1, "%d %c %02X/%01X", drive+1, c, track, sect);

    _animation_showMessage(template, DISK_TRACK_SECTOR_COLS, DISK_TRACK_SECTOR_ROWS);
}

static void _animation_setEnableShowTrackSector(bool enabled) {
    if (enabled) {
        video_backend->animation_showTrackSector = &_animation_showTrackSector;
    } else {
        video_backend->animation_showTrackSector = NULL;
    }
}

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_glalert(void) {
    LOG("Initializing message animation subsystem");

    video_backend->animation_showMessage = &_animation_showMessage;
    video_backend->animation_showPaused = &_animation_showPaused;
    video_backend->animation_showCPUSpeed = &_animation_showCPUSpeed;
    video_backend->animation_showDiskChosen = &_animation_showDiskChosen;
    video_backend->animation_showTrackSector = &_animation_showTrackSector;
    video_backend->animation_setEnableShowTrackSector = &_animation_setEnableShowTrackSector;

    glnode_registerNode(RENDER_MIDDLE, (GLNode){
        .setup = &alert_init,
        .shutdown = &alert_shutdown,
        .render = &alert_render,
        .reshape = &alert_reshape,
#if INTERFACE_TOUCH
        .onTouchEvent = &alert_onTouchEvent,
#endif
    });
}

