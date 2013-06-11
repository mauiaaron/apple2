/* 
 * Apple // emulator for Linux: Video support
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

#include "video.h"
#include "cpu.h"
#include "misc.h"
#include "prefs.h"

#ifdef _640x400
unsigned char video__wider_font[0x8000];
#endif  /* _640x400 */

unsigned char video__font[0x4000];

/* --- Precalculated hi-res page offsets given addr --- */
unsigned int	video__screen_addresses[8192];
unsigned char	video__columns[8192];

unsigned char	*video__fb1,*video__fb2;

#ifdef _640x400
unsigned char	video__wider_hires_even[0x1000];
unsigned char	video__wider_hires_odd[0x1000];
#endif
unsigned char	video__hires_even[0x800];
unsigned char	video__hires_odd[0x800];

unsigned char	video__dhires1[256];
unsigned char	video__dhires2[256];

/* Interface font:
 *  (probably could be made static) 
 *
 * Unlike the normal font, only one version is stored, since the interface
 * is always displayed in forty columns.
 */
#ifdef _640x400
unsigned char   video__wider_int_font[3][0x8000];
#else /* _640x400 */
unsigned char   video__int_font[3][0x4000];
#endif /* _640x400 */

int video__current_page;	/* Current visual page */

int video__strictcolors;

void video_loadfont(int first, 
                    int quantity, 
                    const unsigned char *data,
                    int mode)
{ 
    int i,j;
    unsigned char x,y,fg,bg;

    switch(mode)
    {
    case 2:
       fg = COLOR_BLACK; bg = COLOR_LIGHT_WHITE; break;
    case 3:
       fg = COLOR_FLASHING_WHITE; bg = COLOR_FLASHING_BLACK; break;
    default: 
       fg = COLOR_LIGHT_WHITE; bg = COLOR_BLACK; break;
    }

    i = quantity * 8;

    while (i--)	
    {
        j = 8;
        x = data[i];
        while (j--)
        {
	    y = (x & 128) ? fg : bg;

#ifdef _640x400
	    video__wider_font[(first << 7) + (i << 4) + (j << 1)] =
	    video__wider_font[(first << 7) + (i << 4) + (j << 1) + 1] =
#endif /* _640x400 */ 
            video__font[(first << 6) + (i << 3) + j] = y;
            x <<= 1; 
        }
    }
}

unsigned char	video__odd_colors[2] = {COLOR_LIGHT_PURPLE, COLOR_LIGHT_BLUE};
unsigned char	video__even_colors[2] = {COLOR_LIGHT_GREEN, COLOR_LIGHT_RED};

/* 40col/80col/lores/hires/dhires line offsets */
unsigned short video__line_offset[24] =
		   { 0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
		     0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,
		     0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0 };

unsigned char video__dhires1[256] = {
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x3,0x3,0x5,0x5,0x7,0x7,0x9,0x9,0xb,0xb,0xd,0xd,0xf,0xf,
    0x0,0x1,0x2,0x3,0x6,0x5,0x6,0x7,0xa,0x9,0xa,0xb,0xe,0xd,0xe,0xf,
    0x0,0x1,0x3,0x3,0x7,0x5,0x7,0x7,0xb,0x9,0xb,0xb,0xf,0xd,0xf,0xf,
    0x0,0x1,0x2,0x3,0x4,0x4,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x3,0x3,0x5,0x5,0x7,0x7,0xd,0x9,0xb,0xb,0xd,0xd,0xf,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0xe,0x9,0xa,0xb,0xe,0xd,0xe,0xf,
    0x0,0x1,0x7,0x3,0x7,0x5,0x7,0x7,0xf,0x9,0xb,0xb,0xf,0xd,0xf,0xf,
};

