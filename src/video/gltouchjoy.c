/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

// GL touch joystick -- Created by Aaron Culliney

#include "common.h"

#include "video_util/modelUtil.h"
#include "video_util/matrixUtil.h"
#include "video_util/sourceUtil.h"

#ifdef __APPLE__
#import <CoreFoundation/CoreFoundation.h>
#endif

// VAO optimization (may not be available on all platforms)
#define USE_VAO 0

void gldriver_joystick_reset(void) {
}

