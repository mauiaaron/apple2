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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <dirent.h>
#include <termios.h>
#include <errno.h>
#include <time.h>

#include "interface.h"
#include "timing.h"
#include "keys.h"
#include "disk.h"
#include "misc.h"
#include "video.h"
#include "cpu.h"
#include "prefs.h"

static struct stat statbuf;
static int altdrive;

/*#define undoc_supported 1*/
/*#else*/
/*#define undoc_supported 0*/

static void pad_string(char *s, char c, int len) {
    char *p;

    for (p = s; ((*p != '\0') && (p-s < len-1)); p++)
    {
    }

    while (p-s < len-1)
    {
        *p++ = c;
    }

    *p = '\0';
}

/* in keys.c */
//extern void c_mouse_close();

/* from joystick.c */
#ifdef PC_JOYSTICK
extern int c_open_joystick();
extern void c_calculate_joystick_parms();
extern void c_close_joystick();
extern void c_calibrate_joystick();
extern long js_timelimit;
#endif


/* -------------------------------------------------------------------------
    c_load_interface_font()
   ------------------------------------------------------------------------- */

void c_load_interface_font()
{
    /* Only codes 0x20 -- 0x8A are actually used. But I feel safer
     * explicitly initializing all of them.
     */

    video_loadfont_int(0x00,0x40,ucase_glyphs);
    video_loadfont_int(0x40,0x20,ucase_glyphs);
    video_loadfont_int(0x60,0x20,lcase_glyphs);
    video_loadfont_int(0x80,0x40,ucase_glyphs);
    video_loadfont_int(0xC0,0x20,ucase_glyphs);
    video_loadfont_int(0xE0,0x20,lcase_glyphs);

    video_loadfont_int(0x80,11,interface_glyphs);
}

/* -------------------------------------------------------------------------
    c_interface_print()
   ------------------------------------------------------------------------- */
void c_interface_print( int x, int y, int cs, const char *s )
{
    for (; *s; x++, s++)
    {
        video_plotchar( x, y, cs, *s );
    }

}

/* -------------------------------------------------------------------------
    c_interface_print_screen()
   ------------------------------------------------------------------------- */
void c_interface_print_screen( char screen[24][INTERFACE_SCREEN_X+1] )
{
    for (int y = 0; y < 24; y++)
    {
        c_interface_print( 0, y, 2, screen[ y ] );
    }
}

/* -------------------------------------------------------------------------
    c_interface_redo_bottom()
   ------------------------------------------------------------------------- */

void c_interface_redo_bottom() {

    c_interface_print( 1, 21, 2,
                       " Use arrow keys (or Return) to modify "
                       );
    c_interface_print( 1, 22, 2,
                       " parameters. (Press ESC to exit menu) "
                       );
}

/* -------------------------------------------------------------------------
    c_interface_redo_diskette_bottom()
   ------------------------------------------------------------------------- */

void c_interface_redo_diskette_bottom() {
    c_interface_print( 1, 21, 2,
                       " Move: Arrows, PGUP, PGDN, HOME, END. "
                       );
    c_interface_print( 1, 22, 2,
                       " Return and 'w' select, ESC cancels.  "
                       );
}

/* -------------------------------------------------------------------------
    c_interface_translate_screen()
   ------------------------------------------------------------------------- */

#define IsGraphic(c) ((c) == '|' || (((unsigned char)c) >= 0x80 && ((unsigned char)c) <= 0x8A))
#define IsInside(x,y) ((x) >= 0 && (x) <= xlen-1 && (y) >= 0 && (y) <= ylen-1)

static void _convert_screen_graphics( char *screen, int x, int y, int xlen, int ylen )
{
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
    for (; k >= 0; k--)
    {
        found_glyph = true;

        for (int yy = y - 1; found_glyph && yy <= y + 1; yy++)
        {
            int idx = yy*(xlen+1);
            for (int xx = x - 1; xx <= x + 1; xx++)
            {
                char map_ch = map[k][ yy - y + 1 ][ xx - x + 1 ];

                if (IsInside(xx, yy))
                {
                    char c = *(screen + idx + xx);
                    if (!IsGraphic( c ) && (map_ch == '|'))
                    {
                        found_glyph = false;
                        break;
                    }
                    else if (IsGraphic( c ) && (map_ch == '.'))
                    {
                        found_glyph = false;
                        break;
                    }
                }
                else if (map_ch == '|')
                {
                    found_glyph = false;
                    break;
                }
            }
            idx += xlen+1;
        }

        if (found_glyph)
        {
            break;
        }
    }

    if (found_glyph)
    {
        //screen[ y ][ x ] = 0x80 + k;
        *(screen + y*(xlen+1) + x) = 0x80 + k;
    }
}

