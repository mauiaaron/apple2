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

#define MODEL_DEPTH -0.0625

#define CPUTIMING_TEMPLATE_COLS 8
#define CPUTIMING_TEMPLATE_ROWS 3

// HACK NOTE FIXME TODO : interpolated pixel adjustment still necessary ...
#define MESSAGE_FB_WIDTH ((CPUTIMING_TEMPLATE_COLS * FONT80_WIDTH_PIXELS) + INTERPOLATED_PIXEL_ADJUSTMENT)
#define MESSAGE_FB_HEIGHT (CPUTIMING_TEMPLATE_ROWS * FONT_HEIGHT_PIXELS)

static bool isAvailable = false;
static bool isEnabled = true;

static struct timespec cputiming_begin = { 0 };

static GLModel *cpuMessageObjModel = NULL;

static glanim_t cpuMessageAnimation = { 0 };

// ----------------------------------------------------------------------------

static void _cpuanim_show(GLModel *parent) {
    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;

    if (hudElement->tpl == NULL) {
        // deferred construction ...
        const char messageTemplate[CPUTIMING_TEMPLATE_ROWS][CPUTIMING_TEMPLATE_COLS+1] = {
            "||||||||",
            "| xxx% |",
            "||||||||",
        };

        const unsigned int size = sizeof(messageTemplate);
        hudElement->tpl = calloc(size, 1);
        hudElement->tplWidth = CPUTIMING_TEMPLATE_COLS;
        hudElement->tplHeight = CPUTIMING_TEMPLATE_ROWS;
        memcpy(hudElement->tpl, messageTemplate, size);

        hudElement->pixels = calloc(MESSAGE_FB_WIDTH * MESSAGE_FB_HEIGHT, 1);
        hudElement->pixWidth = MESSAGE_FB_WIDTH;
        hudElement->pixHeight = MESSAGE_FB_HEIGHT;
    }

    const unsigned int row = (CPUTIMING_TEMPLATE_COLS+1);

    char buf[8] = { 0 };
    double scale = (alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor);
    int percentScale = scale * 100;
    if (percentScale < 100.0) {
        snprintf(buf, 3, "%d", percentScale);
        ((hudElement->tpl)+(row*1))[2] = ' ';
        ((hudElement->tpl)+(row*1))[3] = buf[0];
        ((hudElement->tpl)+(row*1))[4] = buf[1];
    } else if (scale == CPU_SCALE_FASTEST) {
        ((hudElement->tpl)+(row*1))[2] = 'm';
        ((hudElement->tpl)+(row*1))[3] = 'a';
        ((hudElement->tpl)+(row*1))[4] = 'x';
    } else {
        snprintf(buf, 4, "%d", percentScale);
        ((hudElement->tpl)+(row*1))[2] = buf[0];
        ((hudElement->tpl)+(row*1))[3] = buf[1];
        ((hudElement->tpl)+(row*1))[4] = buf[2];
    }

    glhud_setupDefault(parent);

    clock_gettime(CLOCK_MONOTONIC, &cputiming_begin);
}

static void cpuanim_init(void) {
    LOG("cpuanim_init ...");

    mdlDestroyModel(&cpuMessageObjModel);
    cpuMessageObjModel = mdlCreateQuad(-0.3, -0.3, 0.6, 0.6, MODEL_DEPTH, MESSAGE_FB_WIDTH, MESSAGE_FB_HEIGHT, GL_RGBA/*RGBA_8888*/, (GLCustom){
            .create = &glhud_createDefault,
            .setup = &_cpuanim_show,
            .destroy = &glhud_destroyDefault,
            });
    if (!cpuMessageObjModel) {
        LOG("not initializing CPU speed animations");
        return;
    }

    isAvailable = true;
}

static void cpuanim_destroy(void) {
    LOG("gldriver_animation_destroy ...");
    if (!isAvailable) {
        return;
    }
    isAvailable = false;
    mdlDestroyModel(&cpuMessageObjModel);
}

static void cpuanim_render(void) {
    if (!isAvailable) {
        return;
    }
    if (!isEnabled) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    float alpha = 0.95;
    struct timespec deltat = timespec_diff(cputiming_begin, now, NULL);
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
    glBindTexture(GL_TEXTURE_2D, cpuMessageObjModel->textureName);
    if (cpuMessageObjModel->texDirty) {
        cpuMessageObjModel->texDirty = false;
        glTexImage2D(GL_TEXTURE_2D, /*level*/0, /*internal format*/GL_RGBA, MESSAGE_FB_WIDTH, MESSAGE_FB_HEIGHT, /*border*/0, /*format*/GL_RGBA, GL_UNSIGNED_BYTE, cpuMessageObjModel->texPixels);
    }
    glUniform1i(uniformTex2Use, TEXTURE_ID_MESSAGE);
    glhud_renderDefault(cpuMessageObjModel);
}

static void cpuanim_reshape(int w, int h) {
    // no-op
}

static void cpuanim_show(void) {
    if (!isAvailable) {
        return;
    }
    _cpuanim_show(cpuMessageObjModel);
    isEnabled = true;
}

__attribute__((constructor))
static void _init_glcpuanim(void) {
    LOG("Registering CPU speed animations");
    video_animation_show_cpuspeed = &cpuanim_show;
    cpuMessageAnimation.ctor = &cpuanim_init;
    cpuMessageAnimation.dtor = &cpuanim_destroy;
    cpuMessageAnimation.render = &cpuanim_render;
    cpuMessageAnimation.reshape = &cpuanim_reshape;
    gldriver_register_animation(&cpuMessageAnimation);
}

