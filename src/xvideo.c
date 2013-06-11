/* 
 * Apple // emulator for Linux: X-Windows graphics support
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>/* MITSHM! */


#include "video.h"
#include "misc.h"
#include "keys.h"

/* copied from svideo.c (didn't bother to rename!) */
static unsigned char	*svga_GM;
static unsigned char	vga_mem_page_0[SCANWIDTH*SCANHEIGHT];		/* page0 framebuffer */
static unsigned char	vga_mem_page_1[SCANWIDTH*SCANHEIGHT];		/* page1 framebuffer */

static Display *display;
static Window win;
static GC gc;
static unsigned int width, height;	/* window size */

static int screen_num;
static Visual* visual;
static XVisualInfo visualinfo;
static XColor colors[256];
XImage *image;
static Colormap cmap;
XEvent xevent;

int		doShm = 1;/* assume true */
XShmSegmentInfo	xshminfo;
int		xshmeventtype;


/* -------------------------------------------------------------------------
    video_setpage(p):    Switch to screen page p
   ------------------------------------------------------------------------- */
void video_setpage(int p)
{
    if (p == video__current_page) return;

    if (p)
    {
	memcpy(vga_mem_page_0,svga_GM,SCANWIDTH*SCANHEIGHT);
	memcpy(svga_GM,vga_mem_page_1,SCANWIDTH*SCANHEIGHT);
	video__current_page = 1;
	video__fb1 = vga_mem_page_0;
	video__fb2 = svga_GM;
    }
    else
    {
	memcpy(vga_mem_page_1,svga_GM,SCANWIDTH*SCANHEIGHT);
	memcpy(svga_GM,vga_mem_page_0,SCANWIDTH*SCANHEIGHT);
	video__current_page = 0;
	video__fb1 = svga_GM;
	video__fb2 = vga_mem_page_1;
    }
}

/*
 * XShm code influenced from the DOOM source code.
 * This tries to find some shared memory to use.  It checks for stale segments
 * (maybe due to an emulator crash) and tries to remove them or use them.
 */
static void getshm(int size) {
    int			key = ('a'<<24) | ('p'<<16) | ('p'<<8) | 'l';
    struct shmid_ds	shminfo;
    int			id;
    int			rc;
    int			counter=5;

    /* Try to allocate shared memory segment.  Use stale segments if any are
     * found.
     */
    do
    {
	id = shmget((key_t) key, 0, 0777);
	if (id != -1)
	{
	    /* we got someone else's ID. check if it's stale. */
	    printf("Found shared memory key=`%c%c%c%c', id=%d\n",
		    (key & 0xff000000)>>24,
		    (key & 0xff0000)>>16,
		    (key & 0xff00)>>8,
		    (key & 0xff),
		    id
		    );
	    rc=shmctl(id, IPC_STAT, &shminfo); /* get stats */
	    if (!rc) {
		/* someone's using the emulator */
		if (shminfo.shm_nattch) {
		    printf( "User uid=%d, key=`%c%c%c%c' appears to be running "
			    "the emulator.\n",
			    shminfo.shm_perm.cuid,
			    (key & 0xff000000)>>24,
			    (key & 0xff0000)>>16,
			    (key & 0xff00)>>8,
			    (key & 0xff)
			    );
		    ++key; /* increase the key count */
		}

		/* found a stale ID. */
		else {
		    /* it's my stale ID */
		    if (getuid() == shminfo.shm_perm.cuid) {
			rc = shmctl(id, IPC_RMID, 0);
			if (!rc)
			    printf("Was able to kill my old shared memory\n");
			else {
			    perror("shmctl");
			    printf("Was NOT able to kill my old shared memory\n");
			}

			id = shmget((key_t)key, size, IPC_CREAT|0777);
			if (id == -1) {
			    perror("shmget");
			    printf("Could not get shared memory\n");
			}

			rc=shmctl(id, IPC_STAT, &shminfo);
			if (rc) {
			    perror("shmctl");
			}

			break;

		    }
		    /* not my ID, but maybe we can use it */
		    if (size == shminfo.shm_segsz) {
			printf( "Will use stale shared memory of uid=%d\n",
				shminfo.shm_perm.cuid);
			break;
		    }
		    /* not my ID, and we can't use it */
		    else {
			printf( "Can't use stale shared memory belonging to uid=%d, "
				"key=`%c%c%c%c', id=%d\n",
				shminfo.shm_perm.cuid,
				(key & 0xff000000)>>24,
				(key & 0xff0000)>>16,
				(key & 0xff00)>>8,
				(key & 0xff),
				id
				);
			++key;
		    }
		}
	    }
	    else
	    {
		/* oops.  what to do now? */
		perror("shmctl");
		printf( "Could not get stats on key=`%c%c%c%c', id=%d\n",
			(key & 0xff000000)>>24,
			(key & 0xff0000)>>16,
			(key & 0xff00)>>8,
			(key & 0xff),
			id
			);
		++key;
	    }
	}
	else
	{
	    /* no stale ID's */
	    id = shmget((key_t)key, size, IPC_CREAT|0777);
	    if (id == -1) {
		perror("shmget");
		printf("Could not get shared memory\n");
		++key;
	    }
	    break;
	}
    } while (--counter);

    if (!counter)
    {
	printf( "System has too many stale/used "
		"shared memory segments!\n");
    }	

    xshminfo.shmid = id;

    /* attach to the shared memory segment */
    image->data = xshminfo.shmaddr = shmat(id, 0, 0);

    if (image->data == -1) {
	perror("shmat");
	printf("Could not attach to shared memory\n");
	exit(1);
    }

    printf( "Using shared memory key=`%c%c%c%c', id=%d, addr=0x%x\n",
	    (key & 0xff000000)>>24,
	    (key & 0xff0000)>>16,
	    (key & 0xff00)>>8,
	    (key & 0xff),
	    id,
	    (int) (image->data));
}


