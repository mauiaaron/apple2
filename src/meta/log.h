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

#if __cplusplus
extern "C" {
#endif

#if VIDEO_OPENGL
extern GLenum safeGLGetError(void);
#else
#   define GLenum int
#   define safeGLGetError() 0
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

#define _SIMPLE_LOG(...) \
    do { \
        char *buf = NULL; \
        int ignored = asprintf(&buf, __VA_ARGS__); \
        (void)ignored; \
        \
        log_outputString(buf); \
        \
        free(buf); \
    } while (0)

#define _LOG(...) \
    do { \
        char *glerr_str = NULL; \
        int ignored; \
        if (_glerr) { \
            ignored = asprintf(&glerr_str, " (glerr:%04X)", _glerr); \
        } \
        \
        char *buf0 = NULL; \
        ignored = asprintf(&buf0, __VA_ARGS__); \
        \
        char *buf = NULL; \
        ignored = asprintf(&buf, "%s:%d (%s) -%s %s", _MYFILE_, __LINE__, __func__, (glerr_str ? : ""), buf0); \
        (void)ignored; \
        \
        log_outputString(buf); \
        \
        free(buf0); \
        free(buf); \
        if (glerr_str) { \
            free(glerr_str); \
        } \
    } while (0)

#if defined(__ANDROID__)
// Apparently some non-conformant Android devices (ahem, Spamsung, ahem) do not actually let me see what the assert
// actually was before aborting/segfaulting ...
#   undef assert
#   define assert(e) \
    do { \
        if (LIKELY((e))) { \
            /* ... ALL GOOD ... */ \
        } else { \
            /*LOG( "!!! ASSERT !!! : " #e); */ \
            /*sleep(1); */ \
            __assert2(_MYFILE_, __LINE__, __func__, #e); \
        } \
    } while (0)
#endif

#if defined(NDEBUG)
#   if defined(__ANDROID__)
// HACK NOTE : keep assertions in Android release code to get better introspection into potential crashes
#   else
#       undef assert
#       define assert(e)
#   endif
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
#   define GL_MAYBELOG(...) \
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

#if __cplusplus
}
#endif

#endif // whole file
