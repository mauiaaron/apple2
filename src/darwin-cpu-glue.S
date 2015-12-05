/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2015 Aaron Culliney
 *
 */

#ifdef __APPLE__
#   if defined(__arm__)
#       include "arm/cpu.S"
#       include "arm/glue.S"
#   elif defined(__arm64__)
#       error aarch64 not implemented ... TODO SOON
#   else
// simulator is x86
#       include "x86/cpu.S"
#       include "x86/glue.S"
#   endif
#endif
