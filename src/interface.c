/*
 * Apple // emulator for Linux: Configuration Interface
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

#if INTERFACE_TOUCH
// touch interface managed elsewhere
bool (*interface_onTouchEvent)(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) = NULL;
bool (*interface_isTouchMenuAvailable)(void) = NULL;
void (*interface_setTouchMenuEnabled)(bool enabled) = NULL;
#endif

// 2015/04/12 : This was legacy code for rendering the menu interfaces on desktop Linux. Portions here are resurrected
// to render HUD messages on desktop and mobile.  Nothing special or pretty here, but has "just worked" for 20+ years ;-)

#define IsGraphic(c) ((c) == '|' || (((unsigned char)c) >= ICONTEXT_BEGIN && ((unsigned char)c) <= ICONTEXT_MENU_END))
#define IsInside(x,y) ((x) >= 0 && (x) <= xlen-1 && (y) >= 0 && (y) <= ylen-1)

// Draws special interface menu "characters" 
static void _convert_screen_graphics(char *screen, const int x, const int y, const int xlen, const int ylen) {
    static char map[11][3][4] ={ { "...",
                                   ".||",
                                   ".|." },

                                 { "...",
                                   "||.",
                                   ".|." },

                                 { ".|.",
                                   ".||",
                                   "..." },

                                 { ".|.",
                                   "||.",
                                   "..." },

                                 { "~|~",
                                   ".|.",
                                   "~|~" },

                                 { "~.~",
                                   "|||",
                                   "~.~" },

                                 { ".|.",
                                   ".||",
                                   ".|." },

                                 { ".|.",
                                   "||.",
                                   ".|." },

                                 { "...",
                                   "|||",
                                   ".|." },

                                 { ".|.",
                                   "|||",
                                   "..." },

                                 { ".|.",
                                   "|||",
                                   ".|." } };

    bool found_glyph = false;
    int k = 10;
    for (; k >= 0; k--) {
        found_glyph = true;

        for (int yy = y - 1; found_glyph && yy <= y + 1; yy++) {
            int idx = yy*(xlen+1);
            for (int xx = x - 1; xx <= x + 1; xx++) {
                char map_ch = map[k][ yy - y + 1 ][ xx - x + 1 ];

                if (IsInside(xx, yy)) {
                    char c = *(screen + idx + xx);
                    if (!IsGraphic( c ) && (map_ch == '|')) {
                        found_glyph = false;
                        break;
                    } else if (IsGraphic( c ) && (map_ch == '.')) {
                        found_glyph = false;
                        break;
                    }
                } else if (map_ch == '|') {
                    found_glyph = false;
                    break;
                }
            }
            idx += xlen+1;
        }

        if (found_glyph) {
            break;
        }
    }

    if (found_glyph) {
        *(screen + y*(xlen+1) + x) = ICONTEXT_BEGIN + k;
    }
}

static void _translate_screen_x_y(char *screen, const int xlen, const int ylen) {
    for (int idx=0, y=0; y < ylen; y++, idx+=xlen+1) {
        for (int x = 0; x < xlen; x++) {
            if (*(screen + idx + x) == '|') {
                _convert_screen_graphics(screen, x, y, xlen, ylen);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Menu/HUD message printing

static void _interface_plotLine(uint8_t *fb, int fb_pix_width, int fb_pix_x_adjust, int col, int row, interface_colorscheme_t cs, const char *message) {
    for (; *message; col++, message++) {
        char c = *message;
        unsigned int off = row * fb_pix_width * FONT_HEIGHT_PIXELS + col * FONT80_WIDTH_PIXELS + fb_pix_x_adjust;
        interface_plotChar(fb+off, fb_pix_width, cs, c);
    }
}

void interface_plotMessage(uint8_t *fb, interface_colorscheme_t cs, char *message, int message_cols, int message_rows) {
    _translate_screen_x_y(message, message_cols, message_rows);
    int fb_pix_width = (message_cols*FONT80_WIDTH_PIXELS);
    for (int row=0, idx=0; row<message_rows; row++, idx+=message_cols+1) {
        _interface_plotLine(fb, fb_pix_width, 0, 0, row, cs, &message[ idx ]);
    }
}

// ----------------------------------------------------------------------------
// Desktop Legacy Menu Interface

#ifdef INTERFACE_CLASSIC

static void _interface_plotMessageCentered(uint8_t *fb, int fb_cols, int fb_rows, interface_colorscheme_t cs, char *message, const int message_cols, const int message_rows) {
    _translate_screen_x_y(message, message_cols, message_rows);
    int col = (fb_cols - message_cols) >> 1;
    int row = (fb_rows - message_rows) >> 1;
    int fb_pix_width = (fb_cols*FONT80_WIDTH_PIXELS)+INTERPOLATED_PIXEL_ADJUSTMENT;
    assert(fb_pix_width == SCANWIDTH);
    int row_max = row + message_rows;
    for (int idx=0; row<row_max; row++, idx+=message_cols+1) {
        _interface_plotLine(fb, fb_pix_width, _INTERPOLATED_PIXEL_ADJUSTMENT_PRE, col, row, cs, &message[ idx ]);
    }
}

static struct stat statbuf = { 0 };
static int altdrive = 0;
bool in_interface = false;

void video_plotchar(const int col, const int row, const interface_colorscheme_t cs, const uint8_t c) {
    unsigned int off = row * SCANWIDTH * FONT_HEIGHT_PIXELS + col * FONT80_WIDTH_PIXELS + _INTERPOLATED_PIXEL_ADJUSTMENT_PRE;
    interface_plotChar(video__fb1+off, SCANWIDTH, cs, c);
}

void copy_and_pad_string(char *dest, const char* src, const char c, const int len, const char cap) {
    const char* p = src;
    char* d = dest;

    for (; ((*p != '\0') && (p-src < len-1)); p++) {
        *d++ = *p;
    }

    while (d-dest < len-1) {
        *d++ = c;
    }

    *d = cap;
}

static void pad_string(char *s, const char c, const int len) {
    char *p = s;

    for (; ((*p != '\0') && (p-s < len-1)); p++) {
        // counting ...
    }

    while (p-s < len-1) {
        *p++ = c;
    }

    *p = '\0';
}

void c_interface_print( int x, int y, const interface_colorscheme_t cs, const char *s ) {
    _interface_plotLine(video__fb1, SCANWIDTH, _INTERPOLATED_PIXEL_ADJUSTMENT_PRE, x, y, cs, s);
}

/* -------------------------------------------------------------------------
    c_interface_print_screen()
   ------------------------------------------------------------------------- */
