/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#include "common.h"

#define SAVE_MAGICK  "A2VM"
#define SAVE_MAGICK2 "A2V2"
#define SAVE_VERSION 2
#define SAVE_MAGICK_LEN sizeof(SAVE_MAGICK)

typedef struct module_ctor_node_s {
    struct module_ctor_node_s *next;
    long order;
    startup_callback_f ctor;
} module_ctor_node_s;

static module_ctor_node_s *head = NULL;
static bool emulatorShuttingDown = false;

bool do_logging = true; // also controlled by NDEBUG
FILE *error_log = NULL;
const char *data_dir = NULL;
char **argv = NULL;
int argc = 0;
CrashHandler_s *crashHandler = NULL;

#if defined(CONFIG_DATADIR)
static void _init_common(void) {
    LOG("Initializing common...");
    data_dir = STRDUP(CONFIG_DATADIR PATH_SEPARATOR PACKAGE_NAME);
}

static __attribute__((constructor)) void __init_common(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_FIRST, &_init_common);
}
#elif defined(ANDROID) || defined(__APPLE__)
    // data_dir is set up elsewhere
#else
#   error "Specify a CONFIG_DATADIR and PACKAGE_NAME"
#endif

static bool _save_state(int fd, const uint8_t * outbuf, ssize_t outmax) {
    ssize_t outlen = 0;
    do {
        if (TEMP_FAILURE_RETRY(outlen = write(fd, outbuf, outmax)) == -1) {
            ERRLOG("error writing emulator save state file");
            break;
        }
        outbuf += outlen;
        outmax -= outlen;
    } while (outmax > 0);

    return outmax == 0;
}

static bool _load_state(int fd, uint8_t * inbuf, ssize_t inmax) {
    ssize_t inlen = 0;
    do {
        if (TEMP_FAILURE_RETRY(inlen = read(fd, inbuf, inmax)) == -1) {
            ERRLOG("error reading emulator save state file");
            break;
        }
        if (inlen == 0) {
            ERRLOG("error reading emulator save state file (truncated)");
            break;
        }
        inbuf += inlen;
        inmax -= inlen;
    } while (inmax > 0);

    return inmax == 0;
}

bool emulator_saveState(const char * const path) {
    int fd = -1;
    bool saved = false;

#if !TESTING
    assert(cpu_isPaused() && "should be paused to save state");
#endif

    do {
        TEMP_FAILURE_RETRY(fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR));
        if (fd < 0) {
            break;
        }
        assert(fd != 0 && "crazy platform");

        // save header
        if (!_save_state(fd, (const uint8_t *)SAVE_MAGICK2, SAVE_MAGICK_LEN)) {
            break;
        }

        StateHelper_s helper = {
            .fd = fd,
            .version = SAVE_VERSION,
            .save = &_save_state,
            .load = &_load_state,
        };

        if (!disk6_saveState(&helper)) {
            break;
        }

        if (!vm_saveState(&helper)) {
            break;
        }

        if (!cpu65_saveState(&helper)) {
            break;
        }

        if (!video_saveState(&helper)) {
            break;
        }

        if (!mb_saveState(&helper)) {
            break;
        }

        TEMP_FAILURE_RETRY(fsync(fd));
        saved = true;
    } while (0);

    if (fd >= 0) {
        TEMP_FAILURE_RETRY(close(fd));
    }

    if (!saved) {
        ERRLOG("could not write to the emulator save state file");
        unlink(path);
    }

    return saved;
}

