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

#define INTERFACE_SCREEN_X 80

typedef enum interface_colorscheme_t {
    GREEN_ON_BLACK = 0,
    GREEN_ON_BLUE,
    RED_ON_BLACK,
} interface_colorscheme_t;

#ifdef INTERFACE_CLASSIC
extern bool in_interface;
void video_plotchar(int col, int row, interface_colorscheme_t cs, uint8_t c);
void c_interface_begin(int current_key);
void c_interface_print(int x, int y, const interface_colorscheme_t cs, const char *s);
void c_interface_print_submenu_centered(char *submenu, int xlen, int ylen);
void c_interface_keyboard_layout();
void c_interface_parameters();
void c_interface_credits();
void c_interface_exit(int ch);
void c_interface_translate_screen(char screen[24][INTERFACE_SCREEN_X+1]);
void c_interface_select_diskette(int);
#endif

// Plots a character into the specified framebuffer
void interface_plotChar(uint8_t *fb, int fb_pix_width, int col, int row, interface_colorscheme_t cs, uint8_t c);

// Plots a string/template into the specified framebuffer
void interface_printMessage(uint8_t *fb, int fb_pix_width, int col, int row, interface_colorscheme_t cs, const char *message);

// Plots a string/template into the specified framebuffer
void interface_printMessageCentered(uint8_t *fb, int fb_cols, int fb_rows, interface_colorscheme_t cs, char *message, int message_cols, int message_rows);

#endif