static void c_initialize_colors() {
    static unsigned char col2[ 3 ] = {255,255,255};
    static int		firstcall = 1;
    int			c,i,j;

    if (visualinfo.class == PseudoColor && visualinfo.depth == 8)
    {
	/* initialize the colormap */
	if (firstcall) {
	    firstcall = 0;
	    for (i=0; i<256; i++) {
		colors[i].pixel = i;
		colors[i].flags = DoRed|DoGreen|DoBlue;
	    }
	}

	/* align the palette for hires graphics */
	for (i = 0; i < 8; i++) {
	    for (j = 0; j < 3; j++) {
		c = (i & 1) ? col2[ j ] : 0;
		colors[ j+i*3+32].red = (c<<8) + c;
		c = (i & 2) ? col2[ j ] : 0;
		colors[ j+i*3+32].green = (c<<8) + c;
		c = (i & 4) ? col2[ j ] : 0;
		colors[ j+i*3+32].blue = (c<<8) + c;
	    }
	}
	colors[ COLOR_FLASHING_BLACK].red = 0;
	colors[ COLOR_FLASHING_BLACK].green = 0;
	colors[ COLOR_FLASHING_BLACK].blue = 0;

	colors[ COLOR_LIGHT_WHITE].red = (255<<8)|255;
	colors[ COLOR_LIGHT_WHITE].green = (255<<8)|255;
	colors[ COLOR_LIGHT_WHITE].blue = (255<<8)|255;

	colors[ COLOR_FLASHING_WHITE].red = (255<<8)|255;
	colors[ COLOR_FLASHING_WHITE].green = (255<<8)|255;
	colors[ COLOR_FLASHING_WHITE].blue = (255<<8)|255;

	colors[0x00].red = 0; colors[0x00].green = 0; 
        colors[0x00].blue = 0;   /* Black */
	colors[0x10].red = 195; colors[0x10].green = 0;
        colors[0x10].blue = 48;  /* Magenta */
	colors[0x20].red = 0; colors[0x20].green = 0;
        colors[0x20].blue = 130; /* Dark Blue */
	colors[0x30].red = 166; colors[0x30].green = 52;
	colors[0x30].blue = 170; /* Purple */
	colors[0x40].red = 0; colors[0x40].green = 146;
        colors[0x40].blue = 0;   /* Dark Green */
	colors[0x50].red = 105; colors[0x50].green = 105;
        colors[0x50].blue = 105; /* Dark Grey*/
	colors[0x60].red = 113; colors[0x60].green = 24;
        colors[0x60].blue = 255; /* Medium Blue */
	colors[0x70].red = 12; colors[0x70].green = 190;
	colors[0x70].blue = 235; /* Light Blue */
	colors[0x80].red = 150; colors[0x80].green = 85;
        colors[0x80].blue = 40; /* Brown */
	colors[0x90].red = 255; colors[0xa0].green = 24;
        colors[0x90].blue = 44; /* Orange */
	colors[0xa0].red = 150; colors[0xa0].green = 170;
        colors[0xa0].blue = 170; /* Light Gray */
	colors[0xb0].red = 255; colors[0xb0].green = 158;
	colors[0xb0].blue = 150; /* Pink */
	colors[0xc0].red = 0; colors[0xc0].green = 255;
        colors[0xc0].blue = 0; /* Green */
	colors[0xd0].red = 255; colors[0xd0].green = 255;
	colors[0xd0].blue = 0; /* Yellow */
	colors[0xe0].red = 130; colors[0xe0].green = 255;
        colors[0xe0].blue = 130; /* Aqua */
	colors[0xf0].red = 255; colors[0xf0].green = 255;
	colors[0xf0].blue = 255; /* White */

	/* mirror of lores colors optimized for dhires code */
	colors[0x00].red = 0; colors[0x00].green = 0; 
        colors[0x00].blue = 0;   /* Black */
	colors[0x08].red = 195; colors[0x08].green = 0;
        colors[0x08].blue = 48;  /* Magenta */
	colors[0x01].red = 0; colors[0x01].green = 0;
        colors[0x01].blue = 130; /* Dark Blue */
	colors[0x09].red = 166; colors[0x09].green = 52;
	colors[0x09].blue = 170; /* Purple */
	colors[0x02].red = 0; colors[0x02].green = 146;
        colors[0x02].blue = 0;   /* Dark Green */
	colors[0x0a].red = 105; colors[0x0A].green = 105;
        colors[0x0a].blue = 105; /* Dark Grey*/
	colors[0x03].red = 113; colors[0x03].green = 24;
        colors[0x03].blue = 255; /* Medium Blue */
	colors[0x0b].red = 12; colors[0x0b].green = 190;
	colors[0x0b].blue = 235; /* Light Blue */
	colors[0x04].red = 150; colors[0x04].green = 85;
        colors[0x04].blue = 40; /* Brown */
	colors[0x0c].red = 255; colors[0x0c].green = 24;
        colors[0x0c].blue = 44; /* Orange */
	colors[0x05].red = 150; colors[0x05].green = 170;
        colors[0x05].blue = 170; /* Light Gray */
	colors[0x0d].red = 255; colors[0x0d].green = 158;
	colors[0x0d].blue = 150; /* Pink */
	colors[0x06].red = 0; colors[0x06].green = 255;
        colors[0x06].blue = 0; /* Green */
	colors[0x0e].red = 255; colors[0x0e].green = 255;
	colors[0x0e].blue = 0; /* Yellow */
	colors[0x07].red = 130; colors[0x07].green = 255;
        colors[0x07].blue = 130; /* Aqua */
	colors[0x0f].red = 255; colors[0x0f].green = 255;
	colors[0x0f].blue = 255; /* White */

	for (i=0; i<16; i++) {
	    colors[i].red = (colors[i].red<<8) | colors[i].red;
	    colors[i].green = (colors[i].green<<8) | colors[i].green;
	    colors[i].blue = (colors[i].blue<<8) | colors[i].blue;

	    colors[i<<4].red = (colors[i<<4].red<<8) | colors[i<<4].red;
	    colors[i<<4].green = (colors[i<<4].green<<8) | colors[i<<4].green;
	    colors[i<<4].blue = (colors[i<<4].blue<<8) | colors[i<<4].blue;
	}
	// store the colors to the current colormap
	XStoreColors(display, cmap, colors, 256);
    }
}


