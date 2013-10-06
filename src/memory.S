/* 
 * Apple // emulator for Linux: 
 *   Functions for memory-access indirection table.
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

#include "apple2.h"
#include "cpu.h"
#include "misc.h"

	/* --------------------------------------------------------------------
	   ....................................................................
	    Apple ][+ Memory documentation (//e differs mostly in softswitches)
	  
	    I use two jump tables; one for get_memory and one for set_memory
	    respectively. The jump tables contain exactly 64k entries to
            avoid unnecessary range checking. Furthermore, special access
	    or to serve memory mapped I/O services are easily obtained by
	    patching the jump table with various routines.
	  
	    Apple 64k address space:
	  
	    (Two byte addresses are represented with least significant
	     byte first, e.g. address FA59 is represented as 59 FA)
	  
	    Address		Description
	  
	    0000 - 00FF   	Zero page RAM
	    0100 - 01FF		Stack
	    0200 - 03EF		RAM
	    03F0 - 03F1		Address for BRK instruction
	                        (normally 59 FA = FA59)
	    03F2 - 03F3         Soft entry vector (normally BF 9D = 9DBF)
	    03F4                Power-up byte
	    03F5 - 03F7         Jump instruction to the subroutine which
	                        handles Applesoft II &-commands
	   			(normally 4C 58 FF = JMP FF58)
	    03F8 - 03FA		Jump instruction to the subroutine which
	                        handles User CTRL-Y commands
	                        (normally 4C 65 FF = JMP FF65)
	    03FB - 03FD		Jump instruction to the subroutine which
	                        handles Non-Maskable Interrupts (NMI)
	                        (normally 4C 65 FF = JMP FF65)
	    03FE - 03FF         Address of the subroutine which handles
	                        Interrupt ReQuests (IRQ)
	                        (normally 65 FF = FF65)
	    0400 - 07FF         Basically primary video text page
	    0478 - 047F		I/O Scratchpad RAM Addresses for Slot 0 - 7
	    04F8 - 04FF		               - " " -
	    0578 - 057F		               - " " -
	    05F8 - 05FF		               - " " -
	    0678 - 067F		               - " " -
	    06F8 - 06FF		               - " " -
	    0778 - 077F		               - " " -
	    07F8 - 07FF		               - " " -
	    ----- These two addresses are pretty strange; the values
	          provided were inconsistent at least on my Apple II
	    05F8		Holds the slot number of the disk
	  			controller card from which DOS was
	                        booted.
	    07F8		Holds the slot number (CX, X = Slot #)
	                        of the slot that is currently active.
	  
	    0800 - 0BFF		Basically secondary video text page
	  
	    0C00 - 1FFF		Plain RAM
	    2000 - 3FFF		Primary video hires page (RAM)
	    4000 - 5FFF         Secondary video Hi-Res page (RAM)
	    6000 - BFFF		Plain RAM
	                        (Normally the operating system is
	                         loaded into ~= 9C00 - BFFF)
	    C000 - C00F		Keyboard data
	                        (C00X contains the character ASCII
	                         code of the pressed key. The
	                         value is available at any C00X
	                         address)
	    C010 - C01F         Clear Keyboard strobe
	    C020 - C02F         Cassette output toggle
	    C030 - C03F         Speaker toggle
	    C040 - C04F         Utility strobe
	    C050		Set graphics mode
	    C051		Set text mode
	    C052		Set all text or graphics
	    C053		Mix text and graphics
	    C054		Display primary page
	    C055		Display secondary page
	    C056		Display low-res graphics
	    C057		Display hi-res graphics
	    C058 - C05F		Annunciator outputs
	    C060		Cassette input
	    C061 - C063		Pushbutton inputs (button 1, 2 and 3)
	    C064 - C067		Game controller inputs
	    C068 - C06F		Same as C060 - C067
	    C070 - C07F		Game controller strobe
	    C080 - C08F		Slot 0 I/O space (usually a language card)
	                        --- If language card
	    C080		    Reset language card
	                            Read mode enabled
	  			    Write mode disabled
	                            Read from language card
	  			    Write to ROM (impossible of course)
	    C081		--- First access
	  			    Read mode disabled
	                            Read from ROM
	  			--- On second access
	                            Write mode enabled
	  			    Write to language card
	    C082		--- (Disable language card)
	                            Read mode disabled
	                            Write mode disabled
	                            Read from ROM
	                            Write to ROM
	    C083		--- First access
	                            Read mode enabled
	  			    Read from language card
	  		 	--- On second access
	  			    Write mode enabled
	  			    Write to language card
	    C088 - C08B		Same as C080 - C083, but
	  			switch to second bank, i.e.
	  			map addresses D000-DFFF to
	  			other 4k area.
	    C100 - C1FF		Slot 1 PROM
	    C200 - C2FF		Slot 2 PROM
	    C300 - C3FF		Slot 3 PROM
	    C400 - C4FF		Slot 4 PROM
	    C500 - C5FF		Slot 5 PROM
	    C600 - C6FF		Slot 6 PROM
	    C700 - C7FF		Slot 7 PROM
	    C800 - CFFF		Expansion ROM (for peripheral cards)
	    CFFF		Disable access to expansion ROM for
	  			ALL peripheral cards.
	    D000 - DFFF		ROM or 4k RAM if language card is
	  			enabled. However, there are TWO 4k
	  			banks that can be mapped onto addresses
	  			D000 - DFFF. See C088 - C08B.
	    E000 - FFFF		ROM or 8k RAM if language card is
	  			enabled.
	   ----------------------------------------------------------------- */


		.comm	SN(softswitches),4
		
		.comm	SN(base_ramrd),4
		.comm	SN(base_ramwrt),4
		.comm	SN(base_textrd),4
		.comm	SN(base_textwrt),4
		.comm	SN(base_hgrrd),4
		.comm	SN(base_hgrwrt),4

		.comm	SN(base_stackzp),4
		.comm	SN(base_d000_rd),4
		.comm	SN(base_d000_wrt),4
		.comm	SN(base_e000_rd),4
		.comm	SN(base_e000_wrt),4

		.comm	SN(base_cxrom),4
		.comm	SN(base_c3rom),4

