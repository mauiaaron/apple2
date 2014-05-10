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

