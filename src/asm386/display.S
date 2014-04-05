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
    Graphics routines.
    Care has been taken to isolate the dimension-dependent (320x200 vs 640x400)
    routines.
   ------------------------------------------------------------------------- */

#define Font		SN(video__wider_font)
#define Font80		SN(video__font)

/* -------------------------------------------------------------------------
 * Plot exatly 7 pixels from FROM to TO.
 * ecx: scratch
 * ------------------------------------------------------------------------- */
#define Plot7Pixels(FROM,TO)\
		movl	(FROM), %ecx;			/* long -> GM */      \
		movl	%ecx, (TO);				      \
		addl	$4, FROM;			/* inc pointers */    \
		addl	$4, TO;					      \
		movw	(FROM), %cx;			/* word -> GM */      \
		movw	%cx, (TO);				      \
		addl	$2, FROM;			/* inc pointers */    \
		addl	$2, TO;					      \
		movb	(FROM), %cl;			/* byte -> GM */      \
		movb	%cl, (TO);


#define LoadHiresTableRef(TABLE)\
		leal	SN(video__wider_hires_##TABLE), %ebx;\
		shll	$4, %eax;/* *16 */\
		addl	%eax, %ebx;

/* -------------------------------------------------------------------------
 * Plot a normal swath of pixels.
 * For 640x400 this is 2 rows of 14 pixels.
 * ebx: from table
 * eax: to graphics memory
 * ------------------------------------------------------------------------- */
#define PlotPixels\
		PlotPixels640\
		subl	$12, %ebx;\
		addl	$SCANWIDTH-12, %eax;\
		PlotPixels640\
		subl	$SCANWIDTH, %eax;\
		addl	$1, %eax;
#define PlotPixels640\
		movl	(%ebx), %ecx;			/* long -> GM */      \
		movl	%ecx, (%eax);				      \
		addl	$4, %eax;			/* inc pointers */    \
		addl	$4, %ebx;					      \
		movl	(%ebx), %ecx;			/* long -> GM */      \
		movl	%ecx, (%eax);				      \
		addl	$4, %eax;			/* inc pointers */    \
		addl	$4, %ebx;					      \
		movl	(%ebx), %ecx;			/* long -> GM */      \
		movl	%ecx, (%eax);				      \
		addl	$4, %eax;			/* inc pointers */    \
		addl	$4, %ebx;					      \
		movw	(%ebx), %cx;			/* word -> GM */      \
		movw	%cx, (%eax);

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
 * Plot an 80 column character row.  We can do this only in 640x400 resolution.
 * For 640x400 this is 2 rows of 7 pixels.
 * esi: from table
 * eax: to graphics memory
 * ------------------------------------------------------------------------- */
#define PlotCharacter80Row\
		Plot7Pixels(%esi,%eax);\
		addl	$ SCANWIDTH-6, %eax;/* Go to next row */\
		subl	$6, %esi;\
		Plot7Pixels(%esi,%eax);\

/* -------------------------------------------------------------------------
 * Plot a 40 column character row.
 * For 640x400 this is 2 rows of 14 pixels.
 * esi: from table
 * eax: to graphics memory
 * ------------------------------------------------------------------------- */
#define PlotCharacter40Row\
		PlotCharacter40Row640\
		subl	$12, %esi;\
		addl	$ SCANWIDTH-12, %eax;\
		PlotCharacter40Row640\
		addl	$2, %esi;
#define PlotCharacter40Row640\
		movl	(%esi), %ecx;	\
		movl	%ecx, (%eax);	\
		addl	$4, %esi;	\
		addl	$4, %eax;	\
		movl	(%esi), %ecx;	\
		movl	%ecx, (%eax);	\
		addl	$4, %esi;	\
		addl	$4, %eax;	\
		movl	(%esi), %ecx;	\
		movl	%ecx, (%eax);	\
		addl	$4, %esi;	\
		addl	$4, %eax;	\
		movw	(%esi), %cx;	\
		movw	%cx, (%eax);

/* -------------------------------------------------------------------------
 * Plot a 40 column row of lores graphics.
 * For 640x400 this is 2 rows of 14 pixels.
 * esi: from table
 * eax: to graphics memory
 * ------------------------------------------------------------------------- */
#define PlotBlockRow				\
		PlotBlockRow640			\
		addl	$ SCANWIDTH-12, %eax;	\
		PlotBlockRow640
#define PlotBlockRow640\
		movl	%edx, (%eax);		\
		addl	$4, %eax;		\
		movl	%edx, (%eax);		\
		addl	$4, %eax;		\
		movl	%edx, (%eax);		\
		addl	$4, %eax;		\
		movw	%dx, (%eax);

/* -------------------------------------------------------------------------
 * Get the adjancent color bytes in memory.
 * For 640x400 mode, we need to remember to move around by a factor of 2.
 * ebx: graphics memory index
 * eax: temp buffer for comparison
 * ------------------------------------------------------------------------- */
#define GrabAdjGMBytes\
		subl	$3, %ebx;\
		movw	(%ebx), %cx;\
		movw	%cx, (%eax);			/* GM -> temp */\
		addl	$9, %eax;\
		addl	$18, %ebx;\
		movw	(%ebx), %cx;\
		movw	%cx, (%eax);			/* GM -> temp */\
		decl	%eax;

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
		testb	$0xFF, SN(video__strictcolors);		      \
		jnz	PB_dynamic##X;			/* dynamic color mode */\
		LoadHiresTableRef(TABLE);\
		CalcHiresGM(BASE,%eax,GM);		/* eax = GM */\
		PlotPixels;				/* temp -> GM */\
		jmp	PB_exit##X;\
		\
PB_dynamic##X:\
		leal	SN(video__hires_##TABLE)(,%eax,8), %ebx;\
		leal	SN(temp), %eax;	/* eax = temp */\
		addl	$2, %eax;\
		Plot7Pixels(%ebx,%eax);			/* 7bytes -> temp+2 */\
		\
		subl	$8, %eax;\
		CalcHiresGM(BASE,%ebx,GM);	/* ebx = GM */\
		/* copy adjacent color bytes into temp array */\
		GrabAdjGMBytes;\
		\
		/* calculate dynamic colors in temp array */\
		DynamicCalculateColor(X,OPP_TABLE,INTERP_COLOR,ALT_INTERP_COLOR);\
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
 * Dynamic calculation of color at the edges of bytes.
 * ------------------------------------------------------------------------- */
#define DynamicCalculateColor(X,OPP_TABLE,INTERP_COLOR,ALT_INTERP_COLOR);\
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
		movb	%ch, %cl;					      \
		xorb	%ch, %ch;					      \
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
	decw	EffectiveAddr;


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
		call	SN(PRE##odd##X##);	 \
		decw	EffectiveAddr;	/* previous address */		 \
	movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al;\
		call	SN(PRE##even##X##);	 \
		jmp	update_hires_cont_##X;				 \
update_hires_mixed_##X:							 \
		call	SN(PRE##odd##X##_mixed);	 \
		decw	EffectiveAddr;	/* previous address */		 \
	movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al;\
		call	SN(PRE##even##X##_mixed);	 \
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
		call	SN(PRE##text##X);		\
		jmp	update_cont_##X;				\
update_mixed_##X:							\
		call	SN(PRE##text##X##_mixed);		\
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
		andw	$0xFFFF, EffectiveAddr;		/* erase offset */    \
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
		addl	$BANK2, %ecx;					      \
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


/* -------------------------------------------------------------------------
 * setup to plot the text/lores stuff.
 * eax: graphics memory pointer
 * ------------------------------------------------------------------------- */
#define PlotTextPagePre(OFF,GM,PAGE)\
		pushal;					/*Save everything -MUST BE MATCHED!*/\
		xorb	%ah, %ah;\
		movl	%eax, %esi;			/*ESI=EAX=Chr code*/\
		subw	OFF, EffectiveAddr;		/*Normalize scrn addr*/\
							/*Compute row*/\
		movl	SN(video__screen_addresses)(,EffectiveAddr_E,4), %eax;\
		addl	SN(GM), %eax;		/*Graphic addr*/


/* -------------------------------------------------------------------------
 * Common code for plotting an 80 column character.
 * Only 640x400 resolution can do this.
 * eax: graphics memory pointer
 * esi: precalculated font pointer
 * OFF 0x400, 0x800
 * PAGE 0, 1
 * ------------------------------------------------------------------------- */
#define Plot80Character(TAG,OFF,GM,PAGE)\
		PlotTextPagePre(OFF,GM,PAGE)/* does a pushal */\
plot_80character_correct_page##TAG:\
    		addw	%bx, %ax;				/*screen offset*/\
		shll	$6, %esi;				/* * 64 = 8cols * 8rows*/\
		addl	$ Font80, %esi;	/*Font addr*/\
		\
		PlotCharacter80Row;\
		addl	$2, %esi;\
		addl	$ SCANWIDTH-6, %eax;			/*Go to next row*/\
		\
		PlotCharacter80Row;\
		addl	$2, %esi;\
		addl	$ SCANWIDTH-6, %eax;			/*Go to next row*/\
		\
		PlotCharacter80Row;\
		addl	$2, %esi;\
		addl	$ SCANWIDTH-6, %eax;			/*Go to next row*/\
		\
		PlotCharacter80Row;\
		addl	$2, %esi;\
		addl	$ SCANWIDTH-6, %eax;			/*Go to next row*/\
		\
		PlotCharacter80Row;\
		addl	$2, %esi;\
		addl	$ SCANWIDTH-6, %eax;			/*Go to next row*/\
		\
		PlotCharacter80Row;\
		addl	$2, %esi;\
		addl	$ SCANWIDTH-6, %eax;			/*Go to next row*/\
		\
		PlotCharacter80Row;\
		addl	$2, %esi;\
		addl	$ SCANWIDTH-6, %eax;			/*Go to next row*/\
		\
		PlotCharacter80Row;\
		\
		popal;	/* MATCHES pushal from PlotTextPagePre */



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
		xorl	%edi, %edi

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

E(video__write_text0)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$SS_TEXT, SN(softswitches) # Text mode?
		jnz	plot_character0
		testl	$SS_HIRES, SN(softswitches) # lores mode?
		jz	plot_block0
		ret

E(video__write_text0_mixed)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)
                                 # Text or mixed mode?
		jnz	plot_character0
		testl	$SS_HIRES, SN(softswitches) # Not hires mode?
		jz	plot_block0
		ret

E(video__write_text1)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$SS_TEXT, SN(softswitches)
		jnz	plot_character1
		testl	$SS_HIRES, SN(softswitches) # lores mode?
		jz	plot_block1
		ret

E(video__write_text1_mixed)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)
                                 # Text or mixed mode?
		jnz	plot_character1
		testl	$SS_HIRES, SN(softswitches) # Not hires mode?
		jz	plot_block1
		ret

/* video__write_2e_text0 - handle text page //e specific */
E(video__write_2e_text0)
		addl	SN(base_textwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_textwrt), EffectiveAddr_E
iie_soft_write_text0:
		testl	$SS_TEXT, SN(softswitches)	# Text mode?
		jz	iie_write_lores0		# graphics
		testl	$SS_80COL, SN(softswitches)
    		jnz	plot_80character0		# 80 col text
		testl	$SS_TEXTWRT, SN(softswitches)
		jnz	ram_nop				# NOP (in auxram)
		jmp	plot_character0			# 40 col text

iie_write_lores0:
		testl	$(SS_HIRES|SS_TEXTWRT), SN(softswitches)
		jz	plot_block0  # lores & 80col
		ret

/* video__write_2e_text0_mixed - handle mixed text page //e specific */
E(video__write_2e_text0_mixed)
		addl	SN(base_textwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_textwrt), EffectiveAddr_E
iie_soft_write_text0_mixed:
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)
		jz	iie_write_lores0
		testl	$SS_80COL, SN(softswitches)
		jnz	plot_80character0
		testl	$SS_TEXTWRT, SN(softswitches)
		jnz	ram_nop				# NOP (in auxram)
		jmp	plot_character0			# 40 col text

/* video__write_2e_text1 - handle text page1 //e specific */
E(video__write_2e_text1)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_text1:
		testl	$SS_TEXT, SN(softswitches)	# Text mode?
		jz	iie_write_lores1		# graphics
		testl	$SS_80COL, SN(softswitches)
    		jnz	plot_80character1		# 80 col text
		testl	$SS_RAMWRT, SN(softswitches)
		jnz	ram_nop				# NOP (in auxram)
		jmp	plot_character1			# 40 col text

iie_write_lores1:
		testl	$(SS_HIRES|SS_RAMWRT), SN(softswitches) 
		jz	plot_block1	# lores & main bank
		ret


/* video__write_2e_text1_mixed - handle mixed page 1 //e specific */
E(video__write_2e_text1_mixed)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_text1_mixed:
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)
		jz	iie_write_lores1
		testl	$SS_80COL, SN(softswitches)
		jnz	plot_80character1
		testl	$SS_RAMWRT, SN(softswitches)
		jnz	ram_nop				# NOP (in auxram)
		jmp	plot_character1			# 40 col text


E(video__write_even0)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$SS_TEXT, SN(softswitches) # Text mode?
		jnz	ram_nop
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jnz	plot_even_byte0
		ret

E(video__write_even0_mixed)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches) 
                 jnz    ram_nop         # Text/mixed mode?
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jnz	plot_even_byte0
		ret

E(video__write_odd0)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$SS_TEXT, SN(softswitches) # Text mode?
		jnz	ram_nop
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jnz	plot_odd_byte0
		ret

E(video__write_odd0_mixed)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)	
                jnz    ram_nop          # Text/mixed mode?
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jnz	plot_odd_byte0
		ret

/* video__write_2e_even0 - handle hires page //e specific */
E(video__write_2e_even0)
		addl	SN(base_hgrwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_hgrwrt), EffectiveAddr_E
iie_soft_write_even0:
		testl	$SS_TEXT, SN(softswitches)
		jnz	ram_nop				# text
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jz	ram_nop				# lores
		testl	$SS_80COL, SN(softswitches)
    		jz	1f				# not dhires
		testl	$SS_DHIRES, SN(softswitches)# dhires mode?
		jnz	iie_plot_dhires0		# dhires
1:		testl	$SS_HGRWRT, SN(softswitches)
		jnz	ram_nop				# in auxram
		jmp	plot_even_byte0			# plot hires


/* video__write_2e_even0_mixed - handle mixed hires page //e specific */
E(video__write_2e_even0_mixed)
		addl	SN(base_hgrwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_hgrwrt), EffectiveAddr_E
iie_soft_write_even0_mixed:
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)
		jnz	ram_nop	 			# text/mix
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jz	ram_nop				# lores
		testl	$SS_80COL, SN(softswitches)
    		jz	1f				# not dhires
		testl	$SS_DHIRES, SN(softswitches)# dhires mode?
		jnz	iie_plot_dhires0		# dhires
1:		testl	$SS_HGRWRT, SN(softswitches)
		jnz	ram_nop				# in auxram
		jmp	plot_even_byte0			# plot hires

/* video__write_2e_odd0 - handle hires page //e specific */
E(video__write_2e_odd0)
		addl	SN(base_hgrwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_hgrwrt), EffectiveAddr_E
iie_soft_write_odd0:
		testl	$SS_TEXT, SN(softswitches)	# Text mode?
		jnz	ram_nop				# text
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jz	ram_nop				# lores
		testl	$SS_80COL, SN(softswitches)
    		jz	1f				# not dhires
		testl	$SS_DHIRES, SN(softswitches)# dhires mode?
		jnz	iie_plot_dhires0		# dhires
1:		testl	$SS_HGRWRT, SN(softswitches)
		jnz	ram_nop				# in auxram
		jmp	plot_odd_byte0			# plot hires

/* video__write_2e_odd0_mixed - handle mixed hires page //e specific */
E(video__write_2e_odd0_mixed)
		addl	SN(base_hgrwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_hgrwrt), EffectiveAddr_E
iie_soft_write_odd0_mixed:
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)	
		jnz	ram_nop			# text/mix
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jz	ram_nop				# lores
		testl	$SS_80COL, SN(softswitches)
    		jz	1f				# not dhires
		testl	$SS_DHIRES, SN(softswitches) # dhires mode?
		jnz	iie_plot_dhires0		# dhires