/* HACK: this is incredibly broken.
 * Map the X keysyms back into scancodes so the routines in keys.c can deal
 * with it.  We do this to be compatible with what the SVGAlib version does.
 */
static int keysym_to_scancode(void) {
    static int rc = 0xFF;

    switch(rc = XKeycodeToKeysym(display, xevent.xkey.keycode, 0))
    {
	case XK_F1:
	    rc = 59; break;
	case XK_F2:
	    rc = 60; break;
	case XK_F3:
	    rc = 61; break;
	case XK_F4:
	    rc = 62; break;
	case XK_F5:
	    rc = 63; break;
	case XK_F6:
	    rc = 64; break;
	case XK_F7:
	    rc = 65; break;
	case XK_F8:
	    rc = 66; break;
	case XK_F9:
	    rc = 67; break;
	case XK_F10:
	    rc = 68; break;
	case XK_Left:
	    rc = 105; break;
	case XK_Right:
	    rc = 106; break;
	case XK_Down:
	    rc = 108; break;
	case XK_Up:
	    rc = 103; break;
	case XK_Escape:
	    rc = 1; break;
	case XK_Return:
	    rc = 28; break;
	case XK_Tab:
	    rc = 15; break;
	case XK_Shift_L:
	    rc = SCODE_L_SHIFT; break;
	case XK_Shift_R:
	    rc = SCODE_R_SHIFT; break;
	case XK_Control_L:
	    rc = SCODE_L_CTRL; break;
	case XK_Control_R:
	    rc = SCODE_R_CTRL; break;
	case XK_Caps_Lock:
	    rc = SCODE_CAPS; break;
	case XK_BackSpace:
	    rc = 14; break;
	case XK_Insert:
	    rc = 110; break;
	case XK_Pause:
	    rc = 119; break;
	case XK_Break:
	    /* Pause and Break are the same key, but have different 
             * scancodes (on PC keyboards).  Ctrl makes the difference.  
             *
             * We assume the X server is passing along the distinction to us,
	     * rather than making us check Ctrl manually.
             */
	    rc = 101; break; 
	case XK_Print:
	    rc = 99; break;
	case XK_Delete:
	    rc = 111; break;
	case XK_End:
	    rc = 107; break;
	case XK_Home:
	    rc = 102; break;
	case XK_Page_Down:
	    rc = 109; break;
	case XK_Page_Up:
	    rc = 104; break;

	// keypad joystick movement 
	case XK_KP_5:
	case XK_KP_Begin:
	    rc = SCODE_J_C; break;
	case XK_KP_4:
	case XK_KP_Left:
	    rc = SCODE_J_L; break;
	case XK_KP_8:
	case XK_KP_Up:
	    rc = SCODE_J_U; break;
	case XK_KP_6:
	case XK_KP_Right:
	    rc = SCODE_J_R; break;
	case XK_KP_2:
	case XK_KP_Down:
	    rc = SCODE_J_D; break;

	case XK_Alt_L:
	    rc = 56; break;
	case XK_Alt_R:
	    rc = 100; break;

	default:
	    if ((rc >= XK_space) && (rc <= XK_asciitilde))
		rc = xevent.xkey.keycode - 8;
	    break;
    }

    return rc & 0xFF;/* normalize */
}