/* ram_nop, read_ram_default, write_ram_default: default ram/rom
   read/write functions. */
E(ram_nop)
		ret

E(read_unmapped_softswitch)
		movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al
    		ret

E(write_unmapped_softswitch)
    		ret
	
E(read_keyboard)
		movb	SN(apple_ii_64k)+0xC000, %al
        	ret

E(read_keyboard_strobe)
		andb	$0x7f, SN(apple_ii_64k)+0xC000
		andb	$0x7f, SN(apple_ii_64k)+0x1C000
		movb	SN(apple_ii_64k)+0xC000, %al /* HACK: necessary? */
		ret

E(read_random)
                pushal
		call	SN(c_read_random)
                popal
		movb	SN(random_value), %al
		ret

E(read_speaker_toggle_pc)
                pushal
		call	SN(SpkrToggle)
                popal // FIXME, do we need to set %al to something here?
		ret

E(read_switch_primary_page)
		testl	$SS_PAGE2, SN(softswitches)
		jnz	_read_switch_primary
		ret
_read_switch_primary:
		andl	$~(SS_PAGE2|SS_SCREEN), SN(softswitches)
		pushal
		pushl	$0
		call	SN(video_setpage)
		addl	$4, %esp
		popal
		ret

E(read_switch_secondary_page)
		testl	$SS_PAGE2, SN(softswitches)
		jz	_read_switch_secondary
		ret
_read_switch_secondary:
		orl	$(SS_PAGE2|SS_SCREEN), SN(softswitches)
		pushal
		pushl	$1
		call	SN(video_setpage)
		addl	$4, %esp
		popal
		ret

/* PAGE2 off. if 80STORE on then we use main text page1, and if HIRES
	on we also use main hires page1, regardless of RAMRD and
	RAMWRT. */
E(iie_page2_off)
		testl	$SS_PAGE2, SN(softswitches)	# already off?
		jz	ram_nop
		andl	$~(SS_PAGE2|SS_SCREEN), SN(softswitches)
		testl	$SS_80STORE, SN(softswitches)
		jz	_iie_page2_off_hires_off	# no 80STORE
		andl	$~(SS_TEXTRD|SS_TEXTWRT), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_textrd)
		movl	$SN(apple_ii_64k), SN(base_textwrt)
		testl	$SS_HIRES, SN(softswitches)
		jz	_iie_page2_off_hires_off	# no HIRES
		andl	$~(SS_HGRRD|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_hgrrd)
		movl	$SN(apple_ii_64k), SN(base_hgrwrt)
