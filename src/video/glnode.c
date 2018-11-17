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

#include "video/glnode.h"
#include "video/glinput.h"

bool safe_to_do_opengl_logging = false;

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

static video_backend_s glnode_backend = { 0 };
video_animation_s glnode_animations = { 0 };

#if USE_GLUT
static bool glut_in_main_loop = false;
#endif

// -----------------------------------------------------------------------------

void glnode_registerNode(glnode_render_order_t order, GLNode node) {

    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);

    glnode_array_node_s *arrayNode = MALLOC(sizeof(glnode_array_node_s));
    assert((uintptr_t)arrayNode);
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
        while (p && (order > p->order)) {
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

// -----------------------------------------------------------------------------

#if USE_GLUT
static void _glnode_updateGLUT(int unused) {
#if FPS_LOG
    static uint32_t prevCount = 0;
    static uint32_t idleCount = 0;

    idleCount++;

    static struct timespec prev = { 0 };
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (now.tv_sec != prev.tv_sec) {
        LOG("%u", idleCount-prevCount);
        prevCount = idleCount;
        prev = now;
    }
#endif

    c_keys_handle_input(-1, 0, 0);
    glutPostRedisplay();
    glutTimerFunc(17, _glnode_updateGLUT, 0);
}

static void _glnode_initGLUTPre(void) {
    glutInit(&argc, argv);
    glutInitDisplayMode(/*GLUT_DOUBLE|*/GLUT_RGBA);
    glutInitWindowSize(SCANWIDTH*1.5, SCANHEIGHT*1.5);
    //glutInitContextVersion(4, 0); -- Is this needed?
    glutInitContextProfile(GLUT_CORE_PROFILE);
    /*glutWindow = */glutCreateWindow(PACKAGE_NAME);
    GL_ERRQUIT("GLUT initialization");

    if (glewInit()) {
        ERRQUIT("Unable to initialize GLEW");
    }
}

static void _glnode_reshapeGLUT(int w, int h) {
    prefs_setLongValue(PREF_DOMAIN_INTERFACE, PREF_DEVICE_WIDTH, w);
    prefs_setLongValue(PREF_DOMAIN_INTERFACE, PREF_DEVICE_HEIGHT, h);
    prefs_setLongValue(PREF_DOMAIN_INTERFACE, PREF_DEVICE_LANDSCAPE, true);
    prefs_sync(PREF_DOMAIN_INTERFACE);
}

static void _glnode_initGLUTPost(void) {
    glutTimerFunc(16, _glnode_updateGLUT, 0);
    glutDisplayFunc(video_render);
    glutReshapeFunc(_glnode_reshapeGLUT);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    glutKeyboardFunc(gldriver_on_key_down);
    glutKeyboardUpFunc(gldriver_on_key_up);
    glutSpecialFunc(gldriver_on_key_special_down);
    glutSpecialUpFunc(gldriver_on_key_special_up);
    //glutMouseFunc(gldriver_mouse);
    //glutMotionFunc(gldriver_mouse_drag);
    c_joystick_reset();
}
#endif

static const char *glnode_name(void) {
    return "OpenGL";
}

static void glnode_setupNodes(void *ctx) {
    LOG("BEGIN glnode_setupNodes ...");

#if USE_GLUT
#   if !TEST_CPU
    joydriver_resetJoystick = &_glutJoystickReset;
#   endif
    _glnode_initGLUTPre();
#endif

    safe_to_do_opengl_logging = true;
    glnode_array_node_s *p = head;
    while (p) {
        p->node.setup();
        p = p->next;
    }

#if USE_GLUT
    _glnode_initGLUTPost();
#endif

    LOG("END glnode_setupNodes ...");
}

static void glnode_shutdownNodes(void) {
    LOG("BEGIN glnode_shutdownNodes ...");

#if USE_GLUT
    if (glut_in_main_loop) {
        assert(!emulator_isShuttingDown());
        glutLeaveMainLoop();
        return;
    }
#endif

    glnode_array_node_s *p = tail;
    while (p) {
        p->node.shutdown();
        p = p->last;
    }

    if (emulator_isShuttingDown()) {
        // clean up to make Valgrind happy ...
        p = head;
        while (p) {
            glnode_array_node_s *next = p->next;
            FREE(p);
            p = next;
        }
        head=NULL;
        tail=NULL;
    }

    LOG("END glnode_shutdownNodes ...");
}

static void glnode_renderNodes(void) {
    SCOPE_TRACE_VIDEO("glnode render");

    glClear(GL_COLOR_BUFFER_BIT);

    glnode_array_node_s *p = head;
    while (p) {
        p->node.render();
        p = p->next;
    }

#if USE_GLUT
    glutSwapBuffers();
#endif
}

#if INTERFACE_TOUCH
static int64_t glnode_onTouchEvent(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) {
    SCOPE_TRACE_TOUCH("glnode onTouchEvent");
    glnode_array_node_s *p = tail;
    int64_t flags = 0x0;
    while (p) {
        flags = p->node.onTouchEvent(action, pointer_count, pointer_idx, x_coords, y_coords);
        if (flags & TOUCH_FLAGS_HANDLED) {
            break;
        }
        p = p->last;
    }
    return flags;
}
#endif

static void glnode_mainLoop(void) {
#if USE_GLUT
    LOG("BEGIN GLUT main loop...");
    glut_in_main_loop = true;
    glutMainLoop();
    glut_in_main_loop = false;
    LOG("END GLUT main loop...");
#endif
}

//----------------------------------------------------------------------------

static void _init_glnode_manager(void) {
    LOG("Initializing GLNode manager subsystem");

    glnode_backend.name      = &glnode_name;
    glnode_backend.init      = &glnode_setupNodes;
    glnode_backend.main_loop = &glnode_mainLoop;
    glnode_backend.render    = &glnode_renderNodes;
    glnode_backend.shutdown  = &glnode_shutdownNodes;
    glnode_backend.anim      = &glnode_animations;

#if INTERFACE_CLASSIC
    glnode_backend.plotChar  = &display_plotChar;
    glnode_backend.plotLine  = &display_plotLine;
#endif

    glnode_backend.flashText = &display_flashText;
    glnode_backend.flushScanline = &display_flushScanline;
    glnode_backend.frameComplete = &display_frameComplete;

#if INTERFACE_TOUCH
    interface_onTouchEvent = &glnode_onTouchEvent;
#endif

    video_registerBackend(&glnode_backend, VID_PRIO_GRAPHICS_GL);
}

static __attribute__((constructor)) void __init_glnode_manager(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_EARLY, &_init_glnode_manager);
}