1:		testl	$SS_HGRWRT, SN(softswitches)
		jnz	ram_nop				# in auxram
		jmp	plot_odd_byte0			# plot hires

E(video__write_even1)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$SS_TEXT, SN(softswitches) # Text mode?
		jnz	ram_nop
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jnz	plot_even_byte1
		ret

E(video__write_even1_mixed)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$SS_TEXT|SS_MIXED, SN(softswitches)
		jnz	ram_nop # text/mixed
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jnz	plot_even_byte1
		ret

E(video__write_odd1)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl	$SS_TEXT, SN(softswitches)	# Text mode?
		jnz	ram_nop
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jnz	plot_odd_byte1
		ret

E(video__write_odd1_mixed)
		movb	%al, SN(apple_ii_64k)(,EffectiveAddr_E,1)
		testl 	$(SS_TEXT|SS_MIXED), SN(softswitches)
		jnz	ram_nop  # text/mixed
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jnz	plot_odd_byte1
		ret

/* video__write_2e_even1 - write hires page1 //e specific */
E(video__write_2e_even1)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_even1:
		testl	$SS_TEXT, SN(softswitches) # Text mode?
		jnz	ram_nop				# text
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jz	ram_nop				# lores
		testl	$SS_80COL, SN(softswitches)
    		jz	1f				# not dhires
		testl	$SS_DHIRES, SN(softswitches) # dhires mode?
		jnz	iie_plot_dhires1		# dhires
