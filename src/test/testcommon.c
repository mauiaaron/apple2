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

void (*testing_cyclesOverflow)(void) = NULL;
unsigned long (*testing_getCyclesCount)(void) = NULL;

// ----------------------------------------------------------------------------

void test_common_setup(void) {
}

void test_type_input(const char *input) {
    debugger_setInputText(input, false);
}

void test_type_input_deterministically(const char *input) {
    debugger_setInputText(input, true);
}

void test_breakpoint(void *arg) {
    fprintf(GREATEST_STDOUT, "OOPS set a breakpoint in test_breakpoint() to diagnose test failure...\n");
}

void test_common_init(void) {
#if __ANDROID__
    // tags help us wade through log soup
#else
    do_std_logging = false;// silence regular emulator logging
#endif

    extern void emulator_ctors(void);
    emulator_ctors();

    char *envvar = NULL;
    ASPRINTF(&envvar, "APPLE2IX_JSON=%s/.apple2.test.json", HOMEDIR);
    assert(envvar);
    putenv(envvar);
    LEAK(envvar);

    prefs_load();
    prefs_setLongValue(PREF_DOMAIN_VIDEO, PREF_COLOR_MODE, COLOR_MODE_MONO);
    prefs_setLongValue(PREF_DOMAIN_VIDEO, PREF_MONO_MODE, MONO_MODE_BW);
    prefs_setBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, true);
    prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, (CPU_SCALE_FASTEST * 100.));
    prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, (CPU_SCALE_FASTEST * 100.));
    prefs_save();

    c_debugger_set_watchpoint(WATCHPOINT_ADDR);

    fprintf(stderr, "Break in test_breakpoint() to catch and diagnose test failures...\n");
    c_debugger_set_timeout(0);
}

#if (TARGET_OS_MAC || TARGET_OS_PHONE)
#   define PATHS_COUNT 8
char **_copy_paths_mac(const char *fileName) {
    const char *fmts[PATHS_COUNT + 1] = {
        "%s%sdisks/%s",
        "%s%sdisks/demo/%s",
        "%s%sdisks/blanks/%s",
        "%s%sdisks/3rd-party-test/%s",
        "%s%sexternal-disks/%s",
        "%s%sexternal-disks/demo/%s",
        "%s%sexternal-disks/blanks/%s",
        "%s%sexternal-disks/3rd-party-test/%s",
        NULL,
    };

    char **paths = CALLOC(1, sizeof(fmts));
    assert(paths);

#   if TARGET_OS_SIMULATOR || !TARGET_OS_EMBEDDED
    const char *prefixPath = "";
    const char *sep = "";
#   else // TARGET_OS_EMBEDDED
    const char *prefixPath = data_dir;
    const char *sep = "/";
#   endif

    do {
        const char **fmtPtr = &fmts[0];
        unsigned int idx = 0;
        while (*fmtPtr) {
            const char *fmt = *fmtPtr;
            
            char *diskFile = NULL;
            CFStringRef fileString = NULL;
            CFURLRef fileURL = NULL;
            
            int len = ASPRINTF(&diskFile, fmt, prefixPath, sep, fileName);
            assert(diskFile);
            fileString = CFStringCreateWithCString(kCFAllocatorDefault, diskFile, kCFStringEncodingUTF8);
            assert(fileString);
            
#   if TARGET_OS_SIMULATOR || !TARGET_OS_EMBEDDED
            // Use disks directly out of bundle ... is this OKAY?
            fileURL = CFBundleCopyResourceURL(CFBundleGetMainBundle(), fileString, NULL, NULL);
#   else
            // AppDelegate should have copied disks to a R/W location
            fileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, fileString, kCFURLPOSIXPathStyle, /*isDirectory*/false);
#   endif
            if (!fileURL) {
                if (CFStringHasSuffix(fileString, CFSTR(".gz")) || CFStringHasSuffix(fileString, CFSTR(".GZ"))) {
                    *(diskFile + len - 3) = '\0';
                    CFRELEASE(fileString);
                    fileString = CFStringCreateWithCString(kCFAllocatorDefault, diskFile, kCFStringEncodingUTF8);
                    assert(fileString);
#   if TARGET_OS_SIMULATOR || !TARGET_OS_EMBEDDED
                    // Use disks directly out of bundle ... is this OKAY?
                    fileURL = CFBundleCopyResourceURL(CFBundleGetMainBundle(), fileString, NULL, NULL);
#   else
                    // AppDelegate should have copied disks to a R/W location
                    fileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, fileString, kCFURLPOSIXPathStyle, /*isDirectory*/false);
#   endif
                }
            }
            
            if (fileURL) {
                CFStringRef filePath = CFURLCopyFileSystemPath(fileURL, kCFURLPOSIXPathStyle);
                assert(filePath);
                CFIndex length = CFStringGetLength(filePath);
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
                char *disk0 = (char *)MALLOC(maxSize);
                if (CFStringGetCString(filePath, disk0, maxSize, kCFStringEncodingUTF8)) {
                    paths[idx++] = disk0;
                }
                CFRELEASE(filePath);
                CFRELEASE(fileURL);
            }
            
            FREE(diskFile);
            CFRELEASE(fileString);
            
            ++fmtPtr;
        }
    } while (0);

    return paths;
}

