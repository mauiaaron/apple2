/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2017 Aaron Culliney
 *
 */

// ncurses renderer (for those of us who still CLI ;)

#include "common.h"
#include "video/video.h"

#include <sys/poll.h>

#if HAVE_NCURSES_H
#   include <ncurses.h>
#elif HAVE_NCURSESW_NCURSES_H
#   include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_NCURSES_H
#   include <ncurses/ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#   include <ncurses/curses.h>
#elif HAVE_CURSES_H
#   include <curses.h>
#endif

#define THIRTYFPS NANOSECONDS_PER_SECOND / 30UL // 30FPS

#define ASCII_BACK  0x08
#define ASCII_TAB   0x09
#define ASCII_LF    0x0a
#define ASCII_CR    0x0d
#define ASCII_ESC   0x1b

//#define COLOR_BLACK   0
//#define COLOR_MAGENTA 5
#define COLOR_DARKBLUE  32
#define COLOR_PURPLE    33
#define COLOR_DARKGREEN 34
#define COLOR_DARKGREY  35
#define COLOR_MEDBLUE   36
#define COLOR_LIGHTBLUE 37
#define COLOR_BROWN     38
#define COLOR_ORANGE    39
#define COLOR_LIGHTGREY 40
#define COLOR_PINK      41
//#define COLOR_GREEN   2
//#define COLOR_YELLOW  3
#define COLOR_AQUA      42
//#define COLOR_WHITE   7

static bool ncvideo_running = true;

static WINDOW *winCurr = NULL;
static WINDOW *winMenu = NULL;  // 24 x 80 text
static WINDOW *winTxt40 = NULL; // 24 x 40 text
static WINDOW *winTxt80 = NULL; // 24 x 80 text
static WINDOW *winScale = NULL; // graphics mode scaled/interpolated to terminal dimensions

// ----------------------------------------------------------------------------
// ncurses video backend helper routines

static void _nc_convertAppleGlyphs(INOUT chtype *c, OUTPARM char **s, uint8_t *cs) {
    switch (*c) {

        case 0x7f: // cursor ...
            *c = '_';
            *cs = BLACK_ON_RED;
            break;

        case 0x80: // closed apple ...
            *c = '@';
            *cs = BLACK_ON_MAGENTA;
            break;
        case 0x81: // open apple ...
            *c = '@';
            *cs = BLACK_ON_RED;
            break;
        case 0x82: // caret ...
            *c = '^';
            *cs = BLACK_ON_RED;
            break;
        case 0x83: // hourglass ...
            *c = '%';
            *cs = BLACK_ON_RED;
            break;
        case 0x84: // checkmark ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "✓";
#else
            *c = 'x';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x85: // reverse checkmark ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "✓";
            ////*cs = INVERSE_CURRENT;
#else
            *c = 'X';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x86: // runner left ...
            *c = ']';
            *cs = BLACK_ON_RED;
            break;
        case 0x87: // runner right ...
            *c = '[';
            *cs = BLACK_ON_RED;
            break;
        case 0x88: // left arrow ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "←";
#else
            *c = '<';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x89: // ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "…";
#else
            *c = '.';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x8a: // down arrow ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "↓";
#else
            *c = '!';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x8b: // up arrow ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "↑";
#else
            *c = '^';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x8c: // top bar ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "¯";
#else
            *c = '-';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x8d: // CR ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "⏎";
#else
            *c = '^';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x8e: // block ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "█";
#else
            *c = '#';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x8f: // filled left arrow ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "⇦";
#else
            *c = '<';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x90: // filled right arrow ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "⇨";
#else
            *c = '>';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x91: // filled down arrow ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "⇩";
#else
            *c = '!';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x92: // filled up arrow ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "⇧";
#else
            *c = '^';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x93: // mdash ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "—";
#else
            *c = '-';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x94: // box bottom left ...
            *c = '\\';
            *cs = BLACK_ON_RED;
            break;
        case 0x95: // right arrow ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "→";
#else
            *c = '>';
#endif
            *cs = BLACK_ON_RED;
            break;

        case 0x96: // hash #1 ...
            *c = '#';
            *cs = BLACK_ON_RED;
            break;
        case 0x97: // hash #2 ...
            *c = '#';
            *cs = BLACK_ON_RED;
            break;
        case 0x98: // folder left ...
            *c = 'f';
            *cs = BLACK_ON_RED;
            break;
        case 0x99: // folder right ...
            *c = 'f';
            *cs = BLACK_ON_RED;
            break;
        case 0x9a: // right full bar ...
            *c = '|';
            *cs = BLACK_ON_RED;
            break;
        case 0x9b: // diamond ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "◆";
#else
            *c = '*';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x9c: // top and bottom bars ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "◆";
#else
            *c = '=';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x9d: // white plus ...
            *c = '+';
            *cs = BLACK_ON_RED;
            break;
        case 0x9e: // box and dot ...
#if NCURSES_UTF8
            *c = '\0';
            *s = "▣";
#else
            *c = '#';
#endif
            *cs = BLACK_ON_RED;
            break;
        case 0x9f: // left full bar ...
            *c = '|';
            *cs = BLACK_ON_RED;
            break;

        // interface menus ...
        case 0xA0:
        case 0xA3:
            *c = '/';
            break;
        case 0xA1:
        case 0xA2:
            *c = '\\';
            break;
        case 0xA4:
            *c = '|';
            break;
        case 0xA5:
            *c = '-';
            break;
        case 0xA6:
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA:
            *c = '+';
            break;
    }
}

