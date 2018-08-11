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

#if INTERFACE_TOUCH
// touch interface managed elsewhere
int64_t (*interface_onTouchEvent)(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords) = NULL;
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

void interface_plotMessage(uint8_t *fb, const interface_colorscheme_t cs, char *message, const uint8_t message_cols, const uint8_t message_rows) {
    _translate_screen_x_y(message, message_cols, message_rows);
    display_plotMessage(fb, cs, message, message_cols, message_rows);
}

// ----------------------------------------------------------------------------
// Desktop Legacy Menu Interface

#if INTERFACE_CLASSIC

static bool isShowing = false;

static char disk_path[PATH_MAX] = { 0 };

static void _interface_plotMessageCentered(int fb_cols, int fb_rows, interface_colorscheme_t cs, char *message, const int message_cols, const int message_rows) {
    _translate_screen_x_y(message, message_cols, message_rows);
    int col = (fb_cols - message_cols) >> 1;
    int row = (fb_rows - message_rows) >> 1;
    int fb_pix_width = (fb_cols*FONT80_WIDTH_PIXELS)+INTERPOLATED_PIXEL_ADJUSTMENT;
    assert(fb_pix_width == SCANWIDTH);
    int row_max = row + message_rows;
    for (int idx=0; row<row_max; row++, idx+=message_cols+1) {
        video_getCurrentBackend()->plotLine(col, row, cs, &message[idx]);
    }
}

static struct stat statbuf = { 0 };
static int altdrive = 0;

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
    video_getCurrentBackend()->plotLine(/*col:*/x, /*row:*/y, cs, s);
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
    _interface_plotMessageCentered(INTERFACE_SCREEN_X, TEXT_ROWS, RED_ON_BLACK, submenu, message_cols, message_rows);
}

/* ------------------------------------------------------------------------- */

