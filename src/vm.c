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

uint32_t softswitches = 0x0;

const uint8_t *base_vmem = apple_ii_64k[0];
uint8_t *base_ramrd = NULL;
uint8_t *base_ramwrt = NULL;
uint8_t *base_textrd = NULL;
uint8_t *base_textwrt = NULL;
uint8_t *base_hgrrd = NULL;
uint8_t *base_hgrwrt = NULL;

uint8_t *base_stackzp = NULL;
uint8_t *base_d000_rd = NULL;
uint8_t *base_e000_rd = NULL;
uint8_t *base_d000_wrt = NULL;
uint8_t *base_e000_wrt = NULL;

uint8_t *base_c3rom = NULL;
uint8_t *base_c4rom = NULL;
uint8_t *base_c5rom = NULL;
uint8_t *base_cxrom = NULL;

// joystick timer values
int gc_cycles_timer_0 = 0;
int gc_cycles_timer_1 = 0;

#if VM_TRACING
FILE *test_vm_fp = NULL;

typedef struct vm_trace_range_t {
    uint16_t lo;
    uint16_t hi;
} vm_trace_range_t;
#endif

// ----------------------------------------------------------------------------

GLUE_BANK_READ(iie_read_ram_default,base_ramrd);
GLUE_BANK_WRITE(iie_write_ram_default,base_ramwrt);

GLUE_BANK_READ(read_ram_bank,base_d000_rd);
GLUE_BANK_MAYBEWRITE(write_ram_bank,base_d000_wrt);

GLUE_BANK_READ(read_ram_lc,base_e000_rd);
GLUE_BANK_MAYBEWRITE(write_ram_lc,base_e000_wrt);

GLUE_BANK_READ(iie_read_ram_text_page0,base_textrd);
GLUE_BANK_WRITE(iie_write_screen_hole_text_page0,base_textwrt);

GLUE_BANK_READ(iie_read_ram_hires_page0,base_hgrrd);
GLUE_BANK_WRITE(iie_write_screen_hole_hires_page0,base_hgrwrt);

GLUE_BANK_READ(iie_read_ram_zpage_and_stack,base_stackzp);
GLUE_BANK_WRITE(iie_write_ram_zpage_and_stack,base_stackzp);

GLUE_BANK_MAYBE_READ_C3(iie_read_slot3,base_c3rom);
GLUE_BANK_MAYBE_READ_CX(iie_read_slot4,base_c4rom);
GLUE_BANK_MAYBE_READ_CX(iie_read_slot5,base_c5rom);
GLUE_BANK_MAYBE_READ_CX(iie_read_slotx,base_cxrom);

GLUE_EXTERN_C_READ(speaker_toggle);

GLUE_C_READ(ram_nop)
{
    return (cpu65_rw&MEM_WRITE_FLAG) ? 0x0 : floating_bus();
}

GLUE_C_READ(read_unmapped_softswitch)
{
    return c_ram_nop(ea);
}

GLUE_C_WRITE(write_unmapped_softswitch)
{
    // ...
}

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

GLUE_C_READ(read_keyboard)
{
    uint8_t b = apple_ii_64k[0][0xC000];
#if INTERFACE_TOUCH
    // touch interface is expected to rate limit this callback by unregistering/NULLifying
    void (*readCallback)(void) = keydriver_keyboardReadCallback;
    if (readCallback) {
        readCallback();
    }
#endif
    return b;
}

GLUE_C_READ(read_keyboard_strobe)
{
    apple_ii_64k[0][0xC000] &= 0x7F;
    apple_ii_64k[1][0xC000] &= 0x7F;
    return apple_ii_64k[0][0xC000];
}

// ----------------------------------------------------------------------------
// graphics softswitches

GLUE_C_READ(iie_page2_off)
{
    if (!(softswitches & SS_PAGE2)) {
        return floating_bus();
    }

    softswitches &= ~(SS_PAGE2|SS_SCREEN);

    if (softswitches & SS_80STORE) {
        softswitches &= ~(SS_TEXTRD|SS_TEXTWRT);
        base_textrd  = apple_ii_64k[0];
        base_textwrt = apple_ii_64k[0];
        if (softswitches & SS_HIRES) {
            softswitches &= ~(SS_HGRRD|SS_HGRWRT);
            base_hgrrd  = apple_ii_64k[0];
            base_hgrwrt = apple_ii_64k[0];
        }
    }

    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_page2_on)
{
    if (softswitches & SS_PAGE2) {
        return floating_bus();
    }

    softswitches |= SS_PAGE2;

    if (softswitches & SS_80STORE) {
        softswitches |= (SS_TEXTRD|SS_TEXTWRT);
        base_textrd  = apple_ii_64k[1];
        base_textwrt = apple_ii_64k[1];
        if (softswitches & SS_HIRES) {
            softswitches |= (SS_HGRRD|SS_HGRWRT);
            base_hgrrd  = apple_ii_64k[1];
            base_hgrwrt = apple_ii_64k[1];
        }
    } else {
        softswitches |= SS_SCREEN;
    }

    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_check_page2)
{
    return (softswitches & SS_PAGE2) ? 0x80 : 0x00;
}

GLUE_C_READ(read_switch_graphics)
{
    if (softswitches & SS_TEXT) {
        softswitches &= ~SS_TEXT;
        video_setDirty(A2_DIRTY_FLAG);
    }
    return floating_bus();
}

GLUE_C_READ(read_switch_text)
{
    if (!(softswitches & SS_TEXT)) {
        softswitches |= SS_TEXT;
        video_setDirty(A2_DIRTY_FLAG);
    }
    return floating_bus();
}

GLUE_C_READ(iie_check_text)
{
    return (softswitches & SS_TEXT) ? 0x80 : 0x00;
}

GLUE_C_READ(read_switch_no_mixed)
{
    if (softswitches & SS_MIXED) {
        softswitches &= ~SS_MIXED;
        video_setDirty(A2_DIRTY_FLAG);
    }
    return floating_bus();
}

GLUE_C_READ(read_switch_mixed)
{
    if (!(softswitches & SS_MIXED)) {
        softswitches |= SS_MIXED;
        video_setDirty(A2_DIRTY_FLAG);
    }
    return floating_bus();
}