_iie_page2_off_hires_off:
		pushal
		pushl	$0				# page 1 main
		call	SN(video_setpage)
		addl	$4, %esp
		popal
/* 		call	SN(video_redraw)# redraw */
		ret


/* PAGE2 on.  if 80STORE on then we use aux text page 1, and if HIRES
	on we also use aux hires page 1, regardless of RAMRD and
	RAMWRT. */
E(iie_page2_on)
		testl	$SS_PAGE2, SN(softswitches)	# already on?
		jnz	ram_nop
		orl	$SS_PAGE2, SN(softswitches)
		testl	$SS_80STORE, SN(softswitches)
		jz	_iie_page2_on_80store_off	# no 80STORE
		orl	$(SS_TEXTRD|SS_TEXTWRT), SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_textrd)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_textwrt)
		testl	$(SS_HIRES), SN(softswitches)
		jz	_iie_page2_on_cont
		orl	$(SS_HGRRD|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrrd)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrwrt)
_iie_page2_on_cont:
		ret

_iie_page2_on_80store_off:				# no 80STORE
		orl	$SS_SCREEN, SN(softswitches)
		pushal
		pushl	$1				# page 2 main
		call	SN(video_setpage)
		addl	$4, %esp
		popal
/* 		call	SN(video_redraw)# redraw */
		ret

E(read_switch_graphics)
		testl	$SS_TEXT, SN(softswitches)
		jz	ram_nop
		andl	$~SS_TEXT, SN(softswitches)
		call	SN(video_redraw)
		ret		

E(read_switch_text)
		testl	$SS_TEXT, SN(softswitches)
		jnz	ram_nop
		orl	$SS_TEXT, SN(softswitches)
		call	SN(video_redraw)
		ret

E(read_switch_no_mixed)
		testl	$SS_MIXED, SN(softswitches)
		jz	ram_nop
		andl	$~SS_MIXED, SN(softswitches)
		call	SN(video_redraw)
		ret		

E(read_switch_mixed)
		testl	$SS_MIXED, SN(softswitches)
		jnz	ram_nop
		orl	$SS_MIXED, SN(softswitches)
		call	SN(video_redraw)
		ret

E(read_switch_lores)
		testl	$SS_HIRES, SN(softswitches)
		jz	ram_nop
		andl	$~SS_HIRES, SN(softswitches)
		call	SN(video_redraw)
		ret

E(read_switch_hires)
		testl	$SS_HIRES, SN(softswitches)
		jnz	ram_nop
		orl	$SS_HIRES, SN(softswitches)
		call	SN(video_redraw)
		ret

/* HIRES off. use RAMRD/RAMWRT offsets for hires page 1. */
E(iie_hires_off)
		testl	$SS_HIRES, SN(softswitches)	# already off?
		jz	ram_nop
		andl	$~(SS_HIRES|SS_HGRRD|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_hgrrd)
		movl	$SN(apple_ii_64k), SN(base_hgrwrt)
		testl	$SS_RAMRD,SN(softswitches)
		jz	iie_hires_off_no_ramrd
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrrd)
		orl	$SS_HGRRD, SN(softswitches)
iie_hires_off_no_ramrd:
		testl	$SS_RAMWRT,SN(softswitches)
		jz	iie_hires_off_no_ramwrt
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrwrt)
		orl	$SS_HGRWRT, SN(softswitches)
iie_hires_off_no_ramwrt: 
		call	SN(video_redraw)	# update screen
		ret

/* HIRES on. if 80STORE on, use PAGE2 offset for hires page 1. */
E(iie_hires_on)
		testl	$SS_HIRES, SN(softswitches)	# already on?
		jnz	ram_nop
		orl	$SS_HIRES, SN(softswitches)	# hires on
		testl	$SS_80STORE, SN(softswitches)
		jz	iie_hires_on_80store_off	# no 80STORE
		testl	$SS_PAGE2, SN(softswitches)
		jz	iie_hires_on_80store_aux
		andl	$~(SS_HGRRD|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_hgrrd)
		movl	$SN(apple_ii_64k), SN(base_hgrwrt)
		jmp	iie_hires_on_80store_off