#else

#   define PATHS_COUNT 8
#   if defined(__ANDROID__)
#       define X_PATHS_COUNT 1
#   else
#       define X_PATHS_COUNT 0
#   endif
char **_copy_paths_main(const char *fileName) {

    const char *fmts[PATHS_COUNT + X_PATHS_COUNT + 1] = {
        "%s/disks/%s",
        "%s/disks/demo/%s",
        "%s/disks/blanks/%s",
        "%s/disks/3rd-party-test/%s",
        "%s/external-disks/%s",
        "%s/external-disks/demo/%s",
        "%s/external-disks/blanks/%s",
        "%s/external-disks/3rd-party-test/%s",
#   if defined(__ANDROID__)
        // extra paths ...
        "/sdcard/apple2ix/%s",
#   endif
        NULL,
    };

    char **paths = CALLOC(1, sizeof(fmts));
    assert(paths);

    do {
        const char *fmt = NULL;
        const char **fmtPtr = &fmts[0];
        unsigned int idx = 0;
        while (*fmtPtr) {
            fmt = *fmtPtr++;
            if (idx < PATHS_COUNT) {
                ASPRINTF(&paths[idx++], fmt, data_dir, fileName);
            } else {
                ASPRINTF(&paths[idx++], fmt, fileName);
            }
        }

    } while (0);

    return paths;
}

#endif

char **test_copy_disk_paths(const char *fileName) {
#if (TARGET_OS_MAC || TARGET_OS_PHONE)
    return _copy_paths_mac(fileName);
#else
    return _copy_paths_main(fileName);
#endif
}

int test_setup_boot_disk(const char *fileName, int readonly) {

#if (TARGET_OS_MAC || TARGET_OS_PHONE)
    char **paths = _copy_paths_mac(fileName);
#else
    char **paths = _copy_paths_main(fileName);
#endif

    int err = 0;

    char **path = &paths[0];
    while (*path) {
        char *disk = *path;
        ++path;

        int fd = -1;
        TEMP_FAILURE_RETRY(fd = open(disk, readonly ? O_RDONLY : O_RDWR));
        if (fd != -1) {
            err = disk6_insert(fd, /*drive:*/0, disk, readonly) != NULL;
            TEMP_FAILURE_RETRY(close(fd));
            if (!err) {
                break;
            }
        }
    }

    path = &paths[0];
    while (*path) {
        char *disk = *path;
        ++path;
        FREE(disk);
    }

    FREE(paths);

    return err;
}

void sha1_to_str(const uint8_t * const md, char *buf) {
    int i=0;
    for (int j=0; j<SHA_DIGEST_LENGTH; j++, i+=2) {
        sprintf(buf+i, "%02X", md[j]);
    }
}

