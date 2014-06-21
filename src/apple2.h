/*
 * Apple // emulator for Linux: Common definitions
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

#ifndef _A2_H_
#define _A2_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Virtual machine is an Apple ][ (not an NES, etc...)
#define APPLE2_VM 1

/* Symbol naming issues */
#ifdef NO_UNDERSCORES
#define         SN(foo) foo
#define         SNX(foo, INDEX, SCALE) _##foo(%rip,INDEX,SCALE)
#define         E(foo)          .globl foo; .balign 16; foo##:
#define         CALL(foo) foo
#else /* !NO_UNDERSCORES */
#if defined(__APPLE__)
#define         SN(foo) _##foo(%rip)
#define         SNX(foo, INDEX, SCALE) _##foo(%rip,INDEX,SCALE)
#else
#define         SN(foo) _##foo
#define         SNX(foo, INDEX, SCALE) _##foo(,INDEX,SCALE)
#endif
#define         E(foo)          .globl _##foo; .balign 16; _##foo##:
#define         CALL(foo) _##foo
#endif /* !NO_UNDERSCORES */

#endif /* _A2_H_ */