bool emulator_loadState(const char * const path) {
    int fd = -1;
    bool loaded = false;

#if !TESTING
    assert(cpu_isPaused() && "should be paused to load state");
#endif

    video_setDirty(A2_DIRTY_FLAG);

    int version=-1;
    do {
        TEMP_FAILURE_RETRY(fd = open(path, O_RDONLY));
        if (fd < 0) {
            break;
        }
        assert(fd != 0 && "crazy platform");

        // load header
        uint8_t magick[SAVE_MAGICK_LEN] = { 0 };
        if (!_load_state(fd, magick, SAVE_MAGICK_LEN)) {
            break;
        }

        // check header

        if (memcmp(magick, SAVE_MAGICK, SAVE_MAGICK_LEN) == 0) {
            version = 1;
        } else if (memcmp(magick, SAVE_MAGICK2, SAVE_MAGICK_LEN) == 0) {
            version = 2;
        } else {
            ERRLOG("bad header magick in emulator save state file");
            break;
        }

        StateHelper_s helper = {
            .fd = fd,
            .version = version,
            .save = &_save_state,
            .load = &_load_state,
        };

        if (!disk6_loadState(&helper)) {
            break;
        }

        if (!vm_loadState(&helper)) {
            break;
        }

        if (!cpu65_loadState(&helper)) {
            break;
        }

        if (!video_loadState(&helper)) {
            break;
        }

        if (version >= 2) {
            if (!mb_loadState(&helper)) {
                break;
            }
        }

        loaded = true;
    } while (0);

    if (fd >= 0) {
        TEMP_FAILURE_RETRY(close(fd));
    }

    if (!loaded) {
        ERRLOG("could not load emulator save state file");
    }

    return loaded;
}

static void _shutdown_threads(void) {
#if defined(__linux__) && !defined(ANDROID)
    LOG("Emulator waiting for other threads to clean up...");
    do {
        DIR *dir = opendir("/proc/self/task");
        if (!dir) {
            ERRLOG("Cannot open /proc/self/task !");
            break;
        }

        int thread_count = 0;
        struct dirent *d = NULL;
        while ((d = readdir(dir)) != NULL) {
            if (strncmp(".", d->d_name, 2) == 0) {
                // ignore
            } else if (strncmp("..", d->d_name, 3) == 0) {
                // ignore
            } else {
                ++thread_count;
            }
        }

        closedir(dir);

        assert(thread_count >= 1 && "there must at least be one thread =P");
        if (thread_count == 1) {
            break;
        }

        static struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL); // 30Hz framerate
    } while (1);
#endif
}

void emulator_registerStartupCallback(long order, startup_callback_f ctor) {

    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);

    module_ctor_node_s *node = MALLOC(sizeof(module_ctor_node_s));
    assert(node);
    node->next = NULL;
    node->order = order;
    node->ctor = ctor;

    module_ctor_node_s *p0 = NULL;
    module_ctor_node_s *p = head;
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

    pthread_mutex_unlock(&mutex);
}

void emulator_ctors(void) {
    module_ctor_node_s *p = head;
    head = NULL;
    while (p) {
        p->ctor();
        module_ctor_node_s *next = p->next;
        FREE(p);
        p = next;
    }
    head = NULL;
}

void emulator_start(void) {

    emulator_ctors();

    prefs_load(); // user prefs
    prefs_sync(NULL);

#if defined(INTERFACE_CLASSIC) && !TESTING
    c_keys_set_key(kF8); // show credits before emulation start
#endif

#if !defined(__APPLE__) && !defined(ANDROID)
    video_init();
#endif

    timing_startCPU();
}

void emulator_shutdown(void) {
    emulatorShuttingDown = true;
    video_shutdown();
    prefs_shutdown();
    timing_stopCPU();
    _shutdown_threads();
}

bool emulator_isShuttingDown(void) {
    return emulatorShuttingDown;
}

#if !defined(__APPLE__) && !defined(ANDROID)
int main(int _argc, char **_argv) {
    argc = _argc;
    argv = _argv;

#if TESTING
#   if TEST_CPU
    // Currently this test is the only one that blocks current thread and runs as a black screen
    extern int test_cpu(int, char *[]);
    test_cpu(argc, argv);
#   elif TEST_DISK
    extern int test_disk(int, char *[]);
    test_disk(argc, argv);
#   elif TEST_DISPLAY
    extern int test_display(int, char *[]);
    test_display(argc, argv);
#   elif TEST_PREFS
    extern void test_prefs(int, char *[]);
    test_prefs(argc, argv);
#   elif TEST_TRACE
    extern void test_trace(int, char *[]);
    test_trace(argc, argv);
#   elif TEST_UI
    extern int test_ui(int, char *[]);
    test_ui(argc, argv);
#   elif TEST_VM
    extern int test_vm(int, char *[]);
    test_vm(argc, argv);
#   else
#       error "OOPS, no testsuite specified"
#   endif
#endif

    cpu_pause();

    emulator_start();

    cpu_resume();

    video_main_loop();

    emulator_shutdown();

    LOG("Emulator exit ...");

    return 0;
}
#endif

