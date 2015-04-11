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

#include "glhudmodel.h"

void *(*createDefaultGLModelHUDElement)(void) = NULL;
void (*setupDefaultGLModelHUDElement)(GLModel *parent) = NULL;
void (*destroyDefaultGLModelHUDElement)(GLModel *parent) = NULL;

static void *createDefault(void) {
    GLModelHUDElement *custom = (GLModelHUDElement *)calloc(sizeof(GLModelHUDElement), 1);
    if (!custom) {
        return NULL;
    }
    return custom;
}

static void setupDefault(GLModel *parent) {

    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;
    char *submenu = (char *)(hudElement->tpl);
    const unsigned int cols = hudElement->tplWidth;
    const unsigned int rows = hudElement->tplHeight;
    uint8_t *fb = hudElement->pixels;
    const unsigned int fb_w = hudElement->pixWidth;
    const unsigned int fb_h = hudElement->pixHeight;

    // render template into indexed fb
    const unsigned int submenu_width = cols;
    const unsigned int submenu_height = rows;
    video_interface_print_submenu_centered_fb(fb, submenu_width, submenu_height, submenu, submenu_width, submenu_height);

    // generate RGBA_8888 from indexed color
    const unsigned int count = fb_w * fb_h;
    for (unsigned int i=0, j=0; i<count; i++, j+=4) {
        uint8_t index = *(fb + i);
        uint32_t rgb = (((uint32_t)(colormap[index].red)   << 0 ) |
                        ((uint32_t)(colormap[index].green) << 8 ) |
                        ((uint32_t)(colormap[index].blue)  << 16));
        if (rgb == 0 && hudElement->blackIsTransparent) {
            // make black transparent
        } else {
            rgb |=      ((uint32_t)0xff                    << 24);
        }
        *( (uint32_t*)(parent->texPixels + j) ) = rgb;
    }
    parent->texDirty = true;
}

static void destroyDefault(GLModel *parent) {
    GLModelHUDElement *hudElement = (GLModelHUDElement *)parent->custom;
    if (!hudElement) {
        return;
    }

    if (hudElement->tpl) {
        FREE(hudElement->tpl);
    }

    if (hudElement->pixels) {
        FREE(hudElement->pixels);
    }

    FREE(parent->custom);
}

__attribute__((constructor))
static void _init_glhudmodel_common(void) {
    LOG("Setting up default GL HUD model stuff");
    createDefaultGLModelHUDElement = &createDefault;
    setupDefaultGLModelHUDElement = &setupDefault;
    destroyDefaultGLModelHUDElement = &destroyDefault;
}