void c_interface_print_screen( char screen[24][INTERFACE_SCREEN_X+1] ) {
    for (int y = 0; y < 24; y++) {
        c_interface_print( 0, y, 2, screen[ y ] );
    }
}

static void c_interface_translate_screen_x_y(char *screen, const int xlen, const int ylen) {
    _translate_screen_x_y(screen, xlen, ylen);
}

void c_interface_translate_screen( char screen[24][INTERFACE_SCREEN_X+1] ) {
    c_interface_translate_screen_x_y(screen[0], INTERFACE_SCREEN_X, 24);
}

void c_interface_print_submenu_centered( char *submenu, const int message_cols, const int message_rows ) {
    _interface_plotMessageCentered(video__fb1, INTERFACE_SCREEN_X, TEXT_ROWS, RED_ON_BLACK, submenu, message_cols, message_rows);
}

/* ------------------------------------------------------------------------- */

static int c_interface_cut_name(char *name)
{
    char *p = name + strlen(name) - 1;
    int is_gz = 0;

    if (p >= name && *p == 'z')
    {
        p--;
        if (p >= name && *p == 'g')
        {
            p--;
            if (p >= name && *p == '.')
            {
                *p-- = '\0';
                is_gz = 1;
            }
        }
    }

    return is_gz;
}

static int disk_select(const struct dirent *e)
{
    static char cmp[ DISKSIZE ];
    size_t len;
    const char *p;

    strncpy( cmp, disk_path, DISKSIZE );
    strncat( cmp, "/", DISKSIZE-1 );
    strncat( cmp, e->d_name, DISKSIZE-1 );

    /* don't show disk in alternate drive */
    if (!strcmp(cmp, disk6.disk[altdrive].file_name))
    {
        return 0;
    }

    /* show directories except '.' and '..' at toplevel. */
    stat(cmp, &statbuf);
    if (S_ISDIR(statbuf.st_mode) && strcmp(".", e->d_name) &&
        !(!strcmp("..", e->d_name) && !strcmp(disk_path, "/")))
    {
        return 1;
    }

    p = e->d_name;
    len = strlen(p);

    if (len > 3 && (!strcmp(p + len - 3, ".gz")))
    {
        len -= 3;
    }

    if (!strncmp(p + len - 4, ".dsk", 4))
    {
        return 1;
    }

    if (!strncmp(p + len - 4, ".nib", 4))
    {
        return 1;
    }

    if (!strncmp(p + len - 3, ".do", 3))
    {
        return 1;
    }

    if (!strncmp(p + len - 3, ".po", 3))
    {
        return 1;
    }

    return 0;
}

/* -------------------------------------------------------------------------
    c_interface_exit()
   ------------------------------------------------------------------------- */

void c_interface_exit(int ch)
{
    if (c_keys_is_interface_key(ch))
    {
        c_keys_set_key(ch);
    }
    else
    {
        video_setpage(!!(softswitches & SS_SCREEN));
        video_redraw();
    }
}

/* -------------------------------------------------------------------------
    c_interface_select_diskette()
   ------------------------------------------------------------------------- */
#define ZLIB_SUBMENU_H 7
#define ZLIB_SUBMENU_W 40
static char zlibmenu[ZLIB_SUBMENU_H][ZLIB_SUBMENU_W+1] =
//1.  5.  10.  15.  20.  25.  30.  35.  40.
{ "||||||||||||||||||||||||||||||||||||||||",
    "|                                      |",
    "| An error occurred when attempting to |",
    "| handle a compressed disk image:      |",
    "|                                      |",
    "|                                      |",
    "||||||||||||||||||||||||||||||||||||||||" };

static void _eject_disk(int drive) {
    const char *err_str = c_eject_6(drive);
    if (err_str) {
        int ch = -1;
        snprintf(&zlibmenu[4][2], 37, "%s", err_str);
        c_interface_print_submenu_centered(zlibmenu[0], ZLIB_SUBMENU_W, ZLIB_SUBMENU_H);
        while ((ch = c_mygetch(1)) == -1) {
            // ...
        }
    }
}