GLUE_C_READ(iie_check_mixed)
{
    return (softswitches & SS_MIXED) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_annunciator)
{
    if ((ea >= 0xC058) && (ea <= 0xC05B)) {
        // TODO: alternate joystick management?
    }
    return (cpu65_rw&MEM_WRITE_FLAG) ? 0x0 : floating_bus();
}

GLUE_C_READ(iie_hires_off)
{
    if (!(softswitches & SS_HIRES)) {
        return floating_bus();
    }

    softswitches &= ~(SS_HIRES|SS_HGRRD|SS_HGRWRT);
    base_hgrrd  = apple_ii_64k[0];
    base_hgrwrt = apple_ii_64k[0];

    if (softswitches & SS_RAMRD) {
        base_hgrrd = apple_ii_64k[1];
        softswitches |= SS_HGRRD;
    }

    if (softswitches & SS_RAMWRT) {
        base_hgrwrt = apple_ii_64k[1];
        softswitches |= SS_HGRWRT;
    }

    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_hires_on)
{
    if (softswitches & SS_HIRES) {
        return floating_bus();
    }

    softswitches |= SS_HIRES;

    if (softswitches & SS_80STORE) {
        if (softswitches & SS_PAGE2) {
            softswitches |= (SS_HGRRD|SS_HGRWRT);
            base_hgrrd  = apple_ii_64k[1];
            base_hgrwrt = apple_ii_64k[1];
        } else {
            softswitches &= ~(SS_HGRRD|SS_HGRWRT);
            base_hgrrd  = apple_ii_64k[0];
            base_hgrwrt = apple_ii_64k[0];
        }
    }

    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_check_hires)
{
    return (softswitches & SS_HIRES) ? 0x80 : 0x00;
}

// ----------------------------------------------------------------------------
// GC softswitches : Game Controller (joystick/paddles)
#define JOY_STEP_USEC (3300.0 / 256.0)
#define CYCLES_PER_USEC (CLK_6502 / 1000000)
#define JOY_STEP_CYCLES (JOY_STEP_USEC / CYCLES_PER_USEC)

GLUE_C_READ(read_button0)
{
    return joy_button0;
}

GLUE_C_READ(read_button1)
{
    return joy_button1;
}

GLUE_C_READ(read_button2)
{
    return joy_button0 | joy_button1;
}

GLUE_C_READ(read_gc_strobe)
{
    // Read Game Controller (paddle) strobe ...
    // From _Understanding the Apple IIe_ :
    //  * 7-29, discussing PREAD : "The timer duration will vary between 2 and 3302 usecs"
    //  * 7-30, timer reset : "But the timer pulse may still be high from the previous [strobe access] and the timers are
    //  not retriggered by C07X' if they have not yet reset from the previous trigger"
    if (gc_cycles_timer_0 <= 0)
    {
        gc_cycles_timer_0 = (int)((joy_x-5) * JOY_STEP_CYCLES);
    }
    if (gc_cycles_timer_1 <= 0)
    {
        gc_cycles_timer_1 = (int)(joy_y * JOY_STEP_CYCLES) + 2;
    }

    // NOTE (possible TODO FIXME): unimplemented GC2 and GC3 timers since they were not wired on the //e ...
    return floating_bus();
}

GLUE_C_READ(read_gc0)
{
    if (gc_cycles_timer_0 <= 0)
    {
        gc_cycles_timer_0 = 0;
        return 0;
    }
    return 0xFF;
}

GLUE_C_READ(read_gc1)
{
    if (gc_cycles_timer_1 <= 0)
    {
        gc_cycles_timer_1 = 0;
        return 0;
    }
    return 0xFF;
}

GLUE_C_READ(iie_read_gc2)
{
    return floating_bus();
}

GLUE_C_READ(iie_read_gc3)
{
    return floating_bus();
}

// ----------------------------------------------------------------------------
// LC : language card routines

static inline void _lc_to_auxmem() {
    if (softswitches & SS_LCRAM) {
        base_d000_rd += 0x2000;
        base_e000_rd = language_card[0]-0xC000;
    }

    if (softswitches & SS_LCWRT) {
        base_d000_wrt += 0x2000;
        base_e000_wrt = language_card[0]-0xC000;
    }
}

