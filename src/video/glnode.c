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
#include "video/glnode.h"
#include "video/glvideo.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// WARNING : linked list designed for convenience and not performance =P ... if the amount of GLNode objecs grows
// wildly, should rethink this ...
typedef struct glnode_array_node_s {
    struct glnode_array_node_s *next;
    glnode_render_order_t order;
    GLNode node;
} glnode_array_node_s;

static glnode_array_node_s *glNodes = NULL;

// -----------------------------------------------------------------------------

void glnode_registerNode(glnode_render_order_t order, GLNode node) {
    pthread_mutex_lock(&mutex);

    glnode_array_node_s *arrayNode = malloc(sizeof(glnode_array_node_s));
    assert(arrayNode);
    arrayNode->next = NULL;
    arrayNode->order = order;
    arrayNode->node = node;

    if (glNodes == NULL) {
        glNodes = arrayNode;
    } else {
        glnode_array_node_s *p = glNodes;
        while ((order < p->order) && p->next) {
            p = p->next;
        }
        glnode_array_node_s *q = p->next;
        p->next = arrayNode;
        arrayNode->next = q;
    }

    pthread_mutex_unlock(&mutex);
}

void glnode_setupNodes(void) {
    LOG("glnode_setupNodes ...");
    glnode_array_node_s *p = glNodes;
    while (p) {
        p->node.setup();
        p = p->next;
    }
}

void glnode_shutdownNodes(void) {
    LOG("glnode_shutdownNodes ...");
    glnode_array_node_s *p = glNodes;
    while (p) {
        p->node.shutdown();
        p = p->next;
    }
}

void glnode_renderNodes(void) {
    glnode_array_node_s *p = glNodes;
    while (p) {
        p->node.render();
        p = p->next;
    }
}

void glnode_reshapeNodes(int w, int h) {
    glnode_array_node_s *p = glNodes;
    while (p) {
        p->node.reshape(w, h);
        p = p->next;
    }
}

#if INTERFACE_TOUCH
bool glnode_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {
    glnode_array_node_s *p = glNodes;
    bool handled = false;
    while (p) {
        handled = p->node.onTouchEvent(action, pointer_count, pointer_idx, x_coords, y_coords);
        if (handled) {
            break;
        }
        p = p->next;
    }
    return handled;
}
#endif

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_glnode_manager(void) {
    LOG("Initializing GLNode manager subsystem");
#if INTERFACE_TOUCH
    interface_onTouchEvent = &glnode_onTouchEvent;
#endif
}
