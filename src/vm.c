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

/* ----------------------------------------------------------------------------
        Apple //e 64K VM

        (Two byte addresses are represented with least significant
         byte first, e.g. address FA59 is represented as 59 FA)

        This is not meant to be definitive.
        Verify against _Understanding the Apple IIe_ by Jim Sather

        Address         Description
        -----------     -----------
        0000 - 00FF     Zero page RAM
        0100 - 01FF     Stack
        0200 - 03EF     RAM
        03F0 - 03F1     Address for BRK instruction (normally 59 FA = FA59)
        03F2 - 03F3     Soft entry vector (normally BF 9D = 9DBF)
        03F4            Power-up byte
        03F5 - 03F7     Jump instruction to the subroutine which handles
                        Applesoft II &-commands (normally 4C 58 FF = JMP FF58)
        03F8 - 03FA     Jump instruction to the subroutine which handles User
                        CTRL-Y commands (normally 4C 65 FF = JMP FF65)
        03FB - 03FD     Jump instruction to the subroutine which handles
                        Non-Maskable Interrupts (NMI) (normally 4C 65 FF = JMP FF65)
        03FE - 03FF     Address of the subroutine which handles Interrupt
                        ReQuests (IRQ) (normally 65 FF = FF65)
        0400 - 07FF     Basically primary video text page
        0478 - 047F     I/O Scratchpad RAM Addresses for Slot 0 - 7
        04F8 - 04FF                  - " " -
        0578 - 057F                  - " " -
        05F8 - 05FF                  - " " -
        0678 - 067F                  - " " -
        06F8 - 06FF                  - " " -
        0778 - 077F                  - " " -
        07F8 - 07FF                  - " " -
        05F8            Holds the slot number of the disk controller card from
                        which DOS was booted. ??? VERIFY...
        07F8            Holds the slot number (CX, X = Slot #) of the slot that
                        is currently active. ??? VERIFY...
        0800 - 0BFF     Secondary video text page
        0C00 - 1FFF     Plain RAM
        2000 - 3FFF     Primary video hires page (RAM)
        4000 - 5FFF     Secondary video hires page (RAM)
        6000 - BFFF     Plain RAM (Normally the OS is loaded into ~9C00 - BFFF)
        C000 - C00F     Keyboard data (C00X contains the character ASCII code of
                        the pressed key. The value is available at any C00X address)
        C010 - C01F     Clear Keyboard strobe
        C020 - C02F     Cassette output toggle
        C030 - C03F     Speaker toggle
        C040 - C04F     Utility strobe
        C050            Set graphics mode
        C051            Set text mode
        C052            Set all text or graphics
        C053            Mix text and graphics
        C054            Display primary page
        C055            Display secondary page
        C056            Display low-res graphics
        C057            Display hi-res graphics
        C058 - C05F     Annunciator outputs
        C060            Cassette input
        C061 - C063     Pushbutton inputs (button 1, 2 and 3)
        C064 - C067     Game controller inputs
        C068 - C06F     Same as C060 - C067
        C070 - C07F     Game controller strobe
        C080 - C08F     Slot 0 I/O space (usually a language card)
        C080            Reset language card
                            * Read enabled
                            * Write disabled
                            * Read from language card
        C081            --- First access
                            * Read mode disabled
                            * Read from ROM
                        --- On second+ access
                            * Write mode enabled
                            * Write to language card
        C082            --- (Disable language card)
                            * Read mode disabled
                            * Write mode disabled
                            * Read from ROM
        C083            --- First access
                            * Read mode enabled
                            * Read from language card
                        --- On second+ access
                            * Write mode enabled
                            * Write to language card
        C088 - C08B     Same as C080 - C083, but switch to second bank, i.e.
                        map addresses D000-DFFF to other 4K area.
        C100 - C1FF     Slot 1 PROM
        C200 - C2FF     Slot 2 PROM
        C300 - C3FF     Slot 3 PROM
        C400 - C4FF     Slot 4 PROM
        C500 - C5FF     Slot 5 PROM
        C600 - C6FF     Slot 6 PROM
        C700 - C7FF     Slot 7 PROM
        C800 - CFFF     Expansion ROM (for peripheral cards)
        CFFF            Disable access to expansion ROM for
                        ALL peripheral cards.
        D000 - DFFF     ROM or 4K RAM if language card is enabled. However,
                        there are TWO 4K banks that can be mapped onto addresses
                        D000 - DFFF.  See C088 - C08B.
        E000 - FFFF     ROM or 8K RAM if language card is enabled.
---------------------------------------------------------------------------- */

#include "common.h"

#if VM_TRACING
FILE *test_vm_fp = NULL;
#endif

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
#ifdef __APPLE__
    return arc4random_uniform(0x100);
#else
    static time_t seed=0;
    if (!seed) {
        seed = time(NULL);
        srandom(seed);
    }
    return (random() % UINT_MAX);
#endif
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

GLUE_C_READ(iie_check_page2)
{
    return (softswitches & SS_PAGE2) ? 0x80 : 0x00;
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

GLUE_C_READ(iie_check_text)
{
    return (softswitches & SS_TEXT) ? 0x80 : 0x00;
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

GLUE_C_READ(iie_check_mixed)
{
    return (softswitches & SS_MIXED) ? 0x80 : 0x00;
}

GLUE_C_READ(iie_annunciator_noop)
{
    return 0x0;// TBD : mem_floating_bus()
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
        gc_cycles_timer_0 = (int)((joy_x-5) * JOY_STEP_CYCLES);
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

GLUE_C_READ(iie_check_vbl)
{
    // HACK FIXME TODO : enable vertical blanking timing/detection */
    return 0x0;
}

GLUE_C_READ(iie_c3rom_peripheral)
{
    softswitches &= ~SS_C3ROM;
    if (!(softswitches & SS_CXROM)) {
        base_c3rom = apple_ii_64k[0];
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

typedef uint8_t *(VMFunc)(uint16_t);

GLUE_C_READ(iie_cxrom_peripheral)
{
    softswitches &= ~SS_CXROM;
    base_cxrom = apple_ii_64k[0];
#ifdef AUDIO_ENABLED
    extern VMFunc MB_Read;
    base_c4rom = (void*)MB_Read;
    base_c5rom = (void*)MB_Read;
#endif
    if (!(softswitches & SS_C3ROM)) {
        base_c3rom = apple_ii_64k[0];
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
    return apple_ii_64k[1][ea];
}

GLUE_C_READ(iie_disable_slot_expansion)
{
    // HACK TODO FIXME : how does the expansion slot get referenced?  Need to handle other ROMs that might use this area
    // ... Also Need moar tests ...
    return 0x0;
}

// ----------------------------------------------------------------------------

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
void vm_begin_trace(const char *vm_file) {
    if (vm_file) {
        if (test_vm_fp) {
            vm_end_trace();
        }
        test_vm_fp = fopen(vm_file, "w");
    }
}

void vm_end_trace(void) {
    if (test_vm_fp) {
        fflush(test_vm_fp);
        fclose(test_vm_fp);
        test_vm_fp = NULL;
    }
}

void vm_toggle_trace(const char *vm_file) {
    if (test_vm_fp) {
        vm_end_trace();
    } else {
        vm_begin_trace(vm_file);
    }
}
#endif

