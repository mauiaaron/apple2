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

bool do_logging = true; // also controlled by NDEBUG
FILE *error_log = NULL;
int sound_volume = 2;
color_mode_t color_mode = COLOR;
const char *data_dir = NULL;
char **argv = NULL;
int argc = 0;
CrashHandler_s *crashHandler = NULL;

__attribute__((constructor(CTOR_PRIORITY_FIRST)))
static void _init_common(void) {
#if defined(CONFIG_DATADIR)
    data_dir = strdup(CONFIG_DATADIR PATH_SEPARATOR PACKAGE_NAME);
#elif defined(ANDROID)
    // data_dir is set up in JNI
#elif !defined(__APPLE__)
#   error "Specify a CONFIG_DATADIR and PACKAGE_NAME"
#endif
}

static void _shutdown_threads(void) {
#if !TESTING
#   if defined(__linux__) && !defined(ANDROID)
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
#   elif !defined(__APPLE__)
#       error TODO FIXME ... verify leaks-n-things with Valgrind on unknown platform
#   endif
#endif
}

void emulator_start(void) {
#ifdef INTERFACE_CLASSIC
    load_settings(); // user prefs
    c_keys_set_key(kF8); // show credits before emulation start
#endif
    video_init();
    timing_startCPU();
    video_main_loop();
}

void emulator_shutdown(void) {
    video_shutdown();
    timing_stopCPU();
    _shutdown_threads();
}

#if !TESTING && !defined(__APPLE__) && !defined(ANDROID)
int main(int _argc, char **_argv) {
    argc = _argc;
    argv = _argv;

    emulator_start();

    // main loop ...

    emulator_shutdown();

    LOG("Emulator exit ...");

    return 0;
}
#endif