void c_interface_translate_screen( char screen[24][INTERFACE_SCREEN_X+1] )
{
    for (int y = 0; y < 24; y++)
    {
        for (int x = 0; x < INTERFACE_SCREEN_X; x++)
        {
            if (screen[ y ][ x ] == '|')
            {
                _convert_screen_graphics(screen[0], x, y, INTERFACE_SCREEN_X, 24);
            }
        }
    }
}

/* -------------------------------------------------------------------------
    c_interface_translate_menu()
   ------------------------------------------------------------------------- */
void c_interface_translate_menu( char *submenu, int xlen, int ylen )
{
    for (int idx=0, y=0; y < ylen; y++, idx+=xlen+1)
    {
        for (int x = 0; x < xlen; x++)
        {
            if (*(submenu + idx + x) == '|')
            {
                _convert_screen_graphics(submenu, x, y, xlen, ylen);
            }
        }
    }
}

/* -------------------------------------------------------------------------
    c_interface_print_submenu_centered()
   ------------------------------------------------------------------------- */
void c_interface_print_submenu_centered( char *submenu, int xlen, int ylen )
{
    c_interface_translate_menu(submenu, xlen, ylen);
    int x = (INTERFACE_SCREEN_X - xlen) >> 1;
    int y = (24 - ylen) >> 1;

    int ymax = y+ylen;
    for (int idx=0; y < ymax; y++, idx+=xlen+1)
    {
        c_interface_print( x, y, 2, &submenu[ idx ] );
    }
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

static void c_interface_cut_gz(char *name)
{
    char *p = name + strlen(name) - 1;

    p--;
    p--;
    *p = '\0';
}

#define GZ_EXT ".gz"
#define GZ_EXT_LEN 3
#define DISK_EXT ".dsk"
#define DISK_EXT2 ".nib"
#define DISK_EXT_LEN 4

/* does name end with ".gz" ? */
static int c_interface_is_gz(const char *name)
{
    size_t len = strlen( name );

    if (len > GZ_EXT_LEN)    /* shouldn't have a file called ".gz"... */
    {   /*name += len - GZ_EXT_LEN;*/
        return ((strcmp(name+len-GZ_EXT_LEN, GZ_EXT) == 0) ? 1 : 0);
    }

    return 0;
}


/* does name end with ".nib{.gz}" */
static int c_interface_is_nibblized(const char *name)
{
    size_t len = strlen( name );

    if (c_interface_is_gz(name))
    {
        len -= GZ_EXT_LEN;
    }

    if (!strncmp(name + len - DISK_EXT_LEN, DISK_EXT2, DISK_EXT_LEN))
    {
        return 1;
    }

    return 0;
}


int c_interface_disk_select(const struct dirent *e)
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

    if (len > GZ_EXT_LEN && (!strcmp(p + len - GZ_EXT_LEN, GZ_EXT)))
    {
        len -= GZ_EXT_LEN;
    }

    /* true if .dsk or .nib extension */
    if (!strncmp(p + len - DISK_EXT_LEN, DISK_EXT, DISK_EXT_LEN))
    {
        return 1;
    }

    if (!strncmp(p + len - DISK_EXT_LEN, DISK_EXT2, DISK_EXT_LEN))
    {
        return 1;
    }

    return 0;
}

/* -------------------------------------------------------------------------
    c_interface_exit()
   ------------------------------------------------------------------------- */

void c_interface_exit()
{
    video_setpage(!!(softswitches & SS_SCREEN));
    video_redraw();
}


static void c_usleep() {
    // 1.5 secs
    struct timespec delay;
    delay.tv_sec=1;
    delay.tv_nsec=500000000;
    nanosleep(&delay, NULL);
}

/* -------------------------------------------------------------------------
    c_interface_select_diskette()
   ------------------------------------------------------------------------- */

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

    /*
     * TODO : HELP sub-menu showing these and ( '/' '?' to search for regex ...)
      "| Move: Arrows, PGUP, PGDN, HOME, END. |",
      "| Return and 'w' select, ESC cancels.  |",
      */

    struct dirent       **namelist;
    int i, entries;
    pid_t pid;
    static int curpos=0;
    int ch;

    screen[ 1 ][ DRIVE_X ] = (drive == 0) ? 'A' : 'B';

    video_setpage( 0 );

    c_interface_translate_screen( screen );

