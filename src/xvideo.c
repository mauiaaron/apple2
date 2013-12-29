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
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XShm.h> /* MITSHM! */


#include "video.h"
#include "misc.h"
#include "keys.h"

static unsigned char vga_mem_page_0[SCANWIDTH*SCANHEIGHT];              /* page0 framebuffer */
static unsigned char vga_mem_page_1[SCANWIDTH*SCANHEIGHT];              /* page1 framebuffer */

static Display *display;
static Window win;
static GC gc;
static unsigned int width, height;      /* window size */
static A2_VIDSCALE scale = VIDEO_SCALE_1;

static int screen_num;
static XVisualInfo visualinfo;
static XColor colors[256];
XImage *image;
static Colormap cmap;
XEvent xevent;

static uint32_t red_shift;
static uint32_t green_shift;
static uint32_t blue_shift;
static uint32_t alpha_shift;

int doShm = 1;            /* assume true */
XShmSegmentInfo xshminfo;
int xshmeventtype;


/* -------------------------------------------------------------------------
    video_setpage(p):    Switch to screen page p
   ------------------------------------------------------------------------- */
void video_setpage(int p)
{
    video__current_page = p;
}

/*
 * XShm code influenced from the DOOM source code.
 * This tries to find some shared memory to use.  It checks for stale segments
 * (maybe due to an emulator crash) and tries to remove them or use them.
 */
static void getshm(int size) {
    int key = ('a'<<24) | ('p'<<16) | ('p'<<8) | 'l';
    struct shmid_ds shminfo;
    int id;
    int rc;
    int counter=5;

    /* Try to allocate shared memory segment.  Use stale segments if any are
     * found.
     */
    do
    {
        id = shmget((key_t) key, 0, 0777);
        if (id == -1)
        {
            /* no stale ID's */
            id = shmget((key_t)key, size, IPC_CREAT|0777);
            if (id == -1)
            {
                perror("shmget");
                printf("Could not get shared memory\n");
                ++key;
            }

            break;
        }
        else
        {
            /* we got someone else's ID. check if it's stale. */
            printf("Found shared memory key=`%c%c%c%c', id=%d\n", (key & 0xff000000)>>24, (key & 0xff0000)>>16, (key & 0xff00)>>8, (key & 0xff), id);
            rc=shmctl(id, IPC_STAT, &shminfo); /* get stats */
            if (rc)
            {
                /* error.  what to do now? */
                perror("shmctl");
                printf("Could not get stats on key=`%c%c%c%c', id=%d\n", (key & 0xff000000)>>24, (key & 0xff0000)>>16, (key & 0xff00)>>8, (key & 0xff), id);
                ++key;
            }
            else
            {
                if (shminfo.shm_nattch)
                {
                    printf( "User uid=%d, key=`%c%c%c%c' appears to be running the emulator.\n", shminfo.shm_perm.cuid, (key & 0xff000000)>>24, (key & 0xff0000)>>16, (key & 0xff00)>>8, (key & 0xff));
                    ++key; /* increase the key count */
                }
                else
                {
                    if (getuid() == shminfo.shm_perm.cuid)
                    {
                        /* it's my stale ID */
                        rc = shmctl(id, IPC_RMID, 0);
                        if (!rc)
                        {
                            printf("Was able to kill my old shared memory\n");
                        }
                        else
                        {
                            perror("shmctl");
                            printf("Was NOT able to kill my old shared memory\n");
                        }

                        id = shmget((key_t)key, size, IPC_CREAT|0777);
                        if (id == -1)
                        {
                            perror("shmget");
                            printf("Could not get shared memory\n");
                        }

                        rc=shmctl(id, IPC_STAT, &shminfo);
                        if (rc)
                        {
                            perror("shmctl");
                        }

                        break;
                    }

                    if (size == shminfo.shm_segsz)
                    {
                        /* not my ID, but maybe we can use it */
                        printf("Will use stale shared memory of uid=%d\n", shminfo.shm_perm.cuid);
                        break;
                    }
                    /* not my ID, and we can't use it */
                    else
                    {
                        printf("Can't use stale shared memory belonging to uid=%d, key=`%c%c%c%c', id=%d\n", shminfo.shm_perm.cuid, (key & 0xff000000)>>24, (key & 0xff0000)>>16, (key & 0xff00)>>8, (key & 0xff), id);
                        ++key;
                    }
                }
            }
        }
    } while (--counter);

    if (!counter)
    {
        printf( "System has too many stale/used shared memory segments!\n");
    }

    xshminfo.shmid = id;

    /* attach to the shared memory segment */
    image->data = xshminfo.shmaddr = shmat(id, 0, 0);

    if ((int)(image->data) == -1)
    {
        perror("shmat");
        printf("Could not attach to shared memory\n");
        exit(1);
    }

    printf("Using shared memory key=`%c%c%c%c', id=%d, addr=%p\n", (key & 0xff000000)>>24, (key & 0xff0000)>>16, (key & 0xff00)>>8, (key & 0xff), id, image->data);
}


