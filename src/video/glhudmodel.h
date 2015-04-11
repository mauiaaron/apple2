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

#ifndef _GLHUDMODEL_H_
#define _GLHUDMODEL_H_

#include "common.h"
#include "video_util/modelUtil.h"

MODEL_CLASS(GLModelHUDElement,
    void *(*render)(GLModel *parent);

    char *tpl;                  // ASCII template
    unsigned int tplWidth;      // template width
    unsigned int tplHeight;     // template height

    uint8_t *pixels;            // raw texture/FB data
    unsigned int pixWidth;      // FB width
    unsigned int pixHeight;     // FB height

    bool blackIsTransparent;
);

// default model creation
extern void *(*createDefaultGLModelHUDElement)(void);

// default model setup
extern void (*setupDefaultGLModelHUDElement)(GLModel *parent);

// default model destruction
extern void (*destroyDefaultGLModelHUDElement)(GLModel *parent);

#endif
