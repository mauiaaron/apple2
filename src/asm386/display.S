/* 
 * Apple // emulator for Linux: 
 *   Functions for low-level framebuffer output.
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

#define __ASSEMBLY__
#include "apple2.h"
#include "video/video.h"
#include "cpu.h"
#include "misc.h"

/* -------------------------------------------------------------------------
 * Plots a normalized byte of dhires color directly into graphics memory.
 * eax: graphics memory index
 * ebx: dhires_colors index
 * edx: scratch
 * ------------------------------------------------------------------------- */
#define PlotDHiresByte\
		movb	SN(video__dhires2)(,%ebx,1), %dl;\
		movb	%dl, %dh;\
		shll	$16, %edx;\
		movb	SN(video__dhires1)(,%ebx,1), %dl;\
		movb	%dl, %dh;\
		movl	%edx, (%eax);\
		movl	%edx, SCANWIDTH(%eax);\
		addl	$4, %eax;

#define PlotDHiresFirstByte\
		subl	$4, %eax;\
		PlotDHiresByte

/* -------------------------------------------------------------------------
 * compeletely update all the hires/dhires page rows.
 * X=0,1
 * OFFSET=0x2027,0x4027 (page 1, 2)
 * ------------------------------------------------------------------------- */
#define UpdateHiresRows(PRE,X,OFFSET)					 \
update_hires_rows_##X:							 \
		movl	$20, %ecx;	/* ECX: 40 column counter */	 \
		movl	%ebx, %edx;	/* EBX: pixel row counter */	 \
		shrb	$3, %dl;	/* normalize to 0 - 23 */	 \
					/* EDI: row offset */		 \
		movw	SN(video__line_offset)(,%edx,2),	 \
			EffectiveAddr;					 \
		movl	%ebx, %edx;					 \
		andb	$0x7, %dl;					 \
		shlw	$10, %dx;	/* EDX: offset range 0 - 1C00 */ \
		addw	OFFSET, %dx;	/* add base end-row offset */	 \
		addw	%dx, EffectiveAddr;	/* EDI: mem address */	 \