static int _nc_keyToEmulator(int ncKey, bool *is_ascii) {
    int a2key = ncKey;

    *is_ascii = false;
    switch (ncKey) {

        case KEY_F(1):
            a2key = SCODE_F1;
            break;
        case KEY_F(2):
            a2key = SCODE_F2;
            break;
        case KEY_F(3):
            a2key = SCODE_F3;
            break;
        case KEY_F(4):
            a2key = SCODE_F4;
            break;
        case KEY_F(5):
            a2key = SCODE_F5;
            break;
        case KEY_F(6):
            a2key = SCODE_F6;
            break;
        case KEY_F(7):
            a2key = SCODE_F7;
            break;
        case KEY_F(8):
            a2key = SCODE_F8;
            break;
        case KEY_F(9):
            a2key = SCODE_F9;
            break;
        case KEY_F(10):
            a2key = SCODE_F10;
            break;
        case KEY_F(11):
            a2key = SCODE_F11;
            break;
        case KEY_F(12):
            a2key = SCODE_F12;
            break;

        case KEY_DOWN:
            a2key = SCODE_D;
            break;
        case KEY_BACKSPACE:
        case KEY_LEFT:
        case ASCII_BACK:
            a2key = SCODE_L;
            break;
        case KEY_RIGHT:
            a2key = SCODE_R;
            break;
        case KEY_UP:
            a2key = SCODE_U;
            break;

        case ASCII_LF:
        case ASCII_CR:
            a2key = SCODE_RET;
            break;

        case ASCII_ESC:
            a2key = SCODE_ESC;
            break;

        case ASCII_TAB:
        default:
            if (a2key >= 0 && a2key < 256) {
                is_ascii = true;
            } else {
                a2key = -1;
            }
            break;
    }

    return a2key;
}

#define COLOR_VALUES(x) \
            ((NCURSES_COLOR_T)(colormap[x].red  ) * 1000)/255, \
            ((NCURSES_COLOR_T)(colormap[x].green) * 1000)/255, \
            ((NCURSES_COLOR_T)(colormap[x].blue)  * 1000)/255

