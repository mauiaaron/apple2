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
 * Plot a dynamically interpolated swath of pixels.
 * For 640x400 this is 2 rows of 18 pixels.
 * ebx: from table
 * eax: to graphics memory
 * ecx: scratch
 * ------------------------------------------------------------------------- */
#define PlotPixelsExtra\
		subl	$2, %eax;\
		pushl	%edx;\
		xorl	%edx, %edx;\
		PlotPixelsExtraRow;\
		addl	$SCANWIDTH-18, %eax;\
		subl	$9, %ebx;\
		PlotPixelsExtraRow;\
		popl	%edx;
#define PlotPixelsExtraRow\
		movb	$9, %dl;\
	1:	movb	(%ebx), %cl;\
		movb	%cl, %ch;\
		movw	%cx, (%eax);\
		incl	%ebx;\
		addl	$2, %eax;\
		decb	%dl;\
		jnz	1b;

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
 * Calculate the graphics memory offset based on EffectiveAddr.
 * BASE 0x2000, 0x4000
 * PTR register to store the offset
 * ------------------------------------------------------------------------- */
#define CalcHiresGM(BASE,PTR,GM)\
		movl	EffectiveAddr_E, %ecx;		/* ecx = mem addrs */ \
		subw	BASE, EffectiveAddr;		/* - graphics base */ \
		movl	SN(video__screen_addresses)			      \
			(,EffectiveAddr_E,4), PTR;	/* PTR = GM offset */ \
		movl	%ecx, EffectiveAddr_E;		/* + graphics base */ \
		addl	SN(GM), PTR;		/* PTR += GM base */


/* -------------------------------------------------------------------------
 * PlotByte - macro to plot a hires byte into graphics memory.
 * BASE = 0x2000,0x4000.
 * TABLE = expanded_col_hires_even, expanded_col_hires_odd.
 * OPP_TABLE = opposite table
 * INTERP_COLOR = video__even_colors, video__odd_colors
 * ALT_INTERP_COLOR = opposite colors
 * ------------------------------------------------------------------------- */
#define PlotByte(BASE,X,TABLE,OPP_TABLE,INTERP_COLOR,ALT_INTERP_COLOR,GM)\
		pushl	%eax;				/* save regs */	      \
		pushl	%ebx;						      \
		pushl	%ecx;						      \
		\
		xorb	%ah, %ah;			/* clear noise */     \