iie_hires_on_80store_aux:
		orl	$(SS_HGRRD|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrrd)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrwrt)
iie_hires_on_80store_off:
		call	SN(video_redraw)	# update screen
		ret

/*****************************************************************************/

.comm		joy_trigger0, 2
.comm		joy_trigger1, 2
.comm		joy_trigger2, 2
.comm		joy_trigger3, 2

E(read_button0)
		movb	SN(joy_button0), %al
		ret

E(read_button1)
		movb	SN(joy_button1), %al
		ret

E(read_button2)
		movb	SN(joy_button2), %al
		ret

E(read_gc0)
		cmpw	$0xFF, joy_trigger0
		je	read_gc0_cont
		incw	joy_trigger0
read_gc0_cont:
		movw	joy_trigger0, %ax
		cmpw	%ax, SN(joy_x)
		jge	read_gc0_ff	/* XXXX is this correct? */
		movb	$0, %al
		ret
read_gc0_ff:
		movb	$0xFF, %al
		ret

E(read_gc1)
		cmpw	$0xFF, joy_trigger1
		je	read_gc1_cont
		incw	joy_trigger1
read_gc1_cont:
		movw	joy_trigger1, %ax
		cmpw	%ax, SN(joy_y)
		jge	read_gc1_ff
		movb	$0, %al
		ret
read_gc1_ff:
		movb	$0xFF, %al
		ret

/* HACK not doing anything... */
E(iie_read_gc2)
		ret

E(iie_read_gc3)
		ret

E(read_gc_strobe)
		movb	$0, joy_trigger0
		movb	$0, joy_trigger1
		movb	$0, joy_trigger2
		movb	$0, joy_trigger3
		ret

E(iie_c080)
		testl	$SS_ALTZP, SN(softswitches)
		jz	lc_c080
		pushl	$lc_to_auxmem /* sneak this code in when
				       * ][+ routine exits */

/* c080: read RAM; no write; use $D000 bank 2. */
E(lc_c080)
		orl	$SS_LCRAM|SS_BANK2, SN(softswitches)
		andl	$~(SS_LCSEC|SS_LCWRT), SN(softswitches)
		movl	$SN(language_banks)-0xD000, SN(base_d000_rd)
		movl	$SN(language_card)-0xE000, SN(base_e000_rd)
		movl	$0, SN(base_d000_wrt)
		movl	$0, SN(base_e000_wrt)
		ret

E(iie_c081)
		testl	$SS_ALTZP, SN(softswitches)
		jz	lc_c081
		pushl	$lc_to_auxmem /* sneak this code in when
			 	       * ][+ routine exits */

/* c081: read ROM; write RAM; use $D000 bank 2. */
E(lc_c081)
		testl	$SS_LCSEC, SN(softswitches)
		je	lc_c081_exit
		orl	$SS_LCWRT, SN(softswitches)
		movl	$SN(language_banks)-0xD000, SN(base_d000_wrt)
		movl	$SN(language_card)-0xE000, SN(base_e000_wrt)
lc_c081_exit:
		andl	$~SS_LCRAM, SN(softswitches)
		orl 	$SS_LCSEC|SS_BANK2, SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_d000_rd)
		movl	$SN(apple_ii_64k), SN(base_e000_rd) 
		ret

/* There is no iie_c082 --- since the LC is offline, no auxillary memory
 * could be exposed. (ditto for c08a) */

/* c082: read ROM; no write; use $D000 bank 2. */
E(lc_c082)
		andl	$~(SS_LCRAM|SS_LCWRT|SS_LCSEC), SN(softswitches)
		orl	$SS_BANK2, SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_d000_rd)
		movl	$SN(apple_ii_64k), SN(base_e000_rd) 
		movl	$0, SN(base_d000_wrt)
		movl	$0, SN(base_e000_wrt)
		ret

E(iie_c083)
		testl	$SS_ALTZP, SN(softswitches)
		jz	lc_c083
		pushl	$lc_to_auxmem /* sneak this code in when
				       * ][+ routine exits */

