/* 
 * Apple // emulator for Linux: Miscellaneous support
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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "misc.h"
#include "video.h"
#include "disk.h"
#include "interface.h"
#include "keys.h"
#ifdef DEBUGGER
#include "debug.h"
#endif
#include "cpu.h"
#include "glue.h"
#include "prefs.h"

const struct itimerval timer_off = {{0,0},{0,0}};
const struct itimerval timer_on = {{0,TIMER_DELAY},{0,TIMER_DELAY}};

/* ----------------------------------
    internal apple2 variables
   ---------------------------------- */

static unsigned char	apple_ii_rom[12288];
#ifdef APPLE_IIE
static unsigned char	apple_iie_rom[32768];		/* //e */
#endif

#ifdef DEBUGGER
/* in debugger.c */
extern int breakpoints[];
extern int watchpoints[];
extern int op_breakpoints[256];
#endif

GLUE_FIXED_READ(read_ram_default,apple_ii_64k)
GLUE_FIXED_WRITE(write_ram_default,apple_ii_64k)
GLUE_BANK_READ(read_ram_bank,base_d000_rd)
GLUE_BANK_MAYBEWRITE(write_ram_bank,base_d000_wrt)
GLUE_BANK_READ(read_ram_lc,base_e000_rd)
GLUE_BANK_MAYBEWRITE(write_ram_lc,base_e000_wrt)

#ifdef APPLE_IIE

GLUE_BANK_READ(iie_read_ram_default,base_ramrd)
GLUE_BANK_WRITE(iie_write_ram_default,base_ramwrt)
GLUE_BANK_READ(iie_read_ram_text_page0,base_textrd)
GLUE_BANK_WRITE(iie_write_screen_hole_text_page0,base_textwrt)
GLUE_BANK_READ(iie_read_ram_hires_page0,base_hgrrd)
GLUE_BANK_WRITE(iie_write_screen_hole_hires_page0,base_hgrwrt)

GLUE_BANK_READ(iie_read_ram_zpage_and_stack,base_stackzp)
GLUE_BANK_WRITE(iie_write_ram_zpage_and_stack,base_stackzp)

GLUE_BANK_READ(iie_read_slot3,base_c3rom)
GLUE_BANK_READ(iie_read_slotx,base_cxrom)



/* -------------------------------------------------------------------------
    c_set_altchar() - set alternate character set
   ------------------------------------------------------------------------- */
void c_set_altchar()
{
    video_loadfont(0x40,0x20,mousetext_glyphs,1);
    video_loadfont(0x60,0x20,lcase_glyphs,2);

    video_redraw();
}

/* -------------------------------------------------------------------------
    c_set_primary_char() - set primary character set
   ------------------------------------------------------------------------- */
void c_set_primary_char() 
{
    video_loadfont(0x40,0x40,ucase_glyphs,3);

    video_redraw();
}

#endif/* APPLE_IIE */


/* -------------------------------------------------------------------------
    c_initialize_font():     Initialize ROM character table to primary char set
   ------------------------------------------------------------------------- */
void c_initialize_font()
{ 
    video_loadfont(0x00,0x40,ucase_glyphs,2);
    video_loadfont(0x40,0x40,ucase_glyphs,3);
    video_loadfont(0x80,0x40,ucase_glyphs,0);   
    video_loadfont(0xC0,0x20,ucase_glyphs,0);
    video_loadfont(0xE0,0x20,lcase_glyphs,0);

    video_redraw();
}



/* -------------------------------------------------------------------------
    c_initialize_tables()
   ------------------------------------------------------------------------- */

