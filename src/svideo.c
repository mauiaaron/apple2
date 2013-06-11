/* 
 * Apple // emulator for Linux:	svgalib graphics support
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

#include <vga.h>
#include <vgakeyboard.h>
#include <string.h>
#include <stdio.h>
#include <sys/kd.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "video.h"
#include "misc.h"
#include "keys.h"

/* I need at least 560x192 for 80col.  it seems that direct linear
 * framebuffer access to anything in the realm of 640x480x256 not
 * standard with vga/svga?
 * Memory sizes are hardcode here.  ugly ugly ugly.
 */

static unsigned char	*svga_GM;			/* SVGA base address of graphic area */
static unsigned char	vga_mem_page_0[64000];		/* page0 framebuffer */
static unsigned char	vga_mem_page_1[64000];		/* page1 framebuffer */


/* -------------------------------------------------------------------------
    video_setpage(p):    Switch to screen page p
   ------------------------------------------------------------------------- */
void video_setpage(int p)
{
    if (p == video__current_page) return;

    if (p)
    {
	memcpy(vga_mem_page_0,svga_GM,64000);
	memcpy(svga_GM,vga_mem_page_1,64000);
	video__current_page = 1;
	video__fb1 = vga_mem_page_0;
	video__fb2 = svga_GM;
    }
    else
    {
	memcpy(vga_mem_page_1,svga_GM,64000);
	memcpy(svga_GM,vga_mem_page_0,64000);
	video__current_page = 0;
	video__fb1 = svga_GM;
	video__fb2 = vga_mem_page_1;
    }
}

/* -------------------------------------------------------------------------
    c_initialize_colors():    Initialize color palette
   ------------------------------------------------------------------------- */

static void c_initialize_colors()
{

    static unsigned int col2[ 3 ] = { 27, 40, 62 };

    int				i, j;

    /* align the palette for hires graphics */
    for (i = 0; i < 8; i++)
	for (j = 0; j < 3; j++)
	    vga_setpalette( j+i*3+32, (i & 1) ? col2[ j ] : 0,
                                      (i & 2) ? col2[ j ] : 0,
                                      (i & 4) ? col2[ j ] : 0 );

    vga_setpalette( COLOR_FLASHING_BLACK, 0, 0, 0 );
    vga_setpalette( COLOR_FLASHING_WHITE, 63, 63, 63 );

    /* dhires colors are bass-ackwards because it's easier for me to
       deal with the endianness. */
    vga_setpalette( 0x0, 0, 0, 0 ); 	/* (0) Black 		*/
    vga_setpalette( 0x1, 0, 0, 32 );	/* (2) Dark Blue	*/
    vga_setpalette( 0x2, 0, 36, 0 );	/* (4) Dark Green	*/
    vga_setpalette( 0x3, 28, 6, 63 );	/* (6) Medium Blue	*/
    vga_setpalette( 0x4, 37, 21, 10 );	/* (8) Brown		*/
    vga_setpalette( 0x5, 37, 42, 42 );	/* (a) Light Grey	*/
    vga_setpalette( 0x6, 0, 63, 0 );	/* (c) Green		*/
    vga_setpalette( 0x7, 32, 63, 32 );	/* (e) Aqua		*/
    vga_setpalette( 0x8, 48, 0, 12 );	/* (1) Magenta		*/
    vga_setpalette( 0x9, 41, 13, 42 );	/* (3) Purple		*/
    vga_setpalette( 0xa, 26, 26, 26 );	/* (5) Light Grey	*/
    vga_setpalette( 0xb, 3, 47, 58 );	/* (7) Light Blue	*/
    vga_setpalette( 0xc, 63, 6, 11 );	/* (9) Orange		*/
    vga_setpalette( 0xd, 63, 39, 37 );	/* (b) Pink		*/
    vga_setpalette( 0xe, 63, 63, 0 );	/* (d) Yellow		*/
    vga_setpalette( 0xf, 63, 63, 63 );	/* (f) White		*/

    /* lores colors (<<4) */
    vga_setpalette(0x10, 48, 0, 12 );	/* Magenta 		*/
    vga_setpalette(0x20, 0, 0, 32 );	/* Dark Blue 		*/
    vga_setpalette(0x30, 41, 13, 42 );	/* Purple 		*/
    vga_setpalette(0x40, 0, 36, 0 );	/* Dark Green 		*/
    vga_setpalette(0x50, 26, 26, 26 );	/* Dark Gray 		*/
    vga_setpalette(0x60, 28, 6, 63 );	/* Medium Blue 		*/
    vga_setpalette(0x70, 3, 47, 58 );	/* Light Blue		*/
    vga_setpalette(0x80, 37, 21, 10 );	/* Brown 		*/
    vga_setpalette(0x90, 63, 6, 11 );	/* Orange 		*/
    vga_setpalette(0xa0, 37, 42, 42 );	/* Light Grey		*/
    vga_setpalette(0xb0, 63, 39, 37 );	/* Pink 		*/
    vga_setpalette(0xc0, 0, 63, 0 );	/* Green 		*/
    vga_setpalette(0xd0, 63, 63, 0 );	/* Yellow 		*/
    vga_setpalette(0xe0, 32, 63, 32 );	/* Aqua 		*/
    vga_setpalette(0xf0, 63, 63, 63 );	/* White 		*/
}

/* -------------------------------------------------------------------------
    c_initialize_keyboard()
   ------------------------------------------------------------------------- */

static void c_initialize_keyboard()
{
    if (keyboard_init()) {
    	printf("Error: Could not switch to RAW keyboard mode.\n");
	exit(-1);
    }

    keyboard_translatekeys(DONT_CATCH_CTRLC);
    keyboard_seteventhandler( c_read_raw_key );
}

static void c_flash_cursor(int on) {
    if (!on) {
	vga_setpalette( COLOR_FLASHING_BLACK, 0, 0, 0 );
	vga_setpalette( COLOR_FLASHING_WHITE, 63, 63, 63 );
    } else {
	vga_setpalette( COLOR_FLASHING_BLACK, 63, 63, 63 );
	vga_setpalette( COLOR_FLASHING_WHITE, 0, 0, 0 );	
    }
}

/* -------------------------------------------------------------------------
    video_init()
   ------------------------------------------------------------------------- */
void video_init(void) {
    /* I'm forcing VGA mode since it seems to be supported by
     * the lowest common denominator cards.
     */
    printf("Using standard VGA 320x200 mode.\n");
    printf("Press RETURN to continue...");
    getchar();

    vga_setchipset( VGA );
    if (vga_init()) {
	printf("Cannot initialize svgalib!\n");
	exit(1);
    }

    vga_setmode( G320x200x256 );

/*    vga_claimvideomemory( 131072 );*/
    c_initialize_keyboard();
    svga_GM = video__fb1 = vga_getgraphmem();
    video__fb2 = vga_mem_page_1;

    memset(video__fb1,0,64000); /* start as black */
    memset(video__fb2,0,64000);

    c_initialize_colors();






}

void video_shutdown(void) {
    vga_setmode(TEXT);
    keyboard_close();
}

void video_sync(int block)
{
    static int flash_count;

    if (block) 
    {
	/* keyboard_waitforupdate();    --- seems to cause bad crashes... */
	usleep(TIMER_DELAY);
        keyboard_update();
    }
    else
	keyboard_update();

    switch (++flash_count)
    {
    case 6: 
        c_flash_cursor(1);
        break;
    case 12: 
        c_flash_cursor(0);
        flash_count = 0; 
        break;
    default:
        break;
    }
}