static void c_initialize_colors() {
    static unsigned char col2[ 3 ] = { 255,255,255 };
    static int firstcall = 1;
    int c,i,j;

    /* initialize the colormap */
    if (firstcall)
    {
        firstcall = 0;
        for (i=0; i<256; i++)
        {
            colors[i].pixel = i;
            colors[i].flags = DoRed|DoGreen|DoBlue;
        }
    }

    /* align the palette for hires graphics */
    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 3; j++)
        {
            c = (i & 1) ? col2[ j ] : 0;
            colors[ j+i*3+32].red = c;
            c = (i & 2) ? col2[ j ] : 0;
            colors[ j+i*3+32].green = c;
            c = (i & 4) ? col2[ j ] : 0;
            colors[ j+i*3+32].blue = c;
        }
    }

    colors[ COLOR_FLASHING_BLACK].red = 0;
    colors[ COLOR_FLASHING_BLACK].green = 0;
    colors[ COLOR_FLASHING_BLACK].blue = 0;

    colors[ COLOR_LIGHT_WHITE].red   = (255<<8)|255;
    colors[ COLOR_LIGHT_WHITE].green = (255<<8)|255;
    colors[ COLOR_LIGHT_WHITE].blue  = (255<<8)|255;

    colors[ COLOR_FLASHING_WHITE].red   = (255<<8)|255;
    colors[ COLOR_FLASHING_WHITE].green = (255<<8)|255;
    colors[ COLOR_FLASHING_WHITE].blue  = (255<<8)|255;

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

    for (i=0; i<16; i++)
    {
        colors[i].red = (colors[i].red<<8) | colors[i].red;
        colors[i].green = (colors[i].green<<8) | colors[i].green;
        colors[i].blue = (colors[i].blue<<8) | colors[i].blue;

        colors[i<<4].red = (colors[i<<4].red<<8) | colors[i<<4].red;
        colors[i<<4].green = (colors[i<<4].green<<8) | colors[i<<4].green;
        colors[i<<4].blue = (colors[i<<4].blue<<8) | colors[i<<4].blue;
    }

    // store the colors to the current colormap
    //XStoreColors(display, cmap, colors, 256);
}


/* HACK: this is incredibly broken.
 * Map the X keysyms back into scancodes so the routines in keys.c can deal
 * with it.  We do this to be compatible with what the SVGAlib version does.
 */
static int keysym_to_scancode(void) {
    static int rc = 0xFF;

    switch (rc = XkbKeycodeToKeysym(display, xevent.xkey.keycode, 0, 0))
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
        {
            rc = xevent.xkey.keycode - 8;
        }

        break;
    }

    return rc & 0xFF; /* normalize */
}