void c_initialize_tables() {

    int i;

    pre_compact(); /* Prepare for VM compression */

    /* reset everything */
    for (i = 0; i < 0x10000; i++) {
#ifdef APPLE_IIE
	if (apple_mode == IIE_MODE) {
	    cpu65_vmem[i].r = iie_read_ram_default;
	    cpu65_vmem[i].w = iie_write_ram_default;
	} else
#endif
        {
	    cpu65_vmem[i].r = read_ram_default;
	    cpu65_vmem[i].w = write_ram_default;
	}
    }

    /* language card read/write area */
    for (i = 0xD000; i < 0xE000; i++) {
	{
	    cpu65_vmem[i].w =
		write_ram_bank;
	    cpu65_vmem[i].r =
		read_ram_bank;
	}
    }
    for (i = 0xE000; i < 0x10000; i++) {
	{
	    cpu65_vmem[i].w =
		write_ram_lc;
	    cpu65_vmem[i].r =
		read_ram_lc;
	}
    }
    /* done common initialization */

#ifdef APPLE_IIE
    /* initialize zero-page, //e specific */
    if (apple_mode == IIE_MODE) {
	for (i = 0; i < 0x200; i++) {
	    cpu65_vmem[i].r =
		iie_read_ram_zpage_and_stack;
	    cpu65_vmem[i].w =
		iie_write_ram_zpage_and_stack;
	}
    }
#endif

#ifdef APPLE_IIE
    /* initialize first text & hires page, which are specially bank switched 
     *
     * video_set() substitutes it's own hooks for all visible write locations 
     * affect the display, leaving our write-functions in place only at the
     * `screen holes', hence the name.
     */
    for (i = 0x400; i < 0x800; i++) {
	cpu65_vmem[i].r =
	    iie_read_ram_text_page0;
	cpu65_vmem[i].w =
	    iie_write_screen_hole_text_page0;
    }
    for (i = 0x2000; i < 0x4000; i++) {
	cpu65_vmem[i].r =
	    iie_read_ram_hires_page0;
	cpu65_vmem[i].w =
	    iie_write_screen_hole_hires_page0;
    }
#endif

    /* softswich rom */
    for (i = 0xC000; i < 0xC100; i++) {
	cpu65_vmem[i].r =
	    read_unmapped_softswitch;
	cpu65_vmem[i].w =
	    write_unmapped_softswitch;
    }

    /* slot rom */
    for (i = 0xC100; i < 0xD000; i++) {
	cpu65_vmem[i].r =
#ifdef APPLE_IIE
	    iie_read_ram_default;
#else
	    read_ram_default;
#endif
	cpu65_vmem[i].w =
	    ram_nop;
    }

    /* keyboard data and strobe (READ) */
    for (i = 0xC000; i < 0xC010; i++) {
	cpu65_vmem[i].r =
	    read_keyboard;
    }
    for (i = 0xC010; i < 0xC020; i++) {
	cpu65_vmem[i].r =
	cpu65_vmem[i].w =
	    read_keyboard_strobe;
    }

#ifdef APPLE_IIE
    if (apple_mode == IIE_MODE) {

	/* RDBNK2 switch */
	cpu65_vmem[0xC011].r =
	    iie_check_bank;

	/* RDLCRAM switch */
	cpu65_vmem[0xC012].r =
	    iie_check_lcram;

	/* 80STORE switch */
	cpu65_vmem[0xC000].w = iie_80store_off;
	cpu65_vmem[0xC001].w = iie_80store_on;
	cpu65_vmem[0xC018].r = iie_check_80store;

	/* RAMRD switch */
	cpu65_vmem[0xC002].w = iie_ramrd_main;
	cpu65_vmem[0xC003].w = iie_ramrd_aux;
	cpu65_vmem[0xC013].r = iie_check_ramrd;

	/* RAMWRT switch */
	cpu65_vmem[0xC004].w = iie_ramwrt_main;
	cpu65_vmem[0xC005].w = iie_ramwrt_aux;
	cpu65_vmem[0xC014].r = iie_check_ramwrt;

	/* ALTZP switch */
	cpu65_vmem[0xC008].w = iie_altzp_main;
	cpu65_vmem[0xC009].w = iie_altzp_aux;
	cpu65_vmem[0xC016].r = iie_check_altzp;

	/* 80COL switch */
	cpu65_vmem[0xC00C].w = iie_80col_off;
	cpu65_vmem[0xC00D].w = iie_80col_on;
	cpu65_vmem[0xC01F].r = iie_check_80col;

	/* ALTCHAR switch */
	cpu65_vmem[0xC00E].w = iie_altchar_off;
	cpu65_vmem[0xC00F].w = iie_altchar_on;
	cpu65_vmem[0xC01E].r = iie_check_altchar;

	/* SLOTC3ROM switch */
	cpu65_vmem[0xC00A].w = iie_c3rom_internal;
	cpu65_vmem[0xC00B].w = iie_c3rom_peripheral;
	cpu65_vmem[0xC017].r = iie_check_c3rom;

	/* SLOTCXROM switch */
	cpu65_vmem[0xC006].w = iie_cxrom_peripheral;
	cpu65_vmem[0xC007].w = iie_cxrom_internal;
	cpu65_vmem[0xC015].r = iie_check_cxrom;

	/* RDVBLBAR switch */
	cpu65_vmem[0xC019].r =
	    iie_check_vbl;
    }
#endif
    
    /* random number generator */
    for (i = 0xC020; i < 0xC030; i++)
	cpu65_vmem[i].r =
	cpu65_vmem[i].w =
	    read_random;

    /* TEXT switch */
    cpu65_vmem[0xC050].r =
    cpu65_vmem[0xC050].w =
	read_switch_graphics;
    cpu65_vmem[0xC051].r =
    cpu65_vmem[0xC051].w =
	read_switch_text;

#ifdef APPLE_IIE
    if (apple_mode == IIE_MODE) {
	cpu65_vmem[0xC01A].r =
	    iie_check_text;
    }
#endif

    /* MIXED switch */
    cpu65_vmem[0xC052].r =
    cpu65_vmem[0xC052].w =
	read_switch_no_mixed;
    cpu65_vmem[0xC053].r =
    cpu65_vmem[0xC053].w =
	read_switch_mixed;

#ifdef APPLE_IIE
    if (apple_mode == IIE_MODE) {
	cpu65_vmem[0xC01B].r =
	    iie_check_mixed;
    }
    /* PAGE2 switch */
    if (apple_mode == IIE_MODE) {
	cpu65_vmem[0xC054].r =
	cpu65_vmem[0xC054].w =
	    iie_page2_off;
    } else
#endif
    {
	cpu65_vmem[0xC054].r =
	cpu65_vmem[0xC054].w =
	    read_switch_primary_page;
    }

#ifdef APPLE_IIE
    if (apple_mode == IIE_MODE) {
	cpu65_vmem[0xC01C].r =
	    iie_check_page2;
    }
    /* PAGE2 switch */
    if (apple_mode == IIE_MODE) {
	cpu65_vmem[0xC055].r =
	cpu65_vmem[0xC055].w =
	    iie_page2_on;
    } else
#endif
    {
	cpu65_vmem[0xC055].r =
	cpu65_vmem[0xC055].w =
	    read_switch_secondary_page;
    }

    /* HIRES switch */
#ifdef APPLE_IIE
    if (apple_mode == IIE_MODE) {
	cpu65_vmem[0xC01D].r =
	    iie_check_hires;
	cpu65_vmem[0xC056].r =
	cpu65_vmem[0xC056].w =
	    iie_hires_off;
	cpu65_vmem[0xC057].r =
	cpu65_vmem[0xC057].w =
	    iie_hires_on;
    }
    else
#endif
    {
	cpu65_vmem[0xC056].r =
	cpu65_vmem[0xC056].w =
	    read_switch_lores;
	cpu65_vmem[0xC057].r =
	cpu65_vmem[0xC057].w =
	    read_switch_hires;
    }

    /* game I/O switches */
    cpu65_vmem[0xC061].r =
    cpu65_vmem[0xC069].r =
        read_button0;
    cpu65_vmem[0xC062].r =
    cpu65_vmem[0xC06A].r =
        read_button1;
    cpu65_vmem[0xC063].r =
    cpu65_vmem[0xC06B].r =
        read_button2;
    cpu65_vmem[0xC064].r =
    cpu65_vmem[0xC06C].r =
        read_gc0;
    cpu65_vmem[0xC065].r =
    cpu65_vmem[0xC06D].r =
        read_gc1;
#ifdef APPLE_IIE
    if (apple_mode == IIE_MODE) {
	cpu65_vmem[0xC066].r =
	    iie_read_gc2;
	cpu65_vmem[0xC067].r =
	    iie_read_gc3;
    }
#endif
    for (i = 0xC070; i < 0xC080; i++)
	cpu65_vmem[i].r =
	cpu65_vmem[i].w =
	    read_gc_strobe;

#ifdef APPLE_IIE
    if (apple_mode == IIE_MODE) {
	/* IOUDIS switch & read_gc_strobe */
	cpu65_vmem[0xC07E].w =
	    iie_ioudis_on;
	cpu65_vmem[0xC07F].w =
	    iie_ioudis_off;
	cpu65_vmem[0xC07E].r =
	    iie_check_ioudis;
	cpu65_vmem[0xC07F].r =
	    iie_check_dhires;

	/* DHIRES/Annunciator switches */
	cpu65_vmem[0xC05E].w =
	cpu65_vmem[0xC05E].r =
	    iie_dhires_on;
	cpu65_vmem[0xC05F].w =
	cpu65_vmem[0xC05F].r =
	    iie_dhires_off;
    }
#endif

#ifdef APPLE_IIE 
    /* language card softswitches */
    cpu65_vmem[0xC080].r = cpu65_vmem[0xC080].w =
    cpu65_vmem[0xC084].r = cpu65_vmem[0xC084].w =
	(apple_mode == IIE_MODE) ? iie_c080 : lc_c080;
    cpu65_vmem[0xC081].r = cpu65_vmem[0xC081].w =
    cpu65_vmem[0xC085].r = cpu65_vmem[0xC085].w =
	(apple_mode == IIE_MODE) ? iie_c081 : lc_c081;
    cpu65_vmem[0xC082].r = cpu65_vmem[0xC082].w =
    cpu65_vmem[0xC086].r = cpu65_vmem[0xC086].w =
	lc_c082;
    cpu65_vmem[0xC083].r = cpu65_vmem[0xC083].w =
    cpu65_vmem[0xC087].r = cpu65_vmem[0xC087].w =
	(apple_mode == IIE_MODE) ? iie_c083 : lc_c083;

    cpu65_vmem[0xC088].r = cpu65_vmem[0xC088].w =
    cpu65_vmem[0xC08C].r = cpu65_vmem[0xC08C].w =
	(apple_mode == IIE_MODE) ? iie_c088 : lc_c088;
    cpu65_vmem[0xC089].r = cpu65_vmem[0xC089].w =
    cpu65_vmem[0xC08D].r = cpu65_vmem[0xC08D].w =
	(apple_mode == IIE_MODE) ? iie_c089 : lc_c089;
    cpu65_vmem[0xC08A].r = cpu65_vmem[0xC08A].w =
    cpu65_vmem[0xC08E].r = cpu65_vmem[0xC08E].w =
	lc_c08a;
    cpu65_vmem[0xC08B].r = cpu65_vmem[0xC08B].w =
    cpu65_vmem[0xC08F].r = cpu65_vmem[0xC08F].w =
	(apple_mode == IIE_MODE) ? iie_c08b : lc_c08b;
#else /* !APPLE_IIE */
    /* language card softswitches */
    cpu65_vmem[0xC080].r = cpu65_vmem[0xC080].w =
    cpu65_vmem[0xC084].r = cpu65_vmem[0xC084].w =
	lc_c080;
    cpu65_vmem[0xC081].r = cpu65_vmem[0xC081].w =
    cpu65_vmem[0xC085].r = cpu65_vmem[0xC085].w =
	lc_c081;
    cpu65_vmem[0xC082].r = cpu65_vmem[0xC082].w =
    cpu65_vmem[0xC086].r = cpu65_vmem[0xC086].w =
	lc_c082;
    cpu65_vmem[0xC083].r = cpu65_vmem[0xC083].w =
    cpu65_vmem[0xC087].r = cpu65_vmem[0xC087].w =
	lc_c083;

    cpu65_vmem[0xC088].r = cpu65_vmem[0xC088].w =
    cpu65_vmem[0xC08C].r = cpu65_vmem[0xC08C].w =
	lc_c088;
    cpu65_vmem[0xC089].r = cpu65_vmem[0xC089].w =
    cpu65_vmem[0xC08D].r = cpu65_vmem[0xC08D].w =
	lc_c089;
    cpu65_vmem[0xC08A].r = cpu65_vmem[0xC08A].w =
    cpu65_vmem[0xC08E].r = cpu65_vmem[0xC08E].w =
	lc_c08a;
    cpu65_vmem[0xC08B].r = cpu65_vmem[0xC08B].w =
    cpu65_vmem[0xC08F].r = cpu65_vmem[0xC08F].w =
	lc_c08b;
#endif /* !APPLE_IIE */

#ifdef APPLE_IIE
    /* slot i/o area */
    for (i = 0xC100; i < 0xC300; i++) {
	cpu65_vmem[i].r =
	    iie_read_slotx;		/* slots 1 & 2 (x) */
    }
    for (i = 0xC300; i < 0xC400; i++) {
	cpu65_vmem[i].r =
	    iie_read_slot3;		/* slot 3 (80col) */
    }
    for (i = 0xC400; i < 0xC800; i++) {
	cpu65_vmem[i].r =
	    iie_read_slotx;		/* slots 4 - 7 (x) */
    }
    for (i = 0xC800; i < 0xD000; i++) {
	cpu65_vmem[i].r =
	    iie_read_slot_expansion;	/* expansion rom */
    }
    cpu65_vmem[0xCFFF].r =
    cpu65_vmem[0xCFFF].w =
	iie_disable_slot_expansion;
#endif

    video_set(0); /* must be done here, between pre_compact & compact */
    
    disk_install(6); /* Put a Disk ][ Controller in slot 6 */

    compact(); /* Compress memory so that identical pages share storage */

}