static void _nc_initColors(void) {
    if (has_colors() == FALSE) {
        LOG("Your terminal does not support color, so emulated color values will not be correct...");
        return;
    }

    if (can_change_color() == FALSE) {
        LOG("Your terminal does not support changing color values, so emulated color values will not be correct...");
        // drop through to start default color support ...
    }

    int ret = start_color();
    if (ret == ERR) {
        LOG("OOPS, could not initialize ncurses colors");
    }

    // ncurses scales colors between 0 - 1000

    ret = init_color(COLOR_MAGENTA  ,COLOR_VALUES(IDX_MAGENTA  ));
    ret = init_color(COLOR_DARKBLUE ,COLOR_VALUES(IDX_DARKBLUE ));
    ret = init_color(COLOR_PURPLE   ,COLOR_VALUES(IDX_PURPLE   ));
    ret = init_color(COLOR_DARKGREEN,COLOR_VALUES(IDX_DARKGREEN));
    ret = init_color(COLOR_DARKGREY ,COLOR_VALUES(IDX_DARKGREY ));
    ret = init_color(COLOR_MEDBLUE  ,COLOR_VALUES(IDX_MEDBLUE  ));
    ret = init_color(COLOR_LIGHTBLUE,COLOR_VALUES(IDX_LIGHTBLUE));
    ret = init_color(COLOR_BROWN    ,COLOR_VALUES(IDX_BROWN    ));
    ret = init_color(COLOR_ORANGE   ,COLOR_VALUES(IDX_ORANGE   ));
    ret = init_color(COLOR_LIGHTGREY,COLOR_VALUES(IDX_LIGHTGREY));
    ret = init_color(COLOR_PINK     ,COLOR_VALUES(IDX_PINK     ));
    ret = init_color(COLOR_GREEN    ,COLOR_VALUES(IDX_GREEN    ));
    ret = init_color(COLOR_YELLOW   ,COLOR_VALUES(IDX_YELLOW   ));
    ret = init_color(COLOR_AQUA     ,COLOR_VALUES(IDX_AQUA     ));

    // interface and mousetext colors
    init_pair(1+GREEN_ON_BLACK,     COLOR_GREEN, COLOR_BLACK    );
    init_pair(1+GREEN_ON_BLUE,      COLOR_GREEN, COLOR_BLUE     );
    init_pair(1+RED_ON_BLACK,       COLOR_RED,   COLOR_BLACK    );
    init_pair(1+BLUE_ON_BLACK,      COLOR_BLUE,  COLOR_BLACK    );
    init_pair(1+WHITE_ON_BLACK,     COLOR_WHITE, COLOR_BLACK    );

    init_pair(1+BLACK_ON_RED,       COLOR_BLACK, COLOR_RED      );

    // 16 COLORS:
    init_pair(1+BLACK_ON_BLACK,     COLOR_BLACK, COLOR_BLACK    );
    init_pair(1+BLACK_ON_MAGENTA,   COLOR_BLACK, COLOR_MAGENTA  );
    init_pair(1+BLACK_ON_DARKBLUE,  COLOR_BLACK, COLOR_DARKBLUE );
    init_pair(1+BLACK_ON_PURPLE,    COLOR_BLACK, COLOR_PURPLE   );
    init_pair(1+BLACK_ON_DARKGREEN, COLOR_BLACK, COLOR_DARKGREEN);
    init_pair(1+BLACK_ON_DARKGREY,  COLOR_BLACK, COLOR_DARKGREY );
    init_pair(1+BLACK_ON_MEDBLUE,   COLOR_BLACK, COLOR_MEDBLUE  );
    init_pair(1+BLACK_ON_LIGHTBLUE, COLOR_BLACK, COLOR_LIGHTBLUE);
    init_pair(1+BLACK_ON_BROWN,     COLOR_BLACK, COLOR_BROWN    );
    init_pair(1+BLACK_ON_ORANGE,    COLOR_BLACK, COLOR_ORANGE   );
    init_pair(1+BLACK_ON_LIGHTGREY, COLOR_BLACK, COLOR_LIGHTGREY);
    init_pair(1+BLACK_ON_PINK,      COLOR_BLACK, COLOR_PINK     );
    init_pair(1+BLACK_ON_GREEN,     COLOR_BLACK, COLOR_GREEN    );
    init_pair(1+BLACK_ON_YELLOW,    COLOR_BLACK, COLOR_YELLOW   );
    init_pair(1+BLACK_ON_AQUA,      COLOR_BLACK, COLOR_AQUA     );
    init_pair(1+BLACK_ON_WHITE,     COLOR_BLACK, COLOR_WHITE    );
}

static WINDOW *_nc_newwin(unsigned int height, int width, int starty, int startx) {
    WINDOW *win = newwin(height+2, width+2, starty-1, startx-1);
    //box(win, 0 , 0);
    //meta(win, TRUE);
    return win;
}

static void _nc_delwin(WINDOW **winRef) {
    wborder(*winRef, ' ', ' ', ' ',' ',' ',' ',' ',' ');
    wrefresh(*winRef);
    delwin(*winRef);
    *winRef = NULL;
}

// ----------------------------------------------------------------------------
// ncurses video backend main routines

static const char *ncvideo_name(void) {
    return "ncurses";
}

static void ncvideo_init(void *context) {
    // ...
}

