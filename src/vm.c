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

/*
 * Apple //e VM Routines
 *
 */

#include "common.h"

GLUE_C_READ(ram_nop)
{
    return 0x0;
}

GLUE_C_READ(read_unmapped_softswitch)
{
    return apple_ii_64k[0][ea];
}

GLUE_C_WRITE(write_unmapped_softswitch)
{
    // ...
}

GLUE_C_READ(read_keyboard)
{
    return apple_ii_64k[0][0xC000];
}

GLUE_C_READ(read_keyboard_strobe)
{
    apple_ii_64k[0][0xC000] &= 0x7F;
    apple_ii_64k[1][0xC000] &= 0x7F;
    return apple_ii_64k[0][0xC000];
}

GLUE_C_READ(read_random)
{
    static time_t seed=0;
    if (!seed) {
        seed = time(NULL);
        srandom(seed);
    }
    return (random() % UINT_MAX);
}

GLUE_C_READ(speaker_toggle)
{
#ifdef AUDIO_ENABLED
    SpkrToggle();
#endif
    return 0;
}

// ----------------------------------------------------------------------------
// graphics softswitches

GLUE_C_READ(iie_page2_off)
{
    if (!(softswitches & SS_PAGE2)) {
        return 0x0; // TODO: no early return?
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

    video_setpage(0);

    return 0x0;
}

GLUE_C_READ(iie_page2_on)
{
    if (softswitches & SS_PAGE2) {
        return 0x0; // TODO: no early return?
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
        video_setpage(1);
    }

    return 0x0;
}

GLUE_C_READ(read_switch_graphics)
{
    if (softswitches & SS_TEXT) {
        softswitches &= ~SS_TEXT;
        video_redraw();
    }
    return 0x0;
}

GLUE_C_READ(read_switch_text)
{
    if (!(softswitches & SS_TEXT)) {
        softswitches |= SS_TEXT;
        video_redraw();
    }
    return 0x0;
}

GLUE_C_READ(read_switch_no_mixed)
{
    if (softswitches & SS_MIXED) {
        softswitches &= ~SS_MIXED;
        video_redraw();
    }
    return 0x0;
}

GLUE_C_READ(read_switch_mixed)
{
    if (!(softswitches & SS_MIXED)) {
        softswitches |= SS_MIXED;
        video_redraw();
    }
    return 0x0;
}

GLUE_C_READ(iie_hires_off)
{
    if (!(softswitches & SS_HIRES)) {
        return 0x0; // TODO: no early return?
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

    video_redraw();
    return 0x0;
}

GLUE_C_READ(iie_hires_on)
{
    if (softswitches & SS_HIRES) {
        return 0x0; // TODO: no early return?
    }

    softswitches |= SS_HIRES;

    if (softswitches & SS_80STORE) {
        if (softswitches & SS_PAGE2) {
            softswitches &= ~(SS_HGRRD|SS_HGRWRT);
            base_hgrrd  = apple_ii_64k[0];
            base_hgrwrt = apple_ii_64k[0];
        } else {
            softswitches |= (SS_HGRRD|SS_HGRWRT);
            base_hgrrd  = apple_ii_64k[1];
            base_hgrwrt = apple_ii_64k[1];
        }
    }

    video_redraw();
    return 0x0;
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
    return joy_button2;
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
        gc_cycles_timer_0 = (int)(joy_x * JOY_STEP_CYCLES) + 2;
    }
    if (gc_cycles_timer_1 <= 0)
    {
        gc_cycles_timer_1 = (int)(joy_y * JOY_STEP_CYCLES) + 2;
    }

    // NOTE (possible TODO FIXME): unimplemented GC2 and GC3 timers since they were not wired on the //e ...
    return 0x0;
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
    return 0x0;
}

GLUE_C_READ(iie_read_gc3)
{
    return 0x0;
}

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
    return 0x0;
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
    return 0x0;
}

GLUE_C_READ(lc_c082)
{
    softswitches &= ~(SS_LCRAM|SS_LCWRT|SS_LCSEC);
    softswitches |= SS_BANK2;

    base_d000_rd = apple_ii_64k[0];
    base_e000_rd = apple_ii_64k[0];

    base_d000_wrt = 0;
    base_e000_wrt = 0;

    return 0x0;
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
    return 0x0;
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
    return 0x0;
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
    return 0x0;
}

