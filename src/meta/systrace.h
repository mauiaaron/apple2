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
// TODO FIXME : migrate toolchain to https://github.com/catapult-project/catapult.git

#ifndef _META_SYSTRACE_H_
#define _META_SYSTRACE_H_

// Do not enable for official builds!
#define ENABLE_SYSTRACING 0
#define SYSTRACE_CPU 0
#define SYSTRACE_DISK 0
#define SYSTRACE_AUDIO 0
#define SYSTRACE_VIDEO 0
#define SYSTRACE_INTERFACE 0

// WARNING: using a custom tracefile may significantly eat up disk space
#define USE_CUSTOM_TRACE_FILE 1 // If set, will use custom file in $HOME rather than Linux kernel facility

extern void _trace_cleanup(void *token);
extern void _trace_begin(const char *fmt, ...);
extern void _trace_begin_count(uint32_t count, const char *fmt, ...);
extern void _trace_end(void);

#if ENABLE_SYSTRACING

#define _SCOPE_TRACE(ctr, fmt, ...) \
    void *__scope_token##ctr##__ __attribute__((cleanup(_trace_cleanup), unused)) = ({ _trace_begin(fmt, ##__VA_ARGS__); (void *)NULL; })

#   if SYSTRACE_CPU
#       define SCOPE_TRACE_CPU(fmt, ...) _SCOPE_TRACE(__COUNTER__, fmt, ##__VA_ARGS__)
#       define TRACE_CPU_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_CPU_END() _trace_end()
#       define TRACE_CPU_MARK(fmt, ...) do { _trace_begin(fmt, ##__VA_ARGS__); _trace_end(); } while (0)
#   endif

#   if SYSTRACE_DISK
#       define SCOPE_TRACE_DISK(fmt, ...) _SCOPE_TRACE(__COUNTER__, fmt, ##__VA_ARGS__)
#       define TRACE_DISK_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_DISK_END() _trace_end()
#       define TRACE_DISK_MARK(fmt, ...) do { _trace_begin(fmt, ##__VA_ARGS__); _trace_end(); } while (0)
#   endif

#   if SYSTRACE_AUDIO
#       define SCOPE_TRACE_AUDIO(fmt, ...) _SCOPE_TRACE(__COUNTER__, fmt, ##__VA_ARGS__)
#       define TRACE_AUDIO_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_AUDIO_END() _trace_end()
#       define TRACE_AUDIO_MARK(fmt, ...) do { _trace_begin(fmt, ##__VA_ARGS__); _trace_end(); } while (0)
#   endif

#   if SYSTRACE_VIDEO
#       define SCOPE_TRACE_VIDEO(fmt, ...) _SCOPE_TRACE(__COUNTER__, fmt, ##__VA_ARGS__)
#       define TRACE_VIDEO_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_VIDEO_END() _trace_end()
#       define TRACE_VIDEO_MARK(fmt, ...) do { _trace_begin(fmt, ##__VA_ARGS__); _trace_end(); } while (0)
#   endif

#   if SYSTRACE_INTERFACE
#       define SCOPE_TRACE_INTERFACE(fmt, ...) _SCOPE_TRACE(__COUNTER__, fmt, ##__VA_ARGS__)
#       define TRACE_INTERFACE_BEGIN(fmt, ...) _trace_begin(fmt, ##__VA_ARGS__)
#       define TRACE_INTERFACE_END() _trace_end()
#       define TRACE_INTERFACE_MARK(fmt, ...) do { _trace_begin(fmt, ##__VA_ARGS__); _trace_end(); } while (0)
#   endif

#endif

#if !SYSTRACE_CPU
#   define SCOPE_TRACE_CPU(fmt, ...)
#   define TRACE_CPU_BEGIN(fmt, ...)
#   define TRACE_CPU_END()
#   define TRACE_CPU_MARK(fmt, ...)
#endif

#if !SYSTRACE_DISK
#   define SCOPE_TRACE_DISK(fmt, ...)
#   define TRACE_DISK_BEGIN(fmt, ...)
#   define TRACE_DISK_END()
#   define TRACE_DISK_MARK(fmt, ...)
#endif

#if !SYSTRACE_AUDIO
#   define SCOPE_TRACE_AUDIO(fmt, ...)
#   define TRACE_AUDIO_BEGIN(fmt, ...)
#   define TRACE_AUDIO_END()
#   define TRACE_AUDIO_MARK(fmt, ...)
#endif

#if !SYSTRACE_VIDEO
#   define SCOPE_TRACE_VIDEO(fmt, ...)
#   define TRACE_VIDEO_BEGIN(fmt, ...)
#   define TRACE_VIDEO_END()
#   define TRACE_VIDEO_MARK(fmt, ...)
#endif

#if !SYSTRACE_INTERFACE
#   define SCOPE_TRACE_INTERFACE(fmt, ...)
#   define TRACE_INTERFACE_BEGIN(fmt, ...)
#   define TRACE_INTERFACE_END()
#   define TRACE_INTERFACE_MARK(fmt, ...)
#endif

#endif // whole file