1:		testl	$SS_RAMWRT, SN(softswitches)
		jnz	ram_nop				# in auxram
		jmp	plot_even_byte1			# plot hires

/* video__write_2e_even1_mixed - write hires page1 //e specific */
E(video__write_2e_even1_mixed)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_even1_mixed:
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)
		jnz	ram_nop			# text/mix
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jz	ram_nop				# lores
		testl	$SS_80COL, SN(softswitches)
    		jz	1f				# not dhires
		testl	$SS_DHIRES, SN(softswitches)# dhires mode?
		jnz	iie_plot_dhires1		# dhires
1:		testl	$SS_RAMWRT, SN(softswitches)
		jnz	ram_nop				# in auxram
		jmp	plot_even_byte1			# plot hires

/* video__write_2e_odd1 - write hires page1 //e specific */
E(video__write_2e_odd1)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_odd1:
		testl	$SS_TEXT, SN(softswitches)	# Text mode?
		jnz	ram_nop				# text
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jz	ram_nop				# lores
		testl	$SS_80COL, SN(softswitches)
    		jz	1f				# not dhires
		testl	$SS_DHIRES, SN(softswitches)# dhires mode?
		jnz	iie_plot_dhires1		# dhires
1:		testl	$SS_RAMWRT, SN(softswitches) 
		jnz	ram_nop				# in auxram