update_hires_columns_##X:						 \
	movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al;\
		cmpw	$159, %bx;	/* mixed mode boundary */	 \
		jg	update_hires_mixed_##X;				 \
                pushal;\
		call	SN(PRE##odd##X##);	 \
                popal;\
		decw	EffectiveAddr;	/* previous address */		 \
	movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al;\
                pushal;\
		call	SN(PRE##even##X##);	 \
                popal;\
		jmp	update_hires_cont_##X;				 \
update_hires_mixed_##X:							 \
                pushal;\
		call	SN(PRE##odd##X##_mixed);	 \
                popal;\
		decw	EffectiveAddr;	/* previous address */		 \
	movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al;\
                pushal;\
		call	SN(PRE##even##X##_mixed);	 \
                popal;\
update_hires_cont_##X:							 \
		decw	EffectiveAddr;	/* previous address */		 \
		decb	%cl;		/* dec column counter */	 \
		jnz	update_hires_columns_##X;			 \
		decw	%bx;		/* dec row */			 \
		jns	update_hires_rows_##X;


/* -------------------------------------------------------------------------
 * compeletely update all the text page rows.
 * X=0,1
 * OFF=0x427,0x827 (page 1, 2)
 * ------------------------------------------------------------------------- */
#define UpdateRows(PRE,X,OFFSET)				\
update_rows_##X:							\
		movl	$39, %ecx;					\
		movw	SN(video__line_offset)(,%ebx,2),	\
			EffectiveAddr;					\
		addw	OFFSET, EffectiveAddr;				\
update_columns_##X:							\
	movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al;\
		cmpb	$19, %bl;					\
		jg	update_mixed_##X;				\
                pushal;\
		call	SN(PRE##text##X);		\
                popal; \
		jmp	update_cont_##X;				\
update_mixed_##X:							\
                pushal; \
		call	SN(PRE##text##X##_mixed);		\
                popal; \
update_cont_##X:							\
		decw	%di;						\
		decb	%cl;						\
		jns	update_columns_##X;				\
		decb	%bl;						\
		jns	update_rows_##X;


/* -------------------------------------------------------------------------
 * Plot a full double hires color byte into GM
 * OFF 0x2000, 0x4000
 * PROBLEMS:
 * 	graphics artifiacts are not implemented correctly.
 * ------------------------------------------------------------------------- */
#define PlotDHires(OFF,X,GM) \
		pushl	%eax;				/* save regs */	      \
		pushl	%ebx;						      \
		pushl	%ecx;						      \
		pushl	%edx;						      \
		pushl	EffectiveAddr_E;				      \
									      \
		andl	$0xFFFF, EffectiveAddr_E;		/* erase offset */    \
		btr	$0, EffectiveAddr_E;		/* normalize */	      \
		movl	EffectiveAddr_E, %ecx;		/* ecx = mem addrs */ \
		subw	OFF, EffectiveAddr;		/* - graphics base */ \
		movl	SN(video__screen_addresses)			      \
			(,EffectiveAddr_E,4), %eax;	/* eax = GM offset */ \
		movb	SN(video__columns)			      \
			(,EffectiveAddr_E,1), %bl;			      \
		addl	SN(GM), %eax;		/* eax += GM base */  \
									      \
	leal	SN(apple_ii_64k), EffectiveAddr_E;\
		addl	%ecx, EffectiveAddr_E;				      \
		movl	EffectiveAddr_E, %ecx;				      \
		addl	$0x10000, %ecx;					      \
									      \
		testb	$0xFF, %bl;					      \
		jz	plot_dhires##X##_cont;				      \
		movzbl	-1(EffectiveAddr_E), %ebx;			      \
		movb	0(%ecx), %bh;					      \
		btr	$7, %ebx;					      \
		shrb	$3, %bl;					      \
		shlb	$4, %bh;					      \
		orb	%bh, %bl;					      \
		xorb	%bh, %bh;					      \
		PlotDHiresFirstByte					      \
									      \
plot_dhires##X##_cont:							      \
		movl	%ecx, %edx;					      \
		movb	2(%edx), %cl;					      \
		shll	$28, %ecx;					      \
									      \
		movzbl	1(EffectiveAddr_E), %ebx;			      \
		btr	$7, %ebx;		/* erase msb */		      \
		shll	$21, %ebx;					      \
		orl	%ebx, %ecx;					      \
									      \
		movzbl	1(%edx), %ebx;					      \
		btr	$7, %ebx;		/* erase msb */		      \
		shll	$14, %ebx;					      \
		orl	%ebx, %ecx;					      \
									      \
		movzbl	0(EffectiveAddr_E), %ebx;			      \
		btr	$7, %ebx;		/* erase msb */		      \
		shll	$7, %ebx;					      \
		orl	%ebx, %ecx;					      \
									      \
		movzbl	0(%edx), %ebx;					      \
		btr	$7, %ebx;		/* erase msb */		      \
		orl	%ebx, %ecx;					      \
		/* 00000001 11111122 22222333 3333xxxx */		      \
									      \
		PlotDHiresByte						      \
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		popl	EffectiveAddr_E;				      \
		andl	$0xFFFF, EffectiveAddr_E;/* for safety */	      \
		popl	%edx;						      \
		popl	%ecx;			 /* restore regs */	      \
		popl	%ebx;						      \
		popl	%eax;						      \
		ret;


	/* -----------------------------------------------------------------
	 * Scan through video memory (text & graphics) and call the updating
	 * routines.  Depending on softswitch settings, either text or
	 * graphics, page 1 or 2 will be rendered.
	 * This is called on exit from menu screens, etc.
	 * ebx: number of rows (counting down)
	 * ----------------------------------------------------------------- */
E(video_redraw)
		pushal

		/* Temporarily reset some softswitches. This ensures a 
                 * proper update in the case where the video addresses
		 * are pointed at auxillary memory, yet a non-80col mode is
		 * in use.
		 */
		pushl	SN(softswitches)
		andl	$~(SS_TEXTWRT|SS_HGRWRT|SS_RAMWRT),SN(softswitches)

		xorl	%eax, %eax
		xorl	EffectiveAddr_E, EffectiveAddr_E

		/* 24 rows text/lores page 0 */
		movl	$23, %ebx
		UpdateRows(iie_soft_write_,0,$0x427)

		/* 24 rows text/lores page 1 */
		movl	$23, %ebx
		UpdateRows(iie_soft_write_,1,$0x827)

		/* 192 rows hires page 0 */
		movl	$191, %ebx
		UpdateHiresRows(iie_soft_write_,0,$0x2027)

		/* 192 rows hires page 1 */
		movl	$191, %ebx
		UpdateHiresRows(iie_soft_write_,1,$0x4027)

		popl	SN(softswitches)
		popal
		ret
	
		/******************************************/
	
E(iie_plot_dhires0)
		PlotDHires($0x2000,0,video__fb1)
		ret

E(iie_plot_dhires1)
		PlotDHires($0x4000,1,video__fb2)
		ret

