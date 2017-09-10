/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2017 Aaron Culliney
 *
 */

#include "common.h"

typedef struct backend_node_s {
    struct backend_node_s *next;
    long order;
    video_backend_s *backend;
} backend_node_s;

static bool video_initialized = false;
static bool null_backend_running = true;
static backend_node_s *head = NULL;
static video_backend_s *currentBackend = NULL;
static pthread_t render_thread_id = 0;

// ----------------------------------------------------------------------------

void video_init(void) {
    video_initialized = true;

    assert(pthread_self() != cpu_thread_id);
    LOG("(re)setting render_thread_id : %lu -> %lu", (unsigned long)render_thread_id, (unsigned long)pthread_self());
    render_thread_id = pthread_self();

    video_clear();

    currentBackend->init((void*)0);
}

void _video_setRenderThread(pthread_t id) {
    LOG("setting render_thread_id : %lu -> %lu", (unsigned long)render_thread_id, (unsigned long)id);
    render_thread_id = id;
}

bool video_isRenderThread(void) {
    return (pthread_self() == render_thread_id);
}

void video_shutdown(void) {

#if MOBILE_DEVICE
    // WARNING : shutdown should occur on the render thread.  Platform code (iOS, Android) should ensure this is called
    // from within a render pass...
    assert(!render_thread_id || pthread_self() == render_thread_id);
#endif

    currentBackend->shutdown();
}

void video_render(void) {
    assert(pthread_self() == render_thread_id);
    currentBackend->render();
}

void video_main_loop(void) {
    currentBackend->main_loop();
}

void video_clear(void) {
    video_setDirty(A2_DIRTY_FLAG);
}

bool video_saveState(StateHelper_s *helper) {
    bool saved = false;
    int fd = helper->fd;

    do {
        uint8_t state = 0x0;
        if (!helper->save(fd, &state, 1)) {
            break;
        }
        LOG("SAVE (no-op) video__current_page = %02x", state);

        saved = true;
    } while (0);

    return saved;
}

bool video_loadState(StateHelper_s *helper) {
    bool loaded = false;
    int fd = helper->fd;

    do {
        uint8_t state = 0x0;

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD (no-op) video__current_page = %02x", state);

        loaded = true;
    } while (0);

    return loaded;
}

// ----------------------------------------------------------------------------
// Video backend registration and selection

void video_registerBackend(video_backend_s *backend, long order) {
    assert(!video_initialized); // backends cannot be registered after we've picked one to use

    backend_node_s *node = MALLOC(sizeof(backend_node_s));
    assert(node);
    node->next = NULL;
    node->order = order;
    node->backend = backend;

    backend_node_s *p0 = NULL;
    backend_node_s *p = head;
    while (p && (order > p->order)) {
        p0 = p;
        p = p->next;
    }
    if (p0) {
        p0->next = node;
    } else {
        head = node;
    }
    node->next = p;

    currentBackend = head->backend;
}

void video_printBackends(FILE *out) {
    backend_node_s *p = head;
    int count = 0;
    while (p) {
        const char *name = p->backend->name();
        if (count++) {
            fprintf(out, "|");
        }
        fprintf(out, "%s", name);
        p = p->next;
    }
}

static const char *_null_backend_name(void);
void video_chooseBackend(const char *name) {
    if (!name) {
        name = _null_backend_name();
    }

    backend_node_s *p = head;
    while (p) {
        const char *bname = p->backend->name();
        if (strcasecmp(name, bname) == 0) {
            currentBackend = p->backend;
            LOG("Setting current video backend to %s", name);
            break;
        }
        p = p->next;
    }
}

video_animation_s *video_getAnimationDriver(void) {
    return currentBackend->anim;
}

video_backend_s *video_getCurrentBackend(void) {
    return currentBackend;
}

// ----------------------------------------------------------------------------
// NULL video backend ...

static const char *_null_backend_name(void) {
    return "none";
}

static void _null_backend_init(void *context) {
}

static void _null_backend_main_loop(void) {
    while (null_backend_running) {
        sleep(1);
    }
}

static void _null_backend_render(void) {
}

static void _null_backend_shutdown(void) {
    null_backend_running = false;
}

static __attribute__((constructor)) void _init_video(void) {
    static video_backend_s null_backend = { 0 };
    null_backend.name      = &_null_backend_name;
    null_backend.init      = &_null_backend_init;
    null_backend.main_loop = &_null_backend_main_loop;
    null_backend.render    = &_null_backend_render;
    null_backend.shutdown  = &_null_backend_shutdown;
    static video_animation_s _null_animations = { 0 };
    null_backend.anim = &_null_animations;
    video_registerBackend(&null_backend, VID_PRIO_NULL);
}

