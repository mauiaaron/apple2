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
#include "video/glanimation.h"
#include "video/glvideo.h"

void (*video_animation_show_cpuspeed)(void) = NULL;
void (*video_animation_show_track_sector)(int drive, int track, int sect) = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct glanim_array_node_t {
    struct glanim_array_node_t *next;
    glanim_t *anim;
} glanim_array_node_t;

static glanim_array_node_t *animations = NULL;

// -----------------------------------------------------------------------------

void gldriver_register_animation(glanim_t *anim) {
    pthread_mutex_lock(&mutex);

    glanim_array_node_t *node = malloc(sizeof(glanim_array_node_t));
    assert(node);
    node->next = NULL;
    node->anim = anim;

    if (animations == NULL) {
        animations = node;
    } else {
        glanim_array_node_t *p = animations;
        while (p->next) {
            p = p->next;
        }
        p->next = node;
    }

    pthread_mutex_unlock(&mutex);
}

void gldriver_animation_init(void) {
    LOG("gldriver_animation_init ...");
    glanim_array_node_t *p = animations;
    while (p) {
        p->anim->ctor();
        p = p->next;
    }
}

void gldriver_animation_destroy(void) {
    LOG("gldriver_animation_destroy ...");
    glanim_array_node_t *p = animations;
    while (p) {
        p->anim->dtor();
        p = p->next;
    }
}

void gldriver_animation_render(void) {
    glanim_array_node_t *p = animations;
    while (p) {
        p->anim->render();
        p = p->next;
    }
}

void gldriver_animation_reshape(int w, int h) {
    glanim_array_node_t *p = animations;
    while (p) {
        p->anim->reshape(w, h);
        p = p->next;
    }
}