GLUE_C_READ(iie_c080)
{
    softswitches |= (SS_LCRAM|SS_BANK2);
    softswitches &= ~(SS_LCSEC|SS_LCWRT);

    base_d000_rd = language_banks[0]-0xD000;
    base_e000_rd = language_card[0]-0xE000;

    base_d000_wrt = 0;
    base_e000_wrt = 0;

    if (softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

GLUE_C_READ(iie_c081)
{
    if (softswitches & SS_LCSEC) {
        softswitches |= SS_LCWRT;
        base_d000_wrt = language_banks[0]-0xD000;
        base_e000_wrt = language_card[0]-0xE000;
    }

    softswitches |= (SS_LCSEC|SS_BANK2);
    softswitches &= ~SS_LCRAM;

    base_d000_rd = apple_ii_64k[0];
    base_e000_rd = apple_ii_64k[0];

    if (softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

GLUE_C_READ(lc_c082)
{
    softswitches &= ~(SS_LCRAM|SS_LCWRT|SS_LCSEC);
    softswitches |= SS_BANK2;

    base_d000_rd = apple_ii_64k[0];
    base_e000_rd = apple_ii_64k[0];

    base_d000_wrt = 0;
    base_e000_wrt = 0;

    return floating_bus();
}

GLUE_C_READ(iie_c083)
{
    if (softswitches & SS_LCSEC) {
        softswitches |= SS_LCWRT;
        base_d000_wrt = language_banks[0]-0xD000;
        base_e000_wrt = language_card[0]-0xE000;
    }

    softswitches |= (SS_LCSEC|SS_LCRAM|SS_BANK2);
    base_d000_rd = language_banks[0]-0xD000;
    base_e000_rd = language_card[0]-0xE000;

    if (softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

GLUE_C_READ(iie_c088)
{
    softswitches |= SS_LCRAM;
    softswitches &= ~(SS_LCWRT|SS_LCSEC|SS_BANK2);

    base_d000_rd = language_banks[0]-0xC000;
    base_e000_rd = language_card[0]-0xE000;

    base_d000_wrt = 0;
    base_e000_wrt = 0;

    if (softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

GLUE_C_READ(iie_c089)
{
    if (softswitches & SS_LCSEC) {
        softswitches |= SS_LCWRT;
        base_d000_wrt = language_banks[0]-0xC000;
        base_e000_wrt = language_card[0]-0xE000;
    }

    softswitches |= SS_LCSEC;
    softswitches &= ~(SS_LCRAM|SS_BANK2);

    base_d000_rd = apple_ii_64k[0];
    base_e000_rd = apple_ii_64k[0];

    if (softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

GLUE_C_READ(lc_c08a)
{
    softswitches &= ~(SS_LCRAM|SS_LCWRT|SS_LCSEC|SS_BANK2);

    base_d000_rd = apple_ii_64k[0];
    base_e000_rd = apple_ii_64k[0];

    base_d000_wrt = 0;
    base_e000_wrt = 0;

    return floating_bus();
}

GLUE_C_READ(iie_c08b)
{
    if (softswitches & SS_LCSEC) {
        softswitches |= SS_LCWRT;
        base_d000_wrt = language_banks[0]-0xC000;
        base_e000_wrt = language_card[0]-0xE000;
    }

    softswitches |= (SS_LCRAM|SS_LCSEC);
    softswitches &= ~SS_BANK2;

    base_d000_rd = language_banks[0]-0xC000;
    base_e000_rd = language_card[0]-0xE000;

    if (softswitches & SS_ALTZP) {
        _lc_to_auxmem();
    }
    return floating_bus();
}

GLUE_C_READ(iie_check_bank)
{
    return (softswitches & SS_BANK2) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_check_lcram)
{
    return (softswitches & SS_LCRAM) ? 0x80 : 0x00;
}

// ----------------------------------------------------------------------------
// Misc //e softswitches and vm routines

GLUE_C_READ(iie_80store_off)
{
    if (!(softswitches & SS_80STORE)) {
        return floating_bus();
    }

    softswitches &= ~(SS_80STORE|SS_TEXTRD|SS_TEXTWRT|SS_HGRRD|SS_HGRWRT);

    base_textrd  = apple_ii_64k[0];
    base_textwrt = apple_ii_64k[0];
    base_hgrrd   = apple_ii_64k[0];
    base_hgrwrt  = apple_ii_64k[0];

    if (softswitches & SS_RAMRD) {
        softswitches |= (SS_TEXTRD|SS_HGRRD);
        base_textrd = apple_ii_64k[1];
        base_hgrrd  = apple_ii_64k[1];
    }

    if (softswitches & SS_RAMWRT) {
        softswitches |= (SS_TEXTWRT|SS_HGRWRT);
        base_textwrt = apple_ii_64k[1];
        base_hgrwrt  = apple_ii_64k[1];
    }

    if (softswitches & SS_PAGE2) {
        softswitches |= SS_SCREEN;
    }

    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_80store_on)
{
    if (softswitches & SS_80STORE) {
        return floating_bus();
    }

    softswitches |= SS_80STORE;

    if (softswitches & SS_PAGE2) {
        softswitches |= (SS_TEXTRD|SS_TEXTWRT);
        base_textrd  = apple_ii_64k[1];
        base_textwrt = apple_ii_64k[1];
        if (softswitches & SS_HIRES) {
            softswitches |= (SS_HGRRD|SS_HGRWRT);
            base_hgrrd  = apple_ii_64k[1];
            base_hgrwrt = apple_ii_64k[1];
        }
    } else {
        softswitches &= ~(SS_TEXTRD|SS_TEXTWRT);
        base_textrd  = apple_ii_64k[0];
        base_textwrt = apple_ii_64k[0];
        if (softswitches & SS_HIRES) {
            softswitches &= ~(SS_HGRRD|SS_HGRWRT);
            base_hgrrd  = apple_ii_64k[0];
            base_hgrwrt = apple_ii_64k[0];
        }
    }

    softswitches &= ~SS_SCREEN;
    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_check_80store)
{
    return (softswitches & SS_80STORE) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_ramrd_main)
{
    if (!(softswitches & SS_RAMRD)) {
        return floating_bus();
    }

    softswitches &= ~SS_RAMRD;
    base_ramrd = apple_ii_64k[0];

    if (softswitches & SS_80STORE) {
        if (!(softswitches & SS_HIRES)) {
            softswitches &= ~SS_HGRRD;
            base_hgrrd = apple_ii_64k[0];
        }
    } else {
        softswitches &= ~(SS_TEXTRD|SS_HGRRD);
        base_textrd = apple_ii_64k[0];
        base_hgrrd  = apple_ii_64k[0];
    }

    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_ramrd_aux)
{
    if (softswitches & SS_RAMRD) {
        return floating_bus();
    }

    softswitches |= SS_RAMRD;
    base_ramrd = apple_ii_64k[1];

    if (softswitches & SS_80STORE) {
        if (!(softswitches & SS_HIRES)) {
            softswitches |= SS_HGRRD;
            base_hgrrd = apple_ii_64k[1];
        }
    } else {
        softswitches |= (SS_TEXTRD|SS_HGRRD);
        base_textrd = apple_ii_64k[1];
        base_hgrrd  = apple_ii_64k[1];
    }

    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_check_ramrd)
{
    return (softswitches & SS_RAMRD) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_ramwrt_main)
{
    if (!(softswitches & SS_RAMWRT)) {
        return floating_bus();
    }

    softswitches &= ~SS_RAMWRT;
    base_ramwrt = apple_ii_64k[0];

    if (softswitches & SS_80STORE) {
        if (!(softswitches & SS_HIRES)) {
            softswitches &= ~SS_HGRWRT;
            base_hgrwrt = apple_ii_64k[0];
        }
    } else {
        softswitches &= ~(SS_TEXTWRT|SS_HGRWRT);
        base_textwrt = apple_ii_64k[0];
        base_hgrwrt  = apple_ii_64k[0];
    }

    return floating_bus();
}

GLUE_C_READ(iie_ramwrt_aux)
{
    if (softswitches & SS_RAMWRT) {
        return floating_bus();
    }

    softswitches |= SS_RAMWRT;
    base_ramwrt = apple_ii_64k[1];

    if (softswitches & SS_80STORE) {
        if (!(softswitches & SS_HIRES)) {
            softswitches |= SS_HGRWRT;
            base_hgrwrt = apple_ii_64k[1];
        }
    } else {
        softswitches |= (SS_TEXTWRT|SS_HGRWRT);
        base_textwrt = apple_ii_64k[1];
        base_hgrwrt  = apple_ii_64k[1];
    }

    return floating_bus();
}

GLUE_C_READ(iie_check_ramwrt)
{
    return (softswitches & SS_RAMWRT) ? 0x80 : 0x00;
}

GLUE_C_READ_ALTZP(iie_altzp_main)
{
    if (!(softswitches & SS_ALTZP)) {
        /* NOTE : test if ALTZP already off - due to d000-bank issues it is *needed*, not just a shortcut */
        return floating_bus();
    }

    softswitches &= ~SS_ALTZP;
    base_stackzp = apple_ii_64k[0];

    if (softswitches & SS_LCRAM) {
        base_d000_rd -= 0x2000;
        base_e000_rd = language_card[0] - 0xE000;
    }

    if (softswitches & SS_LCWRT) {
        base_d000_wrt -= 0x2000;
        base_e000_wrt = language_card[0] - 0xE000;
    }

    return floating_bus();
}

GLUE_C_READ_ALTZP(iie_altzp_aux)
{
    if (softswitches & SS_ALTZP) {
        /* NOTE : test if ALTZP already on - due to d000-bank issues it is *needed*, not just a shortcut */
        return floating_bus();
    }

    softswitches |= SS_ALTZP;
    base_stackzp = apple_ii_64k[1];

    _lc_to_auxmem();

    return floating_bus();
}

GLUE_C_READ(iie_check_altzp)
{
    return (softswitches & SS_ALTZP) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_80col_off)
{
    if (!(softswitches & SS_80COL)) {
        return floating_bus();
    }

    softswitches &= ~SS_80COL;
    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_80col_on)
{
    if (softswitches & SS_80COL) {
        return floating_bus();
    }

    softswitches |= SS_80COL;
    video_setDirty(A2_DIRTY_FLAG);

    return floating_bus();
}

GLUE_C_READ(iie_check_80col)
{
    return (softswitches & SS_80COL) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_altchar_off)
{
    if (softswitches & SS_ALTCHAR) {
        softswitches &= ~SS_ALTCHAR;
        display_loadFont(0x40,0x40,ucase_glyphs,3);
        video_setDirty(A2_DIRTY_FLAG);
    }
    return floating_bus();
}

GLUE_C_READ(iie_altchar_on)
{
    if (!(softswitches & SS_ALTCHAR)) {
        softswitches |= SS_ALTCHAR;
        display_loadFont(0x40,0x20,mousetext_glyphs,1);
        display_loadFont(0x60,0x20,lcase_glyphs,2);
        video_setDirty(A2_DIRTY_FLAG);
    }
    return floating_bus();
}

GLUE_C_READ(iie_check_altchar)
{
    return (softswitches & SS_ALTCHAR) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_ioudis_off)
{
    softswitches &= ~SS_IOUDIS;
    return c_read_gc_strobe(ea);
}

GLUE_C_READ(iie_ioudis_on)
{
    softswitches |= SS_IOUDIS;
    return c_read_gc_strobe(ea);
}

GLUE_C_READ(iie_check_ioudis)
{
    c_read_gc_strobe(ea);
    return (softswitches & SS_IOUDIS) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_dhires_on)
{
    if (!(softswitches & SS_DHIRES)) {
        softswitches |= SS_DHIRES;
        video_setDirty(A2_DIRTY_FLAG);
    }
    return floating_bus();
}

GLUE_C_READ(iie_dhires_off)
{
    if (softswitches & SS_DHIRES) {
        softswitches &= ~SS_DHIRES;
        video_setDirty(A2_DIRTY_FLAG);
    }
    return floating_bus();
}

GLUE_C_READ(iie_check_dhires)
{
    c_read_gc_strobe(ea); // HACK FIXME : is this correct?
    return (softswitches & SS_DHIRES) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_check_vbl)
{
    bool vbl_bar = false;
    video_scanner_get_address(&vbl_bar);
    uint8_t key = apple_ii_64k[0][0xC000];
    return (key & ~0x80) | (vbl_bar ? 0x80 : 0x00);
}

GLUE_C_READ(iie_c3rom_peripheral)
{
    softswitches &= ~SS_C3ROM;
    if (!(softswitches & SS_CXROM)) {
        base_c3rom = (void *)iie_read_peripheral_card;
    }
    return 0x0;
}

GLUE_C_READ(iie_c3rom_internal)
{
    softswitches |= SS_C3ROM;
    base_c3rom = apple_ii_64k[1];
    return 0x0;
}

GLUE_C_READ(iie_check_c3rom)
{
    return (softswitches & SS_C3ROM) ? 0x00 : 0x80; // reversed pattern
}

GLUE_C_READ(iie_cxrom_peripheral)
{
    softswitches &= ~SS_CXROM;
    base_cxrom = (void *)iie_read_peripheral_card;
    base_c4rom = (void *)iie_read_peripheral_card;
    base_c5rom = (void *)iie_read_peripheral_card;
    if (!(softswitches & SS_C3ROM)) {
        base_c3rom = (void *)iie_read_peripheral_card;
    }
    return 0x0;
}

GLUE_C_READ(iie_cxrom_internal)
{
    softswitches |= SS_CXROM;
    base_cxrom = apple_ii_64k[1];
    base_c3rom = apple_ii_64k[1];
    base_c4rom = apple_ii_64k[1];
    base_c5rom = apple_ii_64k[1];
    return 0x0;
}

GLUE_C_READ(iie_check_cxrom)
{
    return (softswitches & SS_CXROM) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_read_slot_expansion)
{
    // HACK TODO FIXME : how does the expansion slot get referenced?  Need to handle other ROMs that might use this area
    // ... Also Need moar tests ...
    if (ea == 0xCFFF) {
        // disable expansion ROM
    }
    return apple_ii_64k[1][ea];
}

GLUE_C_READ(debug_illegal_bcd)
{
    LOG("Illegal/undefined BCD operation encountered, debug break on c_debug_illegal_bcd to debug...");
    return 0;
}

GLUE_C_WRITE(cpu_irqCheck)
{
    MB_UpdateCycles();
    // TODO : other peripheral card interrupt handling
}

// ----------------------------------------------------------------------------

static void _initialize_iie_switches(void) {

    base_stackzp = apple_ii_64k[0];
    base_d000_rd = apple_ii_64k[0];
    base_d000_wrt = language_banks[0] - 0xD000;
    base_e000_rd = apple_ii_64k[0];
    base_e000_wrt = language_card[0] - 0xE000;

    base_ramrd = apple_ii_64k[0];
    base_ramwrt = apple_ii_64k[0];
    base_textrd = apple_ii_64k[0];
    base_textwrt = apple_ii_64k[0];
    base_hgrrd = apple_ii_64k[0];
    base_hgrwrt= apple_ii_64k[0];

    base_c3rom = apple_ii_64k[1]; // c3rom internal
    base_c4rom = apple_ii_64k[1]; // c4rom internal
    base_c5rom = apple_ii_64k[1]; // c5rom internal
    base_cxrom = (void *)iie_read_peripheral_card; // cxrom peripheral
}

static void _initialize_font(void) {
    display_loadFont(0x00,0x40,ucase_glyphs,2);
    display_loadFont(0x40,0x40,ucase_glyphs,3);
    display_loadFont(0x80,0x40,ucase_glyphs,0);
    display_loadFont(0xC0,0x20,ucase_glyphs,0);
    display_loadFont(0xE0,0x20,lcase_glyphs,0);
}

static void _initialize_apple_ii_memory(void) {
    for (unsigned int i = 0; i < 0x10000; i++) {
        apple_ii_64k[0][i] = 0;
        apple_ii_64k[1][i] = 0;
    }

    // Stripe words of main memory on machine reset ...
    // NOTE: cracked version of J---- will lock up without this
    for (unsigned int i = 0; i < 0xC000;) {
        apple_ii_64k[0][i++] = 0xFF;
        apple_ii_64k[0][i++] = 0xFF;
        i += 2;
    }

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

    for (unsigned int i = 0; i < 0x10000; i++) {
        cpu65_vmem_r[i] = iie_read_ram_default;
        cpu65_vmem_w[i] = iie_write_ram_default;
    }

    // language card read/write area

    for (unsigned int i = 0xD000; i < 0xE000; i++) {
        cpu65_vmem_w[i] = write_ram_bank;
        cpu65_vmem_r[i] = read_ram_bank;
    }

    for (unsigned int i = 0xE000; i < 0x10000; i++) {
        cpu65_vmem_w[i] = write_ram_lc;
        cpu65_vmem_r[i] = read_ram_lc;
    }

    // done common initialization

    // initialize zero-page, //e specific
    for (unsigned int i = 0; i < 0x200; i++) {
        cpu65_vmem_r[i] = iie_read_ram_zpage_and_stack;
        cpu65_vmem_w[i] = iie_write_ram_zpage_and_stack;
    }

    // initialize first text & hires page, which are specially bank switched
    //
    // display_reset() below substitutes it's own hooks for all visible write locations affect the display, leaving our
    // write-functions in place only at the `screen holes', hence the name.
    for (unsigned int i = 0x400; i < 0x800; i++) {
        cpu65_vmem_r[i] = iie_read_ram_text_page0;
        cpu65_vmem_w[i] = iie_write_screen_hole_text_page0;
    }

    for (unsigned int i = 0x2000; i < 0x4000; i++) {
        cpu65_vmem_r[i] = iie_read_ram_hires_page0;
        cpu65_vmem_w[i] = iie_write_screen_hole_hires_page0;
    }

    // softswich rom
    for (unsigned int i = 0xC000; i < 0xC100; i++) {
        cpu65_vmem_r[i] = read_unmapped_softswitch;
        cpu65_vmem_w[i] = write_unmapped_softswitch;
    }

    // slot rom defaults
    for (unsigned int i = 0xC100; i < 0xD000; i++) {
        cpu65_vmem_r[i] = iie_read_ram_default;
        cpu65_vmem_w[i] = ram_nop;
    }

    // keyboard data and strobe (READ)
    for (unsigned int i = 0xC000; i < 0xC010; i++) {
        cpu65_vmem_r[i] = read_keyboard;
    }

    for (unsigned int i = 0xC010; i < 0xC020; i++) {
        cpu65_vmem_r[i] = cpu65_vmem_w[i] = read_keyboard_strobe;
    }

    // RDBNK2 switch
    cpu65_vmem_r[0xC011] = iie_check_bank;

    // RDLCRAM switch
    cpu65_vmem_r[0xC012] = iie_check_lcram;

    // 80STORE switch
    cpu65_vmem_w[0xC000] = iie_80store_off;
    cpu65_vmem_w[0xC001] = iie_80store_on;
    cpu65_vmem_r[0xC018] = iie_check_80store;

    // RAMRD switch
    cpu65_vmem_w[0xC002] = iie_ramrd_main;
    cpu65_vmem_w[0xC003] = iie_ramrd_aux;
    cpu65_vmem_r[0xC013] = iie_check_ramrd;

    // RAMWRT switch
    cpu65_vmem_w[0xC004] = iie_ramwrt_main;
    cpu65_vmem_w[0xC005] = iie_ramwrt_aux;
    cpu65_vmem_r[0xC014] = iie_check_ramwrt;

    // ALTZP switch
    cpu65_vmem_w[0xC008] = iie_altzp_main;
    cpu65_vmem_w[0xC009] = iie_altzp_aux;
    cpu65_vmem_r[0xC016] = iie_check_altzp;

    // 80COL switch
    cpu65_vmem_w[0xC00C] = iie_80col_off;
    cpu65_vmem_w[0xC00D] = iie_80col_on;
    cpu65_vmem_r[0xC01F] = iie_check_80col;

    // ALTCHAR switch
    cpu65_vmem_w[0xC00E] = iie_altchar_off;
    cpu65_vmem_w[0xC00F] = iie_altchar_on;
    cpu65_vmem_r[0xC01E] = iie_check_altchar;

    // SLOTC3ROM switch
    cpu65_vmem_w[0xC00A] = iie_c3rom_internal;
    cpu65_vmem_w[0xC00B] = iie_c3rom_peripheral;
    cpu65_vmem_r[0xC017] = iie_check_c3rom;

    // SLOTCXROM switch
    cpu65_vmem_w[0xC006] = iie_cxrom_peripheral;
    cpu65_vmem_w[0xC007] = iie_cxrom_internal;
    cpu65_vmem_r[0xC015] = iie_check_cxrom;

    // RDVBLBAR switch
    cpu65_vmem_r[0xC019] = iie_check_vbl;

    // TEXT switch
    cpu65_vmem_r[0xC050] = cpu65_vmem_w[0xC050] = read_switch_graphics;
    cpu65_vmem_r[0xC051] = cpu65_vmem_w[0xC051] = read_switch_text;
    cpu65_vmem_r[0xC01A] = iie_check_text;

    // MIXED switch
    cpu65_vmem_r[0xC052] = cpu65_vmem_w[0xC052] = read_switch_no_mixed;
    cpu65_vmem_r[0xC053] = cpu65_vmem_w[0xC053] = read_switch_mixed;
    cpu65_vmem_r[0xC01B] = iie_check_mixed;

    // PAGE2 switch
    cpu65_vmem_r[0xC054] = cpu65_vmem_w[0xC054] = iie_page2_off;
    cpu65_vmem_r[0xC01C] = iie_check_page2;
    cpu65_vmem_r[0xC055] = cpu65_vmem_w[0xC055] = iie_page2_on;

    // HIRES switch
    cpu65_vmem_r[0xC01D] = iie_check_hires;
    cpu65_vmem_r[0xC056] = cpu65_vmem_w[0xC056] = iie_hires_off;
    cpu65_vmem_r[0xC057] = cpu65_vmem_w[0xC057] = iie_hires_on;

    // game I/O switches
    cpu65_vmem_r[0xC061] = cpu65_vmem_r[0xC069] = read_button0;
    cpu65_vmem_r[0xC062] = cpu65_vmem_r[0xC06A] = read_button1;
    cpu65_vmem_r[0xC063] = cpu65_vmem_r[0xC06B] = read_button2;
    cpu65_vmem_r[0xC064] = cpu65_vmem_r[0xC06C] = read_gc0;
    cpu65_vmem_r[0xC065] = cpu65_vmem_r[0xC06D] = read_gc1;
    cpu65_vmem_r[0xC066] = iie_read_gc2;
    cpu65_vmem_r[0xC067] = iie_read_gc3;

    for (unsigned int i = 0xC070; i < 0xC080; i++) {
        cpu65_vmem_r[i] = cpu65_vmem_w[i] = read_gc_strobe;
    }

    // IOUDIS switch & read_gc_strobe
    cpu65_vmem_w[0xC07E] = iie_ioudis_on;
    cpu65_vmem_w[0xC07F] = iie_ioudis_off; // HACK FIXME TODO : double-check this stuff against AWin...
    cpu65_vmem_r[0xC07E] = iie_check_ioudis;
    cpu65_vmem_r[0xC07F] = iie_check_dhires;

    // Annunciator
    for (unsigned int i = 0xC058; i <= 0xC05D; i++) {
        cpu65_vmem_w[i] = cpu65_vmem_r[i] = iie_annunciator;
    }

    // DHIRES
    cpu65_vmem_w[0xC05E] = cpu65_vmem_r[0xC05E] = iie_dhires_on;
    cpu65_vmem_w[0xC05F] = cpu65_vmem_r[0xC05F] = iie_dhires_off;

    // language card softswitches
    cpu65_vmem_r[0xC080] = cpu65_vmem_w[0xC080] = cpu65_vmem_r[0xC084] = cpu65_vmem_w[0xC084] = iie_c080;
    cpu65_vmem_r[0xC081] = cpu65_vmem_w[0xC081] = cpu65_vmem_r[0xC085] = cpu65_vmem_w[0xC085] = iie_c081;
    cpu65_vmem_r[0xC082] = cpu65_vmem_w[0xC082] = cpu65_vmem_r[0xC086] = cpu65_vmem_w[0xC086] = lc_c082;
    cpu65_vmem_r[0xC083] = cpu65_vmem_w[0xC083] = cpu65_vmem_r[0xC087] = cpu65_vmem_w[0xC087] = iie_c083;

    cpu65_vmem_r[0xC088] = cpu65_vmem_w[0xC088] = cpu65_vmem_r[0xC08C] = cpu65_vmem_w[0xC08C] = iie_c088;
    cpu65_vmem_r[0xC089] = cpu65_vmem_w[0xC089] = cpu65_vmem_r[0xC08D] = cpu65_vmem_w[0xC08D] = iie_c089;
    cpu65_vmem_r[0xC08A] = cpu65_vmem_w[0xC08A] = cpu65_vmem_r[0xC08E] = cpu65_vmem_w[0xC08E] = lc_c08a;
    cpu65_vmem_r[0xC08B] = cpu65_vmem_w[0xC08B] = cpu65_vmem_r[0xC08F] = cpu65_vmem_w[0xC08F] = iie_c08b;

    // slot i/o area
    for (unsigned int i = 0xC100; i < 0xC300; i++) {
        cpu65_vmem_r[i] = iie_read_slotx; // slots 1 & 2 (x)
    }

    for (unsigned int i = 0xC300; i < 0xC400; i++) {
        cpu65_vmem_r[i] = iie_read_slot3; // slot 3 (80col)
    }

    for (unsigned int i = 0xC400; i < 0xC500; i++) {
        cpu65_vmem_r[i] = iie_read_slot4; // slot 4 - MB or Phasor
    }

    for (unsigned int i = 0xC500; i < 0xC600; i++) {
        cpu65_vmem_r[i] = iie_read_slot5; // slot 5 - MB #2
    }

    for (unsigned int i = 0xC600; i < 0xC800; i++) {
        cpu65_vmem_r[i] = iie_read_slotx; // slots 6 - 7 (x)
    }

    for (unsigned int i = 0xC800; i < 0xD000; i++) {
        cpu65_vmem_r[i] = iie_read_slot_expansion;
    }

    display_reset();

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
    vm_reinitializeAudio();
    disk6_init();
    _initialize_iie_switches();
    c_joystick_reset();
}

void vm_reinitializeAudio(void) {
    for (unsigned int i = 0xC030; i < 0xC040; i++) {
        cpu65_vmem_r[i] = cpu65_vmem_w[i] = speaker_toggle;
    }
#warning TODO FIXME ... should unset MB/Phasor hooks if volume is zero ...
}

bool vm_saveState(StateHelper_s *helper) {
    bool saved = false;
    int fd = helper->fd;

    do {
        uint8_t serialized[8] = { 0 };

        serialized[0] = (uint8_t)((softswitches & 0xFF000000) >> 24);
        serialized[1] = (uint8_t)((softswitches & 0xFF0000  ) >> 16);
        serialized[2] = (uint8_t)((softswitches & 0xFF00    ) >>  8);
        serialized[3] = (uint8_t)((softswitches & 0xFF      ) >>  0);
        LOG("SAVE softswitches = %08x", softswitches);
        if (!helper->save(fd, serialized, sizeof(softswitches))) {
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
        LOG("SAVE base_ramrd = %d", (base_ramrd == apple_ii_64k[0]) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_ramrd == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        LOG("SAVE base_ramwrt = %d", (base_ramwrt == apple_ii_64k[0]) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_ramwrt == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        LOG("SAVE base_textrd = %d", (base_textrd == apple_ii_64k[0]) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_textrd == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        LOG("SAVE base_textwrt = %d", (base_textwrt == apple_ii_64k[0]) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_textwrt == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        LOG("SAVE base_hgrrd = %d", (base_hgrrd == apple_ii_64k[0]) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_hgrrd == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        LOG("SAVE base_hgrwrt = %d", (base_hgrwrt == apple_ii_64k[0]) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_hgrwrt == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        LOG("SAVE base_stackzp = %d", (base_stackzp == apple_ii_64k[0]) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_stackzp == apple_ii_64k[0]) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }
        LOG("SAVE base_c3rom = %d", (base_c3rom == (void *)iie_read_peripheral_card) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_c3rom == (void *)iie_read_peripheral_card) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }

        LOG("SAVE base_cxrom = %d", (base_cxrom == (void *)iie_read_peripheral_card) ? serialized[0] : serialized[1]);
        if (!helper->save(fd, (base_cxrom == (void *)iie_read_peripheral_card) ? &serialized[0] : &serialized[1], 1)) {
            break;
        }

        if (base_d000_rd == apple_ii_64k[0]) {
            LOG("SAVE base_d000_rd = %d", serialized[0]);
            if (!helper->save(fd, &serialized[0], 1)) { // base_d000_rd --> //e ROM
                break;
            }
        } else if (base_d000_rd == language_banks[0] - 0xD000) {
            LOG("SAVE base_d000_rd = %d", serialized[2]);
            if (!helper->save(fd, &serialized[2], 1)) { // base_d000_rd --> main LC mem
                break;
            }
        } else if (base_d000_rd == language_banks[0] - 0xC000) {
            LOG("SAVE base_d000_rd = %d", serialized[3]);
            if (!helper->save(fd, &serialized[3], 1)) { // base_d000_rd --> main LC mem
                break;
            }
        } else if (base_d000_rd == language_banks[1] - 0xD000) {
            LOG("SAVE base_d000_rd = %d", serialized[4]);
            if (!helper->save(fd, &serialized[4], 1)) { // base_d000_rd --> aux  LC mem
                break;
            }
        } else if (base_d000_rd == language_banks[1] - 0xC000) {
            LOG("SAVE base_d000_rd = %d", serialized[5]);
            if (!helper->save(fd, &serialized[5], 1)) { // base_d000_rd --> aux  LC mem
                break;
            }
        } else {
            LOG("OOPS ... language_banks[0] == %p base_d000_rd == %p", language_banks[0], base_d000_rd);
            assert(false);
        }

        if (base_d000_wrt == 0) {
            LOG("SAVE base_d000_wrt = %d", serialized[0]);
            if (!helper->save(fd, &serialized[0], 1)) { // base_d000_wrt --> no write
                break;
            }
        } else if (base_d000_wrt == language_banks[0] - 0xD000) {
            LOG("SAVE base_d000_wrt = %d", serialized[2]);
            if (!helper->save(fd, &serialized[2], 1)) { // base_d000_wrt --> main LC mem
                break;
            }
        } else if (base_d000_wrt == language_banks[0] - 0xC000) {
            LOG("SAVE base_d000_wrt = %d", serialized[3]);
            if (!helper->save(fd, &serialized[3], 1)) { // base_d000_wrt --> main LC mem
                break;
            }
        } else if (base_d000_wrt == language_banks[1] - 0xD000) {
            LOG("SAVE base_d000_wrt = %d", serialized[4]);
            if (!helper->save(fd, &serialized[4], 1)) { // base_d000_wrt --> aux  LC mem
                break;
            }
        } else if (base_d000_wrt == language_banks[1] - 0xC000) {
            LOG("SAVE base_d000_wrt = %d", serialized[5]);
            if (!helper->save(fd, &serialized[5], 1)) { // base_d000_wrt --> aux  LC mem
                break;
            }
        } else {
            LOG("OOPS ... language_banks[0] == %p base_d000_wrt == %p", language_banks[0], base_d000_wrt);
            assert(false);
        }

        if (base_e000_rd == apple_ii_64k[0]) {
            LOG("SAVE base_e000_rd = %d", serialized[0]);
            if (!helper->save(fd, &serialized[0], 1)) { // base_e000_rd --> //e ROM
                break;
            }
        } else if (base_e000_rd == language_card[0] - 0xE000) {
            LOG("SAVE base_e000_rd = %d", serialized[2]);
            if (!helper->save(fd, &serialized[2], 1)) { // base_e000_rd --> main LC mem
                break;
            }
        } else if (base_e000_rd == language_card[0] - 0xC000) {
            LOG("SAVE base_e000_rd = %d", serialized[3]);
            if (!helper->save(fd, &serialized[3], 1)) { // base_e000_rd --> aux  LC mem
                break;
            }
        } else {
            LOG("OOPS ... language_card[0] == %p base_e000_rd == %p", language_card[0], base_e000_rd);
            assert(false);
        }

        if (base_e000_wrt == 0) {
            LOG("SAVE base_e000_wrt = %d", serialized[0]);
            if (!helper->save(fd, &serialized[0], 1)) { // base_e000_wrt --> no write
                break;
            }
        } else if (base_e000_wrt == language_card[0] - 0xE000) {
            LOG("SAVE base_e000_wrt = %d", serialized[2]);
            if (!helper->save(fd, &serialized[2], 1)) { // base_e000_wrt --> main LC mem
                break;
            }
        } else if (base_e000_wrt == language_card[0] - 0xC000) {
            LOG("SAVE base_e000_wrt = %d", serialized[3]);
            if (!helper->save(fd, &serialized[3], 1)) { // base_e000_wrt --> aux  LC mem
                break;
            }
        } else {
            LOG("OOPS ... language_card[0] == %p base_e000_wrt == %p", language_card[0], base_e000_wrt);
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
        softswitches  = (uint32_t)(serialized[0] << 24);
        softswitches |= (uint32_t)(serialized[1] << 16);
        softswitches |= (uint32_t)(serialized[2] <<  8);
        softswitches |= (uint32_t)(serialized[3] <<  0);
        LOG("LOAD softswitches = %08x", softswitches);

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
        LOG("LOAD base_ramrd = %d", state);
        base_ramrd = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD base_ramwrt = %d", state);
        base_ramwrt = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD base_textrd = %d", state);
        base_textrd = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD base_textwrt = %d", state);
        base_textwrt = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD base_hgrrd = %d", state);
        base_hgrrd = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD base_hgrwrt = %d", state);
        base_hgrwrt = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD base_stackzp = %d", state);
        base_stackzp = state == 0x0 ? apple_ii_64k[0] : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD base_c3rom = %d", state);
        base_c3rom = state == 0x0 ? (void *)iie_read_peripheral_card : apple_ii_64k[1];

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        LOG("LOAD base_cxrom = %d", state);
        if (state == 0) {
            base_cxrom = (void *)iie_read_peripheral_card;
            base_c4rom = (void *)iie_read_peripheral_card;
            base_c5rom = (void *)iie_read_peripheral_card;
        } else {
            base_cxrom = apple_ii_64k[1];
            base_c4rom = apple_ii_64k[1];
            base_c5rom = apple_ii_64k[1];
        }

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        switch (state) {
            case 0:
                base_d000_rd = apple_ii_64k[0];
                break;
            case 2:
                base_d000_rd = language_banks[0] - 0xD000;
                break;
            case 3:
                base_d000_rd = language_banks[0] - 0xC000;
                break;
            case 4:
                base_d000_rd = language_banks[1] - 0xD000;
                break;
            case 5:
                base_d000_rd = language_banks[1] - 0xC000;
                break;
            default:
                LOG("Unknown state base_d000_rd %02x", state);
                assert(false);
                break;
        }
        LOG("LOAD base_d000_rd = %d", state);

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        switch (state) {
            case 0:
                base_d000_wrt = 0;
                break;
            case 2:
                base_d000_wrt = language_banks[0] - 0xD000;
                break;
            case 3:
                base_d000_wrt = language_banks[0] - 0xC000;
                break;
            case 4:
                base_d000_wrt = language_banks[1] - 0xD000;
                break;
            case 5:
                base_d000_wrt = language_banks[1] - 0xC000;
                break;
            default:
                LOG("Unknown state base_d000_wrt %02x", state);
                assert(false);
                break;
        }
        LOG("LOAD base_d000_wrt = %d", state);

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        switch (state) {
            case 0:
                base_e000_rd = apple_ii_64k[0];
                break;
            case 2:
                base_e000_rd = language_card[0] - 0xE000;
                break;
            case 3:
                base_e000_rd = language_card[0] - 0xC000;
                break;
            default:
                LOG("Unknown state base_e000_rd %02x", state);
                assert(false);
                break;
        }
        LOG("LOAD base_e000_rd = %d", state);

        if (!helper->load(fd, &state, 1)) {
            break;
        }
        switch (state) {
            case 0:
                base_e000_wrt = 0;
                break;
            case 2:
                base_e000_wrt = language_card[0] - 0xE000;
                break;
            case 3:
                base_e000_wrt = language_card[0] - 0xC000;
                break;
            default:
                LOG("Unknown state base_e000_wrt %02x", state);
                assert(false);
                break;
        }
        LOG("LOAD base_e000_wrt = %d", state);

        loaded = true;
    } while (0);

    return loaded;
}

void debug_print_softwitches(void) {
    // useful from GDB ...

    fprintf(stderr, "STANDARD: ");
    if (softswitches & SS_TEXT) {
        fprintf(stderr, "SS_TEXT ");
    }
    if (softswitches & SS_MIXED) {
        fprintf(stderr, "SS_MIXED ");
    }
    if (softswitches & SS_HIRES) {
        fprintf(stderr, "SS_HIRES ");
    }
    if (softswitches & SS_PAGE2) {
        fprintf(stderr, "SS_PAGE2 ");
    }
    if (softswitches & SS_BANK2) {
        fprintf(stderr, "SS_BANK2 ");
    }
    if (softswitches & SS_LCRAM) {
        fprintf(stderr, "SS_LCRAM ");
    }
    if (softswitches & SS_80STORE) {
        fprintf(stderr, "SS_80STORE ");
    }
    if (softswitches & SS_80COL) {
        fprintf(stderr, "SS_80COL ");
    }
    if (softswitches & SS_RAMRD) {
        fprintf(stderr, "SS_RAMRD ");
    }
    if (softswitches & SS_RAMWRT) {
        fprintf(stderr, "SS_RAMWRT ");
    }
    if (softswitches & SS_ALTZP) {
        fprintf(stderr, "SS_ALTZP ");
    }
    if (softswitches & SS_DHIRES) {
        fprintf(stderr, "SS_DHIRES ");
    }
    if (softswitches & SS_IOUDIS) {
        fprintf(stderr, "SS_IOUDIS ");
    }
    if (softswitches & SS_CXROM) {
        fprintf(stderr, "SS_CXROM ");
    }
    if (softswitches & SS_C3ROM) {
        fprintf(stderr, "SS_C3ROM ");
    }
    if (softswitches & SS_ALTCHAR) {
        fprintf(stderr, "SS_ALTCHAR ");
    }
    fprintf(stderr, "\n");

    // pseudo #1
    fprintf(stderr, "PSEUDO 1: ");
    if (softswitches & SS_LCSEC) {
        fprintf(stderr, "SS_LCSEC ");
    }
    if (softswitches & SS_LCWRT) {
        fprintf(stderr, "SS_LCWRT ");
    }
    fprintf(stderr, "\n");

    // pseudo #2
    fprintf(stderr, "PSEUDO 2: ");
    if (softswitches & SS_SCREEN) {
        fprintf(stderr, "SS_SCREEN ");
    }
    if (softswitches & SS_TEXTRD) {
        fprintf(stderr, "SS_TEXTRD ");
    }
    if (softswitches & SS_TEXTWRT) {
        fprintf(stderr, "SS_TEXTWRT ");
    }
    if (softswitches & SS_HGRRD) {
        fprintf(stderr, "SS_HGRRD ");
    }
    if (softswitches & SS_HGRWRT) {
        fprintf(stderr, "SS_HGRWRT ");
    }

    fprintf(stderr, "\n");
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