static void post_image() {
    if (doShm) {
	if (!XShmPutImage(
		display,
		win,
		gc,
		image,
		0, 0,
		0, 0,
		SCANWIDTH, SCANHEIGHT,
		True))
	    fprintf(stderr, "XShmPutImage() failed\n");
    } else {
	if (XPutImage(
		display,
		win,
		gc,
		image,
		0, 0,
		0, 0,
		SCANWIDTH, SCANHEIGHT
		))
	    fprintf(stderr, "XPutImage() failed\n");
    }
}

static void c_flash_cursor(int on) {
    // flash only if it's text or mixed modes.
    if (softswitches & (SS_TEXT|SS_MIXED)) {
	if (!on) {
	    colors[ COLOR_FLASHING_BLACK].red = 0;
	    colors[ COLOR_FLASHING_BLACK].green = 0;
	    colors[ COLOR_FLASHING_BLACK].blue = 0;

	    colors[ COLOR_FLASHING_WHITE].red = 0xffff;
	    colors[ COLOR_FLASHING_WHITE].green = 0xffff;
	    colors[ COLOR_FLASHING_WHITE].blue = 0xffff;
	} else {
	    colors[ COLOR_FLASHING_WHITE].red = 0;
	    colors[ COLOR_FLASHING_WHITE].green = 0;
	    colors[ COLOR_FLASHING_WHITE].blue = 0;

	    colors[ COLOR_FLASHING_BLACK].red = 0xffff;
	    colors[ COLOR_FLASHING_BLACK].green = 0xffff;
	    colors[ COLOR_FLASHING_BLACK].blue = 0xffff;
	}

	// store the colors to the current colormap
	XStoreColors(display, cmap, colors, 256);
    }
}

