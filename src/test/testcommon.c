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

#include "testcommon.h"

bool test_do_reboot = true;
char mdstr[(SHA_DIGEST_LENGTH*2)+1];

// ----------------------------------------------------------------------------

void test_common_setup(void) {
}

void test_type_input(const char *input) {
    debugger_setInputText(input);
}

void test_breakpoint(void *arg) {
    fprintf(GREATEST_STDOUT, "DISPLAY NOTE: busy-spinning in test_breakpoint(), needs gdb/lldb intervention to continue...\n");
    volatile bool debug_continue = false;
    while (!debug_continue) {
        struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
}

void test_common_init(void) {
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    do_logging = false;// silence regular emulator logging

    extern void emulator_ctors(void);
    emulator_ctors();

    char *envvar = NULL;
    ASPRINTF(&envvar, "APPLE2IX_JSON=%s/.apple2.test.json", getenv("HOME"));
    assert(envvar);
    putenv(envvar);
    LEAK(envvar);

    prefs_load();
    prefs_setLongValue(PREF_DOMAIN_VIDEO, PREF_COLOR_MODE, COLOR);
    prefs_setBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, true);
    prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, CPU_SCALE_FASTEST);
    prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, CPU_SCALE_FASTEST);
    prefs_save();

    c_debugger_set_watchpoint(WATCHPOINT_ADDR);
    if (0) {
        c_debugger_set_timeout(15);
    } else {
        fprintf(stderr, "NOTE : RUNNING WITH DISPLAY\n");
        fprintf(stderr, "Will spinloop on failed tests for debugger intervention\n");
        c_debugger_set_timeout(0);
    }
}

int test_setup_boot_disk(const char *fileName, int readonly) {
    int err = 0;
    char **path = NULL;

#if defined(__APPLE__)
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFStringRef fileString = CFStringCreateWithCString(/*allocator*/NULL, fileName, CFStringGetSystemEncoding());
    CFURLRef fileURL = CFBundleCopyResourceURL(mainBundle, fileString, NULL, NULL);
    
    if (!fileURL) {
        CFRELEASE(fileString);
        char *fileName2 = NULL;
        ASPRINTF(&fileName2, "disks/demo/%s", fileName);
        fileString = CFStringCreateWithCString(/*allocator*/NULL, fileName2, CFStringGetSystemEncoding());
        FREE(fileName2);
        fileURL = CFBundleCopyResourceURL(mainBundle, fileString, NULL, NULL);
        if (!fileURL) {
            CFRELEASE(fileString);
            ASPRINTF(&fileName2, "disks/blanks/%s", fileName);
            fileString = CFStringCreateWithCString(/*allocator*/NULL, fileName2, CFStringGetSystemEncoding());
            FREE(fileName2);
            fileURL = CFBundleCopyResourceURL(mainBundle, fileString, NULL, NULL);
        }
        assert(fileURL);
    }
    
    CFStringRef filePath = CFURLCopyFileSystemPath(fileURL, kCFURLPOSIXPathStyle);
    CFRELEASE(fileString);
    CFRELEASE(fileURL);
    CFIndex length = CFStringGetLength(filePath);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    char *disk0 = (char *)MALLOC(maxSize);
    if (!CFStringGetCString(filePath, disk0, maxSize, kCFStringEncodingUTF8)) {
        FREE(disk0);
    }
    CFRELEASE(filePath);

    char *paths[] = {
        NULL,
        NULL,
    };
    ASPRINTF(&paths[0], "%s", disk0);
    FREE(disk0);
#else
    char *paths[] = {
        NULL,
        NULL,
        NULL,
        NULL,
    };
    ASPRINTF(&paths[0], "%s/disks/%s", data_dir, fileName);
    ASPRINTF(&paths[1], "%s/disks/demo/%s", data_dir, fileName);
    ASPRINTF(&paths[2], "%s/disks/blanks/%s", data_dir, fileName);
#endif

    path = &paths[0];
    while (*path) {
        char *disk = *path;
        ++path;

        err = disk6_insert(0, disk, readonly) != NULL;
        if (!err) {
            break;
        }

        size_t len = strlen(disk);
        disk[len-3] = '\0'; // try again without '.gz' extension
        err = disk6_insert(0, disk, readonly) != NULL;
        if (!err) {
            break;
        }
    }

    path = &paths[0];
    while (*path) {
        char *disk = *path;
        ++path;
        FREE(disk);
    }

    return err;
}

void sha1_to_str(const uint8_t * const md, char *buf) {
    int i=0;
    for (int j=0; j<SHA_DIGEST_LENGTH; j++, i+=2) {
        sprintf(buf+i, "%02X", md[j]);
    }
}

