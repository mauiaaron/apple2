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

#define SAVE_MAGICK "A2VM"
#define SAVE_MAGICK_LEN sizeof(SAVE_MAGICK)

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
    LOG("Initializing common...");
#if defined(CONFIG_DATADIR)
    data_dir = strdup(CONFIG_DATADIR PATH_SEPARATOR PACKAGE_NAME);
#elif defined(ANDROID)
    // data_dir is set up in JNI
#elif !defined(__APPLE__)
#   error "Specify a CONFIG_DATADIR and PACKAGE_NAME"
#endif
}

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

    assert(cpu_isPaused() && "should be paused to save state");

    do {
        TEMP_FAILURE_RETRY(fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR));
        if (fd < 0) {
            break;
        }
        assert(fd != 0 && "crazy platform");

        // save header
        if (!_save_state(fd, SAVE_MAGICK, SAVE_MAGICK_LEN)) {
            break;
        }

        StateHelper_s helper = {
            .fd = fd,
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

    assert(cpu_isPaused() && "should be paused to load state");

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
        if (memcmp(magick, SAVE_MAGICK, SAVE_MAGICK_LEN) != 0) {
            ERRLOG("bad header magick in emulator save state file");
            break;
        }

        StateHelper_s helper = {
            .fd = fd,
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
    disk6_eject(0);
    disk6_eject(1);
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

