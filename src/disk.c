/*
 * Apple // emulator for Linux: C portion of Disk ][ emulation
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

#define PHASE_BYTES 3328

#define NIB_SIZE 232960
#define DSK_SIZE 143360

static unsigned char slot6_rom[256];
static int slot6_rom_loaded = 0;

struct drive disk6;

static int skew_table_6[16] =           /* Sector skew table */
{ 0,7,14,6,13,5,12,4,11,3,10,2,9,1,8,15 };

static int translate_table_6[256] =     /* Translation table */
{
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x01,
    0x80, 0x80, 0x02, 0x03, 0x80, 0x04, 0x05, 0x06,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x07, 0x08,
    0x80, 0x80, 0x80, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x80, 0x80, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
    0x80, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x1b, 0x80, 0x1c, 0x1d, 0x1e,
    0x80, 0x80, 0x80, 0x1f, 0x80, 0x80, 0x20, 0x21,
    0x80, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x29, 0x2a, 0x2b,
    0x80, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
    0x80, 0x80, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x80, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
};

static void cut_gz(char *name) {
    char *p = name + strlen(name) - 1;
    p--;
    p--;
    *p = '\0';
}

static bool is_gz(const char * const name) {
    size_t len = strlen( name );

    if (len > 3) {
        return (strcmp(name+len-3, ".gz") == 0);
    }

    return false;
}