/* c083: read and write RAM; no write; use $D000 bank 2. */
E(lc_c083)	testl	$SS_LCSEC, SN(softswitches)
		je	lc_c083_exit
		orl	$SS_LCWRT, SN(softswitches)
		movl	$SN(language_banks)-0xD000, SN(base_d000_wrt)
		movl	$SN(language_card)-0xE000, SN(base_e000_wrt)
lc_c083_exit:
		orl	$(SS_LCSEC|SS_LCRAM|SS_BANK2), SN(softswitches)
		movl	$SN(language_banks)-0xD000, SN(base_d000_rd)
		movl	$SN(language_card)-0xE000, SN(base_e000_rd)
		ret

E(iie_c088)
		testl	$SS_ALTZP, SN(softswitches)
		jz	lc_c088
		pushl	$lc_to_auxmem /* sneak this code in when
				       * ][+ routine exits */

/* c088: read RAM; no write; use $D000 bank 1. */
E(lc_c088)
		orl	$(SS_LCRAM), SN(softswitches)
		andl	$~(SS_LCWRT|SS_LCSEC|SS_BANK2), SN(softswitches)
		movl	$SN(language_banks)-0xC000, SN(base_d000_rd)
		movl	$SN(language_card)-0xE000, SN(base_e000_rd)
		movl	$0, SN(base_d000_wrt)
		movl	$0, SN(base_e000_wrt)
		ret

E(iie_c089)
		testl	$SS_ALTZP, SN(softswitches)
		jz	lc_c089
		pushl	$lc_to_auxmem /* sneak this code in when
				       * ][+ routine exits */

/* c089: read ROM; write RAM; use $D000 bank 1. */
E(lc_c089)
		testl	$SS_LCSEC, SN(softswitches)
		jz	lc_c089_exit
		orl	$SS_LCWRT, SN(softswitches)
		movl	$SN(language_banks)-0xC000, SN(base_d000_wrt)
		movl	$SN(language_card)-0xE000, SN(base_e000_wrt)
lc_c089_exit:
		andl	$~(SS_LCRAM|SS_BANK2), SN(softswitches)
		orl	$(SS_LCSEC), SN(softswitches) 
		movl	$SN(apple_ii_64k), SN(base_d000_rd)
		movl	$SN(apple_ii_64k), SN(base_e000_rd) 
		ret

/* there is no iie_c08a */

/* c08a: read ROM; no write; use $D000 bank 1. */
E(lc_c08a)
		andl	$~(SS_LCRAM|SS_LCWRT|SS_LCSEC|SS_BANK2), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_d000_rd)
		movl	$SN(apple_ii_64k), SN(base_e000_rd) 
		movl	$0, SN(base_d000_wrt)
		movl	$0, SN(base_e000_wrt)		
		ret

E(iie_c08b)
		testl	$SS_ALTZP, SN(softswitches)
		jz	lc_c08b
 		pushl	$lc_to_auxmem /* sneak this code in when
				       * ][+ routine exits */

/* c08b: read and write RAM; use $D000 bank 1. */
E(lc_c08b)
		testl	$SS_LCSEC, SN(softswitches)
		jz	lc_c08b_exit
		orl	$SS_LCWRT, SN(softswitches)
		movl	$SN(language_banks)-0xC000, SN(base_d000_wrt)
		movl	$SN(language_card)-0xE000, SN(base_e000_wrt)
lc_c08b_exit:
		orl	$(SS_LCRAM|SS_LCSEC), SN(softswitches)
		andl	$~SS_BANK2,SN(softswitches)
		movl	$SN(language_banks)-0xC000, SN(base_d000_rd)
		movl	$SN(language_card)-0xE000, SN(base_e000_rd)
		ret
/* -------------------------------------------------------------------------
 * misc //e functions
 * ------------------------------------------------------------------------- */


/* 80STORE off. use RAMRD/RAMWRT for text/hires display page 1 */
E(iie_80store_off)
		testl	$SS_80STORE, SN(softswitches) # already  off?
		jz	ram_nop
		andl	$~(SS_80STORE|SS_TEXTRD|SS_TEXTWRT|SS_HGRRD|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k),SN(base_textrd)
		movl	$SN(apple_ii_64k),SN(base_textwrt)
		movl	$SN(apple_ii_64k),SN(base_hgrrd)
		movl	$SN(apple_ii_64k),SN(base_hgrwrt)
		testl	$SS_RAMRD, SN(softswitches)
		jz 	iie_80store_off_noramrd
		orl	$(SS_TEXTRD|SS_HGRRD),SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2,SN(base_textrd)
		movl	$SN(apple_ii_64k)+BANK2,SN(base_hgrrd)
