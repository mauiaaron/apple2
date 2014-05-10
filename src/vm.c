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
// Softswitches

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

