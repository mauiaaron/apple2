/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#include "testcommon.h"

#define TESTBUF_SZ 1024

bool test_do_reboot = true;
char mdstr[(SHA_DIGEST_LENGTH*2)+1];

static char input_str[TESTBUF_SZ]; // ASCII
static unsigned int input_length = 0;
static unsigned int input_counter = 0;

static struct timespec t0 = { 0 };
static struct timespec ti = { 0 };

// ----------------------------------------------------------------------------
// Stub functions because I've reached diminishing returns with the build system ...
//
// NOTE: You'd think the commandline CFLAGS set specifically for this test program would pass down to the sources in
// subdirectories, but it apparently isn't.  GNU buildsystem bug?  Also see HACK FIXME TODO NOTE in Makefile.am
//

uint8_t c_MB_Read(uint16_t addr) {
    return 0x0;
}

void c_MB_Write(uint16_t addr, uint8_t byte) {
}

uint8_t c_PhasorIO(uint16_t addr) {
    return 0x0;
}

void c_speaker_toggle(void) {
}

void c_interface_print(int x, int y, const int cs, const char *s) {
}

// ----------------------------------------------------------------------------

void test_common_setup() {
    input_counter = 0;
    input_length = 0;
    input_str[0] = '\0';
    clock_gettime(CLOCK_MONOTONIC, &t0);
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
    fprintf(GREATEST_STDOUT, "set breakpoint on test_breakpoint to check for problems...\n");
#if !HEADLESS
    if (!is_headless) {
        fprintf(GREATEST_STDOUT, "DISPLAY NOTE: busy-spinning, needs gdb/lldb intervention to continue...\n");
        volatile bool debug_continue = false;
        while (!debug_continue) {
            struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
            nanosleep(&ts, NULL);
        }
    }
#endif
}

// ----------------------------------------------------------------------------

void test_common_init(bool do_cputhread) {
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    do_logging = false;// silence regular emulator logging
    setenv("APPLE2IXCFG", "nosuchconfigfile", 1);

    c_initialize_firsttime();

    // kludgey set max CPU speed... 
    cpu_scale_factor = CPU_SCALE_FASTEST;
    cpu_altscale_factor = CPU_SCALE_FASTEST;
    is_fullspeed = true;

    caps_lock = true;

    if (do_cputhread) {
        // spin off cpu thread
        pthread_create(&cpu_thread_id, NULL, (void *) &cpu_thread, (void *)NULL);
        c_debugger_set_watchpoint(WATCHPOINT_ADDR);
        if (is_headless) {
            c_debugger_set_timeout(15);
        } else {
            fprintf(stderr, "NOTE : RUNNING WITH DISPLAY\n");
            fprintf(stderr, "Will spinloop on failed tests for debugger intervention\n");
            fprintf(stderr, "Pass HEADLESS=1 to environment to run nonstop\n");
            c_debugger_set_timeout(0);
        }
    } else {
#ifdef AUDIO_ENABLED
        DSInit();
        speaker_init();
        MB_Initialize();
#endif
        reinitialize();
    }
}

int test_setup_boot_disk(const char *fileName, int readonly) {
    char *disk = NULL;
    int err = 0;
#ifdef __APPLE__
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFStringRef fileString = CFStringCreateWithCString(/*allocator*/NULL, fileName, CFStringGetSystemEncoding());
    CFURLRef fileURL = CFBundleCopyResourceURL(mainBundle, fileString, NULL, NULL);
    CFRELEASE(fileString);
    CFStringRef filePath = CFURLCopyFileSystemPath(fileURL, kCFURLPOSIXPathStyle);
    CFRELEASE(fileURL);
    CFIndex length = CFStringGetLength(filePath);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    disk = (char *)malloc(maxSize);
    if (!CFStringGetCString(filePath, disk, maxSize, kCFStringEncodingUTF8)) {
        FREE(disk);
    }
    CFRELEASE(filePath);
#else
    asprintf(&disk, "%s/disks/%s", data_dir, fileName);
#endif
    if (c_new_diskette_6(0, disk, readonly)) {
        int len = strlen(disk);
        disk[len-3] = '\0';
        err = (c_new_diskette_6(0, disk, readonly) != NULL);
    }
    FREE(disk);
    return err;
}

void sha1_to_str(const uint8_t * const md, char *buf) {
    int i=0;
    for (int j=0; j<SHA_DIGEST_LENGTH; j++, i+=2) {
        sprintf(buf+i, "%02X", md[j]);
    }
    sprintf(buf+i, "%c", '\0');
}

