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

#ifndef _TESTCOMMON_H_
#define _TESTCOMMON_H_

#include "common.h"
#include "greatest.h"

#ifdef __APPLE__
#   include <CommonCrypto/CommonDigest.h>
#   define SHA_DIGEST_LENGTH CC_SHA1_DIGEST_LENGTH
#   define SHA1 CC_SHA1
#else
#   include "test/sha1.h"
#endif

#define TEST_FINISHED 0xff
#define MIXSWITCH_ADDR 0x1f32   // PEEK(7986) -- NOTE : value is hardcoded in various places
#define WATCHPOINT_ADDR 0x1f33  // PEEK(7987) -- NOTE : value is hardcoded in various places
#define TESTOUT_ADDR 0x1f43     // PEEK(8003) -- NOTE : value is hardcoded in various places

#define BLANK_SCREEN "6C8ABA272F220F00BE0E76A8659A1E30C2D3CDBE"
#define BOOT_SCREEN  "AAFB9163DD526F6E57C3BF9FCA5D5222C19E8A02"

extern char mdstr[(SHA_DIGEST_LENGTH*2)+1];

extern bool test_do_reboot;
void test_breakpoint(void *arg);
void test_common_init(void);
void test_common_setup(void);
void test_type_input(const char *input);
void test_type_input_deterministically(const char *input);
char **test_copy_disk_paths(const char *fileName);
int test_setup_boot_disk(const char *fileName, int readonly);
void sha1_to_str(const uint8_t * const md, char *buf);

static inline bool _matchFramebufferSHA(const char *SHA_STR, bool do_assert) {
    uint8_t md[SHA_DIGEST_LENGTH];

    PIXEL_TYPE *fb = NULL;
    extern void timing_setVideoDirty(void);
    timing_setVideoDirty();
    fb = display_waitForNextCompleteFramebuffer();
    SHA1((const uint8_t *)fb, SCANWIDTH*SCANHEIGHT*PIXEL_STRIDE, md);

    sha1_to_str(md, mdstr);
    bool matches = strcasecmp(mdstr, SHA_STR) == 0;

    if (do_assert) {
        ASSERT(matches && "check global mdstr if failed...");
        PASS();
    } else {
        return matches;
    }
}

#define ASSERT_SHA(SHA_STR) \
do { \
    int ret = _matchFramebufferSHA(SHA_STR, /*do_assert:*/true); \
    if (ret != 0) { \
        return ret; \
    } \
} while (0)

#define WAIT_FOR_FB_SHA(SHA_STR) \
do { \
    unsigned int matchAttempts = 0; \
    const unsigned int maxMatchAttempts = 10; \
    do { \
        bool matches = _matchFramebufferSHA(SHA_STR, /*do_assert:*/false); \
        if (matches) { \
            break; \
        } \
    } while (matchAttempts++ < maxMatchAttempts); \
    if (matchAttempts >= maxMatchAttempts) { \
        fprintf(GREATEST_STDOUT, "DID NOT FIND SHA %s...\n", SHA_STR); \
        ASSERT(0); \
    } \
} while (0)

static inline int ASSERT_SHA_MEM(const char *SHA_STR, uint16_t ea, uint16_t len) {
    uint8_t md[SHA_DIGEST_LENGTH];
    const uint8_t * const mem = &apple_ii_64k[0][ea];
    SHA1(mem, len, md);
    sha1_to_str(md, mdstr);
    ASSERT(strcasecmp(mdstr, SHA_STR) == 0);
    return 0;
}

static inline int ASSERT_SHA_BIN(const char *SHA_STR, const uint8_t * const buf, unsigned int len) {
    uint8_t md[SHA_DIGEST_LENGTH];
    SHA1(buf, len, md);
    sha1_to_str(md, mdstr);
    ASSERT(strcasecmp(mdstr, SHA_STR) == 0);
    return 0;
}

static inline int BOOT_TO_DOS(void) {
    if (test_do_reboot) {
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
        c_debugger_go();
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
        apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    }
    return 0;
}

static inline void REBOOT_TO_DOS(void) {
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    run_args.joy_button0 = 0xff;
    cpu65_interrupt(ResetSig);
}

#endif // whole file