/* FIXME: blocking not implemented... */
void video_sync(int block) {
    static int flash_count = 0;
    // post the image and loop waiting for it to finish and
    // also process other input events
    post_image();
LOOP:
	if (doShm)
	    XNextEvent(
		    display,
		    &xevent);
	else if (!XCheckMaskEvent(
		display,
		KeyPressMask|KeyReleaseMask,
		&xevent))
	    goto POLL_FINISHED;
	switch (xevent.type) {
	    case KeyPress:
		c_read_raw_key(keysym_to_scancode(), 1);
		break;
	    case KeyRelease:
		c_read_raw_key(keysym_to_scancode(), 0);
		break;
	    default:
		if (xevent.type == xshmeventtype)
		    goto POLL_FINISHED;
		break;
	}
	goto LOOP;

POLL_FINISHED:

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

static Cursor hidecursor() {
    Pixmap cursormask;
    XGCValues xgc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, win, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
	    &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
}

static void parseArgs() {
    int i;
    for (i=0; i<argc; i++) {
	if (strstr(argv[i], "-noshm"))
	    doShm=0;
    }
}

void video_init() {
	XSetWindowAttributes attribs;
	unsigned long	attribmask;
	int x, y; 	/* window position */
	int drawingok;
	unsigned int display_width, display_height;
	XGCValues xgcvalues;
	int valuemask;
	char *window_name = "Apple ][";
	char *icon_name = window_name;
	XSizeHints *size_hints;
	XWMHints *wm_hints;
	XClassHint *class_hints;
	XTextProperty windowName, iconName;
	GC gc;
	char *progname;/* name this program was invoked by */
	char *displayname = NULL;

	progname = argv[0];

	/* give up root privileges. equivalent of vga_init() */
	setegid(getgid());
	seteuid(getuid());

	parseArgs();

	if (!(size_hints = XAllocSizeHints())) {
	    fprintf(stderr, "cannot allocate memory for SizeHints\n");
	    exit(1);
	}
	if (!(wm_hints = XAllocWMHints())) {
	    fprintf(stderr, "cannot allocate memory for WMHints\n");
	    exit(1);
	}
	if (!(class_hints = XAllocClassHint())) {
	    fprintf(stderr, "cannot allocate memory for ClassHints\n");
	    exit(1);
	}

	/* connect to X server */
	if ( (display=XOpenDisplay(displayname)) == NULL )
	{
		fprintf(stderr, "cannot connect to X server \"%s\"\n", 
			XDisplayName(displayname));
		exit(1);
	}

	/* get screen size from display structure macro */
	screen_num = DefaultScreen(display);
	if (!XMatchVisualInfo(display, screen_num, 8, PseudoColor, &visualinfo)) {
	    fprintf(stderr,
		    "Sorry bud, xapple2 only supports "
		    "8bit PseudoColor displays.\n"
		    "Maybe you can fix this!\n");
	    exit(1);
	}
	visual = visualinfo.visual;
	display_width = DisplayWidth(display, screen_num);
	display_height = DisplayHeight(display, screen_num);

	/* Note that in a real application, x and y would default to 0
	 * but would be settable from the command line or resource database.  
	 */
	x = y = 0;
	width = SCANWIDTH, height = SCANHEIGHT;

	/* init MITSHM if we're doing it */
	if (doShm) {
	    /* make sure we have it */
	    doShm = XShmQueryExtension(display);

	    /* and it's a local connection */
	    if (!displayname)
		displayname = getenv("DISPLAY");
	    if (displayname)
	    {
		if ((*displayname != ':') || !doShm) {
		    printf("Cannot run MITSHM version of emulator "
			   "with display \"%s\"\n"
			   "Try something like \":0.0\".  "
			   "Reverting to regular X with no sound.\n",
			   displayname);
		    doShm = 0;
		    soundAllowed=0;
		    c_initialize_sound();
		}
	    }
	} else {
	    /* and it's a local connection */
	    if (!displayname)
		displayname = getenv("DISPLAY");
	    if (displayname)
	    {
		if (*displayname != ':') {
		    printf("Sound not allowed for remote display \"%s\".\n",
			   displayname);
		    soundAllowed=0;
		    c_initialize_sound();
		}
	    }
	}

	/* initialize colors */
	cmap = XCreateColormap(display, RootWindow(display, screen_num),
		visual, AllocAll);
	c_initialize_colors();
	attribs.colormap = cmap;
	attribs.border_pixel = 0;

	/* select event types wanted */
	attribmask = CWEventMask | CWColormap | CWBorderPixel;/* HACK CWBorderPixel? */
	attribs.event_mask =
	    KeyPressMask
	    | KeyReleaseMask
	    | ExposureMask;

	/* create opaque window */
	win = XCreateWindow(display, RootWindow(display,screen_num), 
			x, y, width, height,
			0,/* border_width */
			8,/* depth */
			InputOutput,
			visual,
			attribmask,
			&attribs);


	/* set size hints for window manager.  We don't want the user to
	 * dynamically allocate window size since we won't do the right
	 * scaling in response.  Whaddya want, performance or a snazzy gui?
	 */
	size_hints->flags = PPosition | PSize | PMinSize | PMaxSize;
	size_hints->min_width = width;
	size_hints->min_height = height;
	size_hints->max_width = width;
	size_hints->max_height = height;

	/* store window_name and icon_name for niceity. */
	if (XStringListToTextProperty(&window_name, 1, &windowName) == 0) {
	    fprintf(stderr, "structure allocation for windowName failed.\n");
	    exit(1);
	}
		
	if (XStringListToTextProperty(&icon_name, 1, &iconName) == 0) {
	    fprintf(stderr, "structure allocation for iconName failed.\n");
	    exit(1);
	}

	wm_hints->initial_state = NormalState;
	wm_hints->input = True;
	wm_hints->flags = StateHint | IconPixmapHint/* | InputHint*/;

	class_hints->res_name = progname;
	class_hints->res_class = "Apple2";

	XSetWMProperties(display, win, &windowName, &iconName, 
			argv, argc, size_hints, wm_hints, 
			class_hints);

	XDefineCursor(display, win, hidecursor(display, win));

	/* create the GC */
	valuemask = GCGraphicsExposures;
	xgcvalues.graphics_exposures = False;
	gc = XCreateGC(
		display,
		win,
		valuemask,
		&xgcvalues);

	/* display window */
	XMapWindow(display, win);

	/* wait until it is OK to draw */
	drawingok = 0;
	while (!drawingok)
	{
	    XNextEvent(display, &xevent);
	    if ((xevent.type == Expose) && !xevent.xexpose.count)
	    {
		drawingok = 1;
	    }
	}

	xshmeventtype = XShmGetEventBase(display) + ShmCompletion;

	/* create the image */
	if (doShm) {
	    image = XShmCreateImage(
		    display,
		    visual,
		    8,
		    ZPixmap,
		    0,
		    &xshminfo,
		    SCANWIDTH,
		    SCANHEIGHT);

	    if (!image) {
		fprintf(stderr, "XShmCreateImage failed\n");
		exit(1);
	    }
	
	    printf("Allocating shared memory %dx%d region\n",
		    image->bytes_per_line, image->height);

	    getshm(image->bytes_per_line * image->height);

	    /* get the X server to attach to it */
	    if (!XShmAttach(display, &xshminfo)) {
		fprintf(stderr, "XShmAttach() failed in InitGraphics()\n");
		exit(1);
	    }
	} else {
	    char *data = malloc(SCANWIDTH*SCANHEIGHT*sizeof(unsigned char));
	    if (!data) {
		fprintf(stderr, "no memory for image data!\n");
		exit(1);
	    }
	    printf("Creating regular XImage\n");
	    image = XCreateImage(
		    display,
		    visual,
		    8,
		    ZPixmap,
		    0,
		    data,
		    SCANWIDTH,
		    SCANHEIGHT,
		    8/*bitmap_pad*/,
		    SCANWIDTH/*bytes_per_line*/);
	
	    if (!image) {
		fprintf(stderr, "XCreateImage failed\n");
		exit(1);
	    }
	
        }

	svga_GM = video__fb1 = image->data;
	video__fb2 = vga_mem_page_1;

	memset(video__fb1,0,SCANWIDTH*SCANHEIGHT);
	memset(video__fb2,0,SCANWIDTH*SCANHEIGHT);

}

void video_shutdown(void)
{
    if (doShm) {
	// Detach from X server
	if (!XShmDetach(display, &xshminfo))
	    fprintf(stderr,"XShmDetach() failed in video_shutdown()\n");

	// Release shared memory.
	shmdt(xshminfo.shmaddr);
	shmctl(xshminfo.shmid, IPC_RMID, 0);

	// Paranoia.
	image->data = NULL;
    } else {
	free(image->data);
    }

    exit(0);
}
