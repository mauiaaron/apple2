/*
 * Apple // emulator for Linux: Miscellaneous defines
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#ifndef MISC_H
#define MISC_H

#ifndef __ASSEMBLER__

#include "common.h"
#include <unistd.h>
#include <sys/types.h>

#define SW_TEXT 0xC050
#define SW_MIXED 0xC052
#define SW_PAGE2 0xC054
#define SW_HIRES 0xC056
#define SW_80STORE 0xC000
#define SW_RAMRD 0xC002
#define SW_RAMWRT 0xC004
#define SW_ALTZP 0xC008
#define SW_80COL 0xC00C
#define SW_ALTCHAR 0xC00E
#define SW_SLOTC3ROM 0xC00B     /* anomaly */
#define SW_SLOTCXROM 0xC006
#define SW_DHIRES 0xC05E
#define SW_IOUDIS 0xC07E

/* Text characters */
extern const unsigned char ucase_glyphs[0x200];
extern const unsigned char lcase_glyphs[0x100];
extern const unsigned char mousetext_glyphs[0x100];
extern const unsigned char interface_glyphs[88];

unsigned char apple_ii_64k[2][65536]; /* 128k memory */

/* language card memory and settings */
unsigned char language_card[2][8192], language_banks[2][8192];

/* misc stuff */
uint8_t random_value;

/* global ref to commandline args */
char            **argv;
int argc;

/* misc arrays */
#define TEMPSIZE        4096
char temp[ TEMPSIZE ];   /* should be >=4096 (stuff depends on this) */

/* memory offsets from softswitches */
int c8rom_offset;

extern unsigned char *base_ramrd;
extern unsigned char *base_ramwrt;
extern unsigned char *base_textrd;
extern unsigned char *base_textwrt;
extern unsigned char *base_hgrrd;
extern unsigned char *base_hgrwrt;

extern unsigned char *base_stackzp;
extern unsigned char *base_d000_rd;
extern unsigned char *base_e000_rd;
extern unsigned char *base_d000_wrt;
extern unsigned char *base_e000_wrt;

extern unsigned char *base_c3rom;
extern unsigned char *base_cxrom;

/* softswitches */

extern int softswitches;

#endif /* !__ASSEMBLER__ */

#define         SS_TEXT         0x00000001
#define         SS_MIXED        0x00000002
#define         SS_HIRES        0x00000004
#define         SS_PAGE2        0x00000008
#define         SS_BANK2        0x00000010
#define         SS_LCRAM        0x00000020
#define         SS_LCSEC        0x00000040 /* check for double read */
#define         SS_LCWRT        0x00000080 /* LC write enable */
#define         SS_80STORE      0x00000100
#define         SS_80COL        0x00000200
#define         SS_RAMRD        0x00000400
#define         SS_RAMWRT       0x00000800
#define         SS_ALTZP        0x00001000
#define         SS_DHIRES       0x00002000
#define         SS_IOUDIS       0x00004000
#define         SS_CXROM        0x00008000
#define         SS_C3ROM        0x00010000
#define         SS_ALTCHAR      0x00020000

/* Pseudo soft switches.  These are actually functions of other SSes, but are
 * tiresome to calculate as needed.
 *
 */
#define         SS_SCREEN       0x00040000 /* PAGE2 && !80STORE */
#define         SS_TEXTRD       0x00080000 /* (PAGE2 && 80STORE) ||
                                              (RAMRD && !80STORE) */
#define         SS_TEXTWRT      0x00100000 /* (PAGE2 && 80STORE) ||
                                              (RAMWRT && !80STORE) */
#define         SS_HGRRD        0x00200000 /* (PAGE2 && 80STORE && HIRES) ||
                                              (RAMRD && !(80STORE && HIRES) */
#define         SS_HGRWRT       0x00400000 /* (PAGE2 && 80STORE && HIRES) ||
                                              (RAMWRT && !(80STORE && HIRES)) */

#ifndef __ASSEMBLER__
/* -------------------------------------------------------------------------
    misc.c functions
   ------------------------------------------------------------------------- */

void c_initialize_sound_hooks();
void c_disable_sound_hooks();
void c_initialize_font();
void c_initialize_vm();
void c_read_random();
void reinitialize();

/* virtual memory compacter */

void pre_compact(void);
void compact(void);

/* vm hooks */

void            ram_nop(),

write_ram_default(),
write_unmapped_softswitch(),

read_ram_default(),
read_random(),
read_unmapped_softswitch(),
read_keyboard(),
read_keyboard_strobe(),
read_speaker_toggle_pc(),
read_switch_primary_page(),
read_switch_secondary_page(),
read_switch_graphics(),
read_switch_text(),
read_switch_no_mixed(),
read_switch_mixed(),
read_switch_lores(),
read_switch_hires(),

read_button0(),
read_button1(),
read_button2(),
read_gc0(),
read_gc1(),
read_gc_strobe(),

lc_c080(),
lc_c081(),
lc_c082(),
lc_c083(),
lc_c088(),
lc_c089(),
lc_c08a(),
lc_c08b(),
write_ram_bank(),
read_ram_bank(),
write_ram_lc(),
read_ram_lc();

void            iie_write_ram_default(),
iie_read_ram_default(),

/* //e text pages */
iie_read_ram_text_page0(),
iie_write_screen_hole_text_page0(),

/* //e hires page 0 */
iie_read_ram_hires_page0(),
iie_write_screen_hole_hires_page0(),

/* //e zpage,stack, ram banks */
iie_read_ram_zpage_and_stack(),
iie_write_ram_zpage_and_stack(),
iie_read_slot3(),
iie_read_slotx(),
iie_read_slot_expansion(),
iie_disable_slot_expansion(),
iie_read_gc2(),
iie_read_gc3(),

iie_c080(),
iie_c081(),
iie_c083(),
iie_c088(),
iie_c089(),
iie_c08b(),

/* //e toggle softswitches */
iie_ramrd_main(),
iie_ramrd_aux(),
iie_ramwrt_main(),
iie_ramwrt_aux(),
iie_80store_off(),
iie_80store_on(),
iie_altzp_main(),
iie_altzp_aux(),
iie_80col_off(),
iie_80col_on(),
iie_altchar_off(),
iie_altchar_on(),
iie_c3rom_peripheral(),
iie_c3rom_internal(),
iie_cxrom_peripheral(),
iie_cxrom_internal(),
iie_ioudis_on(),
iie_ioudis_off(),
iie_dhires_on(),
iie_dhires_off(),
iie_hires_off(),
iie_hires_on(),
iie_page2_on(),
iie_page2_off(),

/* //e check softswitche settings */
iie_check_80store(),
iie_check_bank(),
iie_check_lcram(),
iie_check_ramrd(),
iie_check_ramwrt(),
iie_check_altzp(),
iie_check_c3rom(),
iie_check_cxrom(),
iie_check_80col(),
iie_check_altchar(),
iie_check_text(),
iie_check_mixed(),
iie_check_hires(),
iie_check_page2(),
iie_check_ioudis(),
iie_check_dhires(),
iie_check_vbl();
#endif /* !__ASSEMBLER__ */

#endif