iie_80store_off_noramrd:
		testl	$SS_RAMRD, SN(softswitches)
		jz 	iie_80store_off_noramwrt
		orl	$(SS_TEXTWRT|SS_HGRWRT),SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2,SN(base_textwrt)
		movl	$SN(apple_ii_64k)+BANK2,SN(base_hgrwrt)
iie_80store_off_noramwrt:
		testl	$SS_PAGE2, SN(softswitches)
		jz	ram_nop
		orl	$SS_SCREEN, SN(softswitches)
		pushal
		pushl	$1
		call	SN(video_setpage)
		addl	$4, %esp
		popal
/* 		call	SN(video_redraw)	# redraw */
		ret

/* 80STORE on. we use PAGE2 offset to access text page 1.  if HIRES on
	we also use PAGE2 offset to access hires page 1 */
E(iie_80store_on)
		testl	$SS_80STORE, SN(softswitches)	# already on?
		jnz	ram_nop
		orl	$SS_80STORE, SN(softswitches)
		testl	$SS_PAGE2, SN(softswitches)
		jnz	iie_80store_on_page2_on
		andl	$~(SS_TEXTRD|SS_TEXTWRT), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_textrd)
		movl	$SN(apple_ii_64k), SN(base_textwrt)
		testl	$SS_HIRES, SN(softswitches)
		jz	iie_80store_on_cont
		andl	$~(SS_HGRRD|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_hgrrd)
		movl	$SN(apple_ii_64k), SN(base_hgrwrt)
		jmp	iie_80store_on_cont

iie_80store_on_page2_on:
		orl	$(SS_TEXTRD|SS_TEXTWRT), SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_textrd)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_textwrt)
		testl	$SS_HIRES, SN(softswitches)
		jz	iie_80store_on_cont
		orl	$(SS_HGRRD|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrrd)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrwrt)
iie_80store_on_cont:
		andl	$~SS_SCREEN, SN(softswitches)
		pushal
		pushl	$0					# page 1
		call	SN(video_setpage)
		addl	$4, %esp
		popal
/* 		call	SN(video_redraw)	# redraw */
		ret

return80:	movb	$0x80,%al
		ret

E(iie_check_80store)
		testl	$SS_80STORE,SN(softswitches)
		jnz	return80
		xorb	%al,%al
		ret

/* RAMRD main. if 80STORE off then text/hires display pages are in
	main memory. */
E(iie_ramrd_main)
		testl	$SS_RAMRD, SN(softswitches)	# already off?
		jz	ram_nop
		andl	$~SS_RAMRD, SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_ramrd)
		testl	$SS_80STORE, SN(softswitches)	# 80store?
		jnz	iie_ramrd_main_80store_on
		andl	$~(SS_TEXTRD|SS_HGRRD), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_textrd)
		movl	$SN(apple_ii_64k), SN(base_hgrrd)
/* 		call	SN(video_redraw)	# redraw */
		ret
iie_ramrd_main_80store_on:
		testl	$SS_HIRES, SN(softswitches)
		jnz	ram_nop
		andl	$~SS_HGRRD, SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_hgrrd)
/* 		call	SN(video_redraw)	# redraw */
		ret

/* RAMRD aux. if 80STORE off then text/hires display pages are in aux
	memory. */
E(iie_ramrd_aux)
		testl	$SS_RAMRD, SN(softswitches) # already on?
		jnz	ram_nop
		orl	$SS_RAMRD, SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_ramrd)
		testl	$SS_80STORE, SN(softswitches)	# 80store?
		jnz	iie_ramrd_aux_80store_on
		orl	$(SS_TEXTRD|SS_HGRRD), SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_textrd)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrrd)
/* 		call	SN(video_redraw)	# redraw */
		ret
iie_ramrd_aux_80store_on:
		testl	$SS_HIRES, SN(softswitches)
		jnz	ram_nop					# already set
		orl	$(SS_HGRRD), SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrrd)
/* 		call	SN(video_redraw)	# redraw */
		ret