/* -------------------------------------------------------------------------
    c_initialize_apple_ii_memory()
   ------------------------------------------------------------------------- */

void c_initialize_apple_ii_memory()
{
     FILE	*f;
     int	i;
     static int ii_rom_loaded = 0;
#ifdef APPLE_IIE
     static int iie_rom_loaded = 0;
#endif

     for (i = 0; i < 0x10000; i++) {
         apple_ii_64k[0][i] = 0;
         apple_ii_64k[1][i] = 0;
     }
     for (i = 0; i < 8192; i++)
         language_card[0][i] = language_card[1][i] = 0;
     for (i = 0; i < 8192; i++)
         language_banks[0][i] = language_banks[1][i] = 0;

     if (!ii_rom_loaded)
     {
         snprintf(temp, TEMPSIZE, "%s/apple_II.rom", system_path);
         if ((f = fopen(temp, "r")) == NULL) {
	     printf("OOPS!\n");
             printf("Cannot find file '%s'.\n",temp);
	     exit(0);
         }
         fread(apple_ii_rom, 0x3000, 1, f);
         fclose(f);
	 ii_rom_loaded = 1;
     }

#ifdef APPLE_IIE
     if (!iie_rom_loaded) {
	 snprintf(temp, TEMPSIZE, "%s/apple_IIe.rom", system_path);
	 if ((f = fopen(temp, "r")) == NULL) {
	     printf("Cannot find file '%s'.\n",temp);
	     exit(0);
	 }
	 fread(apple_iie_rom, 32768, 1, f);
	 fclose(f);
	 iie_rom_loaded = 1;
     }
#endif

     for (i = 0xD000; i < 0x10000; i++)
	 apple_ii_64k[0][i] = apple_ii_rom[i - 0xD000];
     for (i = 0; i < 0x1000; i++)
	 language_banks[0][i] = apple_ii_rom[i];
     for (i = 0; i < 0x2000; i++)
	 language_card[0][i] = apple_ii_rom[i + 0x1000];

#ifdef APPLE_IIE
     if (apple_mode == IIE_MODE) {
	 /* load the rom from 0xC000, slot rom main, internal rom aux */
	 for (i = 0xC000; i < 0x10000; i++) {
	     apple_ii_64k[0][i] = apple_iie_rom[i - 0xC000];
	     apple_ii_64k[1][i] = apple_iie_rom[i - 0x8000];
	 }
	 for (i = 0; i < 0x1000; i++) {
	     language_banks[0][i] = apple_iie_rom[i + 0x1000];
	     language_banks[1][i] = apple_iie_rom[i + 0x5000];
	 }
	 for (i = 0; i < 0x2000; i++) {
	     language_card[0][i] = apple_iie_rom[i + 0x2000];
	     language_card[1][i] = apple_iie_rom[i + 0x6000];
	 }
     }
     else
#endif
     /* softswitch memory HACK - why this? */
     {
	 for (i = 0xC100; i < 0xD000; i++) {
	     apple_ii_64k[0][i] = i & 0xFF;
	     apple_ii_64k[1][i] = i & 0xFF;
	 }
     }

     apple_ii_64k[0][0xC000] = 0x00;
     apple_ii_64k[1][0xC000] = 0x00;
}