static void post_image() {
    // copy Apple //e video memory into XImage uint32_t buffer
    uint8_t *fb = !video__current_page ? video__fb1 : video__fb2;
    uint8_t index;

    unsigned int count = SCANWIDTH * SCANHEIGHT;
    for (unsigned int i=0, j=0; i<count; i++, j+=4)
    {
        index = *(fb + i);
        *( (uint32_t*)(image->data + j) ) = (uint32_t)(
            ((uint32_t)(colors[index].red)   << red_shift)   |
            ((uint32_t)(colors[index].green) << green_shift) |
            ((uint32_t)(colors[index].blue)  << blue_shift)  |
            ((uint32_t)0xff /* alpha */ << alpha_shift)
            );
        if (scale == VIDEO_SCALE_2)
        {
            j+=4;

            // duplicate pixel
            *( (uint32_t*)(image->data + j) ) = (uint32_t)(
                ((uint32_t)(colors[index].red)   << red_shift)   |
                ((uint32_t)(colors[index].green) << green_shift) |
                ((uint32_t)(colors[index].blue)  << blue_shift)  |
                ((uint32_t)0xff /* alpha */ << alpha_shift)
                );

            if (((i+1) % SCANWIDTH) == 0)
            {
                // duplicate entire row
                int stride8 = SCANWIDTH<<3;//*8
                memcpy(/* dest */image->data + j + 4, /* src */image->data + j + 4 - stride8, stride8);
                j += stride8;
            }
        }
    }

    // post image...
    if (doShm)
    {
        if (!XShmPutImage(
                display,
                win,
                gc,
                image,
                0, 0,
                0, 0,
                width, height,
                True))
        {
            fprintf(stderr, "XShmPutImage() failed\n");
        }
    }
    else
    {
        if (XPutImage(
                display,
                win,
                gc,
                image,
                0, 0,
                0, 0,
                width, height
                ))
        {
            fprintf(stderr, "XPutImage() failed\n");
        }
    }
}

static void c_flash_cursor(int on) {
    // flash only if it's text or mixed modes.
    if (softswitches & (SS_TEXT|SS_MIXED))
    {
        if (!on)
        {
            colors[ COLOR_FLASHING_BLACK].red   = 0;
            colors[ COLOR_FLASHING_BLACK].green = 0;
            colors[ COLOR_FLASHING_BLACK].blue  = 0;

            colors[ COLOR_FLASHING_WHITE].red   = 0xffff;
            colors[ COLOR_FLASHING_WHITE].green = 0xffff;
            colors[ COLOR_FLASHING_WHITE].blue  = 0xffff;
        }
        else
        {
            colors[ COLOR_FLASHING_WHITE].red   = 0;
            colors[ COLOR_FLASHING_WHITE].green = 0;
            colors[ COLOR_FLASHING_WHITE].blue  = 0;

            colors[ COLOR_FLASHING_BLACK].red   = 0xffff;
            colors[ COLOR_FLASHING_BLACK].green = 0xffff;
            colors[ COLOR_FLASHING_BLACK].blue  = 0xffff;
        }

        // store the colors to the current colormap
        //XStoreColors(display, cmap, colors, 256);
    }
}

extern void c_handle_input(int scancode, int pressed);

/* FIXME: blocking not implemented... */
void video_sync(int block) {
    static int flash_count = 0;
    // post the image and loop waiting for it to finish and
    // also process other input events
    post_image();

    bool keyevent = true;
    do {
        if (doShm)
        {
            XNextEvent(display, &xevent);
            keyevent = !(xevent.type == xshmeventtype);
        }
        else
        {
            keyevent = XCheckMaskEvent(display, KeyPressMask|KeyReleaseMask, &xevent);
        }

        int scancode = -1;
        int pressed = 0;
        switch (xevent.type)
        {
        case KeyPress:
            scancode = keysym_to_scancode();
            pressed = 1;
            break;
        case KeyRelease:
            scancode = keysym_to_scancode();
            pressed = 0;
            break;
        default:
            break;
        }

        c_handle_input(scancode, pressed);

    } while (keyevent);

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

#if 0
static Cursor hidecursor() {
    Pixmap cursormask;
    XGCValues xgc;
    XColor dummycolour;
    Cursor cursor;
    GC cursor_gc;

    cursormask = XCreatePixmap(display, win, 1, 1, 1 /*depth*/);
    xgc.function = GXclear;
    cursor_gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask, &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,cursor_gc);
    return cursor;
}
#endif

static void parseArgs() {
    int i;
    for (i=0; i<argc; i++)
    {
        if (strstr(argv[i], "-noshm"))
        {
            doShm=0;
        }
        else if (strstr(argv[i], "-2"))
        {
            scale=VIDEO_SCALE_2;
        }
    }
}

