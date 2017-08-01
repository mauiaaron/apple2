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

#define VIDEO_X11 1

#include "common.h"
#include "video/video.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#ifdef HAVE_X11_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h> /* MITSHM! */
#endif

static Display *display;
static Window win;
static GC gc;
static unsigned int width, height;      /* window size */
static unsigned int scale = 2;

static int screen_num;
static XVisualInfo visualinfo;
static XImage *image=NULL;
static Colormap cmap;
static XEvent xevent;

static XSizeHints *size_hints=NULL;
static XWMHints *wm_hints=NULL;
static XClassHint *class_hints=NULL;
static XTextProperty windowName, iconName;

static uint32_t red_shift;
static uint32_t green_shift;
static uint32_t blue_shift;
static uint32_t alpha_shift;

#ifdef HAVE_X11_SHM
static int doShm = 1;            /* assume true */
static XShmSegmentInfo xshminfo;
static int xshmeventtype;
#endif

// pad pixels to uint32_t boundaries
static int bitmap_pad = sizeof(uint32_t);

static bool request_set_mode = false;
static a2_video_mode_t request_mode = VIDEO_2X;

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
} FullScreenHints;

#ifdef HAVE_X11_SHM
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

    if ((size_t)(image->data) == -1)
    {
        perror("shmat");
        printf("Could not attach to shared memory\n");
        exit(1);
    }

    printf("Using shared memory key=`%c%c%c%c', id=%d, addr=%p\n", (key & 0xff000000)>>24, (key & 0xff0000)>>16, (key & 0xff00)>>8, (key & 0xff), id, image->data);
}
#endif

// Map X keysyms into Apple//ix internal-representation scancodes.
static int keysym_to_scancode(void) {
    int rc = XkbKeycodeToKeysym(display, xevent.xkey.keycode, 0, 0);
    switch (rc)
    {
    case XK_F1:
        rc = SCODE_F1; break;
    case XK_F2:
        rc = SCODE_F2; break;
    case XK_F3:
        rc = SCODE_F3; break;
    case XK_F4:
        rc = SCODE_F4; break;
    case XK_F5:
        rc = SCODE_F5; break;
    case XK_F6:
        rc = SCODE_F6; break;
    case XK_F7:
        rc = SCODE_F7; break;
    case XK_F8:
        rc = SCODE_F8; break;
    case XK_F9:
        rc = SCODE_F9; break;
    case XK_F10:
        rc = SCODE_F10; break;
    case XK_F11:
        rc = SCODE_F11; break;
    case XK_F12:
        rc = SCODE_F12; break;
    case XK_Left:
        rc = SCODE_L; break;
    case XK_Right:
        rc = SCODE_R; break;
    case XK_Down:
        rc = SCODE_D; break;
    case XK_Up:
        rc = SCODE_U; break;
    case XK_Escape:
        rc = SCODE_ESC; break;
    case XK_Return:
        rc = SCODE_RET; break;
    case XK_Tab:
        rc = SCODE_TAB; break;
    case XK_Shift_L:
        rc = SCODE_L_SHIFT; break;
    case XK_Shift_R:
        rc = SCODE_R_SHIFT; break;
    case XK_Control_L:
        rc = SCODE_L_CTRL; break;
    case XK_Control_R:
        rc = SCODE_R_CTRL; break;
    case XK_Caps_Lock:
    {
        // NOTE : emulator initially sets state based on a user preference.  But if the user actually taps the Caps Lock
        // key, then sync to system state.
        unsigned int caps_state = 0;
        XkbGetIndicatorState(display, XkbUseCoreKbd, &caps_state);
        caps_lock = (caps_state & 0x01);
        rc = -1; break;
    }
    case XK_BackSpace:
        rc = SCODE_BS; break;
    case XK_Insert:
        rc = SCODE_INS; break;
    case XK_Pause:
        rc = SCODE_PAUSE; break;
    case XK_Break:
        /* Pause and Break are the same key, but have different
         * scancodes (on PC keyboards).  Ctrl makes the difference.
         *
         * We assume the X server is passing along the distinction to us,
         * rather than making us check Ctrl manually.
         */
        rc = SCODE_BRK; break;
    case XK_Print:
        rc = SCODE_PRNT; break;
    case XK_Delete:
        rc = SCODE_DEL; break;
    case XK_End:
        rc = SCODE_END; break;
    case XK_Home:
        rc = SCODE_HOME; break;
    case XK_Page_Down:
        rc = SCODE_PGDN; break;
    case XK_Page_Up:
        rc = SCODE_PGUP; break;

    // keypad joystick movement
    case XK_KP_5:
    case XK_KP_Begin:
        rc = SCODE_KPAD_C; break;
    case XK_KP_4:
    case XK_KP_Left:
        rc = SCODE_KPAD_L; break;
    case XK_KP_8:
    case XK_KP_Up:
        rc = SCODE_KPAD_U; break;
    case XK_KP_6:
    case XK_KP_Right:
        rc = SCODE_KPAD_R; break;
    case XK_KP_2:
    case XK_KP_Down:
        rc = SCODE_KPAD_D; break;
    case XK_KP_7:
    case XK_KP_Home:
        rc = SCODE_KPAD_UL; break;
        break;
    case XK_KP_9:
    case XK_KP_Page_Up:
        rc = SCODE_KPAD_UR; break;
        break;
    case XK_KP_1:
    case XK_KP_End:
        rc = SCODE_KPAD_DL; break;
        break;
    case XK_KP_3:
    case XK_KP_Page_Down:
        rc = SCODE_KPAD_DR; break;
        break;

    case XK_Alt_L:
        rc = SCODE_L_ALT; break;
    case XK_Alt_R:
        rc = SCODE_R_ALT; break;

    default:
        if ((rc >= XK_space) && (rc <= XK_asciitilde))
        {
            rc = xevent.xkey.keycode - 8;
        }
        else
        {
            rc = -1; // unmapped
        }
        break;
    }

    assert(rc < 0x80);
    return rc;
}