/* -------------------------------------------------------------------------
    void c_initialize_sound()
   ------------------------------------------------------------------------- */

void c_initialize_sound()
{
    int i;

    for (i = 0xC030; i < 0xC040; i++)
        cpu65_vmem[i].r = cpu65_vmem[i].w =
	    (sound_mode && soundAllowed) ? read_speaker_toggle_pc : ram_nop;
}

#ifdef APPLE_IIE
/* -------------------------------------------------------------------------
    c_initialize_iie_switches
   ------------------------------------------------------------------------- */
void c_initialize_iie_switches() {

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

    base_c3rom = apple_ii_64k[1];		/* c3rom internal */
    c8rom_offset = 0x10000;			/* c8rom internal */
    base_cxrom = apple_ii_64k[0];		/* cxrom peripheral */
}
#endif

/* -------------------------------------------------------------------------
    void c_initialize_vm()
   ------------------------------------------------------------------------- */
void c_initialize_vm() {
    c_initialize_font();		/* font already read in */
    c_initialize_apple_ii_memory();	/* read in rom memory */
    c_initialize_tables();		/* read/write memory jump tables */
    c_initialize_sound();		/* sound system */
    c_init_6();				/* drive ][, slot 6 */

#ifdef APPLE_IIE
    c_initialize_iie_switches();	/* set the //e softswitches */
#endif

#ifdef MOUSE_EMULATION
    c_initialize_mouse();
#endif
}