static bool is_nib(const char * const name) {
    size_t len = strlen( name );

    if (is_gz(name)) {
        len -= 3;
    }

    if (!strncmp(name + len - 4, ".nib", 4)) {
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------
    c_init_6()
   ------------------------------------------------------------------------- */

void c_init_6()
{
    disk6.disk[0].phase = disk6.disk[1].phase = 42;
    disk6.disk[0].phase_change = disk6.disk[1].phase_change = 0;

    disk6.motor = 1;            /* motor on */
    disk6.drive = 0;            /* first drive active */
    disk6.ddrw = 0;
    disk6.volume = 254;
#if 0 /* BUGS!: */
    file_name_6[0][1024] = '\0';
    file_name_6[1][1024] = '\0';
#endif
}

#ifdef INTERFACE_CLASSIC
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
#endif

/* -------------------------------------------------------------------------
    c_eject_6() - ejects/gzips image file
   ------------------------------------------------------------------------- */

void c_eject_6(int drive) {

    if (disk6.disk[drive].compressed)
    {
        // foo.dsk -> foo.dsk.gz
        const char* const err = def(disk6.disk[drive].file_name, is_nib(disk6.disk[drive].file_name) ? NIB_SIZE : DSK_SIZE);
        if (err)
        {
            ERRLOG("OOPS: An error occurred when attempting to compress a disk image : %s", err);
#ifdef INTERFACE_CLASSIC
            snprintf(&zlibmenu[4][2], 37, "%s", err);
            c_interface_print_submenu_centered(zlibmenu[0], ZLIB_SUBMENU_W, ZLIB_SUBMENU_H);
            while ((int ch = c_mygetch(1)) == -1) {
                // ...
            }
#endif
        }
        else
        {
            unlink(disk6.disk[drive].file_name);
        }
    }

    disk6.disk[drive].compressed = 0;
    disk6.disk[drive].nibblized = 0;
    sprintf(disk6.disk[drive].file_name, "%s", "");
    if (disk6.disk[drive].fp)
    {
        fclose(disk6.disk[drive].fp);
        disk6.disk[drive].fp = NULL;
    }
}

/* -------------------------------------------------------------------------
    c_new_diskette_6()
    inserts a new disk image into the appropriate drive.
    return 0 on success
   ------------------------------------------------------------------------- */
int c_new_diskette_6(int drive, const char * const raw_file_name, int force) {
    struct stat buf;

    /* uncompress the gziped disk */
    char *file_name = strdup(raw_file_name);
    if (is_gz(file_name))
    {
        int rawcount = 0;
        const char *err = inf(file_name, &rawcount); // foo.dsk.gz -> foo.dsk
        if (!err) {
            int expected = is_nib(file_name) ? NIB_SIZE : DSK_SIZE;
            if (rawcount != expected) {
                err = "disk image is not expected size!";
            }
        }
        if (err)
        {
            ERRLOG("OOPS: An error occurred when attempting to inflate/load a disk image : %s", err);
#ifdef INTERFACE_CLASSIC
            snprintf(&zlibmenu[4][2], 37, "%s", err);
            c_interface_print_submenu_centered(zlibmenu[0], ZLIB_SUBMENU_W, ZLIB_SUBMENU_H);
            while ((int ch = c_mygetch(1)) == -1) {
                // ...
            }
#endif
            free(file_name);
            return 1;
        }
        if (unlink(file_name)) // temporarily remove .gz file
        {
            ERRLOG("OOPS, cannot unlink %s", file_name);
        }

        cut_gz(file_name);
    }

    strcpy(disk6.disk[drive].file_name, file_name);
    disk6.disk[drive].file_name[1023]='\0';
    disk6.disk[drive].compressed = true;// always using gz
    disk6.disk[drive].nibblized = is_nib(file_name);
    free(file_name);
    file_name = NULL;

    if (disk6.disk[drive].fp)
    {
        fclose(disk6.disk[drive].fp);
        disk6.disk[drive].fp = NULL;
    }

    if (stat(disk6.disk[drive].file_name, &buf) < 0)
    {
        disk6.disk[drive].fp = NULL;
        c_eject_6(drive);
        return 1;                       /* problem: disk unreadable */
    }
    else
    {
        disk6.disk[drive].file_size = buf.st_size;

        if (!force)
        {
            disk6.disk[drive].fp = fopen(disk6.disk[drive].file_name, "r+");
            disk6.disk[drive].is_protected = false;
        }

        if ((disk6.disk[drive].fp == NULL) || (force))
        {
            disk6.disk[drive].fp = fopen(disk6.disk[drive].file_name, "r");
            disk6.disk[drive].is_protected = true;    /* disk is write protected! */
        }

        if (disk6.disk[drive].fp == NULL)
        {
            /* Failed to open file. */
            c_eject_6(drive);
            return 1;                   /* problem: disk unreadable */
        }

        /* seek to current head position. */
        fseek(disk6.disk[drive].fp, PHASE_BYTES * disk6.disk[drive].phase, SEEK_SET);
    }

    disk6.disk[drive].sector = 0;
    disk6.disk[drive].run_byte = 0;

    return 0;                           /* no problem */
}


/* -------------------------------------------------------------------------
    c_read_nibblized_6_6() - reads a standard .nib file of length 232960 bytes.

        there are 70 phases positioned every 3328 bytes.
   ------------------------------------------------------------------------- */

unsigned char c_read_nibblized_6_6()
{
    static unsigned char ch;

    if (disk6.disk[disk6.drive].phase_change)
    {
        fseek(disk6.disk[disk6.drive].fp, PHASE_BYTES * disk6.disk[disk6.drive].phase, SEEK_SET);
        disk6.disk[disk6.drive].phase_change = false;
    }

    disk6.disk_byte = fgetc(disk6.disk[disk6.drive].fp);
    ch = disk6.disk_byte;
    /* track revolves... */
    if (ftell(disk6.disk[disk6.drive].fp) == (PHASE_BYTES * (disk6.disk[disk6.drive].phase + 2)))
    {
        fseek(disk6.disk[disk6.drive].fp, -2 * PHASE_BYTES, SEEK_CUR);
    }

    return ch;
}

/* -------------------------------------------------------------------------
    c_read_normal_6()
   ------------------------------------------------------------------------- */
unsigned char c_read_normal_6()
{
    int position;
    int old_value;

    unsigned char value = 0;

    /* The run byte tells what's to do */
    switch (disk6.disk[disk6.drive].run_byte)
    {
    case 0: case 1: case 2: case 3: case 4: case 5:
    case 20: case 21: case 22: case 23: case 24:
        /* Sync */
        value = 0xFF;
        break;

    case 6: case 25:
        /* Prologue (first byte) */
        value = 0xD5;
        break;

    case 7: case 26:
        /* Prologue (second byte) */
        value = 0xAA;
        break;

    case 8:
        /* Prologue (third byte) */
        value = 0x96;
        break;

    case 9:
        /* Volume (encoded) */
        value = (disk6.volume >> 1) | 0xAA;
        disk6.checksum = disk6.volume;
        break;

    case 10:
        /* Volume (encoded) */
        value = disk6.volume | 0xAA;
        break;

    case 11:
        /* Track number (encoded) */
        disk6.checksum ^= (disk6.disk[disk6.drive].phase >> 1);
        value = (disk6.disk[disk6.drive].phase >> 2) | 0xAA;
        break;

    case 12:
        /* Track number (encoded) */
        value = (disk6.disk[disk6.drive].phase >> 1) | 0xAA;
        break;

    case 13:
        /* Sector number (encoded) */
        disk6.checksum ^= disk6.disk[disk6.drive].sector;
        value = (disk6.disk[disk6.drive].sector >> 1) | 0xAA;
        break;

    case 14:
        /* Sector number (encoded) */
        value = disk6.disk[disk6.drive].sector | 0xAA;
        break;

    case 15:
        /* Checksum */
        value = (disk6.checksum >> 1) | 0xAA;
        break;

    case 16:
        /* Checksum */
        value = disk6.checksum | 0xAA;
        break;

    case 17: case 371:
        /* Epilogue (first byte) */
        value = 0xDE;
        break;

    case 18: case 372:
        /* Epilogue (second byte) */
        value = 0xAA;
        break;

    case 19: case 373:
        /* Epilogue (third byte) */
        value = 0xEB;
        break;

    case 27:
        /* Data header */
        disk6.exor_value = 0;

        /* Set file position variable */
        disk6.disk[disk6.drive].file_pos = 256 * 16 * (disk6.disk[disk6.drive].phase >> 1) +
                                           256 * skew_table_6[ disk6.disk[disk6.drive].sector ];

        /* File large enough? */
        if (disk6.disk[disk6.drive].file_pos + 255 > disk6.disk[disk6.drive].file_size)
        {
            return 0xFF;
        }

        /* Set position */
        fseek( disk6.disk[disk6.drive].fp, disk6.disk[disk6.drive].file_pos, SEEK_SET );

        /* Read sector */
        if (fread( disk6.disk_data, 1, 256, disk6.disk[disk6.drive].fp ) != 256)
        {
            // error
        }

        disk6.disk_data[ 256 ] = disk6.disk_data[ 257 ] = 0;
        value = 0xAD;
        break;

    case 370:
        /* Checksum */
        value = translate_table_6[disk6.exor_value & 0x3F];

        /* Increment sector number (and wrap if necessary) */
        disk6.disk[disk6.drive].sector++;
        if (disk6.disk[disk6.drive].sector == 16)
        {
            disk6.disk[disk6.drive].sector = 0;
        }

        break;

    default:
        position = disk6.disk[disk6.drive].run_byte - 28;
        if (position >= 0x56)
        {
            position -= 0x56;
            old_value = disk6.disk_data[ position ];
            old_value = old_value >> 2;
            disk6.exor_value ^= old_value;
            value = translate_table_6[disk6.exor_value & 0x3F];
            disk6.exor_value = old_value;
        }
        else
        {
            old_value = 0;
            old_value |= (disk6.disk_data[position] & 0x1) << 1;
            old_value |= (disk6.disk_data[position] & 0x2) >> 1;
            old_value |= (disk6.disk_data[position+0x56] & 0x1) << 3;
            old_value |= (disk6.disk_data[position+0x56] & 0x2) << 1;
            old_value |= (disk6.disk_data[position+0xAC] & 0x1) << 5;
            old_value |= (disk6.disk_data[position+0xAC] & 0x2) << 3;
            disk6.exor_value ^= old_value;
            value = translate_table_6[disk6.exor_value & 0x3F];
            disk6.exor_value = old_value;
        }

        break;
    } /* End switch */

    /* Continue by increasing run byte value */
    disk6.disk[disk6.drive].run_byte++;
    if (disk6.disk[disk6.drive].run_byte > 373)
    {
        disk6.disk[disk6.drive].run_byte = 0;
    }

    disk6.disk_byte = value;
    return value;
}



/* -------------------------------------------------------------------------
    c_write_nibblized_6_6() - writes a standard .nib file of length 232960 bytes.

        there are 70 phases positioned every 3328 bytes.
   ------------------------------------------------------------------------- */

void c_write_nibblized_6_6()
{
    if (disk6.disk[disk6.drive].phase_change)
    {
        fseek(disk6.disk[disk6.drive].fp, PHASE_BYTES * disk6.disk[disk6.drive].phase, SEEK_SET);
        disk6.disk[disk6.drive].phase_change = false;
    }

    fputc(disk6.disk_byte, disk6.disk[disk6.drive].fp);
    /* track revolves... */
    if (ftell(disk6.disk[disk6.drive].fp) == (PHASE_BYTES * (disk6.disk[disk6.drive].phase + 2)))
    {
        fseek(disk6.disk[disk6.drive].fp, -2 * PHASE_BYTES, SEEK_CUR);
    }
}

/* -------------------------------------------------------------------------
    c_write_normal_6()   disk6.disk_byte contains the value
   ------------------------------------------------------------------------- */

void c_write_normal_6()
{
    int position;
    int old_value;

    if (disk6.disk_byte == 0xD5)
    {
        disk6.disk[disk6.drive].run_byte = 6;    /* Initialize run byte value */

    }

    /* The run byte tells what's to do */

    switch (disk6.disk[disk6.drive].run_byte)
    {
    case 0: case 1: case 2: case 3: case 4: case 5:
    case 20: case 21: case 22: case 23: case 24:
        /* Sync */
        break;

    case 6: case 25:
        /* Prologue (first byte) */
        if (disk6.disk_byte == 0xFF)
        {
            disk6.disk[disk6.drive].run_byte--;
        }

        break;

    case 7: case 26:
        /* Prologue (second byte) */
        break;

    case 8:
        /* Prologue (third byte) */
        if (disk6.disk_byte == 0xAD)
        {
            disk6.exor_value = 0, disk6.disk[disk6.drive].run_byte = 27;
        }

        break;

    case 9: case 10:
        /* Volume */
        break;

    case 11: case 12:
        /* Track */
        break;

    case 13: case 14:
        /* Sector */
        break;

    case 15:
        /* Checksum */
        break;

    case 16:
        /* Checksum */
        break;

    case 17: case 371:
        /* Epilogue (first byte) */
        break;

    case 18: case 372:
        /* Epilogue (second byte) */
        break;

    case 19: case 373:
        /* Epilogue (third byte) */
        break;

    case 27:
        disk6.exor_value = 0;
        break;

    case 370:
        /* Set file position variable */
        disk6.disk[disk6.drive].file_pos = 256 * 16 * (disk6.disk[disk6.drive].phase >> 1) +
                                           256 * skew_table_6[ disk6.disk[disk6.drive].sector ];

        /* Is the file large enough? */
        if (disk6.disk[disk6.drive].file_pos + 255 > disk6.disk[disk6.drive].file_size)
        {
            return;
        }


        /* Set position */
        fseek( disk6.disk[disk6.drive].fp, disk6.disk[disk6.drive].file_pos, SEEK_SET );

        /* Write sector */
        fwrite(disk6.disk_data, 1, 256, disk6.disk[disk6.drive].fp);
        fflush( disk6.disk[disk6.drive].fp );
        /* Increment sector number (and wrap if necessary) */
        disk6.disk[disk6.drive].sector++;
        if (disk6.disk[disk6.drive].sector == 16)
        {
            disk6.disk[disk6.drive].sector = 0;
        }

        break;

    default:
        position = disk6.disk[disk6.drive].run_byte - 28;
        disk6.disk_byte = translate_table_6[ disk6.disk_byte ];
        if (position >= 0x56)
        {
            position -= 0x56;
            disk6.disk_byte ^= disk6.exor_value;
            old_value = disk6.disk_byte;
            disk6.disk_data[position] |= (disk6.disk_byte << 2) & 0xFC;
            disk6.exor_value = old_value;
        }
        else
        {
            disk6.disk_byte ^= disk6.exor_value;
            old_value = disk6.disk_byte;
            disk6.disk_data[position] = (disk6.disk_byte & 0x01) << 1;
            disk6.disk_data[position] |= (disk6.disk_byte & 0x02) >> 1;
            disk6.disk_data[position + 0x56] = (disk6.disk_byte & 0x04) >> 1;
            disk6.disk_data[position + 0x56] |= (disk6.disk_byte & 0x08) >> 3;
            disk6.disk_data[position + 0xAC] = (disk6.disk_byte & 0x10) >> 3;
            disk6.disk_data[position + 0xAC] |= (disk6.disk_byte & 0x20) >> 5;
            disk6.exor_value = old_value;
        }

        break;
    } /* End switch */

    disk6.disk[disk6.drive].run_byte++;
    if (disk6.disk[disk6.drive].run_byte > 373)
    {
        disk6.disk[disk6.drive].run_byte = 0;
    }
}

GLUE_C_READ(disk_read_byte)
{
    if (disk6.ddrw)
    {
        if (disk6.disk[disk6.drive].fp == NULL)
        {
            return 0;           /* Return if there is no disk in drive */
        }

        if (disk6.disk[disk6.drive].is_protected)
        {
            return 0;           /* Do not write if diskette is write protected */

        }

        if (disk6.disk_byte < 0x96)
        {
            return 0;           /* Only byte values at least 0x96 are allowed */

        }

        (disk6.disk[disk6.drive].nibblized) ? c_write_nibblized_6_6() : c_write_normal_6();
        return 0; /* ??? */
    }
    else
    {
        if (disk6.disk[disk6.drive].fp == NULL)
        {
            return 0xFF;        /* Return FF if there is no disk in drive */
        }

        if (disk6.motor)                /* Motor turned on? */
        {
            if (disk6.motor > 99)
            {
                return 0;
            }
            else
            {
                disk6.motor++;
            }
        }

        /* handle nibblized_6 or regular disks */
        return (disk6.disk[disk6.drive].nibblized) ? c_read_nibblized_6_6() : c_read_normal_6();
    }
}

GLUE_C_READ(disk_read_phase)
{
    /*
     * Comment from xapple2+ by Phillip Stephens:
     * Turn motor phases 0 to 3 on.  Turning on the previous phase + 1
     * increments the track position, turning on the previous phase - 1
     * decrements the track position.  In this scheme phase 0 and 3 are
     * considered to be adjacent.  The previous phase number can be
     * computed as the track number % 4.
     */

    switch (((ea >> 1) - disk6.disk[disk6.drive].phase) & 3)
    {
    case 1:
        disk6.disk[disk6.drive].phase++;
        break;
    case 3:
        disk6.disk[disk6.drive].phase--;
        break;
    }

    if (disk6.disk[disk6.drive].phase<0)
    {
        disk6.disk[disk6.drive].phase=0;
    }

    if (disk6.disk[disk6.drive].phase>69)
    {
        disk6.disk[disk6.drive].phase=69;
    }

    disk6.disk[disk6.drive].phase_change = true;

    return 0;
}


GLUE_C_READ(disk_read_motor_off)
{
    disk6.motor = 1;
    return disk6.drive;
}

GLUE_C_READ(disk_read_motor_on)
{
    disk6.motor = 0;
    return disk6.drive;
}

GLUE_C_READ(disk_read_select_a)
{
    return disk6.drive = 0;
}

GLUE_C_READ(disk_read_select_b)
{
    return disk6.drive = 1;
}


GLUE_C_READ(disk_read_latch)
{
    return disk6.drive;
}

GLUE_C_READ(disk_read_prepare_in)
{
    disk6.ddrw = 0;
    return disk6.disk[disk6.drive].is_protected ? 0x80 : 0x00;
}

GLUE_C_READ(disk_read_prepare_out)
{
    disk6.ddrw = 1;
    return disk6.drive;
}

GLUE_C_WRITE(disk_write_latch)
{
    disk6.disk_byte = b;
}

void disk_io_initialize(unsigned int slot)
{
    FILE *f;
    int i;
    char temp[PATH_MAX];

    assert(slot == 6);

    /* load Disk II rom */
    if (!slot6_rom_loaded)
    {
        snprintf(temp, PATH_MAX, "%s/slot6.rom", system_path);
        if ((f = fopen( temp, "r" )) == NULL)
        {
            printf("Cannot find file '%s'.\n",temp);
            exit( 0 );
        }

        if (fread(slot6_rom, 0x100, 1, f) != 0x100)
        {
            // error
        }

        fclose(f);
        slot6_rom_loaded = 1;
    }

    memcpy(apple_ii_64k[0] + 0xC600, slot6_rom, 0x100);

    // disk softswitches
    // 0xC0Xi : X = slot 0x6 + 0x8 == 0xE
    cpu65_vmem_r[0xC0E0] = cpu65_vmem_r[0xC0E2] =
                               cpu65_vmem_r[0xC0E4] = cpu65_vmem_r[0xC0E6] =
                                                          ram_nop;

    cpu65_vmem_r[0xC0E1] = cpu65_vmem_r[0xC0E3] =
                               cpu65_vmem_r[0xC0E5] = cpu65_vmem_r[0xC0E7] =
                                                          disk_read_phase;

    cpu65_vmem_r[0xC0E8] =
        disk_read_motor_off;
    cpu65_vmem_r[0xC0E9] =
        disk_read_motor_on;
    cpu65_vmem_r[0xC0EA] =
        disk_read_select_a;
    cpu65_vmem_r[0xC0EB] =
        disk_read_select_b;
    cpu65_vmem_r[0xC0EC] =
        disk_read_byte;
    cpu65_vmem_r[0xC0ED] =
        disk_read_latch;        /* read latch */
    cpu65_vmem_r[0xC0EE] =
        disk_read_prepare_in;
    cpu65_vmem_r[0xC0EF] =
        disk_read_prepare_out;

    for (i = 0xC0E0; i < 0xC0F0; i++)
    {
        cpu65_vmem_w[i] =
            cpu65_vmem_r[i];
    }

    cpu65_vmem_w[0xC0ED] =
        disk_write_latch;       /* write latch */
}
