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
#define BOOT_SCREEN  "F8D6C781E0BB7B3DDBECD69B25E429D845506594"

extern char mdstr[(SHA_DIGEST_LENGTH*2)+1];

extern bool test_do_reboot;
void test_breakpoint(void *arg);
void test_common_init(void);
void test_common_setup(void);
void test_type_input(const char *input);
int test_setup_boot_disk(const char *fileName, int readonly);
void sha1_to_str(const uint8_t * const md, char *buf);

static inline int ASSERT_SHA(const char *SHA_STR) {
    uint8_t md[SHA_DIGEST_LENGTH];
    const uint8_t * const fb = video_scan();
    SHA1(fb, SCANWIDTH*SCANHEIGHT, md);
    sha1_to_str(md, mdstr);
    ASSERT(strcasecmp(mdstr, SHA_STR) == 0);
    return 0;
}

static inline int ASSERT_SHA_MEM(const char *SHA_STR, uint16_t ea, uint16_t len) {
    uint8_t md[SHA_DIGEST_LENGTH];
    const uint8_t * const mem = &apple_ii_64k[0][ea];
    SHA1(mem, len, md);
    sha1_to_str(md, mdstr);
    ASSERT(strcasecmp(mdstr, SHA_STR) == 0);
    return 0;
}

static inline int BOOT_TO_DOS(void) {
    if (test_do_reboot) {
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
        c_debugger_go();
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
        ASSERT_SHA(BOOT_SCREEN);
        apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    }
    return 0;
}

static inline void REBOOT_TO_DOS(void) {
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    joy_button0 = 0xff;
    cpu65_interrupt(ResetSig);
}

#endif // whole file
