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

// Function tracing and profiling

#ifndef _META_TRACE_H_
#define _META_TRACE_H_

#define TRACE_CPU 0
#define TRACE_DISK 0
#define TRACE_AUDIO 0
#define TRACE_VIDEO 0
#define TRACE_TOUCH 0

#if !defined(__linux__)
#   warning Linux-specific function call tracing and general profiling not enabled
#else

extern void _trace_cleanup(void *token);
extern void _trace_begin(const char *fmt, ...);
extern void _trace_begin_count(uint32_t count, const char *fmt, ...);
extern void _trace_end(void);

#define _SCOPE_TRACE(ctr, fmt, ...) \
    void *__scope_token##ctr##__ __attribute__((cleanup(_trace_cleanup), unused)) = ({ _trace_begin(fmt, ##__VA_ARGS__); (void *)NULL; })

#if !defined(NDEBUG)
#   define SCOPE_TRACE(fmt, ...) _SCOPE_TRACE(_COUNTER_, fmt, ##__VA_ARGS__)

#   if TRACE_CPU
#       define SCOPE_TRACE_CPU(fmt, ...) SCOPE_TRACE(fmt, ##__VA_ARGS__)
#       define TRACE_CPU_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_CPU_END() _trace_end()
#   endif

#   if TRACE_DISK
#       define SCOPE_TRACE_DISK(fmt, ...) SCOPE_TRACE(fmt, ##__VA_ARGS__)
#       define TRACE_DISK_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_DISK_END() _trace_end()
#   endif

#   if TRACE_AUDIO
#       define SCOPE_TRACE_AUDIO(fmt, ...) SCOPE_TRACE(fmt, ##__VA_ARGS__)
#       define TRACE_AUDIO_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_AUDIO_END() _trace_end()
#   endif

#   if TRACE_VIDEO
#       define SCOPE_TRACE_VIDEO(fmt, ...) SCOPE_TRACE(fmt, ##__VA_ARGS__)
#       define TRACE_VIDEO_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_VIDEO_END() _trace_end()
#   endif

#   if TRACE_TOUCH
#       define SCOPE_TRACE_TOUCH(fmt, ...) SCOPE_TRACE(fmt, ##__VA_ARGS__)
#       define TRACE_TOUCH_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_TOUCH_END() _trace_end()
#   endif

#endif

#endif // __linux__

#if !defined(SCOPE_TRACE)
#   define SCOPE_TRACE(fmt, ...)
#endif

#if !TRACE_CPU
#   define SCOPE_TRACE_CPU(fmt, ...)
#   define TRACE_CPU_BEGIN(fmt, ...)
#   define TRACE_CPU_END()
#endif

#if !TRACE_DISK
#   define SCOPE_TRACE_DISK(fmt, ...)
#   define TRACE_DISK_BEGIN(fmt, ...)
#   define TRACE_DISK_END()
#endif

#if !TRACE_AUDIO
#   define SCOPE_TRACE_AUDIO(fmt, ...)
#   define TRACE_AUDIO_BEGIN(fmt, ...)
#   define TRACE_AUDIO_END()
#endif

#if !TRACE_VIDEO
#   define SCOPE_TRACE_VIDEO(fmt, ...)
#   define TRACE_VIDEO_BEGIN(fmt, ...)
#   define TRACE_VIDEO_END()
#endif

#if !TRACE_TOUCH
#   define SCOPE_TRACE_TOUCH(fmt, ...)
#   define TRACE_TOUCH_BEGIN(fmt, ...)
#   define TRACE_TOUCH_END()
#endif

#else

#endif // whole file

