/*
 * Apple // emulator for Linux: Keyboard definitions
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

#include "common.h"

#ifndef A2_KEYS_H
#define A2_KEYS_H

#define SCODE_BS 14
#define SCODE_L_CTRL    29
#define SCODE_R_CTRL    97
#define SCODE_L_SHIFT   42
#define SCODE_R_SHIFT   54
#define SCODE_CAPS      58
#define SCODE_J_U       72
#define SCODE_J_D       80
#define SCODE_J_L       75
#define SCODE_J_R       77
#define SCODE_J_C       76

#define kF1     128
#define kF2     129
#define kF3     130
#define kF4     131
#define kF5     132
#define kF6     133
#define kF7     134
#define kF8     135
#define kF9     136
#define kF10    137
#define kF11    138
#define kF12    139

#define J_U     141
#define J_D     142
#define J_L     143
#define J_R     144
#define JUL     145
#define JUR     146
#define JDL     147
#define JDR     148

#define JB0     149
#define JB1     150
#define JB2     151

#define S_D     152
#define S_I     153
#define J_C     154
#define kPAUSE  155

#define kLEFT   8  /* 157 */
#define kRIGHT  21 /* 158 */
#define kUP     11 /* 159 */
#define kDOWN   10 /* 160 */

#define kESC    27 /* 161 */
#define kPGUP   162
#define kHOME   163
#define kPGDN   164
#define kEND    165

extern pthread_mutex_t interface_mutex;

#ifdef PC_JOYSTICK
extern int js_fd;
extern struct JS_DATA_TYPE js;
extern int js_offset_x, js_offset_y;
extern float js_adjustlow_x, js_adjustlow_y, js_adjusthigh_x, js_adjusthigh_y;
#endif

void enter_debugger(void);
int c_mygetch(int block);
int c_rawkey();
void c_keys_set_key(int key);
bool c_keys_is_interface_key(int key);

#endif
