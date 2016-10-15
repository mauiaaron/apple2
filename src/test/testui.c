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
        joy_button0 = 0xff; // OpenApple
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
    ASSERT(strcmp(file_name, "/blank.dsk") == 0);
    ASSERT(disk6.disk[0].phase == 36);
    ASSERT(disk6.disk[0].run_byte == BLANK_RUN_BYTE);
    ASSERT(disk6.disk[0].fd > 0);
    ASSERT(disk6.disk[0].mmap_image != 0);
    ASSERT(disk6.disk[0].mmap_image != MAP_FAILED);
    ASSERT(disk6.disk[0].whole_len == 143360);
    ASSERT(disk6.disk[0].whole_image != NULL);
    ASSERT(disk6.disk[0].track_width == BLANK_TRACK_WIDTH);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);
    extern int skew_table_6_do[16];
    ASSERT(disk6.disk[0].skew_table == skew_table_6_do);

    // VM ...
    ASSERT(softswitches  == 0x000140d1);
    ASSERT_SHA_BIN("97AADDDF5D20B793C4558A8928227F0B52565A98", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("2C82E33E964936187CA1DABF71AE6148916BD131", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("36F1699537024EC6017A22641FF0EC277AFFD49D", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(base_ramrd    == apple_ii_64k[0]);
    ASSERT(base_ramwrt   == apple_ii_64k[0]);
    ASSERT(base_textrd   == apple_ii_64k[0]);
    ASSERT(base_textwrt  == apple_ii_64k[0]);
    ASSERT(base_hgrrd    == apple_ii_64k[0]);
    ASSERT(base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(base_stackzp  == apple_ii_64k[0]);
    ASSERT(base_c3rom    == apple_ii_64k[1]);
    ASSERT(base_cxrom    == (void *)&iie_read_peripheral_card);
    ASSERT(base_d000_rd  == apple_ii_64k[0]);
    ASSERT(base_d000_wrt == language_banks[0] - 0xD000);
    ASSERT(base_e000_rd  == apple_ii_64k[0]);
    ASSERT(base_e000_wrt == language_card[0] - 0xE000);

    // CPU ...
    ASSERT(cpu65_pc      == 0xE783);
    ASSERT(cpu65_ea      == 0x1F33);
    ASSERT(cpu65_a       == 0xFF);
    ASSERT(cpu65_f       == 0x37);
    ASSERT(cpu65_x       == 0xFF);
    ASSERT(cpu65_y       == 0x00);
    ASSERT(cpu65_sp      == 0xF6);

    PASS();
}

TEST test_save_state_1() {
    test_setup_boot_disk(BLANK_DSK, 1);

    BOOT_TO_DOS();

    _assert_blank_boot();

    char *savFile = NULL;
    ASPRINTF(&savFile, "%s/emulator-test.state", HOMEDIR);

    bool ret = emulator_saveState(savFile);
    ASSERT(ret);

    FREE(savFile);
    PASS();
}

TEST test_load_state_1() {

    // ensure stable test
    disk6_eject(0);
    c_debugger_set_timeout(1);
    c_debugger_clear_watchpoints();
    c_debugger_go();
    c_debugger_set_timeout(0);

    char *savFile = NULL;
    ASPRINTF(&savFile, "%s/emulator-test.state", HOMEDIR);

    bool ret = emulator_loadState(savFile);
    ASSERT(ret);

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    _assert_blank_boot();

    unlink(savFile);
    FREE(savFile);

    PASS();
}

TEST test_load_A2VM_good1() {

    // ensure stable test
    disk6_eject(0);
    c_debugger_set_timeout(1);
    c_debugger_clear_watchpoints();
    c_debugger_go();
    c_debugger_set_timeout(0);

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

    bool ret = emulator_loadState(savData);
    ASSERT(ret);

    // ASSERT framebuffer matches expected
    ASSERT_SHA("9C654FEF2A672E16D89ED2FB80C593CD2005A026");

    // Disk6 ...
    ASSERT(disk6.motor_off == 1);
    ASSERT(disk6.drive == 0);
    ASSERT(disk6.ddrw == 0);
    ASSERT(disk6.disk_byte == 0xAA);
    extern int stepper_phases;
    ASSERT(stepper_phases == 0x0);
    ASSERT(disk6.disk[0].is_protected);
    const char *file_name = strrchr(disk6.disk[0].file_name, '/');
    ASSERT(strcmp(file_name, "/testdisplay1.dsk") == 0);
    ASSERT(disk6.disk[0].phase == 34);
    ASSERT(disk6.disk[0].run_byte == 2000);
    ASSERT(disk6.disk[0].fd > 0);
    ASSERT(disk6.disk[0].mmap_image != 0);
    ASSERT(disk6.disk[0].mmap_image != MAP_FAILED);
    ASSERT(disk6.disk[0].whole_len == 143360);
    ASSERT(disk6.disk[0].whole_image != NULL);
    ASSERT(disk6.disk[0].track_width == BLANK_TRACK_WIDTH);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);
    extern int skew_table_6_do[16];
    ASSERT(disk6.disk[0].skew_table == skew_table_6_do);

    // VM ...
    ASSERT(softswitches  == 0x000343d1);
    ASSERT_SHA_BIN("2E3C6163EEAA817B02B00766B9E118D3197D16AF", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("2C82E33E964936187CA1DABF71AE6148916BD131", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("36F1699537024EC6017A22641FF0EC277AFFD49D", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(base_ramrd    == apple_ii_64k[0]);
    ASSERT(base_ramwrt   == apple_ii_64k[0]);
    ASSERT(base_textrd   == apple_ii_64k[0]);
    ASSERT(base_textwrt  == apple_ii_64k[0]);
    ASSERT(base_hgrrd    == apple_ii_64k[0]);
    ASSERT(base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(base_stackzp  == apple_ii_64k[0]);
    ASSERT(base_c3rom    == apple_ii_64k[1]);
    ASSERT(base_cxrom    == (void *)&iie_read_peripheral_card);
    ASSERT(base_d000_rd  == apple_ii_64k[0]);
    ASSERT(base_d000_wrt == language_banks[0] - 0xD000);
    ASSERT(base_e000_rd  == apple_ii_64k[0]);
    ASSERT(base_e000_wrt == language_card[0] - 0xE000);

    // CPU ...
    ASSERT(cpu65_pc      == 0xC83D);
    ASSERT(cpu65_ea      == 0x004E);
    ASSERT(cpu65_a       == 0x0D);
    ASSERT(cpu65_f       == 0x35);
    ASSERT(cpu65_x       == 0x09);
    ASSERT(cpu65_y       == 0x01);
    ASSERT(cpu65_sp      == 0xEA);

    unlink(savData);
    FREE(savData);

    PASS();
}

TEST test_load_A2V2_good1() {

    // ensure stable test
    disk6_eject(0);
    c_debugger_set_timeout(1);
    c_debugger_clear_watchpoints();
    c_debugger_go();
    c_debugger_set_timeout(0);

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

    bool ret = emulator_loadState(savData);
    ASSERT(ret);

    // ASSERT framebuffer matches expected
    ASSERT_SHA("EE8AF0ADB3AF477A713B9E5FD02D655CF8371F7B");

    // Disk6 ...
    ASSERT(disk6.motor_off == 1);
    ASSERT(disk6.drive == 0);
    ASSERT(disk6.ddrw == 0);
    ASSERT(disk6.disk_byte == 0x96);
    extern int stepper_phases;
    ASSERT(stepper_phases == 0x0);
    ASSERT(disk6.disk[0].is_protected);
    const char *file_name = strrchr(disk6.disk[0].file_name, '/');
    ASSERT(strcmp(file_name, "/NSCT.dsk") == 0);
    ASSERT(disk6.disk[0].phase == 26);
    ASSERT(disk6.disk[0].run_byte == 5562);
    ASSERT(disk6.disk[0].fd > 0);
    ASSERT(disk6.disk[0].mmap_image != 0);
    ASSERT(disk6.disk[0].mmap_image != MAP_FAILED);
    ASSERT(disk6.disk[0].whole_len == 143360);
    ASSERT(disk6.disk[0].whole_image != NULL);
    ASSERT(disk6.disk[0].track_width == BLANK_TRACK_WIDTH);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);
    extern int skew_table_6_do[16];
    ASSERT(disk6.disk[0].skew_table == skew_table_6_do);

    // VM ...
    ASSERT(softswitches  == 0x000140f5);
    ASSERT_SHA_BIN("0AEA333FBDAE26959719E3BDD6AEFA408784F614", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("6D54FC0A19EC396113863C15D607EAE55F369C0D", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("36F1699537024EC6017A22641FF0EC277AFFD49D", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(base_ramrd    == apple_ii_64k[0]);
    ASSERT(base_ramwrt   == apple_ii_64k[0]);
    ASSERT(base_textrd   == apple_ii_64k[0]);
    ASSERT(base_textwrt  == apple_ii_64k[0]);
    ASSERT(base_hgrrd    == apple_ii_64k[0]);
    ASSERT(base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(base_stackzp  == apple_ii_64k[0]);
    ASSERT(base_c3rom    == apple_ii_64k[1]);
    ASSERT(base_cxrom    == (void *)&iie_read_peripheral_card);
    ASSERT(base_d000_rd == language_banks[0]-0xD000);
    ASSERT(base_d000_wrt == language_banks[0]-0xD000);
    ASSERT(base_e000_rd  == language_card[0]-0xE000);
    ASSERT(base_e000_wrt == language_card[0]-0xE000);

    // CPU ...
    ASSERT(cpu65_pc      == 0xF30F);
    ASSERT(cpu65_ea      == 0xF30E);
    ASSERT(cpu65_a       == 0x01);
    ASSERT(cpu65_f       == 0x30);
    ASSERT(cpu65_x       == 0x0D);
    ASSERT(cpu65_y       == 0x03);
    ASSERT(cpu65_sp      == 0xFC);

    // Timing ...
    long scaleFactor = (long)(cpu_scale_factor * 100.);
    ASSERT(scaleFactor = 100);
    long altScaleFactor = (long)(cpu_altscale_factor * 100.);
    ASSERT(altScaleFactor = 200);
    ASSERT(alt_speed_enabled);

    // Mockingboard ...
    {
        uint8_t *exData = MALLOC(1024);
        uint8_t *p8 = exData;
        uint16_t *p16 = 0;
        unsigned int *p32 = 0;
        unsigned int changeCount = 0;
        unsigned long *pL = 0;
        unsigned short *pS = 0;

        // ----------------------------------------------------------------- 00

        // --------------- SY6522

        *p8++ = 0x00; // ORA
        *p8++ = 0x04; // ORB
        *p8++ = 0xFF; // DDRA
        *p8++ = 0x07; // DDRB

        p16 = (uint16_t *)p8;

        *p16++ = 0x02EC; // TIMER1_COUNTER
        *p16++ = 0x4FBA; // TIMER1_LATCH
        *p16++ = 0xDFDF; // TIMER2_COUNTER
        *p16++ = 0x0000; // TIMER2_LATCH

        p8 = (uint8_t *)p16;

        *p8++ = 0x00; // SERIAL_SHIFT
        *p8++ = 0x40; // ACR
        *p8++ = 0x00; // PCR
        *p8++ = 0x00; // IFR
        *p8++ = 0x40; // IER

        // --------------- AY8910

        p32 = (unsigned int *)p8;

        *p32++ = (unsigned int)105; // ay_tone_tick[0]
        *p32++ = (unsigned int)35; // ay_tone_tick[1]
        *p32++ = (unsigned int)43; // ay_tone_tick[2]
        *p32++ = (unsigned int)1; // ay_tone_high[0]
        *p32++ = (unsigned int)0; // ay_tone_high[1]
        *p32++ = (unsigned int)0; // ay_tone_high[2]

        *p32++ = (unsigned int)1; // ay_noise_tick
        *p32++ = (unsigned int)152756; // ay_tone_subcycles
        *p32++ = (unsigned int)677044; // ay_env_subcycles
        *p32++ = (unsigned int)5; // ay_env_internal_tick
        *p32++ = (unsigned int)12; // ay_env_tick
        *p32++ = (unsigned int)3033035; // ay_tick_incr

        *p32++ = (unsigned int)162; // ay_tone_period[0]
        *p32++ = (unsigned int)460; // ay_tone_period[1]
        *p32++ = (unsigned int)60; // ay_tone_period[2]

        *p32++ = (unsigned int)5; // ay_noise_period
        *p32++ = (unsigned int)15; // ay_env_period
        *p32++ = (unsigned int)71674; // rng
        *p32++ = (unsigned int)1; // noise_toggle
        *p32++ = (unsigned int)0; // env_first
        *p32++ = (unsigned int)0; // env_rev
        *p32++ = (unsigned int)0; // env_counter

        p8 = (uint8_t *)p32;
        *p8++ = 0xA2; // sound_ay_registers[0]
        *p8++ = 0x00; // sound_ay_registers[1]
        *p8++ = 0xCC; // sound_ay_registers[2]
        *p8++ = 0x01; // sound_ay_registers[3]
        *p8++ = 0x3C; // sound_ay_registers[4]
        *p8++ = 0x00; // sound_ay_registers[5]
        *p8++ = 0x05; // sound_ay_registers[6]
        *p8++ = 0xF8; // sound_ay_registers[7]
        *p8++ = 0x0D; // sound_ay_registers[8]
        *p8++ = 0x0E; // sound_ay_registers[9]
        *p8++ = 0x04; // sound_ay_registers[10]
        *p8++ = 0x0F; // sound_ay_registers[11]
        *p8++ = 0x00; // sound_ay_registers[12]
        *p8++ = 0x00; // sound_ay_registers[13]
        *p8++ = 0x00; // sound_ay_registers[14]
        *p8++ = 0x00; // sound_ay_registers[15]

        p32 = (unsigned int *)p8;
        changeCount = 13;
        *p32++ = changeCount; // ay_change_count

        pL = (unsigned long *)p32;
        *pL++ = 102; // ay_change[0].tstates
        pS = (uint16_t *)pL;
        *pS++ = 2; // ay_change[0].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 0; // ay_change[0].reg
        *p8++ = 102; // ay_change[0].val

        pL = (unsigned long *)p8;
        *pL++ = 159; // ay_change[1].tstates
        pS = (uint16_t *)pL;
        *pS++ = 3; // ay_change[1].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 1; // ay_change[1].reg
        *p8++ = 0; // ay_change[1].val

        pL = (unsigned long *)p8;
        *pL++ = 216; // ay_change[2].tstates
        pS = (uint16_t *)pL;
        *pS++ = 4; // ay_change[2].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 2; // ay_change[2].reg
        *p8++ = 102; // ay_change[2].val

        pL = (unsigned long *)p8;
        *pL++ = 273; // ay_change[3].tstates
        pS = (uint16_t *)pL;
        *pS++ = 5; // ay_change[3].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 3; // ay_change[3].reg
        *p8++ = 2; // ay_change[3].val

        pL = (unsigned long *)p8;
        *pL++ = 330; // ay_change[4].tstates
        pS = (uint16_t *)pL;
        *pS++ = 7; // ay_change[4].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 4; // ay_change[4].reg
        *p8++ = 60; // ay_change[4].val

        pL = (unsigned long *)p8;
        *pL++ = 387; // ay_change[5].tstates
        pS = (uint16_t *)pL;
        *pS++ = 8; // ay_change[5].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 5; // ay_change[5].reg
        *p8++ = 0; // ay_change[5].val

        pL = (unsigned long *)p8;
        *pL++ = 444; // ay_change[6].tstates
        pS = (uint16_t *)pL;
        *pS++ = 9; // ay_change[6].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 6; // ay_change[6].reg
        *p8++ = 5; // ay_change[6].val

        pL = (unsigned long *)p8;
        *pL++ = 501; // ay_change[7].tstates
        pS = (uint16_t *)pL;
        *pS++ = 10; // ay_change[7].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 7; // ay_change[7].reg
        *p8++ = 248; // ay_change[7].val

        pL = (unsigned long *)p8;
        *pL++ = 563; // ay_change[8].tstates
        pS = (uint16_t *)pL;
        *pS++ = 12; // ay_change[8].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 8; // ay_change[8].reg
        *p8++ = 13; // ay_change[8].val

        pL = (unsigned long *)p8;
        *pL++ = 621; // ay_change[9].tstates
        pS = (uint16_t *)pL;
        *pS++ = 13; // ay_change[9].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 9; // ay_change[9].reg
        *p8++ = 14; // ay_change[9].val

        pL = (unsigned long *)p8;
        *pL++ = 679; // ay_change[10].tstates
        pS = (uint16_t *)pL;
        *pS++ = 14; // ay_change[10].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 10; // ay_change[10].reg
        *p8++ = 3; // ay_change[10].val

        pL = (unsigned long *)p8;
        *pL++ = 731; // ay_change[11].tstates
        pS = (uint16_t *)pL;
        *pS++ = 15; // ay_change[11].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 11; // ay_change[11].reg
        *p8++ = 15; // ay_change[11].val

        pL = (unsigned long *)p8;
        *pL++ = 788; // ay_change[12].tstates
        pS = (uint16_t *)pL;
        *pS++ = 17; // ay_change[12].ofs
        p8 = (uint8_t *)pS;
        *p8++ = 12; // ay_change[12].reg
        *p8++ = 0; // ay_change[12].val

        // --------------- SSI263

        *p8++ = 0x00; // DurationPhoneme
        *p8++ = 0x00; // Inflection
        *p8++ = 0x00; // RateInflection
        *p8++ = 0x00; // CtrlArtAmp
        *p8++ = 0x00; // FilterFreq
        *p8++ = 0x00; // CurrentMode

        // ---------------

        *p8++ = 0x0C; // nAYCurrentRegister

        // ----------------------------------------------------------------- 01

        // --------------- SY6522

        *p8++ = 0x00; // ORA
        *p8++ = 0x04; // ORB
        *p8++ = 0xFF; // DDRA
        *p8++ = 0x07; // DDRB

        p16 = (uint16_t *)p8;

        *p16++ = 0xDFDF; // TIMER1_COUNTER
        *p16++ = 0x0000; // TIMER1_LATCH
        *p16++ = 0xDFDF; // TIMER2_COUNTER
        *p16++ = 0x0000; // TIMER2_LATCH

        p8 = (uint8_t *)p16;

        *p8++ = 0x00; // SERIAL_SHIFT
        *p8++ = 0x00; // ACR
        *p8++ = 0x00; // PCR
        *p8++ = 0x00; // IFR
        *p8++ = 0x00; // IER

        // --------------- AY8910

        p32 = (unsigned int *)p8;

        *p32++ = (unsigned int)17; // ay_tone_tick[0]
        *p32++ = (unsigned int)416; // ay_tone_tick[1]
        *p32++ = (unsigned int)27; // ay_tone_tick[2]
        *p32++ = (unsigned int)1; // ay_tone_high[0]
        *p32++ = (unsigned int)0; // ay_tone_high[1]
        *p32++ = (unsigned int)0; // ay_tone_high[2]

        *p32++ = (unsigned int)0; // ay_noise_tick
        *p32++ = (unsigned int)152756; // ay_tone_subcycles
        *p32++ = (unsigned int)677044; // ay_env_subcycles
        *p32++ = (unsigned int)10; // ay_env_internal_tick
        *p32++ = (unsigned int)13; // ay_env_tick
        *p32++ = (unsigned int)3033035; // ay_tick_incr

        *p32++ = (unsigned int)121; // ay_tone_period[0]
        *p32++ = (unsigned int)547; // ay_tone_period[1]
        *p32++ = (unsigned int)60; // ay_tone_period[2]

        *p32++ = (unsigned int)5; // ay_noise_period
        *p32++ = (unsigned int)15; // ay_env_period
        *p32++ = (unsigned int)55289; // rng
        *p32++ = (unsigned int)0; // noise_toggle
        *p32++ = (unsigned int)0; // env_first
        *p32++ = (unsigned int)0; // env_rev
        *p32++ = (unsigned int)0; // env_counter

        p8 = (uint8_t *)p32;
        *p8++ = 0x79; // sound_ay_registers[0]
        *p8++ = 0x00; // sound_ay_registers[1]
        *p8++ = 0x23; // sound_ay_registers[2]
        *p8++ = 0x02; // sound_ay_registers[3]
        *p8++ = 0x3C; // sound_ay_registers[4]
        *p8++ = 0x00; // sound_ay_registers[5]
        *p8++ = 0x05; // sound_ay_registers[6]
        *p8++ = 0xF8; // sound_ay_registers[7]
        *p8++ = 0x0D; // sound_ay_registers[8]
        *p8++ = 0x0E; // sound_ay_registers[9]
        *p8++ = 0x04; // sound_ay_registers[10]
        *p8++ = 0x0F; // sound_ay_registers[11]
        *p8++ = 0x00; // sound_ay_registers[12]
        *p8++ = 0x00; // sound_ay_registers[13]
        *p8++ = 0x00; // sound_ay_registers[14]
        *p8++ = 0x00; // sound_ay_registers[15]

        p32 = (unsigned int *)p8;
        changeCount = 0;
        *p32++ = changeCount; // ay_change_count
        p8 = (uint8_t *)p32;

        // --------------- SSI263

        *p8++ = 0x00; // DurationPhoneme
        *p8++ = 0x00; // Inflection
        *p8++ = 0x00; // RateInflection
        *p8++ = 0x00; // CtrlArtAmp
        *p8++ = 0x00; // FilterFreq
        *p8++ = 0x00; // CurrentMode

        // ---------------

        *p8++ = 0x0C; // nAYCurrentRegister

        // ----------------------------------------------------------------- 02

        // --------------- SY6522

        *p8++ = 0x00; // ORA
        *p8++ = 0x00; // ORB
        *p8++ = 0x00; // DDRA
        *p8++ = 0x00; // DDRB

        p16 = (uint16_t *)p8;

        *p16++ = 0xDFDF; // TIMER1_COUNTER
        *p16++ = 0x0000; // TIMER1_LATCH
        *p16++ = 0xDFDF; // TIMER2_COUNTER
        *p16++ = 0x0000; // TIMER2_LATCH

        p8 = (uint8_t *)p16;

        *p8++ = 0x00; // SERIAL_SHIFT
        *p8++ = 0x00; // ACR
        *p8++ = 0x00; // PCR
        *p8++ = 0x00; // IFR
        *p8++ = 0x00; // IER

        // --------------- AY8910

        p32 = (unsigned int *)p8;

        *p32++ = (unsigned int)0; // ay_tone_tick[0]
        *p32++ = (unsigned int)0; // ay_tone_tick[1]
        *p32++ = (unsigned int)0; // ay_tone_tick[2]
        *p32++ = (unsigned int)1; // ay_tone_high[0]
        *p32++ = (unsigned int)1; // ay_tone_high[1]
        *p32++ = (unsigned int)1; // ay_tone_high[2]

        *p32++ = (unsigned int)3529473; // ay_noise_tick
        *p32++ = (unsigned int)234404; // ay_tone_subcycles
        *p32++ = (unsigned int)758692; // ay_env_subcycles
        *p32++ = (unsigned int)1; // ay_env_internal_tick
        *p32++ = (unsigned int)3529473; // ay_env_tick
        *p32++ = (unsigned int)3033035; // ay_tick_incr

        *p32++ = (unsigned int)1; // ay_tone_period[0]
        *p32++ = (unsigned int)1; // ay_tone_period[1]
        *p32++ = (unsigned int)1; // ay_tone_period[2]

        *p32++ = (unsigned int)0; // ay_noise_period
        *p32++ = (unsigned int)0; // ay_env_period
        *p32++ = (unsigned int)71376; // rng
        *p32++ = (unsigned int)1; // noise_toggle
        *p32++ = (unsigned int)0; // env_first
        *p32++ = (unsigned int)0; // env_rev
        *p32++ = (unsigned int)0; // env_counter

        p8 = (uint8_t *)p32;
        *p8++ = 0x00; // sound_ay_registers[0]
        *p8++ = 0x00; // sound_ay_registers[1]
        *p8++ = 0x00; // sound_ay_registers[2]
        *p8++ = 0x00; // sound_ay_registers[3]
        *p8++ = 0x00; // sound_ay_registers[4]
        *p8++ = 0x00; // sound_ay_registers[5]
        *p8++ = 0x00; // sound_ay_registers[6]
        *p8++ = 0x00; // sound_ay_registers[7]
        *p8++ = 0x00; // sound_ay_registers[8]
        *p8++ = 0x00; // sound_ay_registers[9]
        *p8++ = 0x00; // sound_ay_registers[10]
        *p8++ = 0x00; // sound_ay_registers[11]
        *p8++ = 0x00; // sound_ay_registers[12]
        *p8++ = 0x00; // sound_ay_registers[13]
        *p8++ = 0x00; // sound_ay_registers[14]
        *p8++ = 0x00; // sound_ay_registers[15]

        p32 = (unsigned int *)p8;
        changeCount = 0;
        *p32++ = changeCount; // ay_change_count
        p8 = (uint8_t *)p32;

        // --------------- SSI263

        *p8++ = 0x00; // DurationPhoneme
        *p8++ = 0x00; // Inflection
        *p8++ = 0x00; // RateInflection
        *p8++ = 0x00; // CtrlArtAmp
        *p8++ = 0x00; // FilterFreq
        *p8++ = 0x00; // CurrentMode

        // ---------------

        *p8++ = 0x00; // nAYCurrentRegister

        // ----------------------------------------------------------------- 03

        // --------------- SY6522

        *p8++ = 0x00; // ORA
        *p8++ = 0x00; // ORB
        *p8++ = 0x00; // DDRA
        *p8++ = 0x00; // DDRB

        p16 = (uint16_t *)p8;

        *p16++ = 0xDFDF; // TIMER1_COUNTER
        *p16++ = 0x0000; // TIMER1_LATCH
        *p16++ = 0xDFDF; // TIMER2_COUNTER
        *p16++ = 0x0000; // TIMER2_LATCH

        p8 = (uint8_t *)p16;

        *p8++ = 0x00; // SERIAL_SHIFT
        *p8++ = 0x00; // ACR
        *p8++ = 0x00; // PCR
        *p8++ = 0x00; // IFR
        *p8++ = 0x00; // IER

        // --------------- AY8910

        p32 = (unsigned int *)p8;

        *p32++ = (unsigned int)0; // ay_tone_tick[0]
        *p32++ = (unsigned int)0; // ay_tone_tick[1]
        *p32++ = (unsigned int)0; // ay_tone_tick[2]
        *p32++ = (unsigned int)1; // ay_tone_high[0]
        *p32++ = (unsigned int)1; // ay_tone_high[1]
        *p32++ = (unsigned int)1; // ay_tone_high[2]

        *p32++ = (unsigned int)3529473; // ay_noise_tick
        *p32++ = (unsigned int)234404; // ay_tone_subcycles
        *p32++ = (unsigned int)758692; // ay_env_subcycles
        *p32++ = (unsigned int)1; // ay_env_internal_tick
        *p32++ = (unsigned int)3529473; // ay_env_tick
        *p32++ = (unsigned int)3033035; // ay_tick_incr

        *p32++ = (unsigned int)1; // ay_tone_period[0]
        *p32++ = (unsigned int)1; // ay_tone_period[1]
        *p32++ = (unsigned int)1; // ay_tone_period[2]

        *p32++ = (unsigned int)0; // ay_noise_period
        *p32++ = (unsigned int)0; // ay_env_period
        *p32++ = (unsigned int)71376; // rng
        *p32++ = (unsigned int)1; // noise_toggle
        *p32++ = (unsigned int)0; // env_first
        *p32++ = (unsigned int)0; // env_rev
        *p32++ = (unsigned int)0; // env_counter

        p8 = (uint8_t *)p32;
        *p8++ = 0x00; // sound_ay_registers[0]
        *p8++ = 0x00; // sound_ay_registers[1]
        *p8++ = 0x00; // sound_ay_registers[2]
        *p8++ = 0x00; // sound_ay_registers[3]
        *p8++ = 0x00; // sound_ay_registers[4]
        *p8++ = 0x00; // sound_ay_registers[5]
        *p8++ = 0x00; // sound_ay_registers[6]
        *p8++ = 0x00; // sound_ay_registers[7]
        *p8++ = 0x00; // sound_ay_registers[8]
        *p8++ = 0x00; // sound_ay_registers[9]
        *p8++ = 0x00; // sound_ay_registers[10]
        *p8++ = 0x00; // sound_ay_registers[11]
        *p8++ = 0x00; // sound_ay_registers[12]
        *p8++ = 0x00; // sound_ay_registers[13]
        *p8++ = 0x00; // sound_ay_registers[14]
        *p8++ = 0x00; // sound_ay_registers[15]

        p32 = (unsigned int *)p8;
        changeCount = 0;
        *p32++ = changeCount; // ay_change_count
        p8 = (uint8_t *)p32;

        // --------------- SSI263

        *p8++ = 0x00; // DurationPhoneme
        *p8++ = 0x00; // Inflection
        *p8++ = 0x00; // RateInflection
        *p8++ = 0x00; // CtrlArtAmp
        *p8++ = 0x00; // FilterFreq
        *p8++ = 0x00; // CurrentMode

        // ---------------

        *p8++ = 0x00; // nAYCurrentRegister

        // --------------------------------------------------------------------

        size_t exSiz = p8 - exData;
        ASSERT(exSiz <= 1024);
        mb_testAssertA2V2(exData, exSiz);

        FREE(exData);
    }

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

    srandom(time(NULL));

    test_common_init();

    pthread_t p;
    pthread_create(&p, NULL, (void *)&test_thread, (void *)NULL);
    while (!test_thread_running) {
        struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
    pthread_detach(p);
}

