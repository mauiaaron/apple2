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


/* Symbol naming issues */
#ifdef NO_UNDERSCORES
#define         SN(foo) foo
#define         E(foo)          .globl foo; ALIGN; foo##:
#else /* !NO_UNDERSCORES */
#define         SN(foo) _##foo
#define         E(foo)          .globl _##foo; ALIGN; _##foo##:
#endif /* !NO_UNDERSCORES */

#endif /* _A2_H_ */
