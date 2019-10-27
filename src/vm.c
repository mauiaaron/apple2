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

#include "common.h"

extern const uint8_t apple_iie_rom[32768]; // rom.c

uint8_t apple_ii_64k[2][65536] = { { 0 } };
uint8_t language_card[2][8192] = { { 0 } };
uint8_t language_banks[2][8192] = { { 0 } };

const uint8_t *base_vmem = apple_ii_64k[0];

#if VM_TRACING
FILE *test_vm_fp = NULL;

typedef struct vm_trace_range_t {
    uint16_t lo;
    uint16_t hi;
} vm_trace_range_t;
#endif

// ----------------------------------------------------------------------------

GLUE_BANK_READ(iie_read_ram_default, BASE_RAMRD);
GLUE_BANK_WRITE(iie_write_ram_default, BASE_RAMWRT);

GLUE_BANK_READ(read_ram_bank, BASE_D000_RD);
GLUE_BANK_MAYBEWRITE(write_ram_bank, BASE_D000_WRT);

GLUE_BANK_READ(read_ram_lc, BASE_E000_RD);
GLUE_BANK_MAYBEWRITE(write_ram_lc, BASE_E000_WRT);

GLUE_BANK_READ(iie_read_ram_text_page0, BASE_TEXTRD);

GLUE_BANK_READ(iie_read_ram_hires_page0, BASE_HGRRD);

GLUE_BANK_READ(iie_read_ram_zpage_and_stack, BASE_STACKZP);
GLUE_BANK_WRITE(iie_write_ram_zpage_and_stack, BASE_STACKZP);

GLUE_BANK_MAYBE_READ_C3(iie_read_slot3, BASE_C3ROM);
GLUE_BANK_MAYBE_READ_CX(iie_read_slot4, BASE_C4ROM);
GLUE_BANK_MAYBE_READ_CX(iie_read_slot5, BASE_C5ROM);
GLUE_BANK_MAYBE_READ_CX(iie_read_slotx, BASE_CXROM);

GLUE_NOP(write_ram_nop);

GLUE_C_READ(iie_read_peripheral_card)
{
    assert(((ea >> 12) & 0xf) == 0xC && "should only be called for 0xC100-0xC7FF");
    uint8_t slot = ((ea >> 8) & 0xf);

    // TODO FIXME : route to correct peripheral card
    switch (slot) {
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x7:
            break;

        case 0x4:
        case 0x5:
            return mb_read(ea); // FIXME : hardcoded Mockingboards ...
            break;

        case 0x6:
            return apple_ii_64k[0][ea];
            break;

        default:
            assert(false && "internal configuration error!");
            break;
    }

    // no card in slot
    return floating_bus();
}

static uint8_t read_keyboard_strobe(void) {
    apple_ii_64k[0][0xC000] &= 0x7F;
    apple_ii_64k[1][0xC000] &= 0x7F;
    return apple_ii_64k[0][0xC000];
}

// ----------------------------------------------------------------------------
// graphics softswitches

