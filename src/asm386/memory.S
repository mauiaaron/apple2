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

return80:	movb	$0x80,%al
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
		pushal
		call	SN(c_set_primary_char)
		popal
		ret

E(iie_altchar_on)
		testl	$SS_ALTCHAR, SN(softswitches)
    		jnz	ram_nop
		orl	$SS_ALTCHAR, SN(softswitches)
		pushal
		call	SN(c_set_altchar)
		popal
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
		testl	$SS_DHIRES, SN(softswitches)
		jnz	ram_nop
		orl	$SS_DHIRES, SN(softswitches)
		call	SN(video_redraw)
		ret

E(iie_dhires_off)
		testl	$SS_DHIRES, SN(softswitches)
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
#ifdef AUDIO_ENABLED
		movl	$SN(MB_Read), SN(base_c4rom)
		movl	$SN(MB_Read), SN(base_c5rom) // HACK FIXME -- MB is hardcoded, what about Phasor?
#endif
		testl	$SS_C3ROM, SN(softswitches)
		jnz	1f
		movl	$SN(apple_ii_64k), SN(base_c3rom)
1:		ret

E(iie_cxrom_internal)
		orl	$SS_CXROM, SN(softswitches)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_cxrom)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_c3rom)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_c4rom)
		movl	$SN(apple_ii_64k)+BANK2, SN(base_c5rom)
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