_iie_plot_hires_page1_odd:
		jmp	plot_odd_byte1			# plot hires

/* video__write_2e_odd1_mixed - write hires page1 //e specific */
E(video__write_2e_odd1_mixed)
		addl	SN(base_ramwrt), EffectiveAddr_E
		movb	%al, (EffectiveAddr_E)
		subl	SN(base_ramwrt), EffectiveAddr_E
iie_soft_write_odd1_mixed:
		testl	$(SS_TEXT|SS_MIXED), SN(softswitches)
		jnz	ram_nop			# text/mix
		testl	$SS_HIRES, SN(softswitches) # hires mode?
		jz	ram_nop				# lores
		testl	$SS_80COL, SN(softswitches)
    		jz	1f				# not dhires
		testl	$SS_DHIRES, SN(softswitches)# dhires mode?
		jnz	iie_plot_dhires1		# dhires
1:		testl	$SS_RAMWRT, SN(softswitches)
		jnz	ram_nop				# in auxram
		jmp	plot_odd_byte1			# plot hires

		.align	4
iie_plot_dhires0:
		PlotDHires($0x2000,0,video__fb1)
		ret

iie_plot_dhires1:
		PlotDHires($0x4000,1,video__fb2)
		ret


		.align 4
plot_80character0:
		pushl	%ebx
		pushl	EffectiveAddr_E

		orl	$0x10000, EffectiveAddr_E	# aux ram
		movl	$0, %ebx			# +0 screen offset
		movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al
		andl	$0xFFFF, EffectiveAddr_E
		Plot80Character(0a,$0x400,video__fb1,$0)

		popl	EffectiveAddr_E			# main ram
		movl	$7, %ebx			# +7 screen offset
		movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al
		Plot80Character(0b,$0x400,video__fb1,$0)

		popl	%ebx
		ret

		.align 4
