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

#include "greatest.h"
#include "testcommon.h"

void flags_to_string(uint8_t flags, char *buf) {
    snprintf(buf, MSG_SIZE, "%c%c%c%c%c%c%c%c",
        (flags & N_Flag_6502) ? 'N' : '-',
        (flags & V_Flag_6502) ? 'V' : '-',
        (flags & X_Flag_6502) ? 'X' : '-',
        (flags & B_Flag_6502) ? 'B' : '-',
        (flags & D_Flag_6502) ? 'D' : '-',
        (flags & I_Flag_6502) ? 'I' : '-',
        (flags & Z_Flag_6502) ? 'Z' : '-',
        (flags & C_Flag_6502) ? 'C' : '-' );
}

extern SUITE(test_suite_cpu);
//extern SUITE(test_suite_memory);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_cpu);
    //RUN_SUITE(test_suite_memory);
    GREATEST_MAIN_END();
}