unsigned char video__dhires2[256] = {
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x8,0x8,0x0,0xb,0x8,0xd,0x0,0x0,
    0x1,0x1,0x1,0x1,0x0,0x5,0x1,0x1,0x0,0x9,0xb,0xb,0x0,0xd,0xf,0xf,
    0x0,0x1,0x2,0x2,0x2,0x5,0x2,0x2,0x0,0xa,0xa,0xa,0xe,0xd,0x2,0x2,
    0x3,0x3,0x3,0x3,0x7,0x5,0x7,0x7,0x0,0xb,0xb,0xb,0xf,0xd,0xf,0xf,
    0x0,0x0,0x4,0x0,0x4,0x4,0x4,0x4,0xc,0x8,0x4,0x8,0xc,0xd,0x4,0x4,
    0x5,0x5,0x5,0x5,0x5,0x5,0x5,0x5,0xd,0x4,0x4,0x4,0xd,0xd,0x4,0x4,
    0x6,0x6,0x6,0x2,0xe,0x6,0x6,0x6,0xe,0xe,0xa,0xa,0xe,0x6,0xe,0x6,
    0x7,0x7,0x7,0x7,0x7,0x7,0x7,0x7,0xf,0xf,0xb,0xb,0xf,0xf,0xf,0xf,
};

#ifdef APPLE_IIE

/* -------------------------------------------------------------------------
    c_initialize_dhires_values()
   ------------------------------------------------------------------------- */
static void c_initialize_dhires_values(void) {
    int i;
/*     int value, v; */
/*     unsigned char locolor, hicolor; */

    /* precalculate the colors for all the 256*8 bit combinations. */
/*     for (value = 0x00, v = 0; value <= 0xFF; value++) { */
/* 	locolor = (value & 0x0F) | 0x10; */
/* 	hicolor = (value << 4) | 0x10; */

/* 	dhires_colors[v++] = locolor; */
/* 	dhires_colors[v++] = locolor; */
/* 	dhires_colors[v++] = locolor; */
/* 	dhires_colors[v++] = locolor; */

/* 	dhires_colors[v++] = hicolor; */
/* 	dhires_colors[v++] = hicolor; */
/* 	dhires_colors[v++] = hicolor; */
/* 	dhires_colors[v++] = hicolor; */
/*     } */

    for (i = 0; i < 0x80; i++) {
	video__dhires1[i+0x80] = video__dhires1[i];
	video__dhires2[i+0x80] = video__dhires2[i];
    }
}
#endif

/* -------------------------------------------------------------------------
    c_initialize_hires_values()
   ------------------------------------------------------------------------- */