plot_80character1:
		pushl	%ebx
		pushl	EffectiveAddr_E

		orl	$0x10000, EffectiveAddr_E	# aux ram
		movl	$0, %ebx			# +0 screen offset
		movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al
		andl	$0xFFFF, EffectiveAddr_E
		Plot80Character(1a,$0x800,video__fb2,$1)

		popl	EffectiveAddr_E			# main ram
		movl	$7, %ebx			# +7 screen offset
		movb	SN(apple_ii_64k)(,EffectiveAddr_E,1), %al
		Plot80Character(1b,$0x800,video__fb2,$1)

		popl	%ebx
		ret


/* plot character on first text page */
		.align 4
plot_character0:
		PlotTextPagePre($0x400,video__fb1,$0)
plot_character_correct_page:
		shll	$7, %esi				# * 128
		addl	$ Font, %esi				# Font addr

		PlotCharacter40Row
		addl	$2, %esi;
		addl	$ SCANSTEP, %eax			# Go to next row

		PlotCharacter40Row
		addl	$2, %esi;
		addl	$ SCANSTEP, %eax			# Go to next row

		PlotCharacter40Row
		addl	$2, %esi;
		addl	$ SCANSTEP, %eax			# Go to next row

		PlotCharacter40Row
		addl	$2, %esi;
		addl	$ SCANSTEP, %eax			# Go to next row

		PlotCharacter40Row
		addl	$2, %esi;
		addl	$ SCANSTEP, %eax			# Go to next row

		PlotCharacter40Row
		addl	$2, %esi;
		addl	$ SCANSTEP, %eax			# Go to next row

		PlotCharacter40Row
		addl	$2, %esi;
		addl	$ SCANSTEP, %eax			# Go to next row

		PlotCharacter40Row

		popal
		ret


