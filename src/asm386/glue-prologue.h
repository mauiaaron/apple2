/* 
 * Apple // emulator for Linux: Glue file prologue for Intel 386
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
#include "misc.h"

#define GLUE_FIXED_READ(func,address)			\
E(func)			movb	SN(address)(%edi),%al;  \
			ret;

#define GLUE_FIXED_WRITE(func,address)                  \
E(func)			movb	%al,SN(address)(%edi);  \
			ret;

#define GLUE_BANK_MAYBEREAD(func,pointer) \
E(func)                 testl   $SS_CXROM, SN(softswitches); \
                        jnz     1f; \
                        call    *SN(pointer); \
                        ret; \
1:                      addl    SN(pointer),%edi; \
                        movb    (%edi),%al; \
                        subl    SN(pointer),%edi; \
                        ret;

#define GLUE_BANK_READ(func,pointer) \
E(func)			addl	SN(pointer),%edi;	\
			movb	(%edi),%al;		\
			subl	SN(pointer),%edi;	\
			ret;

#define GLUE_BANK_WRITE(func,pointer) \
E(func)			addl	SN(pointer),%edi;	\
			movb	%al,(%edi);		\
			subl	SN(pointer),%edi;	\
			ret;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
E(func)			addl	SN(pointer),%edi;	\
			cmpl	$0,SN(pointer);		\
			jz	1f;			\
			movb	%al,(%edi);		\
1:			ret;


// TODO FIXME : implement CDECL prologue/epilogues...
#define GLUE_C_WRITE(func) \
E(func)			pushl	%eax;			\
			pushl	%ecx;			\
			pushl	%edx;			\
			andl	$0xff,%eax;		\
			pushl	%eax;			\
			pushl	%edi;			\
			call	SN(c_##func);           \
			popl	%edx; /* dummy */	\
			popl	%edx; /* dummy */	\
			popl	%edx;			\
			popl	%ecx;			\
			popl	%eax;			\
			ret;				\

// TODO FIXME : implement CDECL prologue/epilogues...
#define GLUE_C_READ(func) \
E(func)			/*pushl	%eax;*/			\
			pushl	%ecx;			\
			pushl	%edx;			\
			pushl	%edi;			\
			call	SN(c_##func);           \
			/*movb	%al,12(%esp);*/		\
			popl	%edx; /* dummy */	\
			popl	%edx;			\
			popl	%ecx;			\
			/*popl	%eax;*/			\
			ret;