NEXTDIR:
    c_interface_print_screen( screen );

    altdrive = (drive == 0) ? 1 : 0;
    if (!strcmp("", disk_path))
    {
        sprintf(disk_path, "/");
    }

    /* set to users privilege level for directory access */
    entries = scandir(disk_path, &namelist,
                      c_interface_disk_select, alphasort);

    if (entries <= 0)
    {
        /* 1/17/97 NOTE: scandir (libc5.3.12) seems to freak on broken
           symlinks.  so you won't be able to read from some
           legitimate directories... */
        c_interface_print( 6, 11, 0, "Problem reading directory!" );
        sprintf(disk_path, "/");
        c_usleep();
        c_interface_exit();
        return;
    }

    if (curpos >= entries)
    {
        curpos = entries - 1;
    }

    for (;;)
    {
        for (i = 0; i < 18; i++)
        {
            int ent_no = curpos - 8 + i, slen;
            int in_drive = 0;

            strcpy( temp, " " );
            if (ent_no >= 0 && ent_no < entries)
            {
                snprintf(temp, TEMPSIZE, "%s/%s",
                         disk_path, namelist[ent_no]->d_name);
                if (!strcmp(temp, disk6.disk[drive].file_name))
                {
                    in_drive = 1;
                }

                stat(temp, &statbuf);
                if (S_ISDIR(statbuf.st_mode))
                {
                    snprintf(temp, TEMPSIZE, " %s/",
                             namelist[ ent_no ]->d_name );
                }
                else
                {
                    snprintf(temp, TEMPSIZE, " %s",
                             namelist[ ent_no ]->d_name );
                }

                if (c_interface_cut_name(temp))
                {
                    strncat(temp, " <gz>", TEMPSIZE-1);
                }
                /* write protected disk in drive? */
                else if ((in_drive) && (disk6.disk[drive].protected))
                {
                    strncat(temp, (drive == 0) ? " <r1>" : " <r2>", TEMPSIZE-1);
                }
                else if (in_drive)
                {
                    strncat(temp, (drive == 0) ? " <rw1>" : " <rw2>", TEMPSIZE-1);
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

        if (ch == kUP)          /* Arrow up */
        {
            if (curpos > 0)
            {
                curpos--;
            }
            else
            {
            }
        }
        else if (ch == kDOWN)          /* Arrow down */
        {
            if (curpos < entries - 1)
            {
                curpos++;
            }
            else
            {
            }
        }
        else if (ch == kPGDN)          /* Page down */
        {
            curpos += 16;
            if (curpos > entries - 1)
            {
                curpos = entries - 1;
            }
        }
        else if (ch == kPGUP)            /* Page up */
        {
            curpos -= 16;
            if (curpos < 0)
            {
                curpos = 0;
            }
        }
        else if (ch == kHOME)            /* Home */
        {
            curpos = 0;
        }
        else if (ch == kEND)          /* End */
        {
            curpos = entries - 1;
        }
        else if (ch == kESC)          /* ESC */
        {
            for (i = 0; i < entries; i++)
            {
                free(namelist[ i ]);
            }

            free(namelist);
            c_interface_exit();
            return;
        }
        else if (ch == '?')
        {
#define DISKHELP_SUBMENU_H 17
#define DISKHELP_SUBMENU_W 40
            char submenu[DISKHELP_SUBMENU_H][DISKHELP_SUBMENU_W+1] =
            //1.  5.  10.  15.  20.  25.  30.  35.  40.
            { "||||||||||||||||||||||||||||||||||||||||",
              "|                                      |",
              "|      Disk : Up/Down arrows           |",
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
              "|    Search : Press '/' key to search  |",
              "|                                      |",
              "||||||||||||||||||||||||||||||||||||||||" };
            c_interface_print_submenu_centered(submenu[0], DISKHELP_SUBMENU_W, DISKHELP_SUBMENU_H);

            while ((ch = c_mygetch(1)) == -1)
            {
            }

            c_interface_print_screen( screen );
        }
        else if ((ch == 13) || (toupper(ch) == 'W'))            /* Return */
        {
#define PERM_SUBMENU_H 5
#define PERM_SUBMENU_W 40
            char protmenu[PERM_SUBMENU_H][PERM_SUBMENU_W+1] =
                //1.  5.  10.  15.  20.  25.  30.  35.  40.
            { "||||||||||||||||||||||||||||||||||||||||",
              "|                                      |",
              "|   Disk is read and write protected   |",
              "|                                      |",
              "||||||||||||||||||||||||||||||||||||||||" };

            int len, cmpr = 0;

            snprintf(temp, TEMPSIZE, "%s/%s",
                     disk_path, namelist[ curpos ]->d_name );
            len = strlen(disk_path);

            /* handle disk currently in the drive */
            if (!strcmp(temp, disk6.disk[drive].file_name))
            {
                /* reopen disk, forcing write enabled */
                if (toupper(ch) == 'W')
                {
                    if (c_new_diskette_6(
                            drive,
                            temp,
                            disk6.disk[drive].compressed,
                            disk6.disk[drive].nibblized, 0))
                    {
                        c_interface_print_submenu_centered(protmenu[0], PERM_SUBMENU_W, PERM_SUBMENU_H);
                        while ((ch = c_mygetch(1)) == -1)
                        {
                        }
                        c_interface_print_screen( screen );
                        continue;
                    }

                    for (i = 0; i < entries; i++)
                    {
                        free(namelist[ i ]);    /* clean up */
                    }

                    free(namelist);
                    c_interface_exit();         /* resume emulation */
                    return;
                }

                /* eject the disk and start over */
                c_eject_6(drive);
                for (i = 0; i < entries; i++)
                {
                    free(namelist[ i ]);        /* clean up */
                }

                free(namelist);
                goto NEXTDIR;                   /* I'm lazy */
            }

            /* read another directory */
            stat(temp, &statbuf);
            if (S_ISDIR(statbuf.st_mode))
            {
                if (toupper(ch) == 'W')
                {
                    continue;                    /* can't protect this */

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

                for (i = 0; i < entries; i++)
                {
                    free(namelist[ i ]);        /* clean up */
                }

                free(namelist);
                goto NEXTDIR;                   /* I'm lazy */
            }

            /* uncompress the gziped disk */
            if (c_interface_is_gz(temp))
            {
                if ((pid = fork()))     /* parent process */
                {
                    c_interface_print( 1, 21, 0,
                                       "            Uncompressing...          " );
                    c_interface_print( 1, 22, 0,
                                       "                                      " );
                    if (waitpid(pid, NULL, 0) == -1)
                    {
                        c_interface_print( 1, 21, 0,
                                           "          Problem gunzip'ing          " );
                        c_interface_print( 1, 22, 0,
                                           "                                      " );
                        c_usleep();
                        c_mygetch(1);
                        c_interface_redo_diskette_bottom();
                        continue;
                    }
                }
                else if (!pid)               /* child process */
                {   /* privileged mode - gzip in place */
                    if (execl("/bin/gzip", "/bin/gzip",
                              "-d", temp, NULL) == -1)
                    {
                        snprintf(temp, TEMPSIZE, "%s", sys_errlist[errno]);
                        perror("\tproblem");
                        c_interface_print( 1, 21, 0,
                                           "    Problem exec'ing /bin/gzip -d     " );
                        c_interface_print( 1, 22, 0, temp);
                        c_usleep();
                        exit(-1);
                    }
                }
                else
                {
                    snprintf(temp, TEMPSIZE, "%s", sys_errlist[errno]);
                    c_interface_print( 1, 21, 0,
                                       "            Cannot fork!              " );
                    c_interface_print( 1, 22, 0, temp);
                    c_usleep();
                    c_mygetch(1);
                    c_interface_redo_diskette_bottom();
                    continue;
                }

                c_interface_cut_gz( temp );
                cmpr = 1;
            }

            /* gzip the last disk */
            if (disk6.disk[drive].compressed)
            {
                /* gzip the last disk if it was compressed_6 */

                if ((pid = fork()))     /* parent process */
                {   /* privileged mode - gzip in place */
                    c_interface_print( 1, 21, 0,
                                       "      Compressing old diskette...     " );
                    c_interface_print( 1, 22, 0,
                                       "                                      " );
                    if (waitpid(pid, NULL, 0) == -1)
                    {
                        c_interface_print( 1, 21, 0,
                                           "           Problem gzip'ing           " );
                        c_interface_print( 1, 22, 0,
                                           "                                      " );
                        c_usleep();
                        c_mygetch(1);
                        c_interface_redo_diskette_bottom();
                        continue;
                    }
                }
                else if (!pid)               /* child process */
                {   /* privileged mode - gzip in place */
                    if (execl("/bin/gzip", "/bin/gzip",
                              disk6.disk[drive].file_name, NULL) == -1)
                    {
                        c_interface_print( 1, 21, 0,
                                           "      Problem exec'ing /bin/gzip      " );
                        c_interface_print( 1, 22, 0, temp);
                        c_usleep();
                        exit(-1);
                    }
                }
                else
                {
                    snprintf(temp, TEMPSIZE, "%s", sys_errlist[errno]);
                    c_interface_print( 1, 21, 0,
                                       "            Cannot fork!              " );
                    c_interface_print( 1, 22, 0, temp);
                    c_usleep();
                    c_mygetch(1);
                    c_interface_redo_diskette_bottom();
                    continue;
                }
            }

            /* now try to change the disk */
            if (c_new_diskette_6(
                    drive, temp, cmpr, c_interface_is_nibblized(temp),
                    (toupper(ch) != 'W')))
            {
                c_interface_print_submenu_centered(protmenu[0], PERM_SUBMENU_W, PERM_SUBMENU_H);
                while ((ch = c_mygetch(1)) == -1)
                {
                }
                c_interface_print_screen( screen );
                continue;
            }

            for (i = 0; i < entries; i++)
            {
                free(namelist[ i ]);    /* clean up */
            }

            free(namelist);
            c_interface_exit();         /* resume emulation */
            return;
        }
    }
}

/* -------------------------------------------------------------------------
    c_interface_parameters()
   ------------------------------------------------------------------------- */

typedef enum interface_enum_t {
    OPT_CPU = 0,
    OPT_ALTCPU,
    OPT_PATH,
    OPT_MODE,
    OPT_COLOR,
    OPT_SOUND,
    OPT_JOYSTICK,
    OPT_CALIBRATE, // (NOP)
    OPT_JS_RANGE,
    OPT_ORIGIN_X,
    OPT_ORIGIN_Y,
    OPT_JS_SENSE,
    OPT_JS_SAMPLE,
    OPT_SAVE,
    OPT_QUIT,

    NUM_OPTIONS
} interface_enum_t;

static const char *options[] =
{
    " CPU%     : ",                   /* 0 */
    " ALT CPU% : ",                   /* 0 */
    " Path     : ",
    " Mode     : ",
    " Color    : ",
    " Sound    : ",
    " Joystick : ",                   /* 5 */
    " Calibrate  ",
    " JS Range : ",
    " Origin X : ",
    " Origin Y : ",
    " JS Sens. : ",                   /* 10 */
    " JS Sample: ",
    " Save Prefs ",
    " Quit       "
};

#define INTERFACE_PATH_MAX 65
#define INTERFACE_PATH_MIN 14

void c_interface_parameters()
{
    char screen[24][INTERFACE_SCREEN_X+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.  45.  50.  55.  60.  65.  70.  75.  80.",
    { "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                                                                              |",
      "|                                  Apple //ix                                  |",
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
      "| F1 F2: Slot6 Disk Drive A, Drive B                   F4 : Un/Pause Emulation |",
      "| F5   : Show Keyboard Layout                          F7 :   6502 VM Debugger |",
      "| F9   : Toggle Between CPU% / ALT CPU% Speeds                                 |",
      "| F10  : Show This Menu                                         ESC exits menu |",
      "|                                                                              |",
      "|                         For interface help press '?'                         |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" };


    int i;
    int ch;
    static interface_enum_t option = OPT_CPU;
    static int cur_y = 0, cur_off = 0, cur_x = 0, cur_pos = 0;
    int current_mode = apple_mode;

    /* reset the x position, so we don't lose our cursor if path changes */
    cur_x = 0;
    video_setpage( 0 );

    c_interface_translate_screen( screen );
    c_interface_print_screen( screen );

    for (;;)
    {
        for (i = 0; i < 9; i++)
        {
            cur_off = option - 8;
            if (cur_off < 0)
            {
                cur_off = 0;
            }

            c_interface_print( 1, 5 + i, cur_y == i, options[ i + cur_off ] );

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

            case OPT_MODE:
                sprintf(temp, "%s", (apple_mode == 0) ? "][+             " :
                        (apple_mode == 1) ? "][+ undocumented" :
                        "//e             ");
                break;

            case OPT_COLOR:
                sprintf(temp, "%s", (color_mode == 0) ? "Black/White " :
                        (color_mode == 1) ? "Lazy Color  " :
                        (color_mode == 2) ? "Color       " :
                        (color_mode == 3) ? "Lazy Interp." :
                        "Interpolated");
                break;

            case OPT_SOUND:
                sprintf(temp, "%s", (sound_mode == 0) ? "Off       " :
                        "PC Speaker");
                break;

            case OPT_JOYSTICK:
                sprintf(temp, "%s", (joy_mode == JOY_KYBD)    ? "Linear     " :
                        (joy_mode == JOY_DIGITAL) ? "Digital    " :
                        (joy_mode == JOY_PCJOY)   ? "PC Joystick" :
                        "Off        ");
                break;

            case OPT_CALIBRATE:
                strcpy( temp, "" );
                break;

            case OPT_JS_RANGE:
                sprintf(temp, "%02x", joy_range);
                break;

            case OPT_ORIGIN_X:
                sprintf(temp, "%02x", joy_center_x);
                break;

            case OPT_ORIGIN_Y:
                sprintf(temp, "%02x", joy_center_y);
                break;

            case OPT_JS_SENSE:
                sprintf(temp, "%03d%%", joy_step );
                break;

            case OPT_JS_SAMPLE:
#ifdef PC_JOYSTICK
                sprintf(temp, "%ld", js_timelimit);
#else
                sprintf(temp, "%s", "");
#endif
                break;

            case OPT_SAVE:
                strcpy( temp, "" );
                break;

            case OPT_QUIT:
                strcpy( temp, "" );
                break;

            default:
                break;
            }

            pad_string(temp, ' ', INTERFACE_PATH_MAX+1);
            if (i+cur_off != 2)
            {
                c_interface_print(INTERFACE_PATH_MIN, 5 + i, 0, temp);
            }
            else
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
            if (option > 8)
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
                cur_y = 8;              /* wrap to last y position */
            }
        }
        else if (ch == kDOWN)                  /* Arrow down */
        {
            if (cur_y < 8)
            {
                option++;               /* inc option */
                cur_y++;                /* and inc y position */
            }
            else if (option < NUM_OPTIONS-1)
            {
                option++;               /* only inc option */
            }
            else
            {
                cur_y = option = 0;     /* wrap both to first */
            }
        }
        else if ((ch == kLEFT) && (!is_backspace()))           /* Arrow left */
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

            case OPT_MODE:
                apple_mode--;
                if (apple_mode < 0)
                {
                    apple_mode = 2;
                }
                break;

            case OPT_COLOR:
                if (color_mode == 0)
                {
                    color_mode = 4;
                }
                else
                {
                    --color_mode;
                }
                break;

            case OPT_SOUND:
                if (sound_mode == 0)
                {
                    sound_mode = 1;
                }
                else
                {
                    --sound_mode;
                }
                break;

            case OPT_JOYSTICK:
#ifdef PC_JOYSTICK
                if (joy_mode == 0)
                {
                    joy_mode = 3;
                }
#else
                if (joy_mode == 0)
                {
                    joy_mode = 2;
                }
#endif
                else
                {
                    --joy_mode;
                }
                break;

            case OPT_CALIBRATE:
                break;

            case OPT_JS_RANGE:
                if (joy_range > 10)
                {
                    --joy_range;
                    joy_center_x = joy_range/2;
                    joy_center_y = joy_range/2;
                }
                break;

            case OPT_ORIGIN_X:
                if (joy_center_x > 0)
                {
                    joy_center_x--;
                }
                break;

            case OPT_ORIGIN_Y:
                if (joy_center_y > 0)
                {
                    joy_center_y--;
                }
                break;

            case OPT_JS_SENSE:
                if (joy_step > 1)
                {
                    joy_step--;
                }
                break;

            case OPT_JS_SAMPLE:
#ifdef PC_JOYSTICK
                if (js_timelimit > 2)
                {
                    --js_timelimit;
                }
#endif
                break;

            case OPT_SAVE:
            case OPT_QUIT:
                break;

            default:
                break;
            }
        }
        else if (ch == kRIGHT)                         /* Arrow right */
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

            case OPT_MODE:
                apple_mode++;
                if (apple_mode > 2)
                {
                    apple_mode = 0;
                }
                break;

            case OPT_COLOR:
                color_mode++;
                if (color_mode > 4)
                {
                    color_mode = 0;
                }
                break;

            case OPT_SOUND:
                sound_mode++;
                if (sound_mode > 1)
                {
                    sound_mode = 0;
                }
                break;

            case OPT_JOYSTICK:
#ifdef PC_JOYSTICK
                if (joy_mode == 3)
                {
                    joy_mode = 0;
                }
#else
                if (joy_mode == 2)
                {
                    joy_mode = 0;
                }
#endif
                else
                {
                    ++joy_mode;
                }
                break;

            case OPT_CALIBRATE:
                break;

            case OPT_JS_RANGE:
                if (joy_range < 256)
                {
                    ++joy_range;
                    joy_center_x = joy_range/2;
                    joy_center_y = joy_range/2;
                }
                break;

            case OPT_ORIGIN_X:
                if (joy_center_x < joy_range-1)
                {
                    joy_center_x++;
                }
                break;

            case OPT_ORIGIN_Y:
                if (joy_center_y < joy_range-1)
                {
                    joy_center_y++;
                }
                break;

            case OPT_JS_SENSE:
                if (joy_step < 100)
                {
                    joy_step++;
                }
                break;

            case OPT_JS_SAMPLE:
#ifdef PC_JOYSTICK
                js_timelimit++;
#endif
                break;

            case OPT_SAVE:
            case OPT_QUIT:
                break;

            default:
                break;
            }
        }
        else if (ch == kESC)                           /* exit menu */
        {
            timing_initialize();
            video_set(0);                       /* redo colors */
#ifdef PC_JOYSTICK
            if (joy_mode == JOY_PCJOY)
            {
                c_close_joystick();             /* close the joystick */
                c_open_joystick();              /* reopen the joystick */
                half_joy_range = joy_range/2;
                c_calculate_joystick_parms();
            }
            else
            {
                c_close_joystick();
            }

#endif
            /* reboot machine if different */
            if (current_mode != apple_mode)
            {
                cpu65_interrupt(RebootSig);
            }

            c_interface_exit();
            return;
        }
        else if ((ch == '?') && (option != OPT_PATH))
        {
#define MAINHELP_SUBMENU_H 20
#define MAINHELP_SUBMENU_W 40
            char submenu[MAINHELP_SUBMENU_H][MAINHELP_SUBMENU_W+1] =
            //1.  5.  10.  15.  20.  25.  30.  35.  40.
            { "||||||||||||||||||||||||||||||||||||||||",
              "|                                      |",
              "|  Movement : Up/Down arrows           |",
              "|                                      |",
              "|    Change : Left/Right arrows to     |",
              "|    Values : toggle or press the      |",
              "|             'Return' key to select   |",
              "|                                      |",
              "||||||||||||||||||||||||||||||||||||||||",
              "|                                      |",
              "| Hotkeys used while emulator running: |",
              "|                                      |",
              "| F1 F2: Slot6 Disk Drive A, Drive B   |",
              "| F4   : Toggle Emulation Pause        |",
              "| F5   : Show Keyboard Layout          |",
              "| F7   : Virtual 6502 Debugger         |",
              "| F9   : Toggle Emulator Speed         |",
              "| F10  : Main Menu                     |",
              "|                                      |",
              "||||||||||||||||||||||||||||||||||||||||" };
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

#ifdef PC_JOYSTICK
            /* calibrate joystick */
            if ((ch == 13) && (option == OPT_CALIBRATE))
            {
                c_calibrate_joystick();
            }

#endif
            /* save settings */
            if ((ch == 13) && (option == OPT_SAVE))
            {
                if (save_settings())
                {
#define SAVED_SUBMENU_H 9
#define SAVED_SUBMENU_W 40
                    char submenu[SAVED_SUBMENU_H][SAVED_SUBMENU_W+1] =
                    //1.  5.  10.  15.  20.  25.  30.  35.  40.
                    { "||||||||||||||||||||||||||||||||||||||||",
                      "|                                      |",
                      "|                                      |",
                      "|                                      |",
                      "|            --> Saved <--             |",
                      "|                                      |",
                      "|                                      |",
                      "|                                      |",
                      "||||||||||||||||||||||||||||||||||||||||" };
                    c_interface_print_submenu_centered(submenu[0], SAVED_SUBMENU_W, SAVED_SUBMENU_H);
                    while ((ch = c_mygetch(1)) == -1)
                    {
                    }
                    c_interface_print_screen( screen );
                }
            }

            /* quit apple II simulator */
            if (ch == 13 && option == OPT_QUIT)
            {
                int ch;

#define QUIT_SUBMENU_H 10
#define QUIT_SUBMENU_W 40
                char submenu[QUIT_SUBMENU_H][QUIT_SUBMENU_W+1] =
                //1.  5.  10.  15.  20.  25.  30.  35.  40.
                { "||||||||||||||||||||||||||||||||||||||||",
                  "|                                      |",
                  "|                                      |",
                  "|                                      |",
                  "|           Quit Emulator...           |",
                  "|          Are you sure? (Y/N)         |",
                  "|                                      |",
                  "|                                      |",
                  "|                                      |",
                  "||||||||||||||||||||||||||||||||||||||||" };
                c_interface_print_submenu_centered(submenu[0], QUIT_SUBMENU_W, QUIT_SUBMENU_H);

                while ((ch = c_mygetch(1)) == -1)
                {
                }

                ch = toupper(ch);
                if (ch == 'Y')
                {
                    c_eject_6( 0 );
                    c_eject_6( 1 );
#ifdef PC_JOYSTICK
                    c_close_joystick();
#endif
#ifdef __linux__
                    LOG("Back to Linux, w00t!\n");
#endif
                    video_shutdown();
                    //audio_shutdown();
                    exit( 0 );
                }

                c_interface_print_screen( screen );
            }
        }
    }
}

