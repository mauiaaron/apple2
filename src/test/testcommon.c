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

#define TESTBUF_SZ 1024

bool test_do_reboot = true;
char mdstr[(SHA_DIGEST_LENGTH*2)+1];

static char input_str[TESTBUF_SZ]; // ASCII
static unsigned int input_length = 0;
static unsigned int input_counter = 0;

// ----------------------------------------------------------------------------

void test_common_setup() {
    input_counter = 0;
    input_length = 0;
    input_str[0] = '\0';
}

// ----------------------------------------------------------------------------
// test video functions and stubs

void testing_video_sync() {

    if (!input_length) {
        input_length = strlen(input_str);
    }

    if (input_counter >= input_length) {
        return;
    }

    uint8_t ch = (uint8_t)input_str[input_counter];
    if (ch == '\n') {
        ch = '\r';
    }

    if ( (apple_ii_64k[0][0xC000] & 0x80) || (apple_ii_64k[1][0xC000] & 0x80) ) {
        // last character typed not processed by emulator...
        return;
    }

    apple_ii_64k[0][0xC000] = ch | 0x80;
    apple_ii_64k[1][0xC000] = ch | 0x80;

    ++input_counter;
}

void test_type_input(const char *input) {
    strcat(input_str, input);
}

// ----------------------------------------------------------------------------

void test_breakpoint(void *arg) {
    fprintf(GREATEST_STDOUT, "DISPLAY NOTE: busy-spinning in test_breakpoint(), needs gdb/lldb intervention to continue...\n");
    volatile bool debug_continue = false;
    while (!debug_continue) {
        struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
}

// ----------------------------------------------------------------------------

void test_common_init() {
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    //do_logging = false;// silence regular emulator logging

    prefs_load();
    prefs_setBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, true);
    prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, CPU_SCALE_FASTEST);
    prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, CPU_SCALE_FASTEST);
    prefs_sync(NULL);

    timing_initialize();

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