#warning TODO FIXME : file selection and extension management should be made generic (merge similar code from disk.[hc] and possible Mac/iOS target) ...
static int disk_select(const struct dirent *e) {
    char cmp[PATH_MAX] = { 0 };

    const size_t pathSepSize = strlen(PATH_SEPARATOR);
    const size_t diskNameSize = MIN(PATH_MAX, strlen(disk_path)) + pathSepSize + MIN(PATH_MAX, strlen(e->d_name));

    if (diskNameSize >= PATH_MAX) {
        LOG("OOPS computed path size >= PATH_MAX!");
        return 0;
    }

    strncpy(cmp, disk_path, PATH_MAX-1);
    strncat(cmp, PATH_SEPARATOR, pathSepSize);
    strncat(cmp, e->d_name, PATH_MAX-1);

    /* don't show disk in alternate drive */
    if (disk6.disk[altdrive].file_name && !strcmp(cmp, disk6.disk[altdrive].file_name)) {
        return 0;
    }

    /* show directories except '.' and '..' at toplevel. */
    stat(cmp, &statbuf);
    if (S_ISDIR(statbuf.st_mode) && strcmp(".", e->d_name) &&
        !(!strcmp("..", e->d_name) && !strcmp(disk_path, PATH_SEPARATOR)))
    {
        return 1;
    }

    const char *p = e->d_name;
    size_t len = strlen(p);

    if (len < 4) {
        return 0;
    }

    if (!strncmp(p + len - 3, ".gz", 3)) {
        len -= 3;
    }

    if (len < 4) {
        return 0;
    }

    if (!strncmp(p + len - 3, ".do", 3)) {
        return 1;
    }

    if (!strncmp(p + len - 3, ".po", 3)) {
        return 1;
    }

    if (len < 5) {
        return 0;
    }

    if (!strncmp(p + len - 4, ".dsk", 4)) {
        return 1;
    }

    if (!strncmp(p + len - 4, ".nib", 4)) {
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
        video_setDirty(FB_DIRTY_FLAG);
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
    const char *err_str = disk6_eject(drive);
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

    c_interface_translate_screen( screen );

    do {
        nextdir = false;
        c_interface_print_screen( screen );

        altdrive = (drive == 0) ? 1 : 0;
        if (!strcmp("", disk_path))
        {
            sprintf(disk_path, PATH_SEPARATOR);
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
            snprintf(disk_path, PATH_MAX, "%s", HOMEDIR);
            nextdir = true;
            continue;
        }

        if (curpos >= entries)
        {
            curpos = entries - 1;
        }

        char temp[PATH_MAX] = { 0 };
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
                    if (disk6.disk[drive].file_name && !strcmp(temp, disk6.disk[drive].file_name))
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

                    if (is_gz(temp))
                    {
                        cut_gz(temp);
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

                submenu[ 2 ][ 14 ] = (char)MOUSETEXT_UP;
                submenu[ 2 ][ 20 ] = (char)MOUSETEXT_DOWN;
                c_interface_print_submenu_centered(submenu[0], DISKHELP_SUBMENU_W, DISKHELP_SUBMENU_H);
                while ((ch = c_mygetch(1)) == -1)
                {
                }
                c_interface_print_screen( screen );
            }
            else if ((ch == 13) || (toupper(ch) == 'W'))
            {
                size_t pathlen = strlen(disk_path);
                if (pathlen && disk_path[pathlen-1] == '/') {
                    disk_path[pathlen-1] = '\0';
                }

                snprintf(temp, PATH_MAX, "%s/%s",
                         disk_path, namelist[ curpos ]->d_name );
                size_t len = strlen(disk_path);

                /* handle disk currently in the drive */
                if (disk6.disk[drive].file_name && !strcmp(temp, disk6.disk[drive].file_name))
                {
                    /* reopen disk, forcing write enabled */
                    if (toupper(ch) == 'W')
                    {
                        int fd = -1;
                        TEMP_FAILURE_RETRY(fd = open(temp, O_RDWR));
                        const char *err_str = disk6_insert(fd, drive, temp, /*readonly:*/0);
                        if (fd > 0) {
                            TEMP_FAILURE_RETRY(close(fd));
                        }
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
                            if (video_getAnimationDriver()->animation_showDiskChosen) {
                                video_getAnimationDriver()->animation_showDiskChosen(drive);
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

                    if (len && (disk_path[len-1]) == '/')
                    {
                        disk_path[--len] = '\0';
                    }

                    if (!strcmp("..", namelist[curpos]->d_name))
                    {
                        while (disk_path[len] != '/')
                        {
                            if (!len) {
                                break;
                            }
                            disk_path[len] = '\0';
                            --len;
                        }
                    }
                    else if (strcmp(".", namelist[curpos]->d_name))
                    {
                        size_t ent_len = strlen(namelist[curpos]->d_name)+1+1; // +1 for dir sep +1 for \0
                        snprintf(disk_path + len, MIN(ent_len, PATH_MAX-(len+ent_len)), "/%s", namelist[curpos]->d_name);
                    }

                    prefs_setStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, disk_path);

                    nextdir = true;
                    break;
                }

                _eject_disk(drive);
                c_interface_print_screen( screen );

                int fd = -1;
                TEMP_FAILURE_RETRY(fd = open(temp, O_RDWR));
                const char *err_str = disk6_insert(fd, drive, temp, /*readonly:*/(toupper(ch) != 'W'));
                if (fd > 0) {
                    TEMP_FAILURE_RETRY(close(fd));
                }
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
                    if (video_getAnimationDriver()->animation_showDiskChosen) {
                        video_getAnimationDriver()->animation_showDiskChosen(drive);
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
    long val = 0;

    prefs_parseLongValue(PREF_DOMAIN_VIDEO, PREF_COLOR_MODE, &val, /*base:*/10);
    color_mode_t color_mode = (color_mode_t)val;

    prefs_parseLongValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_MODE, &val, /*base:*/10);
    /* extern */joy_mode = (joystick_mode_t)val;

    prefs_parseLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, &val, /*base:*/10);
    long speaker_volume = val;

    /* reset the x position, so we don't lose our cursor if path changes */
    cur_x = 0;

    screen[ 2 ][ 33 ] = (char)MOUSETEXT_OPENAPPLE;
    screen[ 2 ][ 46 ] = (char)MOUSETEXT_CLOSEDAPPLE;

    c_interface_translate_screen( screen );
    c_interface_print_screen( screen );

#define TEMPSIZE 1024
    char temp[TEMPSIZE] = { 0 };
    bool shutdown = false;
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
                if (cpu_scale_factor > CPU_SCALE_FASTEST_PIVOT) {
                    snprintf(temp, TEMPSIZE, "Fastest");
                } else {
                    snprintf(temp, TEMPSIZE, "%d%%", (int)roundf((cpu_scale_factor * 100.0)));
                }
                break;

            case OPT_ALTCPU:
                if (cpu_altscale_factor > CPU_SCALE_FASTEST_PIVOT) {
                    snprintf(temp, TEMPSIZE, "Fastest");
                } else {
                    snprintf(temp, TEMPSIZE, "%d%%", (int)roundf((cpu_altscale_factor * 100.0)));
                }
                break;

            case OPT_PATH:
                strncpy(temp, disk_path + cur_pos, INTERFACE_PATH_MAX);
                temp[INTERFACE_PATH_MAX] = '\0';
                break;

            case OPT_COLOR:
                sprintf(temp, "%s", (color_mode == COLOR_MODE_COLOR) ? "Color       " :
                        (color_mode == COLOR_MODE_INTERP) ? "Interpolated" : "Black/White ");
                break;

            case OPT_JOYSTICK:
                snprintf(temp, TEMPSIZE, "%s", (joy_mode == JOY_KPAD) ? "Emulated on Keypad" : "PC Joystick      ");
                break;

            case OPT_VOLUME:
                if (speaker_volume == 0)
                {
                    snprintf(temp, TEMPSIZE, "%s", "Off       ");
                }
                else
                {
                    snprintf(temp, TEMPSIZE, "%ld", speaker_volume);
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
                            video_getCurrentBackend()->plotChar(/*col:*/INTERFACE_PATH_MIN + j, /*row:*/5+i, GREEN_ON_BLACK, ' ' );
                            j++;
                            break;
                        }
                        else
                        {
                            video_getCurrentBackend()->plotChar(/*col:*/INTERFACE_PATH_MIN + j, /*row:*/5+i, /*cs:*/GREEN_ON_BLACK, temp[j]);
                        }
                    }
                    else
                    {
                        if (temp[ j ] == '\0')
                        {
                            video_getCurrentBackend()->plotChar(/*col:*/INTERFACE_PATH_MIN + j, /*row:*/5+i, (option == OPT_PATH ? GREEN_ON_BLUE : GREEN_ON_BLACK), ' ' );
                            j++;
                            break;
                        }
                        else
                        {
                            video_getCurrentBackend()->plotChar(/*col:*/INTERFACE_PATH_MIN + j, /*row:*/5+i, (option == OPT_PATH ? GREEN_ON_BLUE : GREEN_ON_BLACK), temp[j]);
                        }

                    }
                }

                for (; j < INTERFACE_PATH_MAX; j++)
                {
                    video_getCurrentBackend()->plotChar(/*col:*/INTERFACE_PATH_MIN + j, /*row:*/5+i, GREEN_ON_BLACK, ' ');
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
                if (cpu_scale_factor > CPU_SCALE_FASTEST_PIVOT) {
                    cpu_scale_factor = CPU_SCALE_FASTEST_PIVOT;
                } else {
                    cpu_scale_factor = roundf((cpu_scale_factor - ((cpu_scale_factor <= 1.0) ? CPU_SCALE_STEP_DIV : CPU_SCALE_STEP)) * 100.f) / 100.f;
                    if (cpu_scale_factor < CPU_SCALE_SLOWEST) {
                        cpu_scale_factor = CPU_SCALE_SLOWEST;
                    }
                }
                prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, roundf(cpu_scale_factor * 100.f));
                break;

            case OPT_ALTCPU:
                if (cpu_altscale_factor > CPU_SCALE_FASTEST_PIVOT) {
                    cpu_altscale_factor = CPU_SCALE_FASTEST_PIVOT;
                } else {
                    cpu_altscale_factor = roundf((cpu_altscale_factor - ((cpu_altscale_factor <= 1.0) ? CPU_SCALE_STEP_DIV : CPU_SCALE_STEP)) * 100.f) / 100.f;
                    if (cpu_altscale_factor < CPU_SCALE_SLOWEST) {
                        cpu_altscale_factor = CPU_SCALE_SLOWEST;
                    }
                }
                prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, roundf(cpu_altscale_factor * 100.f));
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
                color_mode = (color_mode == 0) ? NUM_COLOROPTS-1 : color_mode-1;
                prefs_setLongValue(PREF_DOMAIN_VIDEO, PREF_COLOR_MODE, color_mode);
                break;

            case OPT_VOLUME:
                if (speaker_volume > 0)
                {
                    --speaker_volume;
                }
                prefs_setLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, speaker_volume);
                break;

            case OPT_CAPS:
                caps_lock = false;
                use_system_caps_lock = true;
                prefs_setBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, caps_lock);
                break;

            case OPT_JOYSTICK:
                joy_mode = (joy_mode == 0) ? NUM_JOYOPTS-1 : joy_mode-1;
                prefs_setLongValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_MODE, joy_mode);
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
                cpu_scale_factor = roundf((cpu_scale_factor + ((cpu_scale_factor < 1.0) ? CPU_SCALE_STEP_DIV : CPU_SCALE_STEP)) * 100.f) / 100.f;
                if (cpu_scale_factor > CPU_SCALE_FASTEST_PIVOT) {
                    cpu_scale_factor = CPU_SCALE_FASTEST;
                }
                prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, roundf(cpu_scale_factor * 100.f));
                break;

            case OPT_ALTCPU:
                cpu_altscale_factor = roundf((cpu_altscale_factor + ((cpu_altscale_factor < 1.0) ? CPU_SCALE_STEP_DIV : CPU_SCALE_STEP)) * 100.f) / 100.f;
                if (cpu_altscale_factor > CPU_SCALE_FASTEST_PIVOT) {
                    cpu_altscale_factor = CPU_SCALE_FASTEST;
                }
                prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, roundf(cpu_altscale_factor * 100.f));
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
                color_mode = (color_mode >= NUM_COLOROPTS-1) ? 0 : color_mode+1;
                prefs_setLongValue(PREF_DOMAIN_VIDEO, PREF_COLOR_MODE, color_mode);
                break;

            case OPT_VOLUME:
                speaker_volume++;
                if (speaker_volume > 10)
                {
                    speaker_volume = 10;
                }
                prefs_setLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, speaker_volume);
                break;

            case OPT_CAPS:
                caps_lock = true;
                use_system_caps_lock = false;
                prefs_setBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, caps_lock);
                break;

            case OPT_JOYSTICK:
                joy_mode = (joy_mode == NUM_JOYOPTS-1) ? 0 : joy_mode+1;
                prefs_setLongValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_MODE, joy_mode);
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
            display_reset();
            vm_reinitializeAudio();
            c_joystick_reset();