#if 0
/* -------------------------------------------------------------------------
    c_interface_words() - this is not valid anymore.
    if anyone has his email, let the maintainer know!
   ------------------------------------------------------------------------- */

void c_interface_words()
{
    char screen[24][INTERFACE_SCREEN_X+1] =
    { "||||||||||||||||||||||||||||||||||||||||",
      "|    Apple II+ Emulator Version 0.01   |",
      "||||||||||||||||||||||||||||||||||||||||",
      "| If you have problems with your       |",
      "| keyboard concerning the mapping of   |",
      "| various keys, please let me know.    |",
      "| I use a Swedish keyboard for myself  |",
      "| and the scancodes may differ from US |",
      "| keyboards (or other countries as     |",
      "| well). Currently, my email address   |",
      "| is: d91a1bo@meryl.csd.uu.se. This    |",
      "| address is valid at least one more   |",
      "| year, i.e. as long as I am Computer  |",
      "| Science student at the University    |",
      "| of Uppsala. \"...and there were much  |",
      "| rejoicing! oyeeeeeh\"                 |",
      "|                                      |",
      "|                                      |",
      "|                                      |",
      "|                                      |",
      "|              / Alexander  Oct 9 1994 |",
      "||||||||||||||||||||||||||||||||||||||||",
      "|       (Press any key to exit)        |",
      "||||||||||||||||||||||||||||||||||||||||" };

    int i;

    video_setpage( 0 );

    c_interface_translate_screen( screen );
    c_interface_print_screen( screen );

    while (c_mygetch(1) == -1)
    {
    }

    c_interface_exit();
}
#endif /* if 0 */