E(iie_check_ramrd)
		testl	$SS_RAMRD, SN(softswitches)
		jnz	return80
		xorb	%al,%al	
		ret

/* RAMWRT main. if 80STORE off then text/hires display pages are in
	main memory. */
E(iie_ramwrt_main)
		testl	$SS_RAMWRT, SN(softswitches)	# already off?
		jz	ram_nop
		andl	$~SS_RAMWRT, SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_ramwrt) # main RAM
		testl	$SS_80STORE, SN(softswitches) # 80store?
		jnz	iie_ramwrt_main_80store_on
		andl	$~(SS_TEXTWRT|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_textwrt)
		movl	$SN(apple_ii_64k), SN(base_hgrwrt)
/* 		call	SN(video_redraw)	# redraw */
		ret
iie_ramwrt_main_80store_on:
		testl	$SS_HIRES, SN(softswitches)
		jnz	ram_nop
		andl	$~SS_HGRWRT, SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_hgrwrt)
/* 		call	SN(video_redraw)	# redraw */
		ret

/* RAMWRT aux. if 80STORE off then write to text/hires display pages
	are in aux memory. */
E(iie_ramwrt_aux)
		testl	$SS_RAMWRT, SN(softswitches)	# already on?
		jnz	ram_nop
		orl	$SS_RAMWRT, SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_ramwrt) # aux RAM
		testl	$SS_80STORE, SN(softswitches)	# 80store?
		jnz	iie_ramwrt_aux_80store_on
		orl	$(SS_TEXTWRT|SS_HGRWRT), SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_textwrt)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrwrt)
/* 		call	SN(video_redraw)	# redraw */
		ret
iie_ramwrt_aux_80store_on:
		testl	$SS_HIRES, SN(softswitches)
		jnz	ram_nop
		orl	$SS_HGRWRT, SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_hgrwrt)
/* 		call	SN(video_redraw)	# redraw */
		ret

E(iie_check_ramwrt)
		testl	$SS_RAMWRT, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret
	
E(iie_altzp_main)
		testl	$SS_ALTZP, SN(softswitches)
		jz	1f	/* test if ALTZP already off -
				 * due to d000-bank issues it is 
				 * *needed*, not just a shortcut */
		andl	$~SS_ALTZP, SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_stackzp)
		andl	$0xFFFF, SP_Reg
		testl	$SS_LCRAM|SS_LCWRT, SN(softswitches)
		jz	1f /* shortest path for common case */
		testl	$SS_LCRAM, SN(softswitches)
		jz	2f
		subl	$0x2000, SN(base_d000_rd)
		movl	$SN(language_card)-0xE000, SN(base_e000_rd)
2:		testl	$SS_LCWRT, SN(softswitches)
		jz	1f
		subl	$0x2000, SN(base_d000_wrt)
		movl	$SN(language_card)-0xE000, SN(base_e000_wrt)
1:		ret

E(iie_altzp_aux)
		testl	$SS_ALTZP, SN(softswitches)
		jnz	1f	/* test if ALTZP already on -
				 * due to d000-bank issues it is 
				 * *needed*, not just a shortcut */
		orl	$SS_ALTZP, SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_stackzp)
		orl	$BANK2, SP_Reg
		testl	$SS_LCRAM|SS_LCWRT, SN(softswitches)
		jz	1f /* shortest path for common case */
lc_to_auxmem:	/* called by lc routines */
		testl	$SS_LCRAM, SN(softswitches)
		jz	2f
		addl	$0x2000, SN(base_d000_rd)
		movl	$SN(language_card)-0xC000, SN(base_e000_rd)
2:		testl	$SS_LCWRT, SN(softswitches)
		jz	1f
		addl	$0x2000, SN(base_d000_wrt)
		movl	$SN(language_card)-0xC000, SN(base_e000_wrt)
1:		ret

E(iie_check_altzp)
		testl	$SS_ALTZP, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

E(iie_80col_off)
    		testl	$SS_80COL, SN(softswitches)
    		jz	ram_nop
		andl	$~SS_80COL, SN(softswitches)
		testl	$(SS_TEXT|SS_MIXED|SS_DHIRES), SN(softswitches)
		jnz	SN(video_redraw)
		ret