static void ncvideo_main_loop(void) {

    // start curses mode and check colors ...
    initscr();
    _nc_initColors();

    LOG("ncurses video main loop beginning ...");

    noecho();               // Do not echo output ...
    raw();                  // Line buffering disabled ...
    keypad(stdscr, TRUE);   // Special and F keys ...
    nodelay(stdscr, TRUE);  // getch() is non-blocking ...

    int starty24 = (LINES - TEXT_ROWS) / 2;
    int startx40 = (COLS - 40) / 2;
    int startx80 = (COLS - 80) / 2;

    winMenu  = _nc_newwin(TEXT_ROWS, 80, starty24, startx80);
    winTxt40 = _nc_newwin(TEXT_ROWS, 40, starty24, startx40);
    winTxt80 = _nc_newwin(TEXT_ROWS, 80, starty24, startx80);
    winScale = _nc_newwin(LINES, COLS, 0, 0);

    ////wbkgd(winTxt40, COLOR_PAIR(1)); // GREEN ON BLACK (for now)
    ////wbkgd(winTxt80, COLOR_PAIR(1)); // GREEN ON BLACK (for now)

    static uint8_t fb[SCANWIDTH*SCANHEIGHT*sizeof(uint8_t)];

#if INTERFACE_CLASSIC
    interface_setStagingFramebuffer(fb);
#endif

    while (ncvideo_running) {

        unsigned long wasDirty = 0UL;
        if (!cpu_isPaused()) {
            // check if a2 video memory is dirty
            wasDirty = video_clearDirty(A2_DIRTY_FLAG);
            if (wasDirty) {
                display_renderStagingFramebuffer(fb);
            }
        }

        if (interface_isShowing()) {
            winCurr = winMenu;
            WINDOW *winPrev = winCurr;
            if (winPrev != winCurr) {
                wclear(winPrev);
            }
        }

        wasDirty = video_clearDirty(FB_DIRTY_FLAG);
        if (wasDirty) {
            wrefresh(winCurr);
        }

        // handle keyboard input
        int c = getch();
        if (c == ERR) {
            keys_handleInput(/*scancode:*/-1, /*is_pressed:*/false, /*is_ascii:*/false);
        } else {
            bool is_ascii = false;
            c = _nc_keyToEmulator(c, &is_ascii);
            keys_handleInput(/*scancode:*/c, /*is_pressed:*/true,  is_ascii);
            keys_handleInput(/*scancode:*/c, /*is_pressed:*/false, is_ascii);
        }

        // FIXME TODO ... does not account for execution time drift
        struct timespec thirtyfps = { .tv_sec = 0, .tv_nsec = THIRTYFPS };
        nanosleep(&thirtyfps, NULL);
    }

    _nc_delwin(&winMenu);
    _nc_delwin(&winTxt40);
    _nc_delwin(&winTxt80);
    _nc_delwin(&winScale);

    endwin();
}

static void ncvideo_render(void) {
    // no-op ...
}

static void ncvideo_shutdown(void) {
    ncvideo_running = false;
}

// ----------------------------------------------------------------------------
// plotting callbacks ...

static void _nc_graphicsUpdate(pixel_delta_t pixel) {
    // TODO FIXME ... actually need to scale/plot graphics here

    uint8_t cs = pixel.cs;
    uint8_t row = pixel.row;
    uint8_t col = pixel.col;

    assert(cs == COLOR16);

    WINDOW *win = winCurr;
    chtype c = 0;
    char *s = NULL;

    uint8_t color = (pixel.b & 0x0F);
    c = ' ';
    switch (color) {
        case 0x0: // Black
            cs = BLACK_ON_BLACK;
            break;
        case 0x1: // Magenta
            cs = BLACK_ON_MAGENTA;
            break;
        case 0x2: // Dark Blue
            cs = BLACK_ON_DARKBLUE;
            break;
        case 0x3: // Purple
            cs = BLACK_ON_PURPLE;
            break;
        case 0x4: // Dark Green
            cs = BLACK_ON_DARKGREEN;
            break;
        case 0x5: // Dark Grey
            cs = BLACK_ON_DARKGREY;
            break;
        case 0x6: // Medium Blue
            cs = BLACK_ON_MEDBLUE;
            break;
        case 0x7: // Light Blue
            cs = BLACK_ON_LIGHTBLUE;
            break;
        case 0x8: // Brown
            cs = BLACK_ON_BROWN;
            break;
        case 0x9: // Orange
            cs = BLACK_ON_ORANGE;
            break;
        case 0xa: // Light Grey
            cs = BLACK_ON_LIGHTGREY;
            break;
        case 0xb: // Pink
            cs = BLACK_ON_PINK;
            break;
        case 0xc: // Green
            cs = BLACK_ON_GREEN;
            break;
        case 0xd: // Yellow
            cs = BLACK_ON_YELLOW;
            break;
        case 0xe: // Aqua
            cs = BLACK_ON_AQUA;
            break;
        case 0xf: // WHITE
            cs = BLACK_ON_WHITE;
            break;
    }
}