/* -------------------------------------------------------------------------
    c_interface_keyboard_layout()
   ------------------------------------------------------------------------- */

void c_interface_keyboard_layout()
{
    /* FIXME !!!!!!!!!!!!1  - deprecating ][+ modes ...
    char screen1[24][INTERFACE_SCREEN_X+1] =
    { "||||||||||||||||||||||||||||||||||||||||",
      "|     Apple II+ US Keyboard Layout     |",
      "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "| 1! 2\" 3# 4$ 5% 6& 7' 8( 9) 0 :* -=   |",
      "|  Q  W  E  R  T  Y  U  I  O  P@    CR |",
      "|   A  S  D  F  G  H  J  K  L  ;+ <- ->|",
      "|    Z  X  C  V  B  N^ M ,< .> /?      |",
      "| Where <- -> are the left and right   |",
      "| arrow keys respectively.             |",
      "|                                      |",
      "| Joystick emulation on numeric keypad |",
      "| 7  8  9  for various directions.     |",
      "| 4     6  Press 5 to center linear    |",
      "| 1  2  3  joystick.                   |",
      "|                                      |",
      "| Ctrl-PrintScrn/SysRq is REBOOT.      |",
      "| Ctrl-Pause/Break is RESET.           |",
      "| Pause/Break alone pauses emulation.  |",
      "| Alt Left and Alt Right are Apple     |",
      "| Keys (Joystick buttons 0 & 1).       |",
      "||||||||||||||||||||||||||||||||||||||||",
      "|       (Press any key to exit)        |",
      "||||||||||||||||||||||||||||||||||||||||" };
      */

    char screen[24][INTERFACE_SCREEN_X+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.  45.  50.  55.  60.  65.  70.  75.  80.",
    { "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                         Apple //e US Keyboard Layout                         |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                                      |                                       |",
      "|1! 2@ 3# 4$ 5% 6^ 7& 8* 9( 0) -_ =+ dl|                                       |",
      "| Q  W  E  R  T  Y  U  I  O P [{ ]} \\| |                                       |",
      "|cp A  S  D  F  G  H  J  K  L ;: '\" CR |                                       |",
      "|sh  Z  X  C  V  B  N  M ,< .> /?   sh |                                       |",
      "|ctrl                                  |                                       |",
      "|                                      |                                       |",
      "| Where dl is DEL, cp is CAPS, CR is   |                                       |",
      "| RETURN, sh is SHIFT, ctrl is CONTROL.|                                       |",
      "| Arrow keys are as is.                |                                       |",
      "|                                      |                                       |",
      "|                                      |                                       |",
      "|                                      |                                       |",
      "| Ctrl-PrntScrn/SysRq reboots emulator |                                       |",
      "| Ctrl-Pause/Break is Apple reset      |                                       |",
      "| Pause/Break alone pauses emulation   |                                       |",
      "| Alt Left and Alt Right are Apple     |                                       |",
      "| Keys (Joystick buttons 0 & 1)        |                                       |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
      "|                           (Press any key to exit)                            |",
      "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" };

    // TODO : joystick emulation? and F-keys

    int i;

    video_setpage( 0 );

    c_interface_translate_screen(screen);
    c_interface_print_screen( screen );

    while (c_mygetch(1) == -1)
    {
    }

    c_interface_exit();
}