void video_init() {
    XSetWindowAttributes attribs;
    unsigned long attribmask;
    int x, y;           /* window position */
    int drawingok;
    //unsigned int display_width, display_height;
    XGCValues xgcvalues;
    int valuemask;
    char *window_name = "Apple ][";
    char *icon_name = window_name;
    XSizeHints *size_hints;
    XWMHints *wm_hints;
    XClassHint *class_hints;
    XTextProperty windowName, iconName;
    //GC gc;
    char *progname;    /* name this program was invoked by */
    char *displayname = NULL;

    progname = argv[0];

    /* give up root privileges. equivalent of vga_init() */
    //setegid(getgid());
    //seteuid(getuid());

    parseArgs();

    if (!(size_hints = XAllocSizeHints()))
    {
        fprintf(stderr, "cannot allocate memory for SizeHints\n");
        exit(1);
    }

    if (!(wm_hints = XAllocWMHints()))
    {
        fprintf(stderr, "cannot allocate memory for WMHints\n");
        exit(1);
    }

    if (!(class_hints = XAllocClassHint()))
    {
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

    screen_num = DefaultScreen(display);
    // Note that in a real Xlib application, we would support more than the default visual :-P
    //visual = DefaultVisual(display, screen_num);
    //XVisualInfo *visuals_list=NULL;
    //visualinfo.screen=screen_num;
/*
        int numvisuals=0;
        if (!(visuals_list = XGetVisualInfo(display, VisualScreenMask, &visualinfo, &numvisuals))) {
            fprintf(stderr, "XGetVisualInfo() failed...");
            exit(1);
        }
        visualinfo = visuals_list[0];
        if ( (visualinfo.class == PseudoColor) || (visualinfo.depth == 8) ) {
            fprintf(stderr, "PseudoColor or 8bit color is unimplemented, FIXME!");
            exit(1);
        }
        XFree(visuals_list);
 */
    if (!XMatchVisualInfo(display, XDefaultScreen(display), 32, TrueColor, &visualinfo))
    {
        fprintf(stderr, "no such visual\n");
        exit(1);
    }

    // determine mask bits ...
    //   red_mask: 00ff0000
    //   green_mask: 0000ff00
    //   blue_mask: 000000ff
    //   bits_per_rgb: 8
    unsigned int shift = 0;
    for (unsigned int i=0; i<4; i++)
    {
        if        ((((uint32_t)visualinfo.red_mask  >>shift) & 0xff) == (uint32_t)0xff)
        {
            red_shift   = shift;
        }
        else if ((((uint32_t)visualinfo.green_mask>>shift) & 0xff) == (uint32_t)0xff)
        {
            green_shift = shift;
        }
        else if ((((uint32_t)visualinfo.blue_mask >>shift) & 0xff) == (uint32_t)0xff)
        {
            blue_shift  = shift;
        }
        else
        {
            alpha_shift = shift;
        }

        shift += 8;
    }

    if ((!red_shift) && (!green_shift) && (!blue_shift))
    {
        fprintf(stderr, "Could not calculate red/green/blue color masks...\n");
        exit(1);
    }

    fprintf(stderr, "red mask:%08x green mask:%08x blue mask:%08x\n", (uint32_t)visualinfo.red_mask, (uint32_t)visualinfo.blue_mask, (uint32_t)visualinfo.green_mask);
    fprintf(stderr, "redshift:%08d greenshift:%08d blueshift:%08d alphashift:%08d\n", red_shift, blue_shift, green_shift, alpha_shift);

    /* Note that in a real Xlib application, x and y would default to 0
     * but would be settable from the command line or resource database.
     */
    x = y = 0;
    width = SCANWIDTH*scale;
    height = SCANHEIGHT*scale;

    /* init MITSHM if we're doing it */
    if (doShm)
    {
        /* make sure we have it */
        doShm = XShmQueryExtension(display);
    }

    displayname = getenv("DISPLAY");
    if (displayname)
    {
        if (*displayname != ':')
        {
            printf("NOTE: Sound not allowed for remote display \"%s\".\n", displayname);
            if (doShm)
            {
                printf("NOTE: Cannot run MITSHM version of emulator with display \"%s\"\n"
                       "Try setting DISPLAY to something like \":0.0\"...Reverting to regular X.\n",
                       displayname);
            }

            doShm=0;
            //soundAllowed=0; FIXME TODO enforce this ...
        }
    }

    /* initialize colors */
    c_initialize_colors();

    cmap = XCreateColormap(display, XDefaultRootWindow(display), visualinfo.visual, AllocNone);
    //XStoreColors(display, cmap, colors, 256);
    attribs.colormap = cmap;
    attribs.border_pixel = 0;

    /* select event types wanted */
    attribmask = CWEventMask | CWColormap | CWBorderPixel;    /* HACK CWBorderPixel? */
    attribs.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask;

    /* create opaque window */
    win = XCreateWindow(display, RootWindow(display, screen_num),
                        x, y, width, height,
                        0, /* border_width */
                        visualinfo.depth, /* depth */
                        InputOutput,
                        visualinfo.visual,
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
    if (XStringListToTextProperty(&window_name, 1, &windowName) == 0)
    {
        fprintf(stderr, "structure allocation for windowName failed.\n");
        exit(1);
    }

    if (XStringListToTextProperty(&icon_name, 1, &iconName) == 0)
    {
        fprintf(stderr, "structure allocation for iconName failed.\n");
        exit(1);
    }

    // set up window manager hints...
    wm_hints->initial_state = NormalState;
    wm_hints->input = True;
    wm_hints->flags = StateHint | IconPixmapHint /* | InputHint*/;

    class_hints->res_name = progname;
    class_hints->res_class = "Apple2";

    XSetWMProperties(display, win, &windowName, &iconName,
                     argv, argc, size_hints, wm_hints,
                     class_hints);

    // FIXME!!!!! hidecursor segfaults
    //XDefineCursor(display, win, hidecursor(display, win));

    /* create the GC */
    valuemask = GCGraphicsExposures;
    xgcvalues.graphics_exposures = False;
    gc = XCreateGC(display, win, valuemask, &xgcvalues);

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

    // pad pixels to uint32_t boundaries
    int bitmap_pad = sizeof(uint32_t);
    int pixel_buffer_size = width*height*bitmap_pad;

    xshmeventtype = XShmGetEventBase(display) + ShmCompletion;

    /* create the image */
    if (doShm)
    {
        image = XShmCreateImage(display, visualinfo.visual, visualinfo.depth, ZPixmap, NULL, &xshminfo, width, height);

        if (!image)
        {
            fprintf(stderr, "XShmCreateImage failed\n");
            exit(1);
        }

        printf("Allocating shared memory: bytes_per_line:%ds height:x%d (depth:%d) bitmap_pad:%d\n",
               image->bytes_per_line, image->height, visualinfo.depth, bitmap_pad);

        getshm(pixel_buffer_size);

        /* get the X server to attach to it */
        if (!XShmAttach(display, &xshminfo))
        {
            fprintf(stderr, "XShmAttach() failed in InitGraphics()\n");
            exit(1);
        }
    }
    else
    {
        void *data = malloc(pixel_buffer_size);     // pad to uint32_t
        if (!data)
        {
            fprintf(stderr, "no memory for image data!\n");
            exit(1);
        }

        printf("Creating regular XImage\n");
        image = XCreateImage(display, visualinfo.visual, visualinfo.depth, ZPixmap, 0 /*offset*/, data, width, height, 8, width*bitmap_pad /*bytes_per_line*/);

        if (!image)
        {
            fprintf(stderr, "XCreateImage failed\n");
            exit(1);
        }
    }

    video__fb1 = vga_mem_page_0;
    video__fb2 = vga_mem_page_1;

    // reset Apple2 softframebuffers
    memset(video__fb1,0,SCANWIDTH*SCANHEIGHT);
    memset(video__fb2,0,SCANWIDTH*SCANHEIGHT);

#ifdef KEYPAD_JOYSTICK
    int autorepeat_supported = 0;
    XkbGetDetectableAutoRepeat(display, &autorepeat_supported);
    if (autorepeat_supported)
    {
        LOG("Xkb Setting detectable autorepeat ...");
        XkbSetDetectableAutoRepeat(display, true, NULL);
    }
#endif
}

void video_shutdown(void)
{
    if (doShm)
    {
        // Detach from X server
        if (!XShmDetach(display, &xshminfo))
        {
            fprintf(stderr,"XShmDetach() failed in video_shutdown()\n");
        }

        // Release shared memory.
        shmdt(xshminfo.shmaddr);
        shmctl(xshminfo.shmid, IPC_RMID, 0);

        // Paranoia.
        image->data = NULL;
    }
    else
    {
        free(image->data);
    }

    exit(0);
}
