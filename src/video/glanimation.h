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

#ifndef _GLANIMATION_H_
#define _GLANIMATION_H_

typedef void (*glanim_ctor_fn)(void);
typedef void (*glanim_dtor_fn)(void);
typedef void (*glanim_render_fn)(void);

typedef struct glanim_t {
    glanim_ctor_fn ctor;
    glanim_dtor_fn dtor;
    glanim_render_fn render;
} glanim_t;

// register an animation
void gldriver_register_animation(glanim_t *anim);

// initialize animations module
void gldriver_animation_init(void);

// destroy animations module
void gldriver_animation_destroy(void);

// renders the animation
void gldriver_animation_render(void);

#endif