#if !TESTING
            prefs_save();
#endif
            c_interface_exit(ch);
            break;
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
            submenu[ 1 ][ 14 ] = (char)MOUSETEXT_UP;
            submenu[ 1 ][ 20 ] = (char)MOUSETEXT_DOWN;
            submenu[ 3 ][ 14 ] = (char)MOUSETEXT_LEFT;
            submenu[ 3 ][ 20 ] = (char)MOUSETEXT_RIGHT;

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

                strncpy(temp, disk_path, TEMPSIZE-1);
                for (i = strlen(temp); i >= cur_pos + cur_x; i--)
                {
                    temp[ i + 1 ] = temp[ i ];
                }

                temp[ cur_pos + cur_x ] = ch;
                strncpy(disk_path, temp, PATH_MAX-1);
                if (cur_x < INTERFACE_PATH_MAX-1)
                {
                    cur_x++;
                }
                else if (disk_path[cur_pos + cur_x] != '\0')
                {
                    cur_pos++;
                }

                prefs_setStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, disk_path);
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

                prefs_setStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, disk_path);
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
#if !TESTING
                        prefs_save();
#endif
                        disk6_eject(0);
                        c_interface_print_screen( screen );
                        disk6_eject(1);
                        c_interface_print_screen( screen );
#ifdef __linux__
                        LOG("Back to Linux, w00t!\n");