/* plot character on second text page */
		.align 4
plot_character1:
		PlotTextPagePre($0x800,video__fb2,$1)
		jmp	plot_character_correct_page	# same as page 0


/* plot lores block first page */
	
plot_block0:
		PlotTextPagePre($0x400,video__fb1,$0)
plot_block_correct_page:
		movw	%si, %dx			# Compute color
		andb	$0x0F, %dl
		shlb	$4, %dl
		movb	%dl, %dh
		shll	$16, %edx
		movw	%si, %dx
		andb	$0x0F, %dl
		shlb	$4, %dl
		movb	%dl, %dh

		PlotBlockRow
		addl	$ SCANSTEP, %eax		# Go to next row

		PlotBlockRow
		addl	$ SCANSTEP, %eax		# Go to next row

		PlotBlockRow
		addl	$ SCANSTEP, %eax		# Go to next row

		PlotBlockRow
		addl	$ SCANSTEP, %eax		# Go to next row

		movw	%si, %dx			# Compute color
		andb	$0xF0, %dl
		movb	%dl, %dh
		shll	$16, %edx
		movw	%si, %dx
		andb	$0xF0, %dl
		movb	%dl, %dh

		PlotBlockRow
		addl	$ SCANSTEP, %eax		# Go to next row

		PlotBlockRow
		addl	$ SCANSTEP, %eax		# Go to next row

		PlotBlockRow
		addl	$ SCANSTEP, %eax		# Go to next row

		PlotBlockRow

		popal
		ret

		.align 4
plot_block1:
		PlotTextPagePre($0x800,video__fb2,$1)
    		jmp plot_block_correct_page
	
	

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
	
