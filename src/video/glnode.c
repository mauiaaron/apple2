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

// WARNING : linked list designed for convenience and not performance =P ... if the amount of GLNode objects grows
// wildly, should rethink this ...
typedef struct glnode_array_node_s {
    struct glnode_array_node_s *next;
    struct glnode_array_node_s *last;
    glnode_render_order_t order;
    GLNode node;
} glnode_array_node_s;

static glnode_array_node_s *head = NULL;
static glnode_array_node_s *tail = NULL;

// -----------------------------------------------------------------------------

void glnode_registerNode(glnode_render_order_t order, GLNode node) {
    pthread_mutex_lock(&mutex);

    glnode_array_node_s *arrayNode = malloc(sizeof(glnode_array_node_s));
    assert(arrayNode);
    arrayNode->next = NULL;
    arrayNode->last = NULL;
    arrayNode->order = order;
    arrayNode->node = node;

    if (head == NULL) {
        assert(tail == NULL);
        head = arrayNode;
        tail = arrayNode;
    } else {
        glnode_array_node_s *p0 = NULL;
        glnode_array_node_s *p = head;
        while (p && (order < p->order)) {
            p0 = p;
            p = p->next;
        }
        if (p0) {
            p0->next = arrayNode;
        } else {
            head = arrayNode;
        }
        if (p) {
            p->last = arrayNode;
        } else {
            tail = arrayNode;
        }
        arrayNode->next = p;
        arrayNode->last = p0;
    }

    pthread_mutex_unlock(&mutex);
}

void glnode_setupNodes(void) {
    LOG("glnode_setupNodes ...");
    glnode_array_node_s *p = head;
    while (p) {
        p->node.setup();
        p = p->next;
    }
}

void glnode_shutdownNodes(void) {
    LOG("glnode_shutdownNodes ...");
    glnode_array_node_s *p = head;
    while (p) {
        p->node.shutdown();
        p = p->next;
    }
}

void glnode_renderNodes(void) {
    glnode_array_node_s *p = tail;
    while (p) {
        p->node.render();
        p = p->last;
    }
}

void glnode_reshapeNodes(int w, int h) {
    glnode_array_node_s *p = head;
    while (p) {
        p->node.reshape(w, h);
        p = p->next;
    }
}

#if INTERFACE_TOUCH
int64_t glnode_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {
    glnode_array_node_s *p = head;
    int64_t flags = 0x0;
    while (p) {
        flags = p->node.onTouchEvent(action, pointer_count, pointer_idx, x_coords, y_coords);
        if (flags & TOUCH_FLAGS_HANDLED) {
            break;
        }
        p = p->next;
    }
    return flags;
}
#endif

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_glnode_manager(void) {
    LOG("Initializing GLNode manager subsystem");
#if INTERFACE_TOUCH
    interface_onTouchEvent = &glnode_onTouchEvent;
#endif
}