#endif
                        shutdown = true;
                        c_interface_exit(ch);
                        break;
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
                        break;
                    }
                }

                c_interface_print_screen( screen );
            }
        }
    }

    if (shutdown) {
        video_shutdown(); // soft quit video_main_loop()
    } else {
        prefs_sync(NULL);
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

    screen[ 2 ][ 33 ] = (char)MOUSETEXT_OPENAPPLE;
    screen[ 2 ][ 46 ] = (char)MOUSETEXT_CLOSEDAPPLE;

    screen[ 22 ][ 18 ] = (char)MOUSETEXT_UP;
    screen[ 22 ][ 20 ] = (char)MOUSETEXT_DOWN;

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
        "License version 3 or later (your choice) as published by the Free Software  ",
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
            credits[ 3 ][ 20 ] = (char)(MOUSETEXT_BEGIN + mt_idx);
            credits[ 3 ][ 55 ] = (char)(MOUSETEXT_BEGIN + ((mt_idx+1) % 2));
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

    screen[ 6 ][ 68 ] = (char)MOUSETEXT_UP;
    screen[ 7 ][ 67 ] = (char)MOUSETEXT_LEFT;
    screen[ 7 ][ 69 ] = (char)MOUSETEXT_RIGHT;
    screen[ 8 ][ 68 ] = (char)MOUSETEXT_DOWN;

    screen[ 8 ][ 25 ] = (char)MOUSETEXT_OPENAPPLE;
    screen[ 8 ][ 47 ] = (char)MOUSETEXT_CLOSEDAPPLE;
    screen[ 11 ][ 14 ] = (char)MOUSETEXT_OPENAPPLE;
    screen[ 12 ][ 15 ] = (char)MOUSETEXT_CLOSEDAPPLE;

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

static pthread_mutex_t classic_interface_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t interface_thread_id = 0;

typedef struct interface_key_s {
    int current_key;
} interface_key_s;

static void *interface_thread(void *data)
{
    interface_thread_id = pthread_self();

    cpu_pause();

    char *path = NULL;
    prefs_copyStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, &path);
    if (!path) {
        ASPRINTF(&path, "%s", HOMEDIR);
    }
    strncpy(disk_path, path, PATH_MAX-1);
    disk_path[PATH_MAX-1] = '\0';
    FREE(path);

    interface_key_s *interface_key = (interface_key_s *)data;

    switch (interface_key->current_key) {
    case kF1:
        isShowing = true;
        c_interface_select_diskette( 0 );
        break;

    case kF2:
        isShowing = true;
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
        isShowing = true;
        c_interface_keyboard_layout();
        break;

    case kF7:
        isShowing = true;
        c_interface_debugging();
        break;

    case kF8:
        isShowing = true;
        c_interface_credits();
        break;

    case kF10:
        isShowing = true;
        c_interface_parameters();
        break;

    default:
        break;
    }

    cpu_resume();

    interface_thread_id = 0;
    isShowing = false;
    pthread_mutex_unlock(&classic_interface_lock);
    return NULL;
}

void c_interface_begin(int current_key)
{
    static interface_key_s interface_key = { 0 };

    if (interface_thread_id) {
        return;
    }
    pthread_mutex_lock(&classic_interface_lock);
    interface_thread_id=1; // interface thread starting ...
    interface_key.current_key = current_key;
    pthread_create(&interface_thread_id, NULL, (void *)&interface_thread, &interface_key);
    pthread_detach(interface_thread_id);
}

bool interface_isShowing(void) {
    return isShowing;
}

#endif