E(iie_80col_on)
    		testl	$SS_80COL, SN(softswitches)
    		jnz	ram_nop
		orl	$SS_80COL, SN(softswitches)
		testl	$(SS_TEXT|SS_MIXED|SS_DHIRES), SN(softswitches)
		jnz	SN(video_redraw)
		ret

E(iie_check_80col)
		testl	$SS_80COL, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

E(iie_altchar_off)
		testl	$SS_ALTCHAR, SN(softswitches)
    		jz	ram_nop
		andl	$~SS_ALTCHAR, SN(softswitches)
		call	SN(c_set_primary_char)
		ret

E(iie_altchar_on)
		testl	$SS_ALTCHAR, SN(softswitches)
    		jnz	ram_nop
		orl	$SS_ALTCHAR, SN(softswitches)
		call	SN(c_set_altchar)
		ret

E(iie_check_altchar)
		testl	$SS_ALTCHAR, SN(softswitches)
		jnz	return80
		xorb	%al, %al
                ret

/* unset ioudis flag and read_gc_strobe */
E(iie_ioudis_off)
		andl	$~SS_IOUDIS, SN(softswitches)
		jmp	SN(read_gc_strobe)

/* set ioudis flag and read_gc_strobe */
E(iie_ioudis_on)
		orl	$SS_IOUDIS, SN(softswitches)
		jmp	SN(read_gc_strobe)

/* check ioudis flag and read_gc_strobe */
E(iie_check_ioudis)
		call	SN(read_gc_strobe)
		testl	$SS_IOUDIS, SN(softswitches)		
		jnz	return80
		xorb	%al, %al
		ret

E(iie_dhires_on)
		testl	$SS_IOUDIS, SN(softswitches)
		jz	ram_nop
		orl	$SS_DHIRES, SN(softswitches)
		call	SN(video_redraw)
		ret

E(iie_dhires_off)
		testl	$SS_IOUDIS, SN(softswitches)
		jz	ram_nop
		andl	$~SS_DHIRES, SN(softswitches)
		call	SN(video_redraw)
		ret

/* check dhires flag and read_gc_strobe */
E(iie_check_dhires)
		call	SN(read_gc_strobe)
		testl	$SS_DHIRES, SN(softswitches)
		jnz	return80
		ret

E(iie_check_text)
		testl	$SS_TEXT, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

E(iie_check_mixed)
		testl	$SS_MIXED, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

E(iie_check_page2)
		testl	$SS_PAGE2, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

E(iie_check_hires)
		testl	$SS_HIRES, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

E(iie_check_bank)
		testl	$SS_BANK2, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

E(iie_check_lcram)
		testl	$SS_LCRAM, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

/* HACK not doing anything... */
E(iie_check_vbl)
		ret

	/* ---------------------------------- */

E(iie_c3rom_peripheral)
		andl	$~SS_C3ROM, SN(softswitches)
		testl	$SS_CXROM, SN(softswitches)
		jnz	1f /* CXROM-internal preempts C3ROM */
		movl	$SN(apple_ii_64k), SN(base_c3rom)
1:		ret

E(iie_c3rom_internal)
		orl	$SS_C3ROM, SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_c3rom)
		ret

E(iie_check_c3rom)
		testl	$SS_C3ROM, SN(softswitches)
		jz	return80
		xorb	%al, %al
		ret

E(iie_cxrom_peripheral)
		andl	$~SS_CXROM, SN(softswitches)
		movl	$SN(apple_ii_64k), SN(base_cxrom)
		testl	$SS_C3ROM, SN(softswitches)
		jnz	1f
		movl	$SN(apple_ii_64k), SN(base_c3rom)
1:		ret

E(iie_cxrom_internal)
		orl	$SS_CXROM, SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_cxrom)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_c3rom)
		ret

E(iie_check_cxrom)
		testl	$SS_CXROM, SN(softswitches)
		jnz	return80
		xorb	%al, %al
		ret

/* HACK!!!!! - how does the expansion slot get referenced? need to handle other roms requesting to use this area!!!! */
E(iie_read_slot_expansion)
		orl	SN(c8rom_offset), EffectiveAddr_E
		movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al
		andl	$0xFFFF, EffectiveAddr_E
		ret

/* HACK!!!!! - how does the expansion slot get referenced? */
E(iie_disable_slot_expansion)
		ret