/* -------------------------------------------------------------------------
    void c_initialize_firsttime()
   ------------------------------------------------------------------------- */

static void reinitialize(void)
{
    int i;

#ifdef DEBUGGER
	/* reset the watchpoints and breakpoints */
	for (i=0; i<MAX_BRKPTS; i++) {
	    breakpoints[i] = -1;
	    watchpoints[i] = -1;
	}
	for (i=0; i<0x100; i++) {
	    op_breakpoints[(unsigned char)i] = 0;
	}
#endif

	c_initialize_vm();

	softswitches = SS_TEXT | SS_IOUDIS | SS_C3ROM | SS_LCWRT | SS_LCSEC;

	video_setpage( 0 );

	video_redraw();

#ifdef APPLE_IIE
	if (apple_mode == IIE_MODE)
	    cpu65_set(CPU65_C02);
	else
#endif
	if (apple_mode == IIU_MODE)
	    cpu65_set(CPU65_NMOS);
	else
	    cpu65_set(CPU65_NMOS|CPU65_FAULT);

}

static void c_initialize_firsttime()
{
    /* get IO permission for speaker port. */
    if (/*ioperm(0x42, 1, 1) ||*/ ioperm(0x61, 1, 1)) {
	perror("ioperm");
	printf("cannot get port access to PC speaker.\n");
	printf("sound will not be used.\n");
	soundAllowed=0;
    } else
	soundAllowed=1;

    /* read in system files and calculate system defaults */
    c_load_interface_font();

    /* initialize the video system */
    video_init();

    /* Enable periodic updates */
    signal(SIGVTALRM,c_periodic_update);
    setitimer(ITIMER_VIRTUAL,&timer_on,0); 

    reinitialize();     
}

void c_read_random() {
    random_value = (unsigned char)(rand() >> 8);
}

int main(int sargc, char *sargv[])
{
    int		i;

    argc = sargc;
    argv = sargv;

    for (i = 1; i < argc; i++)
    {
/*
	if (strcmp(argv[i], "-vga") == 0)
	    force_vga_mode = 1;
*/
    }

    load_settings();			/* user prefs */
    c_initialize_firsttime();		/* init svga graphics and vm */

    for (;;) {
	/* execute the emulator */
        cpu65_run();

	reinitialize();
    }
    /* never reached */
}