GLUE_C_READ(lc_c08a)
{
    softswitches &= ~(SS_LCRAM|SS_LCWRT|SS_LCSEC|SS_BANK2);

    base_d000_rd = apple_ii_64k[0];
    base_e000_rd = apple_ii_64k[0];

    base_d000_wrt = 0;
    base_e000_wrt = 0;

    return 0x0;
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
    return 0x0;
}

// ----------------------------------------------------------------------------
// Misc //e softswitches and vm routines

GLUE_C_READ(iie_80store_off)
{
    if (!(softswitches & SS_80STORE)) {
        return 0x0; // TODO: no early return?
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
        video_setpage(1);
    }

    return 0x0;
}

GLUE_C_READ(iie_80store_on)
{
    if (softswitches & SS_80STORE) {
        return 0x0; // TODO: no early return?
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
    video_setpage(0);
    return 0x0;
}

GLUE_C_READ(iie_check_80store)
{
    return (softswitches & SS_80STORE) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_ramrd_main)
{
    if (!(softswitches & SS_RAMRD)) {
        return 0x0; // TODO: no early return?
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

    return 0x0;
}

GLUE_C_READ(iie_ramrd_aux)
{
    if (softswitches & SS_RAMRD) {
        return 0x0; // TODO: no early return?
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

    return 0x0;
}

GLUE_C_READ(iie_check_ramrd)
{
    return (softswitches & SS_RAMRD) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_ramwrt_main)
{
    if (!(softswitches & SS_RAMWRT)) {
        return 0x0; // TODO: no early return?
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

    return 0x0;
}

GLUE_C_READ(iie_ramwrt_aux)
{
    if (softswitches & SS_RAMWRT) {
        return 0x0; // TODO: no early return?
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

    return 0x0;
}

GLUE_C_READ(iie_check_ramwrt)
{
    return (softswitches & SS_RAMWRT) ? 0x80 : 0x00;
}

GLUE_C_READ_ALTZP(iie_altzp_main)
{
    if (!(softswitches & SS_ALTZP)) {
        /* NOTE : test if ALTZP already off - due to d000-bank issues it is *needed*, not just a shortcut */
        return 0x0;
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

    return 0x0;
}

GLUE_C_READ_ALTZP(iie_altzp_aux)
{
    if (softswitches & SS_ALTZP) {
        /* NOTE : test if ALTZP already on - due to d000-bank issues it is *needed*, not just a shortcut */
        return 0x0;
    }

    softswitches |= SS_ALTZP;
    base_stackzp = apple_ii_64k[1];

    _lc_to_auxmem();

    return 0x0;
}

GLUE_C_READ(iie_check_altzp)
{
    return (softswitches & SS_ALTZP) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_80col_off)
{
    if (!(softswitches & SS_80COL)) {
        return 0x0; // TODO: no early return?
    }

    softswitches &= ~SS_80COL;

    if (softswitches & (SS_TEXT|SS_MIXED|SS_DHIRES)) {
        video_redraw();
    }

    return 0x0;
}

GLUE_C_READ(iie_80col_on)
{
    if (softswitches & SS_80COL) {
        return 0x0;
    }

    softswitches |= SS_80COL;

    if (softswitches & (SS_TEXT|SS_MIXED|SS_DHIRES)) {
        video_redraw();
    }

    return 0x0;
}

GLUE_C_READ(iie_check_80col)
{
    return (softswitches & SS_80COL) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_altchar_off)
{
    if (softswitches & SS_ALTCHAR) {
        softswitches &= ~SS_ALTCHAR;
        c_set_primary_char();
    }
    return 0x0;
}

GLUE_C_READ(iie_altchar_on)
{
    if (!(softswitches & SS_ALTCHAR)) {
        softswitches |= SS_ALTCHAR;
        c_set_altchar();
    }
    return 0x0;
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
    // FIXME : does this depend on IOUDIS?
    if (!(softswitches & SS_DHIRES)) {
        softswitches |= SS_DHIRES;
        video_redraw();
    }
    return 0x0;
}

GLUE_C_READ(iie_dhires_off)
{
    // FIXME : does this depend on IOUDIS?
    if (softswitches & SS_DHIRES) {
        softswitches &= ~SS_DHIRES;
        video_redraw();
    }
    return 0x0;
}

GLUE_C_READ(iie_check_dhires)
{
    c_read_gc_strobe(ea); // HACK FIXME : is this correct?
    return (softswitches & SS_DHIRES) ? 0x80 : 0x00;
}

