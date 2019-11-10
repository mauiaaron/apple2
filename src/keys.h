/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#include "common.h"

#ifndef A2_KEYS_H
#define A2_KEYS_H

// NOTES:
// Ugh, key mapping is a bit schizophrenic...  It was originally based on
// Linux/SVGAlib raw scancodes that were mapped to a regular or special keycode
// through a particular map that corresponded to whether a modifier key such as
// shift, control, caps, or a combo, or no modifer was also pressed.  The
// emulator makes use of both the raw scancodes and the mapped values.

// ----------------------------------------------------------------------------
// Scancodes aka "raw" values (range 0-127)

// raw key scancodes

#define SCODE_ESC       1

#define SCODE_BS        14
#define SCODE_TAB       15
#define SCODE_RET       28
#define SCODE_L_CTRL    29
#define SCODE_L_SHIFT   42
#define SCODE_R_SHIFT   54
#define SCODE_L_ALT     56
#define SCODE_CAPS      58

#define SCODE_F1        59
#define SCODE_F2        60
#define SCODE_F3        61
#define SCODE_F4        62
#define SCODE_F5        63
#define SCODE_F6        64
#define SCODE_F7        65
#define SCODE_F8        66
#define SCODE_F9        67
#define SCODE_F10       68
#define SCODE_F11       69
#define SCODE_F12       70

#define SCODE_KPAD_UL   71
#define SCODE_KPAD_U    72
#define SCODE_KPAD_UR   73
#define SCODE_KPAD_L    75
#define SCODE_KPAD_C    76
#define SCODE_KPAD_R    77
#define SCODE_KPAD_DL   79
#define SCODE_KPAD_D    80
#define SCODE_KPAD_DR   81

#define SCODE_R_CTRL    97
#define SCODE_PRNT      99
#define SCODE_R_ALT     100
#define SCODE_BRK       101

#define SCODE_HOME      102
#define SCODE_U         103
#define SCODE_PGUP      104
#define SCODE_L         105
#define SCODE_R         106
#define SCODE_END       107
#define SCODE_D         108
#define SCODE_PGDN      109
#define SCODE_INS       110
#define SCODE_DEL       111

#define SCODE_PAUSE     119

// ----------------------------------------------------------------------------
// Keycodes (range 0-255)

// Apple //e special keycodes

#define kLT     8
#define kTAB    9
#define kDN     10
#define kUP     11
#define kRET    13
#define kRT     21
#define kESC    27
#define kDEL    127

// Emulator special keycodes (> 127)

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

// Emulated Joystick
#define J_U     141
#define J_D     142
#define J_L     143
#define J_R     144
#define JUL     145
#define JUR     146
#define JDL     147
#define JDR     148
#define J_C     154

#define JB0     149 // Alt-L
#define JB1     150 // Alt-R
#define JB2     151 // NOTE : unused ...

#define kPAUSE  155

#define kPGUP   162 // Also : JUR
#define kHOME   163 // Also : JUL
#define kPGDN   164 // Also : JDR
#define kEND    165 // Also : JDL

// ----------------------------------------------------------------------------

extern bool caps_lock;
extern bool use_system_caps_lock;

#ifdef INTERFACE_CLASSIC
int c_mygetch(int block);
int c_rawkey(void);
void c_keys_set_key(int key);
bool c_keys_is_interface_key(int key);
#endif

bool keys_isShifted(void);
int keys_ascii2Scancode(uint8_t ch);
void keys_handleInput(int scan_or_key, bool is_pressed, bool is_ascii);
long keys_consumeLastKey(void);

int keys_scancode2ASCII(int scancode, bool is_shifted, bool is_ctrl);
uint8_t keys_apple2ASCII(uint8_t c, OUTPARM font_mode_t *mode);

#endif

