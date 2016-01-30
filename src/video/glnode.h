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

#ifndef _GLNODE_H_
#define _GLNODE_H_

#include "common.h"

#define TRACKING_NONE (-1)

typedef enum glnode_render_order_t {
    RENDER_BOTTOM=0,    // e.g., the //e framebuffer node itself
    RENDER_LOW   =1,    // e.g., the touchjoy and touchkbd
    RENDER_MIDDLE=10,   // e.g., floating messages
    RENDER_TOP   =20,   // e.g., the HUD menu items
} glnode_render_order_t;

// renderable and potentially interactive node
typedef struct GLNode {
    void (*setup)(void);
    void (*shutdown)(void);
    void (*render)(void);
#if INTERFACE_TOUCH
    int64_t (*onTouchEvent)(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords);
#endif
    void (*reshape)(int w, int h);
} GLNode;

// registers a node with manager
void glnode_registerNode(glnode_render_order_t order, GLNode node);

// setup nodes (occurs when OpenGL context created)
void glnode_setupNodes(void);

// shutdown nodes (occurs when OpenGL context about to be lost)
void glnode_shutdownNodes(void);

// render pass over all nodes in their requested render order
void glnode_renderNodes(void);

// distribute viewport dimension changes
void glnode_reshapeNodes(int w, int h);

#if INTERFACE_TOUCH
// distribute touch event to node which can handle it (in render order)
int64_t glnode_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords);
#endif

#endif

