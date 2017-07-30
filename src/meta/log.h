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

#ifndef _LOG_H_
#define _LOG_H_

#if VIDEO_OPENGL
#   include "video_util/glUtil.h"

// 2015/04/01 ... early calls to glGetError()--before a context exists--causes segfaults on MacOS X
extern bool safe_to_do_opengl_logging;
static inline GLenum safeGLGetError(void) {
    if (safe_to_do_opengl_logging && video_isRenderThread()) {
        return glGetError();
    }
    return (GLenum)0;
}

#else
#   define GLenum int
#   define safeGLGetError() 0
#   define glGetError() 0
#endif

// global logging kill switch
extern bool do_logging;

// log to the standard log facility (e.g., stderr)
extern bool do_std_logging;

// initialize logging facility
void log_init(void);

// print a string to the log file.  Terminating '\n' is added.
void log_outputString(const char * const str);

#define _MYFILE_ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define _LOG(...) \
    do { \
        int _err = errno; \
        errno = 0; \
        \
        char *syserr_str = NULL; \
        char *glerr_str = NULL; \
        if (_err) { \
            asprintf(&syserr_str, " (syserr:%s)", strerror(_err)); \
        } \
        if (_glerr) { \
            asprintf(&glerr_str, " (glerr:%04X)", _glerr); \
        } \
        \
        char *buf0 = NULL; \
        asprintf(&buf0, __VA_ARGS__); \
        \
        char *buf = NULL; \
        asprintf(&buf, "%s:%d (%s) -%s%s %s", _MYFILE_, __LINE__, __func__, (syserr_str ? : ""), (glerr_str ? : ""), buf0); \
        \
        log_outputString(buf); \
        \
        free(buf0); \
        free(buf); \
        if (syserr_str) { \
            free(syserr_str); \
        } \
        if (glerr_str) { \
            free(glerr_str); \
        } \
    } while (0)


#ifdef ANDROID
// Apparently some non-conformant Android devices (ahem, Spamsung, ahem) do not actually let me see what the assert
// actually was before aborting/segfaulting ...
#   undef assert
#   define assert(e) \
    do { \
        if ((e)) { \
            /* ... */ \
        } else { \
            LOG( "!!! ASSERT !!! : " #e ); \
            sleep(1); \
            __assert2(_MYFILE_, __LINE__, __func__, #e); \
        } \
    } while (0)
#endif

#define LOG(...) \
    if (LIKELY(do_logging)) { \
        GLenum _glerr = safeGLGetError(); \
        _LOG(__VA_ARGS__); \
        while ( (_glerr = safeGLGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
    } //

// GL_MAYBELOG() only logs if an OpenGL error occurred
#define GL_MAYBELOG(...) \
    if (LIKELY(do_logging)) { \
        GLenum _glerr = 0; \
        while ( (_glerr = safeGLGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
    } //

#define QUIT_FUNCTION(x) exit(x)

#define ERRQUIT(...) \
    do { \
        GLenum _glerr = safeGLGetError(); \
        _LOG(__VA_ARGS__); \
        while ( (_glerr = safeGLGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
        QUIT_FUNCTION(1); \
    } while (0)

// GL_ERRQUIT() only logs/quits if an OpenGL error occurred
#define GL_ERRQUIT(...) \
    do { \
        GLenum _glerr = 0; \
        GLenum _last_glerr = 0; \
        while ( (_glerr = safeGLGetError()) ) { \
            _last_glerr = _glerr; \
            _LOG(__VA_ARGS__); \
        } \
        if (_last_glerr) { \
            QUIT_FUNCTION(_last_glerr); \
        } \
    } while (0)


#endif // whole file