void c_interface_select_diskette( int drive )
{
#define DISK_PATH_MAX 77
#define DRIVE_X 49
    char screen[24][INTERFACE_SCREEN_X+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.  45.  50.  55.  60.  65.  70.  75.  80.",
    { "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                     Insert diskette into Drive _, Slot 6                     |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                         For interface help press '?'                         |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" };

    struct dirent **namelist;
    bool nextdir = false;
    int i, entries;
    static int curpos=0;
    int ch = -1;

    screen[ 1 ][ DRIVE_X ] = (drive == 0) ? 'A' : 'B';

    video_setpage( 0 );

    c_interface_translate_screen( screen );

    do {
        nextdir = false;
        c_interface_print_screen( screen );

        altdrive = (drive == 0) ? 1 : 0;
        if (!strcmp("", disk_path))
        {
            sprintf(disk_path, "/");
        }

#define DISKERR_PAD 35
#define DISKERR_SUBMENU_H 6
#define DISKERR_SUBMENU_W 40
        char errmenu[DISKERR_SUBMENU_H][DISKERR_SUBMENU_W+1] =
        //1.  5.  10.  15.  20.  25.  30.  35.  40.
        { "||||||||||||||||||||||||||||||||||||||||",
          "|                                      |",
          "| An error occurred:                   |",
          "|                                      |",
          "|                                      |",
          "||||||||||||||||||||||||||||||||||||||||" };
#define DISKERR_SHOWERR(ERR) \
        copy_and_pad_string(&errmenu[3][2], ERR, ' ', DISKERR_PAD, ' '); \
        c_interface_print_submenu_centered(errmenu[0], DISKERR_SUBMENU_W, DISKERR_SUBMENU_H); \
        while ((ch = c_mygetch(1)) == -1) { }

        /* set to users privilege level for directory access */
        entries = scandir(disk_path, &namelist, disk_select, alphasort);

        if (entries <= 0)
        {
            DISKERR_SHOWERR("Problem reading directory");
            snprintf(disk_path, DISKSIZE, "%s", getenv("HOME"));
            nextdir = true;
            continue;
        }

        if (curpos >= entries)
        {
            curpos = entries - 1;
        }

        char temp[PATH_MAX];
        for (;;)
        {
            for (i = 0; i < 18; i++)
            {
                int ent_no = curpos - 8 + i, slen;
                int in_drive = 0;

                strcpy( temp, " " );
                if (ent_no >= 0 && ent_no < entries)
                {
                    snprintf(temp, PATH_MAX, "%s/%s",
                             disk_path, namelist[ent_no]->d_name);
                    if (!strcmp(temp, disk6.disk[drive].file_name))
                    {
                        in_drive = 1;
                    }

                    stat(temp, &statbuf);
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        snprintf(temp, PATH_MAX, " %s/",
                                 namelist[ ent_no ]->d_name );
                    }
                    else
                    {
                        snprintf(temp, PATH_MAX, " %s",
                                 namelist[ ent_no ]->d_name );
                    }

                    if (c_interface_cut_name(temp))
                    {
                        strncat(temp, " <gz>", PATH_MAX-1);
                    }
                    /* write protected disk in drive? */
                    else if ((in_drive) && (disk6.disk[drive].is_protected))
                    {
                        strncat(temp, (drive == 0) ? " <r1>" : " <r2>", PATH_MAX-1);
                    }
                    else if (in_drive)
                    {
                        strncat(temp, (drive == 0) ? " <rw1>" : " <rw2>", PATH_MAX-1);
                    }
                }

                slen = strlen( temp );
                while (slen < DISK_PATH_MAX)
                {
                    temp[ slen++ ] = ' ';
                }

                temp[ DISK_PATH_MAX ] = '\0';

                c_interface_print(1, i + 3, ent_no == curpos, temp);
            }

            do
            {
                ch = c_mygetch(1);
            }
            while (ch == -1);

            if (ch == kUP)
            {
                if (curpos > 0)
                {
                    curpos--;
                }
            }
            else if (ch == kDN)
            {
                if (curpos < entries - 1)
                {
                    curpos++;
                }
            }
            else if (ch == kPGDN)
            {
                curpos += 16;
                if (curpos > entries - 1)
                {
                    curpos = entries - 1;
                }
            }
            else if (ch == kPGUP)
            {
                curpos -= 16;
                if (curpos < 0)
                {
                    curpos = 0;
                }
            }
            else if (ch == kHOME)
            {
                curpos = 0;
            }
            else if (ch == kEND)
            {
                curpos = entries - 1;
            }
            else if ((ch == kESC) || c_keys_is_interface_key(ch))
            {
                break;
            }
            else if (ch == '?')
            {
#define DISKHELP_SUBMENU_H 17
#define DISKHELP_SUBMENU_W 40
                char submenu[DISKHELP_SUBMENU_H][DISKHELP_SUBMENU_W+1] =
                //1.  5.  10.  15.  20.  25.  30.  35.  40.
                { "||||||||||||||||||||||||||||||||||||||||",
                  "|                                      |",
                  "|      Disk : @ and @ arrows           |",
                  "| Selection : PageUp/PageDown          |",
                  "|             Home/End                 |",
                  "|                                      |",
                  "|    Insert : (RO) Press 'Return' to   |",
                  "|      Disk : insert disk read-only    |",
                  "|             (RW) Press 'W' key to    |",
                  "|             insert disk read/write   |",
                  "|                                      |",
                  "|     Eject : Choose selected disk and |",
                  "|      Disk : press 'Return'           |",
                  "|                                      |",
                  "| Exit Menu : ESC returns to emulator  |",
                  "|                                      |",
                  "||||||||||||||||||||||||||||||||||||||||" };

                submenu[ 2 ][ 14 ] = MOUSETEXT_UP;
                submenu[ 2 ][ 20 ] = MOUSETEXT_DOWN;
                c_interface_print_submenu_centered(submenu[0], DISKHELP_SUBMENU_W, DISKHELP_SUBMENU_H);
                while ((ch = c_mygetch(1)) == -1)
                {
                }
                c_interface_print_screen( screen );
            }
            else if ((ch == 13) || (toupper(ch) == 'W'))
            {
                int len;

                snprintf(temp, PATH_MAX, "%s/%s",
                         disk_path, namelist[ curpos ]->d_name );
                len = strlen(disk_path);

                /* handle disk currently in the drive */
                if (!strcmp(temp, disk6.disk[drive].file_name))
                {
                    /* reopen disk, forcing write enabled */
                    if (toupper(ch) == 'W')
                    {
                        const char *err_str = c_new_diskette_6(drive, temp, 0);
                        if (err_str)
                        {
                            int ch = -1;
                            snprintf(&zlibmenu[4][2], 37, "%s", err_str);
                            c_interface_print_submenu_centered(zlibmenu[0], ZLIB_SUBMENU_W, ZLIB_SUBMENU_H);
                            while ((ch = c_mygetch(1)) == -1) {
                                // ...
                            }
                            c_interface_print_screen( screen );
                            continue;
                        }
                        else
                        {
                            if (video_backend->animation_showDiskChosen) {
                                video_backend->animation_showDiskChosen(drive);
                            }
                        }

                        break;
                    }

                    /* eject the disk and start over */
                    _eject_disk(drive);
                    c_interface_print_screen( screen );

                    nextdir = true;
                    break;
                }

                /* read another directory */
                stat(temp, &statbuf);
                if (S_ISDIR(statbuf.st_mode))
                {
                    if (toupper(ch) == 'W')
                    {
                        continue;
                    }

                    if ((disk_path[len-1]) == '/')
                    {
                        disk_path[--len] = '\0';
                    }

                    if (!strcmp("..", namelist[curpos]->d_name))
                    {
                        while (--len && (disk_path[len] != '/'))
                        {
                            disk_path[len] = '\0';
                        }
                    }
                    else if (strcmp(".", namelist[curpos]->d_name))
                    {
                        snprintf(disk_path + len, DISKSIZE-len, "/%s",
                                 namelist[curpos]->d_name);
                    }

                    nextdir = true;
                    break;
                }

                _eject_disk(drive);
                c_interface_print_screen( screen );

                const char *err_str = c_new_diskette_6(drive, temp, (toupper(ch) != 'W'));
                if (err_str)
                {
                    int ch = -1;
                    snprintf(&zlibmenu[4][2], 37, "%s", err_str);
                    c_interface_print_submenu_centered(zlibmenu[0], ZLIB_SUBMENU_W, ZLIB_SUBMENU_H);
                    while ((ch = c_mygetch(1)) == -1) {
                        // ...
                    }
                    c_interface_print_screen( screen );
                    continue;
                }
                else
                {
                    if (video_backend->animation_showDiskChosen) {
                        video_backend->animation_showDiskChosen(drive);
                    }
                }

                break;
            }
        }

        // clean up

        for (i = 0; i < entries; i++)
        {
            free(namelist[ i ]);
        }
        free(namelist);

    } while (nextdir);

    c_interface_exit(ch);
}