static void c_initialize_hires_values(void)
{
    int     value, b, v, e, /*color_toggle,*/ last_not_black;

    /* precalculate the colors for all the 256*8 bit combinations. */
    for (value = 0x00; value <= 0xFF; value++) {
	for (e = value * 8, last_not_black = 0, v = value, b = 0;
	     b < 7; b++, v >>= 1, e++)
	{
	    if (v & 1)
	    {
		video__hires_even[ e ] = last_not_black ?
					    COLOR_LIGHT_WHITE :
					((b & 1) ?
					    ((value & 0x80) ?
					         COLOR_LIGHT_RED :
						 COLOR_LIGHT_GREEN) :
					    ((value & 0x80) ?
						 COLOR_LIGHT_BLUE :
						 COLOR_LIGHT_PURPLE));

		video__hires_odd[ e ] = last_not_black ?
					    COLOR_LIGHT_WHITE :
					((b & 1) ?
					    ((value & 0x80) ?
					         COLOR_LIGHT_BLUE :
						 COLOR_LIGHT_PURPLE) :
					    ((value & 0x80) ?
						 COLOR_LIGHT_RED :
						 COLOR_LIGHT_GREEN));

		if (last_not_black && b > 0)
		    video__hires_even[ e - 1 ] = COLOR_LIGHT_WHITE,
		    video__hires_odd[ e - 1 ] = COLOR_LIGHT_WHITE;

		last_not_black = 1;
	    }
	    else
		video__hires_even[ e ] = COLOR_BLACK,
		video__hires_odd[ e ] = COLOR_BLACK,
	        last_not_black = 0;
	}
    }
    if (color_mode == 0) {	/* Black and White */
        for (value = 0x00; value <= 0xFF; value++) {
	    for (b = 0, e = value * 8; b < 7; b++, e++)	{
		if (video__hires_even[ e ] != COLOR_BLACK)
		    video__hires_even[ e ] = COLOR_LIGHT_WHITE;
		if (video__hires_odd[ e ] != COLOR_BLACK)
		    video__hires_odd[ e ] = COLOR_LIGHT_WHITE;
	    }
	}
    }
    else if (color_mode == LAZY_INTERP) /* Lazy Interpolated color */
    {
        for (value = 0x00; value <= 0xFF; value++)
        {
            for (b = 1, e = value * 8 + 1; b <= 5; b += 2, e += 2)
            {
                if (video__hires_even[ e ] == COLOR_BLACK &&
                    video__hires_even[ e - 1 ] != COLOR_BLACK &&
                    video__hires_even[ e + 1 ] != COLOR_BLACK)
                      video__hires_even[ e ] =
                      video__hires_even[ e - 1 ];

                if (video__hires_odd[ e ] == COLOR_BLACK &&
                    video__hires_odd[ e - 1 ] != COLOR_BLACK &&
                    video__hires_odd[ e + 1 ] != COLOR_BLACK)
                      video__hires_odd[ e ] =
                      video__hires_odd[ e - 1 ];
            }

            for (b = 0, e = value * 8; b <= 6; b += 2, e += 2) {
                if (video__hires_odd[ e ] == COLOR_BLACK) {
                    if (b > 0 && b < 6) {
                    if (video__hires_even[e+1] != COLOR_BLACK &&
                        video__hires_even[e-1] != COLOR_BLACK &&
                        video__hires_even[e+1] != COLOR_LIGHT_WHITE &&
                        video__hires_even[e-1] != COLOR_LIGHT_WHITE)
                        video__hires_even[e] =
                            video__hires_even[e-1];
                    } else if (b == 0) {
                    if (video__hires_even[e+1] != COLOR_BLACK &&
                        video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                        video__hires_even[e] =
                            video__hires_even[e+1];
                    } else {
                    if (video__hires_even[e-1] != COLOR_BLACK &&
                        video__hires_even[e-1] != COLOR_LIGHT_WHITE)
                        video__hires_even[ e ] =
                            video__hires_even[ e - 1 ];
                    }
                }

                if (video__hires_odd[ e ] == COLOR_BLACK) {
                    if (b > 0 && b < 6) {
                    if (video__hires_odd[e+1] != COLOR_BLACK &&
                        video__hires_odd[e-1] != COLOR_BLACK &&
                        video__hires_odd[e+1] != COLOR_LIGHT_WHITE &&
                        video__hires_odd[e-1] != COLOR_LIGHT_WHITE)
                        video__hires_odd[e] =
                            video__hires_odd[e-1];
                    }
                    else if (b == 0) {
                    if (video__hires_odd[e+1] != COLOR_BLACK &&
                        video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                        video__hires_odd[e] =
                            video__hires_odd[e+1];
                    }
                    else if
                        (video__hires_odd[e-1] != COLOR_BLACK &&
                         video__hires_odd[e-1] != COLOR_LIGHT_WHITE)
                        video__hires_odd[e] =
                            video__hires_odd[e-1];
                }
            }
        }
    }
    else if (color_mode == INTERP) {	/* Color and strict interpolation */
        for (value = 0x00; value <= 0xFF; value++) {
	    for (b = 1, e = value * 8 + 1; b <= 5; b += 2, e += 2) {
		if (video__hires_even[e] == COLOR_BLACK) {
		    if (video__hires_even[e-1] != COLOR_BLACK &&
			video__hires_even[e+1] != COLOR_BLACK &&
			video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
			video__hires_even[e+1] != COLOR_LIGHT_WHITE)
		      video__hires_even[e] =
			  video__hires_even[e-1];

		    else if (
			video__hires_even[e-1] != COLOR_BLACK &&
			video__hires_even[e+1] != COLOR_BLACK &&
			video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
			video__hires_even[e+1] == COLOR_LIGHT_WHITE)
		      video__hires_even[e] =
			  video__hires_even[e-1];

		    else if (
			video__hires_even[e-1] != COLOR_BLACK &&
			video__hires_even[e+1] != COLOR_BLACK &&
			video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
			video__hires_even[e+1] != COLOR_LIGHT_WHITE)
		      video__hires_even[e] =
			  video__hires_even[e+1];

		    else if (
			video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
			video__hires_even[e+1] == COLOR_LIGHT_WHITE)
		      video__hires_even[e] = (value & 0x80)
			  ? COLOR_LIGHT_BLUE : COLOR_LIGHT_PURPLE;

		}
		if (video__hires_odd[e] == COLOR_BLACK) {
		    if (video__hires_odd[e-1] != COLOR_BLACK &&
			video__hires_odd[e+1] != COLOR_BLACK &&
			video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
			video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
		      video__hires_odd[e] =
			  video__hires_odd[e-1];

		    else if (
		        video__hires_odd[e-1] != COLOR_BLACK &&
			video__hires_odd[e+1] != COLOR_BLACK &&
			video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
			video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
		      video__hires_odd[e] =
			  video__hires_odd[e-1];

		    else if (
		        video__hires_odd[e-1] != COLOR_BLACK &&
			video__hires_odd[e+1] != COLOR_BLACK &&
			video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
			video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
		      video__hires_odd[e] =
			  video__hires_odd[e+1];

		    else if (
			video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
			video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
			video__hires_odd[e] = (value & 0x80)
			    ? COLOR_LIGHT_RED : COLOR_LIGHT_GREEN;
		}
	    }

	    for (b = 0, e = value * 8; b <= 6; b += 2, e += 2) {
		if (video__hires_even[ e ] == COLOR_BLACK) {
		    if (b > 0 && b < 6) {
		    if (video__hires_even[e-1] != COLOR_BLACK &&
			video__hires_even[e+1] != COLOR_BLACK &&
			video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
			video__hires_even[e+1] != COLOR_LIGHT_WHITE)
			video__hires_even[e] =
			    video__hires_even[e-1];

		    else if (
			video__hires_even[e-1] != COLOR_BLACK &&
			video__hires_even[e+1] != COLOR_BLACK &&
			video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
			video__hires_even[e+1] == COLOR_LIGHT_WHITE)
			video__hires_even[e] =
			    video__hires_even[e-1];

		    else if (
			video__hires_even[e-1] != COLOR_BLACK &&
			video__hires_even[e+1] != COLOR_BLACK &&
			video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
			video__hires_even[e+1] != COLOR_LIGHT_WHITE)
			video__hires_even[e] =
			    video__hires_even[e+1];

		    else if (
			video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
			video__hires_even[e+1] == COLOR_LIGHT_WHITE)
			video__hires_even[e] = (value & 0x80)
			    ? COLOR_LIGHT_RED : COLOR_LIGHT_GREEN;
		    }
		}

		if (video__hires_odd[e] == COLOR_BLACK) {
		    if (b > 0 && b < 6) {
		    if (video__hires_odd[e-1] != COLOR_BLACK &&
			video__hires_odd[e+1] != COLOR_BLACK &&
			video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
			video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
			video__hires_odd[e] =
			    video__hires_odd[e-1];

		    else if (
			video__hires_odd[e-1] != COLOR_BLACK &&
			video__hires_odd[e+1] != COLOR_BLACK &&
			video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
			video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
			video__hires_odd[e] =
			    video__hires_odd[e-1];

		    else if (
			video__hires_odd[e-1] != COLOR_BLACK &&
			video__hires_odd[e+1] != COLOR_BLACK &&
			video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
			video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
			video__hires_odd[e] =
			    video__hires_odd[e+1];

		    else if (
			video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
			video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
			video__hires_odd[e] = (value & 0x80)
			    ? COLOR_LIGHT_BLUE : COLOR_LIGHT_PURPLE;
		    }
		}
	    }
	}
    }
#ifdef _640x400
    /* *2 for 640x400 */
    for (b=0, e=0; b<4096; b++, e++) {
	video__wider_hires_even[b] = video__hires_even[e];
	video__wider_hires_odd[b] = video__hires_odd[e];
        b++;
	video__wider_hires_even[b] = video__hires_even[e];
	video__wider_hires_odd[b] = video__hires_odd[e];
    }
#endif
}


/* -------------------------------------------------------------------------
    c_initialize_row_col_tables()
   ------------------------------------------------------------------------- */
static void c_initialize_row_col_tables(void)
{
    int x, y, off, i;

    /* hires page offsets. initialize to invalid values. */
    for (i = 0; i < 8192; i++) {
	(long)(video__screen_addresses[i]) = -1;
    }

    for (y = 0; y < 24; y++) {
	for (off = 0; off < 8; off++) {
	    for (x = 0; x < 40; x++) {
#ifdef _640x400
		video__screen_addresses[video__line_offset[y] + 0x400*off + x ] =
		    (y*16 + 2*off/* + 8*/) * SCANWIDTH + x*14 + 4;
#else
		video__screen_addresses[video__line_offset[y] + 0x400*off + x ] =
		    (y*8 + off + 4) * 320 + x*7 + 20;
#endif
		video__columns[video__line_offset[y] + 0x400*off + x] =
		    (unsigned char)x;
	    }
	}
    }

}

static void c_initialize_tables_video(void) {
    int x, y, i;	

    /* initialize text/lores & hires graphics */
    for (y = 0; y < 24; y++) {		/* 24 rows */
	for (x = 0; x < 40; x++)	/* 40 cols */
	{
#ifdef APPLE_IIE
	    if (apple_mode == IIE_MODE) {
		/* //e mode: text/lores page 0 */
		cpu65_vmem[ video__line_offset[ y ] + x + 0x400].w =
			(y < 20) ? video__write_2e_text0 :
				   video__write_2e_text0_mixed;
	    }
	    else
#endif
	    {
		/* ][+ modes: text/lores page 0 */
		cpu65_vmem[ video__line_offset[ y ] + x + 0x400].w =
			(y < 20) ? video__write_text0 :
				   video__write_text0_mixed;
	    }

#ifdef APPLE_IIE
	    if (apple_mode == IIE_MODE) {
		cpu65_vmem[ video__line_offset[ y ] + x + 0x800].w =
		    (y < 20) ? video__write_2e_text1 :
		               video__write_2e_text1_mixed;
	    }
	    else
#endif
	    {
	    /* ][+ modes: text/lores page 1 in main memory */
	    cpu65_vmem[ video__line_offset[ y ] + x + 0x800].w =
			(y < 20) ? video__write_text1 :
				   video__write_text1_mixed;
	    }

	    for (i = 0; i < 8; i++)
	    {
		/* //e mode: hires/double hires page 0 */
#ifdef APPLE_IIE
		if (apple_mode == IIE_MODE) {
		    cpu65_vmem[ 0x2000 + video__line_offset[ y ]
					 + 0x400 * i + x ].w =
		        (y < 20) ? ((x & 1) ? video__write_2e_odd0 :
					      video__write_2e_even0)
				 : ((x & 1) ? video__write_2e_odd0_mixed :
					      video__write_2e_even0_mixed);
		}

		/* ][+ modes: hires page 0 */
		else
#endif
		{
		    cpu65_vmem[ 0x2000 + video__line_offset[ y ]
					 + 0x400 * i + x ].w =
		        (y < 20) ? ((x & 1) ? video__write_odd0 :
					      video__write_even0)
				 : ((x & 1) ? video__write_odd0_mixed :
					      video__write_even0_mixed);
		}

#ifdef APPLE_IIE
		if (apple_mode == IIE_MODE) {
		    cpu65_vmem[ 0x4000 + video__line_offset[ y ]
					 + 0x400 * i + x ].w =
		        (y < 20) ? ((x & 1) ? video__write_2e_odd1 :
					      video__write_2e_even1)
				 : ((x & 1) ? video__write_2e_odd1_mixed :
					      video__write_2e_even1_mixed);
		}

		/* ][+ modes: hires page 1 */
		else
#endif
		{
		cpu65_vmem[ 0x4000 + video__line_offset[ y ]
				        + 0x400 * i + x ].w =
		        (y < 20) ? ((x & 1) ? video__write_odd1 :
					      video__write_even1)
				 : ((x & 1) ? video__write_odd1_mixed :
					      video__write_even1_mixed);
		}
	    }
	}
    }
}

void video_set(int flags)
{
    if (color_mode == COLOR)
	video__strictcolors = 1;	/* strict colors */
    else if (color_mode == INTERP)
	video__strictcolors = 2;	/* strict interpolation */
    else
	video__strictcolors = 0;	/* lazy coloration */

    c_initialize_hires_values();	/* precalculate hires values */
    c_initialize_row_col_tables();	/* precalculate hires offsets */
    c_initialize_tables_video();	/* memory jump tables for video */

#ifdef APPLE_IIE
    c_initialize_dhires_values();	/* set up dhires colors */
#endif
}

void video_loadfont_int(int first, int quantity, const unsigned char *data)
{ 
    int i,j;
    unsigned char x;
    int y;

    i = quantity * 8;

    while (i--)	
    {
        j = 8;
        x = data[i];
        while (j--)
        {

#ifdef _640x400
	    y = (first << 7) + (i << 4) + (j << 1);
	    if (x & 128)
            {
		video__wider_int_font[0][y] =
	        video__wider_int_font[0][y+1] = 
	        video__wider_int_font[1][y] =
	        video__wider_int_font[1][y+1] = COLOR_LIGHT_GREEN;
		video__wider_int_font[2][y] =
	        video__wider_int_font[2][y+1] = COLOR_LIGHT_RED;
	    }
            else
            {
	        video__wider_int_font[0][y] =
	        video__wider_int_font[0][y+1] =
	        video__wider_int_font[2][y] =
	        video__wider_int_font[2][y+1] = COLOR_BLACK;
		video__wider_int_font[1][y] =
	        video__wider_int_font[1][y+1] = COLOR_MEDIUM_BLUE;
	    }
#else
	    y = (first << 6) + (i << 3) + j;
	    if (x & 128)
            {	
	        video__int_font[0][y] =
	        video__int_font[1][y] = COLOR_LIGHT_GREEN;
		video__int_font[2][y] = COLOR_LIGHT_RED;
	    }
            else
            {	
	        video__int_font[0][y] =
	        video__int_font[2][y] = COLOR_BLACK;
		video__int_font[1][y] = COLOR_MEDIUM_BLUE;
	    }
#endif /* _640x400 */ 
            x <<= 1; 
        }
    }
}

/* Should probably move this to assembly... */

static void c_interface_print_char40_line(
	unsigned char **d, unsigned char **s)
{
#ifdef _640x400
    *((unsigned int *)(*d)) = *((unsigned int *)(*s));/*32bits*/
    *d += 4, *s += 4;
    *((unsigned int *)(*d)) = *((unsigned int *)(*s));/*32bits*/
    *d += 4, *s += 4;
    *((unsigned int *)(*d)) = *((unsigned int *)(*s));/*32bits*/
    *d += 4, *s += 4;
    *((unsigned short *)(*d)) = *((unsigned short *)(*s));/*16bits*/
    *d += SCANSTEP, *s -= 12;
    *((unsigned int *)(*d)) = *((unsigned int *)(*s));/*32bits*/
    *d += 4, *s += 4;
    *((unsigned int *)(*d)) = *((unsigned int *)(*s));/*32bits*/
    *d += 4, *s += 4;
    *((unsigned int *)(*d)) = *((unsigned int *)(*s));/*32bits*/
    *d += 4, *s += 4;
    *((unsigned short *)(*d)) = *((unsigned short *)(*s));/*16bits*/
    *d += SCANSTEP, *s += 4;
#else
    *((unsigned int *)(*d)) = *((unsigned int *)(*s));/*32bits*/
    *d += 4, *s += 4;
    *((unsigned short *)(*d)) = *((unsigned short *)(*s));/*16bits*/
    *d += 2, *s += 2;
    *((unsigned char *)(*d)) = *((unsigned char *)(*s));/*8bits*/
    *d += SCANSTEP, *s += 2;
#endif
}

void video_plotchar( int x, int y, int scheme, unsigned char c )
{
    int off;
    unsigned char *d;
    unsigned char *s;

#ifdef _640x400
    off = y * SCANWIDTH * 16 + x * 14 + 4;
    s = video__wider_int_font[scheme] + c * 128;
#else
    off = y * 320 * 8 + x * 7 + 1300;
    s = video__int_font[scheme] + c * 64;
#endif
    d = video__fb1 + off;

    c_interface_print_char40_line(&d,&s);
    c_interface_print_char40_line(&d,&s);
    c_interface_print_char40_line(&d,&s);
    c_interface_print_char40_line(&d,&s);
    c_interface_print_char40_line(&d,&s);
    c_interface_print_char40_line(&d,&s);
    c_interface_print_char40_line(&d,&s);
    c_interface_print_char40_line(&d,&s);
}
