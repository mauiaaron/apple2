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
#if 1
#define PlotDHiresByte\
                xorl    %edx, %edx;\
		movb	SN(video__dhires2)(,%ebx,1), %dl;\
		/*movb	%dl, %dh;*/\
		shll	$16, %edx;\
		movb	SN(video__dhires1)(,%ebx,1), %dl;\
		/*movb	%dl, %dh;*/\
		movl	%edx, (%eax);\
		movl	%edx, SCANWIDTH(%eax);
#else
#define PlotDHiresByte\
                pushal;\
                pushl %eax;\
                pushl %ebx;\
                call _plot_dhires_pixels;\
                popl %ebx;\
                popl %eax;\
                popal;
#endif

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
		movzbl	%bl, %ebx;					      \
		subl	$4, %eax;\
		PlotDHiresByte\
		addl	$4, %eax;\
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
		addl	$4, %eax;\
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		addl	$4, %eax;\
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		addl	$4, %eax;\
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		addl	$4, %eax;\
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		addl	$4, %eax;\
		shrl	$4, %ecx;					      \
		movb	%cl, %bl;					      \
		PlotDHiresByte						      \
		addl	$4, %eax;\
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

		/******************************************/
	
E(iie_plot_dhires0)
		PlotDHires($0x2000,0,video__fb1)
		ret

E(iie_plot_dhires1)
		PlotDHires($0x4000,1,video__fb2)
		ret