static uint8_t iie_page2_off(void) {
    if (!(run_args.softswitches & SS_PAGE2)) {
        return floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches &= ~(SS_PAGE2|SS_SCREEN);

    if (run_args.softswitches & SS_80STORE) {
        run_args.softswitches &= ~(SS_TEXTRD|SS_TEXTWRT);
        run_args.base_textrd  = apple_ii_64k[0];
        run_args.base_textwrt = apple_ii_64k[0];
        if (run_args.softswitches & SS_HIRES) {
            run_args.softswitches &= ~(SS_HGRRD|SS_HGRWRT);
            run_args.base_hgrrd  = apple_ii_64k[0];
            run_args.base_hgrwrt = apple_ii_64k[0];
        }
    }

    return floating_bus();
}

static uint8_t iie_page2_on(void) {
    if (run_args.softswitches & SS_PAGE2) {
        return floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches |= SS_PAGE2;

    if (run_args.softswitches & SS_80STORE) {
        run_args.softswitches |= (SS_TEXTRD|SS_TEXTWRT);
        run_args.base_textrd  = apple_ii_64k[1];
        run_args.base_textwrt = apple_ii_64k[1];
        if (run_args.softswitches & SS_HIRES) {
            run_args.softswitches |= (SS_HGRRD|SS_HGRWRT);
            run_args.base_hgrrd  = apple_ii_64k[1];
            run_args.base_hgrwrt = apple_ii_64k[1];
        }
    } else {
        run_args.softswitches |= SS_SCREEN;
    }

    return floating_bus();
}

static uint8_t read_switch_graphics(void) {
    if (run_args.softswitches & SS_TEXT) {
        video_setDirty(A2_DIRTY_FLAG);
        run_args.softswitches &= ~SS_TEXT;
    }
    return floating_bus();
}

static uint8_t read_switch_text(void) {
    if (!(run_args.softswitches & SS_TEXT)) {
        video_setDirty(A2_DIRTY_FLAG);
        run_args.softswitches |= SS_TEXT;
    }
    return floating_bus();
}

static uint8_t read_switch_no_mixed(void) {
    if (run_args.softswitches & SS_MIXED) {
        video_setDirty(A2_DIRTY_FLAG);
        run_args.softswitches &= ~SS_MIXED;
    }
    return floating_bus();
}

static uint8_t read_switch_mixed(void) {
    if (!(run_args.softswitches & SS_MIXED)) {
        video_setDirty(A2_DIRTY_FLAG);
        run_args.softswitches |= SS_MIXED;
    }
    return floating_bus();
}

static uint8_t iie_annunciator(uint16_t ea)
{
    if ((ea >= 0xC058) && (ea <= 0xC05B)) {
        // TODO: alternate joystick management?
    }
    return floating_bus();
}

static uint8_t iie_hires_off(void) {
    if (!(run_args.softswitches & SS_HIRES)) {
        return floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches &= ~(SS_HIRES|SS_HGRRD|SS_HGRWRT);
    run_args.base_hgrrd  = apple_ii_64k[0];
    run_args.base_hgrwrt = apple_ii_64k[0];

    if (run_args.softswitches & SS_RAMRD) {
        run_args.base_hgrrd = apple_ii_64k[1];
        run_args.softswitches |= SS_HGRRD;
    }

    if (run_args.softswitches & SS_RAMWRT) {
        run_args.base_hgrwrt = apple_ii_64k[1];
        run_args.softswitches |= SS_HGRWRT;
    }

    return floating_bus();
}

static uint8_t iie_hires_on(void) {
    if (run_args.softswitches & SS_HIRES) {
        return floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches |= SS_HIRES;

    if (run_args.softswitches & SS_80STORE) {
        if (run_args.softswitches & SS_PAGE2) {
            run_args.softswitches |= (SS_HGRRD|SS_HGRWRT);
            run_args.base_hgrrd  = apple_ii_64k[1];
            run_args.base_hgrwrt = apple_ii_64k[1];
        } else {
            run_args.softswitches &= ~(SS_HGRRD|SS_HGRWRT);
            run_args.base_hgrrd  = apple_ii_64k[0];
            run_args.base_hgrwrt = apple_ii_64k[0];
        }
    }

    return floating_bus();
}

GLUE_C_WRITE(video__write_2e_text0)
{
    do {
        drawpage_mode_t mode = video_currentMainMode(run_args.softswitches);
        if (mode == DRAWPAGE_HIRES) {
            break;
        }
        if (!(run_args.softswitches & SS_PAGE2)) {
            uint8_t b0 = run_args.base_textwrt[ea];
            if (b0 != b) {
                video_setDirty(A2_DIRTY_FLAG);
            }
        }
    } while (0);
    run_args.base_textwrt[ea] = b;
}

GLUE_C_WRITE(video__write_2e_text0_mixed)
{
    do {
        drawpage_mode_t mode = video_currentMixedMode(run_args.softswitches);
        if (mode == DRAWPAGE_HIRES) {
            break;
        }
        if (!(run_args.softswitches & SS_PAGE2)) {
            uint8_t b0 = run_args.base_textwrt[ea];
            if (b0 != b) {
                video_setDirty(A2_DIRTY_FLAG);
            }
        }
    } while (0);
    run_args.base_textwrt[ea] = b;
}

GLUE_C_WRITE(video__write_2e_text1)
{
    do {
        drawpage_mode_t mode = video_currentMainMode(run_args.softswitches);
        if (mode == DRAWPAGE_HIRES) {
            break;
        }
        if ((run_args.softswitches & SS_PAGE2) && !(run_args.softswitches & SS_80STORE)) {
            uint8_t b0 = run_args.base_ramwrt[ea];
            if (b0 != b) {
                video_setDirty(A2_DIRTY_FLAG);
            }
        }
    } while (0);
    run_args.base_ramwrt[ea] = b;
}

GLUE_C_WRITE(video__write_2e_text1_mixed)
{
    do {
        drawpage_mode_t mode = video_currentMixedMode(run_args.softswitches);
        if (mode == DRAWPAGE_HIRES) {
            break;
        }
        if ((run_args.softswitches & SS_PAGE2) && !(run_args.softswitches & SS_80STORE)) {
            uint8_t b0 = run_args.base_ramwrt[ea];
            if (b0 != b) {
                video_setDirty(A2_DIRTY_FLAG);
            }
        }
    } while (0);
    run_args.base_ramwrt[ea] = b;
}

GLUE_C_WRITE(video__write_2e_hgr0)
{
    do {
        drawpage_mode_t mode = video_currentMainMode(run_args.softswitches);
        if (mode == DRAWPAGE_TEXT) {
            break;
        }
        if (!(run_args.softswitches & SS_PAGE2)) {
            uint8_t b0 = run_args.base_hgrwrt[ea];
            if (b0 != b) {
                video_setDirty(A2_DIRTY_FLAG);
            }
        }
    } while (0);
    run_args.base_hgrwrt[ea] = b;
}

GLUE_C_WRITE(video__write_2e_hgr0_mixed)
{
    do {
        drawpage_mode_t mode = video_currentMixedMode(run_args.softswitches);
        if (mode == DRAWPAGE_TEXT) {
            break;
        }
        if (!(run_args.softswitches & SS_PAGE2)) {
            uint8_t b0 = run_args.base_hgrwrt[ea];
            if (b0 != b) {
                video_setDirty(A2_DIRTY_FLAG);
            }
        }
    } while (0);
    run_args.base_hgrwrt[ea] = b;
}

GLUE_C_WRITE(video__write_2e_hgr1)
{
    do {
        drawpage_mode_t mode = video_currentMainMode(run_args.softswitches);
        if (mode == DRAWPAGE_TEXT) {
            break;
        }
        if ((run_args.softswitches & SS_PAGE2) && !(run_args.softswitches & SS_80STORE)) {
            uint8_t b0 = run_args.base_ramwrt[ea];
            if (b0 != b) {
                video_setDirty(A2_DIRTY_FLAG);
            }
        }
    } while (0);
    run_args.base_ramwrt[ea] = b;
}

GLUE_C_WRITE(video__write_2e_hgr1_mixed)
{
    do {
        drawpage_mode_t mode = video_currentMixedMode(run_args.softswitches);
        if (mode == DRAWPAGE_TEXT) {
            break;
        }
        if ((run_args.softswitches & SS_PAGE2) && !(run_args.softswitches & SS_80STORE)) {
            uint8_t b0 = run_args.base_ramwrt[ea];
            if (b0 != b) {
                video_setDirty(A2_DIRTY_FLAG);
            }
        }
    } while (0);
    run_args.base_ramwrt[ea] = b;
}

// ----------------------------------------------------------------------------
// GC softswitches : Game Controller (joystick/paddles)
#define JOY_STEP_USEC (3300.0 / 256.0)
#define CYCLES_PER_USEC (CLK_6502 / 1000000)
#define JOY_STEP_CYCLES (JOY_STEP_USEC / CYCLES_PER_USEC)

static uint8_t read_button0(void) {
    uint8_t b0 = floating_bus() & (~0x80);
    uint8_t b = run_args.joy_button0 & 0x80;
    return b0 | b;
}

static uint8_t read_button1(void) {
    uint8_t b0 = floating_bus() & (~0x80);
    uint8_t b = run_args.joy_button1 & 0x80;
    return b0 | b;
}

static uint8_t read_button2(void) {
    uint8_t b = floating_bus() & (~0x80);
    uint8_t b0 = run_args.joy_button0 & 0x80;
    uint8_t b1 = run_args.joy_button1 & 0x80;
    return b | b0 | b1;
}

static uint8_t read_gc_strobe(void) {
    // Read Game Controller (paddle) strobe ...
    // From _Understanding the Apple IIe_ :
    //  * 7-29, discussing PREAD : "The timer duration will vary between 2 and 3302 usecs"
    //  * 7-30, timer reset : "But the timer pulse may still be high from the previous [strobe access] and the timers are
    //  not retriggered by C07X' if they have not yet reset from the previous trigger"
    if (run_args.gc_cycles_timer_0 <= 0)
    {
        run_args.gc_cycles_timer_0 = (int)((joy_x-5) * JOY_STEP_CYCLES);
    }
    if (run_args.gc_cycles_timer_1 <= 0)
    {
        run_args.gc_cycles_timer_1 = (int)(joy_y * JOY_STEP_CYCLES) + 2;
    }

    // NOTE (possible TODO FIXME): unimplemented GC2 and GC3 timers since they were not wired on the //e ...
    return floating_bus();
}

static uint8_t read_gc0(void) {
    if (run_args.gc_cycles_timer_0 <= 0)
    {
        run_args.gc_cycles_timer_0 = 0;
        return 0;
    }
    return 0xFF;
}

static uint8_t read_gc1(void)
{
    if (run_args.gc_cycles_timer_1 <= 0)
    {
        run_args.gc_cycles_timer_1 = 0;
        return 0;
    }
    return 0xFF;
}

static uint8_t iie_read_gc2(void) {
    return floating_bus();
}

static uint8_t iie_read_gc3(void) {
    return floating_bus();
}

// ----------------------------------------------------------------------------
// LC : language card routines

static inline void _lc_to_auxmem() {
    if (run_args.softswitches & SS_LCRAM) {
        run_args.base_d000_rd += 0x2000;
        run_args.base_e000_rd = language_card[0]-0xC000;
    }

    if (run_args.softswitches & SS_LCWRT) {
        run_args.base_d000_wrt += 0x2000;
        run_args.base_e000_wrt = language_card[0]-0xC000;
    }
}

static uint8_t iie_c080(void) {
    run_args.softswitches |= (SS_LCRAM|SS_BANK2);
    run_args.softswitches &= ~(SS_LCSEC|SS_LCWRT);

    run_args.base_d000_rd = language_banks[0]-0xD000;
    run_args.base_e000_rd = language_card[0]-0xE000;

    run_args.base_d000_wrt = 0;
    run_args.base_e000_wrt = 0;

    if (run_args.softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

static uint8_t iie_c081(void) {
    if (run_args.softswitches & SS_LCSEC) {
        run_args.softswitches |= SS_LCWRT;
        run_args.base_d000_wrt = language_banks[0]-0xD000;
        run_args.base_e000_wrt = language_card[0]-0xE000;
    }

    run_args.softswitches |= (SS_LCSEC|SS_BANK2);
    run_args.softswitches &= ~SS_LCRAM;

    run_args.base_d000_rd = apple_ii_64k[0];
    run_args.base_e000_rd = apple_ii_64k[0];

    if (run_args.softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

static uint8_t lc_c082(void) {
    run_args.softswitches &= ~(SS_LCRAM|SS_LCWRT|SS_LCSEC);
    run_args.softswitches |= SS_BANK2;

    run_args.base_d000_rd = apple_ii_64k[0];
    run_args.base_e000_rd = apple_ii_64k[0];

    run_args.base_d000_wrt = 0;
    run_args.base_e000_wrt = 0;

    return floating_bus();
}

static uint8_t iie_c083(void) {
    if (run_args.softswitches & SS_LCSEC) {
        run_args.softswitches |= SS_LCWRT;
        run_args.base_d000_wrt = language_banks[0]-0xD000;
        run_args.base_e000_wrt = language_card[0]-0xE000;
    }

    run_args.softswitches |= (SS_LCSEC|SS_LCRAM|SS_BANK2);
    run_args.base_d000_rd = language_banks[0]-0xD000;
    run_args.base_e000_rd = language_card[0]-0xE000;

    if (run_args.softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

static uint8_t iie_c088(void) {
    run_args.softswitches |= SS_LCRAM;
    run_args.softswitches &= ~(SS_LCWRT|SS_LCSEC|SS_BANK2);

    run_args.base_d000_rd = language_banks[0]-0xC000;
    run_args.base_e000_rd = language_card[0]-0xE000;

    run_args.base_d000_wrt = 0;
    run_args.base_e000_wrt = 0;

    if (run_args.softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

static uint8_t iie_c089(void) {
    if (run_args.softswitches & SS_LCSEC) {
        run_args.softswitches |= SS_LCWRT;
        run_args.base_d000_wrt = language_banks[0]-0xC000;
        run_args.base_e000_wrt = language_card[0]-0xE000;
    }

    run_args.softswitches |= SS_LCSEC;
    run_args.softswitches &= ~(SS_LCRAM|SS_BANK2);

    run_args.base_d000_rd = apple_ii_64k[0];
    run_args.base_e000_rd = apple_ii_64k[0];

    if (run_args.softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

static uint8_t lc_c08a(void) {
    run_args.softswitches &= ~(SS_LCRAM|SS_LCWRT|SS_LCSEC|SS_BANK2);

    run_args.base_d000_rd = apple_ii_64k[0];
    run_args.base_e000_rd = apple_ii_64k[0];

    run_args.base_d000_wrt = 0;
    run_args.base_e000_wrt = 0;

    return floating_bus();
}

static uint8_t iie_c08b(void) {
    if (run_args.softswitches & SS_LCSEC) {
        run_args.softswitches |= SS_LCWRT;
        run_args.base_d000_wrt = language_banks[0]-0xC000;
        run_args.base_e000_wrt = language_card[0]-0xE000;
    }

    run_args.softswitches |= (SS_LCRAM|SS_LCSEC);
    run_args.softswitches &= ~SS_BANK2;

    run_args.base_d000_rd = language_banks[0]-0xC000;
    run_args.base_e000_rd = language_card[0]-0xE000;

    if (run_args.softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

// ----------------------------------------------------------------------------
// Misc //e softswitches and vm routines

static uint8_t iie_80store_off(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (!(run_args.softswitches & SS_80STORE)) {
        return 0x0;//floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches &= ~(SS_80STORE|SS_TEXTRD|SS_TEXTWRT|SS_HGRRD|SS_HGRWRT);

    run_args.base_textrd  = apple_ii_64k[0];
    run_args.base_textwrt = apple_ii_64k[0];
    run_args.base_hgrrd   = apple_ii_64k[0];
    run_args.base_hgrwrt  = apple_ii_64k[0];

    if (run_args.softswitches & SS_RAMRD) {
        run_args.softswitches |= (SS_TEXTRD|SS_HGRRD);
        run_args.base_textrd = apple_ii_64k[1];
        run_args.base_hgrrd  = apple_ii_64k[1];
    }

    if (run_args.softswitches & SS_RAMWRT) {
        run_args.softswitches |= (SS_TEXTWRT|SS_HGRWRT);
        run_args.base_textwrt = apple_ii_64k[1];
        run_args.base_hgrwrt  = apple_ii_64k[1];
    }

    if (run_args.softswitches & SS_PAGE2) {
        run_args.softswitches |= SS_SCREEN;
    }

    return 0x0;//floating_bus();
}

static uint8_t iie_80store_on(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (run_args.softswitches & SS_80STORE) {
        return 0x0;//floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches |= SS_80STORE;

    if (run_args.softswitches & SS_PAGE2) {
        run_args.softswitches |= (SS_TEXTRD|SS_TEXTWRT);
        run_args.base_textrd  = apple_ii_64k[1];
        run_args.base_textwrt = apple_ii_64k[1];
        if (run_args.softswitches & SS_HIRES) {
            run_args.softswitches |= (SS_HGRRD|SS_HGRWRT);
            run_args.base_hgrrd  = apple_ii_64k[1];
            run_args.base_hgrwrt = apple_ii_64k[1];
        }
    } else {
        run_args.softswitches &= ~(SS_TEXTRD|SS_TEXTWRT);
        run_args.base_textrd  = apple_ii_64k[0];
        run_args.base_textwrt = apple_ii_64k[0];
        if (run_args.softswitches & SS_HIRES) {
            run_args.softswitches &= ~(SS_HGRRD|SS_HGRWRT);
            run_args.base_hgrrd  = apple_ii_64k[0];
            run_args.base_hgrwrt = apple_ii_64k[0];
        }
    }

    run_args.softswitches &= ~SS_SCREEN;

    return 0x0;//floating_bus();
}

static uint8_t iie_ramrd_main(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (!(run_args.softswitches & SS_RAMRD)) {
        return 0x0;//floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches &= ~SS_RAMRD;
    run_args.base_ramrd = apple_ii_64k[0];

    if (run_args.softswitches & SS_80STORE) {
        if (!(run_args.softswitches & SS_HIRES)) {
            run_args.softswitches &= ~SS_HGRRD;
            run_args.base_hgrrd = apple_ii_64k[0];
        }
    } else {
        run_args.softswitches &= ~(SS_TEXTRD|SS_HGRRD);
        run_args.base_textrd = apple_ii_64k[0];
        run_args.base_hgrrd  = apple_ii_64k[0];
    }

    return 0x0;//floating_bus();
}

static uint8_t iie_ramrd_aux(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (run_args.softswitches & SS_RAMRD) {
        return 0x0;//floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches |= SS_RAMRD;
    run_args.base_ramrd = apple_ii_64k[1];

    if (run_args.softswitches & SS_80STORE) {
        if (!(run_args.softswitches & SS_HIRES)) {
            run_args.softswitches |= SS_HGRRD;
            run_args.base_hgrrd = apple_ii_64k[1];
        }
    } else {
        run_args.softswitches |= (SS_TEXTRD|SS_HGRRD);
        run_args.base_textrd = apple_ii_64k[1];
        run_args.base_hgrrd  = apple_ii_64k[1];
    }

    return 0x0;//floating_bus();
}

static uint8_t iie_ramwrt_main(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (!(run_args.softswitches & SS_RAMWRT)) {
        return 0x0;// floating_bus();
    }

    run_args.softswitches &= ~SS_RAMWRT;
    run_args.base_ramwrt = apple_ii_64k[0];

    if (run_args.softswitches & SS_80STORE) {
        if (!(run_args.softswitches & SS_HIRES)) {
            run_args.softswitches &= ~SS_HGRWRT;
            run_args.base_hgrwrt = apple_ii_64k[0];
        }
    } else {
        run_args.softswitches &= ~(SS_TEXTWRT|SS_HGRWRT);
        run_args.base_textwrt = apple_ii_64k[0];
        run_args.base_hgrwrt  = apple_ii_64k[0];
    }

    return 0x0;//floating_bus();
}

static uint8_t iie_ramwrt_aux(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (run_args.softswitches & SS_RAMWRT) {
        return 0x0;//floating_bus();
    }

    run_args.softswitches |= SS_RAMWRT;
    run_args.base_ramwrt = apple_ii_64k[1];

    if (run_args.softswitches & SS_80STORE) {
        if (!(run_args.softswitches & SS_HIRES)) {
            run_args.softswitches |= SS_HGRWRT;
            run_args.base_hgrwrt = apple_ii_64k[1];
        }
    } else {
        run_args.softswitches |= (SS_TEXTWRT|SS_HGRWRT);
        run_args.base_textwrt = apple_ii_64k[1];
        run_args.base_hgrwrt  = apple_ii_64k[1];
    }

    return 0x0;//floating_bus();
}

static uint8_t iie_altzp_main(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (!(run_args.softswitches & SS_ALTZP)) {
        /* NOTE : test if ALTZP already off - due to d000-bank issues it is *needed*, not just a shortcut */
        return 0x0;//floating_bus();
    }

    run_args.softswitches &= ~SS_ALTZP;
    run_args.base_stackzp = apple_ii_64k[0];

    if (run_args.softswitches & SS_LCRAM) {
        run_args.base_d000_rd -= 0x2000;
        run_args.base_e000_rd = language_card[0] - 0xE000;
    }

    if (run_args.softswitches & SS_LCWRT) {
        run_args.base_d000_wrt -= 0x2000;
        run_args.base_e000_wrt = language_card[0] - 0xE000;
    }

    return 0x0;//floating_bus();
}

static uint8_t iie_altzp_aux(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (run_args.softswitches & SS_ALTZP) {
        /* NOTE : test if ALTZP already on - due to d000-bank issues it is *needed*, not just a shortcut */
        return 0x0;//floating_bus();
    }

    run_args.softswitches |= SS_ALTZP;
    run_args.base_stackzp = apple_ii_64k[1];

    _lc_to_auxmem();

    return 0x0;//floating_bus();
}

static uint8_t iie_80col_off(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (!(run_args.softswitches & SS_80COL)) {
        return 0x0;//floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches &= ~SS_80COL;

    return 0x0;//floating_bus();
}

static uint8_t iie_80col_on(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (run_args.softswitches & SS_80COL) {
        return 0x0;//floating_bus();
    }

    video_setDirty(A2_DIRTY_FLAG);
    run_args.softswitches |= SS_80COL;

    return 0x0;//floating_bus();
}

static uint8_t iie_altchar_off(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (run_args.softswitches & SS_ALTCHAR) {
        video_setDirty(A2_DIRTY_FLAG);
        run_args.softswitches &= ~SS_ALTCHAR;
        display_loadFont(/*start:*/0x40, /*qty:*/0x40, /*data:*/ucase_glyphs, FONT_MODE_FLASH);
    }
    return 0x0;//floating_bus();
}

static uint8_t iie_altchar_on(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    if (!(run_args.softswitches & SS_ALTCHAR)) {
        video_setDirty(A2_DIRTY_FLAG);
        run_args.softswitches |= SS_ALTCHAR;
        display_loadFont(/*start:*/0x40, /*qty:*/0x20, /*data:*/mousetext_glyphs, FONT_MODE_MOUSETEXT);
        display_loadFont(/*start:*/0x60, /*qty:*/0x20, /*data:*/lcase_glyphs, FONT_MODE_INVERSE);
    }
    return 0x0;//floating_bus();
}

static uint8_t iie_dhires_on(void) {
    if (!(run_args.softswitches & SS_DHIRES)) {
        video_setDirty(A2_DIRTY_FLAG);
        run_args.softswitches |= SS_DHIRES;
    }
    return floating_bus();
}

static uint8_t iie_dhires_off(void) {
    if (run_args.softswitches & SS_DHIRES) {
        video_setDirty(A2_DIRTY_FLAG);
        run_args.softswitches &= ~SS_DHIRES;
    }
    return floating_bus();
}

static uint8_t iie_check_vbl(void) {
    bool isVBL = false;
    video_scannerAddress(&isVBL);
    uint8_t key = apple_ii_64k[0][0xC000];
    return (key & ~0x80) | (isVBL ? 0x00 : 0x80);
}

static uint8_t iie_c3rom_peripheral(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    run_args.softswitches &= ~SS_C3ROM;
    if (!(run_args.softswitches & SS_CXROM)) {
        run_args.base_c3rom = (void *)iie_read_peripheral_card;
    }
    return 0x0;
}

static uint8_t iie_c3rom_internal(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    run_args.softswitches |= SS_C3ROM;
    run_args.base_c3rom = apple_ii_64k[1];
    return 0x0;
}

static uint8_t iie_cxrom_peripheral(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    run_args.softswitches &= ~SS_CXROM;
    run_args.base_cxrom = (void *)iie_read_peripheral_card;
    run_args.base_c4rom = (void *)iie_read_peripheral_card;
    run_args.base_c5rom = (void *)iie_read_peripheral_card;
    if (!(run_args.softswitches & SS_C3ROM)) {
        run_args.base_c3rom = (void *)iie_read_peripheral_card;
    }
    return 0x0;
}

static uint8_t iie_cxrom_internal(void) {
    assert((run_args.cpu65_rw&MEM_WRITE_FLAG));
    run_args.softswitches |= SS_CXROM;
    run_args.base_cxrom = apple_ii_64k[1];
    run_args.base_c3rom = apple_ii_64k[1];
    run_args.base_c4rom = apple_ii_64k[1];
    run_args.base_c5rom = apple_ii_64k[1];
    return 0x0;
}

// ----------------------------------------------------------------------------
// C0XX softswitch handlers

GLUE_C_READ(read_softswitch)
{
    uint8_t sw = ea & 0xFF;

    //return cpu65_c0_r[sw](ea, b);

    // keyboard data and strobe (READ)
    if (sw >= 0x00 && sw < 0x10) {
        return apple_ii_64k[0][0xC000];
    }
    if (sw == 0x10) {
        return read_keyboard_strobe();
    }

    if (sw >= 0x11 && sw < 0x20) {
        // RDBNK2 switch
        if (sw == 0x11) {
            return (run_args.softswitches & SS_BANK2) SS_BANK2_SHIFT;
        }

        // RDLCRAM switch
        if (sw == 0x12) {
            return (run_args.softswitches & SS_LCRAM) SS_LCRAM_SHIFT;
        }

        // RAMRD switch
        if (sw == 0x13) {
            return (run_args.softswitches & SS_RAMRD) SS_RAMRD_SHIFT;
        }

        // RAMWRT switch
        if (sw == 0x14) {
            return (run_args.softswitches & SS_RAMWRT) SS_RAMWRT_SHIFT;
        }

        // SLOTCXROM switch
        if (sw == 0x15) {
            return (run_args.softswitches & SS_CXROM) SS_CXROM_SHIFT;
        }

        // ALTZP switch
        if (sw == 0x16) {
            return (run_args.softswitches & SS_ALTZP) SS_ALTZP_SHIFT;
        }

        // SLOTC3ROM switch
        if (sw == 0x17) {
            // reversed pattern!
            return (run_args.softswitches & SS_C3ROM) ? 0x00 : 0x80;
        }

        // 80STORE switch
        if (sw == 0x18) {
            return (run_args.softswitches & SS_80STORE) SS_80STORE_SHFT;
        }

        // RDVBLBAR switch
        if (sw == 0x19) {
            return iie_check_vbl();
        }

        // TEXT check switch
        if (sw == 0x1A) {
            return (run_args.softswitches & SS_TEXT) SS_TEXT_SHIFT;
        }

        // MIXED check switch
        if (sw == 0x1B) {
            return (run_args.softswitches & SS_MIXED) SS_MIXED_SHIFT;
        }

        // PAGE2 check switch
        if (sw == 0x1C) {
            return (run_args.softswitches & SS_PAGE2) SS_PAGE2_SHIFT;
        }

        // HIRES check switch
        if (sw == 0x1D) {
            return (run_args.softswitches & SS_HIRES) SS_HIRES_SHIFT;
        }

        // ALTCHAR switch
        if (sw == 0x1E) {
            return (run_args.softswitches & SS_ALTCHAR) SS_ALTCHAR_SHFT;
        }

        // 80COL switch
        if (sw == 0x1F) {
            return (run_args.softswitches & SS_80COL) SS_80COL_SHIFT;
        }
    }

    if (sw >= 0x30 && sw < 0x40) {
        return speaker_toggle();
    }

    // TEXT switch
    if (sw == 0x50) {
        return read_switch_graphics();
    }
    if (sw == 0x51) {
        return read_switch_text();
    }

    // MIXED switch
    if (sw == 0x52) {
        return read_switch_no_mixed();
    }
    if (sw == 0x53) {
        return read_switch_mixed();
    }

    // PAGE2 switch
    if (sw == 0x54) {
        return iie_page2_off();
    }
    if (sw == 0x55) {
        return iie_page2_on();
    }

    // HIRES switch
    if (sw == 0x56) {
        return iie_hires_off();
    }
    if (sw == 0x57) {
        return iie_hires_on();
    }

    // Annunciator
    if (sw >= 0x58 && sw < 0x5E) {
        return iie_annunciator(ea);
    }

    // DHIRES
    if (sw == 0x5E) {
        return iie_dhires_on();
    }
    if (sw == 0x5F) {
        return iie_dhires_off();
    }

    // game I/O switches
    if (sw == 0x61 || sw == 0x69) {
        return read_button0();
    }
    if (sw == 0x62 || sw == 0x6A) {
        return read_button1();
    }
    if (sw == 0x63 || sw == 0x6B) {
        return read_button2();
    }
    if (sw == 0x64 || sw == 0x6C) {
        return read_gc0();
    }
    if (sw == 0x65 || sw == 0x6D) {
        return read_gc1();
    }

    if (sw == 0x66) {
        return iie_read_gc2();
    }
    if (sw == 0x67) {
        return iie_read_gc3();
    }

    if (sw >= 0x70 && sw < 0x7E) {
        return read_gc_strobe();
    }

    // IOUDIS switch & read_gc_strobe
    // HACK FIXME TODO : double-check this stuff against AWin...
    if (sw == 0x7E) {
        read_gc_strobe();
        return (run_args.softswitches & SS_IOUDIS) SS_IOUDIS_SHIFT;
    }
    if (sw == 0x7F) {
        read_gc_strobe(); // HACK FIXME : is this correct?
        return (run_args.softswitches & SS_DHIRES) SS_DHIRES_SHIFT;
    }

    // language card softswitches
    if (sw == 0x80 || sw == 0x84) {
        return iie_c080();
    }
    if (sw == 0x81 || sw == 0x85) {
        return iie_c081();
    }
    if (sw == 0x82 || sw == 0x86) {
        return lc_c082();
    }
    if (sw == 0x83 || sw == 0x87) {
        return iie_c083();
    }

    if (sw == 0x88 || sw == 0x8C) {
        return iie_c088();
    }
    if (sw == 0x89 || sw == 0x8D) {
        return iie_c089();
    }
    if (sw == 0x8A || sw == 0x8E) {
        return lc_c08a();
    }
    if (sw == 0x8B || sw == 0x8F) {
        return iie_c08b();
    }

    if (sw >= 0xE0 && sw < 0xF0) {
        // disk softswitches
        // 0xC0Xi : X = slot 0x6 + 0x8 == 0xE
        return disk6_ioRead(ea);
    }

    return floating_bus();
}

GLUE_C_WRITE(write_softswitch)
{
    uint8_t sw = ea & 0xFF;

    if (sw == 0x00) {
        iie_80store_off();
    }
    if (sw == 0x01) {
        iie_80store_on();
    }

    // RAMRD switch
    if (sw == 0x02) {
        iie_ramrd_main();
    }
    if (sw == 0x03) {
        iie_ramrd_aux();
    }

    // RAMWRT switch
    if (sw == 0x04) {
        iie_ramwrt_main();
    }
    if (sw == 0x05) {
        iie_ramwrt_aux();
    }

    // SLOTCXROM switch
    if (sw == 0x06) {
        iie_cxrom_peripheral();
    }
    if (sw == 0x07) {
        iie_cxrom_internal();
    }

    // ALTZP switch
    if (sw == 0x08) {
        iie_altzp_main();
    }
    if (sw == 0x09) {
        iie_altzp_aux();
    }

    // SLOTC3ROM switch
    if (sw == 0x0A) {
        iie_c3rom_internal();
    }
    if (sw == 0x0B) {
        iie_c3rom_peripheral();
    }

    // 80COL switch
    if (sw == 0x0C) {
        iie_80col_off();
    }
    if (sw == 0x0D) {
        iie_80col_on();
    }

    // ALTCHAR switch
    if (sw == 0x0E) {
        iie_altchar_off();
    }
    if (sw == 0x0F) {
        iie_altchar_on();
    }

    if (sw >= 0x10 && sw < 0x20) {
        read_keyboard_strobe();
    }

    if (sw >= 0x30 && sw < 0x40) {
        speaker_toggle();
    }

    // TEXT switch
    if (sw == 0x50) {
        read_switch_graphics();
    }
    if (sw == 0x51) {
        read_switch_text();
    }

    // MIXED switch
    if (sw == 0x52) {
        read_switch_no_mixed();
    }
    if (sw == 0x53) {
        read_switch_mixed();
    }

    // PAGE2 switch
    if (sw == 0x54) {
        iie_page2_off();
    }
    if (sw == 0x55) {
        iie_page2_on();
    }

    // HIRES switch
    if (sw == 0x56) {
        iie_hires_off();
    }
    if (sw == 0x57) {
        iie_hires_on();
    }

    // Annunciator
    if (sw >= 0x58 && sw < 0x5E) {
        iie_annunciator(ea);
    }

    // DHIRES
    if (sw == 0x5E) {
        iie_dhires_on();
    }
    if (sw == 0x5F) {
        iie_dhires_off();
    }

    if (sw >= 0x70 && sw < 0x7E) {
        read_gc_strobe();
    }

    // IOUDIS switch & read_gc_strobe
    // HACK FIXME TODO : double-check this stuff against AWin...
    if (sw == 0x7E) {
        run_args.softswitches |= SS_IOUDIS;
        read_gc_strobe();
    }
    if (sw == 0x7F) {
        run_args.softswitches &= ~SS_IOUDIS;
        read_gc_strobe();
    }

    // language card softswitches
    if (sw == 0x80 || sw == 0x84) {
        iie_c080();
    }
    if (sw == 0x81 || sw == 0x85) {
        iie_c081();
    }
    if (sw == 0x82 || sw == 0x86) {
        lc_c082();
    }
    if (sw == 0x83 || sw == 0x87) {
        iie_c083();
    }

    if (sw == 0x88 || sw == 0x8C) {
        iie_c088();
    }
    if (sw == 0x89 || sw == 0x8D) {
        iie_c089();
    }
    if (sw == 0x8A || sw == 0x8E) {
        lc_c08a();
    }
    if (sw == 0x8B || sw == 0x8F) {
        iie_c08b();
    }

    if (sw >= 0xE0 && sw < 0xF0) {
        disk6_ioWrite(ea, b);
    }
}

// ----------------------------------------------------------------------------
// //e expansion ROM

GLUE_C_READ(iie_read_slot_expansion)
{
    // HACK TODO FIXME : how does the expansion slot get referenced?  Need to handle other ROMs that might use this area
    // ... Also Need moar tests ...
    if (ea == 0xCFFF) {
        // disable expansion ROM
        return floating_bus();
    }

    return apple_ii_64k[1][ea];
}

GLUE_C_WRITE(debug_illegal_bcd)
{
    LOG("Illegal/undefined BCD operation encountered, debug break on c_debug_illegal_bcd to debug...");
}

// ----------------------------------------------------------------------------

static void _initialize_iie_switches(void) {

    run_args.base_stackzp = apple_ii_64k[0];
    run_args.base_d000_rd = apple_ii_64k[0];
    run_args.base_d000_wrt = language_banks[0] - 0xD000;
    run_args.base_e000_rd = apple_ii_64k[0];
    run_args.base_e000_wrt = language_card[0] - 0xE000;

    run_args.base_ramrd = apple_ii_64k[0];
    run_args.base_ramwrt = apple_ii_64k[0];
    run_args.base_textrd = apple_ii_64k[0];
    run_args.base_textwrt = apple_ii_64k[0];
    run_args.base_hgrrd = apple_ii_64k[0];
    run_args.base_hgrwrt= apple_ii_64k[0];

    run_args.base_c3rom = (void *)iie_read_peripheral_card; // c3rom internal
    run_args.base_c4rom = (void *)iie_read_peripheral_card; // c4rom internal
    run_args.base_c5rom = (void *)iie_read_peripheral_card; // c5rom internal
    run_args.base_cxrom = (void *)iie_read_peripheral_card; // cxrom peripheral

    run_args.softswitches = SS_TEXT | SS_BANK2;
}

static void _initialize_font(void) {
    display_loadFont(/*start:*/0x00, /*qty:*/0x40, /*data:*/ucase_glyphs, FONT_MODE_INVERSE);
    display_loadFont(/*start:*/0x40, /*qty:*/0x40, /*data:*/ucase_glyphs, FONT_MODE_FLASH);
    display_loadFont(/*start:*/0x80, /*qty:*/0x40, /*data:*/ucase_glyphs, FONT_MODE_NORMAL);
    display_loadFont(/*start:*/0xC0, /*qty:*/0x20, /*data:*/ucase_glyphs, FONT_MODE_NORMAL);
    display_loadFont(/*start:*/0xE0, /*qty:*/0x20, /*data:*/lcase_glyphs, FONT_MODE_NORMAL);
}

static void _initialize_apple_ii_memory(void) {
    for (unsigned int i = 0; i < 0x10000; i++) {
        apple_ii_64k[0][i] = 0;
        apple_ii_64k[1][i] = 0;
    }

    // Stripe words of main memory on machine reset ...
    // NOTE: cracked version of Joust will lock up without this
    for (unsigned int i = 0; i < 0xC000;) {
        apple_ii_64k[0][i++] = 0xFF;
        apple_ii_64k[0][i++] = 0xFF;
        i += 2;
    }

#if !TESTING && !CPU_TRACING && !VIDEO_TRACING
    // certain memory locations randomized at cold-boot ...
    for (uint16_t addr = 0x0000; addr < 0xC000; addr += 0x200)
    {
        uint16_t word;

        word = random();
        apple_ii_64k[0][addr + 0x28] = (word >> 0) & 0xFF;
        apple_ii_64k[0][addr + 0x29] = (word >> 8) & 0xFF;

        word = random();
        apple_ii_64k[0][addr + 0x68] = (word >> 0) & 0xFF;
        apple_ii_64k[0][addr + 0x69] = (word >> 8) & 0xFF;
    }

    // memory initialization workarounds ...
    {
        // https://github.com/AppleWin/AppleWin/issues/206
        // work around cold-booting bug in "Pooyan" which expects RNDL and RNDH to be non-zero
        // "Dung Beetles, Ms. PacMan, Pooyan, Star Cruiser, Star Thief, Invas. Force.dsk"
        uint16_t word = (uint16_t)random();
        apple_ii_64k[0][0x4E] = 0x20 | ((word >> 0) & 0xFF);
        apple_ii_64k[0][0x4F] = 0x20 | ((word >> 8) & 0xFF);
    }
#endif

    for (unsigned int i = 0; i < 8192; i++) {
        language_card[0][i] = language_card[1][i] = 0;
    }

    for (unsigned int i = 0; i < 8192; i++) {
        language_banks[0][i] = language_banks[1][i] = 0;
    }

    // load the rom from 0xC000, slot rom main, internal rom aux

    for (unsigned int i = 0xC000; i < 0x10000; i++) {
        apple_ii_64k[0][i] = apple_iie_rom[i - 0xC000];
        apple_ii_64k[1][i] = apple_iie_rom[i - 0x8000];
    }

    for (unsigned int i = 0; i < 0x1000; i++) {
        language_banks[0][i] = apple_iie_rom[i + 0x1000];
        language_banks[1][i] = apple_iie_rom[i + 0x5000];
    }

    for (unsigned int i = 0; i < 0x2000; i++) {
        language_card[0][i] = apple_iie_rom[i + 0x2000];
        language_card[1][i] = apple_iie_rom[i + 0x6000];
    }

    apple_ii_64k[0][0xC000] = 0x00;
    apple_ii_64k[1][0xC000] = 0x00;
}

static void _initialize_tables(void) {

    for (unsigned int i = 0; i < 0x100; i++) {
        cpu65_vmem_r[i] = iie_read_ram_default;
        cpu65_vmem_w[i] = iie_write_ram_default;
    }

    for (unsigned int i = 0xC0; i < 0xD0; i++) {
        cpu65_vmem_w[i] = write_ram_nop;
    }

    // language card read/write area

    for (unsigned int i = 0xD0; i < 0xE0; i++) {
        cpu65_vmem_w[i] = write_ram_bank;
        cpu65_vmem_r[i] = read_ram_bank;
    }

    for (unsigned int i = 0xE0; i < 0x100; i++) {
        cpu65_vmem_w[i] = write_ram_lc;
        cpu65_vmem_r[i] = read_ram_lc;
    }

    // done common initialization

    // initialize zero-page, //e specific
    cpu65_vmem_r[0] = iie_read_ram_zpage_and_stack;
    cpu65_vmem_w[0] = iie_write_ram_zpage_and_stack;
    cpu65_vmem_r[1] = iie_read_ram_zpage_and_stack;
    cpu65_vmem_w[1] = iie_write_ram_zpage_and_stack;

    // initialize first text & hires page, which are specially bank switched
    cpu65_vmem_r[4] = iie_read_ram_text_page0;
    cpu65_vmem_r[5] = iie_read_ram_text_page0;
    cpu65_vmem_r[6] = iie_read_ram_text_page0;
    cpu65_vmem_r[7] = iie_read_ram_text_page0;

    for (unsigned int i = 0x20; i < 0x40; i++) {
        cpu65_vmem_r[i] = iie_read_ram_hires_page0;
    }

    // initialize text/lores & hires graphics routines
    for (unsigned int y = 0; y < TEXT_ROWS; y++) {
        uint16_t row = display_getVideoLineOffset(y);
        for (unsigned int x = 0; x < TEXT_COLS; x++) {
            unsigned int idx = row + x;

            // NOTE : we are doing too much work here calculating the lo_byte positions, but eh, this is just one-time setup :P

            // text/lores pages
            if (y < 20) {
                cpu65_vmem_w[(idx+0x400)>>8] = video__write_2e_text0;
                cpu65_vmem_w[(idx+0x800)>>8] = video__write_2e_text1;
            } else {
                cpu65_vmem_w[(idx+0x400)>>8] = video__write_2e_text0_mixed;
                cpu65_vmem_w[(idx+0x800)>>8] = video__write_2e_text1_mixed;
            }

            // hires/dhires pages
            for (unsigned int i = 0; i < 8; i++) {
                idx = row + (0x400*i) + x;
                if (y < 20) {
                    cpu65_vmem_w[(idx+0x2000)>>8] = video__write_2e_hgr0;
                    cpu65_vmem_w[(idx+0x4000)>>8] = video__write_2e_hgr1;
                } else {
                    cpu65_vmem_w[(idx+0x2000)>>8] = video__write_2e_hgr0_mixed;
                    cpu65_vmem_w[(idx+0x4000)>>8] = video__write_2e_hgr1_mixed;
                }
            }
        }
    }

    // softswich rom
    cpu65_vmem_r[0xC0] = read_softswitch;
    cpu65_vmem_w[0xC0] = write_softswitch;

    // slot i/o area
    cpu65_vmem_r[0xC1] = iie_read_slotx; // slots 1
    cpu65_vmem_r[0xC2] = iie_read_slotx; // slots 2
    cpu65_vmem_r[0xC3] = iie_read_slot3; // slot 3 (80col)
    cpu65_vmem_r[0xC4] = iie_read_slot4; // slot 4 - MB or Phasor
    cpu65_vmem_r[0xC5] = iie_read_slot5; // slot 5 - MB #2
    cpu65_vmem_r[0xC6] = iie_read_slotx; // slots 6
    cpu65_vmem_r[0xC7] = iie_read_slotx; // slots 7

    for (unsigned int i = 0xC8; i < 0xD0; i++) {
        cpu65_vmem_r[i] = iie_read_slot_expansion;
    }

    // Peripheral card slot initializations ...

    // HACK TODO FIXME : this needs to be tied to the UI/configuration system (once we have more/conflicting options)

// FIXME TODO : implement pluggable peripheral API
//if (mockingboard_inserted) {
    mb_io_initialize(4, 5); /* Mockingboard(s) and/or Phasor in slots 4 & 5 */
//}
}

// ----------------------------------------------------------------------------

void vm_initialize(void) {
    _initialize_font();
    _initialize_apple_ii_memory();
    _initialize_tables();
    disk6_init();
    _initialize_iie_switches();
    c_joystick_reset();
}

bool vm_saveState(StateHelper_s *helper) {
    bool saved = false;
    int fd = helper->fd;

    do {
        uint8_t serialized[8] = { 0 };

        serialized[0] = (uint8_t)((run_args.softswitches & 0xFF000000) >> 24);
        serialized[1] = (uint8_t)((run_args.softswitches & 0xFF0000  ) >> 16);
        serialized[2] = (uint8_t)((run_args.softswitches & 0xFF00    ) >>  8);
        serialized[3] = (uint8_t)((run_args.softswitches & 0xFF      ) >>  0);
        if (!helper->save(fd, serialized, sizeof(run_args.softswitches))) {
            break;
        }

        // save main/aux memory state
        if (!helper->save(fd, apple_ii_64k[0], sizeof(apple_ii_64k))) {
            break;
        }

        // save language card
        if (!helper->save(fd, language_card[0], sizeof(language_card))) {
            break;
        }

        // save language banks
        if (!helper->save(fd, language_banks[0], sizeof(language_banks))) {
            break;
        }

        // save offsets
        serialized[0] = 0x0;
        serialized[1] = 0x1;
        serialized[2] = 0x2;
        serialized[3] = 0x3;
        serialized[4] = 0x4;
        serialized[5] = 0x5;
        if (!helper->save(fd, (run_args.base_ramrd == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        if (!helper->save(fd, (run_args.base_ramwrt == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        if (!helper->save(fd, (run_args.base_textrd == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        if (!helper->save(fd, (run_args.base_textwrt == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        if (!helper->save(fd, (run_args.base_hgrrd == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        if (!helper->save(fd, (run_args.base_hgrwrt == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        if (!helper->save(fd, (run_args.base_stackzp == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        if (!helper->save(fd, (run_args.base_c3rom == (void *)iie_read_peripheral_card) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }

        if (!helper->save(fd, (run_args.base_cxrom == (void *)iie_read_peripheral_card) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }

        if (run_args.base_d000_rd == apple_ii_64k[0]) {
            if (!helper->save(fd, &serialized[0], 1)) { // base_d000_rd --> //e ROM
                break;
            }
        } else if (run_args.base_d000_rd == language_banks[0] - 0xD000) {
            if (!helper->save(fd, &serialized[2], 1)) { // base_d000_rd --> main LC mem
                break;
            }
        } else if (run_args.base_d000_rd == language_banks[0] - 0xC000) {
            if (!helper->save(fd, &serialized[3], 1)) { // base_d000_rd --> main LC mem
                break;
            }
        } else if (run_args.base_d000_rd == language_banks[1] - 0xD000) {
            if (!helper->save(fd, &serialized[4], 1)) { // base_d000_rd --> aux  LC mem
                break;
            }
        } else if (run_args.base_d000_rd == language_banks[1] - 0xC000) {
            if (!helper->save(fd, &serialized[5], 1)) { // base_d000_rd --> aux  LC mem
                break;
            }
        } else {
            LOG("OOPS ... language_banks[0] == %p base_d000_rd == %p", language_banks[0], run_args.base_d000_rd);
            assert(false);
        }

        if (run_args.base_d000_wrt == 0) {
            if (!helper->save(fd, &serialized[0], 1)) { // base_d000_wrt --> no write
                break;
            }
        } else if (run_args.base_d000_wrt == language_banks[0] - 0xD000) {
            if (!helper->save(fd, &serialized[2], 1)) { // base_d000_wrt --> main LC mem
                break;
            }
        } else if (run_args.base_d000_wrt == language_banks[0] - 0xC000) {
            if (!helper->save(fd, &serialized[3], 1)) { // base_d000_wrt --> main LC mem
                break;
            }
        } else if (run_args.base_d000_wrt == language_banks[1] - 0xD000) {
            if (!helper->save(fd, &serialized[4], 1)) { // base_d000_wrt --> aux  LC mem
                break;
            }
        } else if (run_args.base_d000_wrt == language_banks[1] - 0xC000) {
            if (!helper->save(fd, &serialized[5], 1)) { // base_d000_wrt --> aux  LC mem
                break;
            }
        } else {
            LOG("OOPS ... language_banks[0] == %p run_args.base_d000_wrt == %p", language_banks[0], run_args.base_d000_wrt);
            assert(false);
        }

        if (run_args.base_e000_rd == apple_ii_64k[0]) {
            if (!helper->save(fd, &serialized[0], 1)) { // base_e000_rd --> //e ROM
                break;
            }
        } else if (run_args.base_e000_rd == language_card[0] - 0xE000) {
            if (!helper->save(fd, &serialized[2], 1)) { // base_e000_rd --> main LC mem
                break;
            }
        } else if (run_args.base_e000_rd == language_card[0] - 0xC000) {
            if (!helper->save(fd, &serialized[3], 1)) { // base_e000_rd --> aux  LC mem
                break;
            }
        } else {
            LOG("OOPS ... language_card[0] == %p run_args.base_e000_rd == %p", language_card[0], run_args.base_e000_rd);
            assert(false);
        }

        if (run_args.base_e000_wrt == 0) {
            if (!helper->save(fd, &serialized[0], 1)) { // base_e000_wrt --> no write
                break;
            }
        } else if (run_args.base_e000_wrt == language_card[0] - 0xE000) {
            if (!helper->save(fd, &serialized[2], 1)) { // base_e000_wrt --> main LC mem
                break;
            }
        } else if (run_args.base_e000_wrt == language_card[0] - 0xC000) {
            if (!helper->save(fd, &serialized[3], 1)) { // base_e000_wrt --> aux  LC mem
                break;
            }
        } else {
            LOG("OOPS ... language_card[0] == %p run_args.base_e000_wrt == %p", language_card[0], run_args.base_e000_wrt);
            assert(false);
        }

        saved = true;
    } while (0);

    return saved;
}

bool vm_loadState(StateHelper_s *helper) {
    bool loaded = false;
    int fd = helper->fd;

    do {

        uint8_t serialized[4] = { 0 };

        if (!helper->load(fd, serialized, sizeof(uint32_t))) {
            break;
        }
        run_args.softswitches  = (uint32_t)(serialized[0] << 24);
        run_args.softswitches |= (uint32_t)(serialized[1] << 16);
        run_args.softswitches |= (uint32_t)(serialized[2] <<  8);
        run_args.softswitches |= (uint32_t)(serialized[3] <<  0);

        // load main/aux memory state
        if (!helper->load(fd, apple_ii_64k[0], sizeof(apple_ii_64k))) {
            break;
        }

        // load language card
        if (!helper->load(fd, language_card[0], sizeof(language_card))) {
            break;
        }

        // load language banks
        if (!helper->load(fd, language_banks[0], sizeof(language_banks))) {
            break;
        }

        // load offsets
        uint8_t state = 0x0;
        if (!helper->load(fd, &state, 1)) {
            break;
        }
        run_args.base_ramrd = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        run_args.base_ramwrt = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        run_args.base_textrd = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        run_args.base_textwrt = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        run_args.base_hgrrd = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        run_args.base_hgrwrt = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        run_args.base_stackzp = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        run_args.base_c3rom = state == 0x0 ? (void *)iie_read_peripheral_card : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        if (state == 0) {
            run_args.base_cxrom = (void *)iie_read_peripheral_card;
            run_args.base_c4rom = (void *)iie_read_peripheral_card;
            run_args.base_c5rom = (void *)iie_read_peripheral_card;
        } else {
            run_args.base_cxrom = apple_ii_64k[1];
            run_args.base_c4rom = apple_ii_64k[1];
            run_args.base_c5rom = apple_ii_64k[1];
        }

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        switch (state) {
            case 0:
                run_args.base_d000_rd = apple_ii_64k[0];
                break;
            case 2:
                run_args.base_d000_rd = language_banks[0] - 0xD000;
                break;
            case 3:
                run_args.base_d000_rd = language_banks[0] - 0xC000;
                break;
            case 4:
                run_args.base_d000_rd = language_banks[1] - 0xD000;
                break;
            case 5:
                run_args.base_d000_rd = language_banks[1] - 0xC000;
                break;
            default:
                LOG("Unknown state base_d000_rd %02x", state);
                assert(false);
                break;
        }

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        switch (state) {
            case 0:
                run_args.base_d000_wrt = 0;
                break;
            case 2:
                run_args.base_d000_wrt = language_banks[0] - 0xD000;
                break;
            case 3:
                run_args.base_d000_wrt = language_banks[0] - 0xC000;
                break;
            case 4:
                run_args.base_d000_wrt = language_banks[1] - 0xD000;
                break;
            case 5:
                run_args.base_d000_wrt = language_banks[1] - 0xC000;
                break;
            default:
                LOG("Unknown state base_d000_wrt %02x", state);
                assert(false);
                break;
        }

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        switch (state) {
            case 0:
                run_args.base_e000_rd = apple_ii_64k[0];
                break;
            case 2:
                run_args.base_e000_rd = language_card[0] - 0xE000;
                break;
            case 3:
                run_args.base_e000_rd = language_card[0] - 0xC000;
                break;
            default:
                LOG("Unknown state base_e000_rd %02x", state);
                assert(false);
                break;
        }

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        switch (state) {
            case 0:
                run_args.base_e000_wrt = 0;
                break;
            case 2:
                run_args.base_e000_wrt = language_card[0] - 0xE000;
                break;
            case 3:
                run_args.base_e000_wrt = language_card[0] - 0xC000;
                break;
            default:
                LOG("Unknown state base_e000_wrt %02x", state);
                assert(false);
                break;
        }
        LOG("LOAD run_args.base_e000_wrt = %d", state);

        loaded = true;
    } while (0);

    return loaded;
}

void vm_printSoftwitches(FILE *fp, bool output_mem, bool output_pseudo) {

    fprintf(fp, "[");
    if (run_args.softswitches & SS_TEXT) {
        fprintf(fp, " TEXT");
    }
    if (run_args.softswitches & SS_MIXED) {
        fprintf(fp, " MIXED");
    }
    if (run_args.softswitches & SS_HIRES) {
        fprintf(fp, " HIRES");
    }
    if (run_args.softswitches & SS_PAGE2) {
        fprintf(fp, " PAGE2");
    }
    if (run_args.softswitches & SS_80STORE) {
        fprintf(fp, " 80STORE");
    }
    if (run_args.softswitches & SS_80COL) {
        fprintf(fp, " 80COL");
    }
    if (run_args.softswitches & SS_DHIRES) {
        fprintf(fp, " DHIRES");
    }
    if (run_args.softswitches & SS_ALTCHAR) {
        fprintf(fp, " ALTCHAR");
    }

    if (output_mem) {
        if (run_args.softswitches & SS_BANK2) {
            fprintf(fp, " BANK2");
        }
        if (run_args.softswitches & SS_LCRAM) {
            fprintf(fp, " LCRAM");
        }
        if (run_args.softswitches & SS_RAMRD) {
            fprintf(fp, " RAMRD");
        }
        if (run_args.softswitches & SS_RAMWRT) {
            fprintf(fp, " RAMWRT");
        }
        if (run_args.softswitches & SS_ALTZP) {
            fprintf(fp, " ALTZP");
        }
        if (run_args.softswitches & SS_IOUDIS) {
            fprintf(fp, " IOUDIS");
        }
        if (run_args.softswitches & SS_CXROM) {
            fprintf(fp, " CXROM");
        }
        if (run_args.softswitches & SS_C3ROM) {
            fprintf(fp, " C3ROM");
        }
    }

    if (output_pseudo) {
        // pseudo #1
        if (run_args.softswitches & SS_LCSEC) {
            fprintf(fp, " SS_LCSEC");
        }
        if (run_args.softswitches & SS_LCWRT) {
            fprintf(fp, " SS_LCWRT");
        }

        // pseudo #2
        if (run_args.softswitches & SS_SCREEN) {
            fprintf(fp, " SS_SCREEN");
        }
        if (run_args.softswitches & SS_TEXTRD) {
            fprintf(fp, " SS_TEXTRD");
        }
        if (run_args.softswitches & SS_TEXTWRT) {
            fprintf(fp, " SS_TEXTWRT");
        }
        if (run_args.softswitches & SS_HGRRD) {
            fprintf(fp, " SS_HGRRD");
        }
        if (run_args.softswitches & SS_HGRWRT) {
            fprintf(fp, " SS_HGRWRT");
        }
    }
    fprintf(fp, " ]");
}

#if VM_TRACING
void vm_trace_begin(const char *vm_file) {
    if (vm_file) {
        if (test_vm_fp) {
            vm_trace_end();
        }
        test_vm_fp = fopen(vm_file, "w");
    }
}

void vm_trace_end(void) {
    if (test_vm_fp) {
        fflush(test_vm_fp);
        fclose(test_vm_fp);
        test_vm_fp = NULL;
    }
}

void vm_trace_toggle(const char *vm_file) {
    if (test_vm_fp) {
        vm_trace_end();
    } else {
        vm_trace_begin(vm_file);
    }
}

void vm_trace_ignore(vm_trace_range_t range) {
    // TODO ...
}

bool vm_trace_is_ignored(uint16_t ea) {
    if ((ea < 0xC000) || (ea > 0xCFFF)) {
        return true;
    }
    return false;
}

#endif