static void post_image() {
    // copy Apple //e video memory into XImage uint32_t buffer
    uint8_t *fb = video_scan();
    uint8_t index;

    unsigned int count = SCANWIDTH * SCANHEIGHT;
    for (unsigned int i=0, j=0; i<count; i++, j+=4)
    {
        index = *(fb + i);
        *( (uint32_t*)(image->data + j) ) = (uint32_t)(
            ((uint32_t)(colormap[index].red)   << red_shift)   |
            ((uint32_t)(colormap[index].green) << green_shift) |
            ((uint32_t)(colormap[index].blue)  << blue_shift)  |
            ((uint32_t)0xff /* alpha */ << alpha_shift)
            );
        if (scale > 1)
        {
            j+=4;

            // duplicate pixel
            *( (uint32_t*)(image->data + j) ) = (uint32_t)(
                ((uint32_t)(colormap[index].red)   << red_shift)   |
                ((uint32_t)(colormap[index].green) << green_shift) |
                ((uint32_t)(colormap[index].blue)  << blue_shift)  |
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
#ifdef HAVE_X11_SHM
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
    } else
#endif
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

void video_driver_sync(void) {

    // post the image and loop waiting for it to finish and
    // also process other input events
    post_image();

    if (!XPending(display)) {
        c_keys_handle_input(/*scancode:*/-1, /*pressed:*/0, 0);
    } else {
        do {
            XNextEvent(display, &xevent);

            int scancode = -1;
            int pressed = 0;
            if (xevent.type == KeyPress || xevent.type == KeyRelease) {
                scancode = keysym_to_scancode();
                pressed = (xevent.type == KeyPress);
            }

            c_keys_handle_input(scancode, pressed, 0);
        } while (XPending(display));
    }
}

static void _redo_image(void);

static void xdriver_main_loop(void) {
    struct timespec sleeptime = { .tv_sec=0, .tv_nsec=16666667 }; // 60Hz
    do {
        if (request_set_mode) {
            request_set_mode = false;
            _redo_image();
        }
        video_driver_sync();
        nanosleep(&sleeptime, NULL);
    } while (1);
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
#ifdef HAVE_X11_SHM
        if (strstr(argv[i], "--noshm"))
        {
            doShm=0;
        } else
#endif
        if (strstr(argv[i], "--help") || strstr(argv[i], "-h")) {
            printf("%s v%s emulator help :\n", PACKAGE_NAME, PACKAGE_VERSION);
            printf("\tPress F10 for main menu\n");
            printf("\tPress F1 and F2 to insert disk images\n");
            exit(0);
        } else if (strstr(argv[i], "--version") || strstr(argv[i], "-v")) {
            printf("%s v%s\n", PACKAGE_NAME, PACKAGE_VERSION);
            exit(0);
        }
    }
}

static void _destroy_image() {
#ifdef HAVE_X11_SHM
    if (doShm)
    {
        // Detach from X server
        if (!XShmDetach(display, &xshminfo))
        {
            fprintf(stderr,"XShmDetach() failed\n");
        }

        XDestroyImage(image);

        // Release shared memory.
        shmdt(xshminfo.shmaddr);
        shmctl(xshminfo.shmid, IPC_RMID, 0);
    } else
#endif
    {
        XDestroyImage(image);
        //free(image->data);
    }
}

static void _create_image() {
    int pixel_buffer_size = width*height*bitmap_pad;

#ifdef HAVE_X11_SHM
    if (doShm) {
        image = XShmCreateImage(display, visualinfo.visual, visualinfo.depth, ZPixmap, NULL, &xshminfo, width, height);

        if (!image) {
            ERRQUIT("XShmCreateImage failed");
        }

        LOG("Allocating shared memory: bytes_per_line:%ds height:x%d (depth:%d) bitmap_pad:%d",
               image->bytes_per_line, image->height, visualinfo.depth, bitmap_pad);

        getshm(pixel_buffer_size);

        /* get the X server to attach to it */
        if (!XShmAttach(display, &xshminfo)) {
            ERRQUIT("XShmAttach() failed");
        }
    } else
#endif
    {
        void *data = malloc(pixel_buffer_size);
        if (!data) {
            ERRQUIT("no memory for image data!");
        }

        LOG("WARNING!!! No XShm extension ... creating regular XImage");
        image = XCreateImage(display, visualinfo.visual, visualinfo.depth, ZPixmap, 0 /*offset*/, data, width, height, 8, width*bitmap_pad /*bytes_per_line*/);

        if (!image) {
            ERRQUIT("XCreateImage failed");
        }
    }
}

static void _size_hints_set_fixed() {
    size_hints->flags = PPosition | PSize | PMinSize | PMaxSize;
    size_hints->min_width = width;
    size_hints->min_height = height;
    size_hints->max_width = XDisplayWidth(display, 0);
    size_hints->max_height = XDisplayHeight(display, 0);
}

static void _size_hints_set_resize() {
    size_hints->flags = USPosition | USSize | PMinSize | PMaxSize;
    size_hints->min_width = width;
    size_hints->min_height = height;
    size_hints->max_width = XDisplayWidth(display, 0);
    size_hints->max_height = XDisplayHeight(display, 0);
}

void video_set_mode(a2_video_mode_t mode) {
    request_mode = mode;
    request_set_mode = true;
}

static void _redo_image(void) {
    _destroy_image();

    scale = (int)request_mode;
    if (scale == VIDEO_FULLSCREEN) {
        scale = 2; // HACK FIXME for now ................
    }

    width = SCANWIDTH*scale;
    height = SCANHEIGHT*scale;

    _size_hints_set_resize();

    //XResizeWindow(display, win, width, height);
    XSetWMProperties(display, win, &windowName, &iconName,
                     argv, argc, size_hints, wm_hints,
                     class_hints);

#if 0
    // TODO ...
    // Fullscreen mode really should "fuzz" the graphics like AppleWin does when emulating NTSC.
    // Also will need to verify the canonical way to switch to fullscreen in X11
    //      http://www.tonyobryan.com/index.php?article=9
    if (mode == VIDEO_FULLSCREEN) {
        FullScreenHints hints = { .flags=2, .decorations=0 };
        Atom property = XInternAtom(display, "_MOTIF_WM_HINTS", True);

        XChangeProperty(display, win, property, property, 32, PropModeReplace, (unsigned char *)&hints, 5);

        /*
        int modecount_return = 0;
        XF86VidModeModeInfo *modesinfo = NULL;
        XF86VidModeGetAllModeLines(display, DefaultScreen(display), &modecount_return, &modesinfo);
        XF86VidModeSwitchToMode(display, DefaultScreen(display), video_mode);
        XF86VidModeSetViewPort(display, DefaultScreen(display), 0, 0);
        */

        int display_w = XDisplayWidth(display, 0);
        int display_h = XDisplayHeight(display, 0);
        LOG("Fullscreen : %d x %d", display_w, display_h);

        XMoveResizeWindow(display, win, 0, 0, display_w, display_h);
        XMapRaised(display, win);
        XGrabPointer(display, win, True, 0, GrabModeAsync, GrabModeAsync, win, 0L, CurrentTime);
        XGrabKeyboard(display, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
    }
#endif

    XWindowChanges changes = { .width=width, .height=height };
    XConfigureWindow(display, win, CWWidth|CWHeight, &changes);

    _create_image();

    _size_hints_set_fixed();
}

static void xdriver_init(void *context) {
    XSetWindowAttributes attribs;
    unsigned long attribmask;
    int x, y;           /* window position */
    int drawingok;
    //unsigned int display_width, display_height;
    XGCValues xgcvalues;
    int valuemask;
    char *window_name = "Apple //ix";
    char *icon_name = window_name;
    //GC gc;
    char *displayname = NULL;

    if (argv == NULL) {
        LOG("No command line arguments ...");
        argc = 0;
    } else {
        parseArgs();
    }

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
        if      ((((uint32_t)visualinfo.red_mask  >>shift) & 0xff) == (uint32_t)0xff)
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

#if 1
    scale = VIDEO_2X;
#else
    scale = a2_video_mode;
    if (a2_video_mode == VIDEO_FULLSCREEN) {
        scale = 1; // HACK FIXME FOR NOW ...
    }
#endif

    /* Note that in a real Xlib application, x and y would default to 0
     * but would be settable from the command line or resource database.
     */
    x = y = 0;
    width = SCANWIDTH*scale;
    height = SCANHEIGHT*scale;

#ifdef HAVE_X11_SHM
    /* init MITSHM if we're doing it */
    if (doShm)
    {
        /* make sure we have it */
        doShm = XShmQueryExtension(display);
    }
#endif

    displayname = getenv("DISPLAY");
    if (displayname)
    {
        if (*displayname != ':')
        {
            printf("NOTE: Sound not allowed for remote display \"%s\".\n", displayname);
#ifdef HAVE_X11_SHM
            if (doShm)
            {
                printf("NOTE: Cannot run MITSHM version of emulator with display \"%s\"\n"
                       "Try setting DISPLAY to something like \":0.0\"...Reverting to regular X.\n",
                       displayname);
            }

            doShm=0;
            //soundAllowed=0; FIXME TODO enforce this ...
#endif
        }
    }

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

    class_hints->res_name = "apple2ix";
    class_hints->res_class = "Apple2";

    _size_hints_set_fixed();
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

#ifdef HAVE_X11_SHM
    xshmeventtype = XShmGetEventBase(display) + ShmCompletion;
#endif

    _create_image();

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

static void xdriver_shutdown(bool emulatorShuttingDown) {
    _destroy_image();
}

static void xdriver_render(void) {
    // no-op
}

static void _init_xvideo(void) {
    LOG("Initializing X11 renderer");

    static video_backend_s xvideo_backend = { 0 };
    static video_animation_s xdriver_animations = { 0 };
    xvideo_backend.init      = &xdriver_init;
    xvideo_backend.main_loop = &xdriver_main_loop;
    xvideo_backend.render    = &xdriver_render;
    xvideo_backend.shutdown  = &xdriver_shutdown;
    xvideo_backend.anim      = &xdriver_animations;
    video_registerBackend(&xvideo_backend, VID_PRIO_GRAPHICS_X);
}

static __attribute__((constructor)) void __init_xvideo(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_EARLY, &_init_xvideo);
}

