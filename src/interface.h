/*
 * Apple // emulator for Linux: Exported menu routines
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

#ifndef A2_INTERFACE_H
#define A2_INTERFACE_H

#include "video/video.h"

extern bool in_interface;

void c_interface_begin(int current_key);
void c_interface_print(int x, int y, const int cs, const char *s);
void c_interface_print_submenu_centered(char *submenu, const int xlen, const int ylen);
void c_interface_keyboard_layout();
void c_interface_parameters();
void c_interface_credits();
void c_interface_exit(int ch);
void c_interface_translate_screen(char screen[24][INTERFACE_SCREEN_X+1]);
void c_interface_select_diskette(int);
#endif