PB_dynamic##X:\
		leal	SN(video__hires_##TABLE)(,%eax,8), %ebx;\
		leal	SN(temp), %eax;	/* eax = temp */\
		addl	$2, %eax;\
                \
                /* plot exactly 7 pixels */ \
		movl	(%ebx), %ecx;			/* long -> GM */      \
		movl	%ecx, (%eax);				      \
		addl	$4, %ebx;			/* inc pointers */    \
		addl	$4, %eax;					      \
		movw	(%ebx), %cx;			/* word -> GM */      \
		movw	%cx, (%eax);				      \
		addl	$2, %ebx;			/* inc pointers */    \
		addl	$2, %eax;					      \
		movb	(%ebx), %cl;			/* byte -> GM */      \
		movb	%cl, (%eax); \
		\
		subl	$8, %eax;\
		CalcHiresGM(BASE,%ebx,GM);	/* ebx = GM */\
		/* copy adjacent color bytes into temp array */\
		subl	$3, %ebx;\
		movw	(%ebx), %cx;\
		movw	%cx, (%eax);			/* GM -> temp */\
		addl	$9, %eax;\
		addl	$18, %ebx;\
		movw	(%ebx), %cx;\
		movw	%cx, (%eax);			/* GM -> temp */\
		decl	%eax; \
		\
/* ------------------------------------------------------------------------- */ \
/* calculate dynamic colors in temp array                                    */ \
		movw	(%eax), %cx;					      \
		testb	$0xFF, %ch;	/* check right color */		      \
		jz	PB_next0##X;	/* right black, do other end */	      \
	movw	SN(apple_ii_64k)(,EffectiveAddr_E,1), %cx;\
		andb	$1, %ch;					      \
		jz	PB_black0##X;	/* right black */		      \
		andb	$0x40, %cl;					      \
		jz	PB_black0##X;	/* inside black, right colored */     \
		movw	$0x3737, (%eax); /* edge is white (#55) */	      \
		jmp	PB_next0##X;					      \
PB_black0##X:								      \
	movzwl	SN(apple_ii_64k)(,EffectiveAddr_E,1), %ecx;\
		movzbl	%ch, %ecx;      				      \
		leal	SN(video__hires_##OPP_TABLE)	      \
			(,%ecx,8), %ebx;				      \
		incl	%eax;						      \
		movb	(%ebx), %cl;					      \
		movb	%cl, (%eax);					      \
		decl	%eax;						      \
PB_next0##X:								      \
		decw	EffectiveAddr;		/* previous byte */	      \
		subl	$7, %eax;	/* left edge of byte */		      \
		movb	(%eax), %cl;					      \
		testb	$0xFF, %cl;	/* check left color */		      \
		jz	PB_next1##X;	/* left black, done */		      \
	movw	SN(apple_ii_64k)(,EffectiveAddr_E,1), %cx;\
		andb	$0x40, %cl;					      \
		jz	PB_black1##X;	/* left black */		      \
		andb	$0x1, %ch;					      \
		jz	PB_black1##X;	/* left colored, inside black */      \
		movw	$0x3737, (%eax); /* edge is white (#55) */	      \
		jmp	PB_next1##X;					      \
PB_black1##X:								      \
	movzbl	SN(apple_ii_64k)(,EffectiveAddr_E,1), %ecx;\
		leal	SN(video__hires_##OPP_TABLE)	      \
			(,%ecx,8), %ebx;				      \
		addl	$6, %ebx;					      \
		movb	(%ebx), %cl;					      \
		movb	%cl, (%eax);					      \
PB_next1##X:								      \
		incw	EffectiveAddr;                                        \
		/* do extra calculation for interpolated colors */	      \
		cmpb	$2, SN(video__strictcolors);			      \
		jne	PB_plot_dynamic##X;				      \
									      \
		decw	EffectiveAddr;					      \
		CalculateInterpColor(X,2,ALT_INTERP_COLOR);		      \
PB_next2##X:								      \
		incw	EffectiveAddr;					      \
		incl	%eax;						      \
		CalculateInterpColor(X,3,INTERP_COLOR);			      \
PB_next3##X:								      \
		addl	$6, %eax;					      \
		CalculateInterpColor(X,4,INTERP_COLOR);			      \
PB_next4##X:								      \
		incw	EffectiveAddr;					      \
		incl	%eax;						      \
		CalculateInterpColor(X,5,ALT_INTERP_COLOR);		      \
PB_next5##X:								      \
	decw	EffectiveAddr; \
/* ------------------------------------------------------------------------- */ \
\
PB_plot_dynamic##X:\
		leal	SN(temp), %ebx;	/* ebx = temp */\
		incl	%ebx;\
		CalcHiresGM(BASE,%eax,GM);		/* eax = GM */\
		PlotPixelsExtra				/* temp -> GM: 1 + 7 + 1 */\
PB_exit##X:\
		popl	%ecx;				/* restore regs */    \
		popl	%ebx;						      \
		popl	%eax;



/* -------------------------------------------------------------------------
 * Calculates the color at the edge of interpolated bytes.
 * Done 4 times in little endian order (...7 0...7 0...)
 * ------------------------------------------------------------------------- */
#define CalculateInterpColor(X,Y,INTERP_COLOR)				\
		testb	$0xFF, (%eax);					\
		jnz	PB_next##Y##X;	/* not black, next */		\
		movw	(%eax), %cx;	/* off+1 in %ch */		\
		testb	$0xFF, %ch;					\
		jz	PB_next##Y##X;	/* off+1 is black, next */	\
		movb	-1(%eax), %cl;	/* off-1 in %cl */		\
		testb	$0xFF, %cl;					\
		jz	PB_next##Y##X;	/* off-1 is black, next */	\
		cmpb	$55, %cl;	/* off-1 is white? */		\
		je	PB_white0##X##Y;				\
		movb	%cl, (%eax);	/* store non-white */		\
		jmp	PB_next##Y##X;	/* next */			\
PB_white0##X##Y:							\
		cmpb	$55, %ch;	/* off+1 is white? */		\
		je	PB_white##X##Y;					\
		movb	%ch, (%eax);	/* store non-white */		\
		jmp	PB_next##Y##X;	/* next */			\
PB_white##X##Y:				/* both sides are white */	\
	movzbl	SN(apple_ii_64k)(,EffectiveAddr_E,1), %ecx;\
		shrb	$7, %cl;					\
 		movb	SN(INTERP_COLOR)(,%ecx,1), %bl;	\
		movb	%bl, (%eax);


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

#define DRAW_GRAPHICS(PARITY, PAGE, TEXT_SW) \
		testl	TEXT_SW, SN(softswitches); \
		jnz	ram_nop /* text */; \
		testl	$SS_HIRES, SN(softswitches); \
		jz	ram_nop	/* lores */; \
		testl	$SS_80COL, SN(softswitches); \
    		jz	1f; \
		testl	$SS_DHIRES, SN(softswitches); \
		jnz	iie_plot_dhires##PAGE /* plot dhires */; \
1:		testl	$SS_HGRWRT, SN(softswitches); \
		jnz	ram_nop	/* in auxram */; \
		call	plot_##PARITY##_byte##PAGE /* plot hires */; \
                ret;

#if 1
/* video__write_2e_even0 - handle hires page //e specific */
E(video__write_2e_even0)
		addl	SN(base_hgrwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_hgrwrt), EffectiveAddr_E
iie_soft_write_even0:
                DRAW_GRAPHICS(even, 0, $SS_TEXT)

/* video__write_2e_even0_mixed - handle mixed hires page //e specific */
E(video__write_2e_even0_mixed)
		addl	SN(base_hgrwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_hgrwrt), EffectiveAddr_E
iie_soft_write_even0_mixed:
                DRAW_GRAPHICS(even, 0, $(SS_TEXT|SS_MIXED))


/* video__write_2e_odd0 - handle hires page //e specific */
E(video__write_2e_odd0)
		addl	SN(base_hgrwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_hgrwrt), EffectiveAddr_E
iie_soft_write_odd0:
                DRAW_GRAPHICS(odd, 0, $SS_TEXT)

/* video__write_2e_odd0_mixed - handle mixed hires page //e specific */
E(video__write_2e_odd0_mixed)
		addl	SN(base_hgrwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_hgrwrt), EffectiveAddr_E
iie_soft_write_odd0_mixed:
                DRAW_GRAPHICS(odd, 0, $(SS_TEXT|SS_MIXED))


/* video__write_2e_even1 - write hires page1 //e specific */
E(video__write_2e_even1)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_even1:
                DRAW_GRAPHICS(even, 1, $SS_TEXT)

/* video__write_2e_even1_mixed - write hires page1 //e specific */
E(video__write_2e_even1_mixed)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_even1_mixed:
                DRAW_GRAPHICS(even, 1, $(SS_TEXT|SS_MIXED))


/* video__write_2e_odd1 - write hires page1 //e specific */
E(video__write_2e_odd1)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_odd1:
                DRAW_GRAPHICS(odd, 1, $SS_TEXT)

/* video__write_2e_odd1_mixed - write hires page1 //e specific */
E(video__write_2e_odd1_mixed)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_odd1_mixed:
                DRAW_GRAPHICS(odd, 1, $(SS_TEXT|SS_MIXED))

/* plot even column hires byte on page 0 */
		.align 4
plot_even_byte0:
		PlotByte($0x2000,0,even,odd,video__even_colors,video__odd_colors,video__fb1)
		ret
	
/* plot odd column hires byte on page 0 */
		.align 4
plot_odd_byte0:
		PlotByte($0x2000,1,odd,even,video__odd_colors,video__even_colors,video__fb1)
		ret

/* plot even column hires byte on page 1 */	
		.align 4
plot_even_byte1:
		PlotByte($0x4000,2,even,odd,video__even_colors,video__odd_colors,video__fb2)
		ret

/* plot odd column hires byte on page 1 */
		.align 4
plot_odd_byte1:
		PlotByte($0x4000,3,odd,even,video__odd_colors,video__even_colors,video__fb2)
		ret
#endif
	
E(iie_plot_dhires0)
		PlotDHires($0x2000,0,video__fb1)
		ret

E(iie_plot_dhires1)
		PlotDHires($0x4000,1,video__fb2)
		ret