/* -------------------------------------------------------------------------
    c_interface_parameters()
   ------------------------------------------------------------------------- */

typedef enum interface_enum_t {
    OPT_CPU = 0,
    OPT_ALTCPU,
    OPT_QUIT,
    OPT_REBOOT,
    OPT_JOYSTICK,
    OPT_CALIBRATE,
    OPT_PATH,
    OPT_COLOR,
#if !VIDEO_OPENGL
    OPT_VIDEO,
#endif
    OPT_VOLUME,
    OPT_CAPS,

    NUM_OPTIONS
} interface_enum_t;

static const char *options[] =
{
    "     CPU% :  ",
    " ALT CPU% :  ",
    "        --> Quit Emulator",
    "        --> Reboot Emulator",
    " Joystick :  ",
    "        --> Calibrate Joystick",
    "     Path :  ",
    "    Color :  ",
#if !VIDEO_OPENGL
    "    Video :  ",
#endif
    "   Volume :  ",
    " CAPSlock :  ",
};

#define INTERFACE_PATH_MAX 65
#define INTERFACE_PATH_MIN 14

void c_interface_parameters()
{
    char screen[24][INTERFACE_SCREEN_X+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.  45.  50.  55.  60.  65.  70.  75.  80.",
    { "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                                                                              |",
      "|                                @ Apple //ix @                                |",
      "|                                                                              |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                              Emulator Hotkeys                                |",
      "|                                                                              |",
      "| F1 F2: Insert Diskette in Slot6 Disk Drive A or Drive B                      |",
      "| F5   : Show Keyboard Layout                               F7 : 6502 Debugger |",
      "| F9   : Toggle Between CPU% / ALT CPU% Speeds                                 |",
      "| F10  : Show This Menu                                                        |",
      "|                                                                              |",
      "|               For interface help press '?' ... ESC exits menu                |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" };

#define PARAMS_H 9 /* visual height */
    int i;
    int ch;
    static interface_enum_t option = OPT_CPU;
    static int cur_y = 0, cur_off = 0, cur_x = 0, cur_pos = 0;

    /* reset the x position, so we don't lose our cursor if path changes */
    cur_x = 0;
    video_setpage( 0 );

    screen[ 2 ][ 33 ] = MOUSETEXT_OPENAPPLE;
    screen[ 2 ][ 46 ] = MOUSETEXT_CLOSEDAPPLE;

    c_interface_translate_screen( screen );
    c_interface_print_screen( screen );

#define TEMPSIZE 1024
    char temp[TEMPSIZE];
    for (;;)
    {
        for (i = 0; (i < PARAMS_H) && (i < NUM_OPTIONS); i++)
        {
            cur_off = (option - PARAMS_H) +1;
            if (cur_off < 0)
            {
                cur_off = 0;
            }

            c_interface_print( 1, 5 + i, cur_y == i, options[i + cur_off]);

            int optlen = strlen(options[i + cur_off]);
            snprintf(temp, TEMPSIZE, " ");
            pad_string(temp, ' ', INTERFACE_PATH_MAX+1-optlen);
            c_interface_print( 1+optlen, 5 + i, 0, temp );

            switch (i + cur_off)
            {
            case OPT_CPU:
                if (cpu_scale_factor >= CPU_SCALE_FASTEST)
                {
                    snprintf(temp, TEMPSIZE, "Fastest");
                }
                else
                {
                    snprintf(temp, TEMPSIZE, "%d%%", (int)(cpu_scale_factor * 100.0));
                }
                break;

            case OPT_ALTCPU:
                if (cpu_altscale_factor >= CPU_SCALE_FASTEST)
                {
                    snprintf(temp, TEMPSIZE, "Fastest");
                }
                else
                {
                    snprintf(temp, TEMPSIZE, "%d%%", (int)(cpu_altscale_factor * 100.0));
                }
                break;

            case OPT_PATH:
                strncpy(temp, disk_path + cur_pos, INTERFACE_PATH_MAX);
                temp[INTERFACE_PATH_MAX] = '\0';
                break;

            case OPT_COLOR:
                sprintf(temp, "%s", (color_mode == COLOR) ? "Color       " :
                        (color_mode == COLOR_INTERP) ? "Interpolated" : "Black/White ");
                break;

#if !VIDEO_OPENGL
            case OPT_VIDEO:
                sprintf(temp, "%s", (a2_video_mode == VIDEO_1X) ? "1X       " : (a2_video_mode == VIDEO_2X) ? "2X       " : "Fullscreen");
                break;
#endif

            case OPT_JOYSTICK:
                snprintf(temp, TEMPSIZE, "%s", (joy_mode == JOY_KPAD) ? "Emulated on Keypad" : "PC Joystick      ");
                break;

            case OPT_VOLUME:
                if (sound_volume == 0)
                {
                    snprintf(temp, TEMPSIZE, "%s", "Off       ");
                }
                else
                {
                    snprintf(temp, TEMPSIZE, "%d", sound_volume);
                }
                break;

            case OPT_CAPS:
                if (caps_lock)
                {
                    snprintf(temp, TEMPSIZE, "%s", "On        ");
                }
                else
                {
                    snprintf(temp, TEMPSIZE, "%s", "Off       ");
                }
                break;

            case OPT_CALIBRATE:
                break;

            case OPT_QUIT:
                break;

            case OPT_REBOOT:
                break;

            default:
                break;
            }

            pad_string(temp, ' ', INTERFACE_PATH_MAX+1);
            int loc = i+cur_off;
            if ((loc != OPT_PATH) && (loc != OPT_QUIT) && (loc != OPT_REBOOT) && (loc != OPT_CALIBRATE))
            {
                c_interface_print(INTERFACE_PATH_MIN, 5 + i, 0, temp);
            }
            else if (loc == OPT_PATH)
            {
                int j;

                for (j = 0; j < INTERFACE_PATH_MAX; j++)
                {
                    if (cur_x != j)
                    {
                        if (temp[ j ] == '\0')
                        {
                            video_plotchar( INTERFACE_PATH_MIN + j, 5+i, 0, ' ' );
                            j++;
                            break;
                        }
                        else
                        {
                            video_plotchar( INTERFACE_PATH_MIN + j, 5+i, 0, temp[ j ] );
                        }
                    }
                    else
                    {
                        if (temp[ j ] == '\0')
                        {
                            video_plotchar( INTERFACE_PATH_MIN + j, 5+i, option==OPT_PATH,' ' );
                            j++;
                            break;
                        }
                        else
                        {
                            video_plotchar( INTERFACE_PATH_MIN + j, 5+i, option==OPT_PATH, temp[ j ]);
                        }

                    }
                }

                for (; j < INTERFACE_PATH_MAX; j++)
                {
                    video_plotchar( INTERFACE_PATH_MIN + j, 5+i, 0, ' ' );
                }
            }
        }

        do
        {
            ch = c_mygetch(1);
        }
        while (ch == -1);

        if (ch == kUP)                  /* Arrow up */
        {
            if (option > PARAMS_H-1)
            {
                option--;               /* only dec option */
            }
            else if (option > 0)
            {
                option--;               /* dec option */
                cur_y--;                /* and dec y position */
            }
            else
            {
                option = NUM_OPTIONS-1; /* wrap to last option */
                cur_y = PARAMS_H-1;              /* wrap to last y position */
                if (cur_y >= NUM_OPTIONS)
                {
                    cur_y = NUM_OPTIONS-1;
                }
            }
        }
        else if (ch == kDN)                  /* Arrow down */
        {
            ++option;

            if (cur_y < PARAMS_H-1)
            {
                cur_y++;                /* and inc y position */
            }

            if (option >= NUM_OPTIONS)
            {
                cur_y = option = 0;     /* wrap both to first */
            }
        }
        else if ((ch == kLT) && (c_rawkey() != SCODE_BS))           /* Arrow left */
        {
            switch (option)
            {
            case OPT_CPU:
                cpu_scale_factor -= (cpu_scale_factor <= 1.0) ? CPU_SCALE_STEP_DIV : CPU_SCALE_STEP;
                if (cpu_scale_factor < CPU_SCALE_SLOWEST)
                {
                    cpu_scale_factor = CPU_SCALE_SLOWEST;
                }
                break;

            case OPT_ALTCPU:
                cpu_altscale_factor -= (cpu_altscale_factor <= 1.0) ? CPU_SCALE_STEP_DIV : CPU_SCALE_STEP;
                if (cpu_altscale_factor < CPU_SCALE_SLOWEST)
                {
                    cpu_altscale_factor = CPU_SCALE_SLOWEST;
                }
                break;

            case OPT_PATH:
                if (cur_x > 0)
                {
                    cur_x--;
                }
                else if (cur_pos > 0)
                {
                    cur_pos--;
                }
                break;

            case OPT_COLOR:
                if (color_mode == 0)
                {
                    color_mode = NUM_COLOROPTS-1;
                }
                else
                {
                    --color_mode;
                }
                break;

#if !VIDEO_OPENGL
            case OPT_VIDEO:
                if (a2_video_mode == 1)
                {
                    a2_video_mode = NUM_VIDOPTS-1;
                }
                else
                {
                    --a2_video_mode;
                }
                video_set_mode(a2_video_mode);
                break;
#endif

            case OPT_VOLUME:
                if (sound_volume > 0)
                {
                    --sound_volume;
                }
                break;

            case OPT_CAPS:
                if (caps_lock) {
                    caps_lock = false;
                }
                break;

            case OPT_JOYSTICK:
                if (joy_mode == 0)
                {
                    joy_mode = NUM_JOYOPTS-1;
                }
                else
                {
                    --joy_mode;
                }
                break;

            case OPT_CALIBRATE:
                break;

            case OPT_QUIT:
                break;

            case OPT_REBOOT:
                break;

            default:
                break;
            }
        }
        else if (ch == kRT)                         /* Arrow right */
        {
            switch (option)
            {
            case OPT_CPU:
                cpu_scale_factor += (cpu_scale_factor < 1.0) ? CPU_SCALE_STEP_DIV : CPU_SCALE_STEP;
                if (cpu_scale_factor >= CPU_SCALE_FASTEST)
                {
                    cpu_scale_factor = CPU_SCALE_FASTEST;
                }
                break;

            case OPT_ALTCPU:
                cpu_altscale_factor += (cpu_altscale_factor < 1.0) ? CPU_SCALE_STEP_DIV : CPU_SCALE_STEP;
                if (cpu_altscale_factor >= CPU_SCALE_FASTEST)
                {
                    cpu_altscale_factor = CPU_SCALE_FASTEST;
                }
                break;

            case OPT_PATH:
                if (cur_x < INTERFACE_PATH_MAX-1)
                {
                    if (disk_path[cur_pos + cur_x] != '\0')
                    {
                        cur_x++;
                    }
                }
                else if (disk_path[cur_pos + cur_x] != '\0')
                {
                    cur_pos++;
                }
                break;

            case OPT_COLOR:
                if (color_mode == NUM_COLOROPTS-1)
                {
                    color_mode = 0;
                }
                else
                {
                    ++color_mode;
                }
                break;

#if !VIDEO_OPENGL
            case OPT_VIDEO:
                if (a2_video_mode == NUM_VIDOPTS-1)
                {
                    a2_video_mode = 1;
                }
                else
                {
                    ++a2_video_mode;
                }
                video_set_mode(a2_video_mode);
                break;
#endif

            case OPT_VOLUME:
                sound_volume++;
                if (sound_volume > 10)
                {
                    sound_volume = 10;
                }
                break;

            case OPT_CAPS:
                if (!caps_lock) {
                    caps_lock = true;
                }
                break;

            case OPT_JOYSTICK:
                if (joy_mode == NUM_JOYOPTS-1)
                {
                    joy_mode = 0;
                }
                else
                {
                    ++joy_mode;
                }
                break;

            case OPT_CALIBRATE:
                break;

            case OPT_QUIT:
                break;

            case OPT_REBOOT:
                break;

            default:
                break;
            }
        }
        else if ((ch == kESC) || c_keys_is_interface_key(ch))
        {
            timing_initialize();
            video_reset();
            c_initialize_sound_hooks();
            c_joystick_reset();
            c_interface_exit(ch);
            return;
        }
        else if ((ch == '?') && (option != OPT_PATH))
        {
#define MAINHELP_SUBMENU_H 18
#define MAINHELP_SUBMENU_W 40
            char submenu[MAINHELP_SUBMENU_H][MAINHELP_SUBMENU_W+1] =
            //1.  5.  10.  15.  20.  25.  30.  35.  40.
            { "||||||||||||||||||||||||||||||||||||||||",
              "|  Movement : @ and @ arrows           |",
              "|                                      |",
              "|    Change : @ and @ arrows to toggle |",
              "|    Values : or press the 'Return'    |",
              "|             key to select            |",
              "||||||||||||||||||||||||||||||||||||||||",
              "| Hotkeys used while emulator running: |",
              "|                                      |",
              "| F1 F2: Slot6 Disk Drive A, Drive B   |",
              "| F5   : Show Keyboard Layout          |",
              "| F7   : 6502 Debugger                 |",
              "| F9   : Toggle Emulator Speed         |",
              "| F10  : Main Menu                     |",
              "|                                      |",
              "| Ctrl-LeftAlt-End Reboots //e         |",
              "| Pause/Brk : Pause Emulator           |",
              "||||||||||||||||||||||||||||||||||||||||" };
            submenu[ 1 ][ 14 ] = MOUSETEXT_UP;
            submenu[ 1 ][ 20 ] = MOUSETEXT_DOWN;
            submenu[ 3 ][ 14 ] = MOUSETEXT_LEFT;
            submenu[ 3 ][ 20 ] = MOUSETEXT_RIGHT;

            c_interface_print_submenu_centered(submenu[0], MAINHELP_SUBMENU_W, MAINHELP_SUBMENU_H);
            while ((ch = c_mygetch(1)) == -1)
            {
            }
            c_interface_print_screen( screen );
        }
        else
        {
            /* got a normal character setting path */
            if (ch >= ' ' && ch < 127 && option == OPT_PATH)
            {
                int i;

                strncpy(temp, disk_path, TEMPSIZE);
                for (i = strlen(temp); i >= cur_pos + cur_x; i--)
                {
                    temp[ i + 1 ] = temp[ i ];
                }

                temp[ cur_pos + cur_x ] = ch;
                strncpy(disk_path, temp, DISKSIZE);
                if (cur_x < INTERFACE_PATH_MAX-1)
                {
                    cur_x++;
                }
                else if (disk_path[cur_pos + cur_x] != '\0')
                {
                    cur_pos++;
                }
            }

            /* Backspace or delete setting path */
            if ((ch == 127 || ch == 8) && (cur_pos + cur_x - 1 >= 0) && (option == OPT_PATH))
            {
                int i;

                for (i = cur_pos + cur_x - 1; disk_path[ i ] != '\0'; i++)
                {
                    disk_path[ i ] = disk_path[ i + 1 ];
                }

                if (cur_x > 0)
                {
                    cur_x--;
                }
                else if (cur_pos > 0)
                {
                    cur_pos--;
                }
            }

            /* calibrate joystick */
            if ((ch == 13) && (option == OPT_CALIBRATE))
            {
                c_joystick_reset();
                c_calibrate_joystick();
                c_interface_print_screen( screen );
            }

#define QUIT_SUBMENU_H 10
#define QUIT_SUBMENU_W 40
            {
                char qsubmenu[QUIT_SUBMENU_H][QUIT_SUBMENU_W+1] =
                    //1.  5.  10.  15.  20.  25.  30.  35.  40.
                    { "||||||||||||||||||||||||||||||||||||||||",
                      "|                                      |",
                      "|                                      |",
                      "|                                      |",
                      "|            Quit Emulator...          |",
                      "|          Are you sure? (Y/N)         |",
                      "|                                      |",
                      "|                                      |",
                      "|                                      |",
                      "||||||||||||||||||||||||||||||||||||||||" };

                /* quit emulator */
                if ((ch == 13) && (option == OPT_QUIT))
                {
                    int ch;
                    c_interface_print_submenu_centered(qsubmenu[0], QUIT_SUBMENU_W, QUIT_SUBMENU_H);

                    while ((ch = c_mygetch(1)) == -1)
                    {
                    }

                    ch = toupper(ch);
                    if (ch == 'Y')
                    {
                        save_settings();
                        c_eject_6( 0 );
                        c_interface_print_screen( screen );
                        c_eject_6( 1 );
                        c_interface_print_screen( screen );
#ifdef __linux__
                        LOG("Back to Linux, w00t!\n");
#endif
                        video_shutdown();
                        c_interface_exit(ch);
                        return;
                    }
                }

                /* reboot emulator */
                if ((ch == 13) && (option == OPT_REBOOT)) {
                    int ch;
                    memcpy(qsubmenu[4]+11, "Reboot", 6);
                    c_interface_print_submenu_centered(qsubmenu[0], QUIT_SUBMENU_W, QUIT_SUBMENU_H);

                    while ((ch = c_mygetch(1)) == -1)
                    {
                    }

                    ch = toupper(ch);
                    if (ch == 'Y')
                    {
                        c_joystick_reset();
                        cpu65_reboot();
                        c_interface_exit(ch);
                        return;
                    }
                }

                c_interface_print_screen( screen );
            }
        }
    }
}

/* -------------------------------------------------------------------------
    c_interface_credits() - Credits and politics
   ------------------------------------------------------------------------- */

void c_interface_credits()
{
    char screen[24][INTERFACE_SCREEN_X+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.  45.  50.  55.  60.  65.  70.  75.  80.",
    { "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                                                                              |",
      "|                                @ Apple //ix @                                |",
      "|                                                                              |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "|                                                                              |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                 @ @ to scroll notes - ESC to begin emulation                 |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" };
#define SCROLL_AREA_X 2
#define SCROLL_AREA_Y 5
#define SCROLL_AREA_HEIGHT 16

    screen[ 2 ][ 33 ] = MOUSETEXT_OPENAPPLE;
    screen[ 2 ][ 46 ] = MOUSETEXT_CLOSEDAPPLE;

    screen[ 22 ][ 18 ] = MOUSETEXT_UP;
    screen[ 22 ][ 20 ] = MOUSETEXT_DOWN;

#define SCROLL_LENGTH 58
#define SCROLL_WIDTH (INTERFACE_SCREEN_X+1-(SCROLL_AREA_X*2))
    char credits[SCROLL_LENGTH+1][SCROLL_WIDTH]=
    //1.  5.  10.  15.  20.  25.  30.  35.  40.  45.  50.  55.  60.  65.  70.  75.  80.",
      { "                                                                            ",
        "                  An Apple //e Emulator for POSIX Systems!                  ",
        "                                                                            ",
        "                    @ Press F8 any time to return here @                    ",
        "                                                                            ",
        "QUICKSTART                                                                  ",
        "                                                                            ",
        "Press F8 at any time to return to this page                                 ",
        "Press F10 to view preferences menu                                          ",
        "Press F1 to load diskette in Slot 6, Drive A                                ",
        "Press F2 to load diskette in Slot 6, Drive B                                ",
        "                                                                            ",
        "AUTHORS/CREDITS                                                             ",
        "                                                                            ",
        "Copyright 1994 Alexander Jean-Claude Bottema                                ",
        "Copyright 1995 Stephen Lee                                                  ",
        "Copyright 1997, 1998 Aaron Culliney                                         ",
        "Copyright 1998, 1999, 2000 Michael Deutschmann                              ",
        "Copyright 2013+ Aaron Culliney                                              ",
        "                                                                            ",
        "ADDITIONAL CREDITS                                                          ",
        "                                                                            ",
        "This software uses various Free & Open Source software, including           ",  
        "                                                                            ",
        " > Audio source code derived from AppleWin -- http://applewin.berlios.de    ",
        " > OpenAL audio library -- http://sourceforge.net/projects/openal-soft      ",
        " > Compression routines from the Zlib project -- http://zlib.net            ",
        "                                                                            ",
        "LICENSE                                                                     ",
        "                                                                            ",
        "This Apple //ix emulator source code is subject to the GNU General Public   ",
        "License version 2 or later (your choice) as published by the Free Software  ",
        "Foundation.  https://fsf.org                                                ",
        "                                                                            ",
        "Emulator source is freely available at https://github.com/mauiaaron/apple2  ",
        "                                                                            ",
        "REPORTING BUGS                                                              ",
        "                                                                            ",
        "Bugs can be reported at https://github.com/mauiaaron/apple2/issues/         ",
        "                                                                            ",
        "FREEDOM                                                                     ",
        "                                                                            ",
        "In a world increasing constrained by digital walled gardens and draconian IP",
        "laws, these organizations are fighting for your computing freedom.  Please  ",
        "consider donating to them:                                                  ",
        "                                                                            ",
        " > Free Software Foundation       -- https://fsf.org                        ",
        " > Electronic Frontier Foundation -- https://eff.org                        ",
        "                                                                            ",
        "3RD PARTY SOFTWARE                                                          ",
        "                                                                            ",
        " > By using this software you agree to comply with all Intellectual Pooperty",
        "   laws in your jurisdiction                                                ",
        "                                                                            ",
        " > ROM images used by the emulator are copyright Apple Computer             ",
        "                                                                            ",
        " > Disk images are copyright by various third parties                       ",
        "                                                                            ",
        "                                                                            " };

    video_setpage( 0 );

    c_interface_translate_screen( screen );
    c_interface_translate_screen_x_y( credits[0], SCROLL_WIDTH, SCROLL_LENGTH);
    c_interface_print_screen( screen );

    int pos = 0;
    int ch = -1;
    int count = -1;
    unsigned int mt_idx = 0;
    for (;;)
    {
        ch = c_mygetch(0);

#define FLASH_APPLE_DELAY 3
        count = (count+1) % FLASH_APPLE_DELAY;
        if (!count)
        {
            mt_idx = (mt_idx+1) % 2;
            credits[ 3 ][ 20 ] = MOUSETEXT_BEGIN + mt_idx;
            credits[ 3 ][ 55 ] = MOUSETEXT_BEGIN + ((mt_idx+1) % 2);
        }

        for (int i=0, p=pos; i<SCROLL_AREA_HEIGHT; i++)
        {
            c_interface_print(SCROLL_AREA_X, SCROLL_AREA_Y+i, 2, credits[p]);
            p = (p+1) % SCROLL_LENGTH;
        }

        if (ch == kUP)
        {
            --pos;
            if (pos < 0)
            {
                pos = 0;
            }
        }
        else if (ch == kDN)
        {
            ++pos;
            if (pos >= SCROLL_LENGTH-SCROLL_AREA_HEIGHT)
            {
                pos = SCROLL_LENGTH-SCROLL_AREA_HEIGHT-1;
            }
        }
        else if ((ch == kESC) || c_keys_is_interface_key(ch))
        {
            break;
        }

        static struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }

    c_interface_exit(ch);
}

/* -------------------------------------------------------------------------
    c_interface_keyboard_layout()
   ------------------------------------------------------------------------- */

void c_interface_keyboard_layout()
{
    char screen[24][INTERFACE_SCREEN_X+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.  45.  50.  55.  60.  65.  70.  75.  80.",
    { "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                         Apple //e US Keyboard Layout                         |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                esc                                   del        RESET        |",
      "|              `~ 1! 2@ 3# 4$ 5% 6^ 7& 8* 9( 0) -_ =+  backspace               |",
      "|              tab  Q  W  E  R  T  Y  U  I  O  P [{ ]} \\|                      |",
      "|              caps  A  S  D  F  G  H  J  K  L ;: '\"  return        @          |",
      "|              shift  Z  X  C  V  B  N  M ,< .> /?  shift          @ @         |",
      "|                ctrl    @        space        @    ctrl            @          |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                                                           |    Quick Help    |",
      "| Left Alt is @ (OpenApple) key (Joystick button 1)         | F1: Disk Drive A |",
      "| Right Alt is @ (ClosedApple) key (Joystick button 2)      | F2: Disk Drive B |",
      "| End is //e RESET key                                      | F5: This Menu    |",
      "|                                                           | F8: Credits      |",
      "| Ctrl-End triggers //e reset vector                        |F10: Options      |",
      "| Ctrl-LeftAlt-End triggers //e reboot                      ||||||||||||||||||||",
      "| Ctrl-RightAlt-End triggers //e system test                                   |",
      "|                                                                              |",
      "| Pause/Break key pauses emulation                                             |",
      "|                                                                              |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                           (Press any key to exit)                            |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" };

    video_setpage( 0 );

    screen[ 6 ][ 68 ] = MOUSETEXT_UP;
    screen[ 7 ][ 67 ] = MOUSETEXT_LEFT;
    screen[ 7 ][ 69 ] = MOUSETEXT_RIGHT;
    screen[ 8 ][ 68 ] = MOUSETEXT_DOWN;

    screen[ 8 ][ 25 ] = MOUSETEXT_OPENAPPLE;
    screen[ 8 ][ 47 ] = MOUSETEXT_CLOSEDAPPLE;
    screen[ 11 ][ 14 ] = MOUSETEXT_OPENAPPLE;
    screen[ 12 ][ 15 ] = MOUSETEXT_CLOSEDAPPLE;

    c_interface_translate_screen(screen);
    c_interface_print_screen( screen );

    int ch = -1;
    while ((ch = c_mygetch(1)) == -1)
    {
    }

    c_interface_exit(ch);
}

/* -------------------------------------------------------------------------
    c_interface_begin()
   ------------------------------------------------------------------------- */

static void *interface_thread(void *current_key)
{
    pthread_mutex_lock(&interface_mutex);
#ifdef AUDIO_ENABLED
    SoundSystemPause();
#endif
    in_interface = true;

    switch ((__SWORD_TYPE)current_key) {
    case kF1:
        c_interface_select_diskette( 0 );
        break;

    case kF2:
        c_interface_select_diskette( 1 );
        break;

    case kPAUSE:
        while (c_mygetch(1) == -1)
        {
            struct timespec ts = { .tv_sec=0, .tv_nsec=33333333/*30Hz*/ };
            nanosleep(&ts, NULL);
        }
        break;

    case kF5:
        c_interface_keyboard_layout();
        break;

#ifdef DEBUGGER
    case kF7:
        c_interface_debugging();
        break;
#endif

    case kF8:
        c_interface_credits();
        break;

    case kF10:
        c_interface_parameters();
        break;

    default:
        break;
    }

#ifdef AUDIO_ENABLED
    SoundSystemUnpause();
#endif
    pthread_mutex_unlock(&interface_mutex);
    in_interface = false;

    return NULL;
}

void c_interface_begin(int current_key)
{
    pthread_t t = 0;
    pthread_create(&t, NULL, (void *)&interface_thread, (void *)((__SWORD_TYPE)current_key));
    pthread_detach(t);
}

#endif

