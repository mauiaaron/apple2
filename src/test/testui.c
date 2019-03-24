/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2016 Aaron Culliney
 *
 */

#include "testcommon.h"
#include <sys/mman.h>

#define BLANK_DSK "blank.dsk.gz"

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testui_setup(void *arg) {
    test_common_setup();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    if (test_do_reboot) {
        run_args.joy_button0 = 0xff; // OpenApple
        cpu65_interrupt(ResetSig);
    }
}

static void testui_teardown(void *arg) {
}

// ----------------------------------------------------------------------------
// Various Tests ...

#if CONFORMANT_TRACKS
#   define BLANK_RUN_BYTE 1208
#   define BLANK_TRACK_WIDTH 6384
#else
#   define BLANK_RUN_BYTE 1130
#   define BLANK_TRACK_WIDTH 6040
#endif

extern uint8_t (*iie_read_peripheral_card)(uint16_t);

static int _assert_blank_boot(void) {
    // Disk6 ...
    ASSERT(disk6.motor_off == 1);
    ASSERT(disk6.drive == 0);
    ASSERT(disk6.ddrw == 0);
    ASSERT(disk6.disk_byte == 0xAA);
    extern int stepper_phases;
    ASSERT(stepper_phases == 0x0);
    ASSERT(disk6.disk[0].is_protected);
    const char *file_name = strrchr(disk6.disk[0].file_name, '/');
    ASSERT(strcmp(file_name, "/blank.dsk.gz") == 0);
    ASSERT(disk6.disk[0].phase == 36);
    ASSERT(disk6.disk[0].run_byte == BLANK_RUN_BYTE);
    ASSERT(disk6.disk[0].fd > 0);
    ASSERT(disk6.disk[0].raw_image_data != 0);
    ASSERT(disk6.disk[0].raw_image_data != MAP_FAILED);
    ASSERT(disk6.disk[0].whole_len == 143360);
    ASSERT(disk6.disk[0].nib_image_data != NULL);
    ASSERT(disk6.disk[0].track_width == BLANK_TRACK_WIDTH);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);
    extern int skew_table_6_do[16];
    ASSERT(disk6.disk[0].skew_table == skew_table_6_do);

    // VM ...
    ASSERT(run_args.softswitches  == 0x000100d1);
    ASSERT_SHA_BIN("97AADDDF5D20B793C4558A8928227F0B52565A98", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("2C82E33E964936187CA1DABF71AE6148916BD131", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("36F1699537024EC6017A22641FF0EC277AFFD49D", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(run_args.base_ramrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_ramwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_textrd   == apple_ii_64k[0]);
    ASSERT(run_args.base_textwrt  == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_stackzp  == apple_ii_64k[0]);
    ASSERT(run_args.base_c3rom    == apple_ii_64k[1]);
    ASSERT(run_args.base_cxrom    == (void *)&iie_read_peripheral_card);
    ASSERT(run_args.base_d000_rd  == apple_ii_64k[0]);
    ASSERT(run_args.base_d000_wrt == language_banks[0] - 0xD000);
    ASSERT(run_args.base_e000_rd  == apple_ii_64k[0]);
    ASSERT(run_args.base_e000_wrt == language_card[0] - 0xE000);

    // CPU ...
    ASSERT(run_args.cpu65_pc      == 0xE783);
    ASSERT(run_args.cpu65_ea      == 0x1F33);
    ASSERT(run_args.cpu65_a       == 0xFF);
    ASSERT(run_args.cpu65_f       == 0x37);
    ASSERT(run_args.cpu65_x       == 0xFF);
    ASSERT(run_args.cpu65_y       == 0x00);
    ASSERT(run_args.cpu65_sp      == 0xF6);

    PASS();
}

static int _get_fds(JSON_ref jsonData, int *fdA, int *fdB) {
    {
        char *pathA = NULL;
        bool ret = json_mapCopyStringValue(jsonData, "diskA", &pathA);
        ASSERT(ret);
        ASSERT(pathA);

        bool readOnlyA = true;
        ret = json_mapParseBoolValue(jsonData, "readOnlyA", &readOnlyA);

        *fdA = -1;
        TEMP_FAILURE_RETRY(*fdA = open(pathA, readOnlyA ? O_RDONLY : O_RDWR));
        FREE(pathA);
    }
    {
        char *pathB = NULL;
        bool ret = json_mapCopyStringValue(jsonData, "diskB", &pathB);
        ASSERT(ret);
        ASSERT(pathB);

        bool readOnlyB = true;
        ret = json_mapParseBoolValue(jsonData, "readOnlyB", &readOnlyB);

        *fdB = -1;
        TEMP_FAILURE_RETRY(*fdB = open(pathB, readOnlyB ? O_RDONLY : O_RDWR));
        FREE(pathB);
    }

    return 0;
}

TEST test_save_state_1() {
    test_setup_boot_disk(BLANK_DSK, 1);

    BOOT_TO_DOS();

    _assert_blank_boot();

    char *savData = NULL;
    ASPRINTF(&savData, "%s/emulator-test.a2state", HOMEDIR);

    int fd = -1;
    TEMP_FAILURE_RETRY(fd = open(savData, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR));
    ASSERT(fd > 0);

    bool ret = emulator_saveState(fd);
    ASSERT(ret);

    WAIT_FOR_FB_SHA(BOOT_SCREEN);

    TEMP_FAILURE_RETRY(close(fd));

    FREE(savData);
    PASS();
}

TEST test_load_state_1() {

    // ensure stable test
    disk6_eject(0);
    debugger_setTimeout(1);
    debugger_clearWatchpoints();
    debugger_go();
    debugger_setTimeout(0);

    char *savData = NULL;
    ASPRINTF(&savData, "%s/emulator-test.a2state", HOMEDIR);

    int fdState = -1;
    TEMP_FAILURE_RETRY(fdState = open(savData, O_RDONLY));
    ASSERT(fdState > 0);

    bool ret = false;
    int fdA = -1;
    int fdB = -1;
    {
        JSON_ref jsonData;
        int siz = json_createFromString("{}", &jsonData);
        ASSERT(siz > 0);
        ret = emulator_stateExtractDiskPaths(fdState, jsonData);
        ASSERT(ret);
        _get_fds(jsonData, &fdA, &fdB);
        json_destroy(&jsonData);
    }

    ret = emulator_loadState(fdState, fdA, fdB);
    ASSERT(ret);

    _assert_blank_boot();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    WAIT_FOR_FB_SHA(BOOT_SCREEN);

    TEMP_FAILURE_RETRY(close(fdState));
    TEMP_FAILURE_RETRY(close(fdA));
    TEMP_FAILURE_RETRY(close(fdB));

    unlink(savData);
    FREE(savData);

    PASS();
}

TEST test_load_A2VM_good1() {

    // ensure stable test
    disk6_eject(0);
    debugger_setTimeout(1);
    debugger_clearWatchpoints();
    debugger_go();
    debugger_setTimeout(0);

    // write saved state to disk

#include "test/a2vm-good1.h"

    const char *homedir = HOMEDIR;
    char *savData = NULL;
    ASPRINTF(&savData, "%s/a2_emul_a2vm.dat", homedir);
    if (savData) {
        unlink(savData);
    }

    FILE *fp = fopen(savData, "w");
    ASSERT(fp);
    size_t dataSiz = sizeof(data);
    if (fwrite(data, 1, dataSiz, fp) != dataSiz) {
        ASSERT(false);
    }
    fflush(fp); fclose(fp); fp = NULL;

    // load state and assert

    int fdState = -1;
    TEMP_FAILURE_RETRY(fdState = open(savData, O_RDONLY));
    ASSERT(fdState > 0);

    bool ret = false;
    int fdA = -1;
    int fdB = -1;
    {
        JSON_ref jsonData;
        int siz = json_createFromString("{}", &jsonData);
        ASSERT(siz > 0);
        ret = emulator_stateExtractDiskPaths(fdState, jsonData);
        ASSERT(ret);
        _get_fds(jsonData, &fdA, &fdB);
        json_destroy(&jsonData);
    }

    ret = emulator_loadState(fdState, fdA, fdB);
    ASSERT(ret);

    // Disk6 ... AVOID ASSERT()ing for non-portable things
    ASSERT(disk6.motor_off == 1);
    ASSERT(disk6.drive == 0);
    ASSERT(disk6.ddrw == 0);
    ASSERT(disk6.disk_byte == 0xAA);
    extern int stepper_phases;
    ASSERT(stepper_phases == 0x0);
    //ASSERT(disk6.disk[0].is_protected);
    //const char *file_name = strrchr(disk6.disk[0].file_name, '/');
    //ASSERT(strcmp(file_name, "/testdisplay1.dsk") == 0);
    ASSERT(disk6.disk[0].phase == 34);
    ASSERT(disk6.disk[0].run_byte == 2000);
    //ASSERT(disk6.disk[0].fd > 0);
    //ASSERT(disk6.disk[0].raw_image_data != 0);
    //ASSERT(disk6.disk[0].raw_image_data != MAP_FAILED);
    //ASSERT(disk6.disk[0].whole_len == 143360);
    //ASSERT(disk6.disk[0].nib_image_data != NULL);
    //ASSERT(disk6.disk[0].track_width == BLANK_TRACK_WIDTH);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);
    //extern int skew_table_6_do[16];
    //ASSERT(disk6.disk[0].skew_table == skew_table_6_do);

    // VM ...
    ASSERT(run_args.softswitches  == 0x000343d1);
    ASSERT_SHA_BIN("2E3C6163EEAA817B02B00766B9E118D3197D16AF", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("2C82E33E964936187CA1DABF71AE6148916BD131", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("36F1699537024EC6017A22641FF0EC277AFFD49D", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(run_args.base_ramrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_ramwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_textrd   == apple_ii_64k[0]);
    ASSERT(run_args.base_textwrt  == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_stackzp  == apple_ii_64k[0]);
    ASSERT(run_args.base_c3rom    == apple_ii_64k[1]);
    ASSERT(run_args.base_cxrom    == (void *)&iie_read_peripheral_card);
    ASSERT(run_args.base_d000_rd  == apple_ii_64k[0]);
    ASSERT(run_args.base_d000_wrt == language_banks[0] - 0xD000);
    ASSERT(run_args.base_e000_rd  == apple_ii_64k[0]);
    ASSERT(run_args.base_e000_wrt == language_card[0] - 0xE000);

    // CPU ...
    ASSERT(run_args.cpu65_pc      == 0xC83D);
    ASSERT(run_args.cpu65_ea      == 0x004E);
    ASSERT(run_args.cpu65_a       == 0x0D);
    ASSERT(run_args.cpu65_f       == 0x35);
    ASSERT(run_args.cpu65_x       == 0x09);
    ASSERT(run_args.cpu65_y       == 0x01);
    ASSERT(run_args.cpu65_sp      == 0xEA);

    // ASSERT framebuffer matches expected
    WAIT_FOR_FB_SHA("8BF579CC23A8A6D5E13F5BA3CEEE9DA7714F72B6");

    TEMP_FAILURE_RETRY(close(fdState));
    TEMP_FAILURE_RETRY(close(fdA));
    TEMP_FAILURE_RETRY(close(fdB));

    unlink(savData);
    FREE(savData);

    PASS();
}

TEST test_load_A2V2_good1() {

    // ensure stable test
    disk6_eject(0);
    debugger_setTimeout(1);
    debugger_clearWatchpoints();
    debugger_go();
    debugger_setTimeout(0);

    // write saved state to disk

#include "test/a2v2-good1.h"

    const char *homedir = HOMEDIR;
    char *savData = NULL;
    ASPRINTF(&savData, "%s/a2_emul_a2v2.dat", homedir);
    if (savData) {
        unlink(savData);
    }

    FILE *fp = fopen(savData, "w");
    ASSERT(fp);
    size_t dataSiz = sizeof(data);
    if (fwrite(data, 1, dataSiz, fp) != dataSiz) {
        ASSERT(false);
    }
    fflush(fp); fclose(fp); fp = NULL;

    // load state and assert

    int fdState = -1;
    TEMP_FAILURE_RETRY(fdState = open(savData, O_RDONLY));
    ASSERT(fdState > 0);

    bool ret = false;
    int fdA = -1;
    int fdB = -1;
    {
        JSON_ref jsonData;
        int siz = json_createFromString("{}", &jsonData);
        ASSERT(siz > 0);
        ret = emulator_stateExtractDiskPaths(fdState, jsonData);
        ASSERT(ret);
        _get_fds(jsonData, &fdA, &fdB);
        json_destroy(&jsonData);
    }

    ret = emulator_loadState(fdState, fdA, fdB);
    ASSERT(ret);

    // Disk6 ... AVOID ASSERT()ing for non-portable things
    ASSERT(disk6.motor_off == 1);
    ASSERT(disk6.drive == 0);
    ASSERT(disk6.ddrw == 0);
    ASSERT(disk6.disk_byte == 0x96);
    extern int stepper_phases;
    ASSERT(stepper_phases == 0x0);
    //ASSERT(disk6.disk[0].is_protected);
    //const char *file_name = strrchr(disk6.disk[0].file_name, '/');
    //ASSERT(strcmp(file_name, "/NSCT.dsk") == 0);
    ASSERT(disk6.disk[0].phase == 26);
    ASSERT(disk6.disk[0].run_byte == 5562);
    //ASSERT(disk6.disk[0].fd > 0);
    //ASSERT(disk6.disk[0].raw_image_data != 0);
    //ASSERT(disk6.disk[0].raw_image_data != MAP_FAILED);
    //ASSERT(disk6.disk[0].whole_len == 143360);
    //ASSERT(disk6.disk[0].nib_image_data != NULL);
    //ASSERT(disk6.disk[0].track_width == BLANK_TRACK_WIDTH);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);
    //extern int skew_table_6_do[16];
    //ASSERT(disk6.disk[0].skew_table == skew_table_6_do);

    // VM ...
    ASSERT(run_args.softswitches  == 0x000140f5);
    ASSERT_SHA_BIN("3B41CCC86A7FCE2A95F1D7A5C4BF7E2AC7A11323", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("54C8611AA3FD1813B1BEE45EF7F4B2303C51C679", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("36F1699537024EC6017A22641FF0EC277AFFD49D", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(run_args.base_ramrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_ramwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_textrd   == apple_ii_64k[0]);
    ASSERT(run_args.base_textwrt  == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_stackzp  == apple_ii_64k[0]);
    ASSERT(run_args.base_c3rom    == apple_ii_64k[1]);
    ASSERT(run_args.base_cxrom    == (void *)&iie_read_peripheral_card);
    ASSERT(run_args.base_d000_rd == language_banks[0]-0xD000);
    ASSERT(run_args.base_d000_wrt == language_banks[0]-0xD000);
    ASSERT(run_args.base_e000_rd  == language_card[0]-0xE000);
    ASSERT(run_args.base_e000_wrt == language_card[0]-0xE000);

    // CPU ...
    ASSERT(run_args.cpu65_pc      == 0xF6EA);
    ASSERT(run_args.cpu65_ea      == 0x0018);
    ASSERT(run_args.cpu65_a       == 0x05);
    ASSERT(run_args.cpu65_f       == 0x33);
    ASSERT(run_args.cpu65_x       == 0x10);
    ASSERT(run_args.cpu65_y       == 0x02);
    ASSERT(run_args.cpu65_sp      == 0xFA);

    // Timing ...
    long scaleFactor = (long)(cpu_scale_factor * 100.);
    ASSERT(scaleFactor == 100);
    long altScaleFactor = (long)(cpu_altscale_factor * 100.);
    ASSERT(altScaleFactor == 200);
    ASSERT(alt_speed_enabled);

    // Mockingboard ...
#include "test/a2v2-good1-mb.h"
    size_t mbSiz = sizeof(mbData);
    mb_testAssertA2V2(mbData, mbSiz);

    // ASSERT framebuffer matches expected
    WAIT_FOR_FB_SHA("3FB6BF44F5A7A6742550B33B627FFC5F06BAEC94");

    TEMP_FAILURE_RETRY(close(fdState));
    TEMP_FAILURE_RETRY(close(fdA));
    TEMP_FAILURE_RETRY(close(fdB));

    unlink(savData);
    FREE(savData);

    PASS();
}

TEST test_load_A2V2_good2() {

    // ensure stable test
    disk6_eject(0);
    debugger_setTimeout(1);
    debugger_clearWatchpoints();
    debugger_go();
    debugger_setTimeout(0);

    // write saved state to disk

#include "test/a2v2-good2.h"

    const char *homedir = HOMEDIR;
    char *savData = NULL;
    ASPRINTF(&savData, "%s/a2_emul_a2v2-2.dat", homedir);
    if (savData) {
        unlink(savData);
    }

    FILE *fp = fopen(savData, "w");
    ASSERT(fp);
    size_t dataSiz = sizeof(data);
    if (fwrite(data, 1, dataSiz, fp) != dataSiz) {
        ASSERT(false);
    }
    fflush(fp); fclose(fp); fp = NULL;

    // load state and assert

    int fdState = -1;
    TEMP_FAILURE_RETRY(fdState = open(savData, O_RDONLY));
    ASSERT(fdState > 0);

    bool ret = false;
    int fdA = -1;
    int fdB = -1;
    {
        JSON_ref jsonData;
        int siz = json_createFromString("{}", &jsonData);
        ASSERT(siz > 0);
        ret = emulator_stateExtractDiskPaths(fdState, jsonData);
        ASSERT(ret);
        _get_fds(jsonData, &fdA, &fdB);
        json_destroy(&jsonData);
    }

    ret = emulator_loadState(fdState, fdA, fdB);
    ASSERT(ret);

    // Disk6 ... AVOID ASSERT()ing for non-portable things
    ASSERT(disk6.motor_off == 1);
    ASSERT(disk6.drive == 0);
    ASSERT(disk6.ddrw == 0);
    ASSERT(disk6.disk_byte == 0xAA);
    extern int stepper_phases;
    ASSERT(stepper_phases == 0x0);
    //ASSERT(disk6.disk[0].is_protected);
    //const char *file_name = strrchr(disk6.disk[0].file_name, '/');
    //ASSERT(strcmp(file_name, "/u5boot.do") == 0);
    ASSERT(disk6.disk[0].phase == 58);
    ASSERT(disk6.disk[0].run_byte == 1208);
    //ASSERT(disk6.disk[0].fd > 0);
    //ASSERT(disk6.disk[0].raw_image_data != 0);
    //ASSERT(disk6.disk[0].raw_image_data != MAP_FAILED);
    //ASSERT(disk6.disk[0].whole_len == 143360);
    //ASSERT(disk6.disk[0].nib_image_data != NULL);
    //ASSERT(disk6.disk[0].track_width == BLANK_TRACK_WIDTH);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);
    //extern int skew_table_6_do[16];
    //ASSERT(disk6.disk[0].skew_table == skew_table_6_do);

    // VM ...
    ASSERT(run_args.softswitches  == 0x000140f4);
    ASSERT_SHA_BIN("CAD59B53F04DE501A76E0C04750155C838EADAE2", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("B3268356F9F4F4ACAE2F4FF49D4FED1D36535DDA", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("0B6E45306506F92554102485CE9B500C6779D145", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(run_args.base_ramrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_ramwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_textrd   == apple_ii_64k[0]);
    ASSERT(run_args.base_textwrt  == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_stackzp  == apple_ii_64k[0]);
    ASSERT(run_args.base_c3rom    == apple_ii_64k[1]);
    ASSERT(run_args.base_cxrom    == (void *)&iie_read_peripheral_card);
    ASSERT(run_args.base_d000_rd == language_banks[0]-0xD000);
    ASSERT(run_args.base_d000_wrt == language_banks[0]-0xD000);
    ASSERT(run_args.base_e000_rd  == language_card[0]-0xE000);
    ASSERT(run_args.base_e000_wrt == language_card[0]-0xE000);

    // CPU ...
    ASSERT(run_args.cpu65_pc      == 0x8474);
    ASSERT(run_args.cpu65_ea      == 0x8474);
    ASSERT(run_args.cpu65_a       == 0x00);
    ASSERT(run_args.cpu65_f       == 0x73);
    ASSERT(run_args.cpu65_x       == 0x04);
    ASSERT(run_args.cpu65_y       == 0x21);
    ASSERT(run_args.cpu65_sp      == 0xF1);

    // Timing ...
    long scaleFactor = (long)(cpu_scale_factor * 100.);
    ASSERT(scaleFactor == 100);
    long altScaleFactor = (long)(cpu_altscale_factor * 100.);
    ASSERT(altScaleFactor == 75);
    ASSERT(alt_speed_enabled);

    // Mockingboard ...
#include "test/a2v2-good2-mb.h"
    size_t mbSiz = sizeof(mbData);
    mb_testAssertA2V2(mbData, mbSiz);

    // ASSERT framebuffer matches expected
    WAIT_FOR_FB_SHA("FD20698466B30B5BE9DA298731B6B5AC2C94C56D");

    TEMP_FAILURE_RETRY(close(fdState));
    TEMP_FAILURE_RETRY(close(fdA));
    TEMP_FAILURE_RETRY(close(fdB));

    unlink(savData);
    FREE(savData);

    PASS();
}

TEST test_load_A2V2_good3() {

    // ensure stable test
    disk6_eject(0);
    debugger_setTimeout(1);
    debugger_clearWatchpoints();
    debugger_go();
    debugger_setTimeout(0);

    // write saved state to disk

#include "test/a2v2-good3.h"

    const char *homedir = HOMEDIR;
    char *savData = NULL;
    ASPRINTF(&savData, "%s/a2_emul_a2v2-3.dat", homedir);
    if (savData) {
        unlink(savData);
    }

    FILE *fp = fopen(savData, "w");
    ASSERT(fp);
    size_t dataSiz = sizeof(data);
    if (fwrite(data, 1, dataSiz, fp) != dataSiz) {
        ASSERT(false);
    }
    fflush(fp); fclose(fp); fp = NULL;

    // load state and assert

    int fdState = -1;
    TEMP_FAILURE_RETRY(fdState = open(savData, O_RDONLY));
    ASSERT(fdState > 0);

    bool ret = false;
    int fdA = -1;
    int fdB = -1;
    {
        JSON_ref jsonData;
        int siz = json_createFromString("{}", &jsonData);
        ASSERT(siz > 0);
        ret = emulator_stateExtractDiskPaths(fdState, jsonData);
        ASSERT(ret);
        _get_fds(jsonData, &fdA, &fdB);
        json_destroy(&jsonData);
    }

    ret = emulator_loadState(fdState, fdA, fdB);
    ASSERT(ret);

    // Disk6 ... AVOID ASSERT()ing for non-portable things ... in particular this a2state file contains Droid content://
    // paths that will not be valid (even on the original device), so the disk6_insert() call will have failed.
    // emulator_stateRestore() will have logged the fault but continued optimistically
    ASSERT(disk6.motor_off == 1);
    ASSERT(disk6.drive == 1);
    ASSERT(disk6.ddrw == 0);
    ASSERT(disk6.disk_byte == 0xAA);
    extern int stepper_phases;
    ASSERT(stepper_phases == 0x0);
    ASSERT(disk6.disk[0].phase == 6);
    ASSERT(disk6.disk[0].run_byte == 1141);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);

    ASSERT(disk6.disk[1].phase == 50);
    ASSERT(disk6.disk[1].run_byte == 5277);
    ASSERT(!disk6.disk[1].nibblized);
    ASSERT(!disk6.disk[1].track_dirty);

    // VM ...
    ASSERT(run_args.softswitches  == 0x000140f4);
    ASSERT_SHA_BIN("16A730D3E709F096B693EA4029FC68672CE454B8", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("DF3EE367193484A6A1C28C2BAE0EFEF42E6D19BB", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("343C30374074AB3AEE22581A6477736121390B18", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(run_args.base_ramrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_ramwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_textrd   == apple_ii_64k[0]);
    ASSERT(run_args.base_textwrt  == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrrd    == apple_ii_64k[0]);
    ASSERT(run_args.base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(run_args.base_stackzp  == apple_ii_64k[0]);
    ASSERT(run_args.base_c3rom    == apple_ii_64k[1]);
    ASSERT(run_args.base_cxrom    == (void *)&iie_read_peripheral_card);
    ASSERT(run_args.base_d000_rd == language_banks[0]-0xD000);
    ASSERT(run_args.base_d000_wrt == language_banks[0]-0xD000);
    ASSERT(run_args.base_e000_rd  == language_card[0]-0xE000);
    ASSERT(run_args.base_e000_wrt == language_card[0]-0xE000);

    // CPU ...
    ASSERT(run_args.cpu65_pc      == 0x0E9C);
    ASSERT(run_args.cpu65_ea      == 0x0EB9);
    ASSERT(run_args.cpu65_a       == 0x00);
    ASSERT(run_args.cpu65_f       == 0xB0);
    ASSERT(run_args.cpu65_x       == 0x05);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0xE0);

    // Timing ...
    long scaleFactor = (long)(cpu_scale_factor * 100.);
    ASSERT(scaleFactor == 100);
    long altScaleFactor = (long)(cpu_altscale_factor * 100.);
    ASSERT(altScaleFactor == 100);
    ASSERT(!alt_speed_enabled);

    // Mockingboard ...
#include "test/a2v2-good3-mb.h"
    size_t mbSiz = sizeof(mbData);
    mb_testAssertA2V2(mbData, mbSiz);

    // ASSERT framebuffer matches expected
    WAIT_FOR_FB_SHA("70BFB1572C377578EAAB31AC09C4C383366628BD");

    TEMP_FAILURE_RETRY(close(fdState));
    TEMP_FAILURE_RETRY(close(fdA));
    TEMP_FAILURE_RETRY(close(fdB));

    unlink(savData);
    FREE(savData);

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_ui) {
    pthread_mutex_lock(&interface_mutex);

    test_thread_running = true;

    GREATEST_SET_SETUP_CB(testui_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testui_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------

    RUN_TESTp(test_save_state_1);
    RUN_TESTp(test_load_state_1);

    RUN_TESTp(test_load_A2VM_good1);

    RUN_TESTp(test_load_A2V2_good1);
    RUN_TESTp(test_load_A2V2_good2);
    RUN_TESTp(test_load_A2V2_good3);

#if INTERFACE_TOUCH
#   warning TODO : touch joystick(s), keyboard, mouse, menu
#endif

#if INTERFACE_CLASSIC
#   warning TODO : menus, stuff-n-things
#endif

    // ...
    disk6_eject(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_ui);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static int _test_thread(void) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_ui);
    GREATEST_MAIN_END();
}

static void *test_thread(void *dummyptr) {
    _test_thread();
    return NULL;
}

void test_ui(int _argc, char **_argv) {
    test_argc = _argc;
    test_argv = _argv;

    test_common_init();

    pthread_t p;
    pthread_create(&p, NULL, (void *)&test_thread, (void *)NULL);
    while (!test_thread_running) {
        struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
    pthread_detach(p);
}