static void _nc_textUpdate(pixel_delta_t pixel) {
    uint8_t cs = pixel.cs;
    uint8_t row = pixel.row;
    uint8_t col = pixel.col;

    WINDOW *win = winCurr;
    chtype c = 0;
    char *s = NULL;
    font_mode_t fontMode = FONT_MODE_NORMAL;
    do {
        if (cs == COLOR16) {
            // LORES/DLORES ...
            _nc_graphicsUpdate(pixel);
            return;
        } else if (cs == INVALID_COLORSCHEME) {
            // TEXT
            c = keys_apple2ASCII(pixel.b, &fontMode);
            _nc_convertAppleGlyphs(&c, &s, &cs);
        } else {
            // interface menu
            c = pixel.b;
            _nc_convertAppleGlyphs(&c, &s, &cs);
            win = winMenu;
        }

        attr_t attrs = 0;
        {
            short ignoredPair = 0;
            if (wattr_get(win, &attrs, &ignoredPair, /*opts:*/NULL) == ERR) {
                break;
            }
        }

        if (fontMode == FONT_MODE_INVERSE) {
            wattron(win, A_REVERSE);
        } else if (fontMode == FONT_MODE_FLASH) {
            wattron(win, A_BOLD); // TODO FIXME ... A_BLINK not reliable, but maybe switch between normal and A_REVERSE at the flash rate?
        }

        if (cs != INVALID_COLORSCHEME) {
            wattr_set(win, attrs, cs+1, /*opts:*/NULL);
        }
#if NCURSES_UTF8
        if (s) {
            mvwaddstr(win, row+1, col+1, s);
        } else
#endif
            mvwaddch(win, row+1, col+1, c);
        if (cs != INVALID_COLORSCHEME) {
            wattr_set(win, attrs, 0, /*opts:*/NULL);
        }


        if (fontMode == FONT_MODE_INVERSE) {
            wattroff(win, A_REVERSE);
        } else if (fontMode == FONT_MODE_FLASH) {
            wattroff(win, A_BOLD);
        }
    } while (0);
}

static void _nc_modeChange(pixel_delta_t pixel) {
#warning FIXME TODO, this copies display.c/vm.c routines somewhat, likely need to consolidate if/when other video backends implement video update callbacks
    if (interface_isShowing()) {
        return;
    }

    WINDOW *winPrev = winCurr;
    uint32_t currswitches = run_args.softswitches;
    if ((currswitches & SS_TEXT) && !(currswitches & SS_MIXED)) {
        winCurr = (currswitches & SS_80COL) ? winTxt80 : winTxt40;
    } else {
        winCurr = winScale;
    }
    if (winPrev != winCurr) {
        wclear(winPrev);
        wrefresh(winPrev);
    }
}

// ----------------------------------------------------------------------------

static void _init_ncvideo(void) {
    LOG("Initializing ncurses renderer");

    static video_backend_s ncvideo_backend = { 0 };
    ncvideo_backend.name      = &ncvideo_name;
    ncvideo_backend.init      = &ncvideo_init;
    ncvideo_backend.main_loop = &ncvideo_main_loop;
    ncvideo_backend.render    = &ncvideo_render;
    ncvideo_backend.shutdown  = &ncvideo_shutdown;

    static video_animation_s ncvideo_anim = {
#if 1
        0
        // FIXME TODO ... likely we need to follow the 'glnode' model with 'ncnode.c' and render the ncalert.c stuff over the underlying winCurr here ...
#else
        .animation_showMessage = &_ncanim_showMessage;
        .animation_showPaused = &_ncanim_showPaused;
        .animation_showCPUSpeed = &_ncanim_showCPUSpeed;
        .animation_showDiskChosen = &_ncanim_showDiskChosen;
        .animation_showTrackSector = &_ncanim_showTrackSector;
#endif
    };
    ncvideo_backend.anim      = &ncvideo_anim;

    video_registerBackend(&ncvideo_backend, VID_PRIO_TERMINAL);
    display_setUpdateCallback(DRAWPAGE_TEXT, &_nc_textUpdate);
    display_setUpdateCallback(DRAWPAGE_MODE_CHANGE, &_nc_modeChange);
}

static __attribute__((constructor)) void __init_ncvideo(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_EARLY, &_init_ncvideo);
}

